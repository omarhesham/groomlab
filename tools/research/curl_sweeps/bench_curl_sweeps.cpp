// SPDX-License-Identifier: Apache-2.0
// Research prototype (docs/research/curl_sweeps.md): curls as analytic sweeps and strands as a
// low-order spline plus a quantized residual, measured on the shipped curly preset (ringlets: Curl
// coherent by clump) on the app's head.
//
//   research_curl_sweeps [--density 0.25] [--out results.jsonl]
//
// 1. Sampling: Curl offsets the strand at its own points, so a ringlet is as round as the point
//    count makes it. The preset's strands (Resample 40) against the same groom at 256 segments
//    (Scatter, Curl and the rest at that resolution): the farthest any point of the dense strand is
//    from the shipped polyline, and the points per turn.
// 2. Sweep: a ringlet strand is its pre-curl strand (smooth) plus its clump's curl offset, which
//    every member shares at the same share of its length. Stored as the base (a cubic B-spline of k
//    control points) and one offset curve per clump: the bytes and the error against the dense curl.
// 3. Generic: any strand (the final one, frizz included) as a cubic B-spline fitted by least
//    squares, its control points quantized to 16 bits in the strand's box, against the dense
//    polyline cut to the points it needs (Douglas-Peucker), for a max error.
// 4. Reconstruction: points made per second from each representation (the GPU's work, on the CPU).

#include <array>
#include <atomic>

#include <nlohmann/json.hpp>

#include "../common/research_common.h"
#include "groomlab/curve.h"
#include "groomlab/presets.h"

using namespace research;

namespace {

// ---- Cubic B-splines on [0, 1], uniform knots, clamped (k >= 4 control points) -----------------

// The basis at parameter t for k control points: the first index and the 4 weights.
// Clamped (open uniform) knots: 0 0 0 0, 1/(k-3) ... 1 1 1 1, so the curve starts and ends at its
// end control points. Cox-de Boor for the 4 nonzero cubic basis functions.
inline void basis(int k, float t, int* first, float w[4]) {
    const int spans = k - 3;
    const float x = std::clamp(t, 0.0f, 1.0f) * float(spans);
    const int span = std::min(static_cast<int>(x), spans - 1);
    auto knot = [&](int i) { return float(std::clamp(i - 3, 0, spans)); };  // knot i (in span units)
    const int j = span + 3;  // knot interval [knot(j), knot(j+1))
    float left[4], right[4], N[4];
    N[0] = 1.0f;
    for (int d = 1; d <= 3; ++d) {
        left[d] = x - knot(j + 1 - d);
        right[d] = knot(j + d) - x;
        float saved = 0.0f;
        for (int r = 0; r < d; ++r) {
            const float den = right[r + 1] + left[d - r];
            const float tmp = den != 0.0f ? N[r] / den : 0.0f;
            N[r] = saved + right[r + 1] * tmp;
            saved = left[d - r] * tmp;
        }
        N[d] = saved;
    }
    for (int i = 0; i < 4; ++i) w[i] = N[i];
    *first = span;
}

Vec3f evalSpline(const Vec3f* c, int k, float t) {
    int f;
    float w[4];
    basis(k, t, &f, w);
    return c[f] * w[0] + c[f + 1] * w[1] + c[f + 2] * w[2] + c[f + 3] * w[3];
}

// Least squares: control points c (k of them) of the spline through points p at parameters t,
// with the end points pinned (heavily weighted). Banded normal equations (bandwidth 3), Cholesky.
void fitSpline(const Vec3f* p, const float* t, std::size_t n, int k, Vec3f* c) {
    std::vector<double> band(static_cast<std::size_t>(k) * 4, 0.0);  // A^T A: [row][0..3] = (row, row + d)
    std::vector<double> rhs(static_cast<std::size_t>(k) * 3, 0.0);
    auto add = [&](float ti, const Vec3f& pi, double weight) {
        int f;
        float w[4];
        basis(k, ti, &f, w);
        for (int a = 0; a < 4; ++a) {
            for (int b = a; b < 4; ++b) band[std::size_t(f + a) * 4 + std::size_t(b - a)] += weight * w[a] * w[b];
            rhs[std::size_t(f + a) * 3 + 0] += weight * w[a] * pi.x;
            rhs[std::size_t(f + a) * 3 + 1] += weight * w[a] * pi.y;
            rhs[std::size_t(f + a) * 3 + 2] += weight * w[a] * pi.z;
        }
    };
    for (std::size_t i = 0; i < n; ++i) add(t[i], p[i], 1.0);
    add(t[0], p[0], 1e4);
    add(t[n - 1], p[n - 1], 1e4);
    for (int i = 0; i < k; ++i) band[std::size_t(i) * 4] += 1e-9;  // (regularize empty spans)
    // Banded Cholesky: L stored in place, L[i][i-d] at band[(i-d)*4 + d].
    for (int i = 0; i < k; ++i) {
        for (int d = 3; d >= 1; --d) {  // L[i][j], j = i - d
            const int j = i - d;
            if (j < 0) continue;
            double s = band[std::size_t(j) * 4 + std::size_t(d)];
            for (int m = 1; m <= 3 - d; ++m) {  // sum over q = j - m >= i - 3
                const int q = j - m;
                if (q < 0) break;
                s -= band[std::size_t(q) * 4 + std::size_t(i - q)] * band[std::size_t(q) * 4 + std::size_t(j - q)];
            }
            band[std::size_t(j) * 4 + std::size_t(d)] = s / band[std::size_t(j) * 4];
        }
        double s = band[std::size_t(i) * 4];
        for (int d = 1; d <= 3; ++d) {
            const int q = i - d;
            if (q < 0) break;
            const double l = band[std::size_t(q) * 4 + std::size_t(d)];
            s -= l * l;
        }
        band[std::size_t(i) * 4] = std::sqrt(std::max(s, 1e-30));
    }
    for (int axis = 0; axis < 3; ++axis) {
        std::vector<double> y(static_cast<std::size_t>(k));
        for (int i = 0; i < k; ++i) {
            double s = rhs[std::size_t(i) * 3 + std::size_t(axis)];
            for (int d = 1; d <= 3; ++d) {
                const int q = i - d;
                if (q < 0) break;
                s -= band[std::size_t(q) * 4 + std::size_t(d)] * y[std::size_t(q)];
            }
            y[std::size_t(i)] = s / band[std::size_t(i) * 4];
        }
        for (int i = k - 1; i >= 0; --i) {
            double s = y[std::size_t(i)];
            for (int d = 1; d <= 3; ++d) {
                const int q = i + d;
                if (q >= k) break;
                s -= band[std::size_t(i) * 4 + std::size_t(d)] * y[std::size_t(q)];
            }
            y[std::size_t(i)] = s / band[std::size_t(i) * 4];
        }
        for (int i = 0; i < k; ++i) {
            float& dst = axis == 0 ? c[i].x : axis == 1 ? c[i].y : c[i].z;
            dst = static_cast<float>(y[std::size_t(i)]);
        }
    }
}

// Quantize control points to 16 bits in their box (6 bytes a point + 24 the box).
void quantize16(Vec3f* c, int k) {
    Vec3f lo = c[0], hi = c[0];
    for (int i = 0; i < k; ++i) {
        lo = {std::min(lo.x, c[i].x), std::min(lo.y, c[i].y), std::min(lo.z, c[i].z)};
        hi = {std::max(hi.x, c[i].x), std::max(hi.y, c[i].y), std::max(hi.z, c[i].z)};
    }
    auto q = [](float v, float a, float b) {
        if (b <= a) return a;
        const float s = std::round((v - a) / (b - a) * 65535.0f);
        return a + s / 65535.0f * (b - a);
    };
    for (int i = 0; i < k; ++i) c[i] = {q(c[i].x, lo.x, hi.x), q(c[i].y, lo.y, hi.y), q(c[i].z, lo.z, hi.z)};
}

// The farthest any of the dense points is from polyline q (m points), both sampled root to tip
// alike: each dense point against the segments around the same share of the length.
float distanceToPolyline(const Vec3f* dense, std::size_t n, const Vec3f* q, std::size_t m) {
    float worst = 0.0f;
    std::size_t seg = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const float share = float(i) / float(n - 1);
        const std::size_t center = std::min(m - 2, static_cast<std::size_t>(share * float(m - 1)));
        float best = 1e30f;
        const std::size_t lo = center > 3 ? center - 3 : 0, hi = std::min(m - 2, center + 3);
        for (std::size_t s = lo; s <= hi; ++s) {
            const Vec3f a = q[s], b = q[s + 1], ab = b - a;
            const float len2 = ab.dot(ab);
            const float u = len2 > 0.0f ? std::clamp((dense[i] - a).dot(ab) / len2, 0.0f, 1.0f) : 0.0f;
            best = std::min(best, (dense[i] - (a + ab * u)).length());
        }
        (void)seg;
        worst = std::max(worst, best);
    }
    return worst;
}

// Douglas-Peucker: the points kept so that every point is within eps of the kept polyline.
std::size_t douglasPeucker(const Vec3f* p, std::size_t n, float eps) {
    std::vector<char> keep(n, 0);
    keep[0] = keep[n - 1] = 1;
    std::vector<std::pair<std::size_t, std::size_t>> stack{{0, n - 1}};
    while (!stack.empty()) {
        auto [a, b] = stack.back();
        stack.pop_back();
        float worst = 0.0f;
        std::size_t at = a;
        const Vec3f ab = p[b] - p[a];
        const float len2 = ab.dot(ab);
        for (std::size_t i = a + 1; i < b; ++i) {
            const float u = len2 > 0.0f ? std::clamp((p[i] - p[a]).dot(ab) / len2, 0.0f, 1.0f) : 0.0f;
            const float d = (p[i] - (p[a] + ab * u)).length();
            if (d > worst) {
                worst = d;
                at = i;
            }
        }
        if (worst > eps) {
            keep[at] = 1;
            stack.push_back({a, at});
            stack.push_back({at, b});
        }
    }
    std::size_t kept = 0;
    for (char k : keep) kept += k;
    return kept;
}

struct CurlyGroom {
    StrandsPtr base, curled, final;  // clump (pre-curl), curl, frizzNoise outputs
};

CurlyGroom evaluateCurly(const std::string& graphJson, const MeshPtr& scalp, const GuidesPtr& guides, int segments, float density) {
    GraphDocument doc = loadGraphJson(graphJson);
    doc.graph.node(doc.node("resample")).params().set("segments", std::int64_t{segments});
    ExternalInputs ext;
    setGroomExternals(ext, scalp, guides, nullptr);
    EvalContext ctx;
    ctx.externals = &ext;
    ctx.previewDensity = density;
    CurlyGroom g;
    doc.graph.evaluateStrands(doc.output, ctx);
    g.base = doc.graph.evaluateStrands(doc.node("clump"), ctx);
    g.curled = doc.graph.evaluateStrands(doc.node("curl"), ctx);
    g.final = doc.graph.evaluateStrands(doc.node("frizzNoise"), ctx);
    return g;
}

std::FILE* jsonOut = nullptr;

}  // namespace

int main(int argc, char** argv) {
    float density = 0.25f;
    std::string out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--density" && i + 1 < argc) density = std::stof(argv[++i]);
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
    }
    if (!out.empty()) jsonOut = std::fopen(out.c_str(), "w");
    const MeshPtr head = loadObj(std::string(RESEARCH_DATA) + "/app/head_scalp.obj");
    const Vec3f box = boxSize(*head);
    const float size = std::max({box.x, box.y, box.z});
    std::ifstream f(std::string(RESEARCH_PRESETS) + "/curly.json");
    const std::string presetText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::string graph = presetGraphJson(presetText, double(size));
    const auto fitted = nlohmann::json::parse(graph);
    float curlRadius = 0.0f, curlFrequency = 0.0f, width = 0.0f;
    for (const auto& node : fitted["nodes"]) {
        if (node["name"] == "curl") {
            curlRadius = node["params"]["radius"].get<float>();
            curlFrequency = node["params"]["frequency"].get<float>();
        }
        if (node["name"] == "width") width = node["params"]["width"].get<float>();
    }
    // Ringlet guides: shoulder length, hanging down (a groomer's starting point for curls).
    const GuidesPtr guides = hairstyleGuides(head, 400, size, 8, 11, 0.25f);
    std::printf("curly preset on the head (size %.1f): curl radius %.3f, frequency %.3f turns/unit, width %.4f; preview density %.2f\n",
                size, curlRadius, curlFrequency, width, density);

    // ---- 1. Sampling: the shipped resolution against the dense one ----
    const int kDense = 256;
    const auto t0 = std::chrono::steady_clock::now();
    const CurlyGroom dense = evaluateCurly(graph, head, guides, kDense, density);
    std::printf("dense (%d segments): %zu strands, evaluated in %.0f ms\n", kDense, dense.final->strandCount(), msSince(t0));
    const std::size_t N = dense.final->strandCount();
    std::vector<float> lengths(N), turns(N);
    for (std::size_t s = 0; s < N; ++s) {
        lengths[s] = arcLength(dense.base->strandPoints(s), dense.base->strandPointCount(s));
        turns[s] = lengths[s] * curlFrequency;
    }
    for (const int segments : {16, 40, 64, 128}) {
        const CurlyGroom g = evaluateCurly(graph, head, guides, segments, density);
        if (g.final->strandCount() != N) {
            std::printf("segments %d: %zu strands (not the same set)\n", segments, g.final->strandCount());
            continue;
        }
        Stats err, errCurl, perTurn;
        for (std::size_t s = 0; s < N; ++s) {
            const float e = distanceToPolyline(dense.final->strandPoints(s), dense.final->strandPointCount(s), g.final->strandPoints(s),
                                               g.final->strandPointCount(s));
            const float ec = distanceToPolyline(dense.curled->strandPoints(s), dense.curled->strandPointCount(s),
                                                g.curled->strandPoints(s), g.curled->strandPointCount(s));
            err.add(e);
            errCurl.add(ec);
            perTurn.add(turns[s] > 0.0f ? double(segments) / turns[s] : 0.0);
        }
        std::printf("segments %3d (%s): %.1f points per turn (median); dense curve's farthest from it: curl mean %.4f p99 %.4f max %.4f; "
                    "final mean %.4f p99 %.4f max %.4f (units; strand width %.4f)\n",
                    segments, segments == 40 ? "shipped" : "       ", perTurn.quantile(0.5), errCurl.mean(), errCurl.quantile(0.99),
                    errCurl.max(), err.mean(), err.quantile(0.99), err.max(), width);
        if (jsonOut) {
            std::fprintf(jsonOut, "{\"kind\":\"sampling\",\"segments\":%d,\"perTurn\":%.2f,\"curlMean\":%.5f,\"curlP99\":%.5f,\"curlMax\":%.5f,"
                         "\"finalMean\":%.5f,\"finalP99\":%.5f,\"finalMax\":%.5f,\"width\":%.5f}\n",
                         segments, perTurn.quantile(0.5), errCurl.mean(), errCurl.quantile(0.99), errCurl.max(), err.mean(),
                         err.quantile(0.99), err.max(), width);
        }
    }

    // ---- 2. Sweep: base spline per strand + one offset curve per clump ----
    const std::vector<float>* clumpIds = dense.curled->strandChannel("clumpId");
    const std::size_t n = dense.curled->strandPointCount(0);
    std::vector<float> t(n);
    for (std::size_t i = 0; i < n; ++i) t[i] = float(i) / float(n - 1);
    // Clumps and their shared offsets (the mean of the members' offsets) and how far members stray.
    std::map<long long, std::vector<std::size_t>> clumps;
    for (std::size_t s = 0; s < N; ++s) {
        const long long id = clumpIds ? std::llround((*clumpIds)[s]) : -1;
        clumps[id].push_back(s);
    }
    std::map<long long, std::vector<Vec3f>> shared;
    float stray = 0.0f;
    for (const auto& [id, members] : clumps) {
        std::vector<Vec3f> o(n);
        for (const std::size_t s : members) {
            for (std::size_t i = 0; i < n; ++i) o[i] += dense.curled->strandPoints(s)[i] - dense.base->strandPoints(s)[i];
        }
        for (Vec3f& v : o) v = v * (1.0f / float(members.size()));
        for (const std::size_t s : members) {
            for (std::size_t i = 0; i < n; ++i) {
                const Vec3f own = dense.curled->strandPoints(s)[i] - dense.base->strandPoints(s)[i];
                stray = std::max(stray, (own - o[i]).length());
            }
        }
        shared[id] = std::move(o);
    }
    std::printf("\nsweep: %zu strands in %zu clumps (%.1f a clump); a member's curl offset differs from its clump's by at most %.2e\n", N,
                clumps.size(), double(N) / double(clumps.size()), stray);
    const float epsList[] = {0.01f * width, 0.1f * width, 0.25f * width, 1.0f * width};
    // The analytic sweep: each strand's base as a smooth spline (16 control points, 16-bit), a curl
    // about it in its rotation-minimizing frames, angle 2 pi f s + phase over the base's arc length
    // s, radius r times the preset's profile. The truth is the sweep itself (4096 samples a strand);
    // the representations are measured against it: a polyline of m points a turn (what Curl makes
    // at its input's points), the polyline cut to eps (Douglas-Peucker), a generic spline, and the
    // sweep's own numbers (exact up to the frames' discretization).
    struct Case {
        const char* name;
        float radius, frequency;  // units, turns per unit
    };
    const Case cases[] = {{"curly (preset)", curlRadius, curlFrequency}, {"coily (coil, 3 widths, 3 turns/unit)", 3.0f * width, 3.0f}};
    const Ramp profile{{0.0f, 0.1f}, {0.2f, 1.0f}, {1.0f, 1.0f}};
    const std::size_t kTruth = 4096;
    const int kBase = 16;
    const std::size_t sweepStrands = std::min<std::size_t>(N, 2000);
    for (const Case& cs : cases) {
        // Bases and the sweeps' numbers.
        std::vector<std::vector<Vec3f>> baseCtrl(sweepStrands);
        std::vector<float> freq(sweepStrands), phase(sweepStrands), radius(sweepStrands), total(sweepStrands);
        Rng rng(99);
        for (std::size_t s = 0; s < sweepStrands; ++s) {
            baseCtrl[s].assign(kBase, Vec3f{});
            fitSpline(dense.base->strandPoints(s), t.data(), n, kBase, baseCtrl[s].data());
            quantize16(baseCtrl[s].data(), kBase);
            freq[s] = cs.frequency * (1.0f + 0.175f * (2.0f * rng.nextFloat() - 1.0f));
            phase[s] = 6.2831853f * rng.nextFloat();
            radius[s] = cs.radius * (1.0f + 0.35f * (2.0f * rng.nextFloat() - 1.0f));
        }
        // The sweep's frames and arc length: rotation-minimizing frames at a fixed resolution along
        // the base spline (kFrames samples, whatever the sweep is made at, so it is one curve at
        // any point count), seeded from the spline's own tangent at the root across the world axis
        // it is least aligned with (well conditioned whichever way the strand leaves the skin).
        // A sweep at m points (parameters even in t): base on the spline, frame and arc from the
        // tables, the offset r profile(t) (eased in over the first 2%: continuous at the root).
        constexpr std::size_t kFrames = 256;
        std::vector<std::vector<Frame>> frameTable(sweepStrands);
        std::vector<std::vector<float>> arcTable(sweepStrands);
        auto makeTables = [&](std::size_t s) {
            std::vector<Vec3f> b(kFrames);
            for (std::size_t i = 0; i < kFrames; ++i) b[i] = evalSpline(baseCtrl[s].data(), kBase, float(i) / float(kFrames - 1));
            const Vec3f tan0 = (evalSpline(baseCtrl[s].data(), kBase, 1e-4f) - evalSpline(baseCtrl[s].data(), kBase, 0.0f)).normalized();
            const Vec3f axis = std::fabs(tan0.x) < std::fabs(tan0.y) ? (std::fabs(tan0.x) < std::fabs(tan0.z) ? Vec3f(1, 0, 0) : Vec3f(0, 0, 1))
                                                                     : (std::fabs(tan0.y) < std::fabs(tan0.z) ? Vec3f(0, 1, 0) : Vec3f(0, 0, 1));
            rotationMinimizingFramesInto(b.data(), kFrames, tan0.cross(axis).normalized(), frameTable[s]);
            arcTable[s].assign(kFrames, 0.0f);
            for (std::size_t i = 1; i < kFrames; ++i) arcTable[s][i] = arcTable[s][i - 1] + (b[i] - b[i - 1]).length();
        };
        parallelFor(0, sweepStrands, makeTables, 8);
        auto sweep = [&](std::size_t s, std::size_t m, std::vector<Vec3f>& out) {
            out.resize(m);
            const std::vector<Frame>& fr = frameTable[s];
            const std::vector<float>& arc = arcTable[s];
            for (std::size_t i = 0; i < m; ++i) {
                const float tt = float(i) / float(m - 1);
                const float x = tt * float(kFrames - 1);
                const std::size_t k = std::min(static_cast<std::size_t>(x), kFrames - 2);
                const float f = x - float(k);
                const Vec3f nrm = (fr[k].normal * (1.0f - f) + fr[k + 1].normal * f).normalized();
                const Vec3f bin = (fr[k].binormal * (1.0f - f) + fr[k + 1].binormal * f).normalized();
                const float a = arc[k] + (arc[k + 1] - arc[k]) * f;
                const float angle = 6.2831853f * freq[s] * a + phase[s];
                const float r = radius[s] * profile(tt) * std::min(1.0f, tt / 0.02f);
                out[i] = evalSpline(baseCtrl[s].data(), kBase, tt) + nrm * (r * std::cos(angle)) + bin * (r * std::sin(angle));
            }
        };
        std::vector<std::vector<Vec3f>> truth(sweepStrands);
        parallelFor(0, sweepStrands, [&](std::size_t s) {
            sweep(s, kTruth, truth[s]);
            total[s] = arcLength(truth[s].data(), kTruth);
        }, 8);
        // Bases whose fitted spline folds back (a step of the frame table turning more than 30
        // degrees: a kink of the incoming strand the 16-point fit loops round) are left out of the
        // sweep's statistics and counted.
        std::vector<char> smooth(sweepStrands, 1);
        std::size_t folded = 0;
        for (std::size_t s = 0; s < sweepStrands; ++s) {
            for (std::size_t i = 1; i < kFrames && smooth[s]; ++i) {
                smooth[s] = frameTable[s][i].tangent.dot(frameTable[s][i - 1].tangent) > 0.866f ? 1 : 0;
            }
            folded += smooth[s] ? 0 : 1;
        }
        std::printf("  (bases folding back, left out: %zu of %zu)\n", folded, sweepStrands);
        if (std::getenv("SWEEP_DEBUG2")) {
            // The 16 pts/turn polyline's worst strands: their base's sharpest turn per table step.
            std::vector<std::pair<float, std::size_t>> worst;
            for (std::size_t s = 0; s < sweepStrands; ++s) {
                Stats tmp;
                const float turnsHere = arcTable[s].back() * freq[s];
                const std::size_t m = std::max<std::size_t>(2, static_cast<std::size_t>(std::ceil(turnsHere * 16)) + 1);
                std::vector<Vec3f> poly(m);
                for (std::size_t i = 0; i < m; ++i) poly[i] = truth[s][std::min(kTruth - 1, static_cast<std::size_t>(std::llround(double(i) / double(m - 1) * double(kTruth - 1))))];
                worst.push_back({distanceToPolyline(truth[s].data(), kTruth, poly.data(), m), s});
            }
            std::sort(worst.rbegin(), worst.rend());
            for (int w = 0; w < 5; ++w) {
                const std::size_t s = worst[w].second;
                float sharp = 0.0f;
                for (std::size_t i = 1; i < kFrames; ++i) sharp = std::max(sharp, std::acos(std::clamp(frameTable[s][i].tangent.dot(frameTable[s][i - 1].tangent), -1.0f, 1.0f)));
                std::printf("debug2: strand %zu error %.3f base length %.2f sharpest step %.1f deg; base ctrl[0..3] (%.2f %.2f %.2f) (%.2f %.2f %.2f)\n", s,
                            worst[w].first, arcTable[s].back(), sharp * 57.3f, baseCtrl[s][0].x, baseCtrl[s][0].y, baseCtrl[s][0].z,
                            baseCtrl[s][1].x, baseCtrl[s][1].y, baseCtrl[s][1].z);
            }
        }
        if (std::getenv("SWEEP_DEBUG")) {
            for (std::size_t s = 0; s < 3; ++s) {
                float jump = 0.0f;
                std::size_t at = 0;
                for (std::size_t i = 1; i < kTruth; ++i) {
                    const float d = (truth[s][i] - truth[s][i - 1]).length();
                    if (d > jump) {
                        jump = d;
                        at = i;
                    }
                }
                std::vector<Vec3f> half;
                for (std::size_t i = 0; i < kTruth; i += 2) half.push_back(truth[s][i]);
                std::printf("debug strand %zu: largest step %.4f at %zu (mean %.5f); self distance %.2e; half %.2e\n", s, jump, at,
                            total[s] / float(kTruth - 1), distanceToPolyline(truth[s].data(), kTruth, truth[s].data(), kTruth),
                            distanceToPolyline(truth[s].data(), kTruth, half.data(), half.size()));
            }
        }
        Stats turnsStat;
        for (std::size_t s = 0; s < sweepStrands; ++s) {
            std::vector<Vec3f> b(kTruth);
            for (std::size_t i = 0; i < kTruth; ++i) b[i] = evalSpline(baseCtrl[s].data(), kBase, float(i) / float(kTruth - 1));
            turnsStat.add(arcLength(b.data(), kTruth) * freq[s]);
        }
        std::printf("\nsweep case %s: radius %.4f, %.1f turns a strand (median), %zu strands\n", cs.name, cs.radius, turnsStat.quantile(0.5),
                    sweepStrands);
        // Polyline of m points a turn (the sweep sampled there: what Curl makes on its input's
        // points, with exact frames): its error against the truth, and the chord formula.
        for (const int perTurn : {4, 6, 8, 11, 16, 24, 32, 48, 64}) {
            Stats err;
            std::vector<float> e(sweepStrands);
            parallelFor(0, sweepStrands, [&](std::size_t s) {
                const std::size_t m = std::max<std::size_t>(2, static_cast<std::size_t>(std::ceil(turnsStat.v[s] * perTurn)) + 1);
                thread_local std::vector<Vec3f> poly;
                poly.resize(m);
                for (std::size_t i = 0; i < m; ++i) {
                    const std::size_t j = std::min(kTruth - 1, static_cast<std::size_t>(std::llround(double(i) / double(m - 1) * double(kTruth - 1))));
                    poly[i] = truth[s][j];
                }
                e[s] = distanceToPolyline(truth[s].data(), kTruth, poly.data(), m);
            }, 8);
            for (std::size_t s = 0; s < sweepStrands; ++s) {
                if (smooth[s]) err.add(e[s]);
            }
            const double chord = cs.radius * (1.0 - std::cos(3.14159265 / perTurn));
            std::printf("  polyline %2d pts/turn: max error mean %.2e p99 %.2e (%.2f widths) max %.2e, chord formula r(1-cos(pi/m)) %.2e; "
                        "%.0f B a strand\n",
                        perTurn, err.mean(), err.quantile(0.99), err.quantile(0.99) / width, err.max(), chord, (turnsStat.quantile(0.5) * perTurn + 1) * 12.0);
            if (jsonOut) {
                std::fprintf(jsonOut, "{\"kind\":\"polyline\",\"case\":\"%s\",\"perTurn\":%d,\"errMean\":%.3e,\"errMax\":%.3e,\"chord\":%.3e,"
                             "\"bytes\":%.0f}\n",
                             cs.name, perTurn, err.mean(), err.max(), chord, (turnsStat.quantile(0.5) * perTurn + 1) * 12.0);
            }
        }
        // Douglas-Peucker on the truth, and the sweep reconstructed at a pts/turn (frames from
        // its own samples: their drift is the sweep's only error besides the chords).
        for (const float eps : {0.1f * width, 0.25f * width, 1.0f * width}) {
            std::atomic<long long> dp{0};
            parallelFor(0, sweepStrands, [&](std::size_t s) { dp += static_cast<long long>(douglasPeucker(truth[s].data(), kTruth, eps)); }, 8);
            // Points a turn the chord formula asks for eps (at the strand's radius), and the
            // reconstruction's measured error with that many.
            const int m = static_cast<int>(std::ceil(3.14159265 / std::acos(std::max(-1.0, 1.0 - double(eps) / cs.radius))));
            Stats rec;
            std::vector<float> e(sweepStrands);
            parallelFor(0, sweepStrands, [&](std::size_t s) {
                const std::size_t count = static_cast<std::size_t>(std::ceil(turnsStat.v[s] * m)) + 1;
                thread_local std::vector<Vec3f> poly;
                sweep(s, count, poly);
                e[s] = distanceToPolyline(truth[s].data(), kTruth, poly.data(), count);
            }, 8);
            for (std::size_t s = 0; s < sweepStrands; ++s) {
                if (smooth[s]) rec.add(e[s]);
            }
            const double sweepBytes = kBase * 6.0 + 24.0 + 12.0;  // 16-bit base, its box, f / phase / radius
            std::printf("  eps %.2f widths: polyline cut to eps %.0f pts = %.0f B; chord formula %d pts/turn = %.0f pts; the sweep "
                        "made with that many: error p99 %.2e (%.2f widths) max %.2e mean %.2e; sweep stored in %.0f B (%.0fx less than the cut "
                        "polyline)\n",
                        eps / width, double(dp.load()) / double(sweepStrands), double(dp.load()) * 12.0 / double(sweepStrands), m,
                        turnsStat.quantile(0.5) * m + 1, rec.quantile(0.99), rec.quantile(0.99) / width, rec.max(), rec.mean(), sweepBytes,
                        double(dp.load()) * 12.0 / double(sweepStrands) / sweepBytes);
            if (jsonOut) {
                std::fprintf(jsonOut, "{\"kind\":\"sweep\",\"case\":\"%s\",\"widths\":%.2f,\"dpPts\":%.1f,\"chordPerTurn\":%d,\"recMax\":%.3e,"
                             "\"recMean\":%.3e,\"sweepBytes\":%.0f}\n",
                             cs.name, eps / width, double(dp.load()) / double(sweepStrands), m, rec.max(), rec.mean(), sweepBytes);
            }
        }
        // Screen-adaptive: points to draw a strand within half a pixel at a few framings (pixels
        // per unit: the head filling a 1080p view, a quarter of it, a tenth), against the fixed 41.
        for (const double pxPerUnit : {50.0, 12.0, 4.0}) {
            const double rpx = cs.radius * pxPerUnit;
            double perTurn = rpx > 0.5 ? 3.14159265 / std::acos(1.0 - 0.5 / rpx) : 0.0;  // below: a ribbon of the coil's band
            const double turnsMedian = turnsStat.quantile(0.5);
            const double points = rpx > 0.5 ? turnsMedian * perTurn + 1 : 9.0;  // (the base alone: its spline within half a pixel)
            std::printf("  %.0f px a unit: radius %.2f px -> %s, %.0f points a strand (shipped Curl: %d)\n", pxPerUnit, rpx,
                        rpx > 0.5 ? "a helix" : "a ribbon as wide as the coil, its coverage the fiber's share", points, 41);
            if (jsonOut) {
                std::fprintf(jsonOut, "{\"kind\":\"adaptive\",\"case\":\"%s\",\"pxPerUnit\":%.0f,\"rpx\":%.3f,\"points\":%.1f}\n", cs.name, pxPerUnit,
                             rpx, points);
            }
        }
        // Reconstruction throughput: sweeps made at 16 points a turn (frames included).
        {
            std::vector<std::vector<Vec3f>> outs(sweepStrands);
            std::atomic<long long> made{0};
            const double ms = bestMs(3, [&] {
                made = 0;
                parallelFor(0, sweepStrands, [&](std::size_t s) {
                    const std::size_t count = static_cast<std::size_t>(std::ceil(turnsStat.v[s] * 16)) + 1;
                    sweep(s, count, outs[s]);
                    made += static_cast<long long>(count);
                }, 8);
            });
            const double tablesMs = bestMs(3, [&] { parallelFor(0, sweepStrands, makeTables, 8); });
            std::printf("  reconstruction: %.0f Mpts/s on the CPU (24 threads; base spline, frames and arc from the tables, offsets); "
                        "the tables (%zu RMF samples a strand) %.3f us a strand\n",
                        double(made.load()) / ms / 1e3, kFrames, tablesMs * 1e3 / double(sweepStrands));
        }
    }

    // ---- 3. Generic: the final strand (frizz included) as a quantized spline ----
    std::printf("\ngeneric (final strands, frizz included):\n");
    for (const float eps : epsList) {
        std::atomic<long long> pts{0}, dp{0};
        parallelFor(0, N, [&](std::size_t s) {
            const Vec3f* p = dense.final->strandPoints(s);
            std::vector<Vec3f> c;
            int k = 4;
            for (;; k = k < 8 ? k + 1 : k + k / 4) {
                c.assign(std::size_t(k), Vec3f{});
                fitSpline(p, t.data(), n, k, c.data());
                quantize16(c.data(), k);
                float e = 0.0f;
                for (std::size_t i = 0; i < n; ++i) e = std::max(e, (evalSpline(c.data(), k, t[i]) - p[i]).length());
                if (e <= eps || k >= int(n)) break;
            }
            pts += k;
            dp += static_cast<long long>(douglasPeucker(p, n, eps));
        }, 32);
        const double splineBytes = (double(pts.load()) * 6.0 + double(N) * 24.0) / double(N);
        const double polyBytes = double(dp.load()) * 12.0 / double(N);
        std::printf("eps %.5f (%.2f widths): spline %.1f ctrl pts = %.0f B a strand; polyline cut %.1f pts = %.0f B; dense %zu pts = %.0f B; "
                    "spline %.1fx smaller than dense, %.1fx than the cut polyline\n",
                    eps, eps / width, double(pts.load()) / double(N), splineBytes, double(dp.load()) / double(N), polyBytes, n,
                    double(n) * 12.0, double(n) * 12.0 / splineBytes, polyBytes / splineBytes);
        if (jsonOut) {
            std::fprintf(jsonOut, "{\"kind\":\"generic\",\"eps\":%.6f,\"widths\":%.3f,\"splinePts\":%.2f,\"splineBytes\":%.1f,\"dpPts\":%.2f,"
                         "\"polyBytes\":%.1f,\"denseBytes\":%.0f}\n",
                         eps, eps / width, double(pts.load()) / double(N), splineBytes, double(dp.load()) / double(N), polyBytes,
                         double(n) * 12.0);
        }
    }

    // ---- 4. Reconstruction cost: points out per second ----
    {
        const int k = 24;  // a typical generic spline at a quarter width (see above)
        std::vector<Vec3f> ctrl(N * std::size_t(k));
        parallelFor(0, N, [&](std::size_t s) { fitSpline(dense.final->strandPoints(s), t.data(), n, k, ctrl.data() + s * std::size_t(k)); }, 32);
        std::vector<Vec3f> outPts(N * n);
        const double splineMs = bestMs(5, [&] {
            parallelFor(0, N, [&](std::size_t s) {
                for (std::size_t i = 0; i < n; ++i) outPts[s * n + i] = evalSpline(ctrl.data() + s * std::size_t(k), k, t[i]);
            }, 64);
        });
        const double copyMs = bestMs(5, [&] {
            parallelFor(0, N, [&](std::size_t s) {
                std::copy(dense.final->strandPoints(s), dense.final->strandPoints(s) + n, outPts.data() + s * n);
            }, 64);
        });
        std::printf("\nreconstruction (%zu strands x %zu points): spline of %d ctrl pts %.2f ms (%.0f Mpts/s), copying the dense points "
                    "%.2f ms\n",
                    N, n, k, splineMs, double(N * n) / splineMs / 1e3, copyMs);
    }
    if (jsonOut) std::fclose(jsonOut);
    return 0;
}
