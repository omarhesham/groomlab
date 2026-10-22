// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

#include "groomlab/display.h"

namespace groomlab::maya {

// The viewport colors of GroomLab's overlays (core display.h), set by the UI theme
// (groomlab.ui.theme) through groomLabDisplayTheme. Colors are linear RGB; `version` is bumped
// on every change, so the viewport redraws.
using DisplayTheme = groomlab::DisplayTheme;

DisplayTheme& displayTheme();

// `groomLabDisplayTheme`: sets the viewport colors. Each flag takes a string
// of linear RGB floats ("r g b"); -pointSize a number.
// -reset goes back to the defaults. Redraws every view.
class DisplayThemeCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

}  // namespace groomlab::maya
