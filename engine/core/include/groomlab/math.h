// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <algorithm>
#include <cmath>

#include "groomlab/portable_math.h"

namespace groomlab {

// Minimal vector type for the core. Conversion to Imath/MFloatPoint/GfVec3f
// happens at the host and file-format boundaries.
struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3f() = default;
    constexpr Vec3f(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3f operator+(const Vec3f& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3f operator-(const Vec3f& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3f operator*(float s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3f operator/(float s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3f operator-() const { return {-x, -y, -z}; }

    Vec3f& operator+=(const Vec3f& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3f& operator-=(const Vec3f& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3f& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

    constexpr bool operator==(const Vec3f& o) const { return x == o.x && y == o.y && z == o.z; }
    constexpr bool operator!=(const Vec3f& o) const { return !(*this == o); }

    float length() const { return std::sqrt(dot(*this)); }
    constexpr float dot(const Vec3f& o) const { return x * o.x + y * o.y + z * o.z; }
    constexpr Vec3f cross(const Vec3f& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    Vec3f normalized() const {
        const float len = length();
        return len > 0.0f ? *this / len : Vec3f{};
    }
};

constexpr Vec3f operator*(float s, const Vec3f& v) { return v * s; }

// v rotated by `angle` radians about the unit vector `axis` (right-handed).
inline Vec3f rotateAxisAngle(const Vec3f& v, const Vec3f& axis, float angle) {
    const float c = pm::cos(angle), s = pm::sin(angle);
    return v * c + axis.cross(v) * s + axis * (axis.dot(v) * (1.0f - c));
}

// Symmetry across the plane where coordinate `axis` (0 x, 1 y, 2 z) equals
// `center`: a point mirrored, and a direction mirrored (its axis component flipped).
inline Vec3f mirrorPoint(const Vec3f& p, int axis, float center) {
    Vec3f out = p;
    if (axis == 0) out.x = 2.0f * center - p.x;
    if (axis == 1) out.y = 2.0f * center - p.y;
    if (axis == 2) out.z = 2.0f * center - p.z;
    return out;
}
inline Vec3f mirrorVector(const Vec3f& v, int axis) {
    Vec3f out = v;
    if (axis == 0) out.x = -v.x;
    if (axis == 1) out.y = -v.y;
    if (axis == 2) out.z = -v.z;
    return out;
}


// 0 at e0, 1 at e1 and a smooth S between, held past the ends (reversed
// edges run it backwards; equal edges make a step at e0). As GLSL and the
// expression language define it.
inline float smoothstep(float e0, float e1, float x) {
    if (e1 == e0) return x < e0 ? 0.0f : 1.0f;
    const float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// A positive size rounded to a step of a log lattice of `stepsPerDecade` steps per power of ten
// (`up`: to the step at or above it), anchored at 1: a value taken from the whole scalp or all the
// strands, rounded so that an edit changing it a little keeps it. A decade being whole steps, a
// scene scaled by a power of ten (cm to m or mm) rounds to the same step scaled alike: the groom
// looks the same. Not positive or not finite: as it is.
inline float roundToDecadeStep(float value, int stepsPerDecade, bool up = false) {
    if (!(value > 0.0f) || !std::isfinite(value)) return value;
    const double steps = pm::log10(static_cast<double>(value)) * stepsPerDecade;
    // A hair over a step (rounding, as a scaled value's logarithm carries) stays on it.
    const double whole = up ? std::ceil(steps - 1e-9) : std::round(steps);
    return static_cast<float>(pm::pow(10.0, whole / stepsPerDecade));
}

}  // namespace groomlab
