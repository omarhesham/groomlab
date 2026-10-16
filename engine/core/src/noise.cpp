// SPDX-License-Identifier: Apache-2.0
#include "groomlab/noise.h"

#include <algorithm>
#include <cmath>

#include "groomlab/random.h"

#if defined(_M_X64) || defined(__SSE2__)
#define GROOMLAB_NOISE_SSE2 1
#include <emmintrin.h>
#else
#define GROOMLAB_NOISE_SSE2 0
#endif

namespace groomlab {

namespace {

float fade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }
float lerp(float a, float b, float t) { return a + (b - a) * t; }

// One cheap, well-mixed hash per lattice corner.
std::uint64_t cornerHash(std::int64_t ix, std::int64_t iy, std::int64_t iz, std::uint64_t seed) {
    std::uint64_t h = seed ^ (static_cast<std::uint64_t>(ix) * 0x9E3779B185EBCA87ull) ^
                      (static_cast<std::uint64_t>(iy) * 0xC2B2AE3D27D4EB4Full) ^
                      (static_cast<std::uint64_t>(iz) * 0x165667B19E3779F9ull);
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    return h;
}

// Perlin's 12 edge gradients, as (x, y, z) components.
constexpr float kGradients[12][3] = {{1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
                                     {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1}};

// Gradient index (0-11) for 16 hash bits: bits % 12, through a table (no division per corner).
struct GradientIndex {
    unsigned char index[65536];
    GradientIndex() {
        for (unsigned b = 0; b < 65536; ++b) index[b] = static_cast<unsigned char>(b % 12u);
    }
};
const GradientIndex kGradientIndex;

inline float grad(unsigned bits, float dx, float dy, float dz) {
    const float* g = kGradients[kGradientIndex.index[bits]];
    return g[0] * dx + g[1] * dy + g[2] * dz;
}

// The 8 corner hashes of lattice cell (ix, iy, iz), corner c at (+c&1, +c>>1&1, +c>>2&1).
void cellHashes(std::int64_t ix, std::int64_t iy, std::int64_t iz, std::uint64_t seed, std::uint64_t out[8]) {
    for (int c = 0; c < 8; ++c) out[c] = cornerHash(ix + (c & 1), iy + ((c >> 1) & 1), iz + ((c >> 2) & 1), seed);
}

// Evaluates `channels` (1-3) independent gradient noises at once; channel c
// takes its gradient from bits [16c, 16c + 16) of the corner hash, so the
// lattice work is shared. (VectorNoise below does the same per point, in the same order.)
template <int Channels>
void noiseChannels(const Vec3f& p, std::uint64_t seed, float out[Channels]) {
    const float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z);
    const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy), iz = static_cast<std::int64_t>(fz);
    const float dx = p.x - fx, dy = p.y - fy, dz = p.z - fz;
    const float u = fade(dx), v = fade(dy), w = fade(dz);

    float corner[8][Channels];
    for (int c = 0; c < 8; ++c) {
        const int ox = c & 1, oy = (c >> 1) & 1, oz = (c >> 2) & 1;
        const std::uint64_t h = cornerHash(ix + ox, iy + oy, iz + oz, seed);
        for (int k = 0; k < Channels; ++k) {
            corner[c][k] = grad(static_cast<unsigned>((h >> (16 * k)) & 0xffff), dx - ox, dy - oy, dz - oz);
        }
    }
    for (int k = 0; k < Channels; ++k) {
        const float x00 = lerp(corner[0][k], corner[1][k], u);
        const float x10 = lerp(corner[2][k], corner[3][k], u);
        const float x01 = lerp(corner[4][k], corner[5][k], u);
        const float x11 = lerp(corner[6][k], corner[7][k], u);
        out[k] = lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
    }
}

// cornerHash in four dimensions: the w term mixed in as the others are.
std::uint64_t cornerHash4(std::int64_t ix, std::int64_t iy, std::int64_t iz, std::int64_t iw, std::uint64_t seed) {
    std::uint64_t h = seed ^ (static_cast<std::uint64_t>(ix) * 0x9E3779B185EBCA87ull) ^
                      (static_cast<std::uint64_t>(iy) * 0xC2B2AE3D27D4EB4Full) ^
                      (static_cast<std::uint64_t>(iz) * 0x165667B19E3779F9ull) ^
                      (static_cast<std::uint64_t>(iw) * 0xD6E8FEB86659FD93ull);
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 32;
    return h;
}

// Perlin's 32 four-dimensional edge gradients for 16 hash bits: gradient bits % 32 has axis
// (bits % 32) >> 3 at 0 and the other three at 1, in axis order each -1 where its bit (0, 1, 2)
// is set. Its dot with (dx, dy, dz, dw), summed in that order.
inline float grad4(unsigned bits, float dx, float dy, float dz, float dw) {
    const unsigned i = bits & 31u, zero = i >> 3;
    const float d[4] = {dx, dy, dz, dw};
    float g[4];
    unsigned bit = 0;
    for (unsigned a = 0; a < 4; ++a) g[a] = a == zero ? 0.0f : (((i >> bit++) & 1u) != 0u ? -1.0f : 1.0f);
    return ((g[0] * d[0] + g[1] * d[1]) + g[2] * d[2]) + g[3] * d[3];
}

// noiseChannels in four dimensions (p, w): 16 corners, corner c at (+c&1, +c>>1&1, +c>>2&1,
// +c>>3&1), blended along x, then y, z and w.
template <int Channels>
void noiseChannels4(const Vec3f& p, float pw, std::uint64_t seed, float out[Channels]) {
    const float fx = std::floor(p.x), fy = std::floor(p.y), fz = std::floor(p.z), fw = std::floor(pw);
    const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy), iz = static_cast<std::int64_t>(fz),
               iw = static_cast<std::int64_t>(fw);
    const float dx = p.x - fx, dy = p.y - fy, dz = p.z - fz, dw = pw - fw;
    const float u = fade(dx), v = fade(dy), w = fade(dz), t = fade(dw);

    float corner[16][Channels];
    for (int c = 0; c < 16; ++c) {
        const int ox = c & 1, oy = (c >> 1) & 1, oz = (c >> 2) & 1, ow = (c >> 3) & 1;
        const std::uint64_t h = cornerHash4(ix + ox, iy + oy, iz + oz, iw + ow, seed);
        for (int k = 0; k < Channels; ++k) {
            corner[c][k] = grad4(static_cast<unsigned>((h >> (16 * k)) & 0xffff), dx - static_cast<float>(ox),
                                 dy - static_cast<float>(oy), dz - static_cast<float>(oz), dw - static_cast<float>(ow));
        }
    }
    for (int k = 0; k < Channels; ++k) {
        float atW[2];
        for (int h = 0; h < 2; ++h) {
            const int b = h * 8;
            const float x00 = lerp(corner[b][k], corner[b + 1][k], u);
            const float x10 = lerp(corner[b + 2][k], corner[b + 3][k], u);
            const float x01 = lerp(corner[b + 4][k], corner[b + 5][k], u);
            const float x11 = lerp(corner[b + 6][k], corner[b + 7][k], u);
            atW[h] = lerp(lerp(x00, x10, v), lerp(x01, x11, v), w);
        }
        out[k] = lerp(atW[0], atW[1], t);
    }
}

}  // namespace

float gradientNoise(const Vec3f& p, std::uint64_t seed) {
    float v[1];
    noiseChannels<1>(p, seed, v);
    return v[0];
}

float fractalNoise(const Vec3f& p, std::uint64_t seed, int octaves) {
    float sum = 0.0f, amplitude = 1.0f, norm = 0.0f;
    Vec3f q = p;
    for (int o = 0; o < std::max(octaves, 1); ++o) {
        sum += gradientNoise(q, hashCombine(seed, static_cast<std::uint64_t>(o))) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        q = q * 2.0f;
    }
    return sum / norm;
}

float fractalNoise4(const Vec3f& p, float w, std::uint64_t seed, int octaves) {
    float sum = 0.0f, amplitude = 1.0f, norm = 0.0f;
    Vec3f q = p;
    float qw = w;
    for (int o = 0; o < std::max(octaves, 1); ++o) {
        float v[1];
        noiseChannels4<1>(q, qw, hashCombine(seed, static_cast<std::uint64_t>(o)), v);
        sum += v[0] * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        q = q * 2.0f;
        qw *= 2.0f;
    }
    return sum / norm;
}

Vec3f vectorNoise4(const Vec3f& p, float w, std::uint64_t seed, int octaves) {
    Vec3f sum;
    float amplitude = 1.0f, norm = 0.0f;
    Vec3f q = p;
    float qw = w;
    for (int o = 0; o < std::max(octaves, 1); ++o) {
        float v[3];
        noiseChannels4<3>(q, qw, hashCombine(seed, static_cast<std::uint64_t>(o)), v);
        sum += Vec3f(v[0], v[1], v[2]) * amplitude;
        norm += amplitude;
        amplitude *= 0.5f;
        q = q * 2.0f;
        qw *= 2.0f;
    }
    return sum / norm;
}

void octaveWeights(float spacing, int octaves, float* out) {
    float cells = spacing;  // the spacing in this octave's cells
    for (int o = 0; o < octaves; ++o) {
        out[o] = spacing > 0.0f ? 1.0f - smoothstep(0.5f, 1.0f, cells) : 1.0f;
        cells *= 2.0f;
    }
}

Vec3f vectorNoise(const Vec3f& p, std::uint64_t seed, int octaves, const float* weights) {
    Vec3f sum;
    float amplitude = 1.0f, norm = 0.0f;
    Vec3f q = p;
    for (int o = 0; o < std::max(octaves, 1); ++o) {
        // A faded octave (weight 0) adds nothing: skipped, the sum as if it were added.
        const float w = weights ? weights[o] : 1.0f;
        if (w != 0.0f) {
            float v[3];
            noiseChannels<3>(q, hashCombine(seed, static_cast<std::uint64_t>(o)), v);
            sum += Vec3f(v[0], v[1], v[2]) * (amplitude * w);
        }
        norm += amplitude;
        amplitude *= 0.5f;
        q = q * 2.0f;
    }
    return sum / norm;
}

VectorNoise::VectorNoise(std::uint64_t seed, int octaves, const float* weights) : count_(std::clamp(octaves, 1, kMaxOctaves)) {
    for (int o = 0; o < count_; ++o) {
        octaves_[o].seed = hashCombine(seed, static_cast<std::uint64_t>(o));
        weights_[o] = weights ? weights[o] : 1.0f;
    }
}

Vec3f VectorNoise::operator()(const Vec3f& p) {
    // vectorNoise's sum, octave by octave in the same order.
    Vec3f sum;
    float amplitude = 1.0f, norm = 0.0f;
    Vec3f q = p;
    for (int o = 0; o < count_; ++o) {
        if (weights_[o] == 0.0f) {  // faded out: adds nothing (vectorNoise skips it too)
            norm += amplitude;
            amplitude *= 0.5f;
            q = q * 2.0f;
            continue;
        }
        Octave& oct = octaves_[o];
        const float fx = std::floor(q.x), fy = std::floor(q.y), fz = std::floor(q.z);
        const auto ix = static_cast<std::int64_t>(fx), iy = static_cast<std::int64_t>(fy), iz = static_cast<std::int64_t>(fz);
        if (!oct.valid || ix != oct.ix || iy != oct.iy || iz != oct.iz) {
            std::uint64_t hashes[8];
            cellHashes(ix, iy, iz, oct.seed, hashes);
            for (int c = 0; c < 8; ++c) {
                for (int k = 0; k < 3; ++k) {
                    const float* g = kGradients[kGradientIndex.index[(hashes[c] >> (16 * k)) & 0xffff]];
                    for (int a = 0; a < 3; ++a) oct.g[k][a][c] = g[a];
                }
            }
            oct.ix = ix;
            oct.iy = iy;
            oct.iz = iz;
            oct.valid = true;
            oct.xyValid = false;
        }
        // noiseFromCell's arithmetic, in its order: g.x * (dx - ox) + g.y * (dy - oy) + g.z * (dz - oz).
        if (!oct.xyValid || q.x != oct.x || q.y != oct.y) {
            const float dx = q.x - fx, dy = q.y - fy;
            oct.u = fade(dx);
            oct.v = fade(dy);
            for (int k = 0; k < 3; ++k) {
                for (int c = 0; c < 8; ++c) {
                    oct.xy[k][c] = oct.g[k][0][c] * (dx - static_cast<float>(c & 1)) +
                                   oct.g[k][1][c] * (dy - static_cast<float>((c >> 1) & 1));
                }
            }
            oct.x = q.x;
            oct.y = q.y;
            oct.xyValid = true;
        }
        const float dz = q.z - fz;
        const float w = fade(dz);
        const float dz1 = dz - 1.0f;
        float v[3];
#if GROOMLAB_NOISE_SSE2
        // Four lanes at a time, the same operations per lane as the scalar code below.
        const __m128 vz0 = _mm_set1_ps(dz), vz1 = _mm_set1_ps(dz1), vu = _mm_set1_ps(oct.u), vv = _mm_set1_ps(oct.v);
        for (int k = 0; k < 3; ++k) {
            const __m128 lo = _mm_add_ps(_mm_loadu_ps(oct.xy[k]), _mm_mul_ps(_mm_loadu_ps(oct.g[k][2]), vz0));
            const __m128 hi = _mm_add_ps(_mm_loadu_ps(oct.xy[k] + 4), _mm_mul_ps(_mm_loadu_ps(oct.g[k][2] + 4), vz1));
            const __m128 even = _mm_shuffle_ps(lo, hi, _MM_SHUFFLE(2, 0, 2, 0));  // c0 c2 c4 c6
            const __m128 odd = _mm_shuffle_ps(lo, hi, _MM_SHUFFLE(3, 1, 3, 1));   // c1 c3 c5 c7
            const __m128 xs = _mm_add_ps(even, _mm_mul_ps(_mm_sub_ps(odd, even), vu));  // x00 x10 x01 x11
            const __m128 a = _mm_shuffle_ps(xs, xs, _MM_SHUFFLE(2, 0, 2, 0));          // x00 x01
            const __m128 b = _mm_shuffle_ps(xs, xs, _MM_SHUFFLE(3, 1, 3, 1));          // x10 x11
            alignas(16) float y[4];
            _mm_store_ps(y, _mm_add_ps(a, _mm_mul_ps(_mm_sub_ps(b, a), vv)));
            v[k] = lerp(y[0], y[1], w);
        }
#else
        for (int k = 0; k < 3; ++k) {
            float corner[8];
            for (int c = 0; c < 8; ++c) corner[c] = oct.xy[k][c] + oct.g[k][2][c] * (c < 4 ? dz : dz1);
            const float x00 = lerp(corner[0], corner[1], oct.u);
            const float x10 = lerp(corner[2], corner[3], oct.u);
            const float x01 = lerp(corner[4], corner[5], oct.u);
            const float x11 = lerp(corner[6], corner[7], oct.u);
            v[k] = lerp(lerp(x00, x10, oct.v), lerp(x01, x11, oct.v), w);
        }
#endif
        sum += Vec3f(v[0], v[1], v[2]) * (amplitude * weights_[o]);
        norm += amplitude;
        amplitude *= 0.5f;
        q = q * 2.0f;
    }
    return sum / norm;
}

}  // namespace groomlab
