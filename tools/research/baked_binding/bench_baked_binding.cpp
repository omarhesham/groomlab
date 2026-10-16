// SPDX-License-Identifier: Apache-2.0
// Research prototype (docs/research/baked_binding.md): Interpolate's binding baked on a lattice of
// samples over the scalp, each strand's binding a barycentric blend of its sub-triangle's three
// sample bindings, and (the "shape field") each sample's blended shape baked too, so a strand blends
// three curves instead of its guides. Measured against the current Interpolate (the operator itself:
// its binding records through operator_state.h, its output as the reference shape).
//
//   research_baked_binding [--quick] [--compact] [--out results.jsonl]
//   research_baked_binding --dump points.bin [strands]   (Interpolate's points on the app head)
//   research_baked_binding --compare a.bin b.bin         (two builds' dumps apart, per scenario)
//
// Everything here is read-only use of the core; nothing of the product changes.

#include <array>
#include <atomic>
#include <cstring>
#include <set>

#include "../common/research_common.h"
#include "groomlab/curve.h"
#include "groomlab/frames.h"
#include "groomlab/operator_state.h"
#include "groomlab/parts.h"

using namespace research;

namespace {

constexpr std::size_t kSlots = 16;  // the GPU's BIND_SLOTS (look.slang): what a record may hold

Value V(const StrandsPtr& s) { return Value(s); }

// ---- The blend of one strand from a record (InterpolateOp's reference path, point at a time) ----

struct Scratch {
    std::vector<Vec3f> blended;
};

// Strand s blended from guides `g` with normalized weights `w` (twists `tw`, may be null), as the
// operator's reference path makes it (keepClearance off, influence 1, preserveLength on).
void blendStrand(const InterpolateGuideRows& rows, const ScalpMesh& scalp, const StrandSet& strands, std::size_t s,
                 const std::uint32_t* g, const float* w, const float* tw, std::size_t count, Vec3f* out, Scratch& sc) {
    const std::size_t n = strands.strandPointCount(s);
    const Vec3f* in = strands.strandPoints(s);
    const std::size_t samples = rows.samples;
    const Vec3f root = in[0];
    const StrandRoot& r = strands.roots()[s];
    const Vec3f sn = scalp.contains(r) ? scalp.normal(r) : Vec3f{};
    const bool haveNormal = sn.length() > 0.0f;
    sc.blended.assign(samples, root);
    float guideLength = 0.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const std::size_t gi = g[i];
        const float wi = w[i];
        guideLength += rows.lengths[gi] * wi;
        const Vec3f* o = rows.offsets.data() + gi * samples;
        if (!haveNormal || rows.normals[gi].length() == 0.0f) {
            for (std::size_t j = 0; j < samples; ++j) sc.blended[j] += o[j] * wi;
        } else if (tw && tw[i] != 0.0f) {
            const TwistedTurn turn(rows.normals[gi], sn, tw[i]);
            for (std::size_t j = 0; j < samples; ++j) sc.blended[j] += turn(o[j]) * wi;
        } else {
            const Rotation turn(rows.normals[gi], sn);
            for (std::size_t j = 0; j < samples; ++j) sc.blended[j] += turn(o[j]) * wi;
        }
    }
    const float own = arcLength(sc.blended.data(), samples);
    if (own > 1e-8f && guideLength > 0.0f) {
        const float scale = std::min(guideLength / own, 2.0f);
        for (std::size_t j = 1; j < samples; ++j) sc.blended[j] = sc.blended[0] + (sc.blended[j] - sc.blended[0]) * scale;
    }
    resampleByArcLengthInto(sc.blended.data(), samples, out, n);
}

// ---- The lattice of samples over the scalp -------------------------------------------------------

// Level L: each triangle's lattice points (i/L, j/L) (u toward its second vertex, v its third), the
// points on shared edges and vertices one sample. Samples are bound as strands rooted there.
struct Lattice {
    int L = 1;
    std::vector<StrandRoot> roots;
    std::vector<std::uint32_t> triSamples;  // per triangle, (L+1)(L+2)/2 sample indices
    std::size_t perTri() const { return static_cast<std::size_t>((L + 1) * (L + 2) / 2); }
    std::size_t at(int i, int j) const {  // lattice index in a triangle: rows of i
        return static_cast<std::size_t>(i * (L + 1) - i * (i - 1) / 2 + j);
    }
};

Lattice makeLattice(const ScalpMesh& scalp, int L) {
    Lattice lat;
    lat.L = L;
    std::unordered_map<std::uint64_t, std::uint32_t> ids;
    lat.triSamples.resize(scalp.triangleCount() * lat.perTri());
    for (std::size_t t = 0; t < scalp.triangleCount(); ++t) {
        const auto& tri = scalp.triangles()[t];
        for (int i = 0; i <= L; ++i) {
            for (int j = 0; j <= L - i; ++j) {
                std::uint64_t key;
                auto edgeKey = [&](std::int32_t a, std::int32_t b, int k) {  // k steps from a toward b
                    if (a > b) {
                        std::swap(a, b);
                        k = L - k;
                    }
                    return (1ull << 62) | (std::uint64_t(a) << 36) | (std::uint64_t(b) << 12) | std::uint64_t(k);
                };
                if (i == 0 && j == 0) key = std::uint64_t(tri[0]);
                else if (i == L) key = std::uint64_t(tri[1]);
                else if (j == L) key = std::uint64_t(tri[2]);
                else if (j == 0) key = edgeKey(tri[0], tri[1], i);
                else if (i == 0) key = edgeKey(tri[0], tri[2], j);
                else if (i + j == L) key = edgeKey(tri[1], tri[2], j);
                else key = (2ull << 62) | (std::uint64_t(t) << 24) | (std::uint64_t(i) << 12) | std::uint64_t(j);
                auto [it, fresh] = ids.emplace(key, static_cast<std::uint32_t>(lat.roots.size()));
                if (fresh) {
                    StrandRoot r;
                    r.face = static_cast<std::int32_t>(t);
                    r.u = float(i) / float(L);
                    r.v = float(j) / float(L);
                    lat.roots.push_back(r);
                }
                lat.triSamples[t * lat.perTri() + lat.at(i, j)] = it->second;
            }
        }
    }
    return lat;
}

// The sub-triangle of root `r` and its three samples' barycentric weights.
void locate(const Lattice& lat, const StrandRoot& r, std::uint32_t c[3], float lambda[3]) {
    const int L = lat.L;
    const float x = std::clamp(r.u, 0.0f, 1.0f) * float(L), y = std::clamp(r.v, 0.0f, 1.0f) * float(L);
    int i = std::min(static_cast<int>(x), L - 1), j = std::min(static_cast<int>(y), L - 1);
    if (i + j > L - 1) {  // on the last diagonal (rounding): back inside
        if (i > 0) --i; else --j;
    }
    const float fx = x - float(i), fy = y - float(j);
    const std::uint32_t* tri = lat.triSamples.data() + static_cast<std::size_t>(r.face) * lat.perTri();
    if (fx + fy <= 1.0f || i + j + 2 > L) {
        c[0] = tri[lat.at(i, j)];
        c[1] = tri[lat.at(i + 1, j)];
        c[2] = tri[lat.at(i, j + 1)];
        lambda[0] = 1.0f - fx - fy;
        lambda[1] = fx;
        lambda[2] = fy;
    } else {
        c[0] = tri[lat.at(i + 1, j + 1)];
        c[1] = tri[lat.at(i, j + 1)];
        c[2] = tri[lat.at(i + 1, j)];
        lambda[0] = fx + fy - 1.0f;
        lambda[1] = 1.0f - fx;
        lambda[2] = 1.0f - fy;
    }
    for (int k = 0; k < 3; ++k) lambda[k] = std::max(lambda[k], 0.0f);
}

// A merged record: up to 3 x kSlots guides.
struct Merged {
    std::array<std::uint32_t, 3 * kSlots> g;
    std::array<float, 3 * kSlots> w, tw;
    std::size_t count = 0;
    float spread = 0.0f;  // the largest difference of a guide's weight between the corners
};

// Strand weights W = sum_k lambda_k W_k (each W_k normalized), twists weight-averaged; weights under
// `drop` of the largest left out, at most kSlots kept (largest first), renormalized.
void merge(const InterpolateRecords& rec, const std::uint32_t c[3], const float lambda[3], float drop, Merged& m) {
    m.count = 0;
    std::array<float, 3 * kSlots> per[3];  // each corner's normalized weight of each merged guide
    for (int k = 0; k < 3; ++k) {
        const std::uint32_t b = rec.offsets[c[k]], e = rec.offsets[c[k] + 1];
        if (!rec.bound[c[k]]) continue;
        for (std::uint32_t r = b; r < e; ++r) {
            const std::uint32_t g = rec.guides[r];
            const float w = interpolateWeight(rec.weights[r]);
            std::size_t at = 0;
            while (at < m.count && m.g[at] != g) ++at;
            if (at == m.count) {
                m.g[at] = g;
                m.w[at] = 0.0f;
                m.tw[at] = 0.0f;
                per[0][at] = per[1][at] = per[2][at] = 0.0f;
                ++m.count;
            }
            per[k][at] = w;
            m.w[at] += lambda[k] * w;
            m.tw[at] += lambda[k] * w * rec.twists[r];
        }
    }
    m.spread = 0.0f;
    float largest = 0.0f;
    for (std::size_t i = 0; i < m.count; ++i) {
        const float hi = std::max({per[0][i], per[1][i], per[2][i]}), lo = std::min({per[0][i], per[1][i], per[2][i]});
        m.spread = std::max(m.spread, hi - lo);
        if (m.w[i] > 0.0f) m.tw[i] /= m.w[i];
        largest = std::max(largest, m.w[i]);
    }
    // Drop the negligible, keep the kSlots largest.
    std::size_t kept = 0;
    for (std::size_t i = 0; i < m.count; ++i) {
        if (m.w[i] < drop * largest) continue;
        m.g[kept] = m.g[i];
        m.w[kept] = m.w[i];
        m.tw[kept] = m.tw[i];
        ++kept;
    }
    m.count = kept;
    if (m.count > kSlots) {
        std::array<std::size_t, 3 * kSlots> order;
        for (std::size_t i = 0; i < m.count; ++i) order[i] = i;
        std::partial_sort(order.begin(), order.begin() + kSlots, order.begin() + m.count,
                          [&](std::size_t a, std::size_t b) { return m.w[a] > m.w[b]; });
        Merged t = m;
        for (std::size_t i = 0; i < kSlots; ++i) {
            m.g[i] = t.g[order[i]];
            m.w[i] = t.w[order[i]];
            m.tw[i] = t.tw[order[i]];
        }
        m.count = kSlots;
    }
    float sum = 0.0f;
    for (std::size_t i = 0; i < m.count; ++i) sum += m.w[i];
    for (std::size_t i = 0; i < m.count; ++i) m.w[i] /= sum;
}

// ---- One configuration -------------------------------------------------------------------------

struct Config {
    std::string scalpName;
    MeshPtr scalp;
    double strands = 0;
    int guides = 0;
    double beta = 4.0;  // lattice spacing h = guide spacing / beta
};

std::FILE* jsonOut = nullptr;

GuidesPtr posed(const GuideSet& guides, Vec3f by) {
    auto out = std::make_shared<GuideSet>(guides);
    for (Guide& g : out->guides()) {
        for (std::size_t k = 1; k < g.points.size(); ++k) g.points[k] += by * float(k);
    }
    return out;
}

GuidesPtr rootsMoved(const GuideSet& guides, const ScalpMesh& scalp, float step, int howMany) {
    auto out = std::make_shared<GuideSet>(guides);
    const std::size_t n = out->size();
    for (int m = 0; m < howMany; ++m) {
        Guide& g = out->guides()[(n * (2 * m + 1)) / (2 * howMany)];
        const Vec3f from = scalp.position(g.root);
        const SurfacePoint at = scalp.closestPoint(from + Vec3f(step, 0.0f, step * 0.7f));
        for (Vec3f& p : g.points) p += at.position - from;
        g.root = at.root;
    }
    return out;
}

void runConfig(const Config& cfg, const std::vector<double>& betas) {
    const ScalpMesh& scalp = *cfg.scalp;
    const Vec3f box = boxSize(scalp);
    const float size = std::max({box.x, box.y, box.z});
    const StrandsPtr strands = scatter(cfg.scalp, cfg.strands, 15, size * 0.4);
    const GuidesPtr guides = hairstyleGuides(cfg.scalp, cfg.guides, size);
    auto& reg = OperatorRegistry::instance();
    ExternalInputs none;
    EvalContext ctx;
    ctx.externals = &none;
    auto newOp = [&] {
        auto op = reg.create(op::kInterpolate);
        op->params().set("keepClearance", false);
        return op;
    };
    const Value guideValue(guides), scalpValue(cfg.scalp);

    // ---- The current Interpolate: bind + blend (a new node), blend alone (binding kept) ----
    std::unique_ptr<Operator> cur;
    StrandsPtr curOut;
    newOp()->evaluate(ctx, {V(strands), guideValue, scalpValue});  // warm: the scalp's graph, the threads
    const double firstMs = bestMs(5, [&] {
        cur = newOp();
        curOut = valueAs<StrandsPtr>(cur->evaluate(ctx, {V(strands), guideValue, scalpValue}));
    });
    const float poseStep = size * 0.002f;
    int poseK = 0;
    const double blendOpMs = bestMs(5, [&] {
        cur->evaluate(ctx, {V(strands), Value(posed(*guides, Vec3f(poseStep * float(++poseK), 0, 0))), scalpValue});
    });
    // The binding alone (an upper bound): a new node taking length only binds the same way and its
    // "blend" is a scale of each strand's points.
    const double bindOnlyMs = bestMs(3, [&] {
        auto op = newOp();
        op->params().set("take", std::string("length"));
        op->evaluate(ctx, {V(strands), guideValue, scalpValue});
    });
    cur->evaluate(ctx, {V(strands), guideValue, scalpValue});  // its result again, for the state
    curOut = valueAs<StrandsPtr>(cur->evaluate(ctx, {V(strands), guideValue, scalpValue}));
    InterpolateState state;
    if (!interpolateState(*cur, curOut, &state)) {
        std::printf("no state\n");
        return;
    }
    InterpolateRecords rec;
    interpolateRecords(state, &rec);
    std::vector<const Guide*> kept;
    for (const Guide& g : guides->guides()) {
        if (g.points.size() >= 2) kept.push_back(&g);
    }
    InterpolateGuideRows rows;
    interpolateGuideRows(cur->params(), kept, cfg.scalp, interpolateSamples(*strands), &rows);
    const std::size_t N = strands->strandCount();
    double guidesPerStrand = 0.0;
    std::size_t maxGuides = 0;
    for (std::size_t s = 0; s < N; ++s) {
        const std::size_t k = rec.offsets[s + 1] - rec.offsets[s];
        guidesPerStrand += double(k);
        maxGuides = std::max(maxGuides, k);
    }
    guidesPerStrand /= double(N);

    // My blend from the current records (checks the harness: it must give the operator's points).
    std::vector<Vec3f> mine(strands->pointCount());
    auto blendAllFromRecords = [&] {
        parallelFor(0, N, [&](std::size_t s) {
            thread_local Scratch sc;
            thread_local std::vector<float> w;
            const std::uint32_t b = rec.offsets[s], e = rec.offsets[s + 1];
            Vec3f* out = mine.data() + strands->strandBegin(s);
            if (!rec.bound[s]) {
                std::copy(strands->strandPoints(s), strands->strandPoints(s) + strands->strandPointCount(s), out);
                return;
            }
            w.resize(e - b);
            for (std::uint32_t r = b; r < e; ++r) w[r - b] = interpolateWeight(rec.weights[r]);
            blendStrand(rows, scalp, *strands, s, rec.guides.data() + b, w.data(), rec.twists.data() + b, e - b, out, sc);
        }, 256);
    };
    const double myBlendMs = bestMs(3, blendAllFromRecords);
    double harness = 0.0;
    for (std::size_t i = 0; i < mine.size(); ++i) harness = std::max(harness, double((mine[i] - curOut->positions()[i]).length()));

    // Guide spacing: sqrt(area / guides).
    const double spacing = std::sqrt(scalp.totalArea() / double(cfg.guides));
    double edge = 0.0;
    for (const auto& t : scalp.triangles()) {
        edge += (scalp.points()[t[0]] - scalp.points()[t[1]]).length() + (scalp.points()[t[1]] - scalp.points()[t[2]]).length() +
                (scalp.points()[t[2]] - scalp.points()[t[0]]).length();
    }
    edge /= 3.0 * double(scalp.triangleCount());

    std::printf("\n== %s (%zu v, %zu tris, size %.1f), %zu strands x %zu pts, %d guides (spacing %.3f, edge %.3f)\n",
                cfg.scalpName.c_str(), scalp.pointCount(), scalp.triangleCount(), size, N, strands->strandPointCount(0),
                cfg.guides, spacing, edge);
    std::size_t twisted = 0;
    for (const float t : rec.twists) twisted += t != 0.0f;
    std::printf("current: first eval (bind+blend) %.1f ms, blend alone %.1f ms, bind alone (take=length node) %.1f ms; "
                "%.2f guides/strand (max %zu, %zu twisted); my blend %.1f ms (harness max diff %.2e); records %.1f MB CPU "
                "(interpolateRecordBytes), %.1f MB GPU packed (2 words a strand, 1 a record, twists)\n",
                firstMs, blendOpMs, bindOnlyMs, guidesPerStrand, maxGuides, twisted, myBlendMs, harness,
                double(interpolateRecordBytes(state)) / 1e6, double((2 * N + rec.guides.size()) * 4 + twisted * 4) / 1e6);

    // (Compact records, the --compact study: the operator keeps them so itself since B1 and B2, its
    // weights 16-bit shares; their bytes are on the line above.)

    // Strand lengths, for relative errors.
    std::vector<float> lengths(N);
    parallelFor(0, N, [&](std::size_t s) { lengths[s] = arcLength(curOut->strandPoints(s), curOut->strandPointCount(s)); });

    for (const double beta : betas) {
        const int L = std::max(1, static_cast<int>(std::ceil(beta * edge / spacing)));
        const Lattice lat = makeLattice(scalp, L);
        const std::size_t S = lat.roots.size();
        // Sample strands: rooted at the lattice points, 2 points along the normal.
        auto sampleSet = std::make_shared<StrandSet>();
        sampleSet->reserve(S, 2 * S);
        for (std::size_t i = 0; i < S; ++i) {
            const Vec3f p = scalp.position(lat.roots[i]), n = scalp.normal(lat.roots[i]);
            sampleSet->addStrand(i + 1, {p, p + n * (size * 0.01f)}, {}, lat.roots[i]);
        }
        const StrandsPtr samplesPtr = sampleSet;
        std::unique_ptr<Operator> bake;
        StrandsPtr bakeOut;
        const double bakeMs = bestMs(3, [&] {
            bake = newOp();
            bakeOut = valueAs<StrandsPtr>(bake->evaluate(ctx, {V(samplesPtr), guideValue, scalpValue}));
        });
        InterpolateState bstate;
        interpolateState(*bake, bakeOut, &bstate);
        InterpolateRecords brec;
        interpolateRecords(bstate, &brec);

        // Gather: each strand's merged record.
        for (const float drop : {0.0f, 0.01f}) {
            std::vector<std::uint32_t> offs(N + 1, 0);
            std::vector<Merged> merged(N);
            const double gatherMs = bestMs(3, [&] {
                parallelFor(0, N, [&](std::size_t s) {
                    std::uint32_t c[3];
                    float lambda[3];
                    locate(lat, strands->roots()[s], c, lambda);
                    merge(brec, c, lambda, drop, merged[s]);
                }, 1024);
            });
            double mergedGuides = 0.0;
            for (std::size_t s = 0; s < N; ++s) mergedGuides += double(merged[s].count);
            mergedGuides /= double(N);

            std::vector<Vec3f> baked(strands->pointCount());
            const double bakedBlendMs = bestMs(3, [&] {
                parallelFor(0, N, [&](std::size_t s) {
                    thread_local Scratch sc;
                    const Merged& m = merged[s];
                    blendStrand(rows, scalp, *strands, s, m.g.data(), m.w.data(), m.tw.data(), m.count,
                                baked.data() + strands->strandBegin(s), sc);
                }, 256);
            });
            // Errors against the operator's points: per strand, its farthest point.
            Stats abs, rel;
            std::vector<float> perStrand(N);
            parallelFor(0, N, [&](std::size_t s) {
                float e = 0.0f;
                const std::size_t b = strands->strandBegin(s), n = strands->strandPointCount(s);
                for (std::size_t j = 0; j < n; ++j) e = std::max(e, (baked[b + j] - curOut->positions()[b + j]).length());
                perStrand[s] = e;
            });
            for (std::size_t s = 0; s < N; ++s) {
                abs.add(perStrand[s]);
                rel.add(perStrand[s] / std::max(lengths[s], 1e-6f));
            }
            // With the spread test: strands whose corners disagree by more than tau fall back to their
            // own search (the current binding): how many, and the error left.
            const float tau = 0.25f;
            std::size_t fallback = 0;
            Stats absFb;
            for (std::size_t s = 0; s < N; ++s) {
                if (merged[s].spread > tau) {
                    ++fallback;
                    absFb.add(0.0);
                } else {
                    absFb.add(perStrand[s]);
                }
            }
            std::printf("  beta %.0f (L %d, %zu samples = %.1f per guide, %.3f strands per sample): bake %.1f ms, gather %.1f ms "
                        "(drop %.2f: %.2f guides/strand), blend from merged %.1f ms; error abs mean %.2e p99 %.2e max %.2e "
                        "(of strand length: p99 %.2e max %.2e); spread>%.2f fallback %.2f%% -> max %.2e p99 %.2e\n",
                        beta, L, S, double(S) / cfg.guides, double(N) / double(S), bakeMs, gatherMs, drop, mergedGuides,
                        bakedBlendMs, abs.mean(), abs.quantile(0.99), abs.max(), rel.quantile(0.99), rel.max(), tau,
                        100.0 * double(fallback) / double(N), absFb.max(), absFb.quantile(0.99));

            if (drop == 0.0f) {
                // ---- The shape field: each sample's blended shape (turned to its normal), each strand
                // three of them turned to its own. ----
                const std::size_t samples = rows.samples;
                std::vector<Vec3f> field(S * samples);
                std::vector<float> fieldLen(S), fieldW(S);
                std::vector<Vec3f> sampleNormal(S);
                const double shapeBakeMs = bestMs(3, [&] {
                    parallelFor(0, S, [&](std::size_t v) {
                        const Vec3f n = scalp.normal(lat.roots[v]);
                        sampleNormal[v] = n;
                        Vec3f* row = field.data() + v * samples;
                        std::fill(row, row + samples, Vec3f{});
                        float len = 0.0f;
                        if (!brec.bound[v]) {
                            fieldLen[v] = 0.0f;
                            return;
                        }
                        for (std::uint32_t r = brec.offsets[v]; r < brec.offsets[v + 1]; ++r) {
                            const std::uint32_t g = brec.guides[r];
                            const float w = interpolateWeight(brec.weights[r]);
                            len += rows.lengths[g] * w;
                            const Vec3f* o = rows.offsets.data() + std::size_t(g) * samples;
                            if (rows.normals[g].length() == 0.0f) {
                                for (std::size_t j = 0; j < samples; ++j) row[j] += o[j] * w;
                            } else if (brec.twists[r] != 0.0f) {
                                const TwistedTurn turn(rows.normals[g], n, brec.twists[r]);
                                for (std::size_t j = 0; j < samples; ++j) row[j] += turn(o[j]) * w;
                            } else {
                                const Rotation turn(rows.normals[g], n);
                                for (std::size_t j = 0; j < samples; ++j) row[j] += turn(o[j]) * w;
                            }
                        }
                        fieldLen[v] = len;
                    }, 64);
                });
                std::vector<Vec3f> shaped(strands->pointCount());
                const double shapeBlendMs = bestMs(3, [&] {
                    parallelFor(0, N, [&](std::size_t s) {
                        thread_local std::vector<Vec3f> blended;
                        std::uint32_t c[3];
                        float lambda[3];
                        locate(lat, strands->roots()[s], c, lambda);
                        const std::size_t n = strands->strandPointCount(s);
                        const Vec3f root = strands->strandPoints(s)[0];
                        const Vec3f sn = scalp.normal(strands->roots()[s]);
                        blended.assign(samples, root);
                        float len = 0.0f;
                        for (int k = 0; k < 3; ++k) {
                            if (lambda[k] <= 0.0f) continue;
                            const Rotation turn(sampleNormal[c[k]], sn);
                            const Vec3f* row = field.data() + std::size_t(c[k]) * samples;
                            for (std::size_t j = 0; j < samples; ++j) blended[j] += turn(row[j]) * lambda[k];
                            len += fieldLen[c[k]] * lambda[k];
                        }
                        const float own = arcLength(blended.data(), samples);
                        if (own > 1e-8f && len > 0.0f) {
                            const float scale = std::min(len / own, 2.0f);
                            for (std::size_t j = 1; j < samples; ++j) blended[j] = blended[0] + (blended[j] - blended[0]) * scale;
                        }
                        resampleByArcLengthInto(blended.data(), samples, shaped.data() + strands->strandBegin(s), n);
                    }, 256);
                });
                Stats sabs, srel;
                for (std::size_t s = 0; s < N; ++s) {
                    float e = 0.0f;
                    const std::size_t b = strands->strandBegin(s), n = strands->strandPointCount(s);
                    for (std::size_t j = 0; j < n; ++j) e = std::max(e, (shaped[b + j] - curOut->positions()[b + j]).length());
                    sabs.add(e);
                    srel.add(e / std::max(lengths[s], 1e-6f));
                }
                std::printf("    shape field: bake %.1f ms (%.1f MB), blend %.1f ms; error abs mean %.2e p99 %.2e max %.2e "
                            "(of length p99 %.2e max %.2e)\n",
                            shapeBakeMs, double(S * samples * 12 + S * 16) / 1e6, shapeBlendMs, sabs.mean(), sabs.quantile(0.99),
                            sabs.max(), srel.quantile(0.99), srel.max());

                // ---- Second order: each sample's weights' gradient along the skin (central
                // differences: the operator binds 4 more points a sample), blended as
                // W(x) = sum_k lambda_k (W_k + 1/2 grad W_k . (x - x_k)), exact for quadratic
                // weights (error O(h^3) where the weights are smooth). ----
                const float delta = float(edge / L) * 0.25f;
                std::vector<Vec3f> tan1(S), tan2(S), xs(S);
                std::vector<float> step(4 * S);
                auto probes = std::make_shared<StrandSet>();
                probes->reserve(4 * S, 8 * S);
                for (std::size_t v = 0; v < S; ++v) {
                    const Vec3f n = scalp.normal(lat.roots[v]);
                    xs[v] = scalp.position(lat.roots[v]);
                    tan1[v] = (std::fabs(n.x) < 0.9f ? Vec3f(1, 0, 0) : Vec3f(0, 1, 0)).cross(n).normalized();
                    tan2[v] = n.cross(tan1[v]);
                    for (int k = 0; k < 4; ++k) {
                        const Vec3f axis = k < 2 ? tan1[v] : tan2[v];
                        const float sign = (k % 2 == 0) ? 1.0f : -1.0f;
                        const SurfacePoint sp = scalp.closestPoint(xs[v] + axis * (sign * delta));
                        step[4 * v + k] = (sp.position - xs[v]).dot(axis);
                        probes->addStrand(4 * v + k + 1, {sp.position, sp.position + sp.normal * (size * 0.01f)}, {}, sp.root);
                    }
                }
                const StrandsPtr probesPtr = probes;
                std::unique_ptr<Operator> probeOp;
                StrandsPtr probeOut;
                const double probeMs = bestMs(1, [&] {
                    probeOp = newOp();
                    probeOut = valueAs<StrandsPtr>(probeOp->evaluate(ctx, {V(probesPtr), guideValue, scalpValue}));
                });
                InterpolateState pstate;
                interpolateState(*probeOp, probeOut, &pstate);
                InterpolateRecords prec;
                interpolateRecords(pstate, &prec);
                struct Grad {
                    std::uint32_t g;
                    float w, d1, d2, tw;
                };
                std::vector<std::uint32_t> gOff(S + 1, 0);
                std::vector<std::vector<Grad>> perSample(S);
                parallelFor(0, S, [&](std::size_t v) {
                    std::vector<Grad>& list = perSample[v];
                    auto weightOf = [&](const InterpolateRecords& r, std::size_t i, std::uint32_t g) {
                        for (std::uint32_t q = r.offsets[i]; q < r.offsets[i + 1]; ++q) {
                            if (r.guides[q] == g) return interpolateWeight(r.weights[q]);
                        }
                        return 0.0f;
                    };
                    auto add = [&](std::uint32_t g) {
                        for (const Grad& e : list) {
                            if (e.g == g) return;
                        }
                        Grad e;
                        e.g = g;
                        e.w = weightOf(brec, v, g);
                        e.tw = 0.0f;
                        for (std::uint32_t q = brec.offsets[v]; q < brec.offsets[v + 1]; ++q) {
                            if (brec.guides[q] == g) e.tw = brec.twists[q];
                        }
                        const float s1 = step[4 * v] - step[4 * v + 1], s2 = step[4 * v + 2] - step[4 * v + 3];
                        e.d1 = s1 != 0.0f ? (weightOf(prec, 4 * v, g) - weightOf(prec, 4 * v + 1, g)) / s1 : 0.0f;
                        e.d2 = s2 != 0.0f ? (weightOf(prec, 4 * v + 2, g) - weightOf(prec, 4 * v + 3, g)) / s2 : 0.0f;
                        list.push_back(e);
                    };
                    for (std::uint32_t q = brec.offsets[v]; q < brec.offsets[v + 1]; ++q) add(brec.guides[q]);
                    for (int k = 0; k < 4; ++k) {
                        for (std::uint32_t q = prec.offsets[4 * v + k]; q < prec.offsets[4 * v + k + 1]; ++q) add(prec.guides[q]);
                    }
                }, 64);
                std::vector<Vec3f> second(strands->pointCount());
                std::atomic<std::size_t> negatives{0};
                const double secondMs = bestMs(1, [&] {
                    parallelFor(0, N, [&](std::size_t s) {
                        thread_local std::vector<std::uint32_t> g;
                        thread_local std::vector<float> w, tw;
                        thread_local Scratch sc;
                        g.clear();
                        w.clear();
                        tw.clear();
                        std::uint32_t c[3];
                        float lambda[3];
                        locate(lat, strands->roots()[s], c, lambda);
                        const Vec3f x = strands->strandPoints(s)[0];
                        for (int k = 0; k < 3; ++k) {
                            const Vec3f d = x - xs[c[k]];
                            const float a1 = d.dot(tan1[c[k]]), a2 = d.dot(tan2[c[k]]);
                            for (const Grad& e : perSample[c[k]]) {
                                const float add = lambda[k] * (e.w + 0.5f * (e.d1 * a1 + e.d2 * a2));
                                std::size_t at = 0;
                                while (at < g.size() && g[at] != e.g) ++at;
                                if (at == g.size()) {
                                    g.push_back(e.g);
                                    w.push_back(0.0f);
                                    tw.push_back(e.tw);
                                }
                                w[at] += add;
                            }
                        }
                        float sum = 0.0f;
                        for (float& v : w) {
                            if (v < 0.0f) {
                                negatives.fetch_add(1, std::memory_order_relaxed);
                                v = 0.0f;  // (clamped: a blend of guides, never past them)
                            }
                            sum += v;
                        }
                        for (float& v : w) v /= sum;
                        blendStrand(rows, scalp, *strands, s, g.data(), w.data(), tw.data(), g.size(),
                                    second.data() + strands->strandBegin(s), sc);
                    }, 256);
                });
                Stats qabs;
                for (std::size_t s = 0; s < N; ++s) {
                    float e = 0.0f;
                    const std::size_t b = strands->strandBegin(s), n = strands->strandPointCount(s);
                    for (std::size_t j = 0; j < n; ++j) e = std::max(e, (second[b + j] - curOut->positions()[b + j]).length());
                    qabs.add(e);
                }
                std::printf("    second order (gradients from 4 probes a sample): probes bound %.1f ms, gather+blend %.1f ms; "
                            "error abs mean %.2e p99 %.2e max %.2e (negative weights clamped %zu)\n",
                            probeMs, secondMs, qabs.mean(), qabs.quantile(0.99), qabs.max(), negatives.load());

                // ---- Guide roots moved (a dab with children following): the samples bound again
                // (the operator's own partial rebind, on the samples) and the strands of every
                // triangle with a changed sample gathered again; against the current node's rebind. ----
                // current: a rebind and a full blend, minus the blend (each try other roots: a new binding)
                int tries = 0;
                const double curMovedMs = bestMs(3, [&] {
                    const GuidesPtr m = rootsMoved(*guides, scalp, float(spacing) * (0.2f + 0.05f * float(++tries)), 3);
                    cur->evaluate(ctx, {V(strands), Value(m), scalpValue});
                }) - blendOpMs;
                tries = 0;
                const double rebakeMs = bestMs(3, [&] {
                    const GuidesPtr m = rootsMoved(*guides, scalp, float(spacing) * (0.2f + 0.05f * float(++tries)), 3);
                    bake->evaluate(ctx, {V(samplesPtr), Value(m), scalpValue});
                });
                const GuidesPtr moved = rootsMoved(*guides, scalp, float(spacing) * 0.3f, 3);
                const Value movedValue(moved);
                const StrandsPtr bakeMoved = valueAs<StrandsPtr>(bake->evaluate(ctx, {V(samplesPtr), movedValue, scalpValue}));
                InterpolateState mstate;
                interpolateState(*bake, bakeMoved, &mstate);
                InterpolateRecords mrec;
                interpolateRecords(mstate, &mrec);
                std::vector<char> changed(S, 0);
                std::size_t changedSamples = 0;
                for (std::size_t v = 0; v < S; ++v) {
                    const std::uint32_t a = brec.offsets[v + 1] - brec.offsets[v], b = mrec.offsets[v + 1] - mrec.offsets[v];
                    bool diff = a != b;
                    for (std::uint32_t r = 0; r < a && !diff; ++r) {
                        diff = brec.guides[brec.offsets[v] + r] != mrec.guides[mrec.offsets[v] + r] ||
                               brec.weights[brec.offsets[v] + r] != mrec.weights[mrec.offsets[v] + r];
                    }
                    changed[v] = diff;
                    changedSamples += diff;
                }
                std::vector<char> triChanged(scalp.triangleCount(), 0);
                for (std::size_t t = 0; t < scalp.triangleCount(); ++t) {
                    for (std::size_t k = 0; k < lat.perTri() && !triChanged[t]; ++k) triChanged[t] = changed[lat.triSamples[t * lat.perTri() + k]];
                }
                std::vector<std::uint32_t> again;
                for (std::size_t s = 0; s < N; ++s) {
                    if (triChanged[static_cast<std::size_t>(strands->roots()[s].face)]) again.push_back(static_cast<std::uint32_t>(s));
                }
                const auto t2 = std::chrono::steady_clock::now();
                parallelFor(0, again.size(), [&](std::size_t i) {
                    std::uint32_t c[3];
                    float lambda[3];
                    locate(lat, strands->roots()[again[i]], c, lambda);
                    merge(mrec, c, lambda, 0.0f, merged[again[i]]);
                }, 1024);
                const double regatherMs = msSince(t2);
                std::printf("    3 guide roots moved: current rebind %.1f ms; baked: samples rebound %.1f ms (%zu of %zu changed), "
                            "%zu strands gathered again %.2f ms\n",
                            curMovedMs, rebakeMs, changedSamples, S, again.size(), regatherMs);
                if (jsonOut) {
                    std::fprintf(jsonOut,
                                 "{\"scalp\":\"%s\",\"vertices\":%zu,\"strands\":%zu,\"guides\":%d,\"beta\":%.1f,\"L\":%d,\"samples\":%zu,"
                                 "\"curFirstMs\":%.2f,\"curBlendMs\":%.2f,\"curBindMs\":%.2f,\"guidesPerStrand\":%.3f,"
                                 "\"myBlendMs\":%.2f,\"bakeMs\":%.2f,\"gatherMs\":%.2f,\"mergedGuides\":%.3f,\"bakedBlendMs\":%.2f,"
                                 "\"errMean\":%.3e,\"errP99\":%.3e,\"errMax\":%.3e,\"relP99\":%.3e,\"relMax\":%.3e,"
                                 "\"fallbackPct\":%.3f,\"errMaxFallback\":%.3e,\"shapeBakeMs\":%.2f,\"shapeBlendMs\":%.2f,"
                                 "\"shapeErrMean\":%.3e,\"shapeErrP99\":%.3e,\"shapeErrMax\":%.3e,\"curMovedMs\":%.2f,"
                                 "\"rebakeMs\":%.2f,\"changedSamples\":%zu,\"regathered\":%zu,\"regatherMs\":%.3f,\"spacing\":%.4f,"
                                 "\"lattice\":%.4f,\"size\":%.3f,\"bindOnlyMs\":%.2f,\"probeMs\":%.2f,\"secondMs\":%.2f,"
                                 "\"secondErrMean\":%.3e,\"secondErrP99\":%.3e,\"secondErrMax\":%.3e}\n",
                                 cfg.scalpName.c_str(), scalp.pointCount(), N, cfg.guides, beta, L, S, firstMs, blendOpMs,
                                 firstMs - blendOpMs, guidesPerStrand, myBlendMs, bakeMs, gatherMs, mergedGuides, bakedBlendMs,
                                 abs.mean(), abs.quantile(0.99), abs.max(), rel.quantile(0.99), rel.max(),
                                 100.0 * double(fallback) / double(N), absFb.max(), shapeBakeMs, shapeBlendMs, sabs.mean(),
                                 sabs.quantile(0.99), sabs.max(), curMovedMs, rebakeMs, changedSamples, again.size(), regatherMs,
                                 spacing, edge / L, size, bindOnlyMs, probeMs, secondMs, qabs.mean(), qabs.quantile(0.99), qabs.max());
                    std::fflush(jsonOut);
                }
            }
        }
    }
}

// ---- Interpolate's points on the app head, for comparing two builds (--dump, --compare) ----------
//
// 100k strands (16 points), 1000 guides, a graph Scalp -> Scatter -> Resample -> Interpolate with
// each of: the defaults, followSurface off, keepClearance on and off, a limitDistance, a part down
// the middle, three painted regions, one neighbor, power 1. Written as (name, float count, floats).

struct Scenario {
    std::string name;
    std::string params;  // the Interpolate's params (JSON object body)
    bool parts = false, regions = false;
};

std::vector<Scenario> scenarios() {
    return {{"defaults", ""},
            {"noFollowSurface", R"("followSurface": false)"},
            {"keepClearance", R"("keepClearance": true)"},
            {"noKeepClearance", R"("keepClearance": false)"},
            {"limitDistance", R"("limitDistance": 0.6)"},
            {"limitDistanceNoFollow", R"("limitDistance": 0.6, "followSurface": false)"},
            {"parts", "", true, false},
            {"regions", R"("regionMap": "regions")", false, true},
            {"oneNeighbor", R"("neighbors": 1)"},
            {"power1", R"("power": 1.0)"}};
}

void dump(const std::string& path, double strandCount) {
    const MeshPtr head = loadObj(std::string(RESEARCH_DATA) + "/app/head_scalp.obj");
    const Vec3f box = boxSize(*head);
    const float size = std::max({box.x, box.y, box.z});
    Vec3f lo = head->points().front(), hi = lo;
    for (const Vec3f& p : head->points()) {
        lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
        hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
    }
    const Vec3f mid = (lo + hi) * 0.5f;
    // Regions: left, right front, right back (per vertex).
    auto painted = std::make_shared<ScalpMesh>(*head);
    {
        std::vector<ScalpMesh::Color> colors;
        for (const ScalpMesh::Triangle& t : head->triangles()) {
            for (const std::int32_t v : t) {
                const Vec3f& p = head->points()[static_cast<std::size_t>(v)];
                colors.push_back(p.x < mid.x ? ScalpMesh::Color{1, 0, 0, 1} : p.z > mid.z ? ScalpMesh::Color{0, 1, 0, 1} : ScalpMesh::Color{0, 0, 1, 1});
            }
        }
        painted->setCornerColors("regions", std::move(colors));
    }
    // A part down the middle of the top, front to back.
    PartSet partSet;
    std::vector<Vec3f> line;
    for (int i = 0; i <= 10; ++i) {
        const float z = lo.z + (hi.z - lo.z) * (0.2f + 0.6f * static_cast<float>(i) / 10.0f);
        line.push_back(head->closestPoint({mid.x, hi.y + size, z}).position);
    }
    partSet.parts.push_back(PartSet::fromPoints("middle", line, *head));
    const auto lines = std::make_shared<const PartLines>(partSet, *head);
    const GuidesPtr guides = hairstyleGuides(head, 1000, size);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    for (const Scenario& sc : scenarios()) {
        const MeshPtr scalp = sc.regions ? MeshPtr(painted) : head;
        const std::string graph = R"({"version": 1, "output": "out", "nodes": [
            {"name": "scalp", "type": "Scalp"}, {"name": "guides", "type": "Guides"},
            {"name": "scatter", "type": "Scatter", "params": {"density": )" +
                                  std::to_string(strandCount / head->totalArea()) + R"(, "length": )" +
                                  std::to_string(size * 0.4) + R"(}},
            {"name": "resample", "type": "Resample", "params": {"segments": 15}},
            {"name": "out", "type": "Interpolate", "params": {)" + sc.params + R"(}}],
          "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"},
                          {"from": "scatter", "to": "resample", "input": "strands"},
                          {"from": "resample", "to": "out", "input": "strands"},
                          {"from": "guides", "to": "out", "input": "guides"},
                          {"from": "scalp", "to": "out", "input": "scalp"}]})";
        GraphDocument doc = loadGraphJson(graph);
        ExternalInputs ext;
        setGroomExternals(ext, scalp, guides, nullptr, sc.parts ? PartsPtr(lines) : nullptr);
        EvalContext ctx;
        ctx.externals = &ext;
        const StrandsPtr out = doc.graph.evaluateStrands(doc.output, ctx);
        const std::uint32_t nameLength = static_cast<std::uint32_t>(sc.name.size());
        const std::uint64_t floats = out->pointCount() * 3, strands = out->strandCount();
        std::fwrite(&nameLength, 4, 1, f);
        std::fwrite(sc.name.data(), 1, nameLength, f);
        std::fwrite(&strands, 8, 1, f);
        for (std::size_t s = 0; s < out->strandCount(); ++s) {
            const std::uint32_t n = static_cast<std::uint32_t>(out->strandPointCount(s));
            std::fwrite(&n, 4, 1, f);
        }
        std::fwrite(&floats, 8, 1, f);
        std::fwrite(out->positions().data(), 4, floats, f);
        std::printf("%s: %zu strands, %zu points\n", sc.name.c_str(), out->strandCount(), out->pointCount());
    }
    std::fclose(f);
    std::printf("head size %.2f\n", size);
}

struct Dumped {
    std::string name;
    std::vector<std::uint32_t> counts;
    std::vector<Vec3f> points;
};

std::vector<Dumped> readDump(const std::string& path) {
    std::vector<Dumped> out;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    std::uint32_t nameLength = 0;
    while (f && std::fread(&nameLength, 4, 1, f) == 1) {
        Dumped d;
        d.name.resize(nameLength);
        std::fread(d.name.data(), 1, nameLength, f);
        std::uint64_t strands = 0, floats = 0;
        std::fread(&strands, 8, 1, f);
        d.counts.resize(strands);
        std::fread(d.counts.data(), 4, strands, f);
        std::fread(&floats, 8, 1, f);
        d.points.resize(floats / 3);
        std::fread(d.points.data(), 4, floats, f);
        out.push_back(std::move(d));
    }
    if (f) std::fclose(f);
    return out;
}

// Per scenario, each strand's farthest point apart in the two dumps: mean, p99, max (micrometres:
// the head is in cm), and how many strands moved at all.
void compare(const std::string& a, const std::string& b) {
    const std::vector<Dumped> x = readDump(a), y = readDump(b);
    for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
        if (x[i].name != y[i].name || x[i].counts != y[i].counts) {
            std::printf("%s: different strands\n", x[i].name.c_str());
            continue;
        }
        Stats st;
        std::size_t moved = 0, over = 0, at = 0;
        for (const std::uint32_t n : x[i].counts) {
            double worst = 0.0;
            for (std::uint32_t j = 0; j < n; ++j, ++at) worst = std::max(worst, double((x[i].points[at] - y[i].points[at]).length()));
            st.add(worst * 1e4);
            moved += worst > 0.0;
            over += worst * 1e4 > 5.0;
        }
        std::printf("%-22s strands %zu, moved %zu, over 5 um %zu; per strand (um): mean %.3f p99 %.3f p99.99 %.3f max %.3f\n",
                    x[i].name.c_str(), x[i].counts.size(), moved, over, st.mean(), st.quantile(0.99), st.quantile(0.9999), st.max());
    }
}

}  // namespace

int main(int argc, char** argv) {
    bool quick = false, compactOnly = false;
    std::string out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--quick") quick = true;
        else if (a == "--compact") compactOnly = true;
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
        else if (a == "--dump" && i + 1 < argc) {
            dump(argv[i + 1], i + 2 < argc ? std::stod(argv[i + 2]) : 100000.0);
            return 0;
        } else if (a == "--compare" && i + 2 < argc) {
            compare(argv[i + 1], argv[i + 2]);
            return 0;
        }
    }
    if (!out.empty()) jsonOut = std::fopen(out.c_str(), "w");
    const MeshPtr head = loadObj(std::string(RESEARCH_DATA) + "/app/head_scalp.obj");
    std::printf("threads: %zu\n", parallelThreads());
    std::printf("head: %zu vertices, %zu triangles, area %.1f\n", head->pointCount(), head->triangleCount(), head->totalArea());
    std::vector<double> strandCounts = quick ? std::vector<double>{100000} : std::vector<double>{100000, 500000, 1000000};
    std::vector<int> guideCounts = quick ? std::vector<int>{1000} : std::vector<int>{100, 1000, 5000};
    std::vector<double> betas = quick ? std::vector<double>{2, 4, 8, 16} : std::vector<double>{2, 4, 8};
    if (compactOnly) betas.clear();
    for (const int g : guideCounts) {
        for (const double n : strandCounts) {
            Config c;
            c.scalpName = "app_head";
            c.scalp = head;
            c.strands = n;
            c.guides = g;
            runConfig(c, betas);
        }
    }
    if (!quick && !compactOnly) {
        // A dense production-like scalp (the head split twice and three times: 19.6k / 78k vertices):
        // the vertices themselves as the samples where they are dense enough (beta from the edge).
        for (const int levels : {2, 3}) {
            Config c;
            c.scalpName = "app_head_sub" + std::to_string(levels);
            c.scalp = subdivideMidpoint(*head, levels);
            c.strands = 500000;
            c.guides = 1000;
            runConfig(c, {1, 4});
        }
    }
    if (jsonOut) std::fclose(jsonOut);
    return 0;
}
