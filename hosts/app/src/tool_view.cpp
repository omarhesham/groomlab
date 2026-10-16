// SPDX-License-Identifier: Apache-2.0
#include "tool_view.h"

#include <cmath>

namespace groomlab::app {

std::array<double, 16> worldToClipRows(const QMatrix4x4& m) {
    // The core wants clip = [x y z 1] * M', so M' is the transpose of Qt's column-vector
    // matrix: row r of M' is column r of M.
    std::array<double, 16> rows{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) rows[static_cast<std::size_t>(r * 4 + c)] = static_cast<double>(m(c, r));
    }
    return rows;
}

ToolView toolViewFor(const OrbitCamera& camera, int width, int height) {
    if (width <= 0 || height <= 0) return {};
    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    return ToolView::fromMatrix(worldToClipRows(camera.viewProjection(aspect)), width, height);
}

}  // namespace groomlab::app
