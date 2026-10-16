#version 440
// SPDX-License-Identifier: Apache-2.0
#extension GL_GOOGLE_include_directive : require

// Strand lines in per-vertex colors (the color modes), width as coverage: the alpha is the share
// of a pixel each point's width covers (display.h lineCoverage, at the render's density:
// lineCoverageAtDensity), which strands.frag draws hashed.

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 uv;  // u plus twice the strand's number, the point's width (StrandLines::writeUvs)

layout(location = 0) out vec4 vColor;
layout(location = 1) out float vU;
layout(location = 2) out float vOwn;  // the coverage before the density's (lineKeepShare)

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 itemColor;
    vec4 highlight;
    vec4 camera;
    vec4 light;
    vec4 viewport;  // xy the size; z the pixels a unit spans at clip w 1 (0: coverage off); w the widths' scale
    vec4 lines;     // x the preview's density the coverage is taken from (1: as it is); y the target's samples;
                    // z the level of detail's class exponent (0: none, strand_lod.h)
};

// display.h lineCoverage (no floor) at the render's density and widened by the level of detail's
// class (line_coverage.glsl lineCoverageOf: x; y the strand's own, lineKeepShare's); viewport.z 0:
// coverage off, opaque.
#include "../../../engine/gpu/shaders/line_coverage.glsl"
vec2 coverage(float width, float w) {
    return viewport.z <= 0.0 ? vec2(1.0, 1.0) : lineCoverageOf(width, w, viewport.w, viewport.z, 0.0, lines.x, lines.z);
}

void main() {
    gl_Position = mvp * vec4(position, 1.0);
    vec2 c = coverage(uv.y, gl_Position.w);
    vColor = vec4(color.rgb, c.x);
    vU = uv.x;
    vOwn = c.y;
}
