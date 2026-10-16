// SPDX-License-Identifier: Apache-2.0
#include "marker_set.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <maya/MColor.h>
#include <maya/MDagPath.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MFrameContext.h>
#include <maya/MHWGeometry.h>
#include <maya/MIntArray.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPoint.h>
#include <maya/MUIDrawManager.h>
#include <maya/MUserData.h>
#include <maya/MVectorArray.h>

#include "node_ids.h"

namespace groomlab::maya {

const MTypeId MarkerSet::kTypeId(kGroomLabMarkerSetTypeId);
const MString MarkerSet::kTypeName("groomLabMarkerSet");
const MString MarkerSet::kDrawDbClassification("drawdb/geometry/groomLabMarkerSet");
const MString MarkerSet::kRegistrantId("groomLabMarkerSetOverride");

MObject MarkerSet::aPoints;
MObject MarkerSet::aNormals;
MObject MarkerSet::aMatched;
MObject MarkerSet::aHover;
MObject MarkerSet::aColor;
MObject MarkerSet::aSize;
MObject MarkerSet::aCamera;

MStatus MarkerSet::initialize() {
    MFnTypedAttribute typed;
    MFnNumericAttribute numeric;
    MFnVectorArrayData vectors;
    MFnIntArrayData ints;
    MFnStringData strings;

    aPoints = typed.create("points", "pts", MFnData::kVectorArray, vectors.create());
    typed.setStorable(false);
    aNormals = typed.create("normals", "nrm", MFnData::kVectorArray, vectors.create());
    typed.setStorable(false);
    aMatched = typed.create("matched", "mat", MFnData::kIntArray, ints.create());
    typed.setStorable(false);
    aHover = numeric.create("hover", "hov", MFnNumericData::kInt, -1);
    numeric.setStorable(false);
    aColor = numeric.createColor("color", "col");
    numeric.setDefault(1.0f, 0.85f, 0.1f);
    aSize = numeric.create("size", "sz", MFnNumericData::kFloat, 7.0);
    numeric.setMin(1.0);
    aCamera = typed.create("camera", "cam", MFnData::kString, strings.create(""));

    for (MObject* a : {&aPoints, &aNormals, &aMatched, &aHover, &aColor, &aSize, &aCamera}) {
        const MStatus status = addAttribute(*a);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    return MS::kSuccess;
}

namespace {

struct Marker {
    MPoint point;
    MVector normal;
    bool hasNormal = false;
    bool matched = true;
};

class MarkerSetData : public MUserData {
public:
    MarkerSetData() = default;
    std::vector<Marker> markers;
    int hover = -1;
    MColor color;
    double radius = 7.0;
    bool shown = true;
};

// Whether `name` names the camera of `cameraPath` (its transform or shape, short or full).
bool namesCamera(const MString& name, const MDagPath& cameraPath) {
    MDagPath transform(cameraPath);
    if (transform.node().hasFn(MFn::kCamera)) transform.pop();
    return name == transform.partialPathName() || name == transform.fullPathName() ||
           name == cameraPath.partialPathName() || name == cameraPath.fullPathName();
}

}  // namespace

MUserData* MarkerSetDrawOverride::prepareForDraw(const MDagPath& objPath, const MDagPath& cameraPath,
                                                 const MHWRender::MFrameContext&, MUserData* oldData) {
    auto* data = dynamic_cast<MarkerSetData*>(oldData);
    if (!data) data = new MarkerSetData();
    data->markers.clear();
    try {
        const MObject node = objPath.node();
        const MString camera = MPlug(node, MarkerSet::aCamera).asString();
        data->shown = camera.length() == 0 || !cameraPath.isValid() || namesCamera(camera, cameraPath);
        MVectorArray points, normals;
        MIntArray matched;
        MObject value = MPlug(node, MarkerSet::aPoints).asMObject();
        if (!value.isNull()) points = MFnVectorArrayData(value).array();
        value = MPlug(node, MarkerSet::aNormals).asMObject();
        if (!value.isNull()) normals = MFnVectorArrayData(value).array();
        value = MPlug(node, MarkerSet::aMatched).asMObject();
        if (!value.isNull()) matched = MFnIntArrayData(value).array();
        data->markers.resize(points.length());
        for (unsigned int i = 0; i < points.length(); ++i) {
            Marker& m = data->markers[i];
            m.point = MPoint(points[i]);
            if (i < normals.length() && normals[i].length() > 1e-12) {
                m.normal = normals[i].normal();
                m.hasNormal = true;
            }
            m.matched = i >= matched.length() || matched[i] != 0;
        }
        data->hover = MPlug(node, MarkerSet::aHover).asInt();
        const MPlug color(node, MarkerSet::aColor);
        data->color = MColor(color.child(0).asFloat(), color.child(1).asFloat(), color.child(2).asFloat(), 1.0f);
        data->radius = std::max(1.0, static_cast<double>(MPlug(node, MarkerSet::aSize).asFloat()));
    } catch (...) {
        data->markers.clear();  // nothing thrown into Maya's draw
    }
    return data;
}

void MarkerSetDrawOverride::addUIDrawables(const MDagPath&, MHWRender::MUIDrawManager& draw,
                                           const MHWRender::MFrameContext& context, const MUserData* userData) {
    using MHWRender::MUIDrawManager;
    const auto* data = dynamic_cast<const MarkerSetData*>(userData);
    if (!data || !data->shown || data->markers.empty()) return;
    try {
        // The way to the camera: from the point to the eye (perspective) or against the view
        // direction (orthographic: the projection's w row is (0, 0, 0, 1)).
        const MMatrix projection = context.getMatrix(MHWRender::MFrameContext::kProjectionMtx);
        const bool ortho = std::abs(projection(3, 3) - 1.0) < 1e-9 && std::abs(projection(2, 3)) < 1e-9;
        const MDoubleArray eyeTuple = context.getTuple(MHWRender::MFrameContext::kViewPosition);
        const MDoubleArray dirTuple = context.getTuple(MHWRender::MFrameContext::kViewDirection);
        const MPoint eye = eyeTuple.length() >= 3 ? MPoint(eyeTuple[0], eyeTuple[1], eyeTuple[2]) : MPoint();
        const MVector viewDir = dirTuple.length() >= 3 ? MVector(dirTuple[0], dirTuple[1], dirTuple[2]) : MVector(0, 0, -1);

        const double r = data->radius;
        const MColor dark(0.0f, 0.0f, 0.0f, 1.0f), white(1.0f, 1.0f, 1.0f, 1.0f);
        const auto faded = [](MColor c, bool faint) {
            if (faint) c.a *= 0.35f;
            return c;
        };

        draw.beginDrawable(MUIDrawManager::kNonSelectable);
        draw.beginDrawInXray();
        draw.setDepthPriority(MHWRender::MRenderItem::sActiveWireDepthPriority + 10);
        draw.setFontSize(static_cast<unsigned int>(std::lround(std::max(9.0, r * 1.85))));
        draw.setFontWeight(MUIDrawManager::kWeightBold);
        for (std::size_t i = 0; i < data->markers.size(); ++i) {
            const Marker& m = data->markers[i];
            double x = 0.0, y = 0.0;
            context.worldToViewport(m.point, x, y);
            if (!std::isfinite(x) || !std::isfinite(y)) continue;
            if (!ortho) {
                // Behind the eye: no projection.
                if ((m.point - eye) * viewDir <= 0.0) continue;
            }
            bool faint = false;
            if (m.hasNormal) {
                const MVector toCamera = ortho ? -viewDir : (eye - m.point);
                faint = m.normal * toCamera < 0.0;
            }
            const bool lit = data->hover >= 0 && static_cast<std::size_t>(data->hover) == i;
            const MPoint at(x, y, 0.0);
            const MColor color = faded(data->color, faint);
            const MColor outline = faded(dark, faint);
            if (lit) {
                draw.setColor(faded(white, faint));
                draw.setLineWidth(2.0f);
                draw.circle2d(at, r + 4.5, 32, false);
            }
            // Nothing drawn overlaps (the draw manager may not keep the order between opaque and
            // transparent items): the dark edges sit beside the color, not under it.
            if (m.matched) {
                draw.setColor(color);
                draw.circle2d(at, r, 32, true);
                draw.setColor(outline);
                draw.setLineWidth(1.5f);
                draw.circle2d(at, r + 0.75, 32, false);
            } else {
                draw.setColor(color);
                draw.setLineWidth(2.5f);
                draw.circle2d(at, r, 32, false);
                draw.setColor(outline);
                draw.setLineWidth(1.0f);
                draw.circle2d(at, r + 1.75, 32, false);
                draw.circle2d(at, r - 1.75, 32, false);
            }
            // The number, up and right, outlined dark for any background.
            const MString number(std::to_string(i + 1).c_str());
            const MPoint label(x + r + 3.0, y + r + 1.0, 0.0);
            draw.setColor(outline);
            for (const auto& d : {std::make_pair(-1.0, 0.0), std::make_pair(1.0, 0.0), std::make_pair(0.0, -1.0),
                                  std::make_pair(0.0, 1.0), std::make_pair(-1.0, -1.0), std::make_pair(1.0, 1.0),
                                  std::make_pair(-1.0, 1.0), std::make_pair(1.0, -1.0)}) {
                draw.text2d(MPoint(label.x + d.first, label.y + d.second, 0.0), number, MUIDrawManager::kLeft);
            }
            draw.setColor(lit ? faded(white, faint) : color);
            draw.text2d(label, number, MUIDrawManager::kLeft);
        }
        draw.endDrawInXray();
        draw.endDrawable();
    } catch (...) {
    }
}

}  // namespace groomlab::maya
