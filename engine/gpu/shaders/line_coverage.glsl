// SPDX-License-Identifier: Apache-2.0
// GroomLab's strand lines' width as coverage, one source for every host that draws it: the
// standalone app's line shaders and visibility rasterizer (hosts/app/shaders: #include) and Maya's
// Viewport 2.0 lines and hair effects and the lit lines' coverage fragment (hosts/maya: embedded at
// build time, renamed into HLSL for DirectX 11 and kept as GLSL for OpenGL). display.h is the
// reference (lineCoverage, lineCoverageAtDensity, linePcg3d, lineHashThreshold, lineKeepShare,
// lineSampleMask; tests/engine/core/test_display.cpp checks it), these the same arithmetic.
//
// The words both languages share, as hair_bsdf.glsl's: GLSL's vector types (vec2, uvec3: Maya
// renames them float2, uint3 for HLSL) and functions, never a vector built from one scalar, no
// global constants (an HLSL effect would take them for uniforms), no uniforms: each host hands in
// its own.

// Jarzynski and Olano 2020's pcg3d (display.h linePcg3d).
uvec3 linePcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    v ^= v >> 16u;
    v.x += v.y * v.z; v.y += v.z * v.x; v.z += v.x * v.y;
    return v;
}

// One stretch's hash: the strand, the stretch's index along u at 2^level stretches a unit of u, the level.
float lineStretchHash(uint strand, float u, float level) {
    return float(linePcg3d(uvec3(strand, uint(max(floor(u * exp2(level)), 0.0)), uint(int(level) + 64))).x) * (1.0 / 4294967296.0);
}

// A threshold per strand and stretch of it, 2^level stretches a unit of u, the stretches of the two
// powers of two around `level` blended and their blend made uniform again (Wyman and McGuire).
float lineLevelThreshold(uint strand, float u, float level) {
    float lo = floor(level), f = level - lo;
    float x = (1.0 - f) * lineStretchHash(strand, u, lo) + f * lineStretchHash(strand, u, lo + 1.0);
    float a = min(f, 1.0 - f);
    float t = x;
    if (a > 1e-4) {
        if (x < a) t = x * x / (2.0 * a * (1.0 - a));
        else if (x < 1.0 - a) t = (x - 0.5 * a) / (1.0 - a);
        else t = 1.0 - (1.0 - x) * (1.0 - x) / (2.0 * a * (1.0 - a));
    }
    return clamp(t, 1e-6, 1.0);
}

// A threshold per strand and stretch of it a pixel long (`perPixel`: the change of u from one pixel
// to the next, the derivatives taken before any branch).
float lineStretchThreshold(uint strand, float u, float perPixel) {
    return lineLevelThreshold(strand, u, log2(1.0 / max(perPixel, 1e-7)));
}

// The threshold a fragment is kept above (display.h lineHashThreshold) on a target of `samples`
// samples a pixel. Several: the pixel-long stretch's, each fragment covering samples of its own
// (lineSampleMask). One sample a pixel (or a fragment kept or not as a whole): stretches of about
// four pixels, so a thin strand shows as short dashes rather than scattered dots. (Not one threshold
// a whole strand: strands share their numbers, a thousand of them, and those drawn over each other
// would come and go together.)
float lineThreshold(uint strand, float u, float perPixel, uint samples) {
    return lineLevelThreshold(strand, u, log2(1.0 / max(perPixel, 1e-7)) - (samples <= 1u ? 2.0 : 0.0));
}

// The share of the coverage kept on one sample a pixel (display.h lineKeepShare: kLineHashedBelow
// 0.1, kLineBlendedAbove 0.4).
float lineKeepShare(float c, float own) {
    float x = clamp((own - 0.1) / (0.4 - 0.1), 0.0, 1.0);
    return c + (1.0 - c) * x * x * (3.0 - 2.0 * x);
}

// The samples of `n` a fragment of coverage `c` and threshold `t` covers (display.h lineSampleMask).
uint lineSampleMask(float c, float t, uint strand, uint n) {
    n = clamp(n, 1u, 32u);
    uint k = uint(clamp(floor(c * float(n) + 1.0 - t), 0.0, float(n)));
    uint full = n >= 32u ? 0xFFFFFFFFu : (1u << n) - 1u;
    if (k == 0u) return 0u;
    if (k >= n) return full;
    uint run = (1u << k) - 1u;
    uint r = linePcg3d(uvec3(strand, 0x9E37u, 0x51u)).y % n;
    return r == 0u ? run : ((run << r) | (run >> (n - r))) & full;
}

// The share of a pixel a strand `width` wide covers at clip w `w` (display.h lineCoverage: times
// `widthScale` and `pixelScale`, linePixelScale, clamped to [floor, 1]), taken to the render's
// `density` (lineCoverageAtDensity) and widened by the level of detail's class (`lod`: the strands
// it stands for, strand_lod.h; 0 none): x; y the coverage lineKeepShare reads, the strand's own (0
// for a thinned class's strand: it stands for several, hashed alone). (1, 1) for a negative width
// (no widths: opaque) or a point at or behind the eye.
vec2 lineCoverageOf(float width, float w, float widthScale, float pixelScale, float coverageFloor, float density, float lod) {
    if (width < 0.0 || w <= 0.0) return vec2(1.0, 1.0);
    float c = clamp(width * widthScale * pixelScale / w, coverageFloor, 1.0);
    float exponent = (lod > 0.0 ? lod : 1.0) / (density > 0.0 && density < 1.0 ? density : 1.0);
    return vec2(exponent != 1.0 && c < 1.0 ? 1.0 - pow(1.0 - c, exponent) : c, lod > 0.0 ? 0.0 : c);
}
