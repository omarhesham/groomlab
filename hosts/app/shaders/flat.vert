#version 440
// SPDX-License-Identifier: Apache-2.0

// Unlit lines and triangles in the item's color (guides, parts, flow, dimmed strands).

layout(location = 0) in vec3 position;

layout(location = 0) out vec4 vColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 highlight;
    vec4 camera;
    vec4 light;
};

void main() {
    vColor = vec4(1.0);
    gl_Position = mvp * vec4(position, 1.0);
}
