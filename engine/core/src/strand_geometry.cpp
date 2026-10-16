// SPDX-License-Identifier: Apache-2.0
#include "groomlab/strand_geometry.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>

#include "groomlab/frames.h"
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

StrandMesh buildStrandMesh(const StrandSet& strands, const ScalpMesh* scalp, const StrandMeshOptions& options) {
    const bool ribbon = options.style == StrandMeshOptions::Style::Ribbon;
    auto layer = std::make_shared<FeatherLayer>();
    layer->sides = ribbon ? 2 : std::max(3, options.sides);
    const std::size_t n = strands.strandCount();
    layer->offsets.reserve(n + 1);
    layer->featherIds.reserve(n);
    layer->rings.reserve(strands.pointCount());
    const std::vector<float>& widths = strands.widths();
    const std::vector<StrandRoot>& roots = strands.roots();
    const bool rooted = scalp && scalp->triangleCount() > 0 && roots.size() == n;
    std::vector<Frame> frames;
    std::vector<float> arc;
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t count = strands.strandPointCount(k);
        const Vec3f* p = strands.strandPoints(k);
        const std::size_t begin = strands.strandBegin(k);
        if (count >= 2) {
            Vec3f up = anyPerpendicular((p[1] - p[0]).normalized());
            if (rooted && roots[k].face >= 0 && static_cast<std::size_t>(roots[k].face) < scalp->triangleCount()) {
                const Vec3f normal = scalp->normal(roots[k]);
                if (normal.length() > 0.5f) up = normal;
            }
            rotationMinimizingFramesInto(p, count, up, frames);
            arc.assign(count, 0.0f);
            for (std::size_t i = 1; i < count; ++i) arc[i] = arc[i - 1] + (p[i] - p[i - 1]).length();
            const float total = arc.back() > 0.0f ? arc.back() : 1.0f;
            for (std::size_t i = 0; i < count; ++i) {
                RachisRing ring;
                ring.center = p[i];
                ring.tangent = frames[i].tangent;
                ring.normal = frames[i].normal;
                ring.side = frames[i].binormal;
                const float w = begin + i < widths.size() ? widths[begin + i] : 1.0f;
                ring.radius = 0.5f * std::max(w * options.widthScale, options.minWidth);
                ring.u = arc[i] / total;
                layer->rings.push_back(ring);
            }
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

}  // namespace groomlab
