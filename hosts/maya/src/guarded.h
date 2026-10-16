// SPDX-License-Identifier: Apache-2.0
#pragma once

// Nothing thrown in GroomLab code may cross into Maya: an exception leaving a
// command, a tool event or a callback ends the session (often without a crash
// report). Entry points run their work through guarded(), which turns any
// exception into a Script Editor error and a failure status.

#include <exception>
#include <utility>

#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

namespace groomlab::maya {

template <class Work>
MStatus guarded(const char* where, Work&& work) {
    try {
        return std::forward<Work>(work)();
    } catch (const std::exception& e) {
        MGlobal::displayError(MString("GroomLab ") + where + ": " + e.what());
    } catch (...) {
        MGlobal::displayError(MString("GroomLab ") + where + ": unexpected error");
    }
    return MS::kFailure;
}

}  // namespace groomlab::maya
