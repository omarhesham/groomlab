// SPDX-License-Identifier: Apache-2.0
// B1 measurement (docs/research/curl_sweeps.md, "Curl v2 math"): how far Curl's output at a
// strand resolution is from the same groom's curl in the limit, the points it makes, and its time.
//
//   research_curl_b1 [--legacy] [--density 0.1] [--strands 500000] [--out results.jsonl]
//
// --legacy measures the Curl before v2 (legacy_curl.h, a copy of it), else the shipped one.
// 1. The shipped curly preset (ringlets: Curl coherent by clump) and a coily groom (the same bases,
//    each strand coiled on its own: radius 3 strand widths, 3 turns a unit), on the app's head,
//    evaluated with Resample (so Scatter, Interpolate and Curl) at 16, 40, 64 and 128 segments
//    against 256 (the limit): the farthest any point of the limit's curl is from the coarse
//    polyline (Curl's output, before frizz), the points a strand and a turn. Then Curl's own part
//    of it, on the limit's own input taken at these segments (each point at its share), so the
//    nodes before Curl, which depend on the resolution too, are left out: each point against the
//    limit's curl at its share (Curl adding no points: its math alone), and the limit's curl
//    against the coarse polyline (as the groom's Curl makes it: points added included); and the
//    floor, the limit's input against its own coarse polyline.
// 2. Curl's own time on `--strands` strands of 16 points (the GPU benchmark's: Scatter, Clump),
//    each strand on its own and coherent by clump, adding no points (tolerance 0) and as it is.

#include <atomic>

#include <nlohmann/json.hpp>

#include "../common/research_common.h"
#include "groomlab/curve.h"
#include "groomlab/presets.h"
#include "legacy_curl.h"

using namespace research;

namespace {

std::string curlType = op::kCurl;  // or "CurlLegacy"

// The farthest any point of `dense` (n points) is from polyline `q` (m points): each dense point
// against every segment of q (a turn of a curl lies far from the next: its pitch is many radii).
float distanceToPolyline(const Vec3f* dense, std::size_t n, const Vec3f* q, std::size_t m) {
    float worst = 0.0f;
    for (std::size_t i = 0; i < n; ++i) {
        float best = 1e30f;
        for (std::size_t s = 0; s + 1 < m; ++s) {
            const Vec3f a = q[s], ab = q[s + 1] - a;
            const float len2 = ab.dot(ab);
            const float u = len2 > 0.0f ? std::clamp((dense[i] - a).dot(ab) / len2, 0.0f, 1.0f) : 0.0f;
            best = std::min(best, (dense[i] - (a + ab * u)).length());
        }
        worst = std::max(worst, best);
    }
    return worst;
}

struct CurlRun {
    StrandsPtr base, curled;  // Curl's input and output
    ParamSet params;          // Curl's
    double curlMs = 0.0;
};

// Curl (with `params`) on `in`, on the head (its scalp normals seed the frames).
StrandsPtr curlOn(const StrandsPtr& in, const ParamSet& params, const MeshPtr& scalp) {
    ExternalInputs ext;
    setGroomExternals(ext, scalp, nullptr, nullptr);
    EvalContext ctx;
    ctx.externals = &ext;
    auto curl = OperatorRegistry::instance().create(curlType);
    curl->params() = params;
    return valueAs<StrandsPtr>(curl->evaluate(ctx, {Value(in)}));
}
// `in` at `segments` segments, each new point at its share of the points (by index, as the graph's
// nodes place a strand's points at every resolution: Curl by clump reads that share), its
// strand channels kept.
StrandsPtr resampled(const StrandsPtr& in, int segments) {
    auto out = std::make_shared<StrandSet>();
    const std::size_t m = static_cast<std::size_t>(segments) + 1;
    out->resizeUniform(in->strandCount(), m);
    out->mutableIds() = in->ids();
    out->mutableRoots() = in->roots();
    for (const std::string& name : in->strandChannelNames()) {
        out->addStrandChannel(name, in->strandChannelDefault(name)) = *in->strandChannel(name);
    }
    parallelFor(0, in->strandCount(), [&](std::size_t s) {
        Vec3f* to = out->strandPoints(s);
        for (std::size_t j = 0; j < m; ++j) {
            to[j] = pointAlong(in->strandPoints(s), in->strandPointCount(s), float(j) / float(m - 1));
        }
    });
    return out;
}

CurlRun evaluate(const std::string& graphJson, const MeshPtr& scalp, const GuidesPtr& guides, int segments, float density,
                 const nlohmann::json& curlParams) {
    nlohmann::json g = nlohmann::json::parse(graphJson);
    for (auto& node : g["nodes"]) {
        if (node["name"] == "resample") node["params"]["segments"] = segments;
        if (node["name"] == "curl") {
            node["type"] = curlType;
            for (auto it = curlParams.begin(); it != curlParams.end(); ++it) node["params"][it.key()] = it.value();
        }
    }
    GraphDocument doc = loadGraphJson(g.dump());
    ExternalInputs ext;
    setGroomExternals(ext, scalp, guides, nullptr);
    EvalContext ctx;
    ctx.externals = &ext;
    ctx.previewDensity = density;
    CurlRun out;
    doc.graph.evaluateStrands(doc.output, ctx);
    out.base = doc.graph.evaluateStrands(doc.node("clump"), ctx);
    out.curled = doc.graph.evaluateStrands(doc.node("curl"), ctx);
    // Curl alone, again on the same input (best of 3).
    auto curl = OperatorRegistry::instance().create(curlType);
    curl->params() = doc.graph.node(doc.node("curl")).params();
    out.curlMs = bestMs(3, [&] { curl->evaluate(ctx, {Value(out.base)}); });
    out.params = curl->params();
    return out;
}

std::FILE* jsonOut = nullptr;

}  // namespace

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    float density = 0.1f;
    std::size_t strands = 500000;
    std::string out;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--density" && i + 1 < argc) density = std::stof(argv[++i]);
        else if (a == "--strands" && i + 1 < argc) strands = static_cast<std::size_t>(std::stod(argv[++i]));
        else if (a == "--legacy") curlType = "CurlLegacy";
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
    }
    legacy::registerCurlLegacy();
    const bool v2 = curlType == op::kCurl;
    if (!out.empty()) jsonOut = std::fopen(out.c_str(), "w");
    const MeshPtr head = loadObj(std::string(RESEARCH_DATA) + "/app/head_scalp.obj");
    const Vec3f box = boxSize(*head);
    const float size = std::max({box.x, box.y, box.z});
    std::ifstream f(std::string(RESEARCH_PRESETS) + "/curly.json");
    const std::string presetText((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::string graph = presetGraphJson(presetText, double(size));
    float curlRadius = 0.0f, curlFrequency = 0.0f, width = 0.0f;
    const nlohmann::json fitted = nlohmann::json::parse(graph);
    for (const auto& node : fitted["nodes"]) {
        if (node["name"] == "curl") {
            curlRadius = node["params"]["radius"].get<float>();
            curlFrequency = node["params"]["frequency"].get<float>();
        }
        if (node["name"] == "width") width = node["params"]["width"].get<float>();
    }
    const GuidesPtr guides = hairstyleGuides(head, 400, size, 8, 11, 0.25f);
    std::printf("%s; head size %.1f (units: cm), curl radius %.3f, frequency %.3f turns/unit, strand width %.4f, preview density %.2f\n",
                v2 ? "Curl v2" : "Curl before v2", size, curlRadius, curlFrequency, width, density);

    struct Case {
        const char* name;
        nlohmann::json params;
        float frequency;
    };
    const Case cases[] = {
        {"curly", nlohmann::json::object(), curlFrequency},
        {"coily", {{"coherence", "strand"}, {"radius", 3.0 * width}, {"frequency", 3.0}}, 3.0f},
    };
    for (const Case& cs : cases) {
        const CurlRun dense = evaluate(graph, head, guides, 256, density, cs.params);
        const std::size_t N = dense.curled->strandCount();
        Stats densePoints;
        for (std::size_t s = 0; s < N; ++s) densePoints.add(double(dense.curled->strandPointCount(s)));
        std::printf("\n%s: limit 256 segments, %zu strands, %.0f points a strand (mean)\n", cs.name, N, densePoints.mean());
        // Curl's own math: the limit's input curled at its own points (v2: tolerance 0, no points added).
        ParamSet exact = dense.params;
        if (v2) exact.set("tolerance", 0.0);
        const StrandsPtr denseExact = curlOn(dense.base, exact, head);
        for (const int segments : {16, 40, 64, 128}) {
            const CurlRun g = evaluate(graph, head, guides, segments, density, cs.params);
            if (g.curled->strandCount() != N) {
                std::printf("  segments %d: %zu strands (not the same set)\n", segments, g.curled->strandCount());
                continue;
            }
            const StrandsPtr coarseIn = resampled(dense.base, segments);
            const StrandsPtr sameExact = curlOn(coarseIn, exact, head);      // at its own points
            const StrandsPtr sameShape = curlOn(coarseIn, g.params, head);   // as the groom's Curl makes it
            Stats err, points, perTurn, pointErr, shapeErr, inputErr;
            std::vector<float> e(N), ep(N), es(N), ei(N);
            parallelFor(0, N, [&](std::size_t s) {
                e[s] = distanceToPolyline(dense.curled->strandPoints(s), dense.curled->strandPointCount(s), g.curled->strandPoints(s),
                                          g.curled->strandPointCount(s));
                // Each point against the limit's curl at its share (both at their own points: by index).
                const std::size_t m = sameExact->strandPointCount(s), dn = denseExact->strandPointCount(s);
                float worst = 0.0f;
                for (std::size_t i = 0; i < m; ++i) {
                    const Vec3f at = pointAlong(denseExact->strandPoints(s), dn, float(i) / float(m - 1));
                    worst = std::max(worst, (sameExact->strandPoints(s)[i] - at).length());
                }
                ep[s] = worst;
                es[s] = distanceToPolyline(denseExact->strandPoints(s), dn, sameShape->strandPoints(s), sameShape->strandPointCount(s));
                ei[s] = distanceToPolyline(dense.base->strandPoints(s), dense.base->strandPointCount(s), coarseIn->strandPoints(s),
                                           coarseIn->strandPointCount(s));
            }, 16);
            for (std::size_t s = 0; s < N; ++s) {
                err.add(e[s]);
                pointErr.add(ep[s]);
                shapeErr.add(es[s]);
                inputErr.add(ei[s]);
                const std::size_t n = g.curled->strandPointCount(s);
                points.add(double(n));
                const float turns = arcLength(dense.base->strandPoints(s), dense.base->strandPointCount(s)) * cs.frequency;
                perTurn.add(turns > 0.0f ? double(n - 1) / turns : 0.0);
            }
            auto line = [&](const char* what, Stats& st) {
                std::printf("      %-58s mean %.4f mm p99 %.4f mm max %.4f mm (%.2f widths)\n", what, st.mean() * 10.0, st.quantile(0.99) * 10.0,
                            st.max() * 10.0, st.max() / width);
            };
            std::printf("  segments %3d: %5.1f points a strand (max %3.0f), %5.1f a turn (median, min %4.1f); Curl %.1f ms\n", segments,
                        points.mean(), points.max(), perTurn.quantile(0.5), perTurn.quantile(0.0), g.curlMs);
            line("the groom's curl off the limit's (polyline):", err);
            line("Curl alone, each point off the limit's curl at its share:", pointErr);
            line("Curl alone, the limit's curl off its polyline:", shapeErr);
            line("(the input alone, the limit's off its polyline:)", inputErr);
            if (jsonOut) {
                std::fprintf(jsonOut,
                             "{\"kind\":\"sampling\",\"curl\":\"%s\",\"case\":\"%s\",\"segments\":%d,\"points\":%.2f,\"pointsMax\":%.0f,"
                             "\"perTurn\":%.2f,\"perTurnMin\":%.2f,\"curlMs\":%.2f,\"groom\":[%.4f,%.4f,%.4f],\"point\":[%.5f,%.5f,%.5f],"
                             "\"shape\":[%.4f,%.4f,%.4f],\"input\":[%.4f,%.4f,%.4f]}\n",
                             curlType.c_str(), cs.name, segments, points.mean(), points.max(), perTurn.quantile(0.5), perTurn.quantile(0.0),
                             g.curlMs, err.mean() * 10.0, err.quantile(0.99) * 10.0, err.max() * 10.0, pointErr.mean() * 10.0,
                             pointErr.quantile(0.99) * 10.0, pointErr.max() * 10.0, shapeErr.mean() * 10.0, shapeErr.quantile(0.99) * 10.0,
                             shapeErr.max() * 10.0, inputErr.mean() * 10.0, inputErr.quantile(0.99) * 10.0, inputErr.max() * 10.0);
            }
        }
    }

    // ---- 2. Curl's time on many strands: before v2 and v2, taking turns (best of 5 each) ----
    if (strands > 0) {
        GraphDocument doc = loadGraphJson(R"({"version": 1, "output": "clump", "nodes": [
            {"name": "scalp", "type": "Scalp"},
            {"name": "scatter", "type": "Scatter", "params": {"density": )" +
                                          std::to_string(double(strands) / head->totalArea()) + R"(, "length": )" +
                                          std::to_string(size * 0.45) + R"(}},
            {"name": "resample", "type": "Resample", "params": {"segments": 15}},
            {"name": "clump", "type": "Clump", "params": {"clumpBy": "count", "count": 400, "strength": 0.8}}],
          "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"},
                          {"from": "scatter", "to": "resample", "input": "strands"},
                          {"from": "resample", "to": "clump", "input": "strands"}]})");
        ExternalInputs ext;
        setGroomExternals(ext, head, nullptr, nullptr);
        EvalContext ctx;
        ctx.externals = &ext;
        const StrandsPtr clumped = doc.graph.evaluateStrands(doc.output, ctx);
        for (const char* coherence : {"strand", "clump"}) {
            struct Variant {
                const char* name;
                std::string type;
                double tolerance;  // < 0: none (before v2)
            };
            const Variant variants[] = {{"before v2", "CurlLegacy", -1.0}, {"v2, tolerance 0", op::kCurl, 0.0}, {"v2, tolerance 0.1", op::kCurl, 0.1}};
            double best[3] = {1e30, 1e30, 1e30};
            double pointsOut[3] = {};
            for (int round = 0; round < 5; ++round) {
                for (int v = 0; v < 3; ++v) {
                    auto curl = OperatorRegistry::instance().create(variants[v].type);
                    curl->params().set("radius", 0.3);
                    curl->params().set("frequency", 0.5);
                    curl->params().set("coherence", std::string(coherence));
                    if (variants[v].tolerance >= 0.0) curl->params().set("tolerance", variants[v].tolerance);
                    StrandsPtr made;
                    const auto t0 = std::chrono::steady_clock::now();
                    made = valueAs<StrandsPtr>(curl->evaluate(ctx, {Value(clumped)}));
                    best[v] = std::min(best[v], msSince(t0));
                    pointsOut[v] = double(made->pointCount()) / double(made->strandCount());
                }
            }
            for (int v = 0; v < 3; ++v) {
                std::printf("\nCurl (%s, %s) on %zu strands of %zu points: %.1f ms (%.1f points a strand out)", coherence, variants[v].name,
                            clumped->strandCount(), clumped->strandPointCount(0), best[v], pointsOut[v]);
                if (jsonOut) {
                    std::fprintf(jsonOut, "{\"kind\":\"time\",\"variant\":\"%s\",\"coherence\":\"%s\",\"strands\":%zu,\"ms\":%.2f,\"points\":%.2f}\n",
                                 variants[v].name, coherence, clumped->strandCount(), best[v], pointsOut[v]);
                }
            }
            std::printf("\n");
        }
    }
    if (jsonOut) std::fclose(jsonOut);
    return 0;
}
