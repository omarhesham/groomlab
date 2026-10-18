// SPDX-License-Identifier: Apache-2.0
// GroomLab's own tools for the host (tools.h, docs/BLENDER.md "Tools"): the core's BrushTool over
// a SessionStrokeHost of the session's groom, driven in the host's region pixels through a view
// the host gives in its own space (turned into the groom's here), as the standalone app's
// BrushSession drives it in its viewport; the selection, hidden groups and the guide hierarchy's
// edits; painted maps; the undo stack (one step per stroke or edit); the display buffers
// DisplayBuilder builds for Maya's viewport, answered in the host's space.

#include "tools.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "groomlab/brush_catalog.h"
#include "groomlab/display.h"
#include "groomlab/flow.h"
#include "groomlab/guide_selection.h"
#include "groomlab/paths.h"
#include "groomlab/random.h"
#include "groomlab/scalp_maps.h"
#include "groomlab/scalp_paint.h"
#include "groomlab/stroke.h"
#include "groomlab/texture.h"
#include "groomlab/view_projection.h"

#include "request.h"
#include "session.h"

namespace groomlab::helper {

namespace fs = std::filesystem;

struct ToolState;

namespace {

void writeTexture(ToolState& t, const std::string& name);

}  // namespace

// The session's groom as the stroke host: inputs and previews through the session, each edit
// applied to it and kept for undo (SessionStrokeHost); texture maps painted in their files (kept
// open between strokes while the file is what they wrote, as every host keeps them).
class HelperStrokeHost : public SessionStrokeHost {
public:
    HelperStrokeHost(Session& s, ToolState& t) : SessionStrokeHost(*s.groom), s_(s), t_(t) {}

    std::shared_ptr<TexturePaint> texturePaint(const std::string& name, int channels, int resolution) override;
    // Kept: a dropped stroke put its paint back as it was (BrushTool::dropMapStroke), and undo
    // and redo write their tiles into it.
    void dropTexturePaint() override {}
    void commit(StrokeEdit edit) override;

    std::map<std::string, KeptTexturePaint> kept;  // by map name

private:
    Session& s_;
    ToolState& t_;
};

struct PaintTarget {
    std::string attribute = "clump";
    float value = 1.0f;
    std::string kind = "guides", texture;
};

struct ToolState {
    explicit ToolState(Session& s)
        : session(s), host(s, *this), overlayOut("s" + std::to_string(s.id) + "overlay"),
          displayOut("s" + std::to_string(s.id) + "display"), colorsOut("s" + std::to_string(s.id) + "colors") {
        // As the app's BrushSession starts the tool.
        tool.settings.brush.radius = 0.25f;
        tool.settings.screenRadius = 60.0f;
        static std::uint64_t made = 0;
        generation = blockName("s" + std::to_string(s.id) + "display", static_cast<unsigned>(++made));
    }

    Session& session;
    HelperStrokeHost host;
    BrushTool tool;
    PaintTarget beforeFreeze;  // what Paint painted before Freeze took the brush
    bool selectTool = false;   // the Select tool (the lasso): no strokes

    // The view the pixels are in (the last one given), groom space.
    bool hasView = false;
    ToolView view;
    std::array<double, 16> rows{};
    double width = 0.0, height = 0.0;
    bool hasTarget = false;
    Vec3f target;

    // The guide selection (ids), its soft radius (groom units) and the hidden groups.
    std::vector<GuideId> selection;
    float softRadius = 0.0f;
    std::set<std::string> hidden;
    std::uint64_t selectionVersion = 1;

    // The stroke in flight.
    float pressX = 0.0f, pressY = 0.0f;

    // What redo would bring back (labels and texture maps), beside the host's stack.
    std::vector<std::string> redoLabels, redoMaps;

    // The brush overlay: the ring, the guides it would touch, a Part or Flow stroke's path.
    DisplayGeometry overlay;
    std::uint64_t overlayVersion = 0;
    bool ringValid = false, ringOnScalp = false;
    Vec3f ringCenter, ringNormal;
    float ringRadius = 0.0f;

    // The display (Maya's buffers) and the strand colors.
    DisplayBuilder builder;
    std::string generation;
    Arena overlayOut, displayOut, colorsOut;
    std::uint64_t colorsKey = 0, colorsVersion = 0;
};

Session::Session(int id)
    : id(id), strands("s" + std::to_string(id) + "strands"), guides("s" + std::to_string(id) + "guides"),
      scalp("s" + std::to_string(id) + "scalp") {}

Session::~Session() = default;

namespace {

ToolState& toolsOf(Session& s) {
    if (!s.tools) s.tools = std::make_unique<ToolState>(s);
    return *s.tools;
}

// ---- Spaces -------------------------------------------------------------------------------------

json hostPoint(const HostSpace& space, const Vec3f& p) {
    float h[3];
    space.toHost(p, h);
    return {h[0], h[1], h[2]};
}

Vec3f hostDirection(const HostSpace& space, const Vec3f& d) {
    return space.zUp() ? Vec3f{d.x, -d.z, d.y} : d;
}

Vec3f groomPointOf(const json& v, const HostSpace& space, const char* what) {
    if (!v.is_array() || v.size() != 3 || !v[0].is_number() || !v[1].is_number() || !v[2].is_number()) {
        fail("bad_request", std::string(what) + " is a point [x, y, z]");
    }
    return space.toGroom(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
}

float toGroomLength(const HostSpace& space, double v) { return static_cast<float>(v * space.scale()); }
double toHostLength(const HostSpace& space, float v) { return static_cast<double>(v) / space.scale(); }

// The host's symmetry axis ("x", "y", "z", "none") and center as the groom's: Z up, the host's y
// is the groom's -z and its z the groom's y.
void symmetryToGroom(const HostSpace& space, const std::string& axis, double center, int& groomAxis, float& groomCenter) {
    const int a = symmetryAxisFromName(axis);  // throws std::invalid_argument
    const float c = toGroomLength(space, center);
    if (a < 0 || !space.zUp()) {
        groomAxis = a;
        groomCenter = c;
    } else if (a == 0) {
        groomAxis = 0;
        groomCenter = c;
    } else if (a == 1) {
        groomAxis = 2;
        groomCenter = -c;
    } else {
        groomAxis = 1;
        groomCenter = c;
    }
}

void symmetryToHost(const HostSpace& space, int groomAxis, float groomCenter, std::string& axis, double& center) {
    if (groomAxis < 0) {
        axis = "none";
        center = toHostLength(space, groomCenter);
        return;
    }
    int a = groomAxis;
    double c = toHostLength(space, groomCenter);
    if (space.zUp()) {
        if (groomAxis == 2) {
            a = 1;
            c = -c;
        } else if (groomAxis == 1) {
            a = 2;
        }
    }
    axis = symmetryAxisName(a);
    center = c;
}

// ---- Views --------------------------------------------------------------------------------------

using Mat4 = std::array<double, 16>;  // rows first: m[r * 4 + c]

Mat4 matrixOf(const json& v, const char* what) {
    std::vector<double> values;
    if (v.is_array()) {
        for (const json& row : v) {
            if (row.is_array()) {
                for (const json& x : row) {
                    if (!x.is_number()) fail("bad_request", std::string(what) + " holds something that is not a number");
                    values.push_back(x.get<double>());
                }
            } else if (row.is_number()) {
                values.push_back(row.get<double>());
            } else {
                fail("bad_request", std::string(what) + " holds something that is not a number");
            }
        }
    }
    if (values.size() != 16) fail("bad_request", std::string(what) + " is a 4x4 matrix (4 rows of 4, or 16 numbers rows first)");
    Mat4 m{};
    for (std::size_t i = 0; i < 16; ++i) {
        if (!std::isfinite(values[i])) fail("bad_request", std::string(what) + " holds a NaN or an infinity");
        m[i] = values[i];
    }
    return m;
}

Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 m{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            double sum = 0.0;
            for (int k = 0; k < 4; ++k) sum += a[static_cast<std::size_t>(r * 4 + k)] * b[static_cast<std::size_t>(k * 4 + c)];
            m[static_cast<std::size_t>(r * 4 + c)] = sum;
        }
    }
    return m;
}

// The groom's point (centimeters, Y up) as the host's (column vectors: host = C * groom).
Mat4 groomToHost(const HostSpace& space) {
    const double k = 1.0 / space.scale();
    Mat4 m{};
    m[15] = 1.0;
    if (space.zUp()) {
        m[0] = k;   // x = gx
        m[6] = -k;  // y = -gz
        m[9] = k;   // z = gy
    } else {
        m[0] = m[5] = m[10] = k;
    }
    return m;
}

// The view a request gives (host space; Blender's matrices as they print: rows first, column
// vectors, OpenGL clip space): its world-to-clip matrix taken to the groom's space in the core's
// convention (row vectors: the transpose), over a port of the region's size.
void setView(ToolState& t, const json& v) {
    const HostSpace& space = t.session.space;
    if (!v.is_object()) fail("bad_request", "view is an object: viewMatrix, projectionMatrix (or perspectiveMatrix), width, height");
    Mat4 clip;
    if (v.contains("perspectiveMatrix") && !v["perspectiveMatrix"].is_null()) {
        clip = matrixOf(v["perspectiveMatrix"], "view.perspectiveMatrix");
    } else {
        clip = multiply(matrixOf(need(v, "projectionMatrix"), "view.projectionMatrix"), matrixOf(need(v, "viewMatrix"), "view.viewMatrix"));
    }
    if (v.contains("objectMatrix") && !v["objectMatrix"].is_null()) clip = multiply(clip, matrixOf(v["objectMatrix"], "view.objectMatrix"));
    const Mat4 full = multiply(clip, groomToHost(space));
    Mat4 rows{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) rows[static_cast<std::size_t>(r * 4 + c)] = full[static_cast<std::size_t>(c * 4 + r)];
    }
    const double width = optNumber(v, "width", 0.0), height = optNumber(v, "height", 0.0);
    if (!(width > 0.0) || !(height > 0.0)) fail("bad_request", "view.width and view.height are the region's size in pixels");
    ToolView view = ToolView::fromMatrix(rows, width, height);
    if (!view.projection.valid() || !view.projection.invertible() || !view.hasCamera) {
        fail("bad_request", "the view's matrix cannot be inverted: no camera to brush from");
    }
    t.view = std::move(view);
    t.rows = rows;
    t.width = width;
    t.height = height;
    t.hasView = true;
    t.hasTarget = v.contains("target") && !v["target"].is_null();
    if (t.hasTarget) {
        // The target is in the host's world; with an object matrix, the add-on gives it in the
        // scalp object's space (as every point it sends).
        t.target = groomPointOf(v["target"], space, "view.target");
    }
}

void viewFromArgs(ToolState& t, const json& args) {
    if (args.contains("view") && !args["view"].is_null()) setView(t, args["view"]);
    if (!t.hasView) fail("state", "no view: give \"view\" (the region's matrices and size)");
}

json viewJson(const ToolState& t) {
    const HostSpace& space = t.session.space;
    json j;
    j["eye"] = hostPoint(space, t.view.eye);
    const Vec3f d = hostDirection(space, t.view.viewDirection);
    j["viewDirection"] = {d.x, d.y, d.z};
    j["orthographic"] = t.view.orthographic;
    j["groomMatrix"] = t.rows;  // the core's world-to-clip in groom space (rows first, row vectors)
    j["width"] = t.width;
    j["height"] = t.height;
    return j;
}

// The depth the ring sits at off the scalp: the view's target, else the scalp's middle.
bool depthPoint(ToolState& t, Vec3f& out) {
    if (t.hasTarget) {
        out = t.target;
        return true;
    }
    const MeshPtr scalp = t.session.groom->rawScalp();
    if (!scalp || scalp->pointCount() == 0) return false;
    Vec3f lo = scalp->points().front(), hi = lo;
    for (const Vec3f& p : scalp->points()) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    out = (lo + hi) * 0.5f;
    return true;
}

bool atTargetDepth(ToolState& t, float x, float y, Vec3f& out) {
    Vec3f origin, direction, eye, along, target;
    bool ortho = false;
    if (!t.view.ray(x, y, origin, direction) || !t.view.camera(eye, along, ortho) || !depthPoint(t, target)) return false;
    const float d = (target - origin).dot(along) / std::max(direction.dot(along), 1e-6f);
    if (d <= 0.0f) return false;
    out = origin + direction * d;
    return true;
}

// ---- Settings -----------------------------------------------------------------------------------

constexpr const char* kFreezeTool = "freeze";
constexpr const char* kSelectTool = "select";

std::string toolIdOf(const ToolState& t) {
    if (t.selectTool) return kSelectTool;
    const BrushToolSettings& s = t.tool.settings;
    if (s.tool == BrushToolSettings::Tool::Brush && s.brush.mode == BrushMode::Attribute && s.brush.attribute == kFreezeTool &&
        s.paintKind == "guides") {
        return kFreezeTool;
    }
    return s.modeName();
}

// The rail's tools by the catalog's ids, as the app's BrushSession::setTool picks them: a brush
// mode, "freeze" (Paint on the guides' freeze mask toward 1; Ctrl: 0), or "select" (the lasso).
void setTool(ToolState& t, const std::string& id) {
    BrushToolSettings& s = t.tool.settings;
    const bool freezing = toolIdOf(t) == kFreezeTool;
    if (id == kSelectTool) {
        t.selectTool = true;
        return;
    }
    if (id == kFreezeTool) {
        t.selectTool = false;
        if (!freezing) t.beforeFreeze = {s.brush.attribute, s.brush.value, s.paintKind, s.paintTexture};
        s.setMode("paint");
        s.brush.freeze = kFreezeTool;
        s.brush.attribute = kFreezeTool;
        s.brush.value = 1.0f;
        s.paintKind = "guides";
        s.paintTexture.clear();
        return;
    }
    if (id == "width" || id == "density") fail("bad_request", std::string(kRemovedParamBrushes) + " (the helper: map_for_param)");
    try {
        BrushToolSettings().setMode(id);  // throws for an unknown id, before anything changes
    } catch (const std::invalid_argument&) {
        fail("bad_request", "no tool '" + id + "': the catalog's ids (tool_catalog), select, freeze or clump");
    }
    t.selectTool = false;
    if (freezing) {
        s.brush.attribute = t.beforeFreeze.attribute;
        s.brush.value = t.beforeFreeze.value;
        s.paintKind = t.beforeFreeze.kind;
        s.paintTexture = t.beforeFreeze.texture;
    }
    s.setMode(id);
}

float settingNumber(const json& v, const char* key) {
    if (!v.is_number()) fail("bad_request", std::string("setting ") + key + " is a number");
    const double d = v.get<double>();
    if (!std::isfinite(d)) fail("bad_request", std::string("setting ") + key + " must be finite");
    return static_cast<float>(d);
}

bool settingBool(const json& v, const char* key) {
    if (!v.is_boolean()) fail("bad_request", std::string("setting ") + key + " is true or false");
    return v.get<bool>();
}

int settingInt(const json& v, const char* key) {
    if (!v.is_number_integer()) fail("bad_request", std::string("setting ") + key + " is a whole number");
    return v.get<int>();
}

std::string settingString(const json& v, const char* key) {
    if (v.is_null()) return {};
    if (!v.is_string()) fail("bad_request", std::string("setting ") + key + " is a string");
    return v.get<std::string>();
}

// The settings the palette sends (Maya's groomLabBrushCtx flags by their long names, and the
// catalog's setting ids): lengths in host units, the symmetry in the host's axes.
void applySettings(ToolState& t, const json& settings) {
    if (!settings.is_object()) fail("bad_request", "settings is an object");
    BrushToolSettings s = t.tool.settings;
    float softRadius = t.softRadius;
    const HostSpace& space = t.session.space;
    std::string symmetry;
    bool symmetrySet = false;
    double symmetryCenter = 0.0;
    {
        std::string axis;
        symmetryToHost(space, s.brush.mirrorAxis, s.brush.mirrorCenter, axis, symmetryCenter);
        symmetry = axis;
    }
    try {
        for (const auto& [key, v] : settings.items()) {
            const char* k = key.c_str();
            if (key == "radius") s.setScreenRadius(settingNumber(v, k));
            else if (key == "strength") s.brush.strength = settingNumber(v, k);
            else if (key == "falloff") s.setHardness(1.0f - settingNumber(v, k));
            else if (key == "hardness") s.setHardness(settingNumber(v, k));
            // The curves: a ramp ([[position, value], ...], or its JSON text), null or "" for none.
            else if (key == "falloffProfile") s.setFalloffProfile(v.is_string() ? v.get<std::string>() : v.is_null() ? std::string() : v.dump());
            else if (key == "strengthProfile") s.setStrengthProfile(v.is_string() ? v.get<std::string>() : v.is_null() ? std::string() : v.dump());
            else if (key == "level") s.brush.level = settingInt(v, k);
            else if (key == "group") s.brush.group = settingString(v, k);
            else if (key == "propagate") s.brush.propagate = settingBool(v, k);
            else if (key == "length") s.place.length = toGroomLength(space, settingNumber(v, k));
            else if (key == "segments") s.setSegments(settingInt(v, k));
            else if (key == "tubeRadius") {
                const float r = toGroomLength(space, settingNumber(v, k));
                s.place.tube.width = {r, r};
            } else if (key == "spacing") s.place.spacing = toGroomLength(space, settingNumber(v, k));
            else if (key == "matchNeighbors") s.place.matchNeighbors = settingBool(v, k);
            else if (key == "soloNewGuides") s.place.soloNewGuides = settingBool(v, k);
            else if (key == "selectedOnly") s.selectedOnly = settingBool(v, k);
            else if (key == "softSelectRadius") softRadius = std::max(0.0f, toGroomLength(space, settingNumber(v, k)));
            else if (key == "symmetry") {
                symmetry = settingString(v, k);
                if (symmetry.empty()) symmetry = "none";
                symmetrySet = true;
            } else if (key == "symmetryCenter") {
                symmetryCenter = settingNumber(v, k);
                symmetrySet = true;
            } else if (key == "attribute") s.brush.attribute = settingString(v, k);
            else if (key == "value") s.brush.value = settingNumber(v, k);
            else if (key == "pointwise") s.brush.pointwise = settingBool(v, k);
            else if (key == "collide") s.brush.collide = settingBool(v, k);
            else if (key == "frontFacingOnly" || key == "maskByScalp") s.maskByScalp = settingBool(v, k);
            else if (key == "stretch") s.brush.stretch = settingBool(v, k);
            else if (key == "pressure") s.pressure = settingBool(v, k);
            else if (key == "partName") s.partName = settingString(v, k).empty() ? std::string("part") : settingString(v, k);
            else if (key == "flowSink") {
                const std::string sink = settingString(v, k);
                if (!sink.empty()) flowSinkAngle(sink);  // throws for an unknown kind
                s.flowSink = sink;
            } else if (key == "targets") {
                if (v.is_array()) {
                    std::string names;
                    for (const json& n : v) names += (names.empty() ? "" : ",") + settingString(n, k);
                    s.setTargets(names.empty() ? std::string("none") : names);
                } else {
                    s.setTargets(settingString(v, k));
                }
            } else if (key == "eraseGuides") s.eraseGuides = settingBool(v, k);
            else if (key == "eraseParts") s.eraseParts = settingBool(v, k);
            else if (key == "eraseFlow") s.eraseFlow = settingBool(v, k);
            else if (key == "paintKind") {
                const std::string kind = settingString(v, k);
                if (kind != "guides" && kind != "vertices" && kind != "texture") fail("bad_request", "paintKind is guides, vertices or texture");
                s.paintKind = kind;
            } else if (key == "paintTexture") s.paintTexture = settingString(v, k);
            else if (key == "paintResolution") s.setPaintResolution(settingInt(v, k));
            else if (key == "paintColor") {
                if (!v.is_array() || (v.size() != 3 && v.size() != 4)) fail("bad_request", "paintColor is [r, g, b] or [r, g, b, a]");
                PaintColor c{0.0f, 0.0f, 0.0f, 1.0f};
                for (std::size_t i = 0; i < v.size(); ++i) c[i] = settingNumber(v[i], k);
                s.setPaintColor(c);
            } else if (key == "paintChannels") s.paintChannels = channelMask(settingString(v, k).empty() ? std::string("rgba") : settingString(v, k));
            else if (key == "smoothStrength") s.setSmoothStrength(settingNumber(v, k));
            else fail("bad_request", "no tool setting '" + key + "'");
        }
        if (symmetrySet && symmetry == "topology") {
            s.setSymmetry("topology");  // across the scalp's own surface: no axis to turn into the groom's
        } else if (symmetrySet) {
            symmetryToGroom(space, symmetry, symmetryCenter, s.brush.mirrorAxis, s.brush.mirrorCenter);
            s.brush.mirrorTopology = false;
        }
    } catch (const std::invalid_argument& e) {
        fail("bad_request", e.what());
    }
    t.tool.settings = s;
    t.softRadius = softRadius;
}

json settingsJson(const ToolState& t) {
    const BrushToolSettings& s = t.tool.settings;
    const HostSpace& space = t.session.space;
    std::string axis;
    double center = 0.0;
    symmetryToHost(space, s.brush.mirrorAxis, s.brush.mirrorCenter, axis, center);
    json j;
    j["radius"] = s.screenRadius;
    j["strength"] = s.brush.strength;
    j["falloff"] = 1.0f - s.brush.hardness;
    j["hardness"] = s.brush.hardness;
    // The curves as ramps (null: none).
    j["falloffProfile"] = s.brush.falloffProfile ? json::parse(s.falloffProfileJson()) : json();
    j["strengthProfile"] = s.brush.strengthProfile ? json::parse(s.strengthProfileJson()) : json();
    j["level"] = s.brush.level;
    j["group"] = s.brush.group;
    j["propagate"] = s.brush.propagate;
    j["length"] = toHostLength(space, s.place.length);
    j["segments"] = s.place.segments;
    j["tubeRadius"] = toHostLength(space, s.place.tube.width.empty() ? 0.0f : s.place.tube.width.front());
    j["spacing"] = toHostLength(space, s.place.spacing);
    j["matchNeighbors"] = s.place.matchNeighbors;
    j["soloNewGuides"] = s.place.soloNewGuides;
    j["selectedOnly"] = s.selectedOnly;
    j["softSelectRadius"] = toHostLength(space, t.softRadius);
    j["symmetry"] = s.brush.mirrorTopology ? std::string("topology") : axis;
    j["symmetryCenter"] = center;
    j["attribute"] = s.brush.attribute;
    j["value"] = s.brush.value;
    j["pointwise"] = s.brush.pointwise;
    j["collide"] = s.brush.collide;
    j["frontFacingOnly"] = s.maskByScalp;
    j["stretch"] = s.brush.stretch;
    j["pressure"] = s.pressure;
    j["partName"] = s.partName;
    j["flowSink"] = s.flowSink;
    j["targets"] = s.targetsName();
    j["eraseGuides"] = s.eraseGuides;
    j["eraseParts"] = s.eraseParts;
    j["eraseFlow"] = s.eraseFlow;
    j["paintKind"] = s.paintKind;
    j["paintTexture"] = s.paintTexture;
    j["paintResolution"] = s.paintResolution;
    j["paintColor"] = s.paintColor;
    j["paintChannels"] = channelLetters(s.paintChannels);
    j["smoothStrength"] = s.smoothStrength;
    return j;
}

json toolJson(const ToolState& t) {
    const BrushEntry* entry = brushEntry(toolIdOf(t));
    return {{"tool", toolIdOf(t)},
            {"mode", t.tool.settings.modeName()},
            {"settings", settingsJson(t)},
            {"shows", entry ? json(entry->settings) : json::array()},
            {"ringColor", brushRingColor(t.tool.settings)},
            {"pathColor", brushPathColor(t.tool.settings)},
            {"hoverColor", brushHoverColor(t.tool.settings)}};
}

// ---- Selection ----------------------------------------------------------------------------------

// The selection less the guides that are gone (an undo, an erase).
void pruneSelection(ToolState& t) {
    if (t.selection.empty()) return;
    const std::shared_ptr<const GuideHierarchy> h = t.session.groom->storedHierarchy();
    std::vector<GuideId> kept;
    for (const GuideId id : t.selection) {
        if (h && h->guide(id)) kept.push_back(id);
    }
    if (kept.size() != t.selection.size()) {
        t.selection = std::move(kept);
        ++t.selectionVersion;
    }
}

void setSelection(ToolState& t, std::vector<GuideId> ids) {
    if (ids == t.selection) return;
    t.selection = std::move(ids);
    ++t.selectionVersion;
}

// What strokes read of the selection and the hidden groups.
void prepareHost(ToolState& t) {
    pruneSelection(t);
    t.host.setSelection(t.selection, t.softRadius);
    t.host.setHiddenGroups(t.hidden);
}

json selectionJson(const ToolState& t) {
    return {{"selection", t.selection}, {"count", t.selection.size()}, {"softSelectRadius", toHostLength(t.session.space, t.softRadius)},
            {"version", t.selectionVersion}};
}

std::vector<GuideId> idsOf(const json& v) {
    std::vector<GuideId> ids;
    if (!v.is_array()) fail("bad_request", "ids is a list of guide IDs");
    for (const json& id : v) {
        if (!id.is_number_unsigned() && !(id.is_number_integer() && id.get<std::int64_t>() > 0)) fail("bad_request", "a guide ID is a positive integer");
        ids.push_back(id.get<GuideId>());
    }
    return ids;
}

// ---- Undo ---------------------------------------------------------------------------------------

json undoJson(const ToolState& t) {
    const std::vector<StrokeEdit>& done = t.host.edits();
    return {{"canUndo", !done.empty()},
            {"canRedo", !t.redoLabels.empty()},
            {"undoLabel", done.empty() ? json(nullptr) : json(done.back().mode)},
            {"redoLabel", t.redoLabels.empty() ? json(nullptr) : json(t.redoLabels.back())},
            {"undoDepth", done.size()},
            {"redoDepth", t.redoLabels.size()}};
}

bool undo(ToolState& t) {
    const std::vector<StrokeEdit>& done = t.host.edits();
    if (done.empty()) return false;
    const StrokeEdit& last = done.back();
    const std::string label = last.mode, map = last.textureMap;
    if (!map.empty() && !last.texture.empty() && !t.host.paintOf(map)) {
        try {
            t.host.texturePaint(map, last.texture.channels, last.texture.width);  // the file opened for its tiles
        } catch (const std::exception&) {
        }
    }
    if (!t.host.undo()) return false;
    if (!map.empty()) writeTexture(t, map);
    t.redoLabels.push_back(label);
    t.redoMaps.push_back(map);
    pruneSelection(t);
    return true;
}

bool redo(ToolState& t) {
    if (t.redoLabels.empty()) return false;
    const std::string map = t.redoMaps.back();
    if (!t.host.redo()) return false;
    t.redoLabels.pop_back();
    t.redoMaps.pop_back();
    if (!map.empty()) writeTexture(t, map);
    pruneSelection(t);
    return true;
}

// An edit of the stored guides as one undo step (the app's GroomDocument::editGuides): `edit` on
// a copy of the authored hierarchy, evaluated on the scalp again, stored.
void editGuides(ToolState& t, const std::string& label, const std::function<void(GuideHierarchy&)>& edit) {
    if (t.tool.stroking()) t.tool.cancel(&t.host);
    GroomSession& session = *t.session.groom;
    const HierarchyPtr authored = session.authoredHierarchy();
    GuideHierarchy h = authored ? *authored : GuideHierarchy{};
    try {
        edit(h);
        h.evaluate(session.scalp().get());
    } catch (const RequestError&) {
        throw;
    } catch (const std::exception& e) {
        fail("bad_request", e.what());
    }
    StrokeEdit stroke;
    stroke.mode = label;
    stroke.set(StrokeField::Guides, h.toJson());
    t.host.commit(std::move(stroke));
    pruneSelection(t);
}

// ---- Texture maps -------------------------------------------------------------------------------

std::string mapsFolder(Session& s) {
    if (!s.mapsDir.empty()) return s.mapsDir;
    std::error_code ec;
    const fs::path dir = fs::temp_directory_path(ec) / blockName("s" + std::to_string(s.id) + "maps", 0);
    return dir.generic_u8string();
}

void writeTexture(ToolState& t, const std::string& name) {
    const std::shared_ptr<TexturePaint> paint = t.host.paintOf(name);
    if (!paint) return;
    auto it = t.host.kept.find(name);
    std::string path = it != t.host.kept.end() ? it->second.path : std::string();
    if (path.empty()) {
        const auto entry = t.session.groom->scalpMaps().maps.find(name);
        if (entry == t.session.groom->scalpMaps().maps.end()) return;
        path = resolveMapFile(t.session, entry->second.texture);
    }
    if (path.empty()) return;
    std::error_code ec;
    fs::create_directories(fs::u8path(path).parent_path(), ec);
    paint->save(path);
    if (it != t.host.kept.end() && it->second.paint == paint) it->second.written();
}

// ---- The overlay --------------------------------------------------------------------------------

constexpr float kPi = 3.14159265358979f;

void appendColoredLine(DisplayGeometry& g, const std::vector<Vec3f>& points, const std::array<float, 3>& color) {
    if (points.size() < 2) return;
    const auto first = static_cast<std::uint32_t>(g.positions.size() / 3);
    for (const Vec3f& p : points) {
        g.positions.insert(g.positions.end(), {p.x, p.y, p.z});
        g.colors.insert(g.colors.end(), {color[0], color[1], color[2], 1.0f});
    }
    for (std::uint32_t k = 0; k + 1 < points.size(); ++k) g.indices.insert(g.indices.end(), {first + k, first + k + 1});
}

void clearOverlay(ToolState& t) {
    t.overlay = DisplayGeometry();
    t.overlay.overlay = true;
    t.ringValid = t.ringOnScalp = false;
    ++t.overlayVersion;
}

// The brush overlay at (x, y), as the app's BrushSession draws it: the guides the brush would
// touch (hovering, `withLines`), the ring on the scalp under the cursor (else at the target's
// depth) as wide as the brush there, facing the camera (a dot for Place and Draw), and a Part or
// Flow stroke's path so far.
void rebuildOverlay(ToolState& t, float x, float y, bool withLines) {
    clearOverlay(t);
    const BrushToolSettings& s = t.tool.settings;
    using Tool = BrushToolSettings::Tool;
    if (withLines && !t.selectTool) {
        prepareHost(t);
        std::vector<std::vector<Vec3f>> lines;
        try {
            lines = t.tool.affectedLines(t.host, t.view, x, y);
        } catch (const std::exception&) {
            lines.clear();
        }
        for (const std::vector<Vec3f>& line : lines) appendColoredLine(t.overlay, line, brushHoverColor(s));
    }
    Vec3f origin, direction;
    if (!t.selectTool && t.view.ray(x, y, origin, direction)) {
        Vec3f center;
        bool have = false;
        const MeshPtr scalp = t.session.groom->scalp();
        SurfacePoint hit;
        if (scalp && scalp->triangleCount() > 0 && scalp->intersectRay(origin, direction, &hit)) {
            center = hit.position;
            have = true;
            t.ringOnScalp = true;
        } else {
            have = atTargetDepth(t, x, y, center);
        }
        if (have) {
            const bool creating = s.tool == Tool::Place || s.tool == Tool::Draw;
            const float radius = worldRadiusAt(t.view, center, x, y, creating ? 6.0f : s.screenRadius);
            if (radius > 0.0f) {
                Vec3f eye, along;
                bool ortho = false;
                t.view.camera(eye, along, ortho);
                const Vec3f n = ortho ? along : (center - eye).normalized();
                const Vec3f u = (std::abs(n.y) < 0.9f ? Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f}).cross(n).normalized();
                const Vec3f v = n.cross(u);
                std::vector<Vec3f> ring;
                constexpr int kSegments = 48;
                for (int k = 0; k <= kSegments; ++k) {
                    const float a = 2.0f * kPi * static_cast<float>(k) / kSegments;
                    ring.push_back(center + u * (std::cos(a) * radius) + v * (std::sin(a) * radius));
                }
                appendColoredLine(t.overlay, ring, brushRingColor(s));
                t.ringValid = true;
                t.ringCenter = center;
                t.ringRadius = radius;
                t.ringNormal = n;
            }
        }
    }
    if (t.tool.stroking() && (s.tool == Tool::Part || s.tool == Tool::Flow)) appendColoredLine(t.overlay, t.tool.drawnPath(), brushPathColor(s));
}

json positionsToHost(Arena& arena, const std::vector<float>& xyz, const HostSpace& space) {
    const std::size_t n = xyz.size() / 3;
    ArrayRef ref;
    auto* dst = static_cast<float*>(arena.add(n * 3, sizeof(float), "<f4", {n, 3}, &ref));
    for (std::size_t i = 0; i < n; ++i) space.toHost({xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]}, dst + 3 * i);
    return refJson(ref);
}

json directionsToHost(Arena& arena, const std::vector<float>& xyz, const HostSpace& space) {
    const std::size_t n = xyz.size() / 3;
    ArrayRef ref;
    auto* dst = static_cast<float*>(arena.add(n * 3, sizeof(float), "<f4", {n, 3}, &ref));
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3f d = hostDirection(space, {xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]});
        dst[3 * i] = d.x;
        dst[3 * i + 1] = d.y;
        dst[3 * i + 2] = d.z;
    }
    return refJson(ref);
}

std::size_t geometryBytes(const DisplayGeometry& g) {
    return Arena::reserve(g.positions.size() * 4) + Arena::reserve(g.indices.size() * 4) + Arena::reserve(g.colors.size() * 4) +
           Arena::reserve(g.normals.size() * 4);
}

// A geometry's arrays in the host's space: positions (n, 3), indices (pairs or triples), colors
// (n, 4) when per vertex, normals (n, 3) for a shaded surface.
json geometryArrays(Arena& arena, const DisplayGeometry& g, const HostSpace& space) {
    json a;
    a["positions"] = positionsToHost(arena, g.positions, space);
    const std::size_t per = g.triangles ? 3 : 2;
    a["indices"] = put(arena, g.indices.data(), g.indices.size() / per, per);
    if (!g.colors.empty()) a["colors"] = put(arena, g.colors.data(), g.colors.size() / 4, 4);
    if (!g.normals.empty()) a["normals"] = directionsToHost(arena, g.normals, space);
    return a;
}

json overlayJson(ToolState& t, bool arrays) {
    const HostSpace& space = t.session.space;
    json r;
    json ring;
    ring["valid"] = t.ringValid;
    if (t.ringValid) {
        ring["center"] = hostPoint(space, t.ringCenter);
        ring["radius"] = toHostLength(space, t.ringRadius);
        const Vec3f n = hostDirection(space, t.ringNormal);
        ring["normal"] = {n.x, n.y, n.z};
        ring["onScalp"] = t.ringOnScalp;
        ring["screenRadius"] = t.tool.settings.screenRadius;
        ring["color"] = brushRingColor(t.tool.settings);
    }
    r["ring"] = ring;
    json overlay;
    overlay["version"] = t.overlayVersion;
    overlay["vertexCount"] = t.overlay.positions.size() / 3;
    if (arrays) {
        t.overlayOut.begin(geometryBytes(t.overlay));
        overlay["arrays"] = geometryArrays(t.overlayOut, t.overlay, space);
    }
    r["overlay"] = overlay;
    return r;
}

json warningsJson(ToolState& t) {
    json w = t.host.warnings();
    t.host.clearWarnings();
    return w;
}

// ---- Strokes ------------------------------------------------------------------------------------

struct Pixel {
    float x = 0.0f, y = 0.0f, pressure = 1.0f;
};

std::vector<Pixel> pixelsOf(const json& v, float pressure) {
    std::vector<Pixel> out;
    if (!v.is_array()) fail("bad_request", "points is a list of [x, y] or [x, y, pressure]");
    for (const json& p : v) {
        if (!p.is_array() || p.size() < 2 || p.size() > 3) fail("bad_request", "a point is [x, y] or [x, y, pressure]");
        for (const json& c : p) {
            if (!c.is_number() || !std::isfinite(c.get<double>())) fail("bad_request", "a point holds numbers");
        }
        out.push_back({p[0].get<float>(), p[1].get<float>(), p.size() == 3 ? p[2].get<float>() : pressure});
    }
    return out;
}

// The core's tools throw on bad data: the stroke is dropped, the request answers why.
template <class F>
void guarded(ToolState& t, const char* what, F&& fn) {
    try {
        fn();
    } catch (const RequestError&) {
        t.tool.cancel(&t.host);
        throw;
    } catch (const std::exception& e) {
        t.tool.cancel(&t.host);
        fail("stroke", std::string(what) + ": " + e.what());
    }
}

void requireScalp(Session& s) {
    const MeshPtr scalp = s.groom->rawScalp();
    if (!scalp || scalp->triangleCount() == 0) fail("state", "the session has no scalp: set_scalp first");
}

// ---- Guides ---------------------------------------------------------------------------------------

std::string uniqueGroupName(const GuideHierarchy& h, const std::string& base) {
    if (h.findGroup(base) == kNoGroup) return base;
    for (int k = 2;; ++k) {
        const std::string name = base + std::to_string(k);
        if (h.findGroup(name) == kNoGroup) return name;
    }
}

GroupId groupNamed(const GuideHierarchy& h, const json& args, const char* key) {
    const std::string name = needString(args, key);
    const GroupId id = h.findGroup(name);
    if (id == kNoGroup) fail("not_found", "no group named '" + name + "'");
    return id;
}

json guidesInfo(ToolState& t) {
    GroomSession& session = *t.session.groom;
    const std::shared_ptr<const GuideHierarchy> h = session.storedHierarchy();
    json groups = json::array();
    std::size_t frozen = 0, primaries = 0;
    if (h) {
        for (const GuideGroup& g : h->groups()) {
            const GuideGroup* parent = g.parent == kNoGroup ? nullptr : h->group(g.parent);
            groups.push_back({{"id", g.id},
                              {"name", g.name},
                              {"parent", parent ? json(parent->name) : json(nullptr)},
                              {"level", h->groupDepth(g.id)},
                              {"guides", h->guidesInGroup(g.id).size()},
                              {"hidden", t.hidden.count(g.name) > 0}});
        }
        for (const HierarchyGuide& g : h->guides()) {
            if (!g.isChild()) ++primaries;
            float v = 0.0f;
            if (h->effectiveAttribute(g, "freeze", &v) && v >= 0.5f) ++frozen;
        }
    }
    json r;
    r["groups"] = groups;
    r["levels"] = h ? h->maxDepth() + 1 : 0;
    r["guides"] = h ? h->guides().size() : 0;
    r["primaries"] = primaries;
    r["frozen"] = frozen;
    r["layers"] = h ? json(h->attributeNames()) : json::array();
    r["hiddenGroups"] = t.hidden;
    r["selection"] = selectionJson(t);
    r["hierarchyError"] = session.hierarchyError();
    return r;
}

json guidesEdit(ToolState& t, const json& args) {
    const std::string op = needString(args, "op");
    const HostSpace& space = t.session.space;
    auto targets = [&](const GuideHierarchy&) {
        if (args.contains("ids") && !args["ids"].is_null()) return idsOf(args["ids"]);
        pruneSelection(t);
        if (t.selection.empty()) fail("state", op + ": no guides given and none selected");
        return t.selection;
    };
    json r;
    if (op == "add_group") {
        const std::string parent = optString(args, "parent", "");
        std::string made;
        editGuides(t, "add group", [&](GuideHierarchy& h) {
            GroupId parentId = kNoGroup;
            if (!parent.empty()) {
                parentId = h.findGroup(parent);
                if (parentId == kNoGroup) fail("not_found", "no group named '" + parent + "'");
            }
            const std::string name = optString(args, "name", "");
            made = name.empty() ? uniqueGroupName(h, parent.empty() ? "group" : parent + "_children") : name;
            if (h.findGroup(made) != kNoGroup) fail("bad_request", "a group named '" + made + "' is there already");
            h.addGroup(made, parentId);
        });
        r["group"] = made;
    } else if (op == "remove_group") {
        editGuides(t, "remove group", [&](GuideHierarchy& h) { h.removeGroup(groupNamed(h, args, "group")); });
    } else if (op == "rename_group") {
        const std::string from = needString(args, "group"), to = needString(args, "name");
        editGuides(t, "rename group", [&](GuideHierarchy& h) { h.renameGroup(groupNamed(h, args, "group"), to); });
        if (t.hidden.erase(from)) t.hidden.insert(to);
    } else if (op == "generate_children") {
        std::string made;
        std::size_t count = 0;
        editGuides(t, "generate children", [&](GuideHierarchy& h) {
            GroupId id = kNoGroup;
            if (args.contains("group") && !args["group"].is_null()) {
                id = groupNamed(h, args, "group");
            } else if (!h.groups().empty()) {
                id = h.groups().back().id;  // the last group, as Maya's Guides tab
            }
            if (id == kNoGroup) fail("state", "the groom has no guides yet");
            ChildGenerationParams p;
            p.count = static_cast<std::size_t>(std::max(1.0, optNumber(args, "count", 5.0)));
            p.rootSpread = static_cast<float>(optNumber(args, "rootSpread", p.rootSpread));
            p.tipSpread = static_cast<float>(optNumber(args, "tipSpread", p.tipSpread));
            p.twist = static_cast<float>(optNumber(args, "twist", p.twist));
            p.jitter = static_cast<float>(optNumber(args, "jitter", p.jitter));
            p.childTubeScale = static_cast<float>(optNumber(args, "childTubeScale", p.childTubeScale));
            p.seed = static_cast<std::uint64_t>(std::max(0.0, optNumber(args, "seed", 0.0)));
            std::vector<GuideId> parents;
            const bool selected = optBool(args, "selected", false);
            if (selected) {
                pruneSelection(t);
                for (const GuideId g : t.selection) {
                    const HierarchyGuide* guide = h.guide(g);
                    if (guide && guide->group == id) parents.push_back(g);
                }
                if (parents.empty()) fail("state", "none of the selected guides is in that group");
            } else if (h.guidesInGroup(id).empty()) {
                fail("state", "the group has no guides to be parents");
            }
            made = optString(args, "childGroup", "");
            if (made.empty()) {
                for (const GuideGroup& g : h.groups()) {
                    if (g.parent == id) {
                        made = g.name;
                        break;
                    }
                }
            }
            if (made.empty()) made = uniqueGroupName(h, h.group(id)->name + "_children");
            const std::size_t before = h.guides().size();
            h.generateChildren(id, made, p, selected ? &parents : nullptr);
            count = h.guides().size() >= before ? h.guides().size() - before : 0;
        });
        r["group"] = made;
        r["added"] = count;
    } else if (op == "delete_guides") {
        std::vector<GuideId> ids;
        editGuides(t, "delete guides", [&](GuideHierarchy& h) {
            ids = targets(h);
            h.removeGuides(ids);
        });
        r["removed"] = ids.size();
    } else if (op == "lock" || op == "unlock") {
        editGuides(t, op + " guides", [&](GuideHierarchy& h) {
            for (const GuideId id : targets(h)) {
                const HierarchyGuide* g = h.guide(id);
                if (!g || !g->isChild()) continue;
                if (op == "lock") h.lock(id);
                else h.unlock(id);
            }
        });
    } else if (op == "set_attribute") {
        const std::string name = needString(args, "name");
        const float value = static_cast<float>(optNumber(args, "value", 1.0));
        if (!validMapName(name)) fail("bad_request", mapNameError(name));
        editGuides(t, "set " + name, [&](GuideHierarchy& h) {
            for (const GuideId id : targets(h)) {
                if (h.guide(id)) h.setAttribute(id, name, value);
            }
        });
    } else if (op == "freeze" || op == "unfreeze") {
        editGuides(t, op + " guides", [&](GuideHierarchy& h) {
            for (const GuideId id : targets(h)) {
                if (h.guide(id)) h.setAttribute(id, kFreezeTool, op == "freeze" ? 1.0f : 0.0f);
            }
        });
    } else if (op == "unfreeze_all") {
        editGuides(t, "unfreeze all", [&](GuideHierarchy& h) { h.removeAttribute(kFreezeTool); });
    } else if (op == "remove_layer") {
        const std::string name = needString(args, "name");
        editGuides(t, "remove layer " + name, [&](GuideHierarchy& h) { h.removeAttribute(name); });
    } else if (op == "mirror_all" || op == "mirror_group") {
        int axis = 0;
        float center = 0.0f;
        symmetryToGroom(space, optString(args, "axis", "x"), optNumber(args, "center", 0.0), axis, center);
        if (axis < 0) fail("bad_request", "axis is x, y or z");
        const bool fromPositive = optBool(args, "fromPositive", true);
        std::size_t made = 0;
        editGuides(t, op == "mirror_all" ? "mirror all" : "mirror group", [&](GuideHierarchy& h) {
            if (op == "mirror_all") {
                made = h.mirrorAll(axis, center, fromPositive).size();
            } else {
                const GroupId id = groupNamed(h, args, "group");
                if (h.groupDepth(id) != 0) fail("bad_request", "mirror a root group: its children follow");
                made = h.mirrorGroup(id, axis, center, fromPositive).size();
            }
        });
        r["added"] = made;
    } else if (op == "promote_group") {
        editGuides(t, "promote group", [&](GuideHierarchy& h) { h.promoteGroup(groupNamed(h, args, "group")); });
    } else if (op == "demote_group") {
        editGuides(t, "demote group", [&](GuideHierarchy& h) { h.demoteGroup(groupNamed(h, args, "group"), groupNamed(h, args, "parent")); });
    } else if (op == "reparent") {
        editGuides(t, "reparent guides", [&](GuideHierarchy& h) {
            const json& parent = need(args, "parent");
            if (!parent.is_number_unsigned()) fail("bad_request", "parent is a guide ID");
            h.reparent(targets(h), parent.get<GuideId>());
        });
    } else {
        fail("bad_request",
             "op is add_group, remove_group, rename_group, generate_children, delete_guides, lock, unlock, set_attribute, "
             "freeze, unfreeze, unfreeze_all, remove_layer, mirror_all, mirror_group, promote_group, demote_group or reparent");
    }
    r["undo"] = undoJson(t);
    r["guides"] = guidesInfo(t);
    return r;
}

// ---- Maps ---------------------------------------------------------------------------------------

json mapsList(ToolState& t) {
    Session& s = t.session;
    json maps = json::array();
    for (const auto& [name, entry] : s.groom->scalpMaps().maps) {
        json m = {{"name", name}, {"kind", entry.kind}, {"channels", entry.channels}};
        if (entry.kind == "texture") {
            m["texture"] = entry.texture;
            m["file"] = resolveMapFile(s, entry.texture);
        }
        maps.push_back(m);
    }
    if (const auto h = s.groom->storedHierarchy()) {
        for (const std::string& name : h->attributeNames()) {
            maps.push_back({{"name", name}, {"kind", "guides"}, {"channels", 1}, {"freezeMask", name == kFreezeTool}});
        }
    }
    return {{"maps", maps}, {"folder", mapsFolder(s)}};
}

void commitMaps(ToolState& t, const std::string& label, const ScalpMaps& maps) {
    if (t.tool.stroking()) t.tool.cancel(&t.host);
    t.session.mapsOwned = true;
    StrokeEdit edit;
    edit.mode = label;
    edit.set(StrokeField::ScalpMaps, maps.toJson());
    t.host.commit(std::move(edit));
}

// A new scalp map's entry (vertices: `value` or `fill` at every vertex; texture: its image written
// in the maps folder, a free name), not committed.
ScalpMapEntry newScalpMapEntry(Session& s, const ScalpMesh& scalp, const std::string& name, const std::string& kind, bool color,
                               float value, const PaintColor& fill, int resolution);

json mapCreate(ToolState& t, const json& args) {
    Session& s = t.session;
    const std::string name = needString(args, "name");
    const std::string kind = optString(args, "kind", "vertices");
    const bool color = optBool(args, "color", false);
    const float value = static_cast<float>(optNumber(args, "value", 0.0));
    PaintColor fill{0.0f, 0.0f, 0.0f, 1.0f};
    if (args.contains("fill") && !args["fill"].is_null()) {
        const json& f = args["fill"];
        if (!f.is_array() || (f.size() != 3 && f.size() != 4)) fail("bad_request", "fill is [r, g, b] or [r, g, b, a]");
        for (std::size_t i = 0; i < f.size(); ++i) fill[i] = std::clamp(settingNumber(f[i], "fill"), 0.0f, 1.0f);
    }
    if (!validMapName(name)) fail("bad_request", mapNameError(name));
    const json existing = mapsList(t);
    for (const json& m : existing["maps"]) {
        if (m["name"] == name) fail("bad_request", "the groom already has a map named '" + name + "'");
    }
    const MeshPtr scalp = s.groom->cageScalp();
    if (!scalp) fail("state", "no scalp to paint on: set_scalp first");
    if (kind == "guides") {
        if (color) fail("bad_request", "a guide layer is gray: make a color map on the scalp's vertices or as a texture");
        editGuides(t, "create map " + name, [&](GuideHierarchy& h) {
            bool any = false;
            for (const HierarchyGuide& g : h.guides()) {
                if (g.parent != kNoGuide) continue;  // children inherit their primary's
                h.setAttribute(g.id, name, value);
                any = true;
            }
            if (!any) fail("state", "no primary guides to carry the layer: place some first");
        });
        return {{"name", name}, {"kind", kind}, {"undo", undoJson(t)}};
    }
    ScalpMaps maps = s.groom->scalpMaps();
    maps.maps[name] = newScalpMapEntry(s, *scalp, name, kind, color, value, fill, static_cast<int>(optNumber(args, "resolution", 1024.0)));
    commitMaps(t, "create map " + name, maps);
    json r = {{"name", name}, {"kind", kind}, {"undo", undoJson(t)}};
    if (kind == "texture") r["file"] = maps.maps[name].texture;
    return r;
}

ScalpMapEntry newScalpMapEntry(Session& s, const ScalpMesh& scalpMesh, const std::string& name, const std::string& kind, bool color,
                               float value, const PaintColor& fill, int resolution) {
    const ScalpMesh* scalp = &scalpMesh;
    ScalpMapEntry entry;
    entry.channels = color ? kColorMap : kGrayMap;
    if (kind == "vertices") {
        entry.kind = "vertices";
        const std::size_t n = scalp->pointCount();
        entry.values.resize(n * static_cast<std::size_t>(entry.channels));
        for (std::size_t v = 0; v < n; ++v) {
            if (color) {
                for (std::size_t c = 0; c < 4; ++c) entry.values[v * 4 + c] = fill[c];
            } else {
                entry.values[v] = value;
            }
        }
    } else if (kind == "texture") {
        if (!scalp->hasUVs()) fail("state", "the scalp has no UVs to paint a texture map on");
        const fs::path folder = fs::u8path(mapsFolder(s));
        fs::path file = folder / (name + ".png");
        for (int k = 2; fs::exists(file); ++k) file = folder / (name + "_" + std::to_string(k) + ".png");
        try {
            fs::create_directories(file.parent_path());
            const int size = std::clamp(resolution, BrushToolSettings::kMinPaintResolution, BrushToolSettings::kMaxPaintResolution);
            const TexturePaint paint = color ? TexturePaint(*scalp, size, size, fill) : TexturePaint(*scalp, size, size, value);
            paint.save(file.generic_u8string());
        } catch (const std::exception& e) {
            fail("io", std::string("cannot write the map's image: ") + e.what());
        }
        entry.kind = "texture";
        entry.texture = file.generic_u8string();
    } else {
        fail("bad_request", "a map's kind is vertices, texture or guides, not '" + kind + "'");
    }
    return entry;
}

json mapForParam(ToolState& t, const json& args) {
    Session& s = t.session;
    const std::string node = needString(args, "node"), param = needString(args, "param");
    const std::string kind = optString(args, "kind", "vertices");
    if (kind != "vertices" && kind != "texture") fail("bad_request", "a setting's map lives on the scalp: vertices or texture, not '" + kind + "'");
    const MeshPtr scalp = s.groom->cageScalp();
    if (!scalp) fail("state", "no scalp to paint on: set_scalp first");
    if (t.tool.stroking()) t.tool.cancel(&t.host);
    prepareHost(t);
    setTool(t, "paint");  // out of Freeze or Select, as picking Paint does
    const std::string graphBefore = s.groom->data().graphJson;
    ParamMapRef found;
    try {
        found = paramMapOf(graphBefore, s.groom->scalpMaps(), s.groom->storedHierarchy().get(), node, param);
    } catch (const std::exception& e) {
        fail("bad_request", e.what());
    }
    std::string map = found.map, mapKind = found.kind;
    if (map.empty() && kind == "vertices") {
        try {
            const ParamMapBinding b = paintParamMap(t.host, t.tool.settings, node, param);  // made and linked: one step
            map = b.map;
            mapKind = b.kind;
        } catch (const std::exception& e) {
            fail("bad_request", e.what());
        }
    } else if (map.empty()) {
        // A texture: its image beside the groom, white, linked by its file in the same step.
        std::set<std::string> taken;
        for (const json& m : mapsList(t)["maps"]) taken.insert(m["name"].get<std::string>());
        const std::string base = sanitizeMapName(node + "_" + param);
        map = base;
        for (int k = 1; taken.count(map); ++k) map = base + std::to_string(k);
        ScalpMaps maps = s.groom->scalpMaps();
        maps.maps[map] = newScalpMapEntry(s, *scalp, map, "texture", false, 1.0f, PaintColor{1.0f, 1.0f, 1.0f, 1.0f},
                                          static_cast<int>(optNumber(args, "resolution", 1024.0)));
        std::string graph;
        try {
            graph = wireParamMap(graphBefore, maps, node, param, map, "texture");
        } catch (const std::exception& e) {
            fail("bad_request", e.what());
        }
        StrokeEdit edit;
        edit.mode = "map " + node + "." + param;
        edit.set(StrokeField::Graph, std::move(graph));
        edit.set(StrokeField::ScalpMaps, maps.toJson());
        t.host.commit(std::move(edit));
        mapKind = "texture";
    }
    // The Paint brush on it, toward 1: strokes paint the param back to its value, Ctrl toward 0.
    BrushToolSettings& st = t.tool.settings;
    st.setMode("paint");
    st.brush.attribute = map;
    st.paintKind = mapKind;
    const auto entry = s.groom->scalpMaps().maps.find(map);
    st.paintTexture = mapKind == "texture" && entry != s.groom->scalpMaps().maps.end() ? entry->second.texture : std::string();
    st.brush.value = 1.0f;
    const bool changed = s.groom->data().graphJson != graphBefore;
    return {{"name", map}, {"kind", mapKind}, {"made", changed}, {"graphChanged", changed}, {"tool", toolJson(t)}, {"undo", undoJson(t)}};
}

json mapRemove(ToolState& t, const json& args) {
    Session& s = t.session;
    const std::string name = needString(args, "name");
    ScalpMaps maps = s.groom->scalpMaps();
    if (maps.maps.count(name)) {
        maps.maps.erase(name);
        t.host.kept.erase(name);
        t.host.setTexturePaint(name, nullptr);
        commitMaps(t, "remove map " + name, maps);
    } else {
        const auto h = s.groom->storedHierarchy();
        const std::vector<std::string> layers = h ? h->attributeNames() : std::vector<std::string>{};
        if (std::find(layers.begin(), layers.end(), name) == layers.end()) fail("not_found", "no map named '" + name + "'");
        editGuides(t, "remove map " + name, [&](GuideHierarchy& hh) { hh.removeAttribute(name); });
    }
    return {{"removed", name}, {"undo", undoJson(t)}};
}

// The map shown on the scalp: the session's view of a vertex map or guide layer; a texture map
// (its file, or the brush's live image while painting) sampled at the scalp's vertices, as the
// app shows it.
ScalpMapView mapView(Session& s, const std::string& name, const std::string& channel) {
    ScalpMapView view = s.groom->scalpMapView(name, channel);
    if (view.kind != "texture") return view;
    const MeshPtr scalp = s.groom->scalp();
    if (!scalp || !scalp->hasUVs()) {
        view.kind.clear();
        return view;
    }
    TexturePtr texture;
    if (!view.preview) {
        try {
            texture = loadTexture(view.texture);
        } catch (const std::exception&) {
        }
    }
    const Texture::Image* image = texture ? texture->image() : nullptr;
    if (!view.preview && !image) {
        view.kind.clear();
        return view;
    }
    const std::size_t per = view.channels == 4 ? 4 : 1;
    auto values = std::make_shared<std::vector<float>>(scalp->pointCount() * per, 0.0f);
    std::vector<std::uint8_t> seen(scalp->pointCount(), 0);
    const auto& uvs = scalp->cornerUVs();
    const auto& triangles = scalp->triangles();
    for (std::size_t tri = 0; tri < triangles.size(); ++tri) {
        for (std::size_t k = 0; k < 3; ++k) {
            const auto v = static_cast<std::size_t>(triangles[tri][k]);
            if (seen[v]) continue;
            seen[v] = 1;
            const float u = uvs[tri * 3 + k][0], w = uvs[tri * 3 + k][1];
            std::array<float, 4> c{};
            if (view.preview) {
                const int pw = view.preview->width(), ph = view.preview->height();
                if (pw <= 0 || ph <= 0) continue;
                const int x = std::clamp(static_cast<int>(std::floor(u * static_cast<float>(pw))), 0, pw - 1);
                const int y = std::clamp(static_cast<int>(std::floor(w * static_cast<float>(ph))), 0, ph - 1);
                const std::vector<std::uint8_t>& rgba = view.preview->rgba();
                const std::size_t at = (static_cast<std::size_t>(y) * static_cast<std::size_t>(pw) + static_cast<std::size_t>(x)) * 4;
                if (at + 3 >= rgba.size()) continue;
                for (std::size_t i = 0; i < 4; ++i) c[i] = rgba[at + i] / 255.0f;
                if (per == 1) c[0] = (c[0] + c[1] + c[2]) / 3.0f;
            } else {
                c = image->sample(u, w);
                if (per == 1) c[0] = 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
            }
            for (std::size_t i = 0; i < per; ++i) (*values)[v * per + i] = c[i];
        }
    }
    view.kind = "vertices";
    view.values = values;
    view.version = hashCombine(hashCombine(view.version, scalp->contentHash()), view.preview ? view.preview->revision() : 0);
    return view;
}

// ---- Display ------------------------------------------------------------------------------------

std::array<float, 3> rgbOf(const json& v, const char* what) {
    if (!v.is_array() || v.size() < 3) fail("bad_request", std::string(what) + " is [r, g, b]");
    return {settingNumber(v[0], what), settingNumber(v[1], what), settingNumber(v[2], what)};
}

std::array<float, 4> rgbaOf(const json& v, const char* what) {
    const std::array<float, 3> c = rgbOf(v, what);
    return {c[0], c[1], c[2], v.size() > 3 ? settingNumber(v[3], what) : 1.0f};
}

int colorModeOf(const json& v) {
    if (v.is_number_integer()) {
        const int m = v.get<int>();
        if (m < kSolidColors || m > kDensityColors) fail("bad_request", "colorMode is 0-5");
        return m;
    }
    const std::string name = settingString(v, "colorMode");
    static const std::map<std::string, int> modes = {{"solid", kSolidColors}, {"colorSet", kColorSetColors}, {"attribute", kAttributeColors},
                                                     {"random", kRandomColors}, {"clump", kClumpColors},     {"density", kDensityColors}};
    const auto it = modes.find(name);
    if (it == modes.end()) fail("bad_request", "colorMode is solid, colorSet, attribute, random, clump or density");
    return it->second;
}

const char* colorModeName(int mode) {
    switch (mode) {
        case kColorSetColors: return "colorSet";
        case kAttributeColors: return "attribute";
        case kRandomColors: return "random";
        case kClumpColors: return "clump";
        case kDensityColors: return "density";
        default: return "solid";
    }
}

DisplaySettings displaySettingsOf(ToolState& t, const json& v) {
    DisplaySettings s;
    s.guideSmoothing = 4;
    std::string hidden;
    for (const std::string& g : t.hidden) hidden += (hidden.empty() ? "" : " ") + g;
    s.hiddenGuideGroups = hidden;
    if (v.is_null()) return s;
    if (!v.is_object()) fail("bad_request", "settings is an object");
    for (const auto& [key, x] : v.items()) {
        const char* k = key.c_str();
        if (key == "guides") s.guides = settingBool(x, k);
        else if (key == "tubes") s.tubes = settingBool(x, k);
        else if (key == "flow") s.flow = settingBool(x, k);
        else if (key == "guideWidth") s.guideWidth = settingNumber(x, k);
        else if (key == "guideColor") s.guideColor = rgbOf(x, k);
        else if (key == "themedGuideColors") s.themedGuideColors = settingBool(x, k);
        else if (key == "guideColorMode") {
            if (x.is_string()) {
                const std::string m = x.get<std::string>();
                s.guideColorMode = m == "layer" ? 1 : m == "group" ? 2 : m == "level" ? 0 : -1;
                if (s.guideColorMode < 0) fail("bad_request", "guideColorMode is level, layer or group");
            } else {
                s.guideColorMode = std::clamp(settingInt(x, k), 0, 2);
            }
        } else if (key == "guideColorLayer") s.guideColorLayer = settingString(x, k);
        else if (key == "displayLevel") s.displayLevel = settingInt(x, k);
        else if (key == "guidePoints") {
            if (x.is_string()) {
                const std::string m = x.get<std::string>();
                s.guidePoints = m == "off" ? 0 : m == "ends" ? 1 : m == "all" ? 2 : -1;
                if (s.guidePoints < 0) fail("bad_request", "guidePoints is off, ends or all");
            } else {
                s.guidePoints = std::clamp(settingInt(x, k), 0, 2);
            }
        } else if (key == "guideSmoothing") s.guideSmoothing = settingInt(x, k);
        else if (key == "guideDisplaySegments") s.guideDisplaySegments = settingInt(x, k);
        else if (key == "hiddenGuideGroups") {
            if (x.is_array()) {
                std::string names;
                for (const json& n : x) names += (names.empty() ? "" : " ") + settingString(n, k);
                s.hiddenGuideGroups = names;
            } else {
                s.hiddenGuideGroups = settingString(x, k);
            }
        } else if (key == "guideGroupColors") s.guideGroupColors = settingString(x, k);
        else if (key == "theme") {
            if (!x.is_object()) fail("bad_request", "theme is an object");
            DisplayTheme& th = s.theme;
            for (const auto& [tk, tv] : x.items()) {
                const char* n = tk.c_str();
                if (tk == "levels") {
                    if (!tv.is_array() || tv.empty()) fail("bad_request", "theme.levels is a list of [r, g, b]");
                    th.levels.clear();
                    for (const json& c : tv) th.levels.push_back(rgbOf(c, n));
                } else if (tk == "pointSize") th.pointSize = settingNumber(tv, n);
                else if (tk == "selected") th.selected = rgbaOf(tv, n);
                else if (tk == "softSelected") th.softSelected = rgbaOf(tv, n);
                else if (tk == "frozen") th.frozen = rgbOf(tv, n);
                else if (tk == "parts") th.parts = rgbaOf(tv, n);
                else if (tk == "flow") th.flow = rgbaOf(tv, n);
                else if (tk == "flowField") th.flowField = rgbaOf(tv, n);
                else if (tk == "tubes") th.tubes = rgbaOf(tv, n);
                else if (tk == "clumpCenters") th.clumpCenters = rgbOf(tv, n);
                else fail("bad_request", "no theme color '" + tk + "'");
            }
            th.version = hashString(hash64(0x7E3E), x.dump());
        } else fail("bad_request", "no display setting '" + key + "'");
    }
    return s;
}

const char* layerName(DisplayLayer layer) {
    switch (layer) {
        case DisplayLayer::DormantFilled: return "dormantFilled";
        case DisplayLayer::ActiveWire: return "activeWire";
        case DisplayLayer::Selection: return "selection";
        default: return "dormantWire";
    }
}

constexpr std::array<DisplayItemId, 8> kShownItems{DisplayItemId::Guides, DisplayItemId::Tubes,     DisplayItemId::Selected,
                                                   DisplayItemId::SoftSelected, DisplayItemId::Parts, DisplayItemId::Flow,
                                                   DisplayItemId::FlowField, DisplayItemId::ScalpMap};

std::uint64_t haveVersion(const json& have, const std::string& name) {
    if (!have.is_object()) return 0;
    const auto it = have.find(name);
    return it != have.end() && it->is_number_unsigned() ? it->get<std::uint64_t>() : 0;
}

json display(ToolState& t, const json& args) {
    Session& s = t.session;
    GroomSession& session = *s.groom;
    const HostSpace& space = s.space;
    const DisplaySettings settings = displaySettingsOf(t, args.contains("settings") ? args["settings"] : json(nullptr));
    json have = args.contains("have") ? args["have"] : json::object();
    if (args.contains("generation") && args["generation"] != t.generation) have = json::object();  // another builder's versions

    // The view the host last evaluated, with the guides as they are now (a live stroke's preview
    // without an evaluation), the selection and the map shown.
    const ViewResult& last = session.last();
    DisplayInputs in;
    in.scalp = session.scalp();
    in.parts = last.parts;
    in.flow = last.flow;
    in.clumpCenters = last.clumpCenters;  // a viewed Clump's centers: dots
    in.hierarchy = session.hierarchy();
    in.hierarchyVersion = in.hierarchy ? hashCombine(in.hierarchy->contentHash(), reinterpret_cast<std::uintptr_t>(in.hierarchy.get())) : 0;
    in.stroke = session.hasPreview();
    pruneSelection(t);
    if (!t.selection.empty()) {
        const std::vector<GuideId> ids = t.selection;
        const float soft = t.softRadius;
        const HierarchyPtr h = in.hierarchy;
        in.selection = [ids, soft, h] {
            if (soft > 0.0f && h) return softSelection(*h, ids, soft);
            std::map<GuideId, float> weights;
            for (const GuideId id : ids) weights[id] = 1.0f;
            return weights;
        };
        in.selectionHash = hashFloat(hash64(t.selectionVersion), soft);
    }
    if (args.contains("map") && args["map"].is_object()) {
        const json& m = args["map"];
        const std::string name = optString(m, "name", "");
        if (!name.empty()) in.scalpMap = mapView(s, name, optString(m, "channel", "rgb"));
    } else if (args.contains("map") && args["map"].is_string() && !args["map"].get<std::string>().empty()) {
        in.scalpMap = mapView(s, args["map"].get<std::string>(), "rgb");
    }
    const DisplayFrame& frame = t.builder.update(in, settings);

    // What changed against what the host holds: those items' arrays, in one block.
    std::vector<const DisplayItem*> send;
    std::size_t bytes = 0;
    for (const DisplayItemId id : kShownItems) {
        const DisplayItem& item = frame.item(id);
        if (item.geometry && item.colorVersion != haveVersion(have, item.name)) {
            send.push_back(&item);
            bytes += geometryBytes(*item.geometry);
        }
    }
    const std::uint64_t dotsVersion = frame.item(DisplayItemId::Guides).geometryVersion;
    const bool sendDots = dotsVersion != haveVersion(have, "dots");
    if (sendDots) bytes += Arena::reserve(frame.dots.points.size() * 12) * 2;
    t.displayOut.begin(bytes + 64);

    json items = json::object();
    for (const DisplayItemId id : kShownItems) {
        const DisplayItem& item = frame.item(id);
        json j;
        j["version"] = item.colorVersion;
        j["shown"] = item.shown;
        j["vertexCount"] = item.vertexCount;
        j["color"] = item.color;
        j["lineWidth"] = item.lineWidth;
        j["layer"] = layerName(item.layer);
        const DisplayGeometry* g = item.geometry.get();
        j["primitive"] = g && g->triangles ? "triangles" : "lines";
        j["perVertexColors"] = g && !g->colors.empty();
        j["surface"] = g && g->surface;
        const bool changed = std::find(send.begin(), send.end(), &item) != send.end();
        j["changed"] = changed;
        if (changed) j["arrays"] = geometryArrays(t.displayOut, *g, space);
        items[item.name] = j;
    }
    json dots;
    dots["version"] = dotsVersion;
    dots["count"] = frame.dots.points.size();
    dots["pointSize"] = frame.pointSize;
    dots["changed"] = sendDots;
    if (sendDots) {
        json a;
        a["positions"] = putPoints(t.displayOut, frame.dots.points, space, false);
        std::vector<float> colors;
        colors.reserve(frame.dots.colors.size() * 3);
        for (const auto& c : frame.dots.colors) colors.insert(colors.end(), c.begin(), c.end());
        a["colors"] = put(t.displayOut, colors.data(), frame.dots.colors.size(), 3);
        dots["arrays"] = a;
    }
    items["dots"] = dots;

    json r;
    r["generation"] = t.generation;
    r["items"] = items;
    r["overlayPull"] = toHostLength(space, frame.overlayPull);
    r["stroking"] = t.tool.stroking();
    r["selection"] = selectionJson(t);

    // The brush overlay (the ring and what it would touch), when the host holds another.
    json overlay = overlayJson(t, t.overlayVersion != haveVersion(have, "brush"));
    overlay["overlay"]["changed"] = overlay["overlay"].contains("arrays");
    r["brush"] = overlay;

    // The strands' display colors (Maya's strandColorMode) for the last evaluation's strands.
    if (args.contains("strandColors") && args["strandColors"].is_object()) {
        const json& c = args["strandColors"];
        const int mode = colorModeOf(c.contains("mode") ? c["mode"] : json("solid"));
        const std::string source = optString(c, "source", "clumpId");
        const float lo = static_cast<float>(optNumber(c, "min", 0.0)), hi = static_cast<float>(optNumber(c, "max", 1.0));
        const bool perPoint = optBool(c, "perPoint", false);
        const StrandsPtr strands = s.last;
        std::uint64_t key = hashCombine(hash64(reinterpret_cast<std::uintptr_t>(strands.get())), static_cast<std::uint64_t>(mode));
        key = hashString(hashFloat(hashFloat(key, lo), hi), source);
        key = hashCombine(key, perPoint ? 1u : 0u);
        key = hashCombine(key, strands ? strands->strandCount() : 0);
        if (key != t.colorsKey) {
            t.colorsKey = key;
            ++t.colorsVersion;
        }
        const std::string versionName = "strandColors";
        json colors = {{"mode", colorModeName(mode)}, {"version", t.colorsVersion}, {"perPoint", perPoint}};
        const bool want = t.colorsVersion != haveVersion(have, versionName);
        if (mode != kSolidColors && strands && want) {
            const std::vector<std::array<float, 3>> per = strandColors(*strands, last.scalp ? last.scalp.get() : in.scalp.get(), mode, source, lo, hi);
            std::vector<float> flat;
            std::size_t count = 0;
            if (perPoint) {
                count = strands->pointCount();
                flat.reserve(count * 3);
                for (std::size_t i = 0; i < strands->strandCount(); ++i) {
                    for (std::size_t k = 0; k < strands->strandPointCount(i); ++k) flat.insert(flat.end(), per[i].begin(), per[i].end());
                }
            } else {
                count = per.size();
                flat.reserve(count * 3);
                for (const auto& c3 : per) flat.insert(flat.end(), c3.begin(), c3.end());
            }
            t.colorsOut.begin(Arena::reserve(flat.size() * 4));
            colors["colors"] = put(t.colorsOut, flat.data(), count, 3);
            colors["changed"] = true;
        } else {
            colors["changed"] = false;
        }
        colors["strands"] = strands ? strands->strandCount() : 0;
        r["strandColors"] = colors;
    }
    return r;
}

}  // namespace

// ---- The host -------------------------------------------------------------------------------------

std::shared_ptr<TexturePaint> HelperStrokeHost::texturePaint(const std::string& name, int channels, int resolution) {
    const auto entry = s_.groom->scalpMaps().maps.find(name);
    if (entry == s_.groom->scalpMaps().maps.end() || entry->second.kind != "texture" || entry->second.texture.empty()) {
        throw std::runtime_error("this groom has no texture map named " + name);
    }
    const std::string full = resolveMapFile(s_, entry->second.texture);
    const MeshPtr scalp = s_.groom->scalp();
    if (!scalp) throw std::runtime_error("the scalp has no UVs to paint a texture map on");
    requirePaintablePng(full);
    std::shared_ptr<TexturePaint> paint;
    try {
        paint = kept[name].take(*scalp, full, channels, resolution);
    } catch (...) {
        kept.erase(name);
        throw;
    }
    setTexturePaint(name, paint);  // undo and redo write their tiles into it
    return paint;
}

void HelperStrokeHost::commit(StrokeEdit edit) {
    const std::string map = edit.textureMap;
    const bool maps = edit.text(StrokeField::ScalpMaps).set || !map.empty();
    SessionStrokeHost::commit(std::move(edit));
    if (!map.empty()) writeTexture(t_, map);
    t_.redoLabels.clear();
    t_.redoMaps.clear();
    if (maps) s_.mapsOwned = true;
}

// ---- Maps of a file ---------------------------------------------------------------------------------

std::string resolveMapFile(const Session& s, const std::string& stored) { return resolveGroomPath(s.file, stored); }

void adoptMaps(Session& s, const GroomData& data) {
    s.fileCatalog = data.maps;
    s.mapsOwned = false;
    bool any = false;
    for (const auto& [name, info] : data.maps) any = any || info.kind == "vertices" || info.kind == "texture";
    if (!any) return;
    const ScalpMaps maps = scalpMapsFromGroomData(data);
    if (maps.maps.empty()) return;
    s.groom->setScalpMaps(maps);
    s.mapsOwned = true;
}

void writeMaps(const Session& s, GroomFile& file) {
    if (!s.mapsOwned) return;
    const ScalpMaps& maps = s.groom->scalpMaps();
    MapCatalog catalog;
    for (const auto& [name, info] : s.fileCatalog) {
        if (info.kind == "guides") catalog[name] = info;
    }
    for (const auto& [name, info] : maps.catalog()) catalog[name] = info;
    file.data.maps = catalog;
    if (!file.data.scalp) return;
    const ScalpMesh& was = *file.data.scalp;
    auto scalp = std::make_shared<ScalpMesh>(was.points(), was.triangles());
    if (was.hasUVs()) scalp->setCornerUVs(was.cornerUVs());
    if (was.hasRest()) scalp->setRestPoints(was.restPoints());
    scalp->setPolygons(was.polygons());
    scalp->setRenderSubdivision(was.renderSubdivision(), was.renderSubdivisionFrom());
    for (const std::string& set : was.colorSetNames()) {
        const auto it = s.fileCatalog.find(set);
        const bool wasMap = it != s.fileCatalog.end() && it->second.kind == "vertices";
        if (wasMap && !maps.maps.count(set)) continue;
        if (maps.maps.count(set)) continue;  // written from the map below
        if (const auto* colors = was.cornerColors(set)) scalp->setCornerColors(set, *colors);
    }
    for (const auto& [name, entry] : maps.maps) {
        if (entry.kind != "vertices") continue;
        const std::size_t per = entry.channels == kColorMap ? 4 : 1;
        if (entry.values.size() != was.pointCount() * per) continue;
        scalp->setCornerColors(name, cornerColorsFromVertexMap(entry.values, was, static_cast<int>(per)));
    }
    file.data.scalp = scalp;
}

// ---- Commands ---------------------------------------------------------------------------------------

json toolCatalogJson() {
    json tools = json::array();
    for (const BrushEntry& e : brushCatalog()) {
        tools.push_back({{"id", e.id},
                         {"label", e.label},
                         {"category", brushCategoryName(e.category)},
                         {"order", e.order},
                         {"icon", e.icon},
                         {"defaultStrength", e.defaultStrength},
                         {"signed", e.signedStrength},
                         {"hint", e.hint},
                         {"settings", e.settings}});
    }
    json categories = json::array();
    for (const BrushCategory c : brushCategories()) categories.push_back(brushCategoryName(c));
    json settings = json::array();
    for (const BrushSettingSpec& s : brushSettingSpecs()) settings.push_back({{"id", s.id}, {"label", s.label}, {"hint", s.hint}});
    // What New Map > For offers first (stroke.h paintableSettings): the node type and its param.
    json paintable = json::array();
    for (const PaintableSetting& p : paintableSettings()) {
        paintable.push_back({{"id", p.id}, {"label", p.label}, {"type", p.type}, {"param", p.param}, {"hint", p.hint}});
    }
    return {{"tools", tools}, {"categories", categories}, {"settings", settings}, {"paintable", paintable}};
}

void resetTools(Session& s) {
    if (!s.tools) return;
    const BrushToolSettings settings = s.tools->tool.settings;
    const PaintTarget before = s.tools->beforeFreeze;
    const bool select = s.tools->selectTool;
    const float soft = s.tools->softRadius;
    if (s.tools->tool.stroking()) s.tools->tool.cancel(nullptr);
    s.tools.reset();
    ToolState& t = toolsOf(s);
    t.tool.settings = settings;
    t.beforeFreeze = before;
    t.selectTool = select;
    t.softRadius = soft;
}

std::string mapsFolderOf(Session& s) { return mapsFolder(s); }

bool isToolCommand(const std::string& cmd) {
    static const std::set<std::string> kCommands = {
        "tool_set",  "tool_get",      "stroke_begin", "stroke_drag",   "stroke_end",  "stroke_cancel", "hover",    "hover_clear",
        "select",    "selection_get", "selection_set", "pick_level",   "set_hidden_groups", "guides_info", "guides_edit", "maps_list",
        "map_create", "map_remove",   "map_for_param", "undo",        "redo",          "undo_state",  "display"};
    return kCommands.count(cmd) > 0;
}

json toolStateJson(Session& s) {
    ToolState& t = toolsOf(s);
    pruneSelection(t);
    return {{"selection", t.selection}, {"softSelectRadius", toHostLength(s.space, t.softRadius)}, {"hiddenGroups", t.hidden}};
}

void restoreToolState(Session& s, const json& state) {
    ToolState& t = toolsOf(s);
    if (!state.is_object()) return;
    const std::shared_ptr<const GuideHierarchy> h = s.groom->storedHierarchy();
    if (state.contains("selection") && state["selection"].is_array()) {
        std::vector<GuideId> ids = idsOf(state["selection"]);
        ids.erase(std::remove_if(ids.begin(), ids.end(), [&](GuideId id) { return !h || !h->guide(id); }), ids.end());
        setSelection(t, std::move(ids));
    }
    if (state.contains("softSelectRadius") && state["softSelectRadius"].is_number()) {
        t.softRadius = std::max(0.0f, toGroomLength(s.space, state["softSelectRadius"].get<double>()));
    }
    if (state.contains("hiddenGroups") && state["hiddenGroups"].is_array()) {
        t.hidden.clear();
        for (const json& g : state["hiddenGroups"]) {
            if (g.is_string()) t.hidden.insert(g.get<std::string>());
        }
    }
}

bool runToolCommand(Session& s, const std::string& cmd, const json& args, BlockReader&, json& result) {
    if (!isToolCommand(cmd)) return false;
    ToolState& t = toolsOf(s);

    if (cmd == "tool_set") {
        if (args.contains("tool") && !args["tool"].is_null()) {
            if (t.tool.stroking()) t.tool.cancel(&t.host);
            setTool(t, needString(args, "tool"));
        }
        if (args.contains("settings") && !args["settings"].is_null()) applySettings(t, args["settings"]);
        result = toolJson(t);
        return true;
    }
    if (cmd == "tool_get") {
        result = toolJson(t);
        return true;
    }

    if (cmd == "stroke_begin") {
        if (t.selectTool) fail("state", "the Select tool picks guides (select), it does not stroke");
        viewFromArgs(t, args);
        requireScalp(s);
        if (t.tool.stroking()) t.tool.cancel(&t.host);
        const float x = static_cast<float>(optNumber(args, "x", 0.0)), y = static_cast<float>(optNumber(args, "y", 0.0));
        const float pressure = static_cast<float>(optNumber(args, "pressure", 1.0));
        const unsigned modifiers = (optBool(args, "ctrl", false) ? kStrokeCtrl : 0u) | (optBool(args, "shift", false) ? kStrokeShift : 0u);
        prepareHost(t);
        t.host.clearWarnings();
        t.pressX = x;
        t.pressY = y;
        // A press that changed the graph (none does now that map_for_param links a setting's map;
        // the add-on's node tree would follow: graphChanged).
        const std::string graphBefore = s.groom->data().graphJson;
        guarded(t, "press", [&] { t.tool.press(t.host, t.view, x, y, pressure, modifiers); });
        rebuildOverlay(t, x, y, false);
        result = overlayJson(t, optBool(args, "overlay", true));
        result["stroking"] = t.tool.stroking();
        result["mode"] = t.tool.toolModeName();
        result["graphChanged"] = s.groom->data().graphJson != graphBefore;
        result["view"] = viewJson(t);
        result["warnings"] = warningsJson(t);
        return true;
    }
    if (cmd == "stroke_drag") {
        if (!t.tool.stroking()) fail("state", "no stroke: stroke_begin first");
        if (args.contains("view") && !args["view"].is_null()) setView(t, args["view"]);
        const float pressure = static_cast<float>(optNumber(args, "pressure", 1.0));
        std::vector<Pixel> points;
        if (args.contains("points") && !args["points"].is_null()) {
            points = pixelsOf(args["points"], pressure);
        } else {
            points.push_back({static_cast<float>(optNumber(args, "x", 0.0)), static_cast<float>(optNumber(args, "y", 0.0)), pressure});
        }
        for (const Pixel& p : points) guarded(t, "drag", [&] { t.tool.drag(t.host, t.view, p.x, p.y, p.pressure); });
        if (!points.empty()) rebuildOverlay(t, points.back().x, points.back().y, false);
        result = overlayJson(t, optBool(args, "overlay", true));
        result["stroking"] = t.tool.stroking();
        result["changed"] = t.tool.changed();
        result["warnings"] = warningsJson(t);
        return true;
    }
    if (cmd == "stroke_end") {
        const std::size_t before = t.host.edits().size();
        StrokeRelease released = StrokeRelease::None;
        if (t.hasView) guarded(t, "release", [&] { released = t.tool.release(t.host, t.view); });
        result["released"] = released == StrokeRelease::Stroke ? "stroke" : released == StrokeRelease::ShiftClick ? "shift_click" : "none";
        result["committed"] = t.host.edits().size() != before;
        if (released == StrokeRelease::ShiftClick) {
            int level = -2;
            guarded(t, "pick", [&] { level = t.tool.pickLevel(t.host, t.view, t.pressX, t.pressY); });
            result["level"] = level;
            result["group"] = t.tool.settings.brush.group;
            result["settings"] = settingsJson(t);
        }
        if (result["committed"].get<bool>()) {
            t.redoLabels.clear();
            t.redoMaps.clear();
        }
        pruneSelection(t);
        result["undo"] = undoJson(t);
        result["warnings"] = warningsJson(t);
        return true;
    }
    if (cmd == "stroke_cancel") {
        const bool was = t.tool.stroking();
        if (was) t.tool.cancel(&t.host);
        result = {{"cancelled", was}};
        return true;
    }
    if (cmd == "hover") {
        viewFromArgs(t, args);
        const float x = static_cast<float>(optNumber(args, "x", 0.0)), y = static_cast<float>(optNumber(args, "y", 0.0));
        rebuildOverlay(t, x, y, optBool(args, "lines", true) && !t.tool.stroking());
        result = overlayJson(t, optBool(args, "overlay", true));
        result["view"] = viewJson(t);
        return true;
    }
    if (cmd == "hover_clear") {
        clearOverlay(t);
        result = {{"overlay", {{"version", t.overlayVersion}}}};
        return true;
    }
    if (cmd == "select") {
        viewFromArgs(t, args);
        const SelectMode mode = [&] {
            try {
                return selectModeFromName(optString(args, "mode", "replace"));
            } catch (const std::invalid_argument& e) {
                fail("bad_request", e.what());
            }
        }();
        const HierarchyPtr h = s.groom->hierarchy();
        std::vector<GuideId> picked;
        if (h) {
            const BrushToolSettings& st = t.tool.settings;
            const std::vector<GuideId> candidates = guidesAt(*h, st.brush.level, st.brush.group, t.hidden);
            const std::vector<Pixel> lasso = pixelsOf(need(args, "lasso"), 1.0f);
            if (lasso.empty()) fail("bad_request", "lasso holds a point (a click), two (a box) or more");
            std::vector<std::array<float, 2>> polygon;
            if (lasso.size() == 2) {
                const Pixel& a = lasso[0];
                const Pixel& b = lasso[1];
                polygon = {{a.x, a.y}, {b.x, a.y}, {b.x, b.y}, {a.x, b.y}};
            } else {
                for (const Pixel& p : lasso) polygon.push_back({p.x, p.y});
            }
            picked = pickGuides(*h, candidates, pixelProjector(t.view.projection), polygon, optBool(args, "rootsOnly", false),
                                static_cast<float>(optNumber(args, "clickPixels", 12.0)), &t.view.projection);
            // With symmetry (the tool's), each picked guide's mirror guide too.
            const MeshPtr scalp = s.groom->scalp();
            picked = withMirrorGuides(*h, picked, StrokeSymmetry::of(st.brush, scalp.get()));
        }
        pruneSelection(t);
        const std::vector<GuideId> selection = combineSelection(t.selection, picked, mode);
        setSelection(t, selection);
        // "solo" (the add-on's Select tool): every guide not selected frozen, one undo step when
        // that changed anything; a plain pick of nothing unfreezes all.
        bool froze = false;
        if (optBool(args, "solo", false) && (!picked.empty() || mode == SelectMode::Replace)) {
            const std::shared_ptr<const GuideHierarchy> stored = s.groom->authoredHierarchy();
            GuideHierarchy probe = stored ? *stored : GuideHierarchy{};
            if (freezeToSelection(probe, selection)) {
                editGuides(t, "solo selection", [&](GuideHierarchy& g) { freezeToSelection(g, selection); });
                froze = true;
            }
        }
        result = selectionJson(t);
        result["picked"] = picked;
        result["froze"] = froze;
        return true;
    }
    if (cmd == "selection_get") {
        pruneSelection(t);
        result = selectionJson(t);
        return true;
    }
    if (cmd == "selection_set") {
        std::vector<GuideId> ids;
        const std::shared_ptr<const GuideHierarchy> h = s.groom->storedHierarchy();
        if (args.contains("ids") && !args["ids"].is_null()) {
            ids = idsOf(args["ids"]);
        } else if (args.contains("group") && !args["group"].is_null()) {
            if (!h || h->findGroup(needString(args, "group")) == kNoGroup) fail("not_found", "no group named '" + needString(args, "group") + "'");
            ids = guidesAt(*h, -1, needString(args, "group"), t.hidden);
        } else if (args.contains("level") && !args["level"].is_null()) {
            if (h) ids = guidesAt(*h, static_cast<int>(optNumber(args, "level", 0.0)), {}, t.hidden);
        } else if (optBool(args, "all", false)) {
            if (h) ids = guidesAt(*h, -1, {}, t.hidden);
        }
        if (args.contains("softSelectRadius") && !args["softSelectRadius"].is_null()) {
            t.softRadius = std::max(0.0f, toGroomLength(s.space, optNumber(args, "softSelectRadius", 0.0)));
            ++t.selectionVersion;
        }
        SelectMode mode = SelectMode::Replace;
        try {
            mode = selectModeFromName(optString(args, "mode", "replace"));
        } catch (const std::invalid_argument& e) {
            fail("bad_request", e.what());
        }
        if (h) {
            ids.erase(std::remove_if(ids.begin(), ids.end(), [&](GuideId id) { return !h->guide(id); }), ids.end());
        } else {
            ids.clear();
        }
        setSelection(t, combineSelection(t.selection, ids, mode));
        result = selectionJson(t);
        return true;
    }
    if (cmd == "pick_level") {
        viewFromArgs(t, args);
        prepareHost(t);
        int level = -2;
        guarded(t, "pick", [&] {
            level = t.tool.pickLevel(t.host, t.view, static_cast<float>(optNumber(args, "x", 0.0)), static_cast<float>(optNumber(args, "y", 0.0)));
        });
        result = {{"level", level}, {"group", t.tool.settings.brush.group}, {"settings", settingsJson(t)}};
        return true;
    }
    if (cmd == "set_hidden_groups") {
        const json& groups = need(args, "groups");
        if (!groups.is_array()) fail("bad_request", "groups is a list of group names");
        std::set<std::string> hidden;
        for (const json& g : groups) hidden.insert(settingString(g, "groups"));
        t.hidden = std::move(hidden);
        result = {{"hiddenGroups", t.hidden}};
        return true;
    }
    if (cmd == "guides_info") {
        result = guidesInfo(t);
        return true;
    }
    if (cmd == "guides_edit") {
        result = guidesEdit(t, args);
        return true;
    }
    if (cmd == "maps_list") {
        result = mapsList(t);
        return true;
    }
    if (cmd == "map_create") {
        result = mapCreate(t, args);
        return true;
    }
    if (cmd == "map_remove") {
        result = mapRemove(t, args);
        return true;
    }
    if (cmd == "map_for_param") {
        result = mapForParam(t, args);
        return true;
    }
    if (cmd == "undo" || cmd == "redo") {
        if (t.tool.stroking()) t.tool.cancel(&t.host);
        const std::string graphBefore = s.groom->data().graphJson;
        const bool done = cmd == "undo" ? undo(t) : redo(t);
        result = undoJson(t);
        result["done"] = done;
        result["graphChanged"] = s.groom->data().graphJson != graphBefore;  // a setting's map linked or unlinked (map_for_param)
        return true;
    }
    if (cmd == "undo_state") {
        result = undoJson(t);
        return true;
    }
    if (cmd == "display") {
        result = display(t, args);
        return true;
    }
    return false;
}

}  // namespace groomlab::helper
