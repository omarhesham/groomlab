// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

#include "groomlab/guide_set.h"
#include "groomlab/portable_math.h"
#include "groomlab/scalp_mesh.h"

namespace groomlab::test {

// Whether this build evaluates sin/exp/pow... as the bit-exact recordings were made
// (GROOMLAB_PORTABLE_MATH: the same bits on every platform, portable_math.h). A build with it off
// (the C runtime's float functions) checks what follows from them within the tolerances alone.
#if GROOMLAB_PORTABLE_MATH
constexpr bool kRecordedMath = true;
#else
constexpr bool kRecordedMath = false;
#endif

// Unit square in the XZ plane, normal +Y, split into cells x cells quads.
inline std::shared_ptr<const ScalpMesh> makePlane(int cells = 4, float size = 1.0f) {
    std::vector<Vec3f> points;
    std::vector<ScalpMesh::Triangle> tris;
    const float step = size / static_cast<float>(cells);
    for (int z = 0; z <= cells; ++z) {
        for (int x = 0; x <= cells; ++x) points.emplace_back(x * step, 0.0f, z * step);
    }
    auto at = [cells](int x, int z) { return z * (cells + 1) + x; };
    for (int z = 0; z < cells; ++z) {
        for (int x = 0; x < cells; ++x) {
            tris.push_back({at(x, z), at(x, z + 1), at(x + 1, z)});
            tris.push_back({at(x, z + 1), at(x + 1, z + 1), at(x + 1, z)});
        }
    }
    return std::make_shared<const ScalpMesh>(std::move(points), std::move(tris));
}

// UV sphere of `radius`, `rings` x `segments` quads split into triangles
// (outward normals). Pole rows are degenerate triangles; fine for queries.
inline std::shared_ptr<const ScalpMesh> makeSphere(int rings, int segments, float radius = 1.0f) {
    std::vector<Vec3f> points;
    std::vector<ScalpMesh::Triangle> tris;
    for (int r = 0; r <= rings; ++r) {
        const float theta = 3.14159265f * static_cast<float>(r) / rings;
        for (int s = 0; s <= segments; ++s) {
            const float phi = 6.2831853f * static_cast<float>(s) / segments;
            points.emplace_back(radius * std::sin(theta) * std::cos(phi), radius * std::cos(theta),
                                radius * std::sin(theta) * std::sin(phi));
        }
    }
    auto at = [segments](int r, int s) { return r * (segments + 1) + s; };
    for (int r = 0; r < rings; ++r) {
        for (int s = 0; s < segments; ++s) {
            tris.push_back({at(r, s), at(r, s + 1), at(r + 1, s)});
            tris.push_back({at(r, s + 1), at(r + 1, s + 1), at(r + 1, s)});
        }
    }
    return std::make_shared<const ScalpMesh>(std::move(points), std::move(tris));
}

// A thin flap, as an ear stands out of a head: two faces `thickness` apart (z = -thickness / 2,
// normal -z, and z = +thickness / 2, normal +z) rising from y = 0 to `height`, joined over the top
// by a half cylinder about the x axis, all along x in [0, depth]; with `ground`, standing on a
// floor (y = 0, normal +y) out to z = -halfWidth and +halfWidth, one surface with it. About
// `step` between vertices.
inline std::shared_ptr<const ScalpMesh> makeFin(float height, float thickness, float depth, float step, bool ground = false,
                                               float halfWidth = 0.0f) {
    // The profile in (y, z), from the floor on the -z side round to the floor on the +z side.
    std::vector<std::array<float, 2>> profile;
    auto line = [&](float y0, float z0, float y1, float z1) {
        const int n = std::max(1, static_cast<int>(std::ceil(std::hypot(y1 - y0, z1 - z0) / step)));
        for (int i = profile.empty() ? 0 : 1; i <= n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(n);
            profile.push_back({y0 + (y1 - y0) * t, z0 + (z1 - z0) * t});
        }
    };
    const float r = 0.5f * thickness;
    if (ground) line(0.0f, -halfWidth, 0.0f, -r);
    line(0.0f, -r, height, -r);
    const int arc = std::max(4, static_cast<int>(std::ceil(3.14159265f * r / step)));
    for (int i = 1; i <= arc; ++i) {
        const float a = 3.14159265f * static_cast<float>(i) / static_cast<float>(arc);
        profile.push_back({height + r * std::sin(a), -r * std::cos(a)});
    }
    line(height, r, 0.0f, r);
    if (ground) line(0.0f, r, 0.0f, halfWidth);
    const int columns = std::max(1, static_cast<int>(std::ceil(depth / step)));
    std::vector<Vec3f> points;
    std::vector<ScalpMesh::Triangle> tris;
    const int rows = static_cast<int>(profile.size());
    for (int c = 0; c <= columns; ++c) {
        const float x = depth * static_cast<float>(c) / static_cast<float>(columns);
        for (const auto& p : profile) points.emplace_back(x, p[0], p[1]);
    }
    auto at = [rows](int c, int i) { return c * rows + i; };
    for (int c = 0; c < columns; ++c) {
        for (int i = 0; i + 1 < rows; ++i) {
            tris.push_back({at(c, i), at(c, i + 1), at(c + 1, i)});
            tris.push_back({at(c, i + 1), at(c + 1, i + 1), at(c + 1, i)});
        }
    }
    return std::make_shared<const ScalpMesh>(std::move(points), std::move(tris));
}

// A straight guide from `root` along `dir`, `segments` + 1 points.
inline std::vector<Vec3f> straightCurve(Vec3f root, Vec3f dir, int segments = 4) {
    std::vector<Vec3f> pts;
    for (int i = 0; i <= segments; ++i) pts.push_back(root + dir * (static_cast<float>(i) / segments));
    return pts;
}

// A camera at `eye` looking at `target` (y up), vertical field of view `fovDegrees`, as Maya's
// world-to-clip matrix (row vectors: clip = [x y z 1] * M, rows first; OpenGL clip space).
inline std::array<double, 16> lookAtProjection(Vec3f eye, Vec3f target, double fovDegrees, double aspect) {
    const Vec3f f = (target - eye).normalized();
    const Vec3f r = f.cross(Vec3f(0, 1, 0)).normalized();
    const Vec3f u = r.cross(f);
    // Column-vector view matrix rows: r, u, -f; translation -R e.
    const double view[4][4] = {{r.x, r.y, r.z, -r.dot(eye)}, {u.x, u.y, u.z, -u.dot(eye)}, {-f.x, -f.y, -f.z, f.dot(eye)}, {0, 0, 0, 1}};
    const double t = 1.0 / std::tan(fovDegrees * 3.14159265358979 / 360.0), n = 0.1, fa = 100.0;
    const double proj[4][4] = {{t / aspect, 0, 0, 0}, {0, t, 0, 0}, {0, 0, -(fa + n) / (fa - n), -2 * fa * n / (fa - n)}, {0, 0, -1, 0}};
    double pv[4][4] = {};
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            for (int k = 0; k < 4; ++k) pv[i][j] += proj[i][k] * view[k][j];
        }
    }
    std::array<double, 16> m{};
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) m[static_cast<std::size_t>(row * 4 + col)] = pv[col][row];
    }
    return m;
}

// The share of its radius Curl gives a point `arc` along its strand, at share t of its points,
// turning at `frequency`: eased in from the root over the first quarter turn, or the first quarter
// of the strand where that comes first (curl_op.cpp curlEase).
inline float curlEase(float arc, float t, float frequency) {
    const float x = std::min(1.0f, std::max(4.0f * std::fabs(frequency) * arc, 4.0f * t));
    return x * x * (3.0f - 2.0f * x);
}

}  // namespace groomlab::test
