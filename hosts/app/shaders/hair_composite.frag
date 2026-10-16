#version 440
// SPDX-License-Identifier: Apache-2.0

// The visibility path's hair (hair_shade.comp: premultiplied by its alpha) over the HDR scene,
// blended One, OneMinusSrcAlpha. Read texel for texel: the hair image is laid out as the target
// is (gl_FragCoord addresses both the same way on every backend), a reduced frame's corner too.

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 3) uniform Blit {
    vec4 blit;
};

layout(binding = 10) uniform sampler2D hairImage;

void main() {
    fragColor = texelFetch(hairImage, ivec2(gl_FragCoord.xy), 0);
}
