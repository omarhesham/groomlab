// SPDX-License-Identifier: Apache-2.0
#include "active_groom.h"

#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>

#include "groom_shape.h"

namespace groomlab::maya {

bool groomInSelection(const MSelectionList& selection, MDagPath& out) {
    for (unsigned i = 0; i < selection.length(); ++i) {
        MDagPath path;
        if (selection.getDagPath(i, path) != MS::kSuccess) continue;
        // hasFn(kPluginShape) is also true on the shape's transform, so test the node itself.
        if (path.node().hasFn(MFn::kTransform) && path.extendToShape() != MS::kSuccess) continue;
        if (MFnDagNode(path).typeName() == GroomShape::kTypeName) {
            out = path;
            return true;
        }
    }
    return false;
}

bool activeGroom(MDagPath& out) {
    bool exists = false;
    if (MGlobal::optionVarIntValue("groomLabPinGroom", &exists) == 1 && exists) {
        const MString name = MGlobal::optionVarStringValue("groomLabActiveGroom", &exists);
        MSelectionList pinned;
        if (exists && name.length() > 0 && pinned.add(name) == MS::kSuccess && groomInSelection(pinned, out)) return true;
    }
    MSelectionList selection;
    MGlobal::getActiveSelectionList(selection);
    return groomInSelection(selection, out);
}

}  // namespace groomlab::maya
