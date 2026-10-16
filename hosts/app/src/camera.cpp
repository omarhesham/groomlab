// SPDX-License-Identifier: Apache-2.0
#include "camera.h"

#include <algorithm>
#include <cmath>

namespace groomlab::app {

namespace {
constexpr float kPi = 3.14159265358979f;
float radians(float degrees) { return degrees * kPi / 180.0f; }
}  // namespace

OrbitCamera::OrbitCamera() = default;

void OrbitCamera::orbit(float yawDegrees, float pitchDegrees) {
    yaw_ = std::fmod(yaw_ + yawDegrees, 360.0f);
    pitch_ = std::clamp(pitch_ + pitchDegrees, -89.0f, 89.0f);
}

void OrbitCamera::pan(float dxPixels, float dyPixels, int heightPixels) {
    if (heightPixels <= 0) return;
    // The view's height at the target's depth, in scene units per pixel.
    const float unitsPerPixel = 2.0f * distance_ * std::tan(radians(fov_) * 0.5f) / static_cast<float>(heightPixels);
    const QMatrix4x4 v = view();
    const QVector3D right(v(0, 0), v(0, 1), v(0, 2));
    const QVector3D up(v(1, 0), v(1, 1), v(1, 2));
    target_ += (-dxPixels * right + dyPixels * up) * unitsPerPixel;
}

void OrbitCamera::dolly(float factor) {
    if (factor <= 0.0f) return;
    distance_ = std::max(distance_ * factor, 1e-4f);
}

void OrbitCamera::frame(const Bounds& bounds) {
    if (!bounds.valid) {
        target_ = QVector3D(0.0f, 0.0f, 0.0f);
        distance_ = 10.0f;
        return;
    }
    const QVector3D lo(bounds.lo.x, bounds.lo.y, bounds.lo.z), hi(bounds.hi.x, bounds.hi.y, bounds.hi.z);
    target_ = (lo + hi) * 0.5f;
    const float radius = std::max((hi - lo).length() * 0.5f, 1e-3f);
    // The sphere fills the view: tangent of the half field of view at the distance meets the radius.
    distance_ = radius / std::sin(radians(fov_) * 0.5f) * 1.05f;
}

QVector3D OrbitCamera::eye() const {
    const float y = radians(yaw_), p = radians(pitch_);
    const QVector3D dir(std::cos(p) * std::sin(y), std::sin(p), std::cos(p) * std::cos(y));
    return target_ + dir * distance_;
}

QMatrix4x4 OrbitCamera::view() const {
    QMatrix4x4 m;
    m.lookAt(eye(), target_, QVector3D(0.0f, 1.0f, 0.0f));
    return m;
}

QMatrix4x4 OrbitCamera::projection(float aspect, const QMatrix4x4& clipCorrection) const {
    QMatrix4x4 m = clipCorrection;
    const float nearPlane = std::max(distance_ * 0.01f, 1e-4f);
    const float farPlane = distance_ * 100.0f;
    m.perspective(fov_, aspect > 0.0f ? aspect : 1.0f, nearPlane, farPlane);
    return m;
}

}  // namespace groomlab::app
