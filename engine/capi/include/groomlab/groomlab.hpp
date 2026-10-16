// SPDX-License-Identifier: Apache-2.0
// GroomLab C++ wrapper: header-only, C++17, over the C API (groomlab_c.h). RAII handles, views
// into results (no copy) and std::vector copies, exceptions (groomlab::api::Error) from error
// codes. Only the C API crosses the library boundary, so this header builds with any compiler and
// standard library the host uses.
//
//   groomlab::api::Session session = groomlab::api::Session::fromFile("head.groom");
//   session.setFloat("scatter", "density", 120.0);
//   session.setFrame(12);
//   groomlab::api::Strands strands = session.evaluate();
//   for (std::size_t s = 0; s < strands.strandCount(); ++s) { ... strands.positions() ... }

#ifndef GROOMLAB_HPP
#define GROOMLAB_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "groomlab/groomlab_c.h"

namespace groomlab {
namespace api {

// A failing call: its gl_result and gl_last_error()'s message.
class Error : public std::runtime_error {
public:
    Error(gl_result code, const std::string& message)
        : std::runtime_error(std::string(gl_result_name(code)) + ": " + message), code_(code), message_(message) {}
    gl_result code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

private:
    gl_result code_;
    std::string message_;
};

namespace detail {

inline void check(gl_result result) {
    if (result != GL_OK) throw Error(result, gl_last_error());
}

// Takes a library-owned buffer: copied, then released.
inline std::string take(char* text, std::size_t size) {
    std::string out(text ? text : "", text ? size : 0);
    gl_free(text);
    return out;
}
inline std::string take(char* text) { return take(text, text ? std::char_traits<char>::length(text) : 0); }

}  // namespace detail

// The library is one this header can use (the same major version, at least this minor).
inline bool compatible() noexcept { return GROOMLAB_C_API_COMPATIBLE(gl_api_version()); }
inline void requireCompatible() {
    if (!compatible()) {
        const std::uint32_t v = gl_api_version();
        throw Error(GL_ERROR_STATE, "the GroomLab library's API is " + std::to_string(v >> 16) + "." +
                                        std::to_string(v & 0xFFFFu) + "; this header needs " +
                                        std::to_string(GROOMLAB_C_API_VERSION_MAJOR) + "." +
                                        std::to_string(GROOMLAB_C_API_VERSION_MINOR) + " or a later minor");
    }
}
inline std::string versionString() { return gl_version_string(); }
// Frame `frame` at `fps` in seconds, by the rule every host keeps (gl_frame_to_seconds).
inline double frameToSeconds(double frame, double fps) noexcept { return gl_frame_to_seconds(frame, fps); }

inline std::string operatorTypesJson() {
    char* out = nullptr;
    detail::check(gl_operator_types_json(&out));
    return detail::take(out);
}
inline std::string operatorSpecsJson() {
    char* out = nullptr;
    detail::check(gl_operator_specs_json(&out));
    return detail::take(out);
}
inline std::string operatorSpecJson(const std::string& type) {
    char* out = nullptr;
    detail::check(gl_operator_spec_json(type.c_str(), &out));
    return detail::take(out);
}
inline std::string defaultGraphJson() {
    char* out = nullptr;
    detail::check(gl_default_graph_json(&out));
    return detail::take(out);
}

// A read-only view of contiguous values (std::span-like), valid while what it views lives.
template <class T>
class View {
public:
    View() = default;
    View(const T* data, std::size_t size) : data_(size ? data : nullptr), size_(data ? size : 0) {}
    const T* data() const noexcept { return data_; }
    std::size_t size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }
    const T* begin() const noexcept { return data_; }
    const T* end() const noexcept { return data_ + size_; }
    const T& operator[](std::size_t i) const { return data_[i]; }
    std::vector<T> toVector() const { return std::vector<T>(begin(), end()); }

private:
    const T* data_ = nullptr;
    std::size_t size_ = 0;
};

enum class Scope : std::int32_t { Strand = GL_SCOPE_STRAND, Point = GL_SCOPE_POINT };

struct Attribute {
    std::string name;
    Scope scope = Scope::Strand;
    int components = 1;
    View<float> values;  // components per strand or point
};

// An evaluation's result. Move-only; independent of the session that made it.
class Strands {
public:
    Strands() = default;
    explicit Strands(gl_strands* handle) noexcept : h_(handle) {}
    ~Strands() { gl_strands_release(h_); }
    Strands(Strands&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    Strands& operator=(Strands&& o) noexcept {
        if (this != &o) {
            gl_strands_release(h_);
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
    Strands(const Strands&) = delete;
    Strands& operator=(const Strands&) = delete;

    gl_strands* handle() const noexcept { return h_; }
    explicit operator bool() const noexcept { return h_ != nullptr; }

    std::size_t strandCount() const noexcept { return gl_strands_strand_count(h_); }
    std::size_t pointCount() const noexcept { return gl_strands_point_count(h_); }
    View<float> positions() const { return {gl_strands_positions(h_), 3 * pointCount()}; }  // x y z per point
    View<float> widths() const { return {gl_strands_widths(h_), pointCount()}; }
    View<std::uint64_t> ids() const { return {gl_strands_ids(h_), strandCount()}; }
    View<std::int32_t> pointCounts() const { return {gl_strands_point_counts(h_), strandCount()}; }
    View<std::uint64_t> offsets() const { return {gl_strands_offsets(h_), h_ ? strandCount() + 1 : 0}; }
    // One strand's points (x y z each).
    View<float> strandPositions(std::size_t strand) const {
        const View<std::uint64_t> o = offsets();
        return {gl_strands_positions(h_) + 3 * o[strand], 3 * static_cast<std::size_t>(o[strand + 1] - o[strand])};
    }

    std::vector<std::string> attributeNames() const {
        std::vector<std::string> names;
        const std::size_t n = gl_strands_attribute_count(h_);
        names.reserve(n);
        for (std::size_t i = 0; i < n; ++i) names.emplace_back(gl_strands_attribute_name(h_, i));
        return names;
    }
    bool hasAttribute(const std::string& name) const {
        const std::size_t n = gl_strands_attribute_count(h_);
        for (std::size_t i = 0; i < n; ++i) {
            if (name == gl_strands_attribute_name(h_, i)) return true;
        }
        return false;
    }
    // By name: a name there in both scopes gives the per strand one.
    Attribute attribute(const std::string& name) const {
        const float* data = nullptr;
        std::int32_t scope = 0, components = 0;
        detail::check(gl_strands_attribute(h_, name.c_str(), &data, &scope, &components));
        const std::size_t elements = scope == GL_SCOPE_POINT ? pointCount() : strandCount();
        return {name, static_cast<Scope>(scope), components, View<float>(data, elements * static_cast<std::size_t>(components))};
    }
    // By name and scope.
    Attribute attribute(const std::string& name, Scope scope) const {
        const float* data = nullptr;
        std::int32_t components = 0;
        detail::check(gl_strands_attribute_scoped(h_, name.c_str(), static_cast<std::int32_t>(scope), &data, &components));
        const std::size_t elements = scope == Scope::Point ? pointCount() : strandCount();
        return {name, scope, components, View<float>(data, elements * static_cast<std::size_t>(components))};
    }
    // Every attribute, in the order they are listed (a name in both scopes twice).
    std::vector<Attribute> attributes() const {
        std::vector<Attribute> out;
        const std::size_t n = gl_strands_attribute_count(h_);
        out.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            std::int32_t scope = 0;
            detail::check(gl_strands_attribute_info(h_, i, &scope, nullptr));
            out.push_back(attribute(gl_strands_attribute_name(h_, i), static_cast<Scope>(scope)));
        }
        return out;
    }

private:
    gl_strands* h_ = nullptr;
};

struct EvalOptions {
    float density = 1.0f;       // 0-1
    double pointBudget = 0.0;   // 0: no limit
    std::string viewNode;       // "": the output
};

// A groom kept evaluated. Move-only.
class Session {
public:
    Session() {
        requireCompatible();
        detail::check(gl_session_create(&h_));
    }
    ~Session() { gl_session_destroy(h_); }
    Session(Session&& o) noexcept : h_(std::exchange(o.h_, nullptr)) {}
    Session& operator=(Session&& o) noexcept {
        if (this != &o) {
            gl_session_destroy(h_);
            h_ = std::exchange(o.h_, nullptr);
        }
        return *this;
    }
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;

    gl_session* handle() const noexcept { return h_; }

    static Session fromFile(const std::string& path) {
        Session s;
        s.loadFile(path);
        return s;
    }
    static Session fromMemory(const std::string& text, const std::string& baseDir = {}) {
        Session s;
        s.loadMemory(text, baseDir);
        return s;
    }

    // ---- Files
    void loadFile(const std::string& path) { detail::check(gl_session_load_file(h_, path.c_str())); }
    void loadMemory(const std::string& text, const std::string& baseDir = {}) {
        detail::check(gl_session_load_memory(h_, text.data(), text.size(), baseDir.empty() ? nullptr : baseDir.c_str()));
    }
    void saveFile(const std::string& path) { detail::check(gl_session_save_file(h_, path.c_str())); }
    std::string saveMemory() {
        char* data = nullptr;
        std::size_t size = 0;
        detail::check(gl_session_save_memory(h_, &data, &size));
        return detail::take(data, size);
    }

    // ---- Scalp
    // `points`: x y z per point; polygons of `faceCounts[i]` corners, their indices in `faceIndices`;
    // `uvs` (empty: none): u v per polygon corner.
    void setScalp(const std::vector<float>& points, const std::vector<std::int32_t>& faceCounts,
                  const std::vector<std::int32_t>& faceIndices, const std::vector<float>& uvs = {}) {
        if (points.size() % 3 != 0) throw Error(GL_ERROR_INVALID_ARGUMENT, "points must hold 3 floats per point");
        if (!uvs.empty() && uvs.size() != 2 * faceIndices.size()) {
            throw Error(GL_ERROR_INVALID_ARGUMENT, "uvs must hold 2 floats per polygon corner");
        }
        detail::check(gl_session_set_scalp(h_, points.data(), points.size() / 3, faceCounts.data(), faceCounts.size(),
                                           faceIndices.data(), faceIndices.size(), uvs.empty() ? nullptr : uvs.data()));
    }
    void updateScalpPoints(const std::vector<float>& points, const std::vector<float>* restPoints = nullptr) {
        if (points.size() % 3 != 0) throw Error(GL_ERROR_INVALID_ARGUMENT, "points must hold 3 floats per point");
        if (restPoints && restPoints->size() != points.size()) {
            throw Error(GL_ERROR_INVALID_ARGUMENT, "rest points must be as many as the points");
        }
        detail::check(gl_session_update_scalp_points(h_, points.data(), points.size() / 3,
                                                     restPoints ? restPoints->data() : nullptr));
    }
    std::size_t scalpPointCount() const {
        std::size_t points = 0;
        detail::check(gl_session_scalp_size(h_, &points, nullptr));
        return points;
    }
    std::size_t scalpTriangleCount() const {
        std::size_t triangles = 0;
        detail::check(gl_session_scalp_size(h_, nullptr, &triangles));
        return triangles;
    }

    // ---- Time and inputs
    void setTime(double seconds) { detail::check(gl_session_set_time(h_, seconds)); }
    double time() const {
        double t = 0.0;
        detail::check(gl_session_get_time(h_, &t));
        return t;
    }
    void setFrame(double frame) { detail::check(gl_session_set_frame(h_, frame)); }
    // "Y" or "Z"; setting turns the groom (scalp, guides, colliders, world-direction params).
    std::string upAxis() const {
        char* out = nullptr;
        detail::check(gl_session_get_up_axis(h_, &out));
        return detail::take(out);
    }
    void setUpAxis(const std::string& axis) { detail::check(gl_session_set_up_axis(h_, axis.c_str())); }
    void setFramesPerSecond(double fps) { detail::check(gl_session_set_frames_per_second(h_, fps)); }
    double framesPerSecond() const {
        double fps = 0.0;
        detail::check(gl_session_get_frames_per_second(h_, &fps));
        return fps;
    }
    void setControl(const std::string& name, float value) { detail::check(gl_session_set_control(h_, name.c_str(), value)); }
    void setSearchPaths(const std::vector<std::string>& directories) {
        std::vector<const char*> dirs;
        dirs.reserve(directories.size());
        for (const std::string& d : directories) dirs.push_back(d.c_str());
        detail::check(gl_session_set_search_paths(h_, dirs.data(), dirs.size()));
    }

    // ---- Evaluation
    Strands evaluate(const EvalOptions& options = {}) {
        gl_eval_options o;
        gl_eval_options_init(&o);
        o.density = options.density;
        o.point_budget = options.pointBudget;
        o.view_node = options.viewNode.empty() ? nullptr : options.viewNode.c_str();
        gl_strands* out = nullptr;
        detail::check(gl_session_evaluate(h_, &o, &out));
        return Strands(out);
    }
    std::string evalErrorsJson() const {
        char* out = nullptr;
        detail::check(gl_session_eval_errors_json(h_, &out));
        return detail::take(out);
    }

    // ---- Graph
    std::string graphJson() const {
        char* out = nullptr;
        detail::check(gl_session_get_graph_json(h_, &out));
        return detail::take(out);
    }
    void setGraphJson(const std::string& json) { detail::check(gl_session_set_graph_json(h_, json.c_str())); }
    std::vector<std::string> nodeNames() const {
        std::size_t count = 0;
        detail::check(gl_session_node_count(h_, &count));
        std::vector<std::string> names;
        names.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            char* out = nullptr;
            detail::check(gl_session_node_name(h_, i, &out));
            names.push_back(detail::take(out));
        }
        return names;
    }
    std::string nodeType(const std::string& node) const {
        char* out = nullptr;
        detail::check(gl_session_node_type(h_, node.c_str(), &out));
        return detail::take(out);
    }
    std::string nodeSpecJson(const std::string& node) const {
        char* out = nullptr;
        detail::check(gl_session_node_spec_json(h_, node.c_str(), &out));
        return detail::take(out);
    }
    // Raises the node to its operator's newest behaviour (gl_session_upgrade_node): true when it
    // was raised, false when it had the newest already.
    bool upgradeNode(const std::string& node) {
        std::int32_t upgraded = 0;
        detail::check(gl_session_upgrade_node(h_, node.c_str(), &upgraded));
        return upgraded != 0;
    }

    std::string paramJson(const std::string& node, const std::string& param) const {
        char* out = nullptr;
        detail::check(gl_session_get_param_json(h_, node.c_str(), param.c_str(), &out));
        return detail::take(out);
    }
    void setParamJson(const std::string& node, const std::string& param, const std::string& valueJson) {
        detail::check(gl_session_set_param_json(h_, node.c_str(), param.c_str(), valueJson.c_str()));
    }
    double getFloat(const std::string& node, const std::string& param) const {
        double v = 0.0;
        detail::check(gl_session_get_param_float(h_, node.c_str(), param.c_str(), &v));
        return v;
    }
    void setFloat(const std::string& node, const std::string& param, double value) {
        detail::check(gl_session_set_param_float(h_, node.c_str(), param.c_str(), value));
    }
    std::int64_t getInt(const std::string& node, const std::string& param) const {
        std::int64_t v = 0;
        detail::check(gl_session_get_param_int(h_, node.c_str(), param.c_str(), &v));
        return v;
    }
    void setInt(const std::string& node, const std::string& param, std::int64_t value) {
        detail::check(gl_session_set_param_int(h_, node.c_str(), param.c_str(), value));
    }
    bool getBool(const std::string& node, const std::string& param) const {
        std::int32_t v = 0;
        detail::check(gl_session_get_param_bool(h_, node.c_str(), param.c_str(), &v));
        return v != 0;
    }
    void setBool(const std::string& node, const std::string& param, bool value) {
        detail::check(gl_session_set_param_bool(h_, node.c_str(), param.c_str(), value ? 1 : 0));
    }
    std::array<double, 3> getVec3(const std::string& node, const std::string& param) const {
        std::array<double, 3> v{};
        detail::check(gl_session_get_param_vec3(h_, node.c_str(), param.c_str(), v.data()));
        return v;
    }
    void setVec3(const std::string& node, const std::string& param, const std::array<double, 3>& value) {
        detail::check(gl_session_set_param_vec3(h_, node.c_str(), param.c_str(), value.data()));
    }
    std::string getString(const std::string& node, const std::string& param) const {
        char* out = nullptr;
        detail::check(gl_session_get_param_string(h_, node.c_str(), param.c_str(), &out));
        return detail::take(out);
    }
    void setString(const std::string& node, const std::string& param, const std::string& value) {
        detail::check(gl_session_set_param_string(h_, node.c_str(), param.c_str(), value.c_str()));
    }

private:
    gl_session* h_ = nullptr;
};

}  // namespace api
}  // namespace groomlab

#endif  // GROOMLAB_HPP
