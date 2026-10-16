// SPDX-License-Identifier: Apache-2.0
#pragma once

// Maya's views as the core's tools read them (ToolView, view_projection.h).

#include <maya/M3dView.h>

#include "groomlab/guide_selection.h"
#include "groomlab/view_projection.h"

namespace groomlab::maya {

// A viewport as a ToolView: its matrices read once (M3dView::worldToView asks the view for them
// on every call, which dominates a stamp over thousands of guide points), used where they agree
// with worldToView at two points in front of the camera; any disagreement (an unusual camera or
// device) falls back to worldToView point by point (and the brush then skips no guides by their
// bounds). Rays are viewToWorld's, the camera the view's.
ToolView toolView(const M3dView& view);
inline ToolView activeToolView() { return toolView(M3dView::active3dView()); }

// What a selection tool projects with in a view: whole pixels as worldToView gives them (points
// inside the view volume, truncated), read from the matrix when it gives worldToView's answers
// at a few points, else worldToView itself (`matrix` then invalid: no bounds to skip guides by).
struct SelectionProjector {
    ViewProjector project;
    ViewProjection matrix;
};
SelectionProjector selectionProjector(const M3dView& view);

}  // namespace groomlab::maya
