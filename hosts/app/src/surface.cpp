// SPDX-License-Identifier: Apache-2.0
#include "surface.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

#include "groomlab/parallel.h"
#include "groomlab/random.h"

namespace groomlab::app {

namespace {

constexpr float kPi = 3.14159265358979f;

// Smooth normals: each vertex the area-weighted mean of its triangles'.
std::vector<Vec3f> smoothNormals(const ScalpMesh& mesh) {
    const auto& points = mesh.points();
    std::vector<Vec3f> sums(points.size(), Vec3f{0.0f, 0.0f, 0.0f});
    for (const ScalpMesh::Triangle& t : mesh.triangles()) {
        const Vec3f n = (points[t[1]] - points[t[0]]).cross(points[t[2]] - points[t[0]]);
        for (int k = 0; k < 3; ++k) sums[t[k]] += n;
    }
    for (Vec3f& n : sums) n = n.normalized();
    return sums;
}

struct Key {
    std::int32_t point;
    std::array<float, 3> normal;
    std::array<float, 2> uv;
    bool operator==(const Key& o) const { return point == o.point && normal == o.normal && uv == o.uv; }
};
struct KeyHash {
    std::size_t operator()(const Key& k) const {
        std::uint64_t h = static_cast<std::uint64_t>(k.point);
        auto mix = [&](float f) {
            std::uint32_t bits;
            std::memcpy(&bits, &f, sizeof bits);
            h = hash64(h ^ bits);
        };
        for (float f : k.normal) mix(f);
        for (float f : k.uv) mix(f);
        return static_cast<std::size_t>(h);
    }
};

}  // namespace

SurfaceVertices buildSurfaceVertices(const Surface& surface) {
    SurfaceVertices out;
    if (!surface.mesh) return out;
    const ScalpMesh& mesh = *surface.mesh;
    const auto& points = mesh.points();
    const auto& triangles = mesh.triangles();
    const std::vector<Vec3f> smooth = smoothNormals(mesh);
    const bool cornerNormals = surface.cornerNormals.size() == triangles.size() * 3;
    const bool uvs = mesh.hasUVs() && mesh.cornerUVs().size() == triangles.size() * 3;
    // One vertex per distinct (point, normal, uv) corner.
    std::unordered_map<Key, std::uint32_t, KeyHash> lookup;
    std::vector<Key> keys;
    out.indices.reserve(triangles.size() * 3);
    for (std::size_t t = 0; t < triangles.size(); ++t) {
        for (int k = 0; k < 3; ++k) {
            const std::int32_t p = triangles[t][k];
            const Vec3f n = cornerNormals ? surface.cornerNormals[t * 3 + k] : smooth[p];
            const ScalpMesh::UV uv = uvs ? mesh.cornerUVs()[t * 3 + k] : ScalpMesh::UV{0.0f, 0.0f};
            const Key key{p, {n.x, n.y, n.z}, uv};
            auto it = lookup.find(key);
            if (it == lookup.end()) {
                it = lookup.emplace(key, static_cast<std::uint32_t>(keys.size())).first;
                keys.push_back(key);
            }
            out.indices.push_back(it->second);
        }
    }
    // Tangents per vertex from the UV gradients of each triangle (Lengyel), summed.
    std::vector<Vec3f> tangents(keys.size(), Vec3f{0.0f, 0.0f, 0.0f}), bitangents(keys.size(), Vec3f{0.0f, 0.0f, 0.0f});
    if (uvs) {
        for (std::size_t t = 0; t < triangles.size(); ++t) {
            const std::uint32_t i0 = out.indices[t * 3], i1 = out.indices[t * 3 + 1], i2 = out.indices[t * 3 + 2];
            const Vec3f p0 = points[keys[i0].point], p1 = points[keys[i1].point], p2 = points[keys[i2].point];
            const auto& w0 = keys[i0].uv;
            const auto& w1 = keys[i1].uv;
            const auto& w2 = keys[i2].uv;
            const Vec3f e1 = p1 - p0, e2 = p2 - p0;
            const float s1 = w1[0] - w0[0], s2 = w2[0] - w0[0], t1 = w1[1] - w0[1], t2 = w2[1] - w0[1];
            const float det = s1 * t2 - s2 * t1;
            if (std::abs(det) < 1e-12f) continue;
            const float r = 1.0f / det;
            const Vec3f sdir = (e1 * t2 - e2 * t1) * r, tdir = (e2 * s1 - e1 * s2) * r;
            for (std::uint32_t i : {i0, i1, i2}) {
                tangents[i] += sdir;
                bitangents[i] += tdir;
            }
        }
    }
    // The curvature per mesh point: |dn| / |dp| along each edge, averaged over the point's edges.
    std::vector<float> curvature(points.size(), 0.0f), edgeCount(points.size(), 0.0f);
    for (const ScalpMesh::Triangle& tri : triangles) {
        for (int k = 0; k < 3; ++k) {
            const std::int32_t a = tri[k], b = tri[(k + 1) % 3];
            const float dp = (points[b] - points[a]).length();
            if (dp < 1e-9f) continue;
            const float c = (smooth[b] - smooth[a]).length() / dp;
            curvature[a] += c;
            curvature[b] += c;
            edgeCount[a] += 1.0f;
            edgeCount[b] += 1.0f;
        }
    }
    for (std::size_t i = 0; i < points.size(); ++i) curvature[i] = edgeCount[i] > 0.0f ? curvature[i] / edgeCount[i] : 0.0f;
    out.data.resize(keys.size() * SurfaceVertices::kStride);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        const Key& key = keys[i];
        const Vec3f p = points[key.point];
        const Vec3f n{key.normal[0], key.normal[1], key.normal[2]};
        Vec3f t = tangents[i] - n * n.dot(tangents[i]);
        float sign = 1.0f;
        if (t.length() < 1e-8f) {
            // No UVs (or degenerate ones): any direction across the normal.
            const Vec3f axis = std::abs(n.y) < 0.9f ? Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f};
            t = axis.cross(n);
        } else {
            sign = n.cross(t).dot(bitangents[i]) < 0.0f ? -1.0f : 1.0f;
        }
        t = t.normalized();
        float* o = out.data.data() + i * SurfaceVertices::kStride;
        o[0] = p.x;
        o[1] = p.y;
        o[2] = p.z;
        o[3] = n.x;
        o[4] = n.y;
        o[5] = n.z;
        o[6] = t.x;
        o[7] = t.y;
        o[8] = t.z;
        o[9] = sign;
        o[10] = key.uv[0];
        o[11] = key.uv[1];
        o[12] = static_cast<std::size_t>(key.point) < surface.vertexOcclusion.size() ? surface.vertexOcclusion[key.point] : 1.0f;
        o[13] = curvature[key.point];
    }
    return out;
}

std::vector<float> bakeVertexOcclusion(const ScalpMesh& mesh, const std::vector<MeshPtr>& occluders, int rays, float maxDistance) {
    const auto& points = mesh.points();
    std::vector<float> occlusion(points.size(), 1.0f);
    if (rays <= 0 || occluders.empty()) return occlusion;
    const std::vector<Vec3f> normals = smoothNormals(mesh);
    // A little off the surface along the normal, so the point's own triangles do not count.
    const float lift = 1e-3f * maxDistance;
    parallelFor(0, points.size(), [&](std::size_t i) {
        const Vec3f n = normals[i];
        const Vec3f axis = std::abs(n.y) < 0.9f ? Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f};
        const Vec3f t = axis.cross(n).normalized(), b = n.cross(t);
        const Vec3f origin = points[i] + n * lift;
        int hits = 0;
        for (int r = 0; r < rays; ++r) {
            // Cosine-weighted directions, stratified by a hash per point and ray.
            const std::uint64_t h = hash64(static_cast<std::uint64_t>(i) * 977 + r);
            const float u1 = (r + unitFloat(h)) / rays, u2 = unitFloat(hash64(h ^ 0xABCD));
            const float radius = std::sqrt(u1), phi = 2.0f * kPi * u2;
            const Vec3f dir = t * (radius * std::cos(phi)) + b * (radius * std::sin(phi)) + n * std::sqrt(std::max(0.0f, 1.0f - u1));
            for (const MeshPtr& occluder : occluders) {
                if (!occluder) continue;
                SurfacePoint hit;
                float dist = 0.0f;
                if (occluder->intersectRay(origin, dir, &hit, &dist) && dist < maxDistance) {
                    ++hits;
                    break;
                }
            }
        }
        occlusion[i] = 1.0f - static_cast<float>(hits) / static_cast<float>(rays);
    }, 64);
    return occlusion;
}

Surface makeEyeSurface(const Vec3f& center, const Vec3f& forwardIn, const Vec3f& upIn, float radius,
                       const SurfaceMaterial& material, int rings, int segments) {
    Surface s;
    s.name = "eye";
    s.material = material;
    s.isEye = true;
    s.eyeCenter = center;
    s.eyeForward = forwardIn.normalized();
    Vec3f up = upIn - s.eyeForward * s.eyeForward.dot(upIn);
    if (up.length() < 1e-6f) up = std::abs(s.eyeForward.y) < 0.9f ? Vec3f{0.0f, 1.0f, 0.0f} : Vec3f{1.0f, 0.0f, 0.0f};
    s.eyeUp = (up - s.eyeForward * s.eyeForward.dot(up)).normalized();
    s.eyeRadius = radius;
    const Vec3f side = s.eyeUp.cross(s.eyeForward).normalized();
    // The cornea sphere's center along the forward axis: where a sphere of kEyeCorneaRadius
    // passes through the limbus circle (sin = 0.49 out, cos = kEyeLimbusCos ahead).
    const float limbusSin = std::sqrt(1.0f - kEyeLimbusCos * kEyeLimbusCos);
    const float corneaCenter = kEyeLimbusCos - std::sqrt(kEyeCorneaRadius * kEyeCorneaRadius - limbusSin * limbusSin);
    const float limbusAngle = std::acos(kEyeLimbusCos);
    std::vector<Vec3f> points;
    std::vector<ScalpMesh::UV> uvs;
    std::vector<ScalpMesh::Triangle> triangles;
    std::vector<Vec3f> normals;
    // Rings from the apex (theta 0, along forward) to the back; the limbus falls on a ring so
    // the crease is sharp there.
    std::vector<float> thetas;
    for (int r = 0; r <= rings; ++r) thetas.push_back(kPi * r / rings);
    thetas.push_back(limbusAngle);
    std::sort(thetas.begin(), thetas.end());
    for (std::size_t r = 0; r < thetas.size(); ++r) {
        const float theta = thetas[r];
        for (int q = 0; q <= segments; ++q) {
            const float phi = 2.0f * kPi * q / segments;
            // On the unit sphere, then the cornea's bulge inside the limbus.
            Vec3f local{std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta)};
            Vec3f normal = local;
            if (theta < limbusAngle - 1e-5f) {
                // Along the same ray from the center, out to the cornea sphere.
                const float b = -2.0f * local.z * corneaCenter;
                const float c = corneaCenter * corneaCenter - kEyeCorneaRadius * kEyeCorneaRadius;
                const float t = (-b + std::sqrt(std::max(b * b - 4.0f * c, 0.0f))) * 0.5f;
                local = local * t;
                normal = (local - Vec3f{0.0f, 0.0f, corneaCenter}).normalized();
            }
            points.push_back(center + side * (local.x * radius) + s.eyeUp * (local.y * radius) + s.eyeForward * (local.z * radius));
            normals.push_back(side * normal.x + s.eyeUp * normal.y + s.eyeForward * normal.z);
            uvs.push_back({static_cast<float>(q) / segments, 1.0f - theta / kPi});
        }
    }
    const int cols = segments + 1;
    for (std::size_t r = 0; r + 1 < thetas.size(); ++r) {
        for (int q = 0; q < segments; ++q) {
            const std::int32_t a = static_cast<std::int32_t>(r * cols + q), b = a + 1;
            const std::int32_t c = static_cast<std::int32_t>((r + 1) * cols + q), d = c + 1;
            if (r > 0) triangles.push_back({a, c, b});
            if (r + 2 < thetas.size()) triangles.push_back({b, c, d});
        }
    }
    auto mesh = std::make_shared<ScalpMesh>(std::move(points), std::move(triangles));
    std::vector<ScalpMesh::UV> cornerUVs;
    for (const ScalpMesh::Triangle& t : mesh->triangles()) {
        for (int k = 0; k < 3; ++k) {
            cornerUVs.push_back(uvs[t[k]]);
            s.cornerNormals.push_back(normals[t[k]]);
        }
    }
    mesh->setCornerUVs(std::move(cornerUVs));
    s.mesh = mesh;
    return s;
}

}  // namespace groomlab::app
