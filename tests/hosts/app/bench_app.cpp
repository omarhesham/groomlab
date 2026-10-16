// SPDX-License-Identifier: Apache-2.0
// The standalone app against the Maya benchmark (tools/bench/xgen_compare): the bench's head
// scalp (a radius-10 sphere of 160 x 100, cut below 15% of the radius under the equator,
// scaled 1, 1.1, 1.2, polySphere UVs) bending over frames 1-100 as a scalp cache, the bench's
// guides (straight along the normals, one per 1.6 cm cell) and graph (Scatter -> Interpolate ->
// Clump -> Noise -> Curl -> Width -> Resample, 16 points a strand), at 100k and
// 500k strands, timed through GroomDocument as the Maya worker times the groomLabShape: the
// creation with its first evaluation, a clump and a noise tweak to evaluated, the per-frame
// evaluation over frames 1-50 (followScalp off and on), the Alembic export of 24 frames (and
// the USD one, which Maya has no number for) and a headless playback (evaluate, build the
// display, draw offscreen). Skipped unless GROOMLAB_APP_BENCH names the JSON to write;
// tools/bench/xgen_compare/app_exports_vs_maya.py lays it beside the Maya results.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <string>
#include <vector>

#include <QImage>
#include <QSize>
#include <rhi/qrhi.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "groom_document.h"
#include "groomlab/alembic_io.h"
#include "groomlab/guide_hierarchy.h"
#include "offscreen.h"
#include "renderer.h"

namespace groomlab::app {
namespace {

using nlohmann::json;
namespace fs = std::filesystem;

constexpr float kHeadRadius = 10.0f;
constexpr int kSubdivX = 160, kSubdivY = 100;
constexpr float kHairLength = 15.0f;
constexpr float kGuideSpacing = 1.6f;
constexpr int kFrames = 100;
constexpr int kPlayFrames = 50;
constexpr double kFps = 24.0;

double now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

double median(std::vector<double> xs) {
    if (xs.empty()) return 0.0;
    std::sort(xs.begin(), xs.end());
    return xs[xs.size() / 2];
}

// The bench's head: polySphere's layout (rings of kSubdivX points between the poles), the faces
// whose center lies below -0.15 R dropped, then scaled to a head.
std::shared_ptr<ScalpMesh> makeHead() {
    std::vector<Vec3f> points;
    // Rings j = 1 .. kSubdivY - 1 (the poles apart), i around.
    for (int j = 1; j < kSubdivY; ++j) {
        const float phi = static_cast<float>(M_PI) * static_cast<float>(j) / static_cast<float>(kSubdivY);
        for (int i = 0; i < kSubdivX; ++i) {
            const float theta = 2.0f * static_cast<float>(M_PI) * static_cast<float>(i) / static_cast<float>(kSubdivX);
            points.push_back({kHeadRadius * std::sin(phi) * std::cos(theta), kHeadRadius * std::cos(phi), kHeadRadius * std::sin(phi) * std::sin(theta)});
        }
    }
    const std::int32_t top = static_cast<std::int32_t>(points.size());
    points.push_back({0.0f, kHeadRadius, 0.0f});
    const std::int32_t bottom = top + 1;
    points.push_back({0.0f, -kHeadRadius, 0.0f});
    auto ring = [](int j, int i) { return static_cast<std::int32_t>((j - 1) * kSubdivX + (i % kSubdivX)); };
    std::vector<ScalpMesh::Triangle> triangles;
    std::vector<ScalpMesh::UV> uvs;
    auto uv = [](int i, int j) { return ScalpMesh::UV{static_cast<float>(i) / kSubdivX, 1.0f - static_cast<float>(j) / kSubdivY}; };
    auto keep = [&](std::initializer_list<std::int32_t> corners) {
        float y = 0.0f;
        for (const std::int32_t c : corners) y += points[static_cast<std::size_t>(c)].y;
        return y / static_cast<float>(corners.size()) >= -0.15f * kHeadRadius;
    };
    auto tri = [&](std::int32_t a, std::int32_t b, std::int32_t c, ScalpMesh::UV ua, ScalpMesh::UV ub, ScalpMesh::UV uc) {
        triangles.push_back({a, b, c});
        uvs.push_back(ua);
        uvs.push_back(ub);
        uvs.push_back(uc);
    };
    // Wound so the normals point outward (y up, theta running from +x toward +z: a ring's next
    // point is clockwise seen from above, so the fan goes top, a, b).
    for (int i = 0; i < kSubdivX; ++i) {
        const std::int32_t a = ring(1, i), b = ring(1, i + 1);
        if (keep({top, a, b})) tri(top, a, b, uv(i, 0), uv(i, 1), uv(i + 1, 1));
        const std::int32_t c = ring(kSubdivY - 1, i), d = ring(kSubdivY - 1, i + 1);
        if (keep({bottom, c, d})) tri(bottom, d, c, uv(i, kSubdivY), uv(i + 1, kSubdivY - 1), uv(i, kSubdivY - 1));
    }
    for (int j = 1; j + 1 < kSubdivY; ++j) {
        for (int i = 0; i < kSubdivX; ++i) {
            const std::int32_t a = ring(j, i), b = ring(j, i + 1), c = ring(j + 1, i + 1), d = ring(j + 1, i);
            if (!keep({a, b, c, d})) continue;
            tri(a, b, c, uv(i, j), uv(i + 1, j), uv(i + 1, j + 1));
            tri(a, c, d, uv(i, j), uv(i + 1, j + 1), uv(i, j + 1));
        }
    }
    for (Vec3f& p : points) {
        p.y *= 1.1f;
        p.z *= 1.2f;
    }
    auto mesh = std::make_shared<ScalpMesh>(std::move(points), std::move(triangles));
    mesh->setCornerUVs(std::move(uvs));
    return mesh;
}

// The bend deformer keyed over the frames: 0 at the first, 40 at the middle, -40 at the last
// (degrees); each point turned about the handle (0.2 R under the center, 1.3 R tall) by its
// height's share of the curvature, so the crown swings about 5 cm sideways.
std::vector<Vec3f> bentPoints(const ScalpMesh& head, int frame) {
    const double t = static_cast<double>(frame - 1) / static_cast<double>(kFrames - 1);
    const double curvature = t < 0.5 ? 80.0 * t : 40.0 - 80.0 * (t - 0.5);
    const float yh = -0.2f * kHeadRadius, scale = 1.3f * kHeadRadius;
    std::vector<Vec3f> out = head.points();
    for (Vec3f& p : out) {
        const float yn = std::clamp((p.y - yh) / scale, -1.0f, 1.0f);
        const float angle = static_cast<float>(curvature * M_PI / 180.0) * yn * 0.5f;
        const float dy = p.y - yh;
        p.x += dy * std::sin(angle);
        p.y = yh + dy * std::cos(angle);
    }
    return out;
}

json benchGraph(double density) {
    return json::parse(R"({"nodes": [
        {"name": "scalp", "type": "Scalp"}, {"name": "guides", "type": "Guides"},
        {"name": "scatter", "type": "Scatter", "params": {"density": 0.0, "length": 15.0}},
        {"name": "interpolate", "type": "Interpolate"},
        {"name": "clump", "type": "Clump", "params": {"count": 400, "strength": 0.8}},
        {"name": "noise", "type": "Noise", "params": {"amplitude": 0.5, "frequency": 1.0}},
        {"name": "curl", "type": "Curl", "params": {"radius": 0.3, "frequency": 0.5, "tolerance": 0.0}},
        {"name": "width", "type": "Width", "params": {"width": 0.01}},
        {"name": "resample", "type": "Resample", "params": {"segments": 15, "mode": "even"}},
        {"name": "output", "type": "Output"}],
      "connections": [
        {"from": "scalp", "to": "scatter", "input": "scalp"}, {"from": "scatter", "to": "interpolate", "input": "strands"},
        {"from": "guides", "to": "interpolate", "input": "guides"}, {"from": "scalp", "to": "interpolate", "input": "scalp"},
        {"from": "interpolate", "to": "clump", "input": "strands"},
        {"from": "clump", "to": "noise", "input": "strands"},
        {"from": "noise", "to": "curl", "input": "strands"}, {"from": "curl", "to": "width", "input": "strands"},
        {"from": "width", "to": "resample", "input": "strands"}, {"from": "resample", "to": "output", "input": "strands"}],
      "output": "output"})")
        .patch(json::array({{{"op", "replace"}, {"path", "/nodes/2/params/density"}, {"value", density}}}));
}

// The bench scene as groom files: one per strand count, sharing the scalp cache.
struct BenchScene {
    std::string cache;
    std::map<int, std::string> grooms;  // by strand count
    double area = 0.0;
    std::size_t guides = 0, triangles = 0;
};

BenchScene makeScene(const fs::path& dir, const std::vector<int>& counts) {
    BenchScene scene;
    const std::shared_ptr<ScalpMesh> head = makeHead();
    scene.triangles = head->triangleCount();
    scene.area = head->totalArea();
    scene.cache = (dir / "bench.scalp.abc").u8string();
    {
        AlembicMeshWriter writer(scene.cache, "head", 1.0 / kFps, 1.0 / kFps);
        for (int f = 1; f <= kFrames; ++f) writer.writeFrame(head->withPoints(bentPoints(*head, f)));
        writer.close();
    }
    // The guides: the first vertex of every 1.6 cm cell, straight along the smooth normal.
    GuideHierarchy hierarchy;
    const GroupId group = hierarchy.addGroup("primary");
    std::map<std::array<int, 3>, std::size_t> cells;
    for (std::size_t i = 0; i < head->pointCount(); ++i) {
        const Vec3f& p = head->points()[i];
        const std::array<int, 3> key{static_cast<int>(std::floor(p.x / kGuideSpacing)), static_cast<int>(std::floor(p.y / kGuideSpacing)),
                                     static_cast<int>(std::floor(p.z / kGuideSpacing))};
        cells.emplace(key, i);
    }
    std::vector<std::size_t> picked;
    for (const auto& [key, index] : cells) picked.push_back(index);
    std::sort(picked.begin(), picked.end());
    for (const std::size_t index : picked) {
        const Vec3f p = head->points()[index];
        const SurfacePoint sp = head->closestPoint(p);
        const Vec3f n = head->normal(sp.root);
        std::vector<Vec3f> pts;
        for (int k = 0; k < 4; ++k) pts.push_back(p + n * (kHairLength * static_cast<float>(k) / 3.0f));
        hierarchy.addGuide(group, pts, {});
    }
    hierarchy.evaluate(head.get());
    scene.guides = hierarchy.guides().size();
    auto rest = std::make_shared<ScalpMesh>(*head);
    rest->setRestPoints(head->points());
    for (const int count : counts) {
        GroomFile file;
        file.data.scalp = rest;
        file.data.graphJson = benchGraph(static_cast<double>(count) / scene.area).dump();
        file.data.hierarchyJson = hierarchy.toJson();
        file.scalpCache.file = scene.cache;
        file.scalpCache.object = "headShape";
        file.framesPerSecond = kFps;
        const std::string path = (dir / ("bench_" + std::to_string(count) + ".groom")).u8string();
        writeGroomFile(path, file);
        scene.grooms[count] = path;
    }
    return scene;
}

// The bench scene kept for screenshots and scripted runs: GROOMLAB_APP_BENCH_SCENE_DIR names
// the folder; bench_100000.groom with its cache, and bench_cards.groom (20k strands with a
// Cards node of 2 cards a clump before the output) for Cards mode.
TEST(AppBench, WritesTheSceneForScreenshots) {
    const char* out = std::getenv("GROOMLAB_APP_BENCH_SCENE_DIR");
    if (!out || !*out) GTEST_SKIP() << "set GROOMLAB_APP_BENCH_SCENE_DIR=<folder> to write the bench scene";
    const fs::path dir = fs::u8path(out);
    fs::create_directories(dir);
    const BenchScene scene = makeScene(dir, {100000, 20000});
    GroomFile file = readGroomFile(scene.grooms.at(20000));
    json graph = json::parse(file.data.graphJson);
    graph["nodes"].push_back({{"name", "cards"}, {"type", "Cards"}, {"params", {{"cardsPerClump", 2}, {"variations", 16}, {"atlasSize", 1024}}}});
    for (json& c : graph["connections"]) {
        if (c["to"] == "output") c["to"] = "cards";
    }
    graph["connections"].push_back({{"from", "cards"}, {"to", "output"}, {"input", "strands"}});
    file.data.graphJson = graph.dump();
    writeGroomFile((dir / "bench_cards.groom").u8string(), file);
    std::error_code ec;
    fs::remove(fs::u8path(scene.grooms.at(20000)), ec);
    // The hair covers the head: the strands' bounds reach past the scalp's by about the hair
    // length on every side (not a tuft somewhere).
    auto doc = GroomDocument::open((dir / "bench_cards.groom").u8string());
    ViewRequest request;
    const ViewResult& view = doc->evaluate(request);
    ASSERT_TRUE(view.strands) << (view.errors.empty() ? "" : view.errors.front());
    const Bounds hair = view.bounds;
    Bounds scalp;
    for (const Vec3f& p : doc->file().data.scalp->points()) {
        if (!scalp.valid) {
            scalp.lo = scalp.hi = p;
            scalp.valid = true;
        }
        scalp.lo = {std::min(scalp.lo.x, p.x), std::min(scalp.lo.y, p.y), std::min(scalp.lo.z, p.z)};
        scalp.hi = {std::max(scalp.hi.x, p.x), std::max(scalp.hi.y, p.y), std::max(scalp.hi.z, p.z)};
    }
    std::printf("scalp %.1f %.1f %.1f .. %.1f %.1f %.1f, hair %.1f %.1f %.1f .. %.1f %.1f %.1f, %zu guides\n", scalp.lo.x, scalp.lo.y,
                scalp.lo.z, scalp.hi.x, scalp.hi.y, scalp.hi.z, hair.lo.x, hair.lo.y, hair.lo.z, hair.hi.x, hair.hi.y, hair.hi.z,
                scene.guides);
    EXPECT_LT(hair.lo.x, scalp.lo.x - 8.0f);
    EXPECT_GT(hair.hi.x, scalp.hi.x + 8.0f);
    EXPECT_LT(hair.lo.z, scalp.lo.z - 8.0f);
    EXPECT_GT(hair.hi.z, scalp.hi.z + 8.0f);
    EXPECT_GT(hair.hi.y, scalp.hi.y + 8.0f);
}

double fileMb(const std::string& path) {
    std::error_code ec;
    return static_cast<double>(fs::file_size(fs::u8path(path), ec)) / 1e6;
}

TEST(AppBench, ExportsVsMaya) {
    const char* out = std::getenv("GROOMLAB_APP_BENCH");
    if (!out || !*out) GTEST_SKIP() << "set GROOMLAB_APP_BENCH=<results.json> to run the benchmark";
    std::vector<int> counts{100000, 500000};
    if (const char* only = std::getenv("GROOMLAB_APP_BENCH_COUNTS")) {
        counts.clear();
        for (const std::string& word : [&] {
                 std::vector<std::string> words;
                 std::string s(only), w;
                 for (const char c : s) {
                     if (c == ',') {
                         words.push_back(w);
                         w.clear();
                     } else {
                         w += c;
                     }
                 }
                 words.push_back(w);
                 return words;
             }()) {
            if (!word.empty()) counts.push_back(std::atoi(word.c_str()));
        }
    }
    const fs::path dir = fs::temp_directory_path() / "groomlab_app_bench";
    fs::create_directories(dir);
    const BenchScene scene = makeScene(dir, counts);
    json results;
    results["scene"] = {{"triangles", scene.triangles}, {"area_cm2", scene.area}, {"guides", scene.guides}, {"frames", kFrames},
                        {"play_frames", kPlayFrames}, {"fps", kFps}};
    std::unique_ptr<QRhi> rhi = createOffscreenRhi();
    results["draw_api"] = rhi ? rhi->backendName() : "none";

    for (const int count : counts) {
        json r;
        const std::string path = scene.grooms.at(count);
        for (const bool follow : {false, true}) {
            const std::string variant = follow ? "groomlab_follow" : "groomlab";
            json v;
            // Creation, cold: the file read, its caches opened, the first evaluation at density 1.
            double t = now();
            auto doc = GroomDocument::open(path);
            doc->setFollowScalp(follow);
            ViewRequest request;
            request.density = 1.0f;
            const ViewResult* view = &doc->evaluate(request);
            ASSERT_TRUE(view->strands) << (view->errors.empty() ? "" : view->errors.front());
            v["creation_cold_s"] = now() - t;
            v["strands"] = view->strands->strandCount();
            v["points"] = view->strands->pointCount();
            if (!follow) {
                // Tweaks to evaluated: 7 each, the median.
                std::vector<double> clump, noise;
                for (int k = 0; k < 7; ++k) {
                    t = now();
                    doc->setParam("clump", "strength", 0.6 + 0.05 * k);
                    doc->evaluate(request);
                    clump.push_back((now() - t) * 1e3);
                    t = now();
                    doc->setParam("noise", "amplitude", 0.4 + 0.05 * k);
                    doc->evaluate(request);
                    noise.push_back((now() - t) * 1e3);
                }
                doc->setParam("clump", "strength", 0.8);
                doc->setParam("noise", "amplitude", 0.5);
                doc->evaluate(request);
                v["clump_tweak_ms"] = median(clump);
                v["noise_tweak_ms"] = median(noise);
            }
            // Per-frame evaluation over the playback frames (the first apart, as the Maya worker).
            std::vector<double> frameMs;
            for (int f = 1; f <= kPlayFrames; ++f) {
                t = now();
                doc->setFrame(f);
                view = &doc->evaluate(request);
                frameMs.push_back((now() - t) * 1e3);
            }
            ASSERT_TRUE(view->strands);
            frameMs.erase(frameMs.begin());
            v["frame_eval_ms"] = median(frameMs);
            v["eval_fps"] = frameMs.size() / (std::accumulate(frameMs.begin(), frameMs.end(), 0.0) / 1e3);
            // Alembic (and USD) of 24 frames, full density.
            doc->setFrame(1);
            ExportOptions options;
            options.frames.first = 1.0;
            options.frames.last = 24.0;
            const std::string abc = (dir / ("anim_" + std::to_string(count) + ".abc")).u8string();
            t = now();
            doc->exportAlembic(abc, options);
            v["alembic_export_s"] = now() - t;
            v["alembic_mb"] = fileMb(abc);
            fs::remove(fs::u8path(abc));
            const std::string usda = (dir / ("anim_" + std::to_string(count) + ".usda")).u8string();
            t = now();
            doc->exportUsd(usda, options);
            v["usd_export_s"] = now() - t;
            v["usd_mb"] = fileMb(usda);
            fs::remove(fs::u8path(usda));
            // Headless playback: each frame evaluated, its display built, drawn offscreen and
            // waited for (the lookdev path on the device, the line path without one).
            if (rhi) {
                GroomRenderer renderer;
                renderer.setScalp(doc->file().data.scalp);
                renderer.setHairLook(doc->session().hairLook());
                DisplaySettings settings;
                OrbitCamera camera;
                camera.frame(doc->sceneBounds());
                const QSize size(1280, 720);
                std::vector<double> playMs;
                for (int f = 1; f <= kPlayFrames; ++f) {
                    t = now();
                    doc->setFrame(f);
                    view = &doc->evaluate(request);
                    renderer.setScalp(view->scalp);
                    renderer.setHairStrands(view->strands, view->versions.strands);
                    renderer.setFrame(doc->updateDisplay(settings));
                    renderOffscreen(renderer, camera, size, 1, rhi.get());
                    playMs.push_back((now() - t) * 1e3);
                }
                playMs.erase(playMs.begin());
                v["play_fps"] = playMs.size() / (std::accumulate(playMs.begin(), playMs.end(), 0.0) / 1e3);
                v["play_frame_ms"] = median(playMs);
                renderer.releaseResources();
            }
            r[variant] = v;
        }
        results["counts"][std::to_string(count)] = r;
    }
    std::ofstream file(out);
    file << results.dump(2);
    std::error_code ec;
    fs::remove_all(dir, ec);
}

}  // namespace
}  // namespace groomlab::app
