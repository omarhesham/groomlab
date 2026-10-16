// SPDX-License-Identifier: Apache-2.0
#pragma once

// What the GroomLab commands share: errors in the Script Editor, flag values
// with a fallback, and the groom a command acts on.

#include <maya/MArgDatabase.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include "groom_shape.h"

namespace groomlab::maya {

// Shows "command: message" as an error and fails.
inline MStatus fail(const char* command, const MString& message) {
    MGlobal::displayError(MString(command) + ": " + message);
    return MS::kFailure;
}

// A flag's first argument, or `fallback` when the flag is not set. Takes an
// MArgDatabase or an MArgParser (a context's edit flags).
template <class Args>
double flagDouble(const Args& db, const char* flag, double fallback) {
    double v = fallback;
    if (db.isFlagSet(flag)) db.getFlagArgument(flag, 0, v);
    return v;
}

template <class Args>
MString flagString(const Args& db, const char* flag, const MString& fallback = "") {
    MString v = fallback;
    if (db.isFlagSet(flag)) db.getFlagArgument(flag, 0, v);
    return v;
}

// The groom shape named by the command's object (or the selection). Reports
// and fails when there is none or it is not a groomLabShape.
inline MStatus groomArgument(const char* command, const MArgDatabase& db, MDagPath* path) {
    MSelectionList selection;
    db.getObjects(selection);
    if (selection.getDagPath(0, *path) != MS::kSuccess) return fail(command, "select or name a groomLabShape");
    path->extendToShape();
    if (MFnDagNode(*path).typeName() != GroomShape::kTypeName) {
        return fail(command, path->partialPathName() + " is not a groomLabShape");
    }
    return MS::kSuccess;
}

}  // namespace groomlab::maya
