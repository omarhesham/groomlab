// SPDX-License-Identifier: Apache-2.0
// GroomLab strand lines' shading frames from points already on the GPU (line_frames.h): after a
// CPU evaluation a host sends the lit lines' points alone and these write their normals and
// tangents, the frames StrandLines::writeFrames writes on the CPU (display.cpp strandFrame), from
// look.slang's own arithmetic (lineFrame: the GPU look preview's lines kernel), so the two agree
// to float rounding; and new widths alone into the lines' uvs (WidthsCS).
//
// The same resources as look.slang (it is included: its cbuffer, gOffsets, gStrandData holding
// the root normals at gTable.y, gIn the points, gOut and gOut2 the normals and tangents). No
// bitangent is written (the lit lines' shaders read none). A thread per point whatever the
// strands' counts: with gUniformCount (every strand that many points) the strand is p / n, else
// the strand holding p is found in gOffsets (a binary search). gFlag.x: the threads in a row of the
// dispatch (groups in x times POINT_GROUP_SIZE), so one dispatch of rows (groups in y) holds more
// than 65535 groups' points.

#include "look.slang"

// Point p's strand s, its first point and count n: by division with gUniformCount, else the last
// strand starting at or before p (strandBegin(lo) <= p < strandBegin(hi)), a binary search.
void lineStrand(uint p, out uint s, out uint begin, out uint n) {
    if (gUniformCount > 0u) {
        n = gUniformCount;
        s = p / n;
        begin = s * n;
        return;
    }
    uint lo = 0u, hi = gStrandCount;
    while (hi - lo > 1u) {
        const uint mid = (lo + hi) >> 1u;
        if (strandBegin(mid) <= p) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    s = lo;
    begin = strandBegin(s);
    n = strandCountOf(s);
}

[shader("compute")]
[numthreads(POINT_GROUP_SIZE, 1, 1)]
void FramesCS(uint3 id : SV_DispatchThreadID) {
    const uint p = id.x + id.y * gFlag.x;
    if (p >= gPointCount) return;
    uint s, begin, n;
    lineStrand(p, s, begin, n);
    float3 t, nrm, side;
    lineFrame(begin, n, p - begin, rootNormal(s), t, nrm, side);
    storePoint(gOut, p, nrm);
    storePoint(gOut2, p, t);
}

// The lines' widths (gIn, a float a point) into the second float of each point's uvs (gOut,
// float2: display.h StrandLines::writeUvs), the u the host wrote left alone: look.slang's
// UvWidthsCS in rows (gFlag.x threads a row), for more points than one row of groups holds.
[shader("compute")]
[numthreads(POINT_GROUP_SIZE, 1, 1)]
void WidthsCS(uint3 id : SV_DispatchThreadID) {
    const uint p = id.x + id.y * gFlag.x;
    if (p >= gPointCount) return;
    gOut.Store(p * 8 + 4, gIn.Load(p * 4));
}

// ---- Ribbons from the lines (display.cpp strandRibbons) ------------------------------------------
//
// Two vertices a point, 2p and 2p + 1: the point less and plus its frame's side (lineFrame, as the
// CPU's strandFrame) times half its width (the lines' uvs' second float, gAux1: float2 a point;
// kNoLineWidth, negative: kRibbonDefaultWidth) times the width scale (gValue.x). Lit (gFlag.y 0):
// both take the point's normal (gOut2) and tangent (gOut3); unlit (gFlag.y 1): its strand's color
// (gAux0, float4 a strand) shaded by the fixed key light (gValue.yzw, ribbonKeyLight) across the
// ribbon's face, into gOut2 (float4 a vertex). Strands of one point (which the CPU's ribbons leave
// out) make vertices no triangle uses. gFlag.x: the threads in a row of the dispatch.

[shader("compute")]
[numthreads(POINT_GROUP_SIZE, 1, 1)]
void RibbonsCS(uint3 id : SV_DispatchThreadID) {
    const uint p = id.x + id.y * gFlag.x;
    if (p >= gPointCount) return;
    uint s, begin, n;
    lineStrand(p, s, begin, n);
    float3 t, nrm, side;
    lineFrame(begin, n, p - begin, rootNormal(s), t, nrm, side);
    const float width = asfloat(gAux1.Load(p * 8 + 4));
    const float halfWidth = 0.5f * gValue.x * (width < 0.0f ? 0.01f : width);
    const float3 q = loadPoint(p);
    storePoint(gOut, 2 * p, q - side * halfWidth);
    storePoint(gOut, 2 * p + 1, q + side * halfWidth);
    if (gFlag.y == 0u) {
        storePoint(gOut2, 2 * p, nrm);
        storePoint(gOut2, 2 * p + 1, nrm);
        storePoint(gOut3, 2 * p, t);
        storePoint(gOut3, 2 * p + 1, t);
        return;
    }
    const float shade = 0.35f + 0.65f * abs(dot(normalized(cross(side, t)), gValue.yzw));
    const float3 c = asfloat(gAux0.Load3(s * 16));
    const uint4 rgba = asuint(float4(c.x * shade, c.y * shade, c.z * shade, 1.0f));
    gOut2.Store4(2 * p * 16, rgba);
    gOut2.Store4((2 * p + 1) * 16, rgba);
}

// What the ribbons' point counts alone give: their triangles (gOut: six indices a point, two
// triangles from each point to the next; past a strand's last point all six its first vertex,
// drawing nothing) and, with gFlag.y, their uvs (gOut2: float2 a vertex, u root to tip and 0.5, as
// the lit ribbons' uvCoord reads them; gFlag.y 2: the u of the lines' uvs, gAux1, which carries
// twice the strand's number too, display.h lineIdOf, for the hair shading's strand variation).
[shader("compute")]
[numthreads(POINT_GROUP_SIZE, 1, 1)]
void RibbonLayoutCS(uint3 id : SV_DispatchThreadID) {
    const uint p = id.x + id.y * gFlag.x;
    if (p >= gPointCount) return;
    uint s, begin, n;
    lineStrand(p, s, begin, n);
    const uint k = p - begin, a = 2 * p;
    if (k + 1 < n) {
        gOut.Store3(p * 24, uint3(a, a + 1, a + 2));
        gOut.Store3(p * 24 + 12, uint3(a + 1, a + 3, a + 2));
    } else {
        gOut.Store3(p * 24, uint3(a, a, a));
        gOut.Store3(p * 24 + 12, uint3(a, a, a));
    }
    if (gFlag.y != 0u) {
        const float u = gFlag.y == 2u ? asfloat(gAux1.Load(p * 8)) : n > 1 ? float(k) / float(n - 1) : 0.0f;
        gOut2.Store4(p * 16, asuint(float4(u, 0.5f, u, 0.5f)));
    }
}

// The lit ribbons' own colors: each point's (gAux0, float4 a point) on both its vertices (gOut).
[shader("compute")]
[numthreads(POINT_GROUP_SIZE, 1, 1)]
void RibbonColorsCS(uint3 id : SV_DispatchThreadID) {
    const uint p = id.x + id.y * gFlag.x;
    if (p >= gPointCount) return;
    const uint4 c = gAux0.Load4(p * 16);
    gOut.Store4(2 * p * 16, c);
    gOut.Store4((2 * p + 1) * 16, c);
}
