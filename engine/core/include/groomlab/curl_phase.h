// SPDX-License-Identifier: Apache-2.0
#pragma once

// The phase of Curl's turns (curl_op.cpp; look.slang curlAngle repeats it for the GPU preview):
// how far round its curl a point is, and how fast the curl turns there. A display that places
// points by the turn reads it here.
//
// A curl turns at the local frequency
//
//     f_local(s) = f · profile(t(s)) · (1 + spacingRandom · n(τ(s)))
//
// turns per unit of length, s the arc length along the curl's axis, t the share of the strand,
// `profile` Curl's frequencyProfile (0 to 1, as every ramp), n a smooth noise of the strand's (or the clump's)
// own in [-1, 1], and τ = |f| w its turns so far without the noise, w = ∫0^s profile du the
// "profiled arc" (the axis's arc length weighted by the profile: curl_op.cpp Axis integrates it
// along its 32-sample table, two-point Gauss on each piece between the ramp's keys, as the arc
// length itself). The angle is the integral of it, exactly:
//
//     θ(s) = 2π ∫0^s f_local du + φ = 2π (f w + sign(f) · spacingRandom · S(|f| w)) + φ
//
// S being n's integral, in closed form: n is made of layers each 0 at its cells' ends (cells a
// few turns long), whose integrals over a cell telescope (curlSpacingTurns). So the phase is
// resolution-independent (a function of the axis alone), it never turns back (f_local keeps f's
// sign: |n| < 1), and the noise's scale is in turns, not scene units (scale-invariant). With the
// defaults (a profile of 1 everywhere, spacingRandom 0), w is the arc length and θ = 2π f s + φ.
//
// Curl's weight divides the profile by a stretch along the strand (curl_op.cpp: the turns spread
// where hair hangs below them): w then weighs the arc by profile / stretch, and the rest holds.

#include <cmath>
#include <cstdint>
#include <cstring>

namespace groomlab {

constexpr float kCurlTwoPi = 6.28318530718f;

// A 32-bit integer hash (Wellons' lowbias32; look.slang curlHash).
constexpr std::uint32_t curlHash(std::uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// The key of a curl's spacing noise: from the bits of its four draws (CurlOp::drawFor's, the
// strand's or the clump's; on the GPU its per-strand numbers), so it is the strand's (or the
// clump's) own and needs no number of its own.
inline std::uint32_t curlSpacingKey(float r0, float r1, float r2, float r3) {
    std::uint32_t b[4];
    std::memcpy(&b[0], &r0, 4);
    std::memcpy(&b[1], &r1, 4);
    std::memcpy(&b[2], &r2, 4);
    std::memcpy(&b[3], &r3, 4);
    return curlHash(b[0] ^ curlHash(b[1] ^ curlHash(b[2] ^ curlHash(b[3] ^ 0x5BAC1E5u))));
}

// The noise's layers: each a cell `turns` long, weighted (the weights sum to 1, so |n| < 1). Two
// cell lengths apart, so the noise's zeros do not come at regular turns.
struct CurlSpacingLayer {
    float turns, weight;
    std::uint32_t salt;
};
constexpr CurlSpacingLayer kCurlSpacingLayers[2] = {{2.0f, 0.6f, 0x9E3779B9u}, {1.25f, 0.4f, 0x85EBCA6Bu}};

// Lattice value i of a layer of key `key`: in [-1, 1), exactly as the GPU makes it.
inline float curlSpacingLattice(std::uint32_t key, std::uint32_t i) {
    return static_cast<float>(curlHash(key ^ curlHash(i)) >> 8) * (2.0f / 16777216.0f) - 1.0f;
}

// One layer at x (in its cells, 0 or more): its value n(x) = 8 (v1 - v0) u² (1 - u)² in cell
// floor(x) (C¹, 0 at the cell's ends, |n| < 1), and its integral from 0, N(x) = (4/15) (v0 +
// (v1 - v0) S5(u) - v(0)), S5 the quintic smoothstep (each whole cell adds (4/15)(v1 - v0)).
inline void curlSpacingLayer(std::uint32_t key, float x, float* value, float* integral) {
    const float cell = std::floor(x);
    const float u = x - cell;
    const std::uint32_t i = static_cast<std::uint32_t>(cell);
    const float v0 = curlSpacingLattice(key, i), v1 = curlSpacingLattice(key, i + 1u);
    const float dv = v1 - v0;
    const float w = u * (1.0f - u);
    if (value) *value = 8.0f * dv * w * w;
    if (integral) *integral = (4.0f / 15.0f) * (v0 + dv * (u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f)) - curlSpacingLattice(key, 0u));
}

// The spacing noise n at `turns` (|f| w, 0 or more): in (-1, 1).
inline float curlSpacingRate(std::uint32_t key, float turns) {
    float n = 0.0f;
    for (const CurlSpacingLayer& l : kCurlSpacingLayers) {
        float v = 0.0f;
        curlSpacingLayer(key ^ l.salt, turns / l.turns, &v, nullptr);
        n += l.weight * v;
    }
    return n;
}
// S(turns) = ∫0^turns n: the turns the noise adds by then (per unit of spacingRandom).
inline float curlSpacingTurns(std::uint32_t key, float turns) {
    float total = 0.0f;
    for (const CurlSpacingLayer& l : kCurlSpacingLayers) {
        float integral = 0.0f;
        curlSpacingLayer(key ^ l.salt, turns / l.turns, nullptr, &integral);
        total += l.weight * l.turns * integral;
    }
    return total;
}

// The curl's angle (radians) at profiled arc w, for frequency f (the strand's, varied), start
// `phase` (radians) and the spacing noise's amount and key. (With spacingRandom 0: 2π f w + φ,
// as Curl always made it.)
inline float curlAngle(float frequency, float w, float phase, float spacingRandom, std::uint32_t key) {
    float angle = kCurlTwoPi * frequency * w + phase;
    if (spacingRandom > 0.0f) {
        const float extra = spacingRandom * curlSpacingTurns(key, std::fabs(frequency) * w);
        angle += kCurlTwoPi * (frequency < 0.0f ? -extra : extra);
    }
    return angle;
}

// Its turns at profiled arc w, from the start (the angle less the phase, over 2π).
inline float curlTurns(float frequency, float w, float spacingRandom, std::uint32_t key) {
    float turns = frequency * w;
    if (spacingRandom > 0.0f) {
        const float extra = spacingRandom * curlSpacingTurns(key, std::fabs(frequency) * w);
        turns += frequency < 0.0f ? -extra : extra;
    }
    return turns;
}

// The local frequency (turns per unit of length, signed as f) where frequencyProfile is
// `profileValue` and the profiled arc is w.
inline float curlLocalFrequency(float frequency, float profileValue, float w, float spacingRandom, std::uint32_t key) {
    const float p = profileValue > 0.0f ? profileValue : 0.0f;
    const float n = spacingRandom > 0.0f ? curlSpacingRate(key, std::fabs(frequency) * w) : 0.0f;
    return frequency * p * (1.0f + spacingRandom * n);
}

}  // namespace groomlab
