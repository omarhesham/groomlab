#version 440
// SPDX-License-Identifier: Apache-2.0

// One axis of the separable subsurface blur (Jimenez, Zsolnai, Jarabo and Gutierrez 2015,
// "Separable Subsurface Scattering"): the skin's diffuse irradiance (surface_lookdev.frag's
// SSS_PREPASS variant) convolved along `direction` with the diffusion profile's
// one-dimensional kernel (look.cpp's separableSssKernel: Burley's normalized diffusion,
// Christensen and Burley 2015, the taps crowded toward the center). The kernel's offsets are
// in world units at scale 1; the pixel's width (the irradiance's alpha: the material's radius
// scale times its strength) and its view depth turn them into pixels, so the blur is as wide
// on the screen as the scattering is on the skin. Taps follow the surface: one whose depth
// differs from the center's by far more than their lateral distance (an edge: the ear over
// the cheek, the jaw over the neck) or that holds no skin takes the center's value instead,
// which keeps the kernel's weights whole. In the accumulated frames the taps are jittered
// inside their cells, so the mean converges to the exact convolution without banding.

layout(location = 0) in vec2 vUv;

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 3) uniform Blit {
    vec4 blit;  // w: flip y (fullscreen.vert reads it; the texel fetches here need no flip)
};

layout(std140, binding = 2) uniform Sss {
    vec4 sssParams;    // xy: the axis (in pixels), z: a pixel's width at unit depth (2 tan(fov/2) / height), w: frame (-1: no jitter)
    vec4 sssParams2;   // x: the tap count, y: unused, zw: the target's size
    vec4 sssKernel[32];  // x: the offset (world units at scale 1), yzw: the rgb weight
};

layout(binding = 10) uniform sampler2D irradiance;
layout(binding = 11) uniform sampler2D depth;

uvec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    return v;
}

void main() {
    ivec2 p = ivec2(gl_FragCoord.xy);
    vec4 center = texelFetch(irradiance, p, 0);
    if (center.a <= 0.0) {
        fragColor = center;
        return;
    }
    float z = max(texelFetch(depth, p, 0).r, 1e-4);
    // World units (at scale 1) to pixels at this depth.
    float scale = center.a / (z * sssParams.z);
    int taps = int(sssParams2.x + 0.5);
    float jitter = 0.0;
    if (sssParams.w >= 0.0) {
        uvec3 h = pcg3d(uvec3(uvec2(p), uint(sssParams.w) * 2u + uint(sssParams.x != 0.0 ? 1 : 0)));
        jitter = float(h.x) * (1.0 / 4294967296.0) - 0.5;
    }
    ivec2 size = ivec2(sssParams2.zw);
    vec3 sum = vec3(0.0);
    for (int i = 0; i < taps; ++i) {
        vec4 k = sssKernel[i];
        float offset = k.x;
        if (jitter != 0.0 && i > 0 && i + 1 < taps) {
            // Within the cell: halfway to each neighbor.
            offset += jitter * 0.5 * (sssKernel[i + 1].x - sssKernel[i - 1].x);
        }
        vec3 value = center.rgb;
        if (offset != 0.0) {
            ivec2 q = ivec2(floor(gl_FragCoord.xy + sssParams.xy * (offset * scale)));
            if (q.x >= 0 && q.y >= 0 && q.x < size.x && q.y < size.y) {
                vec4 s = texelFetch(irradiance, q, 0);
                float zs = texelFetch(depth, q, 0).r;
                // An edge: the tap's depth differs by more than twice the distance between
                // them along the skin (a slope past 63 degrees); its weight returns to the
                // center. Not skin either: the same.
                float lateral = abs(offset) * center.a;
                float slope = abs(zs - z) / max(lateral, 1e-5);
                float reject = s.a > 0.0 ? smoothstep(2.0, 5.0, slope) : 1.0;
                value = mix(s.rgb, center.rgb, reject);
            }
        }
        sum += value * k.yzw;
    }
    fragColor = vec4(sum, center.a);
}
