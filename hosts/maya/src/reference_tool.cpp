// SPDX-License-Identifier: Apache-2.0
#include "reference_tool.h"

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

#include <nlohmann/json.hpp>

#include <maya/M3dView.h>
#include <maya/MArgDatabase.h>
#include <maya/MArgList.h>
#include <maya/MArgParser.h>
#include <maya/MDoubleArray.h>
#include <maya/MFloatPoint.h>
#include <maya/MFloatVector.h>
#include <maya/MFnCamera.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>
#include <maya/MUIDrawManager.h>
#include <maya/MVector.h>

#include "command_args.h"
#include "groomlab/camera_solve.h"
#include "groomlab/curve.h"
#include "guarded.h"

namespace groomlab::maya {

namespace {

constexpr double kMmPerInch = 25.4;

const char* kImagePointFlag[] = {"-ip", "-imagePoint"};
const char* kWorldPointFlag[] = {"-wp", "-worldPoint"};
const char* kImageSizeFlag[] = {"-is", "-imageSize"};
const char* kFilmBackFlag[] = {"-fb", "-filmBack"};
const char* kFocalFlag[] = {"-fl", "-focal"};
const char* kFixFocalFlag[] = {"-ff", "-fixFocal"};
const char* kCameraFlag[] = {"-cam", "-camera"};
const char* kDebugClickFlag[] = {"-dbc", "-debugClick"};
const char* kLastPickFlag[] = {"-lp", "-lastPick"};

// The transform (and shape) of a camera by name.
bool findCamera(const MString& name, MDagPath& transform, MDagPath& shape) {
    MSelectionList list;
    if (name.length() == 0 || list.add(name) != MS::kSuccess || list.getDagPath(0, transform) != MS::kSuccess) return false;
    shape = transform;
    if (shape.extendToShape() != MS::kSuccess || !shape.node().hasFn(MFn::kCamera)) return false;
    // Named by its shape: up to the transform (a path's hasFn also sees a transform's shape).
    if (transform.node().hasFn(MFn::kCamera)) transform.pop();
    return true;
}

double numberAttr(const MFnDependencyNode& node, const char* name, double fallback) {
    MStatus status;
    const MPlug plug = node.findPlug(name, true, &status);
    return status ? plug.asDouble() : fallback;
}

// The film geometry a reference camera maps its photo with: the image fills the film gate
// (the Python side sets the image plane to the film back's size, which has the image's aspect).
struct PhotoCamera {
    MDagPath transform, shape;
    double width = 0.0, height = 0.0;  // pixels (glRefWidth, glRefHeight)
    double focalMm = 35.0, hfa = 1.0, vfa = 1.0, hfo = 0.0, vfo = 0.0;  // mm, inches
    double depth = 100.0;  // the image plane's distance from the eye
    MMatrix world, worldInverse;

    bool load(const std::string& name) {
        if (!findCamera(name.c_str(), transform, shape)) return false;
        const MFnDependencyNode node(transform.node());
        width = numberAttr(node, "glRefWidth", 0.0);
        height = numberAttr(node, "glRefHeight", 0.0);
        if (!(width > 0 && height > 0)) return false;
        const MFnCamera cam(shape);
        focalMm = cam.focalLength();
        hfa = cam.horizontalFilmAperture();
        vfa = cam.verticalFilmAperture();
        hfo = cam.horizontalFilmOffset();
        vfo = cam.verticalFilmOffset();
        // The image plane plugged into the camera (camera.imagePlane[0] <- imagePlaneShape.message).
        MPlug planes = MFnDependencyNode(shape.node()).findPlug("imagePlane", true);
        if (!planes.isNull() && planes.numConnectedElements() > 0) {
            MPlugArray sources;
            planes.connectionByPhysicalIndex(0).connectedTo(sources, true, false);
            if (sources.length() > 0) depth = numberAttr(MFnDependencyNode(sources[0].node()), "depth", depth);
        }
        world = transform.inclusiveMatrix();
        worldInverse = transform.inclusiveMatrixInverse();
        return true;
    }

    // A point in camera space -> photo pixels (origin top-left, v down); false behind the eye.
    bool toPixels(const MPoint& cameraSpace, double& u, double& v) const {
        if (cameraSpace.z >= -1e-9) return false;
        const double xIn = focalMm * cameraSpace.x / -cameraSpace.z / kMmPerInch;
        const double yIn = focalMm * cameraSpace.y / -cameraSpace.z / kMmPerInch;
        u = ((xIn - hfo) / hfa + 0.5) * width;
        v = (0.5 - (yIn - vfo) / vfa) * height;
        return true;
    }

    // Photo pixels -> the world point on the image plane.
    MPoint onImagePlane(double u, double v) const {
        const double xIn = (u / width - 0.5) * hfa + hfo;
        const double yIn = (0.5 - v / height) * vfa + vfo;
        const double k = depth / (focalMm / kMmPerInch);
        return MPoint(xIn * k, yIn * k, -depth) * world;
    }
};

std::string pythonString(const std::string& s) {
    std::string out = "'";
    for (char c : s) {
        if (c == '\\' || c == '\'') out += '\\';
        out += c;
    }
    return out + "'";
}

}  // namespace

// ---------------------------------------------------------------------------
// groomLabCameraSolve

const char* CameraSolveCmd::kName = "groomLabCameraSolve";

void* CameraSolveCmd::creator() { return new CameraSolveCmd(); }

MSyntax CameraSolveCmd::newSyntax() {
    MSyntax s;
    s.addFlag(kImagePointFlag[0], kImagePointFlag[1], MSyntax::kDouble, MSyntax::kDouble);
    s.makeFlagMultiUse(kImagePointFlag[0]);
    s.addFlag(kWorldPointFlag[0], kWorldPointFlag[1], MSyntax::kDouble, MSyntax::kDouble, MSyntax::kDouble);
    s.makeFlagMultiUse(kWorldPointFlag[0]);
    s.addFlag(kImageSizeFlag[0], kImageSizeFlag[1], MSyntax::kDouble, MSyntax::kDouble);
    s.addFlag(kFilmBackFlag[0], kFilmBackFlag[1], MSyntax::kDouble, MSyntax::kDouble);
    s.addFlag(kFocalFlag[0], kFocalFlag[1], MSyntax::kDouble);
    s.addFlag(kFixFocalFlag[0], kFixFocalFlag[1]);
    s.addFlag(kCameraFlag[0], kCameraFlag[1], MSyntax::kString);
    s.enableQuery(false);
    s.enableEdit(false);
    return s;
}

MStatus CameraSolveCmd::doIt(const MArgList& args) {
    return guarded(kName, [&]() -> MStatus {
        MStatus status;
        MArgDatabase db(syntax(), args, &status);
        if (!status) return status;
        std::vector<CameraCorrespondence> pairs;
        const unsigned images = db.numberOfFlagUses(kImagePointFlag[0]);
        const unsigned worlds = db.numberOfFlagUses(kWorldPointFlag[0]);
        if (images != worlds) {
            return fail(kName, MString("give as many -imagePoint as -worldPoint (") + images + " and " + worlds + ")");
        }
        for (unsigned i = 0; i < images; ++i) {
            MArgList ip, wp;
            db.getFlagArgumentList(kImagePointFlag[0], i, ip);
            db.getFlagArgumentList(kWorldPointFlag[0], i, wp);
            CameraCorrespondence c;
            c.u = ip.asDouble(0);
            c.v = ip.asDouble(1);
            c.world = {wp.asDouble(0), wp.asDouble(1), wp.asDouble(2)};
            pairs.push_back(c);
        }
        if (!db.isFlagSet(kImageSizeFlag[0])) return fail(kName, "give -imageSize w h (pixels)");
        CameraSolveOptions options;
        db.getFlagArgument(kImageSizeFlag[0], 0, options.imageWidth);
        db.getFlagArgument(kImageSizeFlag[0], 1, options.imageHeight);
        if (!(options.imageWidth > 0 && options.imageHeight > 0)) return fail(kName, "-imageSize must be positive");

        double hfa = 36.0 / kMmPerInch, focalMm = 35.0;
        bool haveFocal = false;
        if (db.isFlagSet(kCameraFlag[0])) {
            MString name;
            db.getFlagArgument(kCameraFlag[0], 0, name);
            MDagPath transform, shape;
            if (!findCamera(name, transform, shape)) return fail(kName, name + " is not a camera");
            const MFnCamera cam(shape);
            hfa = cam.horizontalFilmAperture();
            focalMm = cam.focalLength();
            haveFocal = true;
            // Its pose to start from: camera axes and eye in world space.
            const MMatrix m = transform.inclusiveMatrix();  // rows: X, Y, Z axes, then the position
            options.hasInitialPose = true;
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < 3; ++c) options.initialPose.rotation[r][c] = m(c, r);
            options.initialPose.position = {m(3, 0), m(3, 1), m(3, 2)};
        }
        if (db.isFlagSet(kFilmBackFlag[0])) {
            db.getFlagArgument(kFilmBackFlag[0], 0, hfa);
        }
        if (db.isFlagSet(kFocalFlag[0])) {
            db.getFlagArgument(kFocalFlag[0], 0, focalMm);
            haveFocal = true;
        }
        if (!(hfa > 0)) return fail(kName, "the film back must be positive");
        options.fixFocal = db.isFlagSet(kFixFocalFlag[0]);
        if (options.fixFocal && !haveFocal) return fail(kName, "-fixFocal needs -focal (or -camera) to keep");
        options.focalPx = haveFocal ? focalMmToPx(focalMm, options.imageWidth, hfa) : 0.0;

        const CameraSolveResult result = solveCamera(pairs, options);
        if (!result.ok) return fail(kName, result.error.c_str());
        const CameraVec3 rotate = eulerXYZDegrees(result.pose.rotation);
        MDoubleArray out;
        for (double v : result.pose.position) out.append(v);
        for (double v : rotate) out.append(v);
        out.append(focalPxToMm(result.focalPx, options.imageWidth, hfa));
        out.append(result.rmsPx);
        for (double e : result.errorsPx) out.append(e);
        setResult(out);
        return MS::kSuccess;
    });
}

// ---------------------------------------------------------------------------
// The pick tool

ReferencePickContext::ReferencePickContext() { setTitleString("GroomLab Reference Points"); }

void ReferencePickContext::getClassName(MString& name) const { name = "groomLabReference"; }

void ReferencePickContext::toolOnSetup(MEvent&) {
    setHelpString("Looking through the photo's camera, click a feature on the photo; in another view, click the same "
                  "feature on the mesh. Pairs fill in order.");
}

MStatus ReferencePickContext::doPress(MEvent& event, MHWRender::MUIDrawManager&, const MHWRender::MFrameContext&) {
    return guarded("ReferencePickContext::doPress", [&]() -> MStatus {
        if (event.mouseButton() != MEvent::kLeftMouse) return MS::kSuccess;
        short x = 0, y = 0;
        event.getPosition(x, y);
        M3dView view = M3dView::active3dView();
        if (pick(view, x, y).empty()) MGlobal::displayWarning("GroomLab reference: nothing under the cursor to pick");
        return MS::kSuccess;
    });
}

std::string ReferencePickContext::pick(M3dView& view, short x, short y) {
    lastPick = pickAt(view, x, y);
    return lastPick;
}

std::string ReferencePickContext::pickAt(M3dView& view, short x, short y) {
    PhotoCamera photo;
    if (!photo.load(camera)) {
        MGlobal::displayWarning(MString("GroomLab reference: ") + camera.c_str() + " is not a reference camera");
        return {};
    }
    MPoint nearPt, farPt;
    if (view.viewToWorld(x, y, nearPt, farPt) != MS::kSuccess) return {};
    MDagPath viewCamera;
    view.getCamera(viewCamera);
    if (viewCamera.node() == photo.shape.node()) {
        // Looking through the photo: the point on it under the cursor.
        double u = 0, v = 0;
        if (!photo.toPixels(farPt * photo.worldInverse, u, v)) return {};
        if (u < 0 || v < 0 || u > photo.width || v > photo.height) {
            MGlobal::displayWarning("GroomLab reference: click on the photo (that is outside it)");
            return {};
        }
        char buffer[160];
        std::snprintf(buffer, sizeof buffer, "(%.6f, %.6f)", u, v);
        MGlobal::executePythonCommand(MString("import groomlab.reference as _ref; _ref.add_pick(") +
                                          pythonString(photo.transform.fullPathName().asChar()).c_str() + ", 'image', " +
                                          buffer + ")",
                                      false, true);
        M3dView::scheduleRefreshAllViews();
        std::snprintf(buffer, sizeof buffer, "image %.6f %.6f", u, v);
        return buffer;
    }
    // Any other view: the nearest visible mesh under the cursor.
    const MVector dir = (farPt - nearPt).normal();
    const MFloatPoint source(float(nearPt.x), float(nearPt.y), float(nearPt.z));
    const MFloatVector direction(float(dir.x), float(dir.y), float(dir.z));
    const float reach = float((farPt - nearPt).length());
    float best = std::numeric_limits<float>::max();
    MFloatPoint bestPoint;
    bool found = false;
    for (MItDag it(MItDag::kDepthFirst, MFn::kMesh); !it.isDone(); it.next()) {
        MDagPath path;
        if (it.getPath(path) != MS::kSuccess || !path.isVisible()) continue;
        MFnMesh mesh(path);
        if (mesh.isIntermediateObject()) continue;
        MFloatPoint hit;
        float param = 0.0f;
        MMeshIsectAccelParams accel = mesh.autoUniformGridParams();
        if (mesh.closestIntersection(source, direction, nullptr, nullptr, false, MSpace::kWorld, reach, false, &accel,
                                     hit, &param, nullptr, nullptr, nullptr, nullptr) &&
            param < best) {
            best = param;
            bestPoint = hit;
            found = true;
        }
    }
    if (!found) return {};
    char buffer[200];
    std::snprintf(buffer, sizeof buffer, "(%.7g, %.7g, %.7g)", bestPoint.x, bestPoint.y, bestPoint.z);
    MGlobal::executePythonCommand(MString("import groomlab.reference as _ref; _ref.add_pick(") +
                                      pythonString(photo.transform.fullPathName().asChar()).c_str() + ", 'mesh', " +
                                      buffer + ")",
                                  false, true);
    M3dView::scheduleRefreshAllViews();
    std::snprintf(buffer, sizeof buffer, "mesh %.7g %.7g %.7g", bestPoint.x, bestPoint.y, bestPoint.z);
    return buffer;
}

MStatus ReferencePickContext::drawFeedback(MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext&) {
    return guarded("ReferencePickContext::drawFeedback", [&]() -> MStatus {
        PhotoCamera photo;
        if (!photo.load(camera)) return MS::kSuccess;
        const MFnDependencyNode node(photo.transform.node());
        MStatus status;
        const MPlug plug = node.findPlug("glRefPairs", true, &status);
        if (!status) return MS::kSuccess;
        const std::string text = plug.asString().asChar();
        nlohmann::json pairs;
        try {
            pairs = nlohmann::json::parse(text.empty() ? "[]" : text);
        } catch (const nlohmann::json::exception&) {
            return MS::kSuccess;
        }
        if (!pairs.is_array()) return MS::kSuccess;
        draw.beginDrawable(MUIDrawManager::kNonSelectable);
        draw.beginDrawInXray();
        draw.setPointSize(9.0f);
        draw.setFontSize(MUIDrawManager::kDefaultFontSize);
        const MColor imageColor(1.0f, 0.78f, 0.2f), meshColor(0.25f, 0.85f, 1.0f);
        for (std::size_t i = 0; i < pairs.size(); ++i) {
            const auto& pair = pairs[i];
            const MString label = MString(" ") + static_cast<int>(i + 1);
            const auto image = pair.find("image");
            if (image != pair.end() && image->is_array() && image->size() == 2) {
                const MPoint p = photo.onImagePlane((*image)[0].get<double>(), (*image)[1].get<double>());
                draw.setColor(imageColor);
                draw.point(p);
                draw.text(p, label, MUIDrawManager::kLeft);
            }
            const auto mesh = pair.find("mesh");
            if (mesh != pair.end() && mesh->is_array() && mesh->size() == 3) {
                const MPoint p((*mesh)[0].get<double>(), (*mesh)[1].get<double>(), (*mesh)[2].get<double>());
                draw.setColor(meshColor);
                draw.point(p);
                draw.text(p, label, MUIDrawManager::kLeft);
            }
        }
        draw.endDrawInXray();
        draw.endDrawable();
        return MS::kSuccess;
    });
}

// ---------------------------------------------------------------------------

const char* ReferencePickContextCmd::kName = "groomLabReferenceCtx";

void* ReferencePickContextCmd::creator() { return new ReferencePickContextCmd(); }

MPxContext* ReferencePickContextCmd::makeObj() {
    context_ = new ReferencePickContext();
    return context_;
}

MStatus ReferencePickContextCmd::appendSyntax() {
    MSyntax s = syntax();
    s.addFlag(kCameraFlag[0], kCameraFlag[1], MSyntax::kString);
    s.addFlag(kDebugClickFlag[0], kDebugClickFlag[1], MSyntax::kString);
    s.addFlag(kLastPickFlag[0], kLastPickFlag[1]);
    return MS::kSuccess;
}

MStatus ReferencePickContextCmd::doEditFlags() {
    return guarded("ReferencePickContextCmd::doEditFlags", [&]() -> MStatus {
        if (!context_) return MS::kFailure;
        MArgParser args = parser();
        MString text;
        if (args.isFlagSet(kCameraFlag[0]) && args.getFlagArgument(kCameraFlag[0], 0, text)) context_->camera = text.asChar();
        if (args.isFlagSet(kDebugClickFlag[0]) && args.getFlagArgument(kDebugClickFlag[0], 0, text)) {
            MStringArray words;
            text.split(' ', words);
            if (words.length() < 2) return fail(kName, "-debugClick takes \"x y\" (view coordinates)");
            M3dView view = M3dView::active3dView();
            context_->pick(view, short(words[0].asInt()), short(words[1].asInt()));
        }
        return MS::kSuccess;
    });
}

MStatus ReferencePickContextCmd::doQueryFlags() {
    return guarded("ReferencePickContextCmd::doQueryFlags", [&]() -> MStatus {
        if (!context_) return MS::kFailure;
        MArgParser args = parser();
        if (args.isFlagSet(kCameraFlag[0])) setResult(MString(context_->camera.c_str()));
        else if (args.isFlagSet(kLastPickFlag[0])) setResult(MString(context_->lastPick.c_str()));
        return MS::kSuccess;
    });
}

// ---------------------------------------------------------------------------
// The Reference Flow tool

namespace {

const char* kGroomFlag[] = {"-g", "-groom"};
const char* kDebugStrokeFlag[] = {"-dbs", "-debugStroke"};
const char* kDebugEraseFlag[] = {"-dbe", "-debugErase"};
const char* kStrokeCountFlag[] = {"-sc", "-strokeCount"};

// The strokes the groom keeps for a camera (glReferenceViews: {"views": [{"camera", "strokes":
// [{"points": [[u, v], ...], "directed"}]}]}), matched by the camera's transform's full path or name.
std::vector<std::vector<std::array<double, 2>>> keptStrokes(const std::string& groom, const PhotoCamera& photo) {
    std::vector<std::vector<std::array<double, 2>>> out;
    MSelectionList list;
    MObject node;
    if (groom.empty() || list.add(groom.c_str()) != MS::kSuccess || list.getDependNode(0, node) != MS::kSuccess) return out;
    MDagPath path;
    if (list.getDagPath(0, path) == MS::kSuccess && path.extendToShape() == MS::kSuccess) node = path.node();
    MStatus status;
    const MPlug plug = MFnDependencyNode(node).findPlug("glReferenceViews", true, &status);
    if (!status) return out;
    nlohmann::json data;
    try {
        const std::string text = plug.asString().asChar();
        data = nlohmann::json::parse(text.empty() ? "{}" : text);
    } catch (const nlohmann::json::exception&) {
        return out;
    }
    const std::string full = photo.transform.fullPathName().asChar(), part = photo.transform.partialPathName().asChar();
    const auto views = data.find("views");
    if (views == data.end() || !views->is_array()) return out;
    for (const auto& view : *views) {
        const std::string cam = view.value("camera", std::string());
        if (cam != full && cam != part && ("|" + cam) != full) continue;
        const auto strokes = view.find("strokes");
        if (strokes == view.end() || !strokes->is_array()) continue;
        for (const auto& s : *strokes) {
            const auto points = s.find("points");
            if (points == s.end() || !points->is_array()) continue;
            std::vector<std::array<double, 2>> stroke;
            for (const auto& p : *points)
                if (p.is_array() && p.size() >= 2) stroke.push_back({p[0].get<double>(), p[1].get<double>()});
            out.push_back(std::move(stroke));
        }
    }
    return out;
}

std::string pixelsList(const std::vector<std::array<double, 2>>& pixels) {
    std::string out = "[";
    char buffer[64];
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        std::snprintf(buffer, sizeof buffer, "%s(%.3f, %.3f)", i ? ", " : "", pixels[i][0], pixels[i][1]);
        out += buffer;
    }
    return out + "]";
}

// A stroke in photo pixels drawn on the image plane as the Flow tool draws its strokes: one smooth
// curve, an arrowhead at the tip.
void drawArrow(MHWRender::MUIDrawManager& draw, const PhotoCamera& photo, const std::vector<std::array<double, 2>>& pixels,
               bool head) {
    if (pixels.size() < 2) return;
    std::vector<Vec3f> run;
    for (const auto& p : pixels) run.push_back(Vec3f(static_cast<float>(p[0]), static_cast<float>(p[1]), 0.0f));
    const std::vector<Vec3f> smooth = pixels.size() > 2 ? smoothPolyline(run.data(), run.size(), 4) : run;
    MPointArray line;
    for (const Vec3f& p : smooth) line.append(photo.onImagePlane(p.x, p.y));
    draw.mesh(MHWRender::MUIDrawManager::kLineStrip, line);
    if (!head) return;
    // The head: a fortieth of the photo's width, along the stroke's last stretch.
    const Vec3f tip = smooth.back();
    Vec3f back = smooth[smooth.size() >= 4 ? smooth.size() - 4 : 0];
    Vec3f dir = tip - back;
    if (dir.length() < 1e-6f) return;
    dir = dir.normalized();
    const float size = static_cast<float>(photo.width) / 40.0f;
    const Vec3f side(-dir.y * 0.6f * size, dir.x * 0.6f * size, 0.0f);
    const Vec3f base = tip - dir * size;
    MPointArray arrow;
    arrow.append(photo.onImagePlane(base.x + side.x, base.y + side.y));
    arrow.append(photo.onImagePlane(tip.x, tip.y));
    arrow.append(photo.onImagePlane(base.x - side.x, base.y - side.y));
    draw.mesh(MHWRender::MUIDrawManager::kLineStrip, arrow);
}

}  // namespace

ReferenceFlowContext::ReferenceFlowContext() { setTitleString("GroomLab Reference Flow"); }

void ReferenceFlowContext::getClassName(MString& name) const { name = "groomLabReferenceFlow"; }

void ReferenceFlowContext::toolOnSetup(MEvent&) {
    setHelpString("Looking through the photo's camera, drag along the hair from root to tip; Ctrl+drag or Ctrl+click erases.");
}

bool ReferenceFlowContext::begin(M3dView& view, short x, short y, bool erase) {
    active_ = false;
    pixels_.clear();
    PhotoCamera photo;
    if (!photo.load(camera)) {
        MGlobal::displayWarning(MString("GroomLab reference flow: ") + camera.c_str() + " is not a reference camera");
        return false;
    }
    MDagPath viewCamera;
    view.getCamera(viewCamera);
    if (viewCamera.node() != photo.shape.node()) {
        MGlobal::displayWarning("GroomLab reference flow: look through the reference camera to draw on its photo");
        return false;
    }
    active_ = true;
    erase_ = erase;
    extend(view, x, y);
    return true;
}

void ReferenceFlowContext::extend(M3dView& view, short x, short y) {
    if (!active_) return;
    PhotoCamera photo;
    if (!photo.load(camera)) return;
    MPoint nearPt, farPt;
    if (view.viewToWorld(x, y, nearPt, farPt) != MS::kSuccess) return;
    double u = 0, v = 0;
    if (!photo.toPixels(farPt * photo.worldInverse, u, v)) return;
    // Points at least a three-hundredth of the photo apart.
    if (!pixels_.empty()) {
        const double du = u - pixels_.back()[0], dv = v - pixels_.back()[1];
        if (std::sqrt(du * du + dv * dv) < photo.width / 300.0) return;
    }
    pixels_.push_back({u, v});
}

void ReferenceFlowContext::end() {
    if (!active_) return;
    active_ = false;
    PhotoCamera photo;
    if (!photo.load(camera)) return;
    const std::string cam = pythonString(photo.transform.fullPathName().asChar()), owner = pythonString(groom);
    if (erase_) {
        if (!pixels_.empty())
            MGlobal::executePythonCommand(MString("import groomlab.from_reference as _fr; _fr.erase_reference_strokes(") +
                                              owner.c_str() + ", " + cam.c_str() + ", " + pixelsList(pixels_).c_str() + ")",
                                          false, true);
    } else {
        double length = 0.0;
        for (std::size_t i = 1; i < pixels_.size(); ++i)
            length += std::hypot(pixels_[i][0] - pixels_[i - 1][0], pixels_[i][1] - pixels_[i - 1][1]);
        if (pixels_.size() >= 2 && length >= photo.width / 100.0)
            MGlobal::executePythonCommand(MString("import groomlab.from_reference as _fr; _fr.add_reference_stroke(") +
                                              owner.c_str() + ", " + cam.c_str() + ", " + pixelsList(pixels_).c_str() + ")",
                                          false, true);
    }
    pixels_.clear();
    M3dView::scheduleRefreshAllViews();
}

MStatus ReferenceFlowContext::doPress(MEvent& event, MHWRender::MUIDrawManager&, const MHWRender::MFrameContext&) {
    return guarded("ReferenceFlowContext::doPress", [&]() -> MStatus {
        if (event.mouseButton() != MEvent::kLeftMouse) return MS::kSuccess;
        short x = 0, y = 0;
        event.getPosition(x, y);
        M3dView view = M3dView::active3dView();
        begin(view, x, y, event.isModifierControl());
        return MS::kSuccess;
    });
}

MStatus ReferenceFlowContext::doDrag(MEvent& event, MHWRender::MUIDrawManager&, const MHWRender::MFrameContext&) {
    return guarded("ReferenceFlowContext::doDrag", [&]() -> MStatus {
        short x = 0, y = 0;
        event.getPosition(x, y);
        M3dView view = M3dView::active3dView();
        extend(view, x, y);
        view.refresh(false, true);
        return MS::kSuccess;
    });
}

MStatus ReferenceFlowContext::doRelease(MEvent&, MHWRender::MUIDrawManager&, const MHWRender::MFrameContext&) {
    return guarded("ReferenceFlowContext::doRelease", [&]() -> MStatus {
        end();
        return MS::kSuccess;
    });
}

int ReferenceFlowContext::stroke(M3dView& view, const std::vector<std::pair<short, short>>& points, bool erase) {
    if (points.empty() || !begin(view, points[0].first, points[0].second, erase)) return -1;
    for (std::size_t i = 1; i < points.size(); ++i) extend(view, points[i].first, points[i].second);
    end();
    return strokeCount();
}

int ReferenceFlowContext::strokeCount() const {
    PhotoCamera photo;
    if (!photo.load(camera)) return 0;
    return static_cast<int>(keptStrokes(groom, photo).size());
}

MStatus ReferenceFlowContext::drawFeedback(MHWRender::MUIDrawManager& draw, const MHWRender::MFrameContext&) {
    return guarded("ReferenceFlowContext::drawFeedback", [&]() -> MStatus {
        PhotoCamera photo;
        if (!photo.load(camera)) return MS::kSuccess;
        draw.beginDrawable(MUIDrawManager::kNonSelectable);
        draw.beginDrawInXray();
        draw.setLineWidth(2.0f);
        draw.setColor(MColor(1.0f, 0.62f, 0.15f));
        for (const auto& s : keptStrokes(groom, photo)) drawArrow(draw, photo, s, true);
        if (active_ && pixels_.size() >= 2) {
            draw.setColor(erase_ ? MColor(1.0f, 0.3f, 0.3f) : MColor(1.0f, 0.9f, 0.4f));
            drawArrow(draw, photo, pixels_, !erase_);
        }
        draw.endDrawInXray();
        draw.endDrawable();
        return MS::kSuccess;
    });
}

const char* ReferenceFlowContextCmd::kName = "groomLabReferenceFlowCtx";

void* ReferenceFlowContextCmd::creator() { return new ReferenceFlowContextCmd(); }

MPxContext* ReferenceFlowContextCmd::makeObj() {
    context_ = new ReferenceFlowContext();
    return context_;
}

MStatus ReferenceFlowContextCmd::appendSyntax() {
    MSyntax s = syntax();
    s.addFlag(kCameraFlag[0], kCameraFlag[1], MSyntax::kString);
    s.addFlag(kGroomFlag[0], kGroomFlag[1], MSyntax::kString);
    s.addFlag(kDebugStrokeFlag[0], kDebugStrokeFlag[1], MSyntax::kString);
    s.addFlag(kDebugEraseFlag[0], kDebugEraseFlag[1]);
    s.addFlag(kStrokeCountFlag[0], kStrokeCountFlag[1]);
    return MS::kSuccess;
}

MStatus ReferenceFlowContextCmd::doEditFlags() {
    return guarded("ReferenceFlowContextCmd::doEditFlags", [&]() -> MStatus {
        if (!context_) return MS::kFailure;
        MArgParser args = parser();
        MString text;
        if (args.isFlagSet(kCameraFlag[0]) && args.getFlagArgument(kCameraFlag[0], 0, text)) context_->camera = text.asChar();
        if (args.isFlagSet(kGroomFlag[0]) && args.getFlagArgument(kGroomFlag[0], 0, text)) context_->groom = text.asChar();
        if (args.isFlagSet(kDebugStrokeFlag[0]) && args.getFlagArgument(kDebugStrokeFlag[0], 0, text)) {
            MStringArray words;
            text.split(' ', words);
            if (words.length() < 2 || words.length() % 2) return fail(kName, "-debugStroke takes \"x0 y0 x1 y1 ...\" (view coordinates)");
            std::vector<std::pair<short, short>> points;
            for (unsigned i = 0; i + 1 < words.length(); i += 2) points.push_back({short(words[i].asInt()), short(words[i + 1].asInt())});
            M3dView view = M3dView::active3dView();
            context_->stroke(view, points, args.isFlagSet(kDebugEraseFlag[0]));
        }
        return MS::kSuccess;
    });
}

MStatus ReferenceFlowContextCmd::doQueryFlags() {
    return guarded("ReferenceFlowContextCmd::doQueryFlags", [&]() -> MStatus {
        if (!context_) return MS::kFailure;
        MArgParser args = parser();
        if (args.isFlagSet(kCameraFlag[0])) setResult(MString(context_->camera.c_str()));
        else if (args.isFlagSet(kGroomFlag[0])) setResult(MString(context_->groom.c_str()));
        else if (args.isFlagSet(kStrokeCountFlag[0])) setResult(context_->strokeCount());
        return MS::kSuccess;
    });
}

}  // namespace groomlab::maya
