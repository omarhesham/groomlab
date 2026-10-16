#version 440
// SPDX-License-Identifier: Apache-2.0

layout(location = 0) in vec4 vColor;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 highlight;
    vec4 camera;
    vec4 light;
};

void main() {
    fragColor = vColor * color;
}
