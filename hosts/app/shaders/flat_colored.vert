#version 440
// SPDX-License-Identifier: Apache-2.0

// Unlit lines and triangles with per-vertex colors (colored strands, guides by level, the scalp map).

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

layout(location = 0) out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 itemColor;
    vec4 highlight;
    vec4 camera;
    vec4 light;
};

void main() {
    vColor = color;
    gl_Position = mvp * vec4(position, 1.0);
}
