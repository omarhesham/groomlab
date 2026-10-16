// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>

namespace groomlab::maya {

// The groom the interactive tools work on: the pinned groom when one is
// pinned (the Tool Palette's pin: optionVars groomLabPinGroom and
// groomLabActiveGroom), else the first groomLabShape in the selection (a
// shape or its transform). Pinning lets artists select a scalp, curves or
// anything else without the tools losing the groom.
bool activeGroom(MDagPath& out);

// The first groomLabShape in `selection`.
bool groomInSelection(const MSelectionList& selection, MDagPath& out);

}  // namespace groomlab::maya
