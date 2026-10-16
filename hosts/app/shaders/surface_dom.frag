#version 440
// SPDX-License-Identifier: Apache-2.0

// A surface in a deep opacity map: as many hairs as make it opaque, in the layers at and
// beyond its depth behind the first hair (as hair_dom.frag counts a hair), so the hair behind
// the surface (and the surface itself, where it faces away) is in its shadow and the hair in
// front of it is not.

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec4 vTangent;
layout(location = 3) in vec2 vUv;
layout(location = 4) in vec2 vExtra;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 1) uniform ViewBlock {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 view;
    vec4 pixel;
    vec4 axis;
    vec4 dom;
};

layout(binding = 5) uniform sampler2D frontDepth;

void main() {
    vec4 lp = viewProj * vec4(vPosition, 1.0);
    vec2 uv = lp.xy / lp.w * 0.5 + 0.5;
    if (dom.w > 0.5) uv.y = 1.0 - uv.y;
    float front = texture(frontDepth, uv).r;
    float d = dot(vPosition, axis.xyz) - axis.w - front;
    float t = dom.x;
    fragColor = vec4(d <= t, d <= 2.0 * t, d <= 4.0 * t, d <= 8.0 * t) * dom.z;
}
