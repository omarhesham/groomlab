// SPDX-License-Identifier: Apache-2.0
// StrandMesh: the strands it is handed as a polygon mesh, tubes or flat ribbons (strand_geometry.h),
// that follows the groom's animation (the scalp, Simulate, keyed params). The node hands its strands
// on as they are, so it sits anywhere in the chain, like Cards: where it sits decides what the mesh
// shows (after Simulate, before Noise...). Hosts make the mesh, only while something takes it
// (GroomSession::evaluateStrandMeshes; Maya's groom outStrandMesh), from an evaluation at the node's
// density; a frame where only the points moved keeps the mesh's topology and UVs.

#include <memory>
#include <vector>

#include "groomlab/graph.h"
#include "groomlab/operators.h"
#include "internal.h"

namespace groomlab {

namespace {

class StrandMeshOp : public Operator {
public:
    std::string typeName() const override { return op::kStrandMesh; }
    std::string category() const override { return "convert"; }
    std::vector<PortSpec> inputs() const override { return {{"strands", PortType::Strands}}; }
    std::vector<ParamSpec> paramSpecs() const override {
        return {
            notAnimatable({"style", std::string("tube"), "tube: a closed tube round each strand (sides around, a fan at the "
                                                         "tip); ribbon: a flat strip lying along the scalp",
                           "", {"tube", "ribbon"}}),
            notAnimatable({"sides", std::int64_t{4}, "A tube's sides"}).range(3, 64).soft(3, 12).when("style", {"tube"}),
            ParamSpec{"widthScale", 1.0, "Times each point's width (the strand's diameter)"}.range(0.0001, 1000).soft(0.1, 10),
            notAnimatable({"density", 1.0, "The share of the strands evaluated for the mesh (as the preview density thins "
                                           "them: a subset of a higher density's)",
                           "fraction"})
                .range(0.0001, 1),
            notAnimatable({"maxStrands", std::int64_t{20000}, "At most this many strands make the mesh (by ID, as density "
                                                              "picks them; 0: all). Tube meshes are heavy: 4 sides make "
                                                              "about 4 vertices per strand point"})
                .range(0, 100000000)
                .soft(0, 100000),
            notAnimatable({"strandUVs", false, "A second UV set (strandUVs) laying every strand out in its own cell of a "
                                               "grid, for baking per strand"}),
        };
    }

    // Its strands as they came: the mesh is the host's, made from them (GroomSession::evaluateStrandMeshes).
    Value evaluate(const EvalContext&, const std::vector<Value>& inputs) const override {
        return requireStrands(inputs, op::kStrandMesh);
    }
};

}  // namespace

void registerStrandMeshOperator(OperatorRegistry& registry) {
    registry.registerType(op::kStrandMesh,
                          {"convert",
                           "The strands it is handed as a polygon mesh (tubes or ribbons) that follows the animation: Maya's groom outStrandMesh, for Alembic or a renderer without curves.",
                           "polygon tubes ribbons geometry"},
                          [] { return std::make_unique<StrandMeshOp>(); });
}

}  // namespace groomlab
