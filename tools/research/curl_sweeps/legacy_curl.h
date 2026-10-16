// SPDX-License-Identifier: Apache-2.0
#pragma once
// The Curl operator as it was before Curl v2 (look_ops.cpp at 6866b32b), full evaluations only,
// registered as "CurlLegacy": measure_curl_b1's "before". Research code.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <map>

#include "groomlab/curve.h"
#include "groomlab/field.h"
#include "groomlab/frames.h"
#include "groomlab/graph.h"
#include "groomlab/operators.h"
#include "groomlab/parallel.h"
#include "groomlab/preview_core.h"
#include "groomlab/random.h"

namespace legacy {

using namespace groomlab;

constexpr float kTwoPi = 6.28318530718f;

inline float paramAt(std::size_t i, std::size_t n) { return n > 1 ? static_cast<float>(i) / static_cast<float>(n - 1) : 0.0f; }
inline const ScalpMesh* scalpOf(const EvalContext& ctx) { return valueAs<MeshPtr>(ctx.external("scalp")).get(); }
inline StrandsPtr requireStrands(const std::vector<Value>& inputs, std::size_t slot, const char* opName) {
    StrandsPtr s = slot < inputs.size() ? valueAs<StrandsPtr>(inputs[slot]) : nullptr;
    if (!s) throw GraphError(std::string(opName) + ": strands input not connected");
    return s;
}
class RampAlong {
public:
    RampAlong(const Ramp& ramp, const StrandSet& strands) : ramp_(ramp), n_(strands.uniformPointCount()) {
        table_.resize(n_);
        for (std::size_t i = 0; i < n_; ++i) table_[i] = ramp_(paramAt(i, n_));
    }
    float operator()(std::size_t i, std::size_t n) const { return n == n_ ? table_[i] : ramp_(paramAt(i, n)); }

private:
    Ramp ramp_;
    std::size_t n_;
    std::vector<float> table_;
};
inline Vec3f frameSeed(const Vec3f& normal, const Vec3f& first, const Vec3f& second) {
    const Vec3f t = second - first;
    const float tt = t.dot(t);
    if (tt > 0.0f) {
        const Vec3f across = normal - t * (normal.dot(t) / tt);
        if (across.dot(across) > 1e-8f) return across.normalized();
    }
    return anyPerpendicular(t);
}
// (Incremental evaluations are never asked of it: these stand in for incremental.h's.)
inline const StrandChanges* someStrandsChanged(const IncrementalInputs&, std::size_t) { return nullptr; }
inline StrandsPtr previousStrandsFor(const IncrementalInputs&, const StrandSet&) { return nullptr; }
inline std::vector<Vec3f>& positionsOfLast(StrandSet& out, const StrandsPtr& last, const IncrementalInputs&) {
    return out.copyPositionsFrom(*last);
}
inline void copyStrandPoints(StrandSet& to, const StrandSet& from, std::size_t s) {
    const Vec3f* src = from.strandPoints(s);
    Vec3f* dst = to.strandPoints(s);
    for (std::size_t i = 0, n = from.strandPointCount(s); i < n; ++i) dst[i] = src[i];
}
inline Vec3f rootNormalOr(const ScalpMesh* scalp, const StrandRoot& root) {
    return scalp && scalp->contains(root) ? scalp->normal(root) : Vec3f{};
}
inline Frame frameBetween(const Frame& a, const Frame& b, float f) {
    if (f <= 0.0f) return a;
    if (f >= 1.0f) return b;
    Frame out;
    const Vec3f t = a.tangent * (1.0f - f) + b.tangent * f;
    out.tangent = t.dot(t) > 0.0f ? t.normalized() : a.tangent;
    Vec3f nrm = a.normal * (1.0f - f) + b.normal * f;
    nrm = nrm - out.tangent * nrm.dot(out.tangent);
    out.normal = nrm.dot(nrm) > 1e-12f ? nrm.normalized() : anyPerpendicular(out.tangent);
    out.binormal = out.tangent.cross(out.normal);
    return out;
}

// Wraps strands into curls around their current shape: a helix (the default), a planar wave or
// zigzag (kinky hair), or a coil, a tight helix a few strand widths across (coily hair). Each
// strand curls on its own, from a phase of its own; with `coherence` clump the strands of a clump
// (Clump's clumpId) curl as one around the clump's middle, with one phase, frequency and plane
// (ringlets).
class CurlLegacyOp : public Operator {
public:
    // The scalp's normal at each root seeds the frames the curls turn in (frameSeed).
    std::vector<std::string> externalInputs() const override { return {"scalp"}; }
    std::string typeName() const override { return "CurlLegacy"; }
    std::string category() const override { return "look"; }
    std::vector<PortSpec> inputs() const override { return {{"strands", PortType::Curves}}; }
    PortSpec output() const override { return {"out", PortType::Curves}; }  // strands or guides, as fed
    std::vector<ParamSpec> paramSpecs() const override {
        return {
            ParamSpec{"radius", 0.5, "Curl radius, scene units (mappable, per point); a coil's is coilWidths", "length"}
                .soft(0, 2)
                .when("shape", {"helix", "wave", "zigzag"}),
            ParamSpec{"frequency", 0.4, "Turns per unit of length", "perLength"}.soft(0, 2),
            ParamSpec{"randomness", 0.3, "Per-strand variation of radius and frequency, 0-1", "fraction"}.range(0, 1),
            {"profile", Ramp{{0.0f, 0.0f}, {0.25f, 1.0f}, {1.0f, 1.0f}}, "Radius along the length, root to tip"},
            {"shape", std::string("helix"),
             "helix: around the strand; wave: a planar sine; zigzag: sharp planar kinks (kinky hair); coil: a tight helix "
             "coilWidths strand widths across (coily hair)", "", {"helix", "wave", "zigzag", "coil"}},
            ParamSpec{"coilWidths", 3.0, "Coil: its radius in strand widths at each point (the widths the strands carry: put "
                                         "a Width node before it)"}
                .soft(0, 10)
                .when("shape", {"coil"}),
            ParamSpec{"phase", 0.0, "Where along a turn every curl starts, in turns (0-1)", "turns"}.soft(0, 1),
            ParamSpec{"phaseRandom", 1.0, "How much of a turn each strand's (or clump's) start varies at random, 0-1",
                      "fraction"}
                .range(0, 1),
            {"coherence", std::string("strand"),
             "strand: each strand curls on its own; clump: the strands of a clump curl together around its middle, with "
             "one phase, frequency and plane (ringlets; after a Clump)", "", {"strand", "clump"}},
            ParamSpec{"clumpChannel", std::string("clumpId"),
                      "Coherence clump: the channel naming each strand's clump (Clump writes clumpId)"}
                .adv()
                .when("coherence", {"clump"}),
            notAnimatable({"seed", std::int64_t{0}, "Random seed"}).adv(),
        };
    }
    // A coil on strands that carry no widths of their own (every width 1, as before any Width).
    std::string note(const EvalContext&) const override {
        return unitWidths_.load(std::memory_order_relaxed)
                   ? "coil reads the strands' widths, and these have none set (1 everywhere): put a Width node before it"
                   : std::string();
    }

    Value evaluate(const EvalContext& ctx, const std::vector<Value>& inputs) const override {
        Value out;
        run(ctx, inputs, nullptr, &out, nullptr);
        return out;
    }
private:
    enum class Shape { Helix, Wave, Zigzag, Coil };
    static constexpr std::size_t kAxisSamples = 32;  // a clump's middle, sampled as Twist samples it

    // One curl's numbers, a strand's (or a clump's): its start (radians), its radius' and
    // frequency's variation, and the plane a wave or zigzag lies in (an angle about the strand).
    struct Draw {
        float phase = 0.0f, vary = 1.0f, frequency = 0.0f, plane = 0.0f;
    };
    struct Settings {
        Shape shape = Shape::Helix;
        float frequency = 4.0f, randomness = 0.3f, phase = 0.0f, phaseRandom = 1.0f, coilWidths = 3.0f;
        std::uint64_t seed = 0;
    };
    // The draws, in the order the helix always made them (its phase, radius and frequency); the
    // plane after them, drawn only for a planar shape.
    static Draw drawFor(std::uint64_t key, const Settings& s) {
        Rng rng(hashCombine(key, s.seed ^ 0xC041));
        Draw d;
        d.phase = kTwoPi * (s.phase + s.phaseRandom * rng.nextFloat());
        d.vary = 1.0f + s.randomness * rng.uniform(-1.0f, 1.0f);
        d.frequency = s.frequency * (1.0f + 0.5f * s.randomness * rng.uniform(-1.0f, 1.0f));
        if (s.shape == Shape::Wave || s.shape == Shape::Zigzag) d.plane = kTwoPi * rng.nextFloat();
        return d;
    }
    // A triangle wave with sin's period, peaks and zeros: straight runs between sharp kinks.
    static float zigzag(float angle) {
        const float u = angle * (1.0f / kTwoPi) + 0.25f;
        return 1.0f - 4.0f * std::fabs(u - std::floor(u) - 0.5f);
    }
    // The offset of a point `r` from the strand at `angle` along its curl, in its frame.
    static Vec3f offsetAt(Shape shape, const Frame& frame, float r, float angle, float plane) {
        if (shape == Shape::Helix || shape == Shape::Coil) return frame.toWorld({r * std::cos(angle), r * std::sin(angle), 0.0f});
        const Vec3f across = frame.normal * std::cos(plane) + frame.binormal * std::sin(plane);
        return across * (r * (shape == Shape::Wave ? std::sin(angle) : zigzag(angle)));
    }

    // In full, or (with `incremental`) only the strands the input changed in, into the last
    // result's copy; false when that cannot be done.
    bool run(const EvalContext& ctx, const std::vector<Value>& inputs, const IncrementalInputs* incremental, Value* result,
             StrandChanges* changed) const {
        const StrandsPtr in = requireStrands(inputs, 0, "CurlLegacy");
        const ScalarField radiusField(params(), "radius", scalpOf(ctx));
        radiusField.validate(*in);
        const bool perPoint = radiusField.perPoint();
        Settings set;
        const std::string shape = params().getString("shape");
        set.shape = shape == "wave" ? Shape::Wave : shape == "zigzag" ? Shape::Zigzag : shape == "coil" ? Shape::Coil : Shape::Helix;
        set.frequency = static_cast<float>(params().getDouble("frequency"));
        set.randomness = static_cast<float>(params().getDouble("randomness"));
        set.phase = static_cast<float>(params().getDouble("phase"));
        set.phaseRandom = static_cast<float>(params().getDouble("phaseRandom"));
        set.coilWidths = static_cast<float>(params().getDouble("coilWidths"));
        set.seed = static_cast<std::uint64_t>(params().getInt("seed"));
        const Ramp profile = params().getRamp("profile");
        const bool byClump = params().getString("coherence") == "clump";
        const bool coil = set.shape == Shape::Coil;
        if (coil) {
            const std::vector<float>& w = in->widths();
            unitWidths_.store(!w.empty() && std::all_of(w.begin(), w.end(), [](float v) { return v == 1.0f; }),
                              std::memory_order_relaxed);
        } else {
            unitWidths_.store(false, std::memory_order_relaxed);
        }

        const StrandChanges* changes = nullptr;
        StrandsPtr last;  // the last result, to make again only the strands the input changed in
        if (incremental) {
            changes = someStrandsChanged(*incremental, 0);
            last = changes ? previousStrandsFor(*incremental, *in) : nullptr;
            if (!last || byClump || !radiusField.strandLocal()) return false;
            if (changes->none()) {  // the same strands in: the same result
                *result = last;
                *changed = *changes;
                return true;
            }
        }
        auto out = std::make_shared<StrandSet>(*in);
        if (last) {
            positionsOfLast(*out, last, *incremental);  // the strands left alone, as they were made
        } else if (byClump) {
            out->mutablePositions();  // cloned once here: the loop writes points while fields read the set
        } else {
            out->overwritePositions();  // every point written: each block of strands copied in before it curls
        }
        const RampAlong along(profile, *out);
        const std::size_t strandCount = out->strandCount();
        // Each strand's frames start from the scalp's normal at its root (frameSeed): the curls
        // turn with the scalp, and neighbours curl in one plane.
        const ScalpMesh* scalp = scalpOf(ctx);
        auto seedOf = [&](std::size_t s, const Vec3f* pts) { return frameSeed(rootNormalOr(scalp, out->roots()[s]), pts[0], pts[1]); };
        // The radius at point i of strand s, before the draw's variation and the profile: a coil's,
        // the param's per point (read before the point moves: the field sees the incoming shape),
        // or the strand's own (`strandRadius`, read once per strand: radiusField.at).
        const float* widths = coil ? out->widths().data() : nullptr;
        auto radiusAt = [&](std::size_t s, std::size_t i, float strandRadius) {
            if (coil) return set.coilWidths * widths[out->strandBegin(s) + i];
            return perPoint ? radiusField.atPoint(*out, s, i) : strandRadius;
        };
        auto strandRadiusOf = [&](std::size_t s) { return perPoint || coil ? 0.0f : radiusField.at(*out, s); };
        auto curl = [&](std::size_t s, const Frame* frames) {
            const std::size_t n = out->strandPointCount(s);
            Vec3f* pts = out->strandPoints(s);
            const Draw d = drawFor(out->ids()[s], set);
            const float r = perPoint || coil ? 0.0f : radiusField.at(*out, s) * d.vary;
            float arc = 0.0f;
            Vec3f previous = pts[0];
            for (std::size_t i = 1; i < n; ++i) {
                const Vec3f original = pts[i];
                arc += (original - previous).length();
                previous = original;
                const float angle = kTwoPi * d.frequency * arc + d.phase;
                const float ri = (perPoint || coil ? radiusAt(s, i, 0.0f) * d.vary : r) * along(i, n);
                pts[i] = original + offsetAt(set.shape, frames[i], ri, angle, d.plane);
            }
        };
        if (byClump) {
            curlByClump(*in, *out, set, along, radiusAt, strandRadiusOf, scalp, [&](std::size_t s) {
                thread_local std::vector<Frame> frames;  // scratch, reused across strands
                const std::size_t count = out->strandPointCount(s);
                if (count < 2) return;
                const Vec3f* pts = out->strandPoints(s);
                rotationMinimizingFramesInto(pts, count, seedOf(s, pts), frames);
                curl(s, frames.data());
            });
            *result = StrandsPtr(out);
            return true;
        }
        if (last) {
            // The strands the input changed in, four in a row at a time where four in a row changed
            // with the same point count (the frames four at a time are each strand's own).
            const std::vector<std::uint32_t>& list = changes->strands;
            std::vector<std::size_t> groups;  // where each group starts in the list (four, or one)
            groups.reserve(list.size());
            for (std::size_t k = 0; k < list.size();) {
                groups.push_back(k);
                const std::size_t s = list[k], n = in->strandPointCount(s);
                bool four = k + 4 <= list.size() && n >= 2 && list[k + 3] == s + 3;
                for (std::size_t j = 1; four && j < 4; ++j) four = in->strandPointCount(s + j) == n;
                k += four ? 4 : 1;
            }
            parallelFor(0, groups.size(), [&](std::size_t g) {
                thread_local std::vector<Frame> frames;  // scratch, reused across strands
                const std::size_t k = groups[g], size = (g + 1 < groups.size() ? groups[g + 1] : list.size()) - k;
                for (std::size_t j = 0; j < size; ++j) copyStrandPoints(*out, *in, list[k + j]);
                if (size == 4) {
                    const std::size_t first = list[k], n = out->strandPointCount(first);
                    frames.resize(4 * n);
                    const Vec3f* pts[4];
                    Vec3f normals[4];
                    Frame* outs[4];
                    for (int q = 0; q < 4; ++q) {
                        pts[q] = out->strandPoints(first + q);
                        normals[q] = seedOf(first + q, pts[q]);
                        outs[q] = frames.data() + q * n;
                    }
                    rotationMinimizingFrames4(pts, n, normals, outs);
                    for (int q = 0; q < 4; ++q) curl(first + q, outs[q]);
                    return;
                }
                const std::size_t s = list[k], count = out->strandPointCount(s);
                if (count < 2) return;
                const Vec3f* pts = out->strandPoints(s);
                rotationMinimizingFramesInto(pts, count, seedOf(s, pts), frames);
                curl(s, frames.data());
            }, 32);
            *changed = *changes;
            *result = StrandsPtr(out);
            return true;
        }
        // Frames four strands at a time where four in a row have the same point count (the usual
        // case): the same frames, side by side.
        parallelFor(0, (strandCount + 3) / 4, [&](std::size_t block) {
            thread_local std::vector<Frame> frames;  // scratch, reused across strands
            const std::size_t first = block * 4, lastStrand = std::min(first + 4, strandCount);
            for (std::size_t s = first; s < lastStrand; ++s) copyStrandPoints(*out, *in, s);
            const std::size_t n = out->strandPointCount(first);
            bool four = lastStrand - first == 4 && n >= 2;
            for (std::size_t s = first + 1; four && s < lastStrand; ++s) four = out->strandPointCount(s) == n;
            if (four) {
                frames.resize(4 * n);
                const Vec3f* pts[4];
                Vec3f normals[4];
                Frame* outs[4];
                for (int k = 0; k < 4; ++k) {
                    pts[k] = out->strandPoints(first + k);
                    normals[k] = seedOf(first + k, pts[k]);
                    outs[k] = frames.data() + k * n;
                }
                rotationMinimizingFrames4(pts, n, normals, outs);
                for (int k = 0; k < 4; ++k) curl(first + k, outs[k]);
                return;
            }
            for (std::size_t s = first; s < lastStrand; ++s) {
                const std::size_t count = out->strandPointCount(s);
                if (count < 2) continue;
                const Vec3f* pts = out->strandPoints(s);
                rotationMinimizingFramesInto(pts, count, seedOf(s, pts), frames);
                curl(s, frames.data());
            }
        }, 64);
        *result = StrandsPtr(out);
        return true;
    }

    // Coherent by clump: each clump's middle (the mean of its core strands, preview_core.h, at the
    // same share of their points, as Twist takes it: the same at every preview density), its
    // rotation-minimizing frames and arc length; every member then takes the clump's curl at its
    // own share of the length, so the clump curls as one. Strands in no clump (no channel value, or
    // a negative one), or in one without a core strand, curl on their own (`alone`).
    template <class Radius, class StrandRadius, class Alone>
    void curlByClump(const StrandSet& in, StrandSet& out, const Settings& set, const RampAlong& along, const Radius& radiusAt,
                     const StrandRadius& strandRadiusOf, const ScalpMesh* scalp, const Alone& alone) const {
        const std::string channel = params().getString("clumpChannel");
        const std::vector<float>* groups = in.strandChannel(channel);
        if (!groups) {
            throw GraphError("Curl: coherence clump needs a Clump before it (no '" + channel + "' channel to find the clumps)");
        }
        std::map<long long, std::size_t> slot;
        std::vector<long long> keys;
        std::vector<std::vector<std::size_t>> members;
        std::vector<std::size_t> clumpOf(in.strandCount(), SIZE_MAX);
        const PreviewCore core(in);
        for (std::size_t s = 0; s < in.strandCount(); ++s) {
            const long long key = std::llround((*groups)[s]);
            if (key < 0 || in.strandPointCount(s) < 2) continue;
            auto it = slot.find(key);
            if (it == slot.end()) {
                it = slot.emplace(key, members.size()).first;
                members.emplace_back();
                keys.push_back(key);
            }
            clumpOf[s] = it->second;
            if (core.core(s)) members[it->second].push_back(s);  // the middle's
        }
        for (std::size_t s = 0; s < in.strandCount(); ++s) {
            if (clumpOf[s] != SIZE_MAX && members[clumpOf[s]].empty()) clumpOf[s] = SIZE_MAX;  // no core strand: alone
        }
        // Each clump's middle, frames and arc length at its samples (members summed in strand order).
        std::vector<Vec3f> axes(members.size() * kAxisSamples);
        std::vector<Frame> frames(axes.size());
        std::vector<float> arcs(axes.size());
        std::vector<Draw> draws(members.size());
        parallelFor(0, members.size(), [&](std::size_t c) {
            if (members[c].empty()) return;
            Vec3f* axis = axes.data() + c * kAxisSamples;
            for (const std::size_t s : members[c]) {
                const Vec3f* pts = in.strandPoints(s);
                const std::size_t n = in.strandPointCount(s);
                for (std::size_t k = 0; k < kAxisSamples; ++k) axis[k] += pointAlong(pts, n, static_cast<float>(k) / (kAxisSamples - 1));
            }
            for (std::size_t k = 0; k < kAxisSamples; ++k) axis[k] = axis[k] / static_cast<float>(members[c].size());
            // Its frames from its members' mean root normal (summed in strand order; frameSeed).
            Vec3f normal;
            for (const std::size_t s : members[c]) normal += rootNormalOr(scalp, in.roots()[s]);
            std::vector<Frame> one;
            rotationMinimizingFramesInto(axis, kAxisSamples, frameSeed(normal.normalized(), axis[0], axis[1]), one);
            std::copy(one.begin(), one.end(), frames.begin() + static_cast<std::ptrdiff_t>(c * kAxisSamples));
            cumulativeArcLength(axis, kAxisSamples, arcs.data() + c * kAxisSamples);
            // A clump's draw: from its key, apart from any strand ID's.
            draws[c] = drawFor(hashCombine(hash64(static_cast<std::uint64_t>(keys[c])), 0xC1D0C0E5ull), set);
        }, 4);
        parallelFor(0, in.strandCount(), [&](std::size_t s) {
            if (clumpOf[s] == SIZE_MAX) {
                alone(s);
                return;
            }
            const std::size_t c = clumpOf[s], n = out.strandPointCount(s);
            const Draw& d = draws[c];
            const Frame* fr = frames.data() + c * kAxisSamples;
            const float* arc = arcs.data() + c * kAxisSamples;
            const float strandRadius = strandRadiusOf(s);
            Vec3f* pts = out.strandPoints(s);
            for (std::size_t i = 1; i < n; ++i) {
                // The clump's curl where this point is along it: its share of the length.
                const float x = paramAt(i, n) * static_cast<float>(kAxisSamples - 1);
                const std::size_t k = std::min(static_cast<std::size_t>(x), kAxisSamples - 2);
                const float f = x - static_cast<float>(k);
                const float clumpArc = arc[k] + (arc[k + 1] - arc[k]) * f;
                const float angle = kTwoPi * d.frequency * clumpArc + d.phase;
                const float ri = radiusAt(s, i, strandRadius) * d.vary * along(i, n);
                pts[i] = pts[i] + offsetAt(set.shape, frameBetween(fr[k], fr[k + 1], f), ri, angle, d.plane);
            }
        });
    }

    mutable std::atomic<bool> unitWidths_{false};
};

inline void registerCurlLegacy() {
    OperatorRegistry::instance().registerType("CurlLegacy", [] { return std::make_unique<CurlLegacyOp>(); });
}

}  // namespace legacy
