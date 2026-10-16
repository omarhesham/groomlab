#version 440
// SPDX-License-Identifier: Apache-2.0

// The display transform: exposure, then ACES (Narkowicz 2015's fit), AgX (Sobotka's default
// look, as Blender ships it: the inset matrix, the sigmoid, the outset) or none, then the
// sRGB curve for the window's 8-bit target.

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 3) uniform Blit {
    vec4 blit;  // x: exposure (linear), y: tone map (0 none, 1 ACES, 2 AgX), z: a reduced frame's scale (0: whole), w: flip y
};

layout(binding = 10) uniform sampler2D frame;

vec3 aces(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 agx(vec3 v) {
    const mat3 inset = mat3(0.856627153315983, 0.137318972929847, 0.11189821299995,
                            0.0951212405381588, 0.761241990602591, 0.0767994186031903,
                            0.0482516061458583, 0.101439036467562, 0.811302368396859);
    const mat3 outset = mat3(1.1271005818144368, -0.1413297634984383, -0.14132976349843826,
                             -0.11060664309660323, 1.157823702216272, -0.11060664309660294,
                             -0.016493938717834573, -0.016493938717834257, 1.2519364065950405);
    v = inset * max(v, vec3(0.0));
    // log2 encoding over the look's range, then the 6th order sigmoid fit.
    const float minEv = -12.47393, maxEv = 4.026069;
    v = clamp(log2(max(v, vec3(1e-10))), vec3(minEv), vec3(maxEv));
    v = (v - minEv) / (maxEv - minEv);
    vec3 v2 = v * v, v4 = v2 * v2;
    v = 15.5 * v4 * v2 - 40.14 * v4 * v + 31.96 * v4 - 6.868 * v2 * v + 0.4298 * v2 + 0.1191 * v - 0.00232;
    v = outset * v;
    // The sigmoid lands in sRGB's curve already: back to linear for the common encode below.
    return pow(max(v, vec3(0.0)), vec3(2.2));
}

vec3 toSrgb(vec3 c) {
    c = clamp(c, 0.0, 1.0);
    return mix(12.92 * c, 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055, step(0.0031308, c));
}

void main() {
    vec2 uv = vUv;
    if (blit.z > 0.0) {
        // A reduced frame drew into the targets' corner (the viewport's origin: the bottom
        // left in the RHI's convention), at the size lookdev_renderer.cpp rounds to: scaled
        // up over the whole target, the taps kept half a texel inside the corner.
        vec2 full = vec2(textureSize(frame, 0));
        vec2 drawn = floor(full * blit.z + 0.5);
        vec2 up = vec2(uv.x, blit.w > 0.5 ? 1.0 - uv.y : uv.y);  // y up from the corner
        up = clamp(up * drawn, vec2(0.5), drawn - 0.5) / full;
        uv = vec2(up.x, blit.w > 0.5 ? 1.0 - up.y : up.y);
    }
    vec3 hdr = texture(frame, uv).rgb * blit.x;
    vec3 mapped = blit.y < 0.5 ? hdr : blit.y < 1.5 ? aces(hdr) : agx(hdr);
    fragColor = vec4(toSrgb(mapped), 1.0);
}
