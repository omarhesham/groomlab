// SPDX-License-Identifier: Apache-2.0
#include "groomlab/guide_set.h"

#include <algorithm>
#include <cstring>

#include "groomlab/curve.h"
#include "groomlab/random.h"
#include "groomlab/scalp_mesh.h"

namespace groomlab {

Guide& GuideSet::add(std::uint64_t id, std::vector<Vec3f> points) {
    Guide g;
    g.id = id;
    g.points = std::move(points);
    guides_.push_back(std::move(g));
    return guides_.back();
}

bool drawnTipFirst(const std::vector<Vec3f>& points, const ScalpMesh& mesh) {
    if (points.size() < 2) return false;
    const float dFront = (mesh.closestPoint(points.front()).position - points.front()).length();
    const float dBack = (mesh.closestPoint(points.back()).position - points.back()).length();
    return dBack + 0.02f * arcLength(points.data(), points.size()) < dFront;
}

StrandRoot rootRigidly(std::vector<Vec3f>& points, const ScalpMesh& mesh) {
    const SurfacePoint sp = mesh.closestPoint(points.front());
    const Vec3f shift = sp.position - points.front();
    for (Vec3f& p : points) p += shift;
    return sp.root;
}

void GuideSet::rootOnMesh(const ScalpMesh& mesh) {
    if (mesh.triangleCount() == 0) return;
    for (Guide& g : guides_) {
        if (!g.points.empty()) g.root = rootRigidly(g.points, mesh);
    }
}

void GuideSet::orientAndRoot(const ScalpMesh& mesh) {
    if (mesh.triangleCount() == 0) return;
    for (Guide& g : guides_) {
        if (drawnTipFirst(g.points, mesh)) std::reverse(g.points.begin(), g.points.end());
    }
    rootOnMesh(mesh);
}

std::uint64_t GuideSet::contentHash() const {
    std::uint64_t h = hash64(guides_.size());
    for (const Guide& g : guides_) {
        h = hashCombine(h, g.id);
        h = hashCombine(h, static_cast<std::uint64_t>(static_cast<std::int64_t>(g.root.face)));
        h = hashFloat(hashFloat(h, g.root.u), g.root.v);
        for (const auto& [name, value] : g.attributes) h = hashFloat(hashString(h, name), value);
        for (const auto& [name, layer] : g.pointAttributes) {
            h = hashCombine(hashString(h, name), layer.size());
            for (const float value : layer) h = hashFloat(h, value);
        }
        h = hashCombine(h, g.points.size());
        for (const Vec3f& p : g.points) h = hashFloat(hashFloat(hashFloat(h, p.x), p.y), p.z);
    }
    return h;
}

}  // namespace groomlab
