#version 440
// SPDX-License-Identifier: Apache-2.0
#extension GL_GOOGLE_include_directive : require

// The flat strand lines' coverage drawn hashed (display.h lineHashThreshold, Wyman and McGuire's
// hashed alpha testing): the fragment kept where its coverage reaches a threshold hashed from the
// strand (the uvs' number) and the pixel-long stretch of it the pixel lies on, so the strands
// crossing a pixel each cover it on their own and the pattern stays on the strands as the camera
// moves. Under LINES_SAMPLE_MASK (the multisampled target's pipeline) the fragment covers
// coverage x samples of the pixel's samples, a run of them turned by the strand's hash
// (lineSampleMask); else it is kept with a share of the coverage (lineKeepShare: the coverage
// itself while the strand's own, before the density's, is low; every fragment once it reaches
// kLineBlendedAbove) and blended with coverage / share, its thresholds' stretches about four pixels
// long (lineThreshold on one sample: short dashes rather than scattered dots).

layout(location = 0) in vec4 vColor;  // rgb; a: the coverage
layout(location = 1) in float vU;     // u plus twice the strand's number
layout(location = 2) in float vOwn;   // the coverage before the density's

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 highlight;
    vec4 camera;
    vec4 light;
    vec4 viewport;
    vec4 lines;  // x the preview's density; y the target's samples; z the level of detail's class; w the pass
};

// display.h linePcg3d, lineHashThreshold, lineKeepShare, lineSampleMask: one source with Maya's.
#include "../../../engine/gpu/shaders/line_coverage.glsl"

void main() {
    float perPixel = length(vec2(dFdx(vU), dFdy(vU)));
    float number = floor(0.5 * vU);
    uint strand = uint(max(number, 0.0));
    float coverage = vColor.a;
#ifdef LINES_SAMPLE_MASK
    uint samples = uint(max(lines.y, 1.0));
    float t = lineThreshold(strand, vU - 2.0 * number, perPixel, samples);
    uint mask = lineSampleMask(coverage, t, strand, samples);
    if (mask == 0u) discard;
    gl_SampleMask[0] = int(mask);
    float alpha = 1.0;
#else
    float t = lineThreshold(strand, vU - 2.0 * number, perPixel, 1u);
    float keep = lineKeepShare(coverage, vOwn);
    if (keep < t) discard;
    float alpha = coverage / keep;
    // Of two passes (the level of detail's blended segments, lines.w): 1 the opaque fragments alone,
    // 2 the blended.
    bool opaque = coverage >= 0.9999 * keep;
    if (lines.w > 1.5 ? opaque : (lines.w > 0.5 && !opaque)) discard;
#endif
    fragColor = vec4(vColor.rgb * color.rgb, color.a * alpha);
}
