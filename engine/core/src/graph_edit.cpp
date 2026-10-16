// SPDX-License-Identifier: Apache-2.0
#include "groomlab/graph_edit.h"

#include <algorithm>
#include <cctype>
#include <mutex>
#include <set>

#include <nlohmann/json.hpp>

#include "groomlab/attributes.h"
#include "groomlab/graph_io.h"
#include "groomlab/operators.h"
#include "groomlab/presets.h"

namespace groomlab {

namespace {

using json = nlohmann::json;

constexpr double kLayoutColumn = 240.0;  // a new node's column right of the graph (the Graph Editor's LAYOUT_DX)

std::string quoted(const std::string& s) { return "'" + s + "'"; }

// Every registered operator's spec (operatorSpecsJson), by type: read once (again when an operator
// was registered since: a test's).
const std::map<std::string, json>& catalog() {
    static std::map<std::string, json> specs;
    static std::size_t registered = 0;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    const std::size_t now = OperatorRegistry::instance().typeNames().size();
    if (specs.empty() || now != registered) {
        std::map<std::string, json> out;
        for (json& spec : json::parse(operatorSpecsJson())) {
            const std::string type = spec.at("type").get<std::string>();
            out[type] = std::move(spec);
        }
        specs = std::move(out);
        registered = now;
    }
    return specs;
}

const json& operatorSpec(const std::string& type) {
    const auto it = catalog().find(type);
    if (it == catalog().end()) throw GraphError("unknown operator type " + quoted(type));
    return it->second;
}

const json* paramSpecOf(const json& spec, const std::string& param) {
    const auto params = spec.find("params");
    if (params == spec.end() || !params->is_array()) return nullptr;
    for (const json& p : *params) {
        if (p.is_object() && p.value("name", std::string()) == param) return &p;
    }
    return nullptr;
}

std::string kindLabel(const std::string& type) {
    if (type == "Strands") return "strands";
    if (type == "Guides") return "guides";
    if (type == "Curves") return "strands or guides";
    return type;
}

bool typesMatch(const std::string& out, const std::string& in) {
    if (out == in) return true;
    const auto curve = [](const std::string& t) { return t == "Strands" || t == "Guides" || t == "Curves"; };
    return (in == "Curves" && curve(out)) || (out == "Curves" && curve(in));
}

json definitionOf(const json& node) {
    const json params = node.value("params", json::object());
    json d = params.value("definition", json::object());
    if (d.is_string()) d = json::parse(d.get<std::string>(), nullptr, false);
    return d.is_object() ? d : json::object();
}

// The type a compound's definition gives: its inner output node's.
std::string definitionOutputType(const json& definition, int depth = 0) {
    const json inner = definition.value("graph", json::object());
    const std::string outputName = inner.value("output", std::string());
    for (const json& n : inner.value("nodes", json::array())) {
        if (n.value("name", std::string()) != outputName) continue;
        const std::string type = n.value("type", std::string());
        if (type == "Input") return n.value("params", json::object()).value("type", std::string("Strands"));
        if (type == "Compound" && depth < 16) return definitionOutputType(definitionOf(n), depth + 1);
        const json& spec = operatorSpec(type);
        const json outputs = spec.value("outputs", json::array());
        return outputs.empty() ? spec.at("output").value("type", std::string("Strands")) : outputs[0].value("type", std::string("Strands"));
    }
    return "Strands";
}

// The behaviour a node's JSON was saved with ("version"; none: its operator's newest).
int storedVersion(const json& node, const json& spec) {
    const int newest = spec.value("version", 1);
    const auto it = node.find("version");
    return it != node.end() && it->is_number_integer() ? it->get<int>() : newest;
}

// Fills in every param the node's JSON leaves out with its spec default, and its version when it
// has none (the newest), inside compounds' definitions too: what a saved graph holds.
void completeNode(json& node, int depth = 0) {
    if (!node.is_object()) return;
    const std::string type = node.value("type", std::string());
    const auto it = catalog().find(type);
    if (it == catalog().end()) return;  // unknown: loading reports it
    const json& spec = it->second;
    if (!node.contains("version")) node["version"] = spec.value("version", 1);
    if (!node.contains("params") || !node["params"].is_object()) node["params"] = json::object();
    json& params = node["params"];
    for (const json& p : spec.value("params", json::array())) {
        const std::string name = p.value("name", std::string());
        if (!name.empty() && !params.contains(name)) params[name] = p.value("default", json());
    }
    if (type == "Compound" && depth < 16) {
        json& definition = params["definition"];
        const bool text = definition.is_string();
        json d = text ? json::parse(definition.get<std::string>(), nullptr, false) : definition;
        if (d.is_object() && d.contains("graph") && d["graph"].is_object()) {
            json& inner = d["graph"];
            if (inner.contains("nodes") && inner["nodes"].is_array()) {
                for (json& n : inner["nodes"]) completeNode(n, depth + 1);
            }
            definition = text ? json(d.dump()) : d;
        }
    }
}

json specOfNode(const json& node) {
    const std::string type = node.value("type", std::string());
    json spec = operatorSpec(type);
    // An Upgrade is available when the node keeps an older behaviour than its operator's newest.
    const int stored = storedVersion(node, spec);
    spec["nodeVersion"] = stored;
    spec["outdated"] = stored < spec.value("version", 1);
    if (!spec.contains("outputs")) spec["outputs"] = json::array({spec["output"]});
    const json params = node.value("params", json::object());
    if (type == "Input") {
        spec["output"]["type"] = params.value("type", std::string("Strands"));
        spec["outputs"] = json::array({spec["output"]});
    } else if (type == "Compound") {
        const json definition = definitionOf(node);
        json inputs = json::array();
        for (const json& port : definition.value("inputs", json::array())) {
            inputs.push_back({{"name", port.value("name", std::string())},
                              {"type", port.value("type", std::string("Strands"))},
                              {"optional", port.value("optional", false)}});
        }
        spec["inputs"] = inputs;
        spec["output"]["type"] = definitionOutputType(definition);
        spec["outputs"] = json::array({spec["output"]});  // a compound gives one output
        // The exposed params, each with its inner param's spec (range, unit, group, advanced...),
        // its default the inner node's value. In the look's order: the inner nodes as the graph
        // lists them, each one's params as its operator does (two names for one param: by name).
        const json inner = definition.value("graph", json::object());
        const json expose = definition.value("expose", json::object());
        std::map<std::string, std::vector<std::string>> exposedAs;  // "node.param" -> the names showing it
        if (expose.is_object()) {
            for (const auto& [exposed, target] : expose.items()) {
                if (target.is_string()) exposedAs[target.get<std::string>()].push_back(exposed);
            }
        }
        for (const json& n : inner.value("nodes", json::array())) {
            if (exposedAs.empty() || !n.is_object()) break;
            const std::string nodeName = n.value("name", std::string());
            const auto it = catalog().find(n.value("type", std::string()));
            if (it == catalog().end()) continue;
            const json innerParams = n.value("params", json::object());
            for (const json& innerSpec : it->second.value("params", json::array())) {
                const std::string param = innerSpec.value("name", std::string());
                const auto names = exposedAs.find(nodeName + "." + param);
                if (names == exposedAs.end()) continue;
                for (const std::string& exposed : names->second) {
                    json exposedSpec = innerSpec;
                    exposedSpec["name"] = exposed;
                    exposedSpec["description"] = innerSpec.value("description", std::string()) + " (exposed " + names->first + ")";
                    if (innerParams.is_object() && innerParams.contains(param)) exposedSpec["default"] = innerParams[param];
                    // When it shows: by the compound's param for the inner param it names when that
                    // is exposed too; else always (the compound cannot change what it names).
                    if (exposedSpec.contains("showWhen")) {
                        json& when = exposedSpec["showWhen"];
                        const auto on = exposedAs.find(nodeName + "." + when.value("param", std::string()));
                        if (on != exposedAs.end()) when["param"] = on->second.front();
                        else exposedSpec.erase("showWhen");
                    }
                    spec["params"].push_back(exposedSpec);
                }
            }
        }
    }
    const json exposedInputs = node.value("paramInputs", json::array());
    if (exposedInputs.is_array() && !exposedInputs.empty()) {
        std::set<std::string> ramps;
        for (const json& p : spec.value("params", json::array())) {
            if (p.value("kind", std::string()) == "ramp") ramps.insert(p.value("name", std::string()));
        }
        for (const json& param : exposedInputs) {
            if (!param.is_string()) continue;
            const std::string name = param.get<std::string>();
            spec["inputs"].push_back({{"name", name}, {"type", ramps.count(name) ? "Curve" : "Field"}, {"optional", true}, {"param", true}});
        }
    }
    return spec;
}

}  // namespace

struct GraphEdit::Impl {
    json graph;

    json& nodes() {
        if (!graph.contains("nodes") || !graph["nodes"].is_array()) graph["nodes"] = json::array();
        return graph["nodes"];
    }
    const json& nodesView() const {
        static const json none = json::array();
        const auto it = graph.find("nodes");
        return it != graph.end() && it->is_array() ? *it : none;
    }
    json& connections() {
        if (!graph.contains("connections") || !graph["connections"].is_array()) graph["connections"] = json::array();
        return graph["connections"];
    }
    const json& connectionsView() const {
        static const json none = json::array();
        const auto it = graph.find("connections");
        return it != graph.end() && it->is_array() ? *it : none;
    }
    const json* find(const std::string& name) const {
        for (const json& n : nodesView()) {
            if (n.is_object() && n.value("name", std::string()) == name) return &n;
        }
        return nullptr;
    }
    const json& node(const std::string& name) const {
        const json* n = find(name);
        if (!n) throw GraphError("no node named " + quoted(name));
        return *n;
    }

    json spec(const std::string& name) const { return specOfNode(node(name)); }

    json inputSpec(const std::string& name, const std::string& input) const {
        const json s = spec(name);
        for (const json& port : s["inputs"]) {
            if (port.value("name", std::string()) == input) return port;
        }
        throw GraphError(name + " has no input " + quoted(input));
    }
    std::vector<std::string> outputNames(const std::string& name) const {
        std::vector<std::string> out;
        const json s = spec(name);
        for (const json& port : s["outputs"]) out.push_back(port.value("name", std::string()));
        return out;
    }
    json outputSpec(const std::string& name, const std::string& output) const {
        const json outputs = spec(name)["outputs"];
        if (output.empty()) return outputs.at(0);
        std::string listed;
        for (const json& port : outputs) {
            if (port.value("name", std::string()) == output) return port;
            listed += (listed.empty() ? "" : ", ") + port.value("name", std::string());
        }
        throw GraphError(name + " has no output " + quoted(output) + " (" + listed + ")");
    }
    std::string sourceOf(const std::string& dst, const std::string& input, std::string* output = nullptr) const {
        for (const json& c : connectionsView()) {
            if (c.value("to", std::string()) == dst && c.value("input", std::string()) == input) {
                if (output) *output = c.value("output", std::string());
                return c.value("from", std::string());
            }
        }
        return {};
    }
    std::string outputType(const std::string& name, const std::string& output, std::set<std::string>& seen) const {
        const std::string out = outputSpec(name, output).value("type", std::string());
        if (out != "Curves" || !seen.insert(name).second) return out;
        const json s = spec(name);
        for (const json& port : s["inputs"]) {
            if (port.value("type", std::string()) != "Curves") continue;
            std::string sourceOutput;
            const std::string source = sourceOf(name, port.value("name", std::string()), &sourceOutput);
            return source.empty() ? out : outputType(source, sourceOutput, seen);
        }
        return out;
    }
    std::set<std::string> upstream(const std::string& name) const {
        std::set<std::string> seen;
        std::vector<std::string> stack{name};
        while (!stack.empty()) {
            const std::string n = stack.back();
            stack.pop_back();
            for (const json& c : connectionsView()) {
                if (c.value("to", std::string()) != n) continue;
                const std::string from = c.value("from", std::string());
                if (seen.insert(from).second) stack.push_back(from);
            }
        }
        return seen;
    }
    // {node: seed} of the nodes of `type`: their own seed, else the default (integers only).
    std::vector<std::pair<std::string, std::int64_t>> seedsOf(const std::string& type) const {
        std::vector<std::pair<std::string, std::int64_t>> out;
        const json* spec = paramSpecOf(operatorSpec(type), "seed");
        for (const json& n : nodesView()) {
            if (!n.is_object() || n.value("type", std::string()) != type) continue;
            const json params = n.value("params", json::object());
            const json value = params.contains("seed") ? params["seed"] : spec ? spec->value("default", json()) : json();
            if (value.is_number_integer()) out.emplace_back(n.value("name", std::string()), value.get<std::int64_t>());
        }
        return out;
    }
    void disconnect(const std::string& dst, const std::string& input, bool* found = nullptr) {
        json kept = json::array();
        bool any = false;
        for (const json& c : connectionsView()) {
            if (c.value("to", std::string()) == dst && c.value("input", std::string()) == input) {
                any = true;
            } else {
                kept.push_back(c);
            }
        }
        if (any) connections() = std::move(kept);
        if (found) *found = any;
    }
    void connect(const std::string& src, const std::string& dst, const std::string& input, const std::string& output) {
        const json port = inputSpec(dst, input);
        const json out = outputSpec(src, output);
        std::set<std::string> seen;
        const std::string outType = outputType(src, output, seen);
        const std::string inType = port.value("type", std::string());
        if (!typesMatch(outType, inType)) {
            const std::string named = !output.empty() && outputNames(src).size() > 1 ? src + "." + output : src;
            throw GraphError("cannot connect " + named + " (it gives " + kindLabel(outType) + ") to " + dst + "." + input +
                             " (it takes " + kindLabel(inType) + ")");
        }
        if (src == dst || upstream(src).count(dst)) throw GraphError("connecting " + src + " to " + dst + " would create a cycle");
        disconnect(dst, input);
        json link = {{"from", src}, {"to", dst}, {"input", input}};
        if (out.value("name", std::string()) != outputNames(src).at(0)) link["output"] = out.value("name", std::string());
        connections().push_back(std::move(link));
    }
};

GraphEdit::GraphEdit(const std::string& graphJson) : impl_(std::make_unique<Impl>()) {
    const bool blank = graphJson.find_first_not_of(" \t\r\n") == std::string::npos;
    impl_->graph = json::parse(blank ? std::string("{}") : graphJson, nullptr, false);
    if (impl_->graph.is_discarded() || !impl_->graph.is_object()) throw GraphError("the graph is not a JSON object");
}

GraphEdit::~GraphEdit() = default;
GraphEdit::GraphEdit(const GraphEdit& other) : impl_(std::make_unique<Impl>(*other.impl_)) {}
GraphEdit& GraphEdit::operator=(const GraphEdit& other) {
    *impl_ = *other.impl_;
    return *this;
}

std::string GraphEdit::toJson() const { return impl_->graph.dump(); }

bool GraphEdit::hasNode(const std::string& name) const { return impl_->find(name) != nullptr; }

std::string GraphEdit::nodeType(const std::string& name) const { return impl_->node(name).value("type", std::string()); }

std::vector<std::string> GraphEdit::nodeNames() const {
    std::vector<std::string> out;
    for (const nlohmann::json& n : impl_->nodesView()) {
        if (n.is_object()) out.push_back(n.value("name", std::string()));
    }
    return out;
}

std::string GraphEdit::uniqueName(const std::string& base) const {
    std::string b = base;
    if (!b.empty()) b[0] = static_cast<char>(std::tolower(static_cast<unsigned char>(b[0])));
    if (!hasNode(b)) return b;
    int i = 1;
    while (hasNode(b + std::to_string(i))) ++i;
    return b + std::to_string(i);
}

std::string GraphEdit::addNode(const std::string& type, const std::string& name, const std::string& paramsJson,
                               std::optional<double> x, std::optional<double> y, std::optional<double> groomSize) {
    const nlohmann::json& spec = operatorSpec(type);
    const std::string chosen = name.empty() ? uniqueName(type) : name;
    if (hasNode(chosen)) throw GraphError("a node named " + quoted(chosen) + " exists");
    nlohmann::json params = paramsJson.empty() ? nlohmann::json::object() : nlohmann::json::parse(paramsJson, nullptr, false);
    if (params.is_discarded() || !params.is_object()) throw GraphError("a node's params are a JSON object");
    if (groomSize) params = nlohmann::json::parse(fitNewNodeParamsJson(type, params.dump(), groomSize));
    // A seed no other node of its type has: two Scatters with one seed would make the same
    // strands, two Flyaways pick the same strays.
    if (const nlohmann::json* seed = paramSpecOf(spec, "seed"); seed && !params.contains("seed")) {
        const auto used = impl_->seedsOf(type);
        const nlohmann::json fallback = seed->value("default", nlohmann::json());
        const bool taken = fallback.is_number_integer() &&
                           std::any_of(used.begin(), used.end(), [&](const auto& s) { return s.second == fallback.get<std::int64_t>(); });
        if (taken) {
            std::int64_t most = used.front().second;
            for (const auto& s : used) most = std::max(most, s.second);
            params["seed"] = most + 1;
        }
    }
    if (!x) {  // a free column right of the graph
        double right = -kLayoutColumn;
        bool any = false;
        for (const nlohmann::json& n : impl_->nodesView()) {
            const nlohmann::json ui = n.value("ui", nlohmann::json::object());
            const double nx = ui.is_object() ? ui.value("x", 0.0) : 0.0;
            right = any ? std::max(right, nx) : nx;
            any = true;
        }
        x = right + kLayoutColumn;
        if (!y) y = 0.0;
    }
    nlohmann::json node = {{"name", chosen}, {"type", type}, {"version", spec.value("version", 1)}, {"params", params},
                           {"ui", {{"x", *x}, {"y", y.value_or(0.0)}}}};
    completeNode(node);  // every param written: a later default change never alters it
    impl_->nodes().push_back(std::move(node));
    return chosen;
}

void GraphEdit::completeParams() {
    for (nlohmann::json& n : impl_->nodes()) completeNode(n);
}

int GraphEdit::nodeVersion(const std::string& name) const {
    const nlohmann::json& node = impl_->node(name);
    return storedVersion(node, operatorSpec(node.value("type", std::string())));
}

bool GraphEdit::upgradeNode(const std::string& name) {
    impl_->node(name);
    for (nlohmann::json& n : impl_->nodes()) {
        if (!n.is_object() || n.value("name", std::string()) != name) continue;
        const nlohmann::json& spec = operatorSpec(n.value("type", std::string()));
        const int newest = spec.value("version", 1);
        if (storedVersion(n, spec) >= newest) return false;
        n["version"] = newest;
        completeNode(n);  // params the newer version added
        return true;
    }
    return false;
}

void GraphEdit::removeNode(const std::string& name) {
    impl_->node(name);
    nlohmann::json kept = nlohmann::json::array();
    for (const nlohmann::json& n : impl_->nodesView()) {
        if (!(n.is_object() && n.value("name", std::string()) == name)) kept.push_back(n);
    }
    impl_->nodes() = std::move(kept);
    nlohmann::json links = nlohmann::json::array();
    for (const nlohmann::json& c : impl_->connectionsView()) {
        if (c.value("from", std::string()) != name && c.value("to", std::string()) != name) links.push_back(c);
    }
    impl_->connections() = std::move(links);
    if (impl_->graph.value("output", std::string()) == name) impl_->graph.erase("output");
}

void GraphEdit::renameNode(const std::string& name, const std::string& newName) {
    impl_->node(name);
    if (newName.empty()) throw GraphError("a node needs a name");
    if (newName == name) return;
    if (hasNode(newName)) throw GraphError("a node named '" + newName + "' exists");
    for (nlohmann::json& n : impl_->nodes()) {
        if (n.is_object() && n.value("name", std::string()) == name) n["name"] = newName;
    }
    for (nlohmann::json& c : impl_->connections()) {
        if (c.value("from", std::string()) == name) c["from"] = newName;
        if (c.value("to", std::string()) == name) c["to"] = newName;
    }
    if (impl_->graph.value("output", std::string()) == name) impl_->graph["output"] = newName;
}

std::string GraphEdit::outputNode() const { return impl_->graph.value("output", std::string()); }

void GraphEdit::setOutputNode(const std::string& name) {
    impl_->node(name);
    impl_->graph["output"] = name;
}

void GraphEdit::clearPositions() {
    for (nlohmann::json& n : impl_->nodes()) {
        if (n.is_object()) n.erase("ui");
    }
}

void GraphEdit::connect(const std::string& src, const std::string& dst, const std::string& input, const std::string& output) {
    impl_->connect(src, dst, input, output);
}

bool GraphEdit::connectByHand(const std::string& src, const std::string& dst, const std::string& input,
                              const std::string& output) {
    impl_->connect(src, dst, input, output);
    // A Clump picking its own centers takes the centers linked to it (its centersFrom: inputPoints).
    if (input != "centers") return false;
    for (json& n : impl_->nodes()) {
        if (!n.is_object() || n.value("name", std::string()) != dst) continue;
        if (n.value("type", std::string()) != op::kClump) return false;
        json& params = n["params"];
        if (!params.is_object()) params = json::object();
        const auto it = params.find("centersFrom");
        if (it != params.end() && !(it->is_string() && it->get<std::string>() == "pick")) return false;
        params["centersFrom"] = "inputPoints";
        return true;
    }
    return false;
}

bool GraphEdit::disconnect(const std::string& dst, const std::string& input) {
    bool found = false;
    impl_->disconnect(dst, input, &found);
    return found;
}

void GraphEdit::checkConnections() {
    const nlohmann::json links = impl_->connectionsView();
    impl_->connections() = nlohmann::json::array();
    for (const nlohmann::json& c : links) {
        impl_->connect(c.value("from", std::string()), c.value("to", std::string()), c.value("input", std::string()),
                       c.value("output", std::string()));
    }
}

std::string GraphEdit::nodeSpecJson(const std::string& name) const { return impl_->spec(name).dump(); }

std::string GraphEdit::outputType(const std::string& name, const std::string& output) const {
    std::set<std::string> seen;
    return impl_->outputType(name, output, seen);
}

std::vector<std::string> GraphEdit::upstream(const std::string& name) const {
    impl_->node(name);
    const std::set<std::string> found = impl_->upstream(name);
    return {found.begin(), found.end()};
}

std::string GraphEdit::channelsJson(const std::vector<std::string>& guideMaps) const {
    return graphChannelsJson(impl_->graph.dump(), guideMaps);
}

std::vector<std::string> GraphEdit::seedWarnings() const {
    // Operators whose nodes of one type should not share a seed, and why (sorted by type).
    static const std::map<std::string, std::string> sharing = {
        {"Flyaway", "they pick the same strays"},
        {"Instance", "they pick the same strands"},
        {"Scales", "their scales vary alike"},
        {"Scatter", "their strands coincide (same roots and IDs)"},
    };
    std::vector<std::string> found;
    for (const auto& [type, why] : sharing) {
        std::map<std::int64_t, std::vector<std::string>> bySeed;
        for (const auto& [name, seed] : impl_->seedsOf(type)) bySeed[seed].push_back(name);
        for (const auto& [seed, names] : bySeed) {
            if (names.size() < 2) continue;
            std::string joined;
            for (const std::string& n : names) joined += (joined.empty() ? "" : " and ") + n;
            found.push_back(joined + " share seed " + std::to_string(seed) + ": " + why + "; give each its own seed");
        }
    }
    return found;
}

std::string completeGraphJson(const std::string& graphJson) {
    GraphEdit edit(graphJson);
    edit.completeParams();
    return edit.toJson();
}

std::string nodeSpecJson(const std::string& nodeJson) {
    const nlohmann::json node = nlohmann::json::parse(nodeJson, nullptr, false);
    if (node.is_discarded() || !node.is_object()) throw GraphError("a node is a JSON object");
    return specOfNode(node).dump();
}

}  // namespace groomlab
