// SPDX-License-Identifier: Apache-2.0
#include "groomlab/strand_geometry.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>

#include "groomlab/frames.h"
#include "groomlab/graph.h"
#include "groomlab/parallel.h"
#include "groomlab/portable_math.h"
#include "groomlab/random.h"
#include "groomlab/scalp_mesh.h"

namespace groomlab {

std::vector<std::size_t> thinStrandsById(const StrandSet& strands, std::size_t maxCount) {
    const std::size_t n = strands.strandCount();
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), std::size_t{0});
    if (n <= maxCount) return order;
    const std::vector<StrandId>& ids = strands.ids();
    // Lowest hashed IDs first (ties: the earlier strand), as keepAtDensity keeps them; each
    // ID hashed once, not at every comparison.
    std::vector<std::uint64_t> keys(n);
    for (std::size_t i = 0; i < n; ++i) keys[i] = hash64(ids[i]);
    auto before = [&](std::size_t a, std::size_t b) { return keys[a] != keys[b] ? keys[a] < keys[b] : a < b; };
    std::nth_element(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(maxCount), order.end(), before);
    order.resize(maxCount);
    std::sort(order.begin(), order.end());
    return order;
}

namespace {

constexpr float kTwoPi = 6.28318531f;  // feathers.cpp's: the points come out as buildRachisMesh makes them

// Strand k's rings (one per point; two points or more) into `out`: rotation-minimizing frames from
// the scalp's normal at its root (`scalp`: the strands are rooted on it; null: any direction across
// the root), the widths times widthScale, u by arc length.
void strandRings(const StrandSet& strands, std::size_t k, const ScalpMesh* scalp, const StrandMeshOptions& options,
                 std::vector<Frame>& frames, std::vector<float>& arc, RachisRing* out) {
    const std::size_t count = strands.strandPointCount(k);
    const Vec3f* p = strands.strandPoints(k);
    const std::size_t begin = strands.strandBegin(k);
    const std::vector<float>& widths = strands.widths();
    Vec3f up = anyPerpendicular((p[1] - p[0]).normalized());
    if (scalp) {
        const StrandRoot& root = strands.roots()[k];
        if (root.face >= 0 && static_cast<std::size_t>(root.face) < scalp->triangleCount()) {
            const Vec3f normal = scalp->normal(root);
            if (normal.length() > 0.5f) up = normal;
        }
    }
    rotationMinimizingFramesInto(p, count, up, frames);
    arc.assign(count, 0.0f);
    for (std::size_t i = 1; i < count; ++i) arc[i] = arc[i - 1] + (p[i] - p[i - 1]).length();
    const float total = arc.back() > 0.0f ? arc.back() : 1.0f;
    for (std::size_t i = 0; i < count; ++i) {
        RachisRing& ring = out[i];
        ring.center = p[i];
        ring.tangent = frames[i].tangent;
        ring.normal = frames[i].normal;
        ring.side = frames[i].binormal;
        const float w = begin + i < widths.size() ? widths[begin + i] : 1.0f;
        ring.radius = 0.5f * std::max(w * options.widthScale, options.minWidth);
        ring.u = arc[i] / total;
    }
}

int meshSides(const StrandMeshOptions& options) {
    return options.style == StrandMeshOptions::Style::Ribbon ? 2 : std::max(3, options.sides);
}

// The vertices a strand of `count` points makes: a ring at each point, and a tube's tip (none for
// fewer than two points).
std::size_t strandVertexCount(std::size_t count, int sides) {
    if (count < 2) return 0;
    return count * static_cast<std::size_t>(sides) + (sides == 2 ? 0 : 1);
}

const ScalpMesh* rootingScalp(const StrandSet& strands, const ScalpMesh* scalp) {
    return scalp && scalp->triangleCount() > 0 && strands.roots().size() == strands.strandCount() ? scalp : nullptr;
}

}  // namespace

StrandMesh buildStrandMesh(const StrandSet& strands, const ScalpMesh* scalp, const StrandMeshOptions& options) {
    auto layer = std::make_shared<FeatherLayer>();
    layer->sides = meshSides(options);
    const std::size_t n = strands.strandCount();
    layer->offsets.reserve(n + 1);
    layer->featherIds.reserve(n);
    layer->rings.reserve(strands.pointCount());
    const ScalpMesh* rooted = rootingScalp(strands, scalp);
    std::vector<Frame> frames;
    std::vector<float> arc;
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t count = strands.strandPointCount(k);
        if (count >= 2) {
            const std::size_t at = layer->rings.size();
            layer->rings.resize(at + count);
            strandRings(strands, k, rooted, options, frames, arc, layer->rings.data() + at);
        }
        layer->offsets.push_back(layer->rings.size());
        layer->featherIds.push_back(static_cast<int>(k));
    }

    StrandMesh out;
    out.mesh = buildRachisMesh({layer});
    if (options.strandUVs && n > 0) {
        const auto grid = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(n))));
        const float cell = 1.0f / static_cast<float>(grid);
        constexpr float kMargin = 0.05f;  // of a cell, on each side
        out.u2.assign(out.mesh.u.size(), 0.0f);
        out.v2.assign(out.mesh.v.size(), 0.0f);
        std::size_t corner = 0;
        for (std::size_t f = 0; f < out.mesh.faceCounts.size(); ++f) {
            const auto strand = static_cast<std::size_t>(out.mesh.faceFeather[f]);
            const float cu = static_cast<float>(strand % grid) * cell, cv = static_cast<float>(strand / grid) * cell;
            for (int c = 0; c < out.mesh.faceCounts[f]; ++c, ++corner) {
                const auto uv = static_cast<std::size_t>(out.mesh.faceUVs[corner]);
                out.u2[uv] = cu + cell * (kMargin + (1.0f - 2.0f * kMargin) * std::clamp(out.mesh.u[uv], 0.0f, 1.0f));
                out.v2[uv] = cv + cell * (kMargin + (1.0f - 2.0f * kMargin) * std::clamp(out.mesh.v[uv], 0.0f, 1.0f));
            }
        }
    }
    return out;
}

std::size_t strandMeshPointCount(const StrandSet& strands, const StrandMeshOptions& options) {
    const int sides = meshSides(options);
    std::size_t total = 0;
    for (std::size_t k = 0; k < strands.strandCount(); ++k) total += strandVertexCount(strands.strandPointCount(k), sides);
    return total;
}

void strandMeshPoints(const StrandSet& strands, const ScalpMesh* scalp, const StrandMeshOptions& options, Vec3f* out) {
    const std::size_t n = strands.strandCount();
    const int sides = meshSides(options);
    const ScalpMesh* rooted = rootingScalp(strands, scalp);
    std::vector<std::size_t> first(n + 1, 0);
    for (std::size_t k = 0; k < n; ++k) first[k + 1] = first[k] + strandVertexCount(strands.strandPointCount(k), sides);
    // The angles buildRachisMesh works out for a ring's vertices.
    std::vector<float> cosines, sines;
    for (int j = 0; sides != 2 && j < sides; ++j) {
        const float phi = kTwoPi * static_cast<float>(j) / static_cast<float>(sides);
        cosines.push_back(pm::cos(phi));
        sines.push_back(pm::sin(phi));
    }
    constexpr std::size_t kBlock = 64;  // strands a task takes, with scratch of its own
    parallelFor(
        0, (n + kBlock - 1) / kBlock,
        [&](std::size_t b) {
            std::vector<Frame> frames;
            std::vector<float> arc;
            std::vector<RachisRing> rings;
            for (std::size_t k = b * kBlock; k < std::min(n, (b + 1) * kBlock); ++k) {
                const std::size_t count = strands.strandPointCount(k);
                if (count < 2) continue;
                rings.resize(count);
                strandRings(strands, k, rooted, options, frames, arc, rings.data());
                Vec3f* o = out + first[k];
                for (const RachisRing& r : rings) {
                    if (sides == 2) {
                        *o++ = r.center + r.side * r.radius;
                        *o++ = r.center + r.side * -r.radius;
                        continue;
                    }
                    for (std::size_t j = 0; j < cosines.size(); ++j) *o++ = r.center + (r.side * cosines[j] + r.normal * sines[j]) * r.radius;
                }
                if (sides != 2) *o = rings.back().center;  // the tip's fan
            }
        },
        1);
}

std::uint64_t strandMeshTopologyKey(const StrandSet& strands, const StrandMeshOptions& options) {
    std::uint64_t key = hashCombine(hash64(0x57A4D5u), static_cast<std::uint64_t>(meshSides(options)));
    key = hashCombine(key, options.strandUVs ? 1u : 0u);
    key = hashCombine(key, strands.strandCount());
    const std::vector<StrandId>& ids = strands.ids();
    for (std::size_t k = 0; k < strands.strandCount(); ++k) {
        key = hashCombine(key, k < ids.size() ? static_cast<std::uint64_t>(ids[k]) : 0u);
        key = hashCombine(key, strands.strandPointCount(k));
    }
    return key;
}

bool sameStrandMeshes(const std::vector<StrandMeshSource>& a, const std::vector<StrandMeshSource>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const StrandMeshOptions &x = a[i].options, &y = b[i].options;
        if (a[i].node != b[i].node || a[i].strands != b[i].strands || x.style != y.style || x.sides != y.sides ||
            x.widthScale != y.widthScale || x.minWidth != y.minWidth || x.strandUVs != y.strandUVs) {
            return false;
        }
    }
    return true;
}

StrandMeshSettings strandMeshSettings(const ParamSet& params) {
    StrandMeshSettings s;
    s.options.style = params.getString("style", "tube") == "ribbon" ? StrandMeshOptions::Style::Ribbon
                                                                    : StrandMeshOptions::Style::Tube;
    s.options.sides = static_cast<int>(params.getInt("sides", 4));
    s.options.widthScale = static_cast<float>(params.getDouble("widthScale", 1.0));
    s.options.strandUVs = params.getBool("strandUVs", false);
    s.density = std::clamp(static_cast<float>(params.getDouble("density", 1.0)), 0.0f, 1.0f);
    s.maxStrands = static_cast<std::size_t>(std::max<std::int64_t>(0, params.getInt("maxStrands", 0)));
    return s;
}

}  // namespace groomlab
