// SPDX-License-Identifier: Apache-2.0
#include "groomlab/point_index.h"

#include <algorithm>
#include <limits>

#include "groomlab/parallel.h"

namespace groomlab {

namespace {

float coord(const Vec3f& p, int axis) { return axis == 0 ? p.x : axis == 1 ? p.y : p.z; }

constexpr int kMaxDepth = 64;
// Subtrees of at least this many points build side by side (each is the same as built alone).
constexpr std::size_t kParallelBuild = 16384;

}  // namespace

PointIndex::PointIndex(std::vector<Vec3f> points) {
    const std::size_t n = points.size();
    std::vector<std::uint32_t> order(n);
    for (std::size_t i = 0; i < n; ++i) order[i] = static_cast<std::uint32_t>(i);
    axes_.resize(n);
    build(points, order.data(), 0, n);
    // Points in tree order: a node's point sits at its own position.
    points_.resize(n);
    parallelFor(0, n, [&](std::size_t i) { points_[i] = points[order[i]]; });
    ids_ = std::move(order);
}

void PointIndex::build(const std::vector<Vec3f>& input, std::uint32_t* order, std::size_t begin, std::size_t end) {
    if (begin >= end) return;

    // Split on the axis with the largest extent (robust for flat point sets).
    Vec3f lo = input[order[begin]], hi = lo;
    for (std::size_t i = begin + 1; i < end; ++i) {
        const Vec3f& p = input[order[i]];
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    const Vec3f ext = hi - lo;
    const int axis = ext.x >= ext.y && ext.x >= ext.z ? 0 : ext.y >= ext.z ? 1 : 2;

    // Equal coordinates in index order: a total order, so the median and each half's points are
    // the same whatever the standard library's nth_element leaves them in, and so is the tree
    // (which of several equally near points a search meets first, and returns).
    const std::size_t mid = begin + (end - begin) / 2;
    std::nth_element(order + begin, order + mid, order + end, [&](std::uint32_t a, std::uint32_t b) {
        const float ca = coord(input[a], axis), cb = coord(input[b], axis);
        return ca < cb || (ca == cb && a < b);
    });
    axes_[mid] = static_cast<std::uint8_t>(axis);

    // The two halves never touch each other's part of `order`.
    if (end - begin >= kParallelBuild) {
        parallelInvoke([&] { build(input, order, begin, mid); }, [&] { build(input, order, mid + 1, end); });
    } else {
        build(input, order, begin, mid);
        build(input, order, mid + 1, end);
    }
}

std::size_t PointIndex::nearest(const Vec3f& p, float* distance2) const {
    std::size_t index = SIZE_MAX;
    float d2 = std::numeric_limits<float>::max();
    if (kNearest(p, 1, &index, &d2) == 0) return SIZE_MAX;
    if (distance2) *distance2 = d2;
    return index;
}

std::size_t PointIndex::kNearest(const Vec3f& p, std::size_t k, std::size_t* indices, float* distances2) const {
    if (points_.empty() || k == 0) return 0;
    std::size_t found = 0;

    // Sorted insertion into the result arrays (k is small).
    auto consider = [&](std::size_t node) {
        const Vec3f d = points_[node] - p;
        const float d2 = d.dot(d);
        if (found == k && d2 >= distances2[k - 1]) return;
        std::size_t pos = std::min(found, k - 1);
        while (pos > 0 && distances2[pos - 1] > d2) {
            distances2[pos] = distances2[pos - 1];
            indices[pos] = indices[pos - 1];
            --pos;
        }
        distances2[pos] = d2;
        indices[pos] = ids_[node];
        if (found < k) ++found;
    };

    // Depth-first with a small explicit stack; each entry is a subtree (its
    // range) and the squared distance to its parent's splitting plane, for pruning.
    struct Entry {
        std::uint32_t begin, end;
        float planeDistance2;
    };
    Entry stack[2 * kMaxDepth + 2];
    int top = 0;
    stack[top++] = {0, static_cast<std::uint32_t>(points_.size()), 0.0f};
    while (top > 0) {
        const Entry e = stack[--top];
        if (found == k && e.planeDistance2 >= distances2[k - 1]) continue;
        const std::uint32_t mid = e.begin + (e.end - e.begin) / 2;
        consider(mid);

        const int axis = axes_[mid];
        const float diff = coord(p, axis) - coord(points_[mid], axis);
        // Children: left [begin, mid), right [mid + 1, end); empty ones are skipped.
        const Entry left{e.begin, mid, 0.0f}, right{mid + 1, e.end, 0.0f};
        Entry nearChild = diff < 0 ? left : right;
        Entry farChild = diff < 0 ? right : left;
        farChild.planeDistance2 = diff * diff;
        if (farChild.begin < farChild.end && top < 2 * kMaxDepth + 1) stack[top++] = farChild;
        if (nearChild.begin < nearChild.end && top < 2 * kMaxDepth + 1) stack[top++] = nearChild;
    }
    return found;
}

}  // namespace groomlab
