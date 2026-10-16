#version 440
// SPDX-License-Identifier: Apache-2.0

// A key light from the viewport's light, a dim fill from the other side, an ambient floor;
// both sides lit so a scalp seen from inside still reads.

layout(location = 0) in vec3 vPosition;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 mvp;
    vec4 color;
    vec4 highlight;
    vec4 camera;
    vec4 light;
};

void main() {
    vec3 n = normalize(vNormal);
    vec3 v = normalize(camera.xyz - vPosition);
    if (dot(n, v) < 0.0) n = -n;
    vec3 l = normalize(light.xyz);
    float key = max(0.0, dot(n, l));
    float fill = max(0.0, dot(n, -l)) * 0.25;
    float rim = pow(1.0 - max(0.0, dot(n, v)), 3.0) * 0.15;
    fragColor = vec4(color.rgb * (light.w + (1.0 - light.w) * (key + fill)) + vec3(rim), color.a);
}
