#version 440
// SPDX-License-Identifier: Apache-2.0

// Guide point dots: each dot is a small diamond facing the camera at a constant size in pixels,
// four vertices sharing the point's world position and carrying their own offset in pixels, so
// the dots follow the camera without the host rebuilding them.

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 offset;
layout(location = 2) in vec4 color;

layout(location = 0) out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 itemColor;
    vec4 highlight;
    vec4 camera;
    vec4 light;
    vec4 viewport;  // the target's size in pixels (x, y)
};

void main() {
    vColor = color;
    vec4 clip = mvp * vec4(position, 1.0);
    // A pixel offset in clip space: two over the viewport's size, scaled by w so it stays the
    // same size at any depth.
    clip.xy += offset * 2.0 / viewport.xy * clip.w;
    gl_Position = clip;
}
