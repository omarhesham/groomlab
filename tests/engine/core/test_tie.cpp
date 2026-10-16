// SPDX-License-Identifier: Apache-2.0
// Tie (tie_op.cpp): ponytails and hair ties. The strands it affects run along the scalp to a ring,
// pass through it in the order they came from and fan into a tail, each keeping its length.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if GROOMLAB_HAS_TBB
#include <tbb/task_arena.h>
#endif

#include "groomlab/graph.h"
#include "groomlab/graph_io.h"
#include "groomlab/operators.h"
#include "test_helpers.h"

namespace groomlab {
namespace {

constexpr float kPi = 3.14159265f;
constexpr float kHead = 10.0f;  // the sphere scalp's radius (a 20-unit head)

std::string vec(const Vec3f& v) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "[%.6g, %.6g, %.6g]", v.x, v.y, v.z);
    return buf;
}

// A sphere head `scale` x kHead in radius, straight strands `length` x scale long (63 segments,
// about 3800 of them) through the nodes `nodes` (JSON array items), the first linked from the
// scatter by `connections` (JSON items). Returns the output node's strands; *input the scatter's.
StrandsPtr run(const std::string& nodes, const std::string& connections, const std::string& output, float scale = 1.0f,
               float length = 32.0f, StrandsPtr* input = nullptr) {
    char base[512];
    std::snprintf(base, sizeof base,
                  R"({"name": "scalp", "type": "Scalp"},
                     {"name": "segments", "type": "Resample", "params": {"segments": 63}},
                     {"name": "scatter", "type": "Scatter", "params": {"density": %.9g, "length": %.9g}})",
                  3.0 / (static_cast<double>(scale) * scale), static_cast<double>(length * scale));
    const std::string graph = std::string(R"({"version": 1, "output": ")") + output + R"(", "nodes": [)" + base + "," + nodes +
                              R"(], "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"},)" + connections + "]}";
    GraphDocument doc = loadGraphJson(graph);
    ExternalInputs ext;
    ext.set("scalp", test::makeSphere(48, 96, kHead * scale), 1);
    EvalContext ctx;
    ctx.externals = &ext;
    if (input) *input = doc.graph.evaluateStrands(doc.nodesByName.at("scatter"), ctx);
    return doc.graph.evaluateStrands(doc.nodesByName.at(output), ctx);
}

std::string link(const char* from, const char* to) {
    return std::string(R"({"from": ")") + from + R"(", "to": ")" + to + R"(", "input": "strands"})";
}

// A Tie node named `name` at `center` with `radius` and extra params (JSON object items, leading comma).
std::string tieNode(const char* name, const Vec3f& center, float radius, const std::string& extra = {}) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.6g", static_cast<double>(radius));
    return std::string(R"({"name": ")") + name + R"(", "type": "Tie", "params": {"center": )" + vec(center) + R"(, "radius": )" +
           buf + extra + "}}";
}

// The least distance from the strand's segments to c.
float closestApproach(const StrandSet& s, std::size_t strand, const Vec3f& c, Vec3f* where = nullptr) {
    const Vec3f* p = s.strandPoints(strand);
    float best = 1e30f;
    for (std::size_t i = 1; i < s.strandPointCount(strand); ++i) {
        const Vec3f d = p[i] - p[i - 1];
        const float len2 = d.dot(d);
        const float u = len2 > 0.0f ? std::clamp((c - p[i - 1]).dot(d) / len2, 0.0f, 1.0f) : 0.0f;
        const Vec3f q = p[i - 1] + d * u;
        const float dist = (q - c).length();
        if (dist < best) {
            best = dist;
            if (where) *where = q;
        }
    }
    return best;
}

float tipTail(const StrandSet& s, std::size_t strand, const char* channel = "tail") {
    const std::vector<float>* t = s.pointChannel(channel);
    return t ? (*t)[s.strandBegin(strand) + s.strandPointCount(strand) - 1] : -1.0f;
}

const Vec3f kBack{0.0f, 2.0f, -12.0f};  // behind the head, a little up

TEST(Tie, EmptyMaskIsTheStrandsAsTheyCame) {
    // A mask that is 0 everywhere (an expression: the node runs and ties nothing), and an affect of 0.
    for (const char* affect : {R"({"expression": "x > 1000"})", "0.0"}) {
        StrandsPtr in;
        const StrandsPtr out = run(tieNode("tie", kBack, 1.2f, std::string(R"(, "affect": )") + affect), link("scatter", "tie"), "tie",
                                   1.0f, 32.0f, &in);
        ASSERT_TRUE(in && out);
        ASSERT_EQ(out->pointCount(), in->pointCount());
        EXPECT_EQ(std::memcmp(out->positions().data(), in->positions().data(), in->pointCount() * sizeof(Vec3f)), 0) << affect;
        EXPECT_EQ(out->pointChannelNames(), in->pointChannelNames()) << affect;
    }
}

TEST(Tie, StrandsPassWithinTheRingInOrder) {
    // At tightness 1 every strand long enough passes within the radius of the center, at the
    // azimuth about the tie's axis its root has (the axis from the head's center through the tie):
    // the strands keep their arrangement through the ring, none crossing another.
    StrandsPtr in;
    const float radius = 1.2f;
    const StrandsPtr out = run(tieNode("tie", kBack, radius), link("scatter", "tie"), "tie", 1.0f, 32.0f, &in);
    ASSERT_TRUE(in && out);
    ASSERT_GT(out->strandCount(), 3000u);
    const Vec3f axis = kBack.normalized();
    Vec3f u1 = Vec3f{0.0f, 1.0f, 0.0f}.cross(axis).normalized();
    const Vec3f u2 = axis.cross(u1);
    auto azimuth = [&](const Vec3f& v) { return std::atan2(v.dot(u2), v.dot(u1)); };
    std::size_t reached = 0, outside = 0, turned = 0, measured = 0;
    float worst = 0.0f;
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        if (tipTail(*out, s) < 0.99f) continue;  // not past the ring
        ++reached;
        Vec3f at;
        const float d = closestApproach(*out, s, kBack, &at);
        worst = std::max(worst, d);
        if (d > radius) ++outside;
        const Vec3f off = at - kBack - axis * (at - kBack).dot(axis);
        if (off.length() < 0.3f * radius) continue;  // too near the axis for an azimuth
        const Vec3f root = in->strandPoints(s)[0];
        const Vec3f rootOff = root - axis * root.dot(axis);
        if (rootOff.length() < 0.05f * kHead) continue;
        ++measured;
        float diff = std::fabs(azimuth(off) - azimuth(rootOff));
        diff = std::min(diff, 2.0f * kPi - diff);
        if (diff > 10.0f * kPi / 180.0f) ++turned;
    }
    std::printf("[ tie ] %zu of %zu strands past the ring, the farthest %.3f from its center (radius %.2f); %zu of %zu turned\n",
                reached, out->strandCount(), worst, radius, turned, measured);
    EXPECT_GT(reached, out->strandCount() * 3 / 4);
    EXPECT_EQ(outside, 0u);
    EXPECT_GT(measured, reached / 2);
    EXPECT_LT(turned, measured / 100 + 1);
}

TEST(Tie, LengthsAndRootsKeptAndOutOfTheScalp) {
    StrandsPtr in;
    const StrandsPtr out = run(tieNode("tie", kBack, 1.2f, R"(, "twist": 0.1, "spread": 20.0)"), link("scatter", "tie"), "tie", 1.0f,
                               32.0f, &in);
    ASSERT_TRUE(in && out);
    ASSERT_EQ(out->offsets(), in->offsets());
    float worstLength = 0.0f, deepest = 1e9f;
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        const Vec3f* a = in->strandPoints(s);
        const Vec3f* b = out->strandPoints(s);
        ASSERT_EQ(a[0], b[0]) << "root " << s << " moved";
        for (std::size_t i = 1; i < out->strandPointCount(s); ++i) {
            const float la = (a[i] - a[i - 1]).length(), lb = (b[i] - b[i - 1]).length();
            worstLength = std::max(worstLength, std::fabs(lb - la) / la);
            deepest = std::min(deepest, b[i].length() - kHead);
        }
    }
    EXPECT_LT(worstLength, 1e-4f);
    EXPECT_GT(deepest, 0.0f) << "a point went into the scalp";
}

TEST(Tie, ShortStrandsStopShortAlongTheRoute) {
    // Strands too short to reach the ring end on the way to it: their lengths kept (no stretching),
    // their tips nearer the tie than they were, none of them past the ring.
    StrandsPtr in;
    const StrandsPtr out = run(tieNode("tie", kBack, 1.2f), link("scatter", "tie"), "tie", 1.0f, 4.0f, &in);
    ASSERT_TRUE(in && out);
    std::size_t nearer = 0, past = 0, tied = 0;
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        const std::size_t n = out->strandPointCount(s);
        const Vec3f root = out->strandPoints(s)[0];
        if ((root - kBack).length() < 6.0f) continue;  // roots near the tie reach it
        ++tied;
        float length = 0.0f, inLength = 0.0f;
        for (std::size_t i = 1; i < n; ++i) {
            length += (out->strandPoints(s)[i] - out->strandPoints(s)[i - 1]).length();
            inLength += (in->strandPoints(s)[i] - in->strandPoints(s)[i - 1]).length();
        }
        EXPECT_NEAR(length, inLength, 1e-4f * inLength);
        if ((out->strandPoints(s)[n - 1] - kBack).length() < (in->strandPoints(s)[n - 1] - kBack).length()) ++nearer;
        if (tipTail(*out, s) > 0.5f) ++past;
    }
    ASSERT_GT(tied, 1000u);
    EXPECT_GT(nearer, tied * 9 / 10);
    EXPECT_EQ(past, 0u);
}

TEST(Tie, TailAttributeRunsFromRootToTip) {
    // 0 at the root, 1 at the tip of every tied strand that passes the ring, rising once; Tie lists
    // it as an attribute it writes.
    const StrandsPtr out = run(tieNode("tie", kBack, 1.2f), link("scatter", "tie"), "tie");
    ASSERT_TRUE(out);
    const std::vector<float>* tail = out->pointChannel("tail");
    ASSERT_TRUE(tail);
    std::size_t passed = 0;
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        const std::size_t b = out->strandBegin(s), n = out->strandPointCount(s);
        EXPECT_EQ((*tail)[b], 0.0f);
        if ((*tail)[b + n - 1] < 1.0f) continue;
        ++passed;
        for (std::size_t i = 1; i < n; ++i) ASSERT_GE((*tail)[b + i], (*tail)[b + i - 1]) << "strand " << s;
    }
    EXPECT_GT(passed, out->strandCount() / 2);
    const std::unique_ptr<Operator> tie = OperatorRegistry::instance().create(op::kTie);
    const std::vector<ChannelWrite> writes = tie->channelWrites();
    ASSERT_EQ(writes.size(), 1u);
    EXPECT_EQ(writes[0].name, "tail");
    EXPECT_EQ(writes[0].param, "tailChannel");
}

TEST(Tie, DoublePonytail) {
    // Two Ties, the left half of the head to a tie on the left, the right half to one on the right:
    // each half's strands pass through their own tie, the other half's are left to the other.
    const Vec3f left{-11.0f, 3.0f, -4.0f}, right{11.0f, 3.0f, -4.0f};
    const StrandsPtr out = run(tieNode("left", left, 1.0f, R"(, "affect": {"expression": "x < 0 ? 1 : 0"})") + "," +
                                   tieNode("right", right, 1.0f, R"(, "affect": {"expression": "x < 0 ? 0 : 1"})"),
                               link("scatter", "left") + "," + link("left", "right"), "right");
    ASSERT_TRUE(out);
    std::size_t lefts = 0, rights = 0;
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        if (tipTail(*out, s) < 0.99f) continue;
        const bool isLeft = out->strandPoints(s)[0].x < 0.0f;
        EXPECT_LT(closestApproach(*out, s, isLeft ? left : right), 1.0f) << "strand " << s;
        (isLeft ? lefts : rights) += 1;
    }
    EXPECT_GT(lefts, out->strandCount() / 4);
    EXPECT_GT(rights, out->strandCount() / 4);
}

TEST(Tie, SecondTieDownTheTail) {
    // A second Tie further down the first one's tail (bubble ponytail): strands that pass both pass
    // within each one's radius, their lengths kept; the tail attribute marks the part past the second.
    const Vec3f first = kBack, second = kBack + kBack.normalized() * 8.0f;
    StrandsPtr in;
    const StrandsPtr one = run(tieNode("a", first, 1.2f), link("scatter", "a"), "a", 1.0f, 40.0f);
    const StrandsPtr out = run(tieNode("a", first, 1.2f) + "," + tieNode("b", second, 0.8f), link("scatter", "a") + "," + link("a", "b"),
                               "b", 1.0f, 40.0f, &in);
    ASSERT_TRUE(one && out && in);
    std::size_t both = 0;
    float worstLength = 0.0f;
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        for (std::size_t i = 1; i < out->strandPointCount(s); ++i) {
            const float la = (in->strandPoints(s)[i] - in->strandPoints(s)[i - 1]).length();
            const float lb = (out->strandPoints(s)[i] - out->strandPoints(s)[i - 1]).length();
            worstLength = std::max(worstLength, std::fabs(lb - la) / la);
        }
        if (tipTail(*out, s) < 0.99f) continue;
        ++both;
        EXPECT_LT(closestApproach(*out, s, first), 1.2f) << "strand " << s;
        EXPECT_LT(closestApproach(*out, s, second), 0.8f) << "strand " << s;
        // Up to the first tie the strand is as the first Tie made it.
        const std::size_t n = out->strandPointCount(s);
        const std::vector<float>& tailOne = *one->pointChannel("tail");
        for (std::size_t i = 0; i < n && tailOne[one->strandBegin(s) + i] < 0.5f; ++i) {
            ASSERT_EQ(out->strandPoints(s)[i], one->strandPoints(s)[i]) << "strand " << s << " point " << i;
        }
    }
    EXPECT_GT(both, out->strandCount() / 2);
    EXPECT_LT(worstLength, 1e-4f);
}

TEST(Tie, ScaleInvariant) {
    // The same groom 100 times as big (head, length, density, center, radius and ease fitted) makes
    // the same strands, 100 times as far apart.
    const std::string extra = R"(, "twist": 0.05, "ease": 0.5)";
    const StrandsPtr a = run(tieNode("tie", kBack, 1.2f, extra), link("scatter", "tie"), "tie", 1.0f);
    const StrandsPtr b = run(tieNode("tie", kBack * 100.0f, 120.0f, R"(, "twist": 0.0005, "ease": 50.0)"), link("scatter", "tie"), "tie",
                             100.0f);
    ASSERT_TRUE(a && b);
    ASSERT_EQ(a->ids(), b->ids());
    ASSERT_EQ(a->pointCount(), b->pointCount());
    float worst = 0.0f;
    for (std::size_t i = 0; i < a->pointCount(); ++i) worst = std::max(worst, (b->positions()[i] / 100.0f - a->positions()[i]).length());
    std::printf("[ tie ] scaled 100x: the farthest point %.2e apart (head %.0f across)\n", worst, 2.0f * kHead);
    EXPECT_LT(worst, 2e-3f * kHead);
}

TEST(Tie, DeterministicOnAnyNumberOfThreads) {
    const std::string nodes = tieNode("tie", kBack, 1.2f, R"(, "twist": 0.1)");
    const StrandsPtr many = run(nodes, link("scatter", "tie"), "tie");
    ASSERT_TRUE(many);
    StrandsPtr one;
#if GROOMLAB_HAS_TBB
    tbb::task_arena single(1);
    single.execute([&] { one = run(nodes, link("scatter", "tie"), "tie"); });
#else
    one = run(nodes, link("scatter", "tie"), "tie");
#endif
    ASSERT_TRUE(one);
    ASSERT_EQ(one->pointCount(), many->pointCount());
    EXPECT_EQ(std::memcmp(one->positions().data(), many->positions().data(), many->pointCount() * sizeof(Vec3f)), 0);
    EXPECT_EQ(*one->pointChannel("tail"), *many->pointChannel("tail"));
}

}  // namespace
}  // namespace groomlab
