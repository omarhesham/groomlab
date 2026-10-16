#version 440
// SPDX-License-Identifier: Apache-2.0

// The deep opacity map's first pass: the distance of the first hair from the viewer along the
// view's axis (linear, whatever the projection), depth-tested so the nearest wins. Fragments
// thinner than a pixel still count: coverage is weighed in the second pass.

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vTangent;
layout(location = 2) in vec4 vAttr;
layout(location = 3) in vec4 vPigment;
layout(location = 4) in vec3 vColor;

layout(location = 0) out float fragDistance;

layout(std140, binding = 1) uniform ViewBlock {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 view;
    vec4 pixel;
    vec4 axis;  // xyz: the view's forward axis; w: the offset along it distances count from
    vec4 dom;
};

// A little above 0 at least: 0 is what a texel nothing was drawn into holds (the surfaces'
// shader takes it as nothing in the way).
void main() { fragDistance = max(dot(vPosition, axis.xyz) - axis.w, 1e-4); }
