// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

#include <maya/MDagPath.h>
#include <maya/MPxCommand.h>
#include <maya/MPxContext.h>
#include <maya/MPxContextCommand.h>
#include <maya/MSyntax.h>

class M3dView;

namespace groomlab::maya {

// Grooming from photos (Groom Tools > Reference): a photo is a camera with an image plane
// (groomlab.reference); lining the camera up with the mesh is a solve from point pairs.

// groomLabCameraSolve: the camera that sees the world points where the image points are
// (groomlab/camera_solve.h: DLT + Levenberg-Marquardt; principal point at the image's center,
// square pixels, no distortion).
//
//   -imagePoint/-ip u v     (multi-use) pixels, origin at the image's top-left, v down
//   -worldPoint/-wp x y z   (multi-use) the same points in world space, in the same order
//   -imageSize/-is w h      the image's size in pixels (required)
//   -filmBack/-fb h v       the camera's film aperture in inches (default 1.417 x h*1.417/w)
//   -focal/-fl mm           the focal length to start from, or keep with -fixFocal
//   -fixFocal/-ff           solve the pose only (4 pairs are enough; 6 otherwise)
//   -camera/-cam name       start from this camera's pose (and its focal and film back
//                           when -focal and -filmBack are not given)
//
// Returns [tx ty tz rx ry rz focalMm rmsPx e1 ... eN]: the camera transform in world space
// (Maya's camera: looking down -Z, Y up; rotation order xyz, degrees), the focal length, the
// RMS reprojection error and each pair's error in pixels. Changes nothing in the scene.
class CameraSolveCmd : public MPxCommand {
public:
    static const char* kName;
    static void* creator();
    static MSyntax newSyntax();
    MStatus doIt(const MArgList& args) override;
};

// The pick tool of Reference > Align: a click in a view that looks through the reference
// camera marks a point on the photo (pixels on its image plane); a click in any other view
// marks a point on the mesh under the cursor (the nearest visible mesh). Each pick goes to
// groomlab.reference.add_pick, which pairs it with the first pair still missing that kind.
// While the tool is on, every pair's points draw numbered in the viewports (image points on
// the photo's plane, mesh points on the mesh), nothing is added to the scene for them.
class ReferencePickContext : public MPxContext {
public:
    ReferencePickContext();
    void getClassName(MString& name) const override;  // "groomLabReference"
    void toolOnSetup(MEvent& event) override;

    MStatus doPress(MEvent& event, MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext& ctx) override;
    MStatus drawFeedback(MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext& ctx) override;

    // Picks at view coordinates (x right, y up from the view's bottom-left) in `view`, as a
    // click would. Returns (and keeps in lastPick) "image u v", "mesh x y z" or "" (nothing
    // there), after passing the pick on to groomlab.reference.add_pick.
    std::string pick(M3dView& view, short x, short y);

    std::string camera;    // the reference camera being aligned (its transform)
    std::string lastPick;  // what the last click picked

private:
    std::string pickAt(M3dView& view, short x, short y);
};

// groomLabReferenceCtx: creates and edits reference pick contexts.
//   groomLabReferenceCtx -e -camera <reference camera> ctxName
//   groomLabReferenceCtx -e -debugClick "x y" ctxName    (tests: a click in the active view)
//   groomLabReferenceCtx -q -lastPick ctxName            "image u v", "mesh x y z" or ""
class ReferencePickContextCmd : public MPxContextCommand {
public:
    static const char* kName;
    static void* creator();
    MPxContext* makeObj() override;
    MStatus appendSyntax() override;
    MStatus doEditFlags() override;
    MStatus doQueryFlags() override;

private:
    ReferencePickContext* context_ = nullptr;
};

// The Reference Flow tool (guides from reference photos' hints): looking through a reference
// camera, a drag over the photo draws a flow stroke, root end first; Ctrl+drag (or Ctrl+click on
// one) erases the strokes it touches. Strokes are kept in the photo's pixels (they stay on the
// photo whatever the camera does later) in the groom's glReferenceViews attribute, through
// groomlab.from_reference (one undo step each), and draw as curved arrows on the image plane, as
// the Flow tool draws its strokes on the scalp. guides_from_reference reads them as hint strokes.
class ReferenceFlowContext : public MPxContext {
public:
    ReferenceFlowContext();
    void getClassName(MString& name) const override;  // "groomLabReferenceFlow"
    void toolOnSetup(MEvent& event) override;

    MStatus doPress(MEvent& event, MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext& ctx) override;
    MStatus doDrag(MEvent& event, MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext& ctx) override;
    MStatus doRelease(MEvent& event, MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext& ctx) override;
    MStatus drawFeedback(MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext& ctx) override;

    // A stroke as the mouse would draw it, through view coordinates (x right, y up from the view's
    // bottom-left) in `view`: press at the first point, drag through the rest, release. `erase`:
    // as with Ctrl held. Returns the strokes now kept for the camera (-1: not looking through it).
    int stroke(M3dView& view, const std::vector<std::pair<short, short>>& points, bool erase);
    int strokeCount() const;

    std::string camera;  // the reference camera drawn over (its transform)
    std::string groom;   // the groom keeping the strokes

private:
    bool begin(M3dView& view, short x, short y, bool erase);
    void extend(M3dView& view, short x, short y);
    void end();

    bool active_ = false, erase_ = false;
    std::vector<std::array<double, 2>> pixels_;  // the stroke being drawn, photo pixels
};

// groomLabReferenceFlowCtx: creates and edits Reference Flow contexts.
//   groomLabReferenceFlowCtx -e -camera <reference camera> -groom <groom> ctxName
//   groomLabReferenceFlowCtx -e -debugStroke "x0 y0 x1 y1 ..." [-debugErase] ctxName   (tests: view coordinates)
//   groomLabReferenceFlowCtx -q -strokeCount ctxName
class ReferenceFlowContextCmd : public MPxContextCommand {
public:
    static const char* kName;
    static void* creator();
    MPxContext* makeObj() override;
    MStatus appendSyntax() override;
    MStatus doEditFlags() override;
    MStatus doQueryFlags() override;

private:
    ReferenceFlowContext* context_ = nullptr;
};

}  // namespace groomlab::maya
