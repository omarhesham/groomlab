// SPDX-License-Identifier: Apache-2.0
// GroomLab following the scalp on the GPU (line_frames.h runFollow; follow_scalp.h): strands
// evaluated on the scalp's rest pose, held on the GPU, carried to the scalp as it is now by the
// rigid motion of each root's frame, straight into the host's line points, with each strand's root
// normal on the scalp now for the lines' frames (FramesCS) or ribbons (RibbonsCS) after it. The
// CPU's carryStrands (follow_scalp.cpp rootFrameOn, RootMotion::point) and the lit lines' root
// normals (display.cpp rootNormal: the scalp's smooth normal under the root, up when unrooted) are
// the reference: the same arithmetic in the same order, so the two agree to float rounding, and a
// root whose frame is the same on both scalps (its face's corners did not move) keeps its points to
// the bit, as the CPU's.
//
// Resources (look.slang's, through lines.slang): gOffsets the strands' layout; gStrandData each
// strand's root (follow_scalp.h FollowRoot, 32 bytes: its face's corners, the edge its frame's x
// runs along or kFollowUnrooted, its barycentrics); gIn the points at rest (float3 a point); gAux0
// the rest scalp, gAux1 the scalp now (float3 points then float3 vertex normals, gFlag.z vertices
// each); gOut the points carried (float3 a point), gOut2 the root normals (float4 a strand: xyz, 0).
// A thread per point (gFlag.x threads a row of the dispatch); a strand's first point's thread writes
// its root normal.

#include "lines.slang"

struct RootFrame {
    float3 origin, x, y, z;
    bool valid;
};

// Not a NaN nor an infinity (by its bits: no compiler may assume them away).
bool finiteBits(float3 v) {
    const uint3 b = asuint(v) & 0x7f800000u;
    return b.x != 0x7f800000u && b.y != 0x7f800000u && b.z != 0x7f800000u;
}

float3 scalpPoint(ByteAddressBuffer scalp, uint v) { return asfloat(scalp.Load3(v * 12)); }
float3 scalpNormal(ByteAddressBuffer scalp, uint v) { return asfloat(scalp.Load3((gFlag.z + v) * 12)); }

// The smooth normal at the root (ScalpMesh::normal): the corners' vertex normals blended by the
// barycentrics, normalized.
float3 rootNormalOn(ByteAddressBuffer scalp, uint4 c, float u, float v) {
    const float w = 1.0f - u - v;
    return normalized(scalpNormal(scalp, c.x) * w + scalpNormal(scalp, c.y) * u + scalpNormal(scalp, c.z) * v);
}

// follow_scalp.cpp rootFrameOn (frameFrom): origin at the root, y the smooth normal, x the edge
// `c.w` of the face made perpendicular to it (anyPerpendicular of the normal where that leaves
// nothing), z = x cross y.
RootFrame rootFrameOn(ByteAddressBuffer scalp, uint4 c, float u, float v) {
    const float w = 1.0f - u - v;
    const float3 p0 = scalpPoint(scalp, c.x), p1 = scalpPoint(scalp, c.y), p2 = scalpPoint(scalp, c.z);
    RootFrame f;
    f.origin = p0 * w + p1 * u + p2 * v;
    const float3 n = rootNormalOn(scalp, c, u, v);
    const uint a = c.w == 0u ? c.x : c.w == 1u ? c.y : c.z;
    const uint b = c.w == 0u ? c.y : c.w == 1u ? c.z : c.x;
    const float3 edgeIn = scalpPoint(scalp, b) - scalpPoint(scalp, a);
    const float3 edge = edgeIn - n * dot(edgeIn, n);
    f.x = len3(edge) > 1e-9f ? normalized(edge) : anyPerpendicular(n);
    f.y = n;
    f.z = cross(f.x, n);
    f.valid = finiteBits(f.origin) && finiteBits(f.x) && finiteBits(n) && finiteBits(f.z) && len3(n) > 0.5f;
    return f;
}

bool sameFrame(RootFrame a, RootFrame b) {
    return all(a.origin == b.origin) && all(a.x == b.x) && all(a.y == b.y) && all(a.z == b.z);
}

[shader("compute")]
[numthreads(POINT_GROUP_SIZE, 1, 1)]
void FollowCS(uint3 id : SV_DispatchThreadID) {
    const uint p = id.x + id.y * gFlag.x;
    if (p >= gPointCount) return;
    uint s, begin, n;
    lineStrand(p, s, begin, n);
    const uint4 c = gStrandData.Load4(s * 32);
    const float2 uv = asfloat(gStrandData.Load2(s * 32 + 16));
    const float3 q = loadPoint(p);
    if (c.w > 2u) {  // not on the scalp: the strand stays, its root normal up
        storePoint(gOut, p, q);
        if (p == begin) gOut2.Store4(s * 16, asuint(float4(0.0f, 1.0f, 0.0f, 0.0f)));
        return;
    }
    const RootFrame from = rootFrameOn(gAux0, c, uv.x, uv.y);
    const RootFrame to = rootFrameOn(gAux1, c, uv.x, uv.y);
    if (p == begin) gOut2.Store4(s * 16, asuint(float4(to.y, 0.0f)));
    // RootMotion::still: a frame not valid on either scalp, or the same on both.
    if (!from.valid || !to.valid || sameFrame(from, to)) {
        storePoint(gOut, p, q);
        return;
    }
    const float3 d = q - from.origin;
    const float lx = dot(d, from.x), ly = dot(d, from.y), lz = dot(d, from.z);
    storePoint(gOut, p, to.origin + to.x * lx + to.y * ly + to.z * lz);
}
