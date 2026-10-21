#version 440
// SPDX-License-Identifier: Apache-2.0

// The reference photo at its opacity (blended over what the pass drew so far).

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform Photo {
    vec4 rect;
    vec4 params;  // x: opacity
};

layout(binding = 1) uniform sampler2D photo;

void main() {
    vec4 c = texture(photo, vUv);
    fragColor = vec4(c.rgb, c.a * params.x);
}
