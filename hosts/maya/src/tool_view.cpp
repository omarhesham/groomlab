// SPDX-License-Identifier: Apache-2.0
#include "tool_view.h"

#include <array>
#include <cmath>

#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MMatrix.h>
#include <maya/MPoint.h>
#include <maya/MVector.h>

namespace groomlab::maya {

namespace {

// The view's world-to-clip matrix (modelView * projection) and port; invalid when unreadable.
ViewProjection matrixOf(const M3dView& view) {
    M3dView v(view);
    MMatrix modelView, projection;
    if (v.modelViewMatrix(modelView) != MS::kSuccess || v.projectionMatrix(projection) != MS::kSuccess) return {};
    const MMatrix m = modelView * projection;
    std::array<double, 16> rows{};
    for (unsigned r = 0; r < 4; ++r) {
        for (unsigned c = 0; c < 4; ++c) rows[r * 4 + c] = m(r, c);
    }
    return ViewProjection(rows, static_cast<double>(v.portWidth()), static_cast<double>(v.portHeight()));
}

bool worldToView(const M3dView& view, const Vec3f& p, short& x, short& y) {
    return M3dView(view).worldToView(MPoint(p.x, p.y, p.z), x, y);
}

}  // namespace

ToolView toolView(const M3dView& view) {
    ToolView out;
    out.hostProject = [view](const Vec3f& p, float& x, float& y) {
        short sx = 0, sy = 0;
        const bool ok = worldToView(view, p, sx, sy);
        x = sx;
        y = sy;
        return ok;
    };
    out.hostRay = [view](float x, float y, Vec3f& origin, Vec3f& direction) {
        MPoint nearPt, farPt;
        if (M3dView(view).viewToWorld(static_cast<short>(x), static_cast<short>(y), nearPt, farPt) != MS::kSuccess) return false;
        const MVector d = (farPt - nearPt).normal();
        origin = {float(nearPt.x), float(nearPt.y), float(nearPt.z)};
        direction = {float(d.x), float(d.y), float(d.z)};
        return true;
    };
    out.hostCamera = [view](Vec3f& eye, Vec3f& along, bool& ortho) {
        MDagPath camera;
        if (M3dView(view).getCamera(camera) != MS::kSuccess) return false;
        const MFnCamera fn(camera);
        ortho = fn.isOrtho();
        const MPoint e = fn.eyePoint(MSpace::kWorld);
        const MVector a = fn.viewDirection(MSpace::kWorld).normal();
        eye = {float(e.x), float(e.y), float(e.z)};
        along = {float(a.x), float(a.y), float(a.z)};
        return true;
    };
    const ViewProjection candidate = matrixOf(view);
    if (!candidate.valid()) return out;
    Vec3f nearPoint, farPoint;
    M3dView v(view);
    const float cx = static_cast<float>(v.portWidth()) * 0.5f, cy = static_cast<float>(v.portHeight()) * 0.5f;
    if (!candidate.unproject(cx, cy, nearPoint, farPoint)) return out;
    for (const float t : {0.001f, 0.3f}) {
        const Vec3f p = nearPoint + (farPoint - nearPoint) * t;
        short sx = 0, sy = 0;
        float x = 0.0f, y = 0.0f;
        if (!worldToView(view, p, sx, sy) || !candidate.project(p, x, y)) return out;
        if (std::fabs(x - sx) > 1.5f || std::fabs(y - sy) > 1.5f) return out;
    }
    out.projection = candidate;
    return out;
}

SelectionProjector selectionProjector(const M3dView& view) {
    SelectionProjector out;
    out.project = [view](const Vec3f& p, std::array<float, 2>* at) {
        short x = 0, y = 0;
        const bool visible = worldToView(view, p, x, y);
        *at = {float(x), float(y)};
        return visible;
    };
    const ViewProjection candidate = matrixOf(view);
    if (!candidate.valid()) return out;
    // The matrix answers as worldToView does (inside the view, the same whole pixels) at points
    // across the view and in depth, or it is not used.
    M3dView v(view);
    const float w = static_cast<float>(v.portWidth()), h = static_cast<float>(v.portHeight());
    for (const auto& [fx, fy] : {std::pair{0.5f, 0.5f}, std::pair{0.21f, 0.77f}, std::pair{0.83f, 0.31f}}) {
        Vec3f nearPoint, farPoint;
        if (!candidate.unproject(fx * w, fy * h, nearPoint, farPoint)) return out;
        for (const float t : {0.001f, 0.05f, 0.3f}) {
            const Vec3f p = nearPoint + (farPoint - nearPoint) * t;
            short sx = 0, sy = 0;
            int x = 0, y = 0;
            const bool maya = worldToView(view, p, sx, sy);
            if (maya != candidate.projectPixel(p, x, y) || (maya && (x != sx || y != sy))) return out;
        }
    }
    out.matrix = candidate;
    out.project = pixelProjector(candidate);
    return out;
}

}  // namespace groomlab::maya
