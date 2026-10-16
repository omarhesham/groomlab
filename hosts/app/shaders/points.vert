#version 440
// SPDX-License-Identifier: Apache-2.0

// One triangle over the whole target, nothing else: the per-point shading pass (hair_lookdev.frag
// under HAIR_POINT_SHADE) finds its point in gl_FragCoord, so no uv and no uniform block (its
// fragment shader has the hair's textures at the bindings fullscreen.vert's Blit block would take).

void main() {
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
