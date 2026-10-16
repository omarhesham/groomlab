// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <cstdint>

#include "groomlab/math.h"

namespace groomlab {

// Seeded 3D gradient (Perlin-style) noise, roughly in [-1, 1], smooth
// (quintic fade) and deterministic across platforms and threads.
float gradientNoise(const Vec3f& p, std::uint64_t seed);

// Fractal sum of `octaves` gradient noises, each doubling frequency and
// halving amplitude, normalized to about [-1, 1].
float fractalNoise(const Vec3f& p, std::uint64_t seed, int octaves);

// fractalNoise and vectorNoise in four dimensions (p, w), with Perlin's 32 four-dimensional
// gradients: a field that can walk a closed path through its last two (Wind's loop: a circle
// there comes back to where it started, smoothly).
float fractalNoise4(const Vec3f& p, float w, std::uint64_t seed, int octaves);
Vec3f vectorNoise4(const Vec3f& p, float w, std::uint64_t seed, int octaves);

constexpr int kMaxNoiseOctaves = 8;

// Each octave's share of a fractal sum read at points `spacing` apart (in p's units: a strand's
// point spacing in the noise's space), `octaves` values into `out`. Octave o's lattice cells are
// 2^-o across, and an octave fades out as the spacing grows from half a cell (its bumps, a cell
// wide, drawn by two points or more) to a whole cell (one point a bump: the points sample it
// above its Nyquist rate, a look that changes with the point count and shimmers), the way
// renderers filter noise to a footprint. No spacing (0 or less): every octave in full.
void octaveWeights(float spacing, int octaves, float* out);

// Three decorrelated fractal noises: a smooth random displacement direction. With `weights`
// (octaveWeights, one per octave) each octave's amplitude is scaled by its weight while the sum
// stays normalized by the unscaled amplitudes: a strand with fewer points gets the same noise
// less its finer octaves, not other noise.
Vec3f vectorNoise(const Vec3f& p, std::uint64_t seed, int octaves, const float* weights = nullptr);

// vectorNoise for a run of nearby points (a strand's, root to tip): the same values, bit for
// bit, with each octave's lattice corners hashed and their gradients looked up once per cell
// rather than once per point, and, while x and y stay the same from one point to the next (a
// strand-space noise walks z alone), the gradients' x and y terms kept too. One per thread or
// strand; cheap to make. Octaves 1 to kMaxOctaves (clamped). `weights` as vectorNoise's.
class VectorNoise {
public:
    VectorNoise(std::uint64_t seed, int octaves, const float* weights = nullptr);
    Vec3f operator()(const Vec3f& p);

    static constexpr int kMaxOctaves = kMaxNoiseOctaves;

private:
    struct Octave {
        std::uint64_t seed = 0;
        std::int64_t ix = 0, iy = 0, iz = 0;
        bool valid = false;
        float g[3][3][8] = {};    // [channel][axis][corner]: the corners' gradients
        float x = 0.0f, y = 0.0f;  // the point x and y the terms below are for
        bool xyValid = false;
        float u = 0.0f, v = 0.0f;  // fade(dx), fade(dy)
        float xy[3][8] = {};       // [channel][corner]: gx * (dx - ox) + gy * (dy - oy)
    };
    Octave octaves_[kMaxOctaves];
    float weights_[kMaxOctaves];
    int count_;
};

}  // namespace groomlab
