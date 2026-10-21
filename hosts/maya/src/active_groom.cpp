// SPDX-License-Identifier: Apache-2.0
#include "active_groom.h"

#include <algorithm>

#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MUuid.h>

#include "groom_shape.h"

namespace groomlab::maya {

namespace {

bool isGroom(const MDagPath& path) {
    // hasFn(kPluginShape) is also true on the shape's transform, so test the node itself.
    return !path.node().hasFn(MFn::kTransform) && MFnDagNode(path).typeName() == GroomShape::kTypeName;
}

}  // namespace

bool groomInSelection(const MSelectionList& selection, MDagPath& out) {
    for (unsigned i = 0; i < selection.length(); ++i) {
        MDagPath path;
        if (selection.getDagPath(i, path) != MS::kSuccess) continue;
        if (path.node().hasFn(MFn::kTransform) && path.extendToShape() != MS::kSuccess) continue;
        if (MFnDagNode(path).typeName() == GroomShape::kTypeName) {
            out = path;
            return true;
        }
    }
    return false;
}

std::vector<MDagPath> groomsInSelection(const MSelectionList& selection, std::string* key) {
    std::vector<MDagPath> grooms;
    for (unsigned i = 0; i < selection.length(); ++i) {
        MDagPath path;
        if (selection.getDagPath(i, path) != MS::kSuccess) continue;
        if (path.node().hasFn(MFn::kTransform)) {
            for (unsigned c = 0; c < path.childCount(); ++c) {
                MDagPath child = path;
                if (child.push(path.child(c)) == MS::kSuccess && isGroom(child)) grooms.push_back(child);
            }
        } else if (isGroom(path)) {
            grooms.push_back(path);
        }
    }
    if (key) {
        std::vector<std::string> ids;
        for (const MDagPath& g : grooms) ids.emplace_back(MFnDagNode(g).uuid().asString().asChar());
        std::sort(ids.begin(), ids.end());
        ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
        key->clear();
        for (const std::string& id : ids) *key += (key->empty() ? "" : " ") + id;
    }
    return grooms;
}

bool activeGroom(MDagPath& out) {
    MSelectionList selection;
    MGlobal::getActiveSelectionList(selection);
    std::string key;
    const std::vector<MDagPath> selected = groomsInSelection(selection, &key);
    bool seenExists = false;
    const MString seen = MGlobal::optionVarStringValue("groomLabCurrentSelection", &seenExists);
    // A groom selected since the current groom was set: the selection picked it.
    if (!selected.empty() && key != std::string(seenExists ? seen.asChar() : "")) {
        out = selected.front();
        return true;
    }
    bool exists = false;
    const int pin = MGlobal::optionVarIntValue("groomLabPinGroom", &exists);
    if (exists && (pin == 1 || pin == 2)) {
        bool found = false;
        const MString name = MGlobal::optionVarStringValue("groomLabActiveGroom", &found);
        MSelectionList current;
        if (found && name.length() > 0 && current.add(name) == MS::kSuccess && groomInSelection(current, out)) return true;
    }
    if (selected.empty()) return false;
    out = selected.front();
    return true;
}

}  // namespace groomlab::maya
