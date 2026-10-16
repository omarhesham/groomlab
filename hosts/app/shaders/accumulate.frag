#version 440
// SPDX-License-Identifier: Apache-2.0

// Progressive refinement: the new frame folded into the running mean of the frames since the
// last change (weight 1 / n for the n-th), each frame rendered with its own sub-pixel jitter,
// stochastic coverage, shadow taps and environment directions, so the mean converges to the
// anti-aliased, order-independent, softly lit image.

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 3) uniform Blit {
    vec4 blit;
};

layout(binding = 10) uniform sampler2D frame;
layout(binding = 11) uniform sampler2D previous;

void main() {
    vec4 now = texture(frame, vUv);
    vec4 before = texture(previous, vUv);
    fragColor = mix(before, now, blit.x);
}
