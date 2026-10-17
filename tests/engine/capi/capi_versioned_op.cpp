// SPDX-License-Identifier: Apache-2.0
// An operator at version 2, as a built-in one becomes after a change of behaviour (none has yet),
// for the C API's node upgrade tests: registered at startup in the core the test links, which the
// C API is compiled into statically there (GROOMLAB_C_STATIC). Version 1 passes its strands
// through; version 2 raises them by "lift", the param it added.

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "groomlab/graph.h"
#include "groomlab/strand_set.h"

namespace {

using namespace groomlab;

class CApiLiftOp : public Operator {
public:
    std::string typeName() const override { return "TestCApiLift"; }
    std::vector<PortSpec> inputs() const override { return {{"strands", PortType::Strands}}; }
    PortSpec output() const override { return {"out", PortType::Strands}; }
    int version() const override { return 2; }
    std::vector<ParamSpec> paramSpecs() const override { return {ParamSpec{"lift", 1.0, "How far version 2 raises the strands"}}; }
    Value evaluate(const EvalContext&, const std::vector<Value>& inputs) const override {
        const auto* in = std::get_if<StrandsPtr>(&inputs.at(0));
        if (!in || !*in) throw GraphError("TestCApiLift: input not connected");
        if (nodeVersion() < 2) return *in;
        auto out = std::make_shared<StrandSet>(**in);
        const float lift = static_cast<float>(params().getDouble("lift"));
        for (Vec3f& p : out->mutablePositions()) p.y += lift;
        return StrandsPtr(out);
    }
};

const bool kRegistered = [] {
    OperatorRegistry::instance().registerType("TestCApiLift", {}, [] { return std::make_unique<CApiLiftOp>(); });
    return true;
}();

}  // namespace
