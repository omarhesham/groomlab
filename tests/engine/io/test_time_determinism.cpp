// SPDX-License-Identifier: Apache-2.0
// Determinism over time: a frame is the same however it is reached. The shipped presets whose
// nodes read time (Wind, Jiggle, Simulate) and a groom following the scalp, on the reference
// groom (tests/data/strokes) over a scalp cache of a moving head, through GroomScene (the CLI's
// and the procedural's host): frame N evaluated cold (a fresh scene, a render) equals frame N
// played to from frame 1 and scrubbed back to, bit for bit. A Simulate node's baked frames read
// back as the frames it simulated.

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "groomlab/alembic_io.h"
#include "groomlab/groom_file.h"
#include "groomlab/groom_scene.h"
#include "groomlab/presets.h"
#include "groomlab/sim_cache.h"

namespace groomlab {
namespace {

namespace fs = std::filesystem;

const std::string kShipped = GROOMLAB_SHIPPED_PRESETS;
constexpr float kDensity = 0.05f;
constexpr int kFrames = 12;

GroomFile referenceGroom() { return readGroomFile(std::string(GROOMLAB_TEST_DATA) + "/strokes/strokes.groom"); }

std::string presetGraph(const std::string& name, const ScalpMesh& scalp) {
    std::ifstream in(fs::u8path(kShipped) / (name + ".json"), std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return presetGraphJson(text.str(), presetScalpSize(scalp));
}

// The reference scalp as a head turning and nodding over frames 0-30 (24 fps), cached.
struct MovingHead {
    // The running test's own file: ctest runs the tests side by side, each in a process of its own.
    std::string cache = (fs::temp_directory_path() /
                         (std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()) +
                          "_groomlab_time_determinism_head.abc")).u8string();
    explicit MovingHead(const ScalpMesh& scalp) {
        AlembicMeshWriter writer(cache, "head", 0.0, 1.0 / 24.0);
        for (int f = 0; f <= 30; ++f) {
            const float turn = 0.6f * std::sin(0.3f * static_cast<float>(f));
            const float c = std::cos(turn), s = std::sin(turn);
            std::vector<Vec3f> points = scalp.points();
            for (Vec3f& p : points) {
                p = Vec3f(c * p.x + s * p.z, p.y + 0.05f * static_cast<float>(f % 7), -s * p.x + c * p.z);
            }
            writer.writeFrame(scalp.withPoints(std::move(points)));
        }
    }
    ~MovingHead() {
        std::error_code ec;
        fs::remove(cache, ec);
    }
};

GroomFile sceneFile(const GroomFile& reference, const std::string& graph, const std::string& cache, bool follow) {
    GroomFile file = reference;
    file.data.graphJson = graph;
    file.scalpCache = {cache, "head"};
    file.searchPaths = {reference.directory, kShipped};
    if (follow) {
        auto rest = std::make_shared<ScalpMesh>(*reference.data.scalp);
        rest->setRestPoints(reference.data.scalp->points());
        file.data.scalp = rest;
        file.data.followScalp = true;
    }
    return file;
}

std::uint64_t at(GroomScene& scene, int frame) {
    const StrandsPtr strands = scene.evaluate(frame / scene.framesPerSecond(), kDensity);
    return strands ? strands->contentHash() : 0;
}

}  // namespace

TEST(TimeDeterminism, AFrameIsTheSameColdPlayedToOrScrubbedBackTo) {
    const GroomFile reference = referenceGroom();
    ASSERT_TRUE(reference.data.scalp);
    const MovingHead head(*reference.data.scalp);
    struct Case {
        std::string preset;
        bool follow;
    };
    for (const Case& c : {Case{"wind_and_jiggle", false}, Case{"everyday_sim", false},
                          Case{"hair_basics", true}, Case{"wind_and_jiggle", true}}) {
        SCOPED_TRACE(c.preset + (c.follow ? " following the scalp" : ""));
        const GroomFile file = sceneFile(reference, presetGraph(c.preset, *reference.data.scalp), head.cache, c.follow);
        GroomScene played(file);
        ASSERT_TRUE(played.readsTime());
        std::map<int, std::uint64_t> want;
        for (int f = 1; f <= kFrames; ++f) want[f] = at(played, f);
        EXPECT_NE(want[2], want[kFrames]) << "nothing moved: the test checks nothing";
        // Cold: a fresh scene at the frame, as a render evaluates it.
        for (const int f : {1, 5, kFrames}) {
            GroomScene cold(file);
            EXPECT_EQ(at(cold, f), want[f]) << "frame " << f << " cold";
        }
        // Scrubbed: the played scene back over its frames, and a fresh one jumping about.
        for (int f = kFrames; f >= 1; --f) EXPECT_EQ(at(played, f), want[f]) << "frame " << f << " scrubbed back";
        GroomScene jumping(file);
        for (const int f : {9, 3, kFrames, 6, 1, 7}) EXPECT_EQ(at(jumping, f), want[f]) << "frame " << f << " jumped to";
    }
}

TEST(TimeDeterminism, ABakedSimulationRendersTheFramesItSimulated) {
    // The contract: a Simulate node's bake (Alembic, float points) is its simulated guides; the
    // strands from a bake read back are the strands of the live simulation, bit for bit.
    const GroomFile reference = referenceGroom();
    const MovingHead head(*reference.data.scalp);
    const GroomFile file = sceneFile(reference, presetGraph("everyday_sim", *reference.data.scalp), head.cache, false);
    GroomScene played(file);
    played.setTime(1 / 24.0);
    const SimRun run = played.session().simulateTo(kFrames);
    ASSERT_TRUE(run.errors.empty());
    const std::string bake = (fs::temp_directory_path() / "groomlab_time_determinism_bake.abc").u8string();
    ASSERT_EQ(writeSimulationCache(played.session(), "simulate", bake, 1, kFrames), static_cast<std::size_t>(kFrames));
    GroomFile baked = file;
    baked.simCaches["simulate"] = {bake, "", played.session().simulationStamp("simulate")};
    {
        GroomScene render(baked);
        for (const int f : {1, 4, kFrames}) EXPECT_EQ(at(render, f), at(played, f)) << "frame " << f;
        EXPECT_EQ(render.session().simulationStatus()[0].frames, 0u) << "the render simulated rather than read the bake";
    }
    std::error_code ec;
    fs::remove(bake, ec);
}

}  // namespace groomlab
