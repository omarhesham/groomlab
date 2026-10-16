// SPDX-License-Identifier: Apache-2.0
// Compound nodes (PG-6): a subgraph packaged as one node.
//
//   {"name": "frizzy", "type": "Compound", "params": {
//       "definition": {
//           "graph": {<graph JSON; Input nodes stand for the compound's inputs>},
//           "inputs": [{"name": "strands", "type": "Strands"}],
//           "expose": {"amount": "noise.amplitude"}},
//       "amount": 0.05}}
//
// An Input node ({"type": "Input", "params": {"name": "strands", "type":
// "Strands"}}) brings the compound input of that name into the subgraph; the
// subgraph's output node gives the compound's output: one output, the output node's first
// (inner links may read any output of an inner node). Exposed params set
// the named inner node params. The outer graph caches the compound as a whole,
// and the inner graph keeps its own cache too: an exposed param tweak or an
// upstream change re-runs only the inner nodes it reaches (a preset inserted as
// a compound costs what its nodes inline would). Each compound input reaches the
// inner graph as an external whose hash is the value's identity: the compound
// holds the last evaluation's inputs, so the same address is the same object
// (one freed cannot be taken by another while held), and a later pass of
// `repeat` salts its hash with the pass.
//
// With `repeat` above 1 the inner graph runs that many passes, each taking
// the previous pass's output as the compound's first Strands/Curves input
// (the other inputs stay). An Input node named "iteration" of type "Field"
// yields the pass index (0-based) as a constant field, so inner params
// exposed as inputs can vary per pass.

#include <algorithm>
#include <memory>
#include <mutex>
#include <set>
#include <type_traits>

#include "groomlab/field.h"
#include "groomlab/graph.h"
#include "groomlab/graph_io.h"
#include "groomlab/operators.h"
#include "groomlab/random.h"
#include "internal.h"

namespace groomlab {

namespace {

const std::string kInputPrefix = "__input:";
const std::string kIterationInput = "iteration";  // the pass index, as a Field input
constexpr std::int64_t kMaxRepeat = 64;

class InputOp : public Operator {
public:
    std::string typeName() const override { return op::kInput; }
    std::string category() const override { return "source"; }
    std::vector<PortSpec> inputs() const override { return {}; }
    PortSpec output() const override {
        PortType type = PortType::Strands;
        try {
            type = portTypeFromName(params().getString("type"));
        } catch (const GraphError&) {
        }
        return {"out", type};
    }
    std::vector<ParamSpec> paramSpecs() const override {
        return {ParamSpec{"name", std::string("strands"),
                          "Compound input this node stands for; \"iteration\" with type Field: the repeat pass index"}
                    .noInput(),
                ParamSpec{"type", std::string("Strands"),
                          "Port type: Strands, Guides, Curves, Mesh, Field, Curve or Value (one value of any kind)", "",
                          {"Strands", "Guides", "Curves", "Mesh", "Field", "Curve", "Value"}}
                    .noInput()};
    }
    std::vector<std::string> externalInputs() const override { return {kInputPrefix + params().getString("name")}; }
    Value evaluate(const EvalContext& ctx, const std::vector<Value>&) const override {
        return ctx.external(kInputPrefix + params().getString("name"));
    }
};

class CompoundOp : public Operator {
public:
    std::string typeName() const override { return op::kCompound; }
    std::string category() const override { return "utility"; }
    std::vector<ParamSpec> paramSpecs() const override {
        return {ParamSpec{"definition", std::string("{}"), "The subgraph, its inputs and exposed params (JSON)"}.noInput(),
                notAnimatable({"repeat", std::int64_t{1},
                               "Passes of the subgraph, each fed the previous pass's output as the first strands "
                               "input (1-64); an inner Input named iteration of type Field gives the pass index"})
                    .range(1, static_cast<double>(kMaxRepeat))
                    .soft(1, 16)};
    }
    std::vector<PortSpec> inputs() const override {
        const auto d = definition();
        return d ? d->inputs : std::vector<PortSpec>{};
    }
    // The exposed params, each its inner param's spec under its own name: each can be an input of
    // the compound's node, a Value port (one value, set on the inner param as the compound's own
    // value is). A field or curve reaches an inner param through an Input node instead.
    std::vector<ParamSpec> extraParamSpecs() const override {
        const auto d = definition();
        if (!d) return {};
        std::lock_guard<std::mutex> lock(d->mutex);
        std::vector<ParamSpec> out;
        for (const auto& [exposed, target] : d->expose) {
            const Operator& inner = d->doc->graph.node(d->doc->node(target.first));
            for (const ParamSpec& s : nodeParamSpecs(inner)) {  // (a compound inside: its exposed ones too)
                if (s.name != target.second) continue;
                ParamSpec spec = s;
                spec.name = exposed;
                spec.showWhen = {};
                // The value it has where the compound sets none: the inner node's.
                if (inner.params().has(s.name)) spec.defaultValue = inner.params().get(s.name);
                if (spec.port != ParamSpec::Port::None) spec.port = ParamSpec::Port::Value;
                out.push_back(std::move(spec));
            }
        }
        return out;
    }
    PortSpec output() const override {
        const auto d = definition();
        return d ? d->output : PortSpec{"out", PortType::Strands};
    }
    std::vector<std::string> externalInputs() const override {
        const auto d = definition();
        return d ? d->externals : std::vector<std::string>{};
    }
    // What the inner nodes would add to their own keys (the inner graph keeps
    // no cache of its own): their salts (file stamps), and the context they
    // read (a Cards node's buildCards, an ImportCurves node's time) with their
    // map params' texture stamps.
    std::uint64_t cacheSalt() const override {
        const auto d = definition();
        if (!d) return 0;
        std::lock_guard<std::mutex> lock(d->mutex);
        applyExposed(*d);
        applyPaths(*d);
        std::uint64_t salt = hash64(0xC0B);
        for (const NodeId id : d->doc->graph.nodeIds()) salt = hashCombine(salt, d->doc->graph.node(id).cacheSalt());
        return salt;
    }
    std::uint64_t contextSalt(const EvalContext& ctx) const override {
        const auto d = definition();
        if (!d) return 0;
        std::lock_guard<std::mutex> lock(d->mutex);
        applyExposed(*d);
        applyPaths(*d);
        std::uint64_t salt = hash64(0xC0C);
        for (const NodeId id : d->doc->graph.nodeIds()) {
            const Operator& op = d->doc->graph.node(id);
            salt = hashCombine(hashCombine(salt, op.contextSalt(ctx)), mapParamsKey(op.params(), ctx));
        }
        return salt;
    }

    Value evaluate(const EvalContext& ctx, const std::vector<Value>& inputs) const override {
        const auto d = definition();
        if (!d) throw GraphError("Compound: " + definitionError());
        std::lock_guard<std::mutex> lock(d->mutex);
        Graph& graph = d->doc->graph;
        applyExposed(*d);
        applyPaths(*d);
        ExternalInputs inner;
        inner.setParent(ctx.externals);
        for (std::size_t i = 0; i < d->inputs.size() && i < inputs.size(); ++i) {
            inner.set(kInputPrefix + d->inputs[i].name, inputs[i], identityHash(inputs[i], 0));
        }
        EvalContext child = ctx;
        child.externals = &inner;
        child.keepPreviewCore = true;  // its result as the inner nodes made it (hidden strands kept)
        // The first strands input, which each further pass replaces with the last pass's output.
        std::size_t chained = d->inputs.size();
        for (std::size_t i = 0; i < d->inputs.size() && i < inputs.size(); ++i) {
            if (d->inputs[i].type == PortType::Strands || d->inputs[i].type == PortType::Curves) {
                chained = i;
                break;
            }
        }
        const auto repeat = static_cast<std::size_t>(params().getInt("repeat"));
        Value result;
        for (std::size_t pass = 0; pass < repeat; ++pass) {
            inner.set(kInputPrefix + kIterationInput, constantField(static_cast<float>(pass)), hash64(0x17E2 + pass));
            if (pass > 0) {
                if (chained == d->inputs.size()) throw GraphError("Compound: repeat needs a Strands input to feed back");
                inner.set(kInputPrefix + d->inputs[chained].name, result, identityHash(result, pass));
            }
            result = graph.evaluate(d->doc->output, child);
        }
        // The inputs the inner cache's keys name, held until the next evaluation's are.
        d->held = inputs;
        return result;
    }

    // The inner graph's cached results and held inputs of the same compound in another copy of
    // the graph (Graph::clone, Graph::adoptCache): what the copy's first evaluation reuses.
    void adoptKeptWork(const Operator& previous) override {
        const auto* other = dynamic_cast<const CompoundOp*>(&previous);
        if (!other) return;
        const auto mine = definition();
        const auto theirs = other->definition();
        if (!mine || !theirs || mine == theirs) return;
        std::lock_guard<std::mutex> lockTheirs(theirs->mutex);
        std::lock_guard<std::mutex> lockMine(mine->mutex);
        mine->doc->graph.adoptCache(theirs->doc->graph);
        mine->held = theirs->held;
    }

    bool forEachActiveInner(const std::string& prefix, const ActiveNodeVisitor& visit) const override {
        const auto d = definition();
        if (!d) return true;
        std::lock_guard<std::mutex> lock(d->mutex);
        applyExposed(*d);  // the inner params as they evaluate
        return d->doc->graph.forEachActiveUpstream(d->doc->output, visit, prefix);
    }

private:
    struct Definition {
        std::unique_ptr<GraphDocument> doc;
        std::vector<PortSpec> inputs;
        PortSpec output;
        std::map<std::string, std::pair<std::string, std::string>> expose;  // name -> (node, param)
        std::vector<std::string> externals;
        std::vector<Value> held;  // the last evaluation's inputs (identityHash)
        std::mutex mutex;
    };

    // The identity of a value as an external's hash: its object's address (held alive in
    // Definition::held while the inner cache names it), and the pass of `repeat` it feeds.
    static std::uint64_t identityHash(const Value& value, std::size_t pass) {
        const std::uint64_t address = std::visit(
            [](const auto& v) -> std::uint64_t {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, std::monostate>) {
                    return 0;
                } else {
                    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(v.get()));
                }
            },
            value);
        return hashCombine(hash64(address), 0x1D + pass);
    }

    // The exposed params' values set on the inner nodes (under d.mutex).
    void applyExposed(Definition& d) const {
        for (const auto& [exposed, target] : d.expose) {
            if (params().has(exposed)) d.doc->graph.node(d.doc->node(target.first)).params().set(target.second, params().get(exposed));
        }
    }

    // The inner nodes' paths under this node's own ("compound/inner"; nested compounds pass
    // theirs on), so a Cards node inside names its cards by where it sits (under d.mutex).
    void applyPaths(Definition& d) const {
        Graph& graph = d.doc->graph;
        for (const NodeId id : graph.nodeIds()) {
            const std::string& inner = graph.label(id);
            graph.node(id).setNodePath(nodePath().empty() ? inner : nodePath() + "/" + inner);
        }
    }

    std::shared_ptr<Definition> definition() const {
        const std::string text = params().getString("definition");
        std::lock_guard<std::mutex> lock(mutex_);
        if (text != parsedText_) {
            parsedText_ = text;
            parsed_.reset();
            error_.clear();
            try {
                parsed_ = parse(text);
            } catch (const std::exception& e) {
                error_ = e.what();
            }
        }
        return parsed_;
    }

    std::string definitionError() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_.empty() ? "no definition" : error_;
    }

    static std::shared_ptr<Definition> parse(const std::string& text) {
        const std::string graph = compoundGraphJson(text);
        auto d = std::make_shared<Definition>();
        d->doc = std::make_unique<GraphDocument>(loadGraphJson(graph));
        d->inputs = compoundInputs(text);
        d->output = d->doc->graph.node(d->doc->output).output();
        for (const auto& [name, target] : compoundExposed(text)) {
            const std::size_t dot = target.find('.');
            if (dot == std::string::npos) throw GraphError("exposed param '" + name + "' must name node.param");
            const std::string node = target.substr(0, dot);
            d->doc->node(node);  // throws for an unknown node
            d->expose[name] = {node, target.substr(dot + 1)};
        }
        std::set<std::string> externals;
        bool maps = false;
        for (const NodeId id : d->doc->graph.nodeIds()) {
            const Operator& op = d->doc->graph.node(id);
            for (const std::string& e : op.externalInputs()) {
                if (e.rfind(kInputPrefix, 0) != 0) externals.insert(e);
            }
            for (const auto& [name, value] : op.params().values()) maps = maps || std::holds_alternative<MapRef>(value);
        }
        for (const std::string& e : d->doc->graph.controlInputs()) externals.insert(e);  // keyed inner params
        if (maps) externals.insert("scalp");  // maps are sampled on the scalp
        d->externals.assign(externals.begin(), externals.end());
        return d;
    }

    mutable std::mutex mutex_;
    mutable std::string parsedText_;
    mutable std::shared_ptr<Definition> parsed_;
    mutable std::string error_;
};

}  // namespace

void registerCompoundOperators(OperatorRegistry& registry) {
    registry.registerType(op::kInput, [] { return std::make_unique<InputOp>(); });
    registry.registerType(op::kCompound, [] { return std::make_unique<CompoundOp>(); });
}

}  // namespace groomlab
