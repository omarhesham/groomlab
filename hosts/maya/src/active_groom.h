// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>

namespace groomlab::maya {

// The groom the interactive tools work on: the current groom (groomlab.set_current_groom: the
// optionVars groomLabActiveGroom and groomLabPinGroom 1, or 2 for the groom a scene opened with,
// groomlab.adopt_scene_groom), unless a groom was selected since it was made current (the grooms
// selected then: groomLabCurrentSelection), then the first selected groom; with no current groom,
// the first selected one. A groom picked in the Groom Tools or the Graph Editor stays current while
// artists select a scalp, curves or anything else; selecting another groom makes that one current.
// groomlab.active_groom (groom.py) follows the same rule and records the change.
bool activeGroom(MDagPath& out);

// The first groomLabShape in `selection`.
bool groomInSelection(const MSelectionList& selection, MDagPath& out);

// The grooms in `selection` (a groom shape, or the groom shapes right under a selected transform),
// in its order, and their key: their UUIDs sorted and space-separated (groom.py's _selected_grooms).
std::vector<MDagPath> groomsInSelection(const MSelectionList& selection, std::string* key = nullptr);

}  // namespace groomlab::maya
