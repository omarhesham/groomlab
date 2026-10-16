// SPDX-License-Identifier: Apache-2.0
#pragma once

// The viewport's camera as the core's interactive tools read it (ToolView, view_projection.h):
// the orbit camera's world-to-clip matrix in the core's row convention, over a port of the
// widget's size, so the brush, the hover and guide picking project, cast rays and mask by the
// scalp through the matrix alone (its weights may then run in parallel). The tools count pixels
// from the bottom left; Qt counts from the top left, so a widget's mouse position goes through
// toPort first.

#include <array>

#include <QMatrix4x4>
#include <QPointF>

#include "camera.h"
#include "groomlab/view_projection.h"

namespace groomlab::app {

// Qt's column-vector matrix (clip = M * v) as the core's rows (clip = [x y z 1] * M', M' the
// transpose, rows first).
std::array<double, 16> worldToClipRows(const QMatrix4x4& m);

// The camera over a port `width` x `height` (the widget's logical pixels), OpenGL clip space
// (no RHI correction: the tools unproject at z = -1 and 1).
ToolView toolViewFor(const OrbitCamera& camera, int width, int height);

// A widget position (y down from the top) as the tools' port pixel (y up from the bottom).
inline std::array<float, 2> toPort(const QPointF& widgetPosition, int height) {
    return {static_cast<float>(widgetPosition.x()), static_cast<float>(height) - static_cast<float>(widgetPosition.y())};
}

// The brush ring's world radius: core's worldRadiusAt (view_projection.h), what the brush touches.
using groomlab::worldRadiusAt;

}  // namespace groomlab::app
