#version 440
// SPDX-License-Identifier: Apache-2.0

// Shaded surfaces: the scalp, guide tubes, instance meshes, cards (normals only, Lambert-like).

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

layout(location = 0) out vec3 vPosition;
layout(location = 1) out vec3 vNormal;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 highlight;
    vec4 camera;
    vec4 light;
};

void main() {
    vPosition = position;
    vNormal = normal;
    gl_Position = mvp * vec4(position, 1.0);
}
