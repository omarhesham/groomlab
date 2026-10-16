// SPDX-License-Identifier: Apache-2.0
// Source operators: bring host-supplied data (ExternalInputs) into the graph.

#include <algorithm>
#include <set>

#include "groomlab/graph.h"
#include "groomlab/operators.h"
#include "groomlab/scalp_pieces.h"
#include "groomlab/subdivision.h"
#include "internal.h"

namespace groomlab {

namespace {

// The `subdivision` param of Scalp and ColliderMesh: Catmull-Clark levels (subdivision.h).
ParamSpec subdivisionSpec(const std::string& what) {
    return notAnimatable({"subdivision", std::string("matchRender"),
                          "Catmull-Clark levels the " + what +
                              " is refined by, so hair sits on the surface the render draws: matchRender takes the "
                              "mesh's render subdivision (Maya: its Arnold subdivision, else its smooth mesh preview; "
                              "a groom file: the level saved with it), off the mesh as it is",
                          "", {"matchRender", "off", "1", "2", "3"}});
}

// What a node's note says of a mesh's subdivision ("" for a cage the node leaves as it is).
std::string subdivisionNote(const std::string& param, const ScalpMesh* refined, const ScalpMesh* cage) {
    const int levels = refined ? refined->subdivisionLevel() : 0;
    std::string from;
    if (param == "matchRender" && cage) {
        from = cage->renderSubdivisionFrom().empty() ? std::string("match render")
                                                     : "match render: " + cage->renderSubdivisionFrom();
    }
    if (levels == 0) return from.empty() ? std::string() : "Subdivision off (" + from + ")";
    std::string note = "Catmull-Clark x" + std::to_string(levels) + ": " + std::to_string(refined->triangleCount()) +
                       " triangles from " + std::to_string(refined->cageFaceCount()) + " faces";
    return from.empty() ? note : note + " (" + from + ")";
}

// The scalp: the session refines it (GroomSession::scalpSubdivision reads this node's
// `subdivision`) before it binds guides, parts and flow to it, so the external is the refined mesh.
// A groom with several scalps (scalp_pieces.h): `input` picks one ("scalp" the groom's own, else
// another's name), handed on as its own mesh (the external scalpPieceExternal(input)); with one
// scalp, the "scalp" external as it is. Another name the session does not know reads the
// external of that name (a host's own mesh input).
class ScalpOp : public Operator {
public:
    std::string typeName() const override { return op::kScalp; }
    std::string category() const override { return "source"; }
    std::vector<PortSpec> inputs() const override { return {}; }
    PortSpec output() const override { return {"scalp", PortType::Mesh}; }
    std::vector<ParamSpec> paramSpecs() const override {
        return {ParamSpec{"input", std::string(kMainScalpName),
                          "Which of the groom's scalps: scalp (its own mesh) or the name of another scalp of the groom "
                          "(Maya: Attribute Editor > Scalps), so a Scatter fed this node grows hair on that mesh alone"},
                subdivisionSpec("scalp")};
    }
    std::vector<std::string> externalInputs() const override {
        const std::string name = params().getString("input");
        return {name, scalpPieceExternal(name)};
    }
    bool refreshesOnCacheHit() const override { return true; }  // the scalp's key leaves its color sets out

    Value evaluate(const EvalContext& ctx, const std::vector<Value>&) const override { return mesh(ctx); }
    std::string note(const EvalContext& ctx) const override {
        const MeshPtr scalp = mesh(ctx);
        const std::string param = params().getString("subdivision");
        if (!scalp) {
            const std::string name = params().getString("input");
            return name == kMainScalpName ? std::string() : "No scalp named '" + name + "' on this groom: nothing to grow on";
        }
        if (scalp->subdivisionLevel() > 0 || param != "matchRender") return subdivisionNote(param, scalp.get(), scalp.get());
        return {};
    }

private:
    MeshPtr mesh(const EvalContext& ctx) const {
        const std::string name = params().getString("input");
        if (MeshPtr piece = valueAs<MeshPtr>(ctx.external(scalpPieceExternal(name)))) return piece;
        return valueAs<MeshPtr>(ctx.external(name));
    }
};

// A collider (or any other mesh) from the host: the external "mesh:<name>"
// (GroomData::meshes, Groom::setMesh; in Maya the groom's colliderMeshes, see
// groomlab.add_collider), for Collide's `collider` input. Null while the host
// has no mesh of that name, which Collide takes as nothing to hit.
class ColliderMeshOp : public Operator {
public:
    std::string typeName() const override { return op::kColliderMesh; }
    std::string category() const override { return "source"; }
    std::vector<PortSpec> inputs() const override { return {}; }
    PortSpec output() const override { return {"mesh", PortType::Mesh}; }
    std::vector<ParamSpec> paramSpecs() const override {
        return {{"name", std::string("collider"), "The collider mesh's name, as registered with the groom (Maya: "
                                                   "Attribute Editor > Colliders, or groomlab.add_collider)"},
                subdivisionSpec("collider"),
                // Read by the session for a Simulate node this feeds (SimInputs::colliderFriction).
                notAnimatable({"friction", -1.0, "How much simulated hair resting on this mesh is kept from sliding, 0-1, "
                                                 "in a Simulate node it feeds (-1: the Simulate node's friction)"})
                    .range(-1, 1)};
    }
    std::vector<std::string> externalInputs() const override { return {externalName()}; }

    // Refined here (the plan kept per topology: a deforming collider re-runs only its weights).
    Value evaluate(const EvalContext& ctx, const std::vector<Value>&) const override {
        const MeshPtr mesh = valueAs<MeshPtr>(ctx.external(externalName()));
        if (!mesh) return mesh;
        return subdivideScalp(mesh, subdivisionLevels(params().getString("subdivision"), mesh->renderSubdivision()));
    }
    std::string note(const EvalContext& ctx) const override {
        const MeshPtr mesh = valueAs<MeshPtr>(ctx.external(externalName()));
        if (!mesh) return {};
        const std::string param = params().getString("subdivision");
        const int levels = subdivisionLevels(param, mesh->renderSubdivision());
        if (levels == 0) return subdivisionNote(param, nullptr, mesh.get());
        // The counts the refined mesh has, from the kept plan (nothing refined again).
        const auto plan = SubdivisionPlan::of(*mesh, levels);
        std::string note = "Catmull-Clark x" + std::to_string(levels) + ": " + std::to_string(plan->triangles().size()) +
                           " triangles from " + std::to_string(plan->cageFaceCount()) + " faces";
        if (param == "matchRender") {
            note += mesh->renderSubdivisionFrom().empty() ? " (match render)" : " (match render: " + mesh->renderSubdivisionFrom() + ")";
        }
        return note;
    }

private:
    std::string externalName() const { return "mesh:" + params().getString("name"); }
};

// Guides from the host: the guide hierarchy ("hierarchy" external, flattened
// to one level or group) plus any flat guides ("guides" external, e.g.
// connected Maya curves). Both are optional; without either the output is null.
// Names in a list separated by spaces or commas.
std::vector<std::string> splitNames(const std::string& names) {
    std::vector<std::string> out;
    std::string name;
    for (const char c : names + " ") {
        if (c == ' ' || c == ',') {
            if (!name.empty()) out.push_back(name);
            name.clear();
        } else {
            name += c;
        }
    }
    return out;
}

// Removes from `set` the guides in the named groups or their child groups.
void excludeGroups(const GuideHierarchy& hierarchy, const std::string& names, GuideSet& set) {
    std::vector<GroupId> excluded;
    std::string name;
    for (const char c : names + " ") {
        if (c == ' ' || c == ',') {
            if (!name.empty()) {
                const GroupId id = hierarchy.findGroup(name);
                if (id == kNoGroup) throw GraphError("Guides: no guide group named '" + name + "' to exclude");
                excluded.push_back(id);
            }
            name.clear();
        } else {
            name += c;
        }
    }
    if (excluded.empty()) return;
    auto isExcluded = [&](GroupId group) {
        for (int depth = 0; group != kNoGroup && depth < 64; ++depth) {
            if (std::find(excluded.begin(), excluded.end(), group) != excluded.end()) return true;
            const GuideGroup* g = hierarchy.group(group);
            group = g ? g->parent : kNoGroup;
        }
        return false;
    };
    auto& guides = set.guides();
    guides.erase(std::remove_if(guides.begin(), guides.end(),
                                [&](const Guide& g) {
                                    const HierarchyGuide* h = hierarchy.guide(g.id);
                                    return h && isExcluded(h->group);
                                }),
                 guides.end());
}

class GuidesOp : public Operator {
public:
    std::string typeName() const override { return op::kGuides; }
    std::string category() const override { return "source"; }
    std::vector<PortSpec> inputs() const override { return {}; }
    PortSpec output() const override { return {"guides", PortType::Guides}; }
    std::vector<ParamSpec> paramSpecs() const override {
        return {
            ParamSpec{"input", std::string("guides"), "Name of the external flat guide input"}.adv(),
            ParamSpec{"hierarchy", std::string("hierarchy"), "Name of the external guide hierarchy input"}.adv(),
            notAnimatable({"level", std::int64_t{-1}, "Hierarchy level to use (-1: deepest), without groups"})
                .soft(-1, 5)
                .when("group", {""}),
            {"group", std::string(), "Hierarchy groups to use instead of a level, space or comma separated, with "
                                     "their guides together (empty: use level)"},
            {"exclude", std::string(), "Groups left out, with their child groups (space or comma separated), "
                                       "e.g. braid paths that are not hair"},
        };
    }
    std::vector<std::string> externalInputs() const override {
        return {params().getString("input"), params().getString("hierarchy")};
    }

    Value evaluate(const EvalContext& ctx, const std::vector<Value>&) const override {
        const GuidesPtr flat = valueAs<GuidesPtr>(ctx.external(params().getString("input")));
        const HierarchyPtr hierarchy = valueAs<HierarchyPtr>(ctx.external(params().getString("hierarchy")));
        if (!hierarchy || hierarchy->guides().empty()) return flat;

        const std::vector<std::string> groupNames = splitNames(params().getString("group"));
        GuideSet set;
        if (!groupNames.empty()) {
            std::set<std::uint64_t> seen;  // a group listed twice counts once
            for (const std::string& groupName : groupNames) {
                const GroupId id = hierarchy->findGroup(groupName);
                if (id == kNoGroup) throw GraphError("Guides: no guide group named '" + groupName + "'");
                const GuideSet one = hierarchy->flattenGroup(id);  // kept alive for the loop
                for (const Guide& g : one.guides()) {
                    if (seen.insert(g.id).second) set.guides().push_back(g);
                }
            }
        } else {
            set = hierarchy->flatten(static_cast<int>(params().getInt("level")));
        }
        excludeGroups(*hierarchy, params().getString("exclude"), set);
        if (flat) {
            for (const Guide& g : flat->guides()) {
                Guide& added = set.add(g.id, g.points);
                added.root = g.root;
                added.attributes = g.attributes;
                added.pointAttributes = g.pointAttributes;
            }
        }
        return GuidesPtr(std::make_shared<GuideSet>(std::move(set)));
    }
};

}  // namespace

void registerSourceOperators(OperatorRegistry& registry) {
    registry.registerType(op::kScalp, [] { return std::make_unique<ScalpOp>(); });
    registry.registerType(op::kColliderMesh, [] { return std::make_unique<ColliderMeshOp>(); });
    registry.registerType(op::kGuides, [] { return std::make_unique<GuidesOp>(); });
}

}  // namespace groomlab
