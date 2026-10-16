#version 440
// SPDX-License-Identifier: Apache-2.0

// A surface in a distance pass (a light's shadow map, a deep opacity map's first layer): its
// distance along the view's axis, where its cutout leaves it (surface_lookdev.frag's cutAway:
// the cards' gaps cast no shadow).

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vTangent;
layout(location = 3) in vec2 vUv;
layout(location = 4) in vec2 vExtra;

layout(location = 0) out float fragDistance;

layout(std140, binding = 1) uniform ViewBlock {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 view;
    vec4 pixel;
    vec4 axis;
    vec4 dom;
};

// surface_lookdev.frag's Material block (the flags and the cutout read here).
layout(std140, binding = 4) uniform Material {
    vec4 base;
    vec4 params;
    vec4 flags;        // y: texture bits (1: the albedo map)
    vec4 skin;
    vec4 skin2;
    vec4 eyeCenter;
    vec4 eyeForward;
    vec4 eyeUp;
    vec4 eyeParams;
    vec4 irisColor;
    vec4 scleraColor;
    vec4 eyeParams2;
    vec4 skin3;
    vec4 cutout;       // x: the albedo alpha's cutoff (0: opaque), y: the albedo map's width in texels
};

layout(binding = 21) uniform sampler2D albedoMap;

bool cutAway(vec2 uv) {
    if (cutout.x <= 0.0 || mod(flags.y, 2.0) < 1.0) return false;
    float a = texture(albedoMap, uv).a;
    vec2 footprint = max(fwidth(uv), vec2(1e-7));
    float lod = max(log2(max(max(footprint.x, footprint.y) * cutout.y, 1e-6)), 0.0);
    vec2 cell = floor(uv / footprint);
    float noise = fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
    return a * (1.0 + 0.25 * lod) < mix(cutout.x, noise, clamp(lod, 0.0, 1.0));
}

// A little above 0 at least: 0 is what a texel nothing was drawn into holds (the readers take
// it as nothing in the way).
void main() {
    if (cutAway(vec2(vUv.x, 1.0 - vUv.y))) discard;
    fragDistance = max(dot(vPosition, axis.xyz) - axis.w, 1e-4);
}
