// SPDX-License-Identifier: Apache-2.0
// Simulate, bake to a cache, render: a groom file's scene steps its Simulate node over the scalp
// cache's frames (its feed), a bake written from the session reads back as the same frames, and a
// groom file naming the bake renders them without simulating (unless the node's settings changed).

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "../core/test_helpers.h"
#include "groomlab/alembic_io.h"
#include "groomlab/groom_file.h"
#include "groomlab/groom_scene.h"
#include "groomlab/guide_hierarchy.h"
#include "groomlab/sim_cache.h"

namespace groomlab {
namespace {

// A temporary file of the running test's own: ctest runs each test in a process of its own, side by
// side, and several build a Scene (one test removing the cache another is reading).
std::string tempPath(const char* name) {
    const std::string test = ::testing::UnitTest::GetInstance()->current_test_info()->name();
    return (std::filesystem::temp_directory_path() / (test + "_" + name)).string();
}

const char* kGraph = R"({"version": 1, "nodes": [
    {"name": "scalp", "type": "Scalp"},
    {"name": "guides", "type": "Guides"},
    {"name": "sim", "type": "Simulate", "params": {"gravity": 9.8, "collisionOffset": 0.02, "damping": 0.2}},
    {"name": "scatter", "type": "Scatter", "params": {"density": 200.0}},
    {"name": "interpolate", "type": "Interpolate"},
    {"name": "output", "type": "Output"}],
  "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"},
                  {"from": "scatter", "to": "interpolate", "input": "strands"},
                  {"from": "guides", "to": "sim", "input": "guides"},
                  {"from": "sim", "to": "interpolate", "input": "guides"},
                  {"from": "scalp", "to": "interpolate", "input": "scalp"},
                  {"from": "interpolate", "to": "output", "input": "strands"}], "output": "output"})";

// A plane scalp sliding +x over frames 2-8 and still after, cached at 24 fps; guides standing on it.
struct Scene {
    MeshPtr plane = test::makePlane(4);
    std::string cache = tempPath("groomlab_sim_scalp.abc");
    Scene() {
        AlembicMeshWriter writer(cache, "head", 0.0, 1.0 / 24.0);
        for (int f = 0; f <= 30; ++f) {
            std::vector<Vec3f> points = plane->points();
            for (Vec3f& p : points) p.x += 0.1f * static_cast<float>(std::clamp(f - 2, 0, 6));
            writer.writeFrame(plane->withPoints(std::move(points)));
        }
    }
    ~Scene() {
        std::error_code ec;
        std::filesystem::remove(cache, ec);
    }
    GroomFile file() const {
        GuideHierarchy h;
        const GroupId group = h.addGroup("primary");
        for (int i = 0; i < 6; ++i) {
            const Vec3f root{0.15f + 0.14f * static_cast<float>(i), 0.0f, 0.5f};
            h.addGuide(group, test::straightCurve(root, {0.1f, 0.6f, 0.0f}, 8));
        }
        h.evaluate(plane.get());
        GroomFile f;
        f.data.scalp = plane;
        f.data.graphJson = kGraph;
        f.data.hierarchyJson = h.toJson();
        f.scalpCache = {cache, "head"};
        return f;
    }
};

TEST(SimulationCache, TheSceneSimulatesOverTheScalpCacheAndABakeRendersTheSameFrames) {
    Scene scene;
    GroomScene played(scene.file());
    // Simulate Range over the cache's frames: the scene's feed gives each frame's scalp.
    played.setTime(1 / 24.0);
    const SimRun run = played.session().simulateTo(24);
    EXPECT_TRUE(run.errors.empty());
    EXPECT_EQ(run.frames, 24);
    // A fresh scene jumping straight to frame 20 steps the same frames on the way.
    GroomScene jumped(scene.file());
    EXPECT_EQ(jumped.evaluate(20 / 24.0)->positions(), played.evaluate(20 / 24.0)->positions());
    // The motion reached the guides: the simulated frame differs from the guides carried rigidly.
    GroomFile still = scene.file();
    std::string bypassed = kGraph;
    bypassed.replace(bypassed.find(R"("type": "Simulate",)"), 19, R"("type": "Simulate", "bypass": true,)");
    still.data.graphJson = bypassed;
    EXPECT_NE(GroomScene(still).evaluate(6 / 24.0)->positions(), played.evaluate(6 / 24.0)->positions());

    // Bake: the frames as an Alembic guide cache, named in a groom file with the settings' stamp.
    const std::string bake = tempPath("groomlab_sim_bake.abc");
    EXPECT_EQ(writeSimulationCache(played.session(), "sim", bake, 1, 24), 24u);
    GroomFile baked = scene.file();
    baked.simCaches["sim"] = {bake, "", played.session().simulationStamp("sim")};
    const std::string groom = tempPath("groomlab_sim_baked.groom");
    writeGroomFile(groom, baked);
    {
        GroomScene render(groom);
        ASSERT_EQ(render.file().simCaches.size(), 1u);
        EXPECT_EQ(render.file().simCaches.at("sim").stamp, played.session().simulationStamp("sim"));
        for (int f : {1, 7, 12, 24}) {
            const StrandsPtr a = render.evaluate(f / 24.0), b = played.evaluate(f / 24.0);
            ASSERT_EQ(a->pointCount(), b->pointCount());
            float worst = 0.0f;
            for (std::size_t i = 0; i < a->pointCount(); ++i) worst = std::max(worst, (a->positions()[i] - b->positions()[i]).length());
            EXPECT_LT(worst, 1e-5f) << "frame " << f;
        }
        const std::vector<SimStatus> status = render.session().simulationStatus();
        ASSERT_EQ(status.size(), 1u);
        EXPECT_TRUE(status[0].baked);
        EXPECT_EQ(status[0].bakedFirst, 1);
        EXPECT_EQ(status[0].bakedLast, 24);
        EXPECT_EQ(status[0].frames, 0u) << "the render simulated rather than read the bake";
    }
    {
        // Other settings since the bake: it is stale, ignored, and the node simulates.
        GroomFile changed = baked;
        changed.simCaches["sim"].stamp ^= 1;
        GroomScene render(std::move(changed));
        render.evaluate(4 / 24.0);
        const SimStatus status = render.session().simulationStatus()[0];
        EXPECT_FALSE(status.baked);
        EXPECT_TRUE(status.bakeStale);
        EXPECT_EQ(status.frames, 4u);
    }
    // A frame not simulated is not baked.
    EXPECT_THROW(writeSimulationCache(played.session(), "sim", bake, 1, 40), std::runtime_error);
    std::error_code ec;
    std::filesystem::remove(bake, ec);
    std::filesystem::remove(groom, ec);
}

TEST(SimulationCache, AWetSimulationBakesItsWetness) {
    // The guides under water: the bake carries each point's wetness, and the render reads it back
    // as the strands' wetness channel.
    Scene scene;
    GroomFile wet = scene.file();
    std::string graph = kGraph;
    const std::string params = R"("damping": 0.2})";
    graph.replace(graph.find(params), params.size(), R"("damping": 0.2, "water": true, "waterLevel": 0.3})");
    wet.data.graphJson = graph;
    GroomScene played(wet);
    played.setTime(1 / 24.0);
    played.session().simulateTo(12);
    const std::string bake = tempPath("groomlab_sim_wet_bake.abc");
    writeSimulationCache(played.session(), "sim", bake, 1, 12);
    wet.simCaches["sim"] = {bake, "", played.session().simulationStamp("sim")};
    {
        GroomScene render(wet);
        const StrandsPtr a = render.evaluate(12 / 24.0), b = played.evaluate(12 / 24.0);
        EXPECT_TRUE(render.session().simulationStatus()[0].baked);
        const std::vector<float>* baked = a->pointChannel(kWetnessChannel);
        const std::vector<float>* live = b->pointChannel(kWetnessChannel);
        ASSERT_TRUE(baked && live);
        ASSERT_EQ(baked->size(), live->size());
        float worst = 0.0f, most = 0.0f;
        for (std::size_t i = 0; i < baked->size(); ++i) {
            worst = std::max(worst, std::abs((*baked)[i] - (*live)[i]));
            most = std::max(most, (*live)[i]);
        }
        EXPECT_LT(worst, 1e-6f);
        EXPECT_GT(most, 0.5f) << "nothing got wet: the test shows nothing";
    }
    std::error_code ec;
    std::filesystem::remove(bake, ec);
}

TEST(SimulationCache, GroomFilesKeepTheBakesTheyName) {
    GroomFile file;
    file.data.scalp = test::makePlane(2);
    file.simCaches["hair"] = {"hair_sim/hair.abc", "hair", 0x0123456789ABCDEFull};
    file.simCaches["beard"] = {"beard.abc", "", 7};
    const GroomFile back = groomFileFromJson(groomFileToJson(file), "C:/shows/abc");
    ASSERT_EQ(back.simCaches.size(), 2u);
    EXPECT_EQ(back.simCaches.at("hair").stamp, 0x0123456789ABCDEFull);
    EXPECT_EQ(back.simCaches.at("hair").object, "hair");
    EXPECT_EQ(std::filesystem::path(back.simCaches.at("beard").file), std::filesystem::path("C:/shows/abc/beard.abc"));
    EXPECT_EQ(formatSimulationStamp(7), "0000000000000007");
    EXPECT_EQ(parseSimulationStamp("not hex"), 0u);
}

}  // namespace
}  // namespace groomlab
