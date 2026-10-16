#version 440
// SPDX-License-Identifier: Apache-2.0

// A surface (the scalp, a prop, an eye) in the lookdev passes: the camera's and the deep
// opacity maps' (the Pass block says whose view). The vertices come interleaved as
// surface.h's buildSurfaceVertices lays them out: position, normal, tangent with the
// bitangent's sign, uv, the baked occlusion and the curvature.

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec4 tangent;
layout(location = 3) in vec2 uv;
layout(location = 4) in vec2 extra;  // x: occlusion, y: curvature (per scene unit)

layout(location = 0) out vec3 vPosition;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec4 vTangent;
layout(location = 3) out vec2 vUv;
layout(location = 4) out vec2 vExtra;

layout(std140, binding = 1) uniform ViewBlock {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 view;
    vec4 pixel;
    vec4 axis;
    vec4 dom;
};

void main() {
    vPosition = position;
    vNormal = normal;
    vTangent = tangent;
    vUv = uv;
    vExtra = extra;
    gl_Position = viewProj * vec4(position, 1.0);
}
