#version 440
// SPDX-License-Identifier: Apache-2.0
#extension GL_GOOGLE_include_directive : require

// Lit hair lines: the core's shading frames per point (DisplayGeometry normals, tangents) and
// their uvs (u plus twice the strand's number, the point's width: StrandLines::writeUvs); width as
// coverage, the share of a pixel each point's width covers (display.h lineCoverage, at the
// render's density: lineCoverageAtDensity), which lines_lit.frag draws hashed.

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 tangent;
layout(location = 3) in vec2 uv;

layout(location = 0) out vec3 vPosition;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vTangent;
layout(location = 3) out float vU;
layout(location = 4) out float vCoverage;
layout(location = 5) out float vOwn;  // the coverage before the density's (lineKeepShare)

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;      // rgb, a
    vec4 highlight;  // rgb the highlight's color, w its power
    vec4 camera;     // xyz the eye, world space
    vec4 light;      // xyz toward the light (unit), w the ambient
    vec4 viewport;   // xy the size; z the pixels a unit spans at clip w 1 (0: coverage off); w the widths' scale
    vec4 lines;      // x the preview's density the coverage is taken from (1: as it is); y the target's samples
};

// display.h lineCoverage (no floor) at the render's density and widened by the level of detail's
// class (line_coverage.glsl lineCoverageOf: x; y the strand's own, lineKeepShare's); viewport.z 0:
// coverage off, opaque.
#include "../../../engine/gpu/shaders/line_coverage.glsl"
vec2 coverage(float width, float w) {
    return viewport.z <= 0.0 ? vec2(1.0, 1.0) : lineCoverageOf(width, w, viewport.w, viewport.z, 0.0, lines.x, lines.z);
}

void main() {
    vPosition = position;
    vNormal = normal;
    vTangent = tangent;
    vU = uv.x;
    gl_Position = mvp * vec4(position, 1.0);
    vec2 c = coverage(uv.y, gl_Position.w);
    vCoverage = c.x;
    vOwn = c.y;
}
