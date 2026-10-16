// SPDX-License-Identifier: Apache-2.0
// GuideHierarchy and what edits it: brushes, guide placement, selection,
// correctives and scalp transfer.

#include "bind_common.h"

#include <optional>
#include <sstream>

#include "groomlab/brush.h"
#include "groomlab/correctives.h"
#include "groomlab/guide_hierarchy.h"
#include "groomlab/guide_selection.h"
#include "groomlab/transfer.h"
#include "groomlab/transfer_markers_edit.h"

namespace groomlab::py_bind {

namespace {

// BrushSettings with the mask stored alongside (the core keeps a pointer).
struct PyBrushSettings : BrushSettings {
    std::map<GuideId, float> maskStorage;
    bool hasMask = false;

    PyBrushSettings() = default;
    PyBrushSettings(const PyBrushSettings& o) : BrushSettings(o), maskStorage(o.maskStorage), hasMask(o.hasMask) {
        mask = hasMask ? &maskStorage : nullptr;
    }
    PyBrushSettings& operator=(const PyBrushSettings& o) {
        BrushSettings::operator=(o);
        maskStorage = o.maskStorage;
        hasMask = o.hasMask;
        mask = hasMask ? &maskStorage : nullptr;
        return *this;
    }
    void setMask(py::handle m) {
        if (m.is_none()) {
            hasMask = false;
            maskStorage.clear();
            mask = nullptr;
            return;
        }
        maskStorage = m.cast<std::map<GuideId, float>>();
        hasMask = true;
        mask = &maskStorage;
    }
    py::object getMask() const { return hasMask ? py::cast(maskStorage) : py::none(); }
};

BrushWeightFn weightFn(py::handle weight) {
    if (weight.is_none()) return {};
    py::function f = py::reinterpret_borrow<py::function>(weight);
    return [f](const Vec3f& p) { return f(p).cast<float>(); };
}

std::vector<Vec3f> guidePoints(const GuideHierarchy& h, GuideId id) {
    const HierarchyGuide* g = h.guide(id);
    if (!g) throw py::key_error("unknown guide " + std::to_string(id));
    return g->points;
}

}  // namespace

void bindHierarchy(py::module_& m) {
    py::class_<TubeProfile>(m, "TubeProfile",
                            "A guide's tube: radii evenly spaced root to tip (linear between) and a twist in radians.")
        .def(py::init<>())
        .def(py::init([](std::vector<float> radius, float twist) {
                 TubeProfile t;
                 if (!radius.empty()) t.radius = std::move(radius);
                 t.twist = twist;
                 return t;
             }),
             py::arg("radius") = std::vector<float>{}, py::arg("twist") = 0.0f)
        .def_readwrite("radius", &TubeProfile::radius)
        .def_readwrite("twist", &TubeProfile::twist)
        .def("radius_at", &TubeProfile::radiusAt, py::arg("t"), "Radius at t in [0, 1] along the guide.")
        .def("__repr__", [](const TubeProfile& t) {
            std::ostringstream s;
            s << "TubeProfile(radius=[";
            for (std::size_t i = 0; i < t.radius.size(); ++i) s << (i ? ", " : "") << t.radius[i];
            s << "], twist=" << t.twist << ")";
            return s.str();
        });

    py::class_<GuideGroup>(m, "GuideGroup", "A named set of guides; groups form a tree.")
        .def(py::init<>())
        .def_readwrite("id", &GuideGroup::id)
        .def_readwrite("name", &GuideGroup::name)
        .def_readwrite("parent", &GuideGroup::parent)
        .def("__repr__", [](const GuideGroup& g) {
            return "<GuideGroup id=" + std::to_string(g.id) + " name='" + g.name + "' parent=" + std::to_string(g.parent) + ">";
        });

    py::class_<HierarchyGuide>(m, "HierarchyGuide",
                               "A guide in a GuideHierarchy (a copy: edit through the hierarchy's methods).")
        .def(py::init<>())
        .def_readwrite("id", &HierarchyGuide::id)
        .def_readwrite("group", &HierarchyGuide::group)
        .def_readwrite("parent", &HierarchyGuide::parent)
        .def_readwrite("locked", &HierarchyGuide::locked)
        .def_readwrite("root", &HierarchyGuide::root)
        .def_readwrite("tube", &HierarchyGuide::tube)
        .def_readwrite("frame_up", &HierarchyGuide::frameUp)
        .def_readwrite("attributes", &HierarchyGuide::attributes)
        .def_readwrite("point_attributes", &HierarchyGuide::pointAttributes,
                       "Per-point layers: {name: one value per point}, painted along the guide.")
        .def_property("points", [](const HierarchyGuide& g) { return g.points; }, [](HierarchyGuide& g, py::handle p) { g.points = toPoints(p); },
                      "World-space shape.")
        .def_property_readonly("root_local", [](const HierarchyGuide& g) { return g.rootLocal; }, "Shape in the root frame, once bound.")
        .def_property_readonly("frame_up_local", [](const HierarchyGuide& g) { return g.frameUpLocal; })
        .def_property_readonly("offsets", [](const HierarchyGuide& g) { return g.offsets; }, "Following children: shape relative to the parent.")
        .def_property_readonly("is_child", &HierarchyGuide::isChild)
        .def_property_readonly("follows_parent", &HierarchyGuide::followsParent)
        .def("__repr__", [](const HierarchyGuide& g) {
            return "<HierarchyGuide id=" + std::to_string(g.id) + " group=" + std::to_string(g.group) + " parent=" +
                   std::to_string(g.parent) + " points=" + std::to_string(g.points.size()) + ">";
        });

    py::class_<ChildGenerationParams>(m, "ChildGenerationParams", "Settings for GuideHierarchy.generate_children.")
        .def(py::init<>())
        .def(py::init([](std::size_t count, float rootSpread, float tipSpread, float twist, float jitter,
                         float childTubeScale, std::uint64_t seed) {
                 ChildGenerationParams p;
                 p.count = count;
                 p.rootSpread = rootSpread;
                 p.tipSpread = tipSpread;
                 p.twist = twist;
                 p.jitter = jitter;
                 p.childTubeScale = childTubeScale;
                 p.seed = seed;
                 return p;
             }),
             py::arg("count") = 5, py::arg("root_spread") = 0.8f, py::arg("tip_spread") = 1.0f, py::arg("twist") = 0.0f,
             py::arg("jitter") = 0.1f, py::arg("child_tube_scale") = 0.35f, py::arg("seed") = 0)
        .def_readwrite("count", &ChildGenerationParams::count)
        .def_readwrite("root_spread", &ChildGenerationParams::rootSpread)
        .def_readwrite("tip_spread", &ChildGenerationParams::tipSpread)
        .def_readwrite("twist", &ChildGenerationParams::twist)
        .def_readwrite("jitter", &ChildGenerationParams::jitter)
        .def_readwrite("child_tube_scale", &ChildGenerationParams::childTubeScale)
        .def_readwrite("seed", &ChildGenerationParams::seed);

    py::class_<ScalpFit>(m, "ScalpFit",
                         "Where a point of the old scalp lands before the closest-point search when transferring "
                         "a groom to another scalp: identity, or a bounding-box fit.")
        .def(py::init<>())
        .def_readwrite("from_center", &ScalpFit::fromCenter)
        .def_readwrite("to_center", &ScalpFit::toCenter)
        .def_readwrite("scale", &ScalpFit::scale)
        .def("apply", &ScalpFit::apply, py::arg("point"))
        .def_static("identity", &ScalpFit::identity)
        .def_static("bounding_boxes", &ScalpFit::boundingBoxes, py::arg("from_mesh"), py::arg("to_mesh"),
                    "Moves the old scalp's box center onto the new one's and scales by the diagonal ratio.");

    py::class_<TransferMarker>(m, "TransferMarker",
                               "A marker pair for a transfer: a point of the old scalp (old_root, old_point) and the "
                               "matching point of the new one. A side whose root is off its mesh is read at its point.")
        .def(py::init([](const Vec3f& oldPoint, const Vec3f& newPoint, const StrandRoot& oldRoot, const StrandRoot& newRoot) {
                 return TransferMarker{oldRoot, oldPoint, newRoot, newPoint};
             }),
             py::arg("old_point") = Vec3f{}, py::arg("new_point") = Vec3f{}, py::arg("old_root") = StrandRoot{},
             py::arg("new_root") = StrandRoot{})
        .def_readwrite("old_root", &TransferMarker::oldRoot)
        .def_readwrite("old_point", &TransferMarker::oldPoint)
        .def_readwrite("new_root", &TransferMarker::newRoot)
        .def_readwrite("new_point", &TransferMarker::newPoint);
    m.def("transfer_markers_to_json", &transferMarkersToJson, py::arg("markers"));
    m.def("transfer_markers_from_json", &transferMarkersFromJson, py::arg("text"));
    m.def("snap_transfer_markers", &snapTransferMarkers, py::arg("markers"), py::arg("from_mesh"), py::arg("to_mesh"),
          "The markers with both sides on the meshes: roots from points where missing, points from roots.");
    m.def("transfer_methods", [] {
        return std::vector<std::string>{"closest", "bounds", "markersAlign", "markersWarp", "uv"};
    });

    py::class_<Similarity>(m, "Similarity", "p -> scale * rotation * p + translation")
        .def_property_readonly("rotation", [](const Similarity& s) {
            return std::vector<Vec3f>{s.rotation.rows[0], s.rotation.rows[1], s.rotation.rows[2]};
        })
        .def_readonly("scale", &Similarity::scale)
        .def_readonly("translation", &Similarity::translation)
        .def("apply", &Similarity::apply, py::arg("point"))
        .def("inverse", &Similarity::inverse);
    m.def("fit_similarity", [](py::handle a, py::handle b) { return fitSimilarity(toPoints(a), toPoints(b)); },
          py::arg("from_points"), py::arg("to_points"), "The least-squares similarity taking one point list onto the other.");

    py::class_<MappedPoint>(m, "MappedPoint")
        .def_readonly("target", &MappedPoint::target)
        .def_readonly("mapped", &MappedPoint::mapped)
        .def_property_readonly("rotation", [](const MappedPoint& p) {
            return std::vector<Vec3f>{p.rotation.rows[0], p.rotation.rows[1], p.rotation.rows[2]};
        })
        .def_readonly("scale", &MappedPoint::scale);

    py::class_<ScalpMapping>(m, "ScalpMapping",
                             "Where points of one scalp land on another, by a transfer method: closest, bounds, "
                             "markersAlign, markersWarp or uv (transfer.h). Keeps both meshes alive.")
        .def(py::init([](std::shared_ptr<ScalpMesh> from, std::shared_ptr<ScalpMesh> to, const std::string& method,
                         const std::vector<TransferMarker>& markers, bool keepLengths) {
                 TransferOptions options;
                 options.method = transferMethodFromName(method);
                 options.markers = markers;
                 options.keepLengths = keepLengths;
                 return std::make_unique<ScalpMapping>(*from, *to, options);
             }),
             py::arg("from_mesh"), py::arg("to_mesh"), py::arg("method") = "closest",
             py::arg("markers") = std::vector<TransferMarker>{}, py::arg("keep_lengths") = false, py::keep_alive<1, 2>(),
             py::keep_alive<1, 3>())
        .def_property_readonly("method", [](const ScalpMapping& s) { return std::string(transferMethodName(s.method())); },
                               "The method used (markersAlign when a warp fell back).")
        .def("map", &ScalpMapping::map, py::arg("old_root"))
        .def("map_point", &ScalpMapping::mapPoint, py::arg("old_position"))
        .def("back", &ScalpMapping::back, py::arg("new_position"), py::arg("new_root") = StrandRoot{})
        .def("apply", &ScalpMapping::apply, py::arg("point"))
        .def("unapply", &ScalpMapping::unapply, py::arg("point"))
        .def_property_readonly("uv_misses", &ScalpMapping::uvMisses);
    m.def("scalp_mapping_warnings",
          [](std::shared_ptr<ScalpMesh> from, std::shared_ptr<ScalpMesh> to, const std::string& method,
             const std::vector<TransferMarker>& markers) {
              TransferOptions options;
              options.method = transferMethodFromName(method);
              options.markers = markers;
              std::vector<std::string> warnings;
              ScalpMapping mapping(*from, *to, options, &warnings);
              return warnings;
          },
          py::arg("from_mesh"), py::arg("to_mesh"), py::arg("method"), py::arg("markers"),
          "The warnings building that mapping gives (a warp falling back to align).");

    // The Transfer Groom windows' marker editor (transfer_markers_edit.h). A side is "old" or
    // "left" (the groom's scalp), "new" or "right" (the new scalp), or a MarkerSide.
    py::enum_<MarkerSide>(m, "MarkerSide").value("OLD", MarkerSide::Old).value("NEW", MarkerSide::New);
    auto sideOf = [](py::handle side) {
        if (py::isinstance<py::str>(side)) {
            const std::string s = side.cast<std::string>();
            if (s == "old" || s == "left") return MarkerSide::Old;
            if (s == "new" || s == "right") return MarkerSide::New;
            throw py::value_error("marker side '" + s + "': old/left or new/right");
        }
        return side.cast<MarkerSide>();
    };
    py::class_<MarkerPoint>(m, "MarkerPoint",
                            "A clicked marker point: its root on the mesh (face -1: position only), its world "
                            "position, and its symmetry group (nonzero: added with its mirror).")
        .def(py::init([](const Vec3f& point, const StrandRoot& root, std::uint32_t group) {
                 return MarkerPoint{root, point, group};
             }),
             py::arg("point") = Vec3f{}, py::arg("root") = StrandRoot{}, py::arg("group") = 0u)
        .def_readwrite("root", &MarkerPoint::root)
        .def_readwrite("point", &MarkerPoint::point)
        .def_readwrite("group", &MarkerPoint::group);
    auto toPoint = [](py::handle p) {
        if (py::isinstance<MarkerPoint>(p)) return p.cast<MarkerPoint>();
        return MarkerPoint{StrandRoot{}, p.cast<Vec3f>(), 0};
    };
    py::class_<TransferMarkerEditor>(m, "TransferMarkerEditor",
                                     "Two ordered marker point lists (old/left and new/right), paired by index, with "
                                     "deletes that take the partner along, symmetry groups and an undo stack.")
        .def(py::init<>())
        .def(py::init<const std::vector<TransferMarker>&>(), py::arg("pairs"))
        .def("reset", &TransferMarkerEditor::reset, py::arg("pairs"), "Both lists from pairs; no history.")
        .def("set_pairs", &TransferMarkerEditor::setPairs, py::arg("pairs"), "Both lists from pairs: one undo step.")
        .def("points", [sideOf](const TransferMarkerEditor& e, py::handle side) { return e.points(sideOf(side)); },
             py::arg("side"))
        .def("count", [sideOf](const TransferMarkerEditor& e, py::handle side) { return e.count(sideOf(side)); },
             py::arg("side"))
        .def("point", [sideOf](const TransferMarkerEditor& e, py::handle side, std::size_t i) {
                 return e.point(sideOf(side), i);
             },
             py::arg("side"), py::arg("index"))
        .def("add", [sideOf, toPoint](TransferMarkerEditor& e, py::handle side, py::handle p) {
                 return e.add(sideOf(side), toPoint(p));
             },
             py::arg("side"), py::arg("point"), "A point (MarkerPoint or x, y, z) at the end; returns its index.")
        .def("add_mirrored",
             [sideOf, toPoint](TransferMarkerEditor& e, py::handle side, py::handle p, py::handle mirror, float merge) {
                 return e.addMirrored(sideOf(side), toPoint(p), toPoint(mirror), merge);
             },
             py::arg("side"), py::arg("point"), py::arg("mirror"), py::arg("merge_distance") = 0.0f,
             "A point and its mirror, one symmetry group (the mirror skipped within merge_distance).")
        .def("removal_set", [sideOf](const TransferMarkerEditor& e, py::handle side, std::size_t i) {
                 return e.removalSet(sideOf(side), i);
             },
             py::arg("side"), py::arg("index"))
        .def("remove", [sideOf](TransferMarkerEditor& e, py::handle side, std::size_t i) {
                 return e.remove(sideOf(side), i);
             },
             py::arg("side"), py::arg("index"), "Deletes a point, its mirror and their partners; returns the indices.")
        .def("move", [sideOf, toPoint](TransferMarkerEditor& e, py::handle side, std::size_t i, py::handle p, bool step) {
                 e.move(sideOf(side), i, toPoint(p), step);
             },
             py::arg("side"), py::arg("index"), py::arg("point"), py::arg("new_step") = true)
        .def("matched", [sideOf](const TransferMarkerEditor& e, py::handle side, std::size_t i) {
                 return e.matched(sideOf(side), i);
             },
             py::arg("side"), py::arg("index"))
        .def("clear", &TransferMarkerEditor::clear)
        .def("undo", &TransferMarkerEditor::undo)
        .def("redo", &TransferMarkerEditor::redo)
        .def_property_readonly("can_undo", &TransferMarkerEditor::canUndo)
        .def_property_readonly("can_redo", &TransferMarkerEditor::canRedo)
        .def_property_readonly("pair_count", &TransferMarkerEditor::pairCount)
        .def_property_readonly("pending_count", &TransferMarkerEditor::pendingCount)
        .def_property_readonly("revision", &TransferMarkerEditor::revision)
        .def("pairs", &TransferMarkerEditor::pairs, "The complete pairs (TransferMarker), in order.")
        .def("summary", &TransferMarkerEditor::summary, "\"6 pairs, 1 unmatched\"");
    m.def("marker_requirement",
          [](const std::string& method, std::size_t pairs) { return markerRequirement(transferMethodFromName(method), pairs); },
          py::arg("method"), py::arg("pairs"), "Why the method cannot run with that many pairs, or \"\".");
    m.def("mirror_x", &mirrorX, py::arg("point"), py::arg("plane_x") = 0.0f);
    m.def("pick_marker_point",
          [](const ScalpMesh& mesh, const Vec3f& origin, const Vec3f& direction) -> py::object {
              MarkerPoint hit;
              if (!pickMarkerPoint(mesh, origin, direction, &hit)) return py::none();
              return py::cast(hit);
          },
          py::arg("mesh"), py::arg("origin"), py::arg("direction"), "The ray's hit as a MarkerPoint, or None.");

    m.attr("NO_GUIDE") = kNoGuide;
    m.attr("NO_GROUP") = kNoGroup;

    py::class_<GuideHierarchy, std::shared_ptr<GuideHierarchy>>(
        m, "GuideHierarchy",
        "Primary guides, children generated inside their parents' tubes, and so on. Children store offsets in "
        "the parent's frame, so editing a parent carries every descendant. Call evaluate(scalp) after edits.")
        .def(py::init<>())
        .def(py::init<const GuideHierarchy&>(), py::arg("other"))
        .def("copy", [](const GuideHierarchy& h) { return GuideHierarchy(h); })
        .def("__copy__", [](const GuideHierarchy& h) { return GuideHierarchy(h); })
        .def("__deepcopy__", [](const GuideHierarchy& h, py::dict) { return GuideHierarchy(h); }, py::arg("memo"))
        // Groups.
        .def("add_group", &GuideHierarchy::addGroup, py::arg("name"), py::arg("parent") = kNoGroup, "Adds a group; returns its ID.")
        .def_property_readonly("groups", [](const GuideHierarchy& h) { return h.groups(); })
        .def("group",
             [](const GuideHierarchy& h, GroupId id) -> py::object {
                 const GuideGroup* g = h.group(id);
                 return g ? py::cast(*g) : py::none();
             },
             py::arg("id"), "The group, or None.")
        .def("find_group", &GuideHierarchy::findGroup, py::arg("name"), "Group ID by name (NO_GROUP if absent).")
        .def("group_depth", &GuideHierarchy::groupDepth, py::arg("id"))
        .def("max_depth", &GuideHierarchy::maxDepth)
        .def("remove_group", &GuideHierarchy::removeGroup, py::arg("id"), "Removes a group, its sub-groups and their guides.")
        .def("rename_group", &GuideHierarchy::renameGroup, py::arg("id"), py::arg("name"))
        // Guides.
        .def("add_guide",
             [](GuideHierarchy& h, GroupId group, py::handle points, const TubeProfile& tube, GuideId id) {
                 return h.addGuide(group, toPoints(points), tube, id);
             },
             py::arg("group"), py::arg("points"), py::arg("tube") = TubeProfile{}, py::arg("id") = kNoGuide,
             "Adds a primary guide to a root group; returns its ID.")
        .def("add_guides", &GuideHierarchy::addGuides, py::arg("group"), py::arg("guides"), py::arg("tube") = TubeProfile{},
             "Adds every guide of a GuideSet as primaries of a root group; returns the IDs. Evaluate after.")
        .def_property_readonly("guides", [](const GuideHierarchy& h) { return h.guides(); }, "Every guide, as copies.")
        .def("guide",
             [](const GuideHierarchy& h, GuideId id) -> py::object {
                 const HierarchyGuide* g = h.guide(id);
                 return g ? py::cast(*g) : py::none();
             },
             py::arg("id"), "A copy of the guide, or None.")
        .def("guide_points", &guidePoints, py::arg("id"), "A guide's world points (KeyError for an unknown ID).")
        .def("guide_ids", [](const GuideHierarchy& h) {
                std::vector<GuideId> ids;
                for (const auto& g : h.guides()) ids.push_back(g.id);
                return ids;
            })
        .def("guides_in_group", &GuideHierarchy::guidesInGroup, py::arg("group"))
        .def("children_of", &GuideHierarchy::childrenOf, py::arg("parent"))
        .def("generate_children",
             [](GuideHierarchy& h, GroupId parentGroup, const std::string& childGroupName,
                const ChildGenerationParams& params, py::object parents) {
                 std::vector<GuideId> ids;
                 const std::vector<GuideId>* p = nullptr;
                 if (!parents.is_none()) {
                     ids = parents.cast<std::vector<GuideId>>();
                     p = &ids;
                 }
                 py::gil_scoped_release release;
                 return h.generateChildren(parentGroup, childGroupName, params, p);
             },
             py::arg("parent_group"), py::arg("child_group_name"), py::arg("params") = ChildGenerationParams{},
             py::arg("parents") = py::none(),
             "Generates children for each parent (default: every guide of the group) into a child group; returns it.")
        .def("add_child_guide",
             [](GuideHierarchy& h, GroupId group, GuideId parent, py::handle points, std::optional<TubeProfile> tube) {
                 return h.addChildGuide(group, parent, toPoints(points), std::move(tube));
             },
             py::arg("group"), py::arg("parent"), py::arg("points"), py::arg("tube") = py::none(),
             "Adds a hand-placed child of `parent` with a world shape; returns its ID. Without a tube it takes "
             "the parent's, scaled by 0.35.")
        // Evaluation.
        .def("evaluate", &GuideHierarchy::evaluate, py::arg("scalp") = nullptr, py::call_guard<py::gil_scoped_release>(),
             "Binds guides to the scalp and recomputes every following child. scalp may be None.")
        .def("bound_topology", &GuideHierarchy::boundTopology)
        .def("evaluate_subtrees", &GuideHierarchy::evaluateSubtrees, py::arg("scalp"), py::arg("changed"),
             py::call_guard<py::gil_scoped_release>(), "Re-evaluates the given guides and their descendants.")
        .def("pose", [](GuideHierarchy& h, GuideId id, py::handle points) { h.pose(id, toPoints(points)); }, py::arg("id"), py::arg("points"),
             "Sets a guide's world points for this evaluation only (a simulated pose).")
        .def("evaluate_descendants", &GuideHierarchy::evaluateDescendants, py::arg("scalp"), py::arg("guides"),
             py::call_guard<py::gil_scoped_release>())
        // Hierarchy tools.
        .def("reparent", &GuideHierarchy::reparent, py::arg("guides"), py::arg("new_parent"))
        .def("promote_group", &GuideHierarchy::promoteGroup, py::arg("group"))
        .def("demote_group", &GuideHierarchy::demoteGroup, py::arg("group"), py::arg("new_parent"))
        .def("mirror_group", &GuideHierarchy::mirrorGroup, py::arg("group"), py::arg("axis"), py::arg("center"),
             py::arg("from_positive"), "Mirrors a root group across a plane; returns the new IDs.")
        .def("mirror_all", &GuideHierarchy::mirrorAll, py::arg("axis"), py::arg("center"), py::arg("from_positive"),
             "Makes the whole groom symmetric; returns the new IDs.")
        .def("transfer", py::overload_cast<const ScalpMesh&, const ScalpMesh&, const ScalpFit&>(&GuideHierarchy::transfer),
             py::arg("from_mesh"), py::arg("to_mesh"), py::arg("fit") = ScalpFit{},
             py::call_guard<py::gil_scoped_release>(), "Moves every guide onto another scalp.")
        .def("transfer", py::overload_cast<const ScalpMapping&>(&GuideHierarchy::transfer), py::arg("mapping"),
             py::call_guard<py::gil_scoped_release>(), "Moves every guide onto another scalp by a ScalpMapping.")
        .def("remove_guides", &GuideHierarchy::removeGuides, py::arg("guides"), "Removes guides and their descendants.")
        .def("lock", &GuideHierarchy::lock, py::arg("id"))
        .def("unlock", &GuideHierarchy::unlock, py::arg("id"))
        .def("set_points", [](GuideHierarchy& h, GuideId id, py::handle points) { h.setPoints(id, toPoints(points)); }, py::arg("id"), py::arg("points"),
             "Replaces a guide's world shape (a brush edit).")
        .def("set_tube", &GuideHierarchy::setTube, py::arg("id"), py::arg("tube"))
        .def("set_attribute", &GuideHierarchy::setAttribute, py::arg("id"), py::arg("name"), py::arg("value"))
        .def("set_point_attribute", &GuideHierarchy::setPointAttribute, py::arg("id"), py::arg("name"), py::arg("values"),
             "Sets a per-point layer (resampled to the guide's points; empty removes it).")
        // Output.
        .def("flatten", &GuideHierarchy::flatten, py::arg("level") = -1, "Guides of every group at a level (-1: deepest) as a GuideSet.")
        .def("flatten_group", &GuideHierarchy::flattenGroup, py::arg("group"))
        .def("effective_attributes", &GuideHierarchy::effectiveAttributes, py::arg("guide"))
        .def("effective_attribute",
             [](const GuideHierarchy& h, GuideId id, const std::string& name) -> py::object {
                 const HierarchyGuide* g = h.guide(id);
                 if (!g) throw py::key_error("unknown guide " + std::to_string(id));
                 float v = 0.0f;
                 if (!h.effectiveAttribute(*g, name, &v)) return py::none();
                 return py::float_(v);
             },
             py::arg("id"), py::arg("name"), "An attribute as inherited down the chain, or None.")
        .def("has_attributes", &GuideHierarchy::hasAttributes)
        .def("effective_point_attributes", &GuideHierarchy::effectivePointAttributes, py::arg("guide"),
             "Per-point layers, own over inherited, each resampled to the guide's points.")
        .def("has_point_attributes", &GuideHierarchy::hasPointAttributes)
        .def("point_attribute_names", &GuideHierarchy::pointAttributeNames)
        .def("attribute_names", &GuideHierarchy::attributeNames)
        .def("remove_attribute", &GuideHierarchy::removeAttribute, py::arg("name"))
        .def("rename_attribute", &GuideHierarchy::renameAttribute, py::arg("old"), py::arg("new"))
        .def("content_hash", &GuideHierarchy::contentHash)
        .def("to_json", &GuideHierarchy::toJson)
        .def_static("from_json", &GuideHierarchy::fromJson, py::arg("text"))
        .def("__len__", [](const GuideHierarchy& h) { return h.guides().size(); })
        .def("__repr__", [](const GuideHierarchy& h) {
            return "<GuideHierarchy groups=" + std::to_string(h.groups().size()) + " guides=" + std::to_string(h.guides().size()) + ">";
        });

    // ---- Brushes -----------------------------------------------------------
    py::enum_<BrushMode>(m, "BrushMode", "Grooming brush modes.")
        .value("Comb", BrushMode::Comb)
        .value("Smooth", BrushMode::Smooth)
        .value("Lift", BrushMode::Lift)
        .value("Length", BrushMode::Length)
        .value("ScaleTube", BrushMode::ScaleTube)
        .value("Attribute", BrushMode::Attribute)
        .value("Twist", BrushMode::Twist)
        .value("Curl", BrushMode::Curl)
        .value("Straighten", BrushMode::Straighten)
        .value("Attract", BrushMode::Attract)
        .value("Erase", BrushMode::Erase)
        .value("Cut", BrushMode::Cut)
        .value("Slide", BrushMode::Slide)
        .value("Direction", BrushMode::Direction)
        .value("Noise", BrushMode::Noise);
    m.def("brush_mode_from_string", &brushModeFromString, py::arg("name"), "Parses a mode name (comb, smooth, lift, ...).");
    m.def("brush_mode_name", &brushModeName, py::arg("mode"));

    py::class_<PyBrushSettings>(m, "BrushSettings",
                                "A brush: a world-space sphere with smooth falloff. mode, radius, strength, the level "
                                "or group edited, symmetry, collision and an optional soft-selection mask.")
        .def(py::init<>())
        .def(py::init([](BrushMode mode, float radius, float strength) {
                 PyBrushSettings s;
                 s.mode = mode;
                 s.radius = radius;
                 s.strength = strength;
                 return s;
             }),
             py::arg("mode"), py::arg("radius") = 0.25f, py::arg("strength") = 1.0f)
        .def_readwrite("mode", &PyBrushSettings::mode)
        .def_readwrite("radius", &PyBrushSettings::radius)
        .def_readwrite("strength", &PyBrushSettings::strength)
        .def_readwrite("level", &PyBrushSettings::level)
        .def_readwrite("group", &PyBrushSettings::group)
        .def_readwrite("propagate", &PyBrushSettings::propagate)
        .def_readwrite("attribute", &PyBrushSettings::attribute)
        .def_readwrite("value", &PyBrushSettings::value)
        .def_readwrite("pointwise", &PyBrushSettings::pointwise,
                       "Attribute mode: paint a per-point layer along the guides instead of one value per guide.")
        .def_property("mask", &PyBrushSettings::getMask, &PyBrushSettings::setMask,
                      "Only these guides ({id: weight}), or None for every target guide.")
        .def_readwrite("mirror_axis", &PyBrushSettings::mirrorAxis)
        .def_readwrite("mirror_center", &PyBrushSettings::mirrorCenter)
        .def_readwrite("collide", &PyBrushSettings::collide)
        .def_readwrite("collide_offset", &PyBrushSettings::collideOffset)
        .def_readwrite("stretch", &PyBrushSettings::stretch)
        .def_readwrite("freeze", &PyBrushSettings::freeze)
        .def_readwrite("hidden_groups", &PyBrushSettings::hiddenGroups);

    m.def("keep_off_scalp", [](py::handle pts, const ScalpMesh& scalp, float offset) { return keepOffScalp(toPoints(pts), scalp, offset); },
          py::arg("points"), py::arg("scalp"), py::arg("offset") = 0.0f, "Pushes a guide's points out of the scalp, lengths kept.");
    m.def("mirror_point", &mirrorPoint, py::arg("point"), py::arg("axis"), py::arg("center"));
    m.def("mirror_vector", &mirrorVector, py::arg("vector"), py::arg("axis"));

    m.def("apply_brush_stamp",
          [](GuideHierarchy& h, const ScalpMesh* scalp, const PyBrushSettings& settings, const Vec3f& center,
             const Vec3f& drag, py::handle weight) {
              if (weight.is_none()) {
                  py::gil_scoped_release release;
                  return applyBrushStamp(h, scalp, settings, center, drag);
              }
              return applyBrushStamp(h, scalp, settings, center, drag, weightFn(weight));
          },
          py::arg("hierarchy"), py::arg("scalp"), py::arg("settings"), py::arg("center"), py::arg("drag") = Vec3f{},
          py::arg("weight") = py::none(),
          "One brush stamp at `center` with the drag since the last stamp; returns the guides changed. "
          "`weight` may be a callable(Vec3) -> float replacing the sphere falloff.");
    m.def("apply_brush_stroke",
          [](GuideHierarchy& h, const ScalpMesh* scalp, const PyBrushSettings& settings, py::handle path) {
              const std::vector<Vec3f> pts = toPoints(path);
              py::gil_scoped_release release;
              return applyBrushStroke(h, scalp, settings, pts);
          },
          py::arg("hierarchy"), py::arg("scalp"), py::arg("settings"), py::arg("path"),
          "A stroke along `path` (stamps at most radius/4 apart); returns the number of distinct guides changed.");

    // The same brushes on flow arrows and part lines (see brush.h for what each mode does to them).
    m.def("apply_brush_stamp_to_flow",
          [](FlowSet& flow, const ScalpMesh& scalp, const PyBrushSettings& settings, const Vec3f& center, const Vec3f& drag,
             py::handle weight) {
              if (weight.is_none()) {
                  py::gil_scoped_release release;
                  return applyBrushStampToFlow(flow, scalp, settings, center, drag);
              }
              return applyBrushStampToFlow(flow, scalp, settings, center, drag, weightFn(weight));
          },
          py::arg("flow"), py::arg("scalp"), py::arg("settings"), py::arg("center"), py::arg("drag") = Vec3f{},
          py::arg("weight") = py::none(),
          "One brush stamp on a FlowSet's pins and sinks (comb, smooth, twist, length, attract, straighten, erase); "
          "returns how many changed.");
    m.def("apply_brush_stroke_to_flow",
          [](FlowSet& flow, const ScalpMesh& scalp, const PyBrushSettings& settings, py::handle path) {
              const std::vector<Vec3f> pts = toPoints(path);
              py::gil_scoped_release release;
              return applyBrushStrokeToFlow(flow, scalp, settings, pts);
          },
          py::arg("flow"), py::arg("scalp"), py::arg("settings"), py::arg("path"),
          "A brush stroke along `path` on a FlowSet; returns the distinct pins and sinks changed.");
    m.def("apply_brush_stamp_to_parts",
          [](PartSet& parts, const ScalpMesh& scalp, const PyBrushSettings& settings, const Vec3f& center, const Vec3f& drag,
             py::handle weight) {
              if (weight.is_none()) {
                  py::gil_scoped_release release;
                  return applyBrushStampToParts(parts, scalp, settings, center, drag);
              }
              return applyBrushStampToParts(parts, scalp, settings, center, drag, weightFn(weight));
          },
          py::arg("parts"), py::arg("scalp"), py::arg("settings"), py::arg("center"), py::arg("drag") = Vec3f{},
          py::arg("weight") = py::none(),
          "One brush stamp on a PartSet's lines (comb, smooth, straighten, attract, cut, erase); returns the parts changed.");
    m.def("apply_brush_stroke_to_parts",
          [](PartSet& parts, const ScalpMesh& scalp, const PyBrushSettings& settings, py::handle path) {
              const std::vector<Vec3f> pts = toPoints(path);
              py::gil_scoped_release release;
              return applyBrushStrokeToParts(parts, scalp, settings, pts);
          },
          py::arg("parts"), py::arg("scalp"), py::arg("settings"), py::arg("path"),
          "A brush stroke along `path` on a PartSet; returns the distinct parts changed.");

    py::class_<PlaceSettings>(m, "PlaceSettings", "How new guides are shaped by place_guide, place_guides and draw_guide.")
        .def(py::init<>())
        .def_readwrite("group", &PlaceSettings::group)
        .def_readwrite("length", &PlaceSettings::length)
        .def_readwrite("segments", &PlaceSettings::segments)
        .def_readwrite("tube", &PlaceSettings::tube)
        .def_readwrite("match_neighbors", &PlaceSettings::matchNeighbors)
        .def_readwrite("spacing", &PlaceSettings::spacing);
    m.def("place_guide",
          [](GuideHierarchy& h, const ScalpMesh& scalp, const PlaceSettings& s, const SurfacePoint& at) {
              return placeGuide(h, scalp, s, at);
          },
          py::arg("hierarchy"), py::arg("scalp"), py::arg("settings"), py::arg("at"),
          "Adds a primary guide at a scalp point; returns its ID.");
    m.def("place_guides",
          [](GuideHierarchy& h, const ScalpMesh& scalp, const PlaceSettings& s, py::handle path) {
              return placeGuides(h, scalp, s, toPoints(path));
          },
          py::arg("hierarchy"), py::arg("scalp"), py::arg("settings"), py::arg("path"),
          "A guide at each path point at least `spacing` from every root of the group; returns the new IDs.");
    m.def("draw_guide",
          [](GuideHierarchy& h, const ScalpMesh& scalp, const PlaceSettings& s, py::handle path) {
              return drawGuide(h, scalp, s, toPoints(path));
          },
          py::arg("hierarchy"), py::arg("scalp"), py::arg("settings"), py::arg("path"),
          "A primary guide shaped like `path`, rooted near path[0]; returns its ID.");
    m.def("place_straight_guide",
          [](GuideHierarchy& h, const ScalpMesh& scalp, const std::string& group, const SurfacePoint& at, float length,
             int segments, const TubeProfile& tube) {
              PlaceSettings s;
              s.group = group;
              s.length = length;
              s.segments = segments;
              s.tube = tube;
              s.matchNeighbors = false;
              return placeGuide(h, scalp, s, at);
          },
          py::arg("hierarchy"), py::arg("scalp"), py::arg("group"), py::arg("at"), py::arg("length"), py::arg("segments"),
          py::arg("tube") = TubeProfile{}, "A straight standing guide (no neighbor matching); returns its ID.");

    // ---- Selection ---------------------------------------------------------
    m.def(
        "guides_at",
        [](const GuideHierarchy& h, int level, const std::string& group, const std::set<std::string>& hidden) {
            return guidesAt(h, level, group, hidden);
        },
        py::arg("hierarchy"), py::arg("level"), py::arg("group") = std::string{}, py::arg("hidden_groups") = std::set<std::string>{},
        "Guides of a level (-1: every level) or of a group, less those of hidden groups.");
    m.def("guides_in_lasso",
          [](const GuideHierarchy& h, const std::vector<GuideId>& candidates, py::function project,
             const std::vector<std::array<float, 2>>& polygon, bool rootsOnly) {
              ViewProjector proj = [project](const Vec3f& world, std::array<float, 2>* view) {
                  py::object r = project(world);
                  if (r.is_none()) return false;
                  *view = r.cast<std::array<float, 2>>();
                  return true;
              };
              return guidesInLasso(h, candidates, proj, polygon, rootsOnly);
          },
          py::arg("hierarchy"), py::arg("candidates"), py::arg("project"), py::arg("polygon"), py::arg("roots_only") = true,
          "Candidates inside a view-space polygon; project(Vec3) returns (x, y) or None when behind the camera.");
    m.def("soft_selection", &softSelection, py::arg("hierarchy"), py::arg("selected"), py::arg("radius"),
          "{id: weight}: selected guides 1, others falling off with root distance to 0 at radius.");
    m.def("mirror_selection", &mirrorSelection, py::arg("hierarchy"), py::arg("selected"), py::arg("axis"),
          py::arg("center"), py::arg("tolerance"), "The guides mirroring the selected ones across a plane.");

    // ---- Correctives ---------------------------------------------------------
    py::class_<CorrectiveLayer>(m, "CorrectiveLayer",
                                "A shot-sculpt layer: per-guide frame-local offsets at a time, with fades or a weight curve.")
        .def(py::init<>())
        .def_readwrite("name", &CorrectiveLayer::name)
        .def_readwrite("time", &CorrectiveLayer::time)
        .def_readwrite("fade_in", &CorrectiveLayer::fadeIn)
        .def_readwrite("fade_out", &CorrectiveLayer::fadeOut)
        .def_readwrite("hold_before", &CorrectiveLayer::holdBefore)
        .def_readwrite("hold_after", &CorrectiveLayer::holdAfter)
        .def_readwrite("weight", &CorrectiveLayer::weight)
        .def_readwrite("enabled", &CorrectiveLayer::enabled)
        .def_readwrite("slot", &CorrectiveLayer::slot)
        .def_readwrite("weight_curve", &CorrectiveLayer::weightCurve)
        .def_readwrite("offsets", &CorrectiveLayer::offsets, "{guide id: [Vec3 per point]} (a copy; assign back to change).")
        .def("weight_at", &CorrectiveLayer::weightAt, py::arg("seconds"))
        .def("__repr__", [](const CorrectiveLayer& l) {
            return "<CorrectiveLayer '" + l.name + "' time=" + std::to_string(l.time) + " guides=" + std::to_string(l.offsets.size()) + ">";
        });

    py::class_<Correctives>(m, "Correctives", "Corrective layers on top of an animated groom.")
        .def(py::init<>())
        .def_readwrite("layers", &Correctives::layers, "The layers (a copy; use layer(name) to edit one in place).")
        .def_property_readonly("empty", &Correctives::empty)
        .def("layer",
             [](Correctives& c, const std::string& name) -> CorrectiveLayer* { return c.layer(name); },
             py::arg("name"), py::return_value_policy::reference_internal, "The layer by name (editable in place), or None.")
        .def("add_layer", [](Correctives& c, const CorrectiveLayer& l) { c.layers.push_back(l); }, py::arg("layer"))
        .def("remove_layer",
             [](Correctives& c, const std::string& name) {
                 for (auto it = c.layers.begin(); it != c.layers.end(); ++it) {
                     if (it->name == name) {
                         c.layers.erase(it);
                         return true;
                     }
                 }
                 return false;
             },
             py::arg("name"))
        .def("to_json", &Correctives::toJson)
        .def_static("from_json", &Correctives::fromJson, py::arg("text"))
        .def("apply",
             [](const Correctives& c, GuideHierarchy& h, double seconds, const ScalpMesh* scalp, py::object skipOwn) {
                 std::set<GuideId> skip;
                 const std::set<GuideId>* p = nullptr;
                 if (!skipOwn.is_none()) {
                     skip = skipOwn.cast<std::set<GuideId>>();
                     p = &skip;
                 }
                 py::gil_scoped_release release;
                 c.apply(h, seconds, scalp, p);
             },
             py::arg("hierarchy"), py::arg("seconds"), py::arg("scalp") = nullptr, py::arg("skip_own") = py::none(),
             "Applies every enabled layer at `seconds` to an evaluated hierarchy.")
        .def("record", &Correctives::record, py::arg("name"), py::arg("posed"), py::arg("sculpted"), py::arg("seconds"),
             py::arg("scalp") = nullptr,
             "Adds a sculpt (sculpted vs posed) to layer `name`, created at `seconds` if missing; returns the guides that got offsets.");
}

}  // namespace groomlab::py_bind
