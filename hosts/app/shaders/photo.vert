#version 440
// SPDX-License-Identifier: Apache-2.0

// A reference photo over (or behind) the groom while the viewport looks through its camera: one
// quad (a strip of four vertices from the index, no vertex buffer) over the photo's rectangle in
// normalized device coordinates, its uv the image's (0, 0 at its top-left row, as uploaded).

layout(location = 0) out vec2 vUv;

layout(std140, binding = 0) uniform Photo {
    vec4 rect;    // x0, y0 (the photo's top-left corner), x1, y1 (its bottom-right), NDC
    vec4 params;  // x: opacity
};

void main() {
    vec2 corner = vec2(float(gl_VertexIndex & 1), float((gl_VertexIndex >> 1) & 1));
    vUv = corner;
    gl_Position = vec4(mix(rect.x, rect.z, corner.x), mix(rect.y, rect.w, corner.y), 0.0, 1.0);
}
