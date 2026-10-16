#version 440
// SPDX-License-Identifier: Apache-2.0

// The deep opacity map's second pass (Yuksel and Keyser 2008): each hair fragment adds its
// coverage to the layers at and beyond its depth behind the first hair (the depth map of the
// first pass), additively, so a layer holds the number of hairs between the light and that
// depth. Four layers at 1, 2, 4 and 8 thicknesses: fine near the surface where shadows show
// most, coarse deep inside. The scalp goes through here too (its own shader adds a count that
// shadows everything behind the head).

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vTangent;
layout(location = 2) in vec4 vAttr;
layout(location = 3) in vec4 vPigment;
layout(location = 4) in vec3 vColor;

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
    float count = vAttr.z;  // a thin strand covers a fraction of the texel
    fragColor = vec4(d <= t, d <= 2.0 * t, d <= 4.0 * t, d <= 8.0 * t) * count;
}
