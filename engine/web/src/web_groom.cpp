// SPDX-License-Identifier: Apache-2.0
#include "web_groom.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <set>
#include <sstream>

#include <nlohmann/json.hpp>

#include "groomlab/display.h"
#include "groomlab/graph.h"
#include "groomlab/graph_edit.h"
#include "groomlab/groom_file.h"
#include "groomlab/groom_session.h"
#include "groomlab/guide_hierarchy.h"
#include "groomlab/guide_set.h"
#include "groomlab/hair_color.h"
#include "groomlab/node_catalog.h"
#include "groomlab/presets.h"
#include "groomlab/scalp_mesh.h"
#include "groomlab/strand_set.h"

namespace groomlab::web {

namespace {

using json = nlohmann::json;

// An OBJ's `v` and `f` lines (and `vt` when every corner has one), polygons as fans.
std::shared_ptr<const ScalpMesh> parseObj(const std::string& text) {
    std::vector<Vec3f> points;
    std::vector<ScalpMesh::UV> uvs, cornerUVs;
    std::vector<ScalpMesh::Triangle> triangles;
    std::vector<std::int32_t> polygons;
    bool allUVs = true;
    std::istringstream in(text);
    std::string line;
    auto index = [](long i, std::size_t count) -> long { return i < 0 ? static_cast<long>(count) + i : i - 1; };
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string key;
        if (!(fields >> key) || key[0] == '#') continue;
        if (key == "v") {
            Vec3f p;
            if (!(fields >> p.x >> p.y >> p.z)) throw std::runtime_error("OBJ: a bad vertex line: " + line);
            points.push_back(p);
        } else if (key == "vt") {
            ScalpMesh::UV uv{0.0f, 0.0f};
            fields >> uv[0] >> uv[1];
            uvs.push_back(uv);
        } else if (key == "f") {
            std::vector<std::int32_t> corners;
            std::vector<ScalpMesh::UV> faceUVs;
            std::string token;
            while (fields >> token) {
                char* end = nullptr;
                const long v = index(std::strtol(token.c_str(), &end, 10), points.size());
                if (v < 0 || static_cast<std::size_t>(v) >= points.size()) throw std::runtime_error("OBJ: a bad face: " + line);
                corners.push_back(static_cast<std::int32_t>(v));
                if (*end == '/' && end[1] != '/' && end[1] != '\0') {
                    const long t = index(std::strtol(end + 1, nullptr, 10), uvs.size());
                    if (t >= 0 && static_cast<std::size_t>(t) < uvs.size()) {
                        faceUVs.push_back(uvs[static_cast<std::size_t>(t)]);
                        continue;
                    }
                }
                allUVs = false;
            }
            if (corners.size() < 3) throw std::runtime_error("OBJ: a face needs 3 corners: " + line);
            polygons.push_back(static_cast<std::int32_t>(corners.size() - 2));
            for (std::size_t k = 1; k + 1 < corners.size(); ++k) {
                triangles.push_back({corners[0], corners[k], corners[k + 1]});
                if (allUVs) cornerUVs.insert(cornerUVs.end(), {faceUVs[0], faceUVs[k], faceUVs[k + 1]});
            }
        }
    }
    if (triangles.empty()) throw std::runtime_error("OBJ: the mesh has no faces");
    auto mesh = std::make_shared<ScalpMesh>(std::move(points), std::move(triangles));
    if (allUVs && !cornerUVs.empty()) mesh->setCornerUVs(std::move(cornerUVs));
    mesh->setPolygons(std::move(polygons));
    return mesh;
}

float toSrgb(float c) {
    c = std::clamp(c, 0.0f, 1.0f);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

// "clumpSize" -> "Clump size".
std::string humanize(const std::string& name) {
    std::string out;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const char c = name[i];
        if (c == '_') {
            out += ' ';
        } else if (i > 0 && std::isupper(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(name[i - 1]))) {
            out += ' ';
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else {
            out += c;
        }
    }
    if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

double numberOr(const json& v, double fallback) {
    if (v.is_number()) return v.get<double>();
    if (v.is_boolean()) return v.get<bool>() ? 1.0 : 0.0;
    return fallback;
}

// A member that is an object (else an empty one) and one that is an array (else an empty one):
// stored graphs may hold null where a key is unset.
json objectAt(const json& j, const char* key) {
    if (!j.is_object()) return json::object();
    const auto it = j.find(key);
    return it != j.end() && it->is_object() ? *it : json::object();
}
json arrayAt(const json& j, const char* key) {
    if (!j.is_object()) return json::array();
    const auto it = j.find(key);
    return it != j.end() && it->is_array() ? *it : json::array();
}
std::string stringAt(const json& j, const char* key, const std::string& fallback = {}) {
    if (!j.is_object()) return fallback;
    const auto it = j.find(key);
    return it != j.end() && it->is_string() ? it->get<std::string>() : fallback;
}
json valueAt(const json& j, const std::string& key) {
    if (!j.is_object()) return json();
    const auto it = j.find(key);
    return it != j.end() ? *it : json();
}

json* findNode(json& graph, const std::string& name) {
    if (!graph.contains("nodes") || !graph["nodes"].is_array()) return nullptr;
    for (json& n : graph["nodes"]) {
        if (n.is_object() && n.value("name", std::string()) == name) return &n;
    }
    return nullptr;
}

// A param's spec (operatorSpecsJson's form) from a node's spec, null when it has none.
const json* paramIn(const json& nodeSpec, const std::string& param) {
    const auto params = nodeSpec.find("params");
    if (params == nodeSpec.end() || !params->is_array()) return nullptr;
    for (const json& p : *params) {
        if (stringAt(p, "name") == param) return &p;
    }
    return nullptr;
}

// A slider for a number param: its spec's slider range (the limits when it gives none, else a
// range around the default), a step from its decimals.
json paramSlider(const std::string& node, const std::string& nodeType, const std::string& param, const std::string& label,
                 const json& spec, const json& value) {
    const std::string kind = spec.value("kind", std::string());
    const double def = numberOr(spec.value("default", json()), 0.0);
    double lo = spec.contains("softMin") ? spec["softMin"].get<double>() : 0.0;
    double hi = spec.contains("softMax") ? spec["softMax"].get<double>() : (def > 0.0 ? def * 2.0 : 1.0);
    const double now = numberOr(value, def);
    lo = std::min(lo, now);
    hi = std::max(hi, now);
    double step = kind == "int" ? 1.0 : std::pow(10.0, -static_cast<double>(spec.value("decimals", 3)));
    return {{"id", "param:" + node + "." + param},
            {"kind", kind == "int" ? "int" : "float"},
            {"label", label},
            {"group", nodeLabel(nodeType) + (node.empty() ? "" : " (" + node + ")")},
            {"node", node},
            {"param", param},
            {"min", lo},
            {"max", hi},
            {"step", step},
            {"default", def},
            {"value", now},
            {"unit", spec.value("unit", std::string())},
            {"description", spec.value("description", std::string())}};
}

bool isPlainNumberParam(const json& spec, const json& node) {
    const std::string kind = spec.value("kind", std::string());
    if ((kind != "float" && kind != "int") || spec.value("advanced", false) || spec.value("group", std::string()) == "Advanced") {
        return false;
    }
    const std::string name = spec.value("name", std::string());
    const json params = objectAt(node, "params");
    if (params.is_object() && params.contains(name) && !params[name].is_number()) return false;  // a map, an expression
    const json inputs = arrayAt(node, "paramInputs");
    if (std::find(inputs.begin(), inputs.end(), json(name)) != inputs.end()) return false;
    const json keyed = objectAt(node, "paramControls");
    return !(keyed.is_object() && keyed.contains(name));
}

// Control nodes anywhere in the graph (compounds' definitions too), in order, copied.
void eachControl(const json& graph, std::vector<json>& out) {
    for (const json& n : arrayAt(graph, "nodes")) {
        if (!n.is_object()) continue;
        const std::string type = stringAt(n, "type");
        if (type == "Control") out.push_back(n);
        if (type == "Compound") {
            const json params = objectAt(n, "params");
            if (params.is_object() && params.contains("definition") && params["definition"].is_object()) {
                eachControl(objectAt(params["definition"], "graph"), out);
            }
        }
    }
}

}  // namespace

WebGroom::WebGroom() = default;
WebGroom::~WebGroom() = default;

bool WebGroom::load(const std::string& groomText, const std::string& objText) {
    error_.clear();
    try {
        const json parsed = json::parse(groomText);
        auto file = std::make_unique<GroomFile>();
        if (parsed.is_object() && parsed.contains("nodes") && !parsed.contains("format")) {
            // A preset: fitted to the mesh given, its controls applied, its "exposed" list kept.
            if (objText.empty()) throw std::runtime_error("a preset needs a mesh (an .obj) to grow on");
            file->data.scalp = parseObj(objText);
            const std::optional<double> size = presetScalpSize(*file->data.scalp);
            json graph = json::parse(applyPresetControlsToGraph(presetGraphJson(groomText, size), presetControlsJson(groomText, size)));
            if (parsed.contains("exposed")) graph["exposed"] = parsed["exposed"];
            file->data.graphJson = graph.dump();
        } else {
            *file = groomFileFromJson(groomText);
            if (!objText.empty()) file->data.scalp = parseObj(objText);
            if (!file->data.scalp) throw std::runtime_error("the groom file has no scalp: drop its mesh (.obj) with it");
        }
        auto session = std::make_unique<GroomSession>(file->data);
        session->setFramesPerSecond(file->framesPerSecond);
        file_ = std::move(file);
        session_ = std::move(session);
        offsets_.clear();
        positions_.clear();
        colors_.clear();
        readScalp();
        return true;
    } catch (const std::exception& e) {
        error_ = e.what();
        return false;
    }
}

void WebGroom::readScalp() {
    scalpPositions_.clear();
    scalpNormals_.clear();
    scalpIndices_.clear();
    const MeshPtr scalp = session_ ? session_->scalp() : nullptr;
    if (!scalp) return;
    for (const Vec3f& p : scalp->points()) scalpPositions_.insert(scalpPositions_.end(), {p.x, p.y, p.z});
    for (const Vec3f& n : scalp->vertexNormals()) scalpNormals_.insert(scalpNormals_.end(), {n.x, n.y, n.z});
    if (scalpNormals_.size() != scalpPositions_.size()) scalpNormals_.clear();
    for (const auto& t : scalp->triangles()) {
        scalpIndices_.insert(scalpIndices_.end(), {static_cast<std::uint32_t>(t[0]), static_cast<std::uint32_t>(t[1]),
                                                   static_cast<std::uint32_t>(t[2])});
    }
}

std::string WebGroom::controlsJson() const {
    error_.clear();
    json out = json::array();
    if (!file_) return out.dump();
    std::string stage = "graph";
    try {
        const json graph = json::parse(file_->data.graphJson.empty() ? "{}" : file_->data.graphJson);
        // 1. The Control nodes: the groom's own sliders.
        std::vector<json> controls;
        eachControl(graph, controls);
        std::set<std::string> seen;
        for (const json& n : controls) {
            const json params = objectAt(n, "params");
            const std::string name = stringAt(params, "name", "control");
            if (!seen.insert(name).second) continue;
            const double def = numberOr(params.value("default", json()), 0.0);
            double lo = numberOr(params.value("min", json()), 0.0), hi = numberOr(params.value("max", json()), 1.0);
            const auto set = file_->data.controls.find(name);
            const double value = set != file_->data.controls.end() ? set->second : def;
            lo = std::min(lo, value);
            hi = std::max(hi, value);
            out.push_back({{"id", "control:" + name},
                           {"kind", "control"},
                           {"label", humanize(name)},
                           {"group", "Controls"},
                           {"node", stringAt(n, "name")},
                           {"param", "value"},
                           {"min", lo},
                           {"max", hi},
                           {"step", hi > lo ? (hi - lo) / 200.0 : 0.01},
                           {"default", def},
                           {"value", value},
                           {"unit", ""},
                           {"description", "A Control node: a slider the groom's author made"}});
        }
        stage = "edit";
        GraphEdit edit(graph.dump());
        // 2. The exposed params: each Compound's (a preset inserted as one node, a library node).
        bool exposed = false;
        for (const json& n : arrayAt(graph, "nodes")) {
            if (!n.is_object() || n.value("type", std::string()) != "Compound") continue;
            const std::string name = n.value("name", std::string());
            const json params = objectAt(n, "params");
            const json expose = params.is_object() && params.contains("definition") && params["definition"].is_object()
                                    ? objectAt(params["definition"], "expose")
                                    : json::object();
            if (!expose.is_object() || expose.empty()) continue;
            const json spec = json::parse(edit.nodeSpecJson(name));
            for (const auto& [param, target] : expose.items()) {
                const json* p = paramIn(spec, param);
                if (!p || !isPlainNumberParam(*p, n)) continue;
                const std::string group = spec.value("label", std::string());
                json slider = paramSlider(name, "Compound", param, humanize(param), *p, valueAt(params, param));
                if (!group.empty()) slider["group"] = group + " (" + name + ")";
                out.push_back(std::move(slider));
                exposed = true;
            }
        }
        // 3. Else the graph's "exposed" list (a preset's key params), else a default pick: one
        // plain number param per look node, Clump, Curl, Noise... first.
        std::vector<std::pair<std::string, std::string>> picks;  // node, param
        std::vector<std::string> labels;
        if (!exposed && graph.contains("exposed") && graph["exposed"].is_array()) {
            for (const json& e : graph["exposed"]) {
                if (!e.is_object()) continue;
                picks.emplace_back(stringAt(e, "node"), stringAt(e, "param"));
                labels.push_back(humanize(stringAt(e, "name", stringAt(e, "param"))));
            }
        }
        if (!exposed && picks.empty()) {
            static const std::vector<std::string> priority = {"Clump", "Wet", "Curl", "Noise", "Braid", "Tangle", "Flyaway",
                                                              "Gravity", "Bend", "Length", "HairColor", "Width"};
            for (const std::string& type : priority) {
                for (const json& n : arrayAt(graph, "nodes")) {
                    if (picks.size() >= 8 || !n.is_object() || n.value("type", std::string()) != type) continue;
                    const std::string name = n.value("name", std::string());
                    const json spec = json::parse(edit.nodeSpecJson(name));
                    for (const json& p : arrayAt(spec, "params")) {
                        if (!isPlainNumberParam(p, n)) continue;
                        picks.emplace_back(name, p.value("name", std::string()));
                        labels.push_back(humanize(p.value("name", std::string())));
                        break;
                    }
                }
            }
        }
        for (std::size_t i = 0; i < picks.size(); ++i) {
            const auto& [node, param] = picks[i];
            if (!edit.hasNode(node)) continue;
            stage = node + "." + param;
            const json spec = json::parse(edit.nodeSpecJson(node));
            const json* p = paramIn(spec, param);
            json copy = graph;
            const json* n = findNode(copy, node);
            if (!p || !n || !isPlainNumberParam(*p, *n)) continue;
            out.push_back(paramSlider(node, edit.nodeType(node), param, labels[i], *p, valueAt(objectAt(*n, "params"), param)));
        }
    } catch (const std::exception& e) {
        error_ = "sliders (" + stage + "): " + e.what();  // a graph that does not read has none
    }
    return out.dump();
}

bool WebGroom::setParam(const std::string& id, double value) {
    error_.clear();
    if (!file_ || !session_ || !std::isfinite(value)) return false;
    try {
        if (id.rfind("control:", 0) == 0) {
            file_->data.controls[id.substr(8)] = static_cast<float>(value);
            session_->setControls(file_->data.controls);
            return true;
        }
        if (id.rfind("param:", 0) != 0) return false;
        const std::string path = id.substr(6);
        const std::size_t dot = path.find('.');
        if (dot == std::string::npos) return false;
        const std::string node = path.substr(0, dot), param = path.substr(dot + 1);
        json graph = json::parse(file_->data.graphJson);
        json* n = findNode(graph, node);
        if (!n) return false;
        const json spec = json::parse(GraphEdit(file_->data.graphJson).nodeSpecJson(node));
        const json* p = paramIn(spec, param);
        if (!p) return false;
        if (!n->contains("params") || !(*n)["params"].is_object()) (*n)["params"] = json::object();
        if (p->value("kind", std::string()) == "int") {
            (*n)["params"][param] = static_cast<long long>(std::llround(value));
        } else {
            (*n)["params"][param] = value;
        }
        return setGraphText(graph.dump());
    } catch (const std::exception& e) {
        error_ = e.what();
        return false;
    }
}

bool WebGroom::setGraphText(const std::string& text) {
    session_->setGraphJson(text);  // throws GraphError, leaving the session as it was
    file_->data.graphJson = text;
    return true;
}

bool WebGroom::setGraphJson(const std::string& graphJson) {
    error_.clear();
    if (!session_) return false;
    try {
        return setGraphText(graphJson);
    } catch (const std::exception& e) {
        error_ = e.what();
        return false;
    }
}

bool WebGroom::evaluate(double density) {
    error_.clear();
    offsets_.clear();
    positions_.clear();
    colors_.clear();
    guideOffsets_.clear();
    guidePositions_.clear();
    errors_.clear();
    if (!session_) {
        error_ = "no groom loaded";
        return false;
    }
    try {
        ViewRequest request;
        request.density = static_cast<float>(std::clamp(density, 0.0, 1.0));
        request.time = 0.0;
        const auto start = std::chrono::steady_clock::now();
        const ViewResult& view = session_->evaluate(request);
        const StrandsPtr strands = view.placed();
        evaluateMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        errors_ = view.errors;
        if (!strands) {
            error_ = errors_.empty() ? "the groom made no strands" : errors_.front();
            return false;
        }
        offsets_.reserve(strands->offsets().size());
        for (const std::size_t o : strands->offsets()) offsets_.push_back(static_cast<std::uint32_t>(o));
        if (offsets_.empty()) offsets_.push_back(0);
        positions_.reserve(strands->pointCount() * 3);
        for (const Vec3f& p : strands->positions()) positions_.insert(positions_.end(), {p.x, p.y, p.z});
        // The strands' own colors as the hair looks, else the look's one color (sRGB for the page).
        colors_.resize(strands->pointCount() * 3);
        if (const auto look = strandLookColors(strands, true)) {
            const std::vector<float> rgba = look->rgba();
            for (std::size_t i = 0; i < strands->pointCount(); ++i) {
                for (int c = 0; c < 3; ++c) colors_[i * 3 + c] = toSrgb(rgba[i * 4 + c]);
            }
        } else {
            StrandChannelMeans means;
            const std::array<float, 3> c = strandsLookColor(strands.get(), view.versions.strands, kDefaultStrandColor, means);
            const std::array<float, 3> shown = hairAppearanceColor(c);
            for (std::size_t i = 0; i < strands->pointCount(); ++i) {
                for (int k = 0; k < 3; ++k) colors_[i * 3 + k] = toSrgb(shown[k]);
            }
        }
        // The guides: the flat ones, else the active hierarchy's.
        GuideSet flattened;
        const GuideSet* guides = file_->data.guides.get();
        if (!guides && view.hierarchy) {
            flattened = view.hierarchy->flatten();
            guides = &flattened;
        }
        if (guides) {
            guideOffsets_.push_back(0);
            for (const Guide& g : guides->guides()) {
                for (const Vec3f& p : g.points) guidePositions_.insert(guidePositions_.end(), {p.x, p.y, p.z});
                guideOffsets_.push_back(static_cast<std::uint32_t>(guidePositions_.size() / 3));
            }
        }
        if (view.scalp && (scalpPositions_.size() != view.scalp->pointCount() * 3)) readScalp();
        return true;
    } catch (const std::exception& e) {
        error_ = e.what();
        return false;
    }
}

std::string WebGroom::statsJson() const {
    json nodes = json::array();
    if (session_) {
        for (const auto& [name, ms] : session_->nodeTimes()) nodes.push_back({{"node", name}, {"ms", ms}});
    }
    return json{{"errors", errors_}, {"nodes", nodes}, {"evaluateMs", evaluateMs_}, {"strands", strandCount()},
                {"points", positions_.size() / 3}}
        .dump();
}

std::string WebGroom::saveGroom() const { return file_ ? groomFileToJson(*file_) : std::string(); }

std::string WebGroom::graphJson() const { return file_ ? file_->data.graphJson : std::string(); }

bool WebGroom::bakeGuides(const std::string& graphJson) {
    error_.clear();
    if (!session_) return false;
    const std::string kept = file_->data.graphJson;
    try {
        session_->setGraphJson(graphJson);
        const StrandsPtr strands = session_->evaluate(1.0f, 0.0);
        session_->setGraphJson(kept);
        if (!strands || strands->empty()) throw std::runtime_error("the guide graph made no strands");
        auto guides = std::make_shared<GuideSet>();
        for (std::size_t s = 0; s < strands->strandCount(); ++s) {
            const Vec3f* p = strands->strandPoints(s);
            guides->add(s + 1, std::vector<Vec3f>(p, p + strands->strandPointCount(s)));
        }
        guides->rootOnMesh(*session_->scalp());
        file_->data.guides = guides;
        session_->setGuides(guides);
        return true;
    } catch (const std::exception& e) {
        try {
            session_->setGraphJson(kept);
        } catch (...) {
        }
        error_ = e.what();
        return false;
    }
}

}  // namespace groomlab::web
