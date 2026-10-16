// SPDX-License-Identifier: Apache-2.0
// Braid (Strands + Guides -> Strands): braids strands along braid paths.
//
// Each path (a guide, e.g. from a "braids" guide group) gathers the strands
// rooted within `region` of its root. Without a braids input it picks its own
// paths among the strands, about `region` apart over the scalp (as a Clump
// picks its centers by size): box braids all over the head with nothing drawn. Along the path, the strands split into
// lobes that weave around it in the path's rotation-minimizing frame:
//   three     classic three-strand braid (lobes trace figure-eights)
//   fishtail  two sections exchanging fine strips
//   rope      two lobes twisted around each other
//   cornrow   a three-strand braid flattened onto the scalp (Dutch: raised)
// `dutch` crosses the lobes under instead of over (a raised braid). Strands
// leave their roots over the first `gather` of their length, the braid
// narrows along `widthProfile`, and over the last `tail` the lobes let go and the
// ends fan out. A share of strands (`strays`) stay loose.
//
// With `feed`, strands rooted anywhere along a path (within `feedDistance` of
// its first `feedUntil` share) join the braid where they are and run along it
// from there, as hair is picked up into cornrows and French braids; past
// `feedUntil` the braid hangs free. The root region (`region`) joins in both
// modes. With `endAtPath` (the default) each fed strand is fitted to the path:
// it ends within the braid's last stretch (`tail`), wherever it joined;
// otherwise longer strands hang straight on past the path's end.
//
// Strands drawn in from a distant root (a ponytail's, or fed from the root)
// would cut straight through the head on their way: with `alongScalp` the
// gathering stretch is pressed onto the scalp (plus a clearance), so the hair
// sweeps over the head into the braid.
//
// The look: `sections` weaves more strands (a four- or five-strand braid, a
// three-ply rope), `depth` rounds or flattens the braid's cross-section,
// `phase` slides the weave along it, `crossingsProfile` varies the crossings along
// its length, and `sectionTwist` twists each section's hair around itself as
// it weaves (as real sections do).
//
// Output channels, per strand: `braidId` (the path's index among the guides, or for paths of its own a
// number from the picked strand's ID; -1 when not
// braided) and `braidFlat` (1 for a cornrow, lying on the scalp), for nodes
// downstream (Cards builds braid cards from them).

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "groomlab/curve.h"
#include "groomlab/frames.h"
#include "groomlab/graph.h"
#include "groomlab/operators.h"
#include "groomlab/parallel.h"
#include "groomlab/point_index.h"
#include "groomlab/portable_math.h"
#include "groomlab/random.h"
#include "incremental.h"
#include "internal.h"

namespace groomlab {

namespace {

constexpr float kTwoPi = 6.2831853f;

enum class Style { Three, Fishtail, Rope, Cornrow };

Style styleFrom(const std::string& name) {
    if (name == "three") return Style::Three;
    if (name == "fishtail") return Style::Fishtail;
    if (name == "rope") return Style::Rope;
    if (name == "cornrow") return Style::Cornrow;
    throw GraphError("Braid: style must be three, fishtail, rope or cornrow");
}

// Sections: `sections` (0: the style's own), fishtail always two.
int lobeCount(Style s, std::int64_t sections) {
    if (s == Style::Fishtail) return 2;
    const int base = s == Style::Rope ? 2 : 3;
    return sections <= 0 ? base : static_cast<int>(std::clamp<std::int64_t>(sections, 2, 8));
}

// A lobe's center across the braid ([0]: side to side, [1]: depth, out of the
// scalp), in braid widths, at weave phase `theta`. Woven braids send each of
// `lobes` sections across in turn: two sections meeting side to side are
// always at opposite depths, so one crosses over the other.
std::array<float, 2> lobeCenter(Style style, int lobe, int lobes, float theta, float depthSign) {
    switch (style) {
        case Style::Three:
        case Style::Cornrow: {
            const float phase = theta + kTwoPi * static_cast<float>(lobe) / static_cast<float>(lobes);
            float depth = 0.35f * pm::sin(2.0f * phase) * depthSign;
            if (style == Style::Cornrow) depth = 0.25f * (0.5f + 0.5f * pm::sin(2.0f * phase));  // above the scalp
            return {0.55f * pm::sin(phase), depth};
        }
        case Style::Fishtail: {
            // Two sections side by side; each sends a strip across, alternating.
            const float side = lobe == 0 ? -1.0f : 1.0f;
            const float phase = 3.0f * theta + 3.14159265f * static_cast<float>(lobe);
            return {side * (0.28f - 0.12f * pm::sin(phase)), 0.18f * pm::sin(2.0f * phase) * depthSign};
        }
        case Style::Rope: {  // plies twisted around each other: wider with more of them
            const float phase = theta + kTwoPi * static_cast<float>(lobe) / static_cast<float>(lobes);
            const float r = 0.3f + 0.06f * static_cast<float>(lobes - 2);
            return {r * pm::cos(phase), r * pm::sin(phase) * depthSign};
        }
    }
    return {0.0f, 0.0f};
}


struct BraidPath {
    std::vector<Vec3f> points;  // resampled evenly
    std::vector<Frame> frames;
    float length = 0.0f;
    Vec3f root;
    StrandId id = 0;           // the guide's (a picked strand's) ID
    std::vector<float> turns;  // weave travelled at each point (tightness integrated); empty: the distance

    // Weave travelled `at` a distance along the path (past its end: at the end's rate).
    float weaveAt(float at, const Ramp& tightness) const {
        if (turns.empty()) return at;
        const std::size_t last = points.size() - 1;
        if (at >= length) return turns.back() + (at - length) * tightness(1.0f);
        const float f = std::max(at, 0.0f) / length * static_cast<float>(last);
        const std::size_t k = std::min(static_cast<std::size_t>(f), last - 1);
        const float w = f - static_cast<float>(k);
        return turns[k] * (1.0f - w) + turns[k + 1] * w;
    }
};

// Everything that shapes the weave, shared by both modes.
struct BraidLook {
    Style style;
    int lobes;
    float depthSign, depth, phase, sectionTwist;
    Ramp tightness;

    // A strand's offset from the path, in the frame's (depth, side) plane, before the fan-out.
    Vec3f offset(int lobe, const Vec3f& inLobe, float weave, float wid) const {
        const float theta = kTwoPi * (weave + phase);
        const auto lc = lobeCenter(style, lobe, lobes, theta, depthSign);
        Vec3f spot = inLobe;
        if (sectionTwist != 0.0f) {  // the section's hair turning around its own center
            const float a = sectionTwist * theta, c = pm::cos(a), s = pm::sin(a);
            spot = {inLobe.x * c - inLobe.y * s, inLobe.x * s + inLobe.y * c, 0.0f};
        }
        // Cornrows lie on the scalp: their strands only ever stack outward.
        const float lobeDepth = style == Style::Cornrow ? std::abs(spot.y) * 0.6f : spot.y;
        return {(lc[0] + spot.x) * wid, (lc[1] + lobeDepth) * depth * wid, 0.0f};
    }
};

// The gathering stretch pressed over the scalp: a point on its way into the
// braid (blend strictly between 0 and 1) is held `clearance` above the scalp,
// rising to where the braid has it as the blend completes; a point that would
// cut through the head is lifted out. Only these points query the scalp.
struct ScalpGather {
    const ScalpMesh* scalp = nullptr;  // null: straight lines
    float clearance = 0.0f;

    void apply(Vec3f& p, float blend) const {
        if (!scalp || !(blend > 0.0f) || !(blend < 1.0f)) return;
        const SurfacePoint sp = scalp->closestPoint(p);
        if (sp.root.face < 0) return;
        const float height = (p - sp.position).dot(sp.normal);
        const float held = clearance + std::max(height - clearance, 0.0f) * blend;
        p = p + sp.normal * (held - height);
    }
};

// Per-strand output channels for nodes downstream.
struct BraidChannels {
    std::vector<float>* id;    // the path braided into, -1 when loose
    std::vector<float>* flat;  // 1 when the braid lies on the scalp (cornrow)
    int first = 0;             // this node's ids start here (after an earlier Braid node's)
    // Paths of its own: each path's number, from its strand's ID (stableGroupNumbers), so an edit
    // of the scalp in one place renumbers no braid elsewhere; null: the path's index (the guides').
    const std::vector<float>* numbers = nullptr;
    void set(std::size_t s, int path, Style style) const {
        (*id)[s] = static_cast<float>(first) + (numbers ? (*numbers)[static_cast<std::size_t>(path)] : static_cast<float>(path));
        (*flat)[s] = style == Style::Cornrow ? 1.0f : 0.0f;
    }
};

bool isConstant(const Ramp& r) {
    for (const auto& key : r.keys) {
        if (key.second != r.keys.front().second) return false;
    }
    return true;
}

class BraidOp : public Operator {
public:
    std::string typeName() const override { return op::kBraid; }
    std::vector<ChannelWrite> channelWrites() const override { return {{"braidId", ""}, {"braidFlat", ""}}; }
    std::string category() const override { return "look"; }
    std::vector<std::string> externalInputs() const override { return {"scalp"}; }
    std::vector<PortSpec> inputs() const override {
        return {{"strands", PortType::Strands}, {"braids", PortType::Guides, true}};
    }
    std::vector<ParamSpec> paramSpecs() const override {
        return {
            {"style", std::string("three"), "three, fishtail, rope or cornrow", "", {"three", "fishtail", "rope", "cornrow"}},
            notAnimatable({"sections", std::int64_t{0}, "Sections woven, 2-8 (0: the style's own: three for braids and cornrows, two "
                                                        "plies for rope; fishtail is always two)"})
                .range(0, 8)
                .when("style", {"three", "rope", "cornrow"}),
            ParamSpec{"dutch", false, "Cross the lobes under instead of over (a raised braid)"}
                .when("style", {"three", "fishtail", "rope"}),
            ParamSpec{"depth", 1.0, "The braid's thickness out of the scalp against its width: below 1 flatter, above "
                                    "rounder"}
                .atLeast(0)
                .soft(0, 2)
                .in("Weave"),
            ParamSpec{"phase", 0.0, "Slides the weave along the braid, in repeats (0.5: half a crossing)", "turns"}
                .soft(0, 1)
                .in("Weave"),
            ParamSpec{"crossingsProfile", Ramp::constant(1.0f), "Crossings along the braid, start to end, times `crossings`"}
                .in("Weave"),
            ParamSpec{"sectionTwist", 0.0, "Turns of each section's hair around its own center per weave repeat "
                                           "(signed)", "turns"}
                .soft(-2, 2)
                .in("Weave"),
            ParamSpec{"width", 0.12, "Braid width at its start, scene units", "length"}.soft(0, 0.5),
            {"widthProfile", Ramp::linear(1.0f, 0.55f), "Width along the braid, start to end"},
            ParamSpec{"crossings", 2.5, "Weave repeats per unit of braid length (tighter: more)", "perLength"}.soft(0, 10),
            ParamSpec{"lobeFill", 0.6, "How much of a lobe's space its strands fill, 0 to 1", "fraction"}
                .range(0, 1)
                .in("Weave"),
            ParamSpec{"region", 0.25, "Strands rooted within this distance of a braid's root join it (large: a "
                                      "ponytail's); without a braids input, the braids it picks among the strands are "
                                      "about this far apart", "length"}
                .soft(0, 2),
            ParamSpec{"feed", false, "Pick up strands all along the path, where they grow (cornrows, French braids)"}
                .in("Feed"),
            ParamSpec{"feedDistance", 0.25, "With feed: strands rooted within this distance of the path join it where "
                                            "they grow", "length"}
                .soft(0, 1)
                .in("Feed")
                .when("feed", {"true"}),
            ParamSpec{"feedUntil", 1.0, "With feed: pick up strands along this share of the path only; past it the "
                                        "braid hangs free (0: only the root region, a ponytail braid)", "fraction"}
                .range(0, 1)
                .in("Feed")
                .when("feed", {"true"}),
            ParamSpec{"endAtPath", true, "With feed: strands end where the braid path ends (staggered over the tail) "
                                         "instead of hanging on past it"}
                .in("Feed")
                .when("feed", {"true"}),
            ParamSpec{"alongScalp", true, "Strands drawn in from afar sweep over the scalp into the braid instead of "
                                          "cutting through the head"}
                .in("Strands"),
            ParamSpec{"gather", 0.15, "Share of the strand spent leaving the scalp into the braid", "fraction"}
                .range(0, 0.9)
                .in("Strands"),
            ParamSpec{"tail", 0.12, "Share at the end where the lobes let go and the ends fan out", "fraction"}
                .range(0, 0.9)
                .in("Strands"),
            ParamSpec{"strays", 0.03, "Share of the region's strands left loose", "fraction"}.range(0, 1).in("Strands"),
            ParamSpec{"fuzz", 0.02, "Random wander within the lobes, scene units", "length"}.soft(0, 0.1).in("Strands"),
            notAnimatable({"seed", std::int64_t{0}, "Random seed"}).in("Strands").adv(),
        };
    }

    Value evaluate(const EvalContext& ctx, const std::vector<Value>& inputs) const override {
        Value out;
        run(ctx, inputs, nullptr, &out, nullptr);
        return out;
    }
    // Without feed each strand is braided by its own root alone (the nearest path's root within
    // the region): while the paths stay, the strands that changed are braided again and the rest
    // keep the last result's points.
    bool evaluateIncremental(const EvalContext& ctx, const std::vector<Value>& inputs, const IncrementalInputs& incremental,
                             Value* out, StrandChanges* changed) const override {
        return run(ctx, inputs, &incremental, out, changed);
    }

private:
    bool run(const EvalContext& ctx, const std::vector<Value>& inputs, const IncrementalInputs* incremental, Value* result,
             StrandChanges* changed) const {
        const StrandsPtr in = valueAs<StrandsPtr>(inputs.at(0));
        if (!in) throw GraphError("Braid: strands input not connected");
        GuidesPtr braids = valueAs<GuidesPtr>(inputs.at(1));
        auto handOn = [&] {  // nothing to braid along: the graph passes the input's changes on
            if (incremental) return false;
            *result = in;
            return true;
        };
        const ScalpMesh* scalp = valueAs<MeshPtr>(ctx.external("scalp")).get();
        const bool own = !braids;
        if (own) {  // no braids input: paths of its own, strands about `region` apart
            if (incremental) return false;
            const float spacing = static_cast<float>(params().getDouble("region"));
            if (!(spacing > 0.0f) || in->empty()) return handOn();
            auto picked = std::make_shared<GuideSet>();
            for (const std::size_t s : pickSpreadStrands(*in, scalp, spacing, static_cast<std::uint64_t>(params().getInt("seed")))) {
                const Vec3f* pts = in->strandPoints(s);
                picked->add(in->ids()[s], std::vector<Vec3f>(pts, pts + in->strandPointCount(s))).root = in->roots()[s];
            }
            braids = GuidesPtr(picked);
        }
        if (braids->empty()) return handOn();

        const Style style = styleFrom(params().getString("style"));
        const int lobes = lobeCount(style, params().getInt("sections"));
        const float depthSign = params().getBool("dutch") ? -1.0f : 1.0f;
        const BraidLook look{style,
                             lobes,
                             depthSign,
                             static_cast<float>(params().getDouble("depth")),
                             static_cast<float>(params().getDouble("phase")),
                             static_cast<float>(params().getDouble("sectionTwist")),
                             params().getRamp("crossingsProfile")};
        const float width = static_cast<float>(params().getDouble("width"));
        const Ramp taper = params().getRamp("widthProfile");
        const float crossings = static_cast<float>(params().getDouble("crossings"));
        const float lobeFill = static_cast<float>(params().getDouble("lobeFill"));
        const float region = static_cast<float>(params().getDouble("region"));
        const float gather = static_cast<float>(params().getDouble("gather"));
        const float tail = static_cast<float>(params().getDouble("tail"));
        const float strays = static_cast<float>(params().getDouble("strays"));
        const float fuzz = static_cast<float>(params().getDouble("fuzz"));
        const bool feed = params().getBool("feed");
        const float feedDistance = static_cast<float>(params().getDouble("feedDistance"));
        const float feedUntil = static_cast<float>(params().getDouble("feedUntil"));
        const bool endAtPath = params().getBool("endAtPath");
        const bool alongScalp = params().getBool("alongScalp");
        const auto seed = static_cast<std::uint64_t>(params().getInt("seed"));

        // Braid paths, evenly resampled, with frames facing out of the scalp.
        std::vector<BraidPath> paths;
        for (const Guide& g : braids->guides()) {
            if (g.points.size() < 2) continue;
            BraidPath p;
            const std::size_t samples = std::max<std::size_t>(64, g.points.size() * 4);
            p.points = resampleByArcLength(g.points.data(), g.points.size(), samples);
            p.length = arcLength(p.points.data(), p.points.size());
            if (!(p.length > 1e-6f)) continue;  // also NaN
            p.root = p.points.front();
            p.id = g.id;
            const bool rooted = scalp && scalp->contains(g.root);
            // Out of the scalp: at the root, or where the path starts when it is not bound.
            Vec3f up = rooted ? scalp->normal(g.root)
                              : (scalp && scalp->triangleCount() > 0 ? scalp->closestPoint(p.root).normal
                                                                     : anyPerpendicular((p.points[1] - p.points[0]).normalized()));
            // The frame's normal: the scalp normal, made perpendicular to the path's start.
            const Vec3f t0 = (p.points[1] - p.points[0]).normalized();
            up = up - t0 * up.dot(t0);
            if (up.length() < 1e-4f) up = anyPerpendicular(t0);
            p.frames = rotationMinimizingFrames(p.points.data(), p.points.size(), up.normalized());
            if (!isConstant(look.tightness) || look.tightness(0.0f) != 1.0f) {
                // The weave travelled along the path: tightness integrated over its length.
                const std::size_t count = p.points.size();
                const float step = p.length / static_cast<float>(count - 1);
                p.turns.assign(count, 0.0f);
                for (std::size_t k = 1; k < count; ++k) {
                    const float u0 = static_cast<float>(k - 1) / static_cast<float>(count - 1);
                    const float u1 = static_cast<float>(k) / static_cast<float>(count - 1);
                    p.turns[k] = p.turns[k - 1] + 0.5f * (std::max(look.tightness(u0), 0.0f) +
                                                          std::max(look.tightness(u1), 0.0f)) * step;
                }
            }
            paths.push_back(std::move(p));
        }
        if (paths.empty()) return handOn();

        // Incrementally: the same paths, no feed and no earlier Braid's ids (numbered on from
        // their highest).
        const StrandChanges* changes = nullptr;
        StrandsPtr last;
        if (incremental) {
            changes = someStrandsChanged(*incremental, 0);
            last = changes ? previousStrandsFor(*incremental, *in) : nullptr;
            if (!last || feed || !inputUnchanged(*incremental, 1) || in->strandChannel("braidId")) return false;
            if (changes->none()) {
                *result = last;
                *changed = *changes;
                return true;
            }
        }

        auto out = std::make_shared<StrandSet>(*in);
        // A Braid node after another keeps the earlier braids' ids and numbers its own on from
        // them, so every braid in the groom has an id of its own (the Cards node's card sets).
        int firstId = 0;
        std::vector<float> earlierIds, earlierFlat;
        if (const std::vector<float>* had = in->strandChannel("braidId")) {
            earlierIds = *had;
            if (const std::vector<float>* flat = in->strandChannel("braidFlat")) earlierFlat = *flat;
            float highest = -1.0f;  // of the core strands (preview_core.h): the same at every preview density
            const PreviewCore core(*in);
            for (std::size_t s = 0; s < earlierIds.size(); ++s) {
                if (core.core(s)) highest = std::max(highest, std::floor(earlierIds[s]));
            }
            firstId = static_cast<int>(groupNumberBase(static_cast<std::int64_t>(highest)));
        }
        BraidChannels channels{nullptr, nullptr, firstId};
        std::vector<float> numbers;
        if (own) {
            std::vector<std::uint64_t> ids(paths.size());
            for (std::size_t b = 0; b < paths.size(); ++b) ids[b] = paths[b].id;
            numbers = stableGroupNumbers(ids);
            channels.numbers = &numbers;
        }
        if (last) {
            positionsOfLast(*out, last, *incremental);
            channels.id = out->copyStrandChannelFrom("braidId", *last);
            channels.flat = out->copyStrandChannelFrom("braidFlat", *last);
            if (!channels.id || !channels.flat) return false;
        } else {
            channels.id = &out->addStrandChannel("braidId", -1.0f);
            channels.flat = &out->addStrandChannel("braidFlat", 0.0f);
        }
        if (!earlierIds.empty()) {
            *channels.id = earlierIds;
            if (earlierFlat.size() == channels.flat->size()) *channels.flat = earlierFlat;
        }
        const float region2 = region * region;
        ScalpGather overScalp;
        if (alongScalp && scalp && scalp->triangleCount() > 0) {
            overScalp.scalp = scalp;
            // (at least a hundredth of the width: a share of the braid, whatever the scene's scale)
            overScalp.clearance = 0.25f * width * std::max(taper(0.0f), 0.04f);
        }
        if (feed) {
            braidFed(*out, paths, region2, feedDistance * feedDistance, feedUntil, look, width, taper, crossings,
                     lobeFill, gather, tail, strays, fuzz, seed, endAtPath, overScalp, channels);
            *result = StrandsPtr(out);
            return true;
        }
        auto braidStrand = [&](std::size_t s) {
            if (last) {  // made again from its incoming points, loose until it joins a braid
                copyStrandPoints(*out, *in, s);
                (*channels.id)[s] = -1.0f;
                (*channels.flat)[s] = 0.0f;
            }
            Vec3f* pts = out->strandPoints(s);
            const std::size_t n = out->strandPointCount(s);
            if (n < 2) return;
            // The nearest braid root within the region.
            const Vec3f root = pts[0];
            int best = -1;
            float bestD2 = region2;
            for (std::size_t b = 0; b < paths.size(); ++b) {
                const float d2 = (paths[b].root - root).dot(paths[b].root - root);
                if (d2 <= bestD2) {
                    bestD2 = d2;
                    best = static_cast<int>(b);
                }
            }
            if (best < 0) return;
            const std::uint64_t id = out->ids()[s];
            Rng rng(hashCombine(id, seed ^ 0xB4A1D5ull));
            if (rng.nextFloat() < strays) return;
            channels.set(s, best, style);
            const BraidPath& path = paths[static_cast<std::size_t>(best)];
            const int lobe = static_cast<int>(rng.nextU64() % static_cast<std::uint64_t>(lobes));
            // A fixed spot within the lobe, plus a slow wander.
            const float a = kTwoPi * rng.nextFloat();
            const float r = lobeFill * std::sqrt(rng.nextFloat());
            const Vec3f inLobe{r * pm::cos(a), r * pm::sin(a), 0.0f};
            const float fanAngle = kTwoPi * rng.nextFloat();
            const float wanderPhase = kTwoPi * rng.nextFloat();

            const std::size_t lastSample = path.points.size() - 1;
            for (std::size_t i = 1; i < n; ++i) {
                const float u = static_cast<float>(i) / static_cast<float>(n - 1);
                const float f = u * static_cast<float>(lastSample);
                const std::size_t k = std::min(static_cast<std::size_t>(f), lastSample - 1);
                const float w = f - static_cast<float>(k);
                const Vec3f center = path.points[k] * (1.0f - w) + path.points[k + 1] * w;
                const Frame& fr = path.frames[w < 0.5f ? k : k + 1];
                const float wid = width * std::max(taper(u), 0.0f);
                const float weave = crossings * path.weaveAt(u * path.length, look.tightness);
                const float letGo = tail > 0.0f ? smoothstep(1.0f - tail, 1.0f, u) : 0.0f;  // lobes release, ends fan out
                const float wander = fuzz * pm::sin(wanderPhase + 7.0f * u);
                const Vec3f local = look.offset(lobe, inLobe, weave, wid) * (1.0f - letGo);
                const float fanDepth = style == Style::Cornrow ? std::abs(pm::sin(fanAngle)) : pm::sin(fanAngle);
                const Vec3f fan = Vec3f{pm::cos(fanAngle), fanDepth, 0.0f} * (letGo * wid * 0.9f);
                // Frame x is the normal (depth, out of the scalp), y the binormal (side to side).
                const Vec3f braided = center + fr.toWorld({local.y + fan.y, local.x + fan.x + wander, 0.0f});
                const float blend = smoothstep(0.0f, gather, u);  // from the root into the braid
                pts[i] = pts[i] * (1.0f - blend) + braided * blend;
                overScalp.apply(pts[i], blend);
            }
        };
        if (last) {
            forEachListed(changes->strands, braidStrand);
            *changed = *changes;
        } else {
            parallelFor(0, out->strandCount(), braidStrand);
        }
        *result = StrandsPtr(out);
        return true;
    }

    // Feed mode: each strand joins the path where it reaches it (or at the
    // point nearest its root) and runs along it by its own length.
    static void braidFed(StrandSet& out, const std::vector<BraidPath>& paths, float region2, float feedDistance2,
                         float feedUntil, const BraidLook& look, float width, const Ramp& taper, float crossings,
                         float lobeFill, float gather, float tail, float strays, float fuzz, std::uint64_t seed,
                         bool endAtPath, const ScalpGather& overScalp, const BraidChannels& channels) {
        const Style style = look.style;
        const int lobes = look.lobes;
        // The path samples a strand may be picked up at (the first `feedUntil`
        // of each path, past the root: that is the region's) in one index, the
        // roots in another, and each path's own (where a strand first reaches
        // its braid).
        std::vector<Vec3f> samples, roots;
        std::vector<std::pair<std::size_t, std::size_t>> sampleOf;  // (path, sample)
        std::vector<PointIndex> pathIndex;
        for (std::size_t b = 0; b < paths.size(); ++b) {
            const std::size_t last = paths[b].points.size() - 1;
            const std::size_t until = static_cast<std::size_t>(feedUntil * static_cast<float>(last) + 1e-3f);
            for (std::size_t k = 1; k <= std::min(until, last); ++k) {
                samples.push_back(paths[b].points[k]);
                sampleOf.emplace_back(b, k);
            }
            roots.push_back(paths[b].root);
            pathIndex.emplace_back(paths[b].points);
        }
        const PointIndex pickUp(samples);
        const PointIndex rootIndex(roots);
        parallelFor(0, out.strandCount(), [&](std::size_t s) {
            Vec3f* pts = out.strandPoints(s);
            const std::size_t n = out.strandPointCount(s);
            if (n < 2) return;
            // Picked up at the nearest path point within feedDistance, or from
            // the nearest root within region: whichever is closer.
            int best = -1;
            std::size_t bestK = 0;
            float bestD2 = std::numeric_limits<float>::infinity();
            {
                float d2 = 0.0f;
                const std::size_t q = pickUp.nearest(pts[0], &d2);
                if (q < sampleOf.size() && d2 <= feedDistance2) {
                    best = static_cast<int>(sampleOf[q].first);
                    bestK = sampleOf[q].second;
                    bestD2 = d2;
                }
                const std::size_t r = rootIndex.nearest(pts[0], &d2);
                if (r < roots.size() && d2 <= region2 && d2 < bestD2) {
                    best = static_cast<int>(r);
                    bestK = 0;
                    bestD2 = d2;
                }
            }
            if (best < 0) return;
            const std::uint64_t id = out.ids()[s];
            Rng rng(hashCombine(id, seed ^ 0xB4A1D5ull));
            if (rng.nextFloat() < strays) return;
            const BraidPath& path = paths[static_cast<std::size_t>(best)];
            const std::size_t last = path.points.size() - 1;
            const float step = path.length / static_cast<float>(last);
            const int lobe = static_cast<int>(rng.nextU64() % static_cast<std::uint64_t>(lobes));
            const float a = kTwoPi * rng.nextFloat();
            const float r = lobeFill * std::sqrt(rng.nextFloat());
            const Vec3f inLobe{r * pm::cos(a), r * pm::sin(a), 0.0f};
            const float fanAngle = kTwoPi * rng.nextFloat();
            const float wanderPhase = kTwoPi * rng.nextFloat();
            thread_local std::vector<float> along;  // arc length from the root to each point, as it came in
            along.resize(n);
            cumulativeArcLength(pts, n, along.data());
            const float strandLength = along[n - 1];
            const float gatherLength = std::max(std::max(gather, 1e-4f) * strandLength, 1e-30f);  // shares of the strand

            // Where the strand joins: where its own shape (e.g. guides combed
            // toward the braid) first comes within the braid, at the path point
            // there; failing that, from its root straight to the nearest point.
            float reach = -1.0f;  // distance along the strand
            std::size_t joinK = bestK;
            {
                const float within = 0.6f * width * std::max(taper(0.0f), 0.1f);
                const PointIndex& index = pathIndex[static_cast<std::size_t>(best)];
                for (std::size_t i = 0; i < n && reach < 0.0f; ++i) {
                    float d2 = 0.0f;
                    const std::size_t k = index.nearest(pts[i], &d2);
                    if (k <= last && d2 <= within * within) {
                        reach = along[i];
                        joinK = k;
                    }
                }
            }
            const float joinAt = static_cast<float>(joinK) * step;
            float approach, blendFrom, blendTo;
            const bool drawnIn = reach < 0.0f;
            if (!drawnIn) {  // following its own shape into the braid
                approach = reach;
                blendFrom = std::max(0.0f, reach - 0.5f * gatherLength);
                blendTo = reach + 0.5f * gatherLength;
            } else {  // drawn in from the root
                approach = std::sqrt(bestD2);
                blendFrom = 0.0f;
                blendTo = std::max(gatherLength, approach);
            }
            const Vec3f endTangent = (path.points[last] - path.points[last - 1]).normalized();
            // Fitted to the path: the strand's length is shared out so it ends within the braid's
            // last stretch (a random spot in the tail, so the end isn't cut square), never past it.
            // A strand joining at or past that end has no braid to run along: it stays loose.
            float squeeze = 1.0f;
            if (endAtPath) {
                const float endAt = path.length * (1.0f - 0.5f * tail * rng.nextFloat());
                if (joinAt >= endAt) return;
                const float room = approach + endAt - joinAt;
                if (strandLength > room && strandLength > 1e-6f) squeeze = room / strandLength;
            }
            channels.set(s, best, style);

            thread_local std::vector<Vec3f> original;
            original.assign(pts, pts + n);
            for (std::size_t i = 1; i < n; ++i) {
                const float fitted = along[i] * squeeze;
                const float at = joinAt + std::max(0.0f, fitted - approach);  // distance along the path
                Vec3f center;
                const Frame* fr;
                if (at < path.length) {
                    const float f = at / step;
                    const std::size_t k = std::min(static_cast<std::size_t>(f), last - 1);
                    const float w = f - static_cast<float>(k);
                    center = path.points[k] * (1.0f - w) + path.points[k + 1] * w;
                    fr = &path.frames[w < 0.5f ? k : k + 1];
                } else {  // hanging on past the end
                    center = path.points[last] + endTangent * (at - path.length);
                    fr = &path.frames[last];
                }
                const float u = std::min(at / path.length, 1.0f);
                const float wid = width * std::max(taper(u), 0.0f);
                const float weave = crossings * path.weaveAt(at, look.tightness);
                const float letGo = tail > 0.0f ? smoothstep((1.0f - tail) * path.length, path.length, at) : 0.0f;
                const float wander = fuzz * pm::sin(wanderPhase + 7.0f * u);
                const Vec3f local = look.offset(lobe, inLobe, weave, wid) * (1.0f - letGo);
                const float fanDepth = style == Style::Cornrow ? std::abs(pm::sin(fanAngle)) : pm::sin(fanAngle);
                const Vec3f fan = Vec3f{pm::cos(fanAngle), fanDepth, 0.0f} * (letGo * wid * 0.9f);
                const Vec3f braided = center + fr->toWorld({local.y + fan.y, local.x + fan.x + wander, 0.0f});
                const float blend = smoothstep(blendFrom, blendTo, fitted);
                pts[i] = original[i] * (1.0f - blend) + braided * blend;
                if (drawnIn) overScalp.apply(pts[i], blend);
            }
        });
    }
};

}  // namespace

void registerBraidOperator(OperatorRegistry& registry) {
    registry.registerType(op::kBraid, [] { return std::make_unique<BraidOp>(); });
}

}  // namespace groomlab
