#version 440
// SPDX-License-Identifier: Apache-2.0
#extension GL_GOOGLE_include_directive : require

// Kajiya-Kay hair shading on lines: a diffuse lobe from the tangent and the light, a highlight
// lobe from the tangent and the half vector (its width from HairLook's roughness, its color from
// its specular: display.h hairHighlight), roots a little darker than tips as rendered hair reads.
// The coverage drawn hashed as strands.frag draws it (display.h lineHashThreshold: a coverage
// mask under LINES_SAMPLE_MASK, else kept with lineKeepShare and blended, stretches of four pixels).

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vTangent;
layout(location = 3) in float vU;         // u plus twice the strand's number
layout(location = 4) in float vCoverage;  // width as coverage (lines_lit.vert)
layout(location = 5) in float vOwn;       // the coverage before the density's

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 highlight;
    vec4 camera;
    vec4 light;
    vec4 viewport;
    vec4 lines;  // x the preview's density; y the target's samples
};

// display.h linePcg3d, lineHashThreshold, lineKeepShare, lineSampleMask: one source with Maya's.
#include "../../../engine/gpu/shaders/line_coverage.glsl"

void main() {
    float perPixel = length(vec2(dFdx(vU), dFdy(vU)));
    float number = floor(0.5 * vU);
    uint strand = uint(max(number, 0.0));
    float u = vU - 2.0 * number;
#ifdef LINES_SAMPLE_MASK
    uint samples = uint(max(lines.y, 1.0));
    float t = lineThreshold(strand, u, perPixel, samples);
    uint mask = lineSampleMask(vCoverage, t, strand, samples);
    if (mask == 0u) discard;
    gl_SampleMask[0] = int(mask);
    float alpha = 1.0;
#else
    float t = lineThreshold(strand, u, perPixel, 1u);
    float keep = lineKeepShare(vCoverage, vOwn);
    if (keep < t) discard;
    float alpha = vCoverage / keep;
    // Of two passes (the level of detail's blended segments, lines.w): 1 the opaque fragments alone,
    // 2 the blended.
    bool opaque = vCoverage >= 0.9999 * keep;
    if (lines.w > 1.5 ? opaque : (lines.w > 0.5 && !opaque)) discard;
#endif
    vec3 tg = normalize(vTangent);
    vec3 n = normalize(vNormal);
    vec3 l = normalize(light.xyz);
    vec3 v = normalize(camera.xyz - vPosition);
    vec3 h = normalize(l + v);
    float tl = dot(tg, l);
    float th = dot(tg, h);
    float diffuse = sqrt(max(0.0, 1.0 - tl * tl));
    // Hair facing away from the light (its normal against it) still catches scattered light.
    float facing = 0.5 + 0.5 * dot(n, l);
    float spec = pow(sqrt(max(0.0, 1.0 - th * th)), highlight.w);
    float ramp = 0.65 + 0.35 * clamp(u, 0.0, 1.0);
    vec3 shaded = color.rgb * (light.w + (1.0 - light.w) * diffuse * facing) * ramp + highlight.rgb * spec;
    fragColor = vec4(shaded, color.a * alpha);
}
