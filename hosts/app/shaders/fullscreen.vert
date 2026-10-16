#version 440
// SPDX-License-Identifier: Apache-2.0

// One triangle over the whole target (no vertex buffer: three vertices from the index). Its uv
// addresses a texture rendered earlier at this very pixel whatever the backend's framebuffer
// orientation: the Blit block says whether rows count from the top (Direct3D, Metal: flip).

layout(location = 0) out vec2 vUv;

layout(std140, binding = 3) uniform Blit {
    vec4 blit;   // x: the new frame's weight (accumulate), or the exposure (tone map); y: the tone map; z: unused; w: flip y
};

void main() {
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    vUv = vec2(corner.x, blit.w > 0.5 ? 1.0 - corner.y : corner.y);
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
