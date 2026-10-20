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
    const int pin = MGlobal::optionVarIntValue("groomLabPinGroom", &exists);
    auto named = [&]() {
        bool found = false;
        const MString name = MGlobal::optionVarStringValue("groomLabActiveGroom", &found);
        MSelectionList pinned;
        return found && name.length() > 0 && pinned.add(name) == MS::kSuccess && groomInSelection(pinned, out);
    };
    if (exists && pin == 1 && named()) return true;
    MSelectionList selection;
    MGlobal::getActiveSelectionList(selection);
    if (groomInSelection(selection, out)) return true;
    return exists && pin == 2 && named();  // the scene's groom, with no groom selected
}

}  // namespace groomlab::maya
