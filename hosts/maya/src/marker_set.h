// SPDX-License-Identifier: Apache-2.0
#pragma once

// groomLabMarkerSet: one side's marker points in the Transfer Groom window
// (groomlab.ui.transfer_window), drawn the way the app's MarkerView draws them,
// in screen space (a constant size whatever the zoom):
//
//   - a matched point: a filled dot in `color` with a dark outline;
//   - an unmatched (pending) point: a hollow ring;
//   - the pair number (index + 1) up and to the right, outlined dark;
//   - the `hover` index: a white ring around it and a white number;
//   - a point whose surface normal faces away from the camera: faint (behind the mesh).
//
// Drawn in x-ray, above the meshes (a dot never sinks into the surface), unselectable and
// unbounded (no bounding box). With `camera` set, it draws only in that camera's views.

#include <maya/MPxDrawOverride.h>
#include <maya/MPxLocatorNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>

namespace groomlab::maya {

class MarkerSet : public MPxLocatorNode {
public:
    static void* creator() { return new MarkerSet(); }
    static MStatus initialize();

    bool isBounded() const override { return false; }
    bool excludeAsLocator() const override { return false; }

    static const MTypeId kTypeId;
    static const MString kTypeName;
    static const MString kDrawDbClassification;
    static const MString kRegistrantId;

    static MObject aPoints;   // vectorArray: world positions
    static MObject aNormals;  // vectorArray: the surface normal at each point (the behind test)
    static MObject aMatched;  // Int32Array: 1 matched, 0 pending
    static MObject aHover;    // int: the hovered index, -1 none
    static MObject aColor;    // float3
    static MObject aSize;     // float: dot radius in pixels
    static MObject aCamera;   // string: draw only in views through this camera ("" every view)
};

class MarkerSetDrawOverride : public MHWRender::MPxDrawOverride {
public:
    static MHWRender::MPxDrawOverride* creator(const MObject& obj) { return new MarkerSetDrawOverride(obj); }

    MHWRender::DrawAPI supportedDrawAPIs() const override { return MHWRender::kAllDevices; }
    bool hasUIDrawables() const override { return true; }
    bool isBounded(const MDagPath&, const MDagPath&) const override { return false; }
    bool disableInternalBoundingBoxDraw() const override { return true; }
    MUserData* prepareForDraw(const MDagPath& objPath, const MDagPath& cameraPath,
                              const MHWRender::MFrameContext& frameContext, MUserData* oldData) override;
    void addUIDrawables(const MDagPath& objPath, MHWRender::MUIDrawManager& draw,
                        const MHWRender::MFrameContext& frameContext, const MUserData* data) override;

private:
    explicit MarkerSetDrawOverride(const MObject& obj) : MHWRender::MPxDrawOverride(obj, nullptr, true) {}
};

}  // namespace groomlab::maya
