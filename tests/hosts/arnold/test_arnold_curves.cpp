// SPDX-License-Identifier: Apache-2.0
// arnold_curves.h against Arnold itself (no Maya): what the MtoA translator and the procedural hand
// Arnold. A tester's Maya went down inside Arnold (ai.dll, on a render thread) while an interactive
// render updated a million-strand groom after Scatter length and Width edits: these cases pin what
// an update may hand Arnold (counts that agree, no negative or NaN radius, no strand Arnold cannot
// take, nothing left over from the last export) and that an update is never made while Arnold
// renders.

#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <thread>

#include <gtest/gtest.h>

#include "groomlab/arnold_curves.h"

namespace {

using namespace groomlab;

// A strand of `n` points rising from (x, 0, z), `width` wide.
std::vector<Vec3f> strandPoints(float x, float z, std::size_t n) {
    std::vector<Vec3f> p;
    for (std::size_t k = 0; k < n; ++k) p.push_back({x, 0.1f * static_cast<float>(k), z});
    return p;
}

StrandSet grid(std::size_t side, std::size_t points, float width) {
    StrandSet s;
    s.reserve(side * side, side * side * points);
    for (std::size_t i = 0; i < side; ++i) {
        for (std::size_t j = 0; j < side; ++j) {
            const auto x = static_cast<float>(i) / static_cast<float>(side) - 0.5f;
            const auto z = static_cast<float>(j) / static_cast<float>(side) - 0.5f;
            s.addStrand(static_cast<StrandId>(i * side + j + 1), strandPoints(x, z, points),
                        std::vector<float>(points, width));
        }
    }
    return s;
}

class ArnoldCurves : public ::testing::Test {
protected:
    void SetUp() override {
        AiBegin(AI_SESSION_INTERACTIVE);
        AiMsgSetConsoleFlags(nullptr, AI_LOG_ERRORS);
        universe = AiUniverse();
        curves = AiNode(universe, AtString("curves"), AtString("strands"));
    }
    void TearDown() override {
        AiUniverseDestroy(universe);
        AiEnd();
    }
    AtUniverse* universe = nullptr;
    AtNode* curves = nullptr;
};

TEST_F(ArnoldCurves, RadiiAreNeverNegativeOrNaN) {
    // A negative Width (a typed value, a map, an expression) or render width scale: radius 0, not
    // inverted bounds.
    StrandSet s;
    s.addStrand(1, strandPoints(0, 0, 3), {0.02f, -0.1f, std::numeric_limits<float>::quiet_NaN()});
    s.addStrand(2, strandPoints(1, 0, 3), {0.01f, 0.01f, 0.01f});
    const ExportView view = describeExport(s, nullptr);
    arnold::setCurves(curves, view, 1, -2.0f);
    const AtArray* radius = AiNodeGetArray(curves, AtString("radius"));
    ASSERT_EQ(AiArrayGetNumElements(radius), 6u);
    for (uint32_t i = 0; i < 6; ++i) EXPECT_EQ(AiArrayGetFlt(radius, i), 0.0f) << i;
    arnold::setCurves(curves, view, 1, 1.0f);
    radius = AiNodeGetArray(curves, AtString("radius"));
    EXPECT_FLOAT_EQ(AiArrayGetFlt(radius, 0), 0.01f);
    EXPECT_EQ(AiArrayGetFlt(radius, 1), 0.0f);
    EXPECT_EQ(AiArrayGetFlt(radius, 2), 0.0f);
    EXPECT_FLOAT_EQ(AiArrayGetFlt(radius, 3), 0.005f);
}

TEST_F(ArnoldCurves, StrandsArnoldCannotTakeAreLeftOut) {
    StrandSet s;
    s.addStrand(1, strandPoints(0, 0, 4));
    s.addStrand(2, strandPoints(1, 0, 1));  // one point: no segment
    std::vector<Vec3f> bad = strandPoints(2, 0, 4);
    bad[2].y = std::numeric_limits<float>::quiet_NaN();
    s.addStrand(3, bad);
    s.addStrand(4, strandPoints(3, 0, 2));
    std::size_t dropped = 0;
    const std::optional<LodPlan> plan = arnold::curvesPlan(s, nullptr, &dropped);
    ASSERT_TRUE(plan);
    EXPECT_EQ(dropped, 2u);
    EXPECT_EQ(plan->kept, (std::vector<std::size_t>{0, 3}));
    EXPECT_EQ(plan->widthScale, (std::vector<float>{1.0f, 1.0f}));

    // Over an LOD's plan: its kept strands less the bad ones, each with its width scale.
    LodPlan lod;
    lod.kept = {1, 2, 3};
    lod.widthScale = {2.0f, 3.0f, 4.0f};
    lod.thinned = 1;
    const std::optional<LodPlan> both = arnold::curvesPlan(s, &lod, &dropped);
    ASSERT_TRUE(both);
    EXPECT_EQ(dropped, 2u);
    EXPECT_EQ(both->kept, (std::vector<std::size_t>{3}));
    EXPECT_EQ(both->widthScale, (std::vector<float>{4.0f}));
    EXPECT_EQ(both->thinned, 1u);

    // Nothing to leave out: no plan of its own.
    const StrandSet good = grid(3, 4, 0.01f);
    EXPECT_FALSE(arnold::curvesPlan(good, nullptr, &dropped));
    EXPECT_EQ(dropped, 0u);

    // The curves made from the plan: counts that agree, every strand of two points or more, and
    // later motion keys filled through the same plan.
    const StrandSet kept = applyLod(s, *plan);
    const ExportView view = describeExport(kept, nullptr);
    arnold::setCurves(curves, view, 2);
    const AtArray* counts = AiNodeGetArray(curves, AtString("num_points"));
    ASSERT_EQ(AiArrayGetNumElements(counts), 2u);
    EXPECT_EQ(AiArrayGetUInt(counts, 0), 4u);
    EXPECT_EQ(AiArrayGetUInt(counts, 1), 2u);
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(curves, AtString("points"))), 6u);
    EXPECT_TRUE(arnold::setPointsKey(curves, s, 1, &*plan));
}

TEST_F(ArnoldCurves, FeathersRenderTheirRachisAsAMeshNotTheirShafts) {
    // Two feathers: each a shaft (isShaft 1) and a barb; with a rachis the shafts stay out of the
    // curves and the rachis renders as a polymesh, each feather in its shaft's color.
    StrandSet s;
    for (int f = 0; f < 2; ++f) {
        s.addStrand(static_cast<StrandId>(10 + f), strandPoints(static_cast<float>(f), 0, 5), std::vector<float>(5, 0.02f));
        s.addStrand(static_cast<StrandId>(20 + f), strandPoints(static_cast<float>(f) + 0.2f, 0, 3), std::vector<float>(3, 0.001f));
    }
    std::vector<float>& isShaft = s.addStrandChannel("isShaft", 0.0f);
    std::vector<float>& featherId = s.addStrandChannel("featherId", 0.0f);
    std::vector<float>& rachisShafts = s.addStrandChannel(kRachisChannel, 0.0f);  // (Propagate with a shaftWidth)
    isShaft[0] = isShaft[2] = 1.0f;
    rachisShafts[0] = rachisShafts[2] = 1.0f;
    featherId[2] = featherId[3] = 1.0f;
    std::vector<float>& red = s.addStrandChannel("colorR", 0.1f);
    s.addStrandChannel("colorG", 0.1f);
    s.addStrandChannel("colorB", 0.1f);
    red[2] = 0.8f;
    // No rachis yet: every strand a curve.
    std::size_t dropped = 0;
    EXPECT_FALSE(arnold::curvesPlan(s, nullptr, &dropped));
    auto layer = std::make_shared<FeatherLayer>();
    for (int shaft : {0, 2}) {
        for (int k = 0; k < 5; ++k) {
            RachisRing ring;
            ring.center = {static_cast<float>(shaft / 2), 0.1f * static_cast<float>(k), 0.0f};
            ring.tangent = {0.0f, 1.0f, 0.0f};
            ring.normal = {0.0f, 0.0f, 1.0f};
            ring.side = ring.tangent.cross(ring.normal);
            ring.radius = 0.01f;
            ring.u = 0.25f * static_cast<float>(k);
            layer->rings.push_back(ring);
        }
        layer->offsets.push_back(layer->rings.size());
        layer->featherIds.push_back(shaft / 2);
    }
    s.addFeatherLayer(layer);
    const std::optional<LodPlan> plan = arnold::curvesPlan(s, nullptr, &dropped);
    ASSERT_TRUE(plan);
    EXPECT_EQ(dropped, 0u) << "the shafts are not unrenderable";
    EXPECT_EQ(plan->kept, (std::vector<std::size_t>{1, 3}));  // the barbs
    const RachisMesh mesh = buildRachisMesh(s.featherLayers());
    AtNode* polymesh = AiNode(universe, AtString("polymesh"), AtString("rachis"));
    arnold::setRachisMesh(polymesh, mesh, rachisColors(s, mesh, {0.5f, 0.5f, 0.5f}), 2);
    const AtArray* vlist = AiNodeGetArray(polymesh, AtString("vlist"));
    EXPECT_EQ(AiArrayGetNumElements(vlist), mesh.points.size());
    EXPECT_EQ(AiArrayGetNumKeys(vlist), 2u);
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(polymesh, AtString("nsides"))), mesh.faceCount());
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(polymesh, AtString("vidxs"))), mesh.faceVertices.size());
    const AtArray* colors = AiNodeGetArray(polymesh, AtString("rachisColor"));
    ASSERT_TRUE(colors);
    bool redSeen = false;
    for (uint32_t i = 0; i < AiArrayGetNumElements(colors); ++i) redSeen = redSeen || AiArrayGetRGB(colors, i).r == 0.8f;
    EXPECT_TRUE(redSeen) << "feather 1 in its shaft's color";
    EXPECT_TRUE(arnold::setRachisMeshKey(polymesh, mesh, mesh, 1));
    RachisMesh other = mesh;
    other.points.pop_back();
    EXPECT_FALSE(arnold::setRachisMeshKey(polymesh, mesh, other, 1)) << "another mesh: the key stays";
}

TEST_F(ArnoldCurves, AReExportLeavesNothingOfTheLastBehind) {
    // An interactive update exports onto the same node: a strand channel the graph no longer writes,
    // or root UVs the scalp no longer has, must not stay behind sized for the strands before.
    StrandSet before = grid(4, 3, 0.01f);
    before.addStrandChannel("clumpId", 1.0f);
    before.addStrandChannel("melanin", 0.5f);
    ExportView view = describeExport(before, nullptr);
    view.rootUVs.assign(view.strandCount(), {0.5f, 0.5f});
    std::vector<std::string> declared;
    arnold::setCurves(curves, view, 1);
    arnold::setStrandData(curves, view, nullptr, &declared);
    EXPECT_TRUE(AiNodeLookUpUserParameter(curves, AtString("clumpId")));
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(curves, AtString("uvs"))), 16u);
    EXPECT_NE(std::find(declared.begin(), declared.end(), "clumpId"), declared.end());

    StrandSet after = grid(3, 3, 0.01f);  // fewer strands, no clumps
    after.addStrandChannel("melanin", 0.2f);
    view = describeExport(after, nullptr);
    arnold::setCurves(curves, view, 1);
    arnold::setStrandData(curves, view, nullptr, &declared);
    EXPECT_FALSE(AiNodeLookUpUserParameter(curves, AtString("clumpId")));
    EXPECT_EQ(std::find(declared.begin(), declared.end(), "clumpId"), declared.end());
    const AtArray* uvs = AiNodeGetArray(curves, AtString("uvs"));
    EXPECT_TRUE(!uvs || AiArrayGetNumElements(uvs) == 0);
    // What stays is sized for the strands now.
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(curves, AtString("melanin"))), 9u);
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(curves, AtString("strand_id"))), 9u);
    EXPECT_EQ(AiArrayGetNumElements(AiNodeGetArray(curves, AtString("num_points"))), 9u);
}

TEST_F(ArnoldCurves, NoRenderNothingToInterrupt) {
    EXPECT_FALSE(arnold::interruptRenderForEdit(universe));  // no render session
    AtRenderSession* session = AiRenderSession(universe, AI_SESSION_INTERACTIVE);
    EXPECT_FALSE(arnold::interruptRenderForEdit(universe));  // one that has not started
    AiRenderSessionDestroy(session);
}

TEST_F(ArnoldCurves, AnUpdateDuringARenderPausesItFirst) {
    // The interactive render's update, as a host that does not pause the render would make it: the
    // render is interrupted (and its threads stopped) before the curves' arrays are replaced with
    // others of other counts, then restarted and finished.
    AtNode* options = AiUniverseGetOptions(universe);
    AiNodeSetInt(options, AtString("xres"), 320);
    AiNodeSetInt(options, AtString("yres"), 240);
    AiNodeSetInt(options, AtString("AA_samples"), 8);
    AiNodeSetInt(options, AtString("GI_diffuse_depth"), 2);
    AiNodeSetBool(options, AtString("skip_license_check"), true);
    AtNode* camera = AiNode(universe, AtString("persp_camera"), AtString("camera"));
    AiNodeSetMatrix(camera, AtString("matrix"), AiM4Translation(AtVector(0.0f, 0.5f, 2.0f)));
    AiNodeSetPtr(options, AtString("camera"), camera);
    AiNode(universe, AtString("skydome_light"), AtString("sky"));
    AtNode* filter = AiNode(universe, AtString("gaussian_filter"), AtString("filter"));
    (void)filter;
    AtNode* driver = AiNode(universe, AtString("driver_exr"), AtString("driver"));
    const std::string out = (std::filesystem::temp_directory_path() / "groomlab_arnold_curves_test.exr").string();
    AiNodeSetStr(driver, AtString("filename"), AtString(out.c_str()));
    AtArray* outputs = AiArrayAllocate(1, 1, AI_TYPE_STRING);
    AiArraySetStr(outputs, 0, AtString("RGBA RGBA filter driver"));
    AiNodeSetArray(options, AtString("outputs"), outputs);
    AiNodeSetStr(curves, AtString("mode"), AtString("thick"));

    StrandSet first = grid(300, 8, 0.004f);  // 90k strands
    first.addStrandChannel("clumpId", 1.0f);
    std::vector<std::string> declared;
    ExportView view = describeExport(first, nullptr);
    arnold::setCurves(curves, view, 1);
    arnold::setStrandData(curves, view, nullptr, &declared);

    AtRenderSession* session = AiRenderSession(universe, AI_SESSION_INTERACTIVE);
    ASSERT_EQ(AiRenderBegin(session, AI_RENDER_MODE_CAMERA), AI_SUCCESS);
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    AtRenderStatus status = AiRenderGetStatus(session);
    while (status != AI_RENDER_STATUS_RENDERING && status != AI_RENDER_STATUS_FINISHED &&
           status != AI_RENDER_STATUS_FAILED && std::chrono::steady_clock::now() < until) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        status = AiRenderGetStatus(session);
    }
    if (status != AI_RENDER_STATUS_RENDERING) {
        AiRenderEnd(session);
        AiRenderSessionDestroy(session);
        GTEST_SKIP() << "the render was over (status " << status << ") before it could be interrupted";
    }

    EXPECT_TRUE(arnold::interruptRenderForEdit(universe));
    status = AiRenderGetStatus(session);
    EXPECT_NE(status, AI_RENDER_STATUS_RENDERING);
    EXPECT_FALSE(arnold::interruptRenderForEdit(universe));  // already paused: nothing to do

    // The update: other strands (other counts), a width that went negative, a channel gone.
    StrandSet second = grid(220, 6, -0.01f);
    view = describeExport(second, nullptr);
    arnold::setCurves(curves, view, 1);
    arnold::setStrandData(curves, view, nullptr, &declared);
    EXPECT_FALSE(AiNodeLookUpUserParameter(curves, AtString("clumpId")));

    AiRenderRestart(session);
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    while (AiRenderGetStatus(session) != AI_RENDER_STATUS_FINISHED && AiRenderGetStatus(session) != AI_RENDER_STATUS_FAILED &&
           std::chrono::steady_clock::now() < end) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    EXPECT_EQ(AiRenderGetStatus(session), AI_RENDER_STATUS_FINISHED);
    AiRenderEnd(session);
    AiRenderSessionDestroy(session);
    std::error_code ignored;
    std::filesystem::remove(out, ignored);
}

}  // namespace
