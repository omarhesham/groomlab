// SPDX-License-Identifier: Apache-2.0
#pragma once

// Shared helpers of the research prototypes (tools/research/*): realistic scalps, strands and
// guides built through the core's own operators, timers and small statistics. Research code: it
// measures, it does not ship.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#include "groomlab/graph.h"
#include "groomlab/graph_io.h"
#include "groomlab/groom.h"
#include "groomlab/guide_set.h"
#include "groomlab/math.h"
#include "groomlab/operators.h"
#include "groomlab/parallel.h"
#include "groomlab/random.h"
#include "groomlab/scalp_mesh.h"
#include "groomlab/strand_set.h"

namespace research {

using namespace groomlab;

inline double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
}

template <class F>
double bestMs(int repeats, F&& f) {
    double best = 1e30;
    for (int r = 0; r < repeats; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        f();
        best = std::min(best, msSince(t0));
    }
    return best;
}

// The app's head scalp (tests/data/app/head_scalp.obj: v and f lines, f v/vt).
inline std::shared_ptr<const ScalpMesh> loadObj(const std::string& path) {
    std::ifstream in(path);
    std::vector<Vec3f> points;
    std::vector<ScalpMesh::Triangle> triangles;
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("v ", 0) == 0) {
            Vec3f p;
            std::sscanf(line.c_str() + 2, "%f %f %f", &p.x, &p.y, &p.z);
            points.push_back(p);
        } else if (line.rfind("f ", 0) == 0) {
            int a = 0, b = 0, c = 0, ta = 0, tb = 0, tc = 0;
            if (std::sscanf(line.c_str() + 2, "%d/%d %d/%d %d/%d", &a, &ta, &b, &tb, &c, &tc) < 6) {
                std::sscanf(line.c_str() + 2, "%d %d %d", &a, &b, &c);
            }
            triangles.push_back({a - 1, b - 1, c - 1});
        }
    }
    return std::make_shared<const ScalpMesh>(std::move(points), std::move(triangles));
}

// Midpoint (4-to-1) subdivision, `levels` times: the same surface, denser vertices (the "50k-vertex
// scalp" of a production head without changing its shape).
inline std::shared_ptr<const ScalpMesh> subdivideMidpoint(const ScalpMesh& mesh, int levels) {
    std::vector<Vec3f> pts = mesh.points();
    std::vector<ScalpMesh::Triangle> tris = mesh.triangles();
    for (int l = 0; l < levels; ++l) {
        std::unordered_map<std::uint64_t, std::int32_t> mid;
        auto edge = [&](std::int32_t a, std::int32_t b) {
            const std::uint64_t key = (std::uint64_t(std::min(a, b)) << 32) | std::uint32_t(std::max(a, b));
            auto it = mid.find(key);
            if (it != mid.end()) return it->second;
            const auto id = static_cast<std::int32_t>(pts.size());
            pts.push_back((pts[a] + pts[b]) * 0.5f);
            mid.emplace(key, id);
            return id;
        };
        std::vector<ScalpMesh::Triangle> next;
        next.reserve(tris.size() * 4);
        for (const auto& t : tris) {
            const std::int32_t ab = edge(t[0], t[1]), bc = edge(t[1], t[2]), ca = edge(t[2], t[0]);
            next.push_back({t[0], ab, ca});
            next.push_back({ab, t[1], bc});
            next.push_back({ca, bc, t[2]});
            next.push_back({ab, bc, ca});
        }
        tris = std::move(next);
    }
    return std::make_shared<const ScalpMesh>(std::move(pts), std::move(tris));
}

inline Vec3f boxSize(const ScalpMesh& m) {
    Vec3f lo = m.points().front(), hi = lo;
    for (const Vec3f& p : m.points()) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    return hi - lo;
}

// Strands scattered on `scalp` by the core's Scatter (blue noise with randomness 0), resampled to
// `segments` segments: what an Interpolate node is handed in a groom.
inline StrandsPtr scatter(const MeshPtr& scalp, double count, int segments, double length, double randomness = 1.0,
                          std::uint64_t seed = 1) {
    const std::string graph = R"({"version": 1, "output": "out", "nodes": [
        {"name": "scalp", "type": "Scalp"},
        {"name": "scatter", "type": "Scatter", "params": {"density": )" +
                              std::to_string(count / scalp->totalArea()) + R"(, "length": )" + std::to_string(length) +
                              R"(, "randomness": )" + std::to_string(randomness) + R"(, "seed": )" + std::to_string(seed) +
                              R"(}},
        {"name": "out", "type": "Resample", "params": {"segments": )" +
                              std::to_string(segments) + R"(}}],
      "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"},
                      {"from": "scatter", "to": "out", "input": "strands"}]})";
    GraphDocument doc = loadGraphJson(graph);
    ExternalInputs ext;
    setGroomExternals(ext, scalp, nullptr, nullptr);
    EvalContext ctx;
    ctx.externals = &ext;
    return doc.graph.evaluateStrands(doc.output, ctx);
}

// A hairstyle's guides: `count` roots spread evenly (Scatter, blue noise), each guide leaving the
// skin along its normal leaned back and down (combed), with a per-guide jitter of its direction and
// length (neighbouring guides disagree as a groomer's do), bent by gravity along its length.
// `scale`: the head's size (its box's largest side). `points`: points per guide.
inline GuidesPtr hairstyleGuides(const MeshPtr& scalp, int count, float scale, int points = 8, std::uint64_t seed = 7,
                                 float jitter = 0.35f) {
    const StrandsPtr roots = scatter(scalp, count, 1, 0.01, 0.0, seed);
    auto guides = std::make_shared<GuideSet>();
    Rng rng(seed * 7919 + 1);
    const Vec3f down(0.0f, -1.0f, -0.6f);
    for (std::size_t s = 0; s < roots->strandCount(); ++s) {
        const StrandRoot r = roots->roots()[s];
        const Vec3f p = scalp->position(r), n = scalp->normal(r);
        Vec3f comb = down - n * down.dot(n);
        if (comb.length() < 1e-4f) comb = Vec3f(1, 0, 0) - n * n.x;
        comb = comb.normalized();
        Vec3f side = n.cross(comb).normalized();
        const float a = (rng.nextFloat() * 2.0f - 1.0f) * jitter, b = (rng.nextFloat() * 2.0f - 1.0f) * jitter;
        Vec3f dir = (n * 0.6f + comb * 0.8f + side * a + n * b).normalized();
        const float length = scale * 0.45f * (0.8f + 0.4f * rng.nextFloat());
        const float seg = length / static_cast<float>(points - 1);
        std::vector<Vec3f> pts{p};
        Vec3f d = dir;
        const float sag = 0.10f + 0.10f * rng.nextFloat();
        for (int k = 1; k < points; ++k) {
            d = (d + Vec3f(0, -sag, 0)).normalized();  // gravity bends it more along its length
            pts.push_back(pts.back() + d * seg);
        }
        Guide& g = guides->add(guides->size() + 1, std::move(pts));
        g.root = r;
    }
    return guides;
}

// Running statistics of a sample of errors.
struct Stats {
    std::vector<double> v;
    void add(double x) { v.push_back(x); }
    double mean() const { return v.empty() ? 0.0 : std::accumulate(v.begin(), v.end(), 0.0) / double(v.size()); }
    double quantile(double q) {
        if (v.empty()) return 0.0;
        std::vector<double> w = v;
        const std::size_t k = std::min(w.size() - 1, static_cast<std::size_t>(q * double(w.size() - 1)));
        std::nth_element(w.begin(), w.begin() + static_cast<std::ptrdiff_t>(k), w.end());
        return w[k];
    }
    double max() const { return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end()); }
};

}  // namespace research
