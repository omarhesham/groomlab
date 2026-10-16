// SPDX-License-Identifier: Apache-2.0
// The Reference node (reference_op.cpp): another groom file's result brought in as it is, by a
// relative path, read again when the file changes, its guides too, bound onto this groom's scalp by
// closest point or by UV, thinned with the preview, merged twice, and the errors it gives (no file,
// a file referencing itself).

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>

#include "groomlab/groom.h"
#include "groomlab/groom_file.h"
#include "groomlab/groom_session.h"
#include "groomlab/paths.h"
#include "test_helpers.h"

namespace groomlab {
namespace {

namespace fs = std::filesystem;

// A sphere with UVs (u around, v down), moved by `offset` and scaled by `scale`.
MeshPtr uvSphere(Vec3f offset = {0, 0, 0}, float scale = 1.0f) {
    const int rings = 10, segments = 20;
    const MeshPtr base = test::makeSphere(rings, segments);
    std::vector<Vec3f> points = base->points();
    for (Vec3f& p : points) p = p * scale + offset;
    auto mesh = std::make_shared<ScalpMesh>(std::move(points), base->triangles());
    std::vector<ScalpMesh::UV> uvs;
    for (const ScalpMesh::Triangle& t : mesh->triangles()) {
        for (const std::int32_t v : t) {
            const int r = v / (segments + 1), s = v % (segments + 1);
            uvs.push_back({static_cast<float>(s) / segments, static_cast<float>(r) / rings});
        }
    }
    mesh->setCornerUVs(std::move(uvs));
    return mesh;
}

std::string scatterGraph(double density, const std::string& extra = {}) {
    return R"({"version": 1, "nodes": [
    {"name": "scalp", "type": "Scalp"},
    {"name": "scatter", "type": "Scatter", "params": {"density": )" + std::to_string(density) + R"(, "length": 0.3}},
    {"name": "output", "type": "Output"})" + extra + R"(],
    "connections": [{"from": "scalp", "to": "scatter", "input": "scalp"},
                    {"from": "scatter", "to": "output", "input": "strands"}], "output": "output"})";
}

// A graph whose output is a Reference node's (`bind`), or two of them merged.
std::string referenceGraph(const std::string& path, const std::string& bind, bool twice = false) {
    std::string nodes = R"({"name": "scalp", "type": "Scalp"},
    {"name": "ref", "type": "Reference", "params": {"path": ")" + path + R"(", "bind": ")" + bind + R"("}},
    {"name": "output", "type": "Output"})";
    std::string links = R"({"from": "ref", "to": "output", "input": "strands"})";
    if (twice) {
        nodes += R"(, {"name": "ref2", "type": "Reference", "params": {"path": ")" + path + R"(", "bind": ")" + bind + R"("}},
    {"name": "merge", "type": "Merge"})";
        links = R"({"from": "ref", "to": "merge", "input": "a"}, {"from": "ref2", "to": "merge", "input": "b"},
    {"from": "merge", "to": "output", "input": "strands"})";
    }
    return R"({"version": 1, "nodes": [)" + nodes + R"(], "connections": [)" + links + R"(], "output": "output"})";
}

class ReferenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        dir_ = fs::temp_directory_path() / ("groomlab_reference_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::create_directories(dir_);
    }
    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir_, ec);
    }
    std::string write(const std::string& name, const std::string& graph, const MeshPtr& scalp, GuidesPtr guides = nullptr) {
        GroomFile file;
        file.data.graphJson = graph;
        file.data.scalp = scalp;
        file.data.guides = std::move(guides);
        const std::string path = (dir_ / name).generic_u8string();  // forward slashes: JSON text
        writeGroomFile(path, file);
        return path;
    }
    // The host groom on `scalp` with `graph`, its relative paths resolving in the test's folder.
    std::unique_ptr<Groom> host(const std::string& graph, const MeshPtr& scalp) {
        GroomData data;
        data.graphJson = graph;
        data.scalp = scalp;
        auto groom = std::make_unique<Groom>(data);
        groom->setPathResolver(searchPathResolver({dir_.u8string()}));
        return groom;
    }
    fs::path dir_;
};

TEST_F(ReferenceTest, BringsInTheFilesStrandsWhereItHasThem) {
    const MeshPtr libScalp = uvSphere();
    const std::string path = write("brows.groom", scatterGraph(60.0), libScalp);
    const StrandsPtr lib = Groom(readGroomFile(path).data).evaluate(1.0f, 0.0);
    ASSERT_GT(lib->strandCount(), 100u);
    // By a path relative to the scene's folder (the resolver), on another scalp: as the file has them.
    auto groom = host(referenceGraph("brows.groom", "world"), uvSphere({7, 0, 0}));
    const StrandsPtr out = groom->evaluate(1.0f, 0.0);
    ASSERT_EQ(out->strandCount(), lib->strandCount());
    EXPECT_EQ(out->positions(), lib->positions());
    EXPECT_EQ(out->ids(), lib->ids());
    for (const StrandRoot& r : out->roots()) EXPECT_EQ(r.face, -1);  // not on this scalp
    // The preview thins it as it thins the file's own groom.
    const StrandsPtr preview = groom->evaluate(0.25f, 0.0);
    const StrandsPtr libPreview = Groom(readGroomFile(path).data).evaluate(0.25f, 0.0);
    EXPECT_EQ(preview->strandCount(), libPreview->strandCount());
    EXPECT_LT(preview->strandCount(), out->strandCount());
}

TEST_F(ReferenceTest, ReadsTheFileAgainWhenItChanges) {
    const MeshPtr scalp = uvSphere();
    const std::string path = write("fur.groom", scatterGraph(40.0), scalp);
    auto groom = host(referenceGraph(path, "world"), scalp);
    const std::size_t before = groom->evaluate(1.0f, 0.0)->strandCount();
    EXPECT_EQ(groom->evaluate(1.0f, 0.0)->strandCount(), before);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    write("fur.groom", scatterGraph(120.0), scalp);  // denser: another file
    EXPECT_GT(groom->evaluate(1.0f, 0.0)->strandCount(), 2 * before);
}

TEST_F(ReferenceTest, BindsOntoThisScalpByClosestPointAndByUv) {
    const MeshPtr libScalp = uvSphere();
    const std::string path = write("patch.groom", scatterGraph(30.0), libScalp);
    const StrandsPtr lib = Groom(readGroomFile(path).data).evaluate(1.0f, 0.0);
    // Closest point on the head the library was made on: the same strands, rooted on it.
    const MeshPtr moved = uvSphere({0, 0, 0}, 1.0f);
    {
        auto groom = host(referenceGraph(path, "closestPoint"), moved);
        const StrandsPtr out = groom->evaluate(1.0f, 0.0);
        ASSERT_EQ(out->strandCount(), lib->strandCount());
        for (std::size_t s = 0; s < out->strandCount(); ++s) {
            ASSERT_TRUE(moved->contains(out->roots()[s]));
            for (std::size_t k = 0; k < out->strandPointCount(s); ++k) {
                EXPECT_LT((out->strandPoints(s)[k] - lib->strandPoints(s)[k]).length(), 1e-4f);
            }
        }
    }
    // By UV on a head twice the size elsewhere: each root at its UV there, strands standing on it.
    const MeshPtr big = uvSphere({3, 0, 0}, 2.0f);
    auto groom = host(referenceGraph(path, "uv"), big);
    const StrandsPtr out = groom->evaluate(1.0f, 0.0);
    ASSERT_GT(out->strandCount(), lib->strandCount() * 9 / 10);
    for (std::size_t s = 0; s < out->strandCount(); ++s) {
        const StrandRoot& root = out->roots()[s];
        ASSERT_TRUE(big->contains(root));
        const Vec3f p0 = out->strandPoints(s)[0];
        EXPECT_LT((p0 - big->position(root)).length(), 1e-4f);
        EXPECT_NEAR((p0 - Vec3f(3, 0, 0)).length(), 2.0f, 0.05f);
        const std::size_t n = out->strandPointCount(s);
        const Vec3f along = (out->strandPoints(s)[n - 1] - p0).normalized();
        EXPECT_GT(along.dot(big->normal(root)), 0.95f);  // standing out of the surface as on the file's
    }
}

TEST_F(ReferenceTest, GuidesOutputAndTwoReferencesMerged) {
    const MeshPtr scalp = uvSphere();
    auto guides = std::make_shared<GuideSet>();
    guides->add(11, test::straightCurve({0, 1, 0}, {0, 1, 0}));
    guides->add(12, test::straightCurve({1, 0, 0}, {1, 0, 0}));
    guides->rootOnMesh(*scalp);
    const std::string path = write("guided.groom", scatterGraph(30.0), scalp, guides);
    auto groom = host(referenceGraph(path, "closestPoint", true), scalp);
    const StrandsPtr one = Groom(readGroomFile(path).data).evaluate(1.0f, 0.0);
    const StrandsPtr both = groom->evaluate(1.0f, 0.0);
    EXPECT_EQ(both->strandCount(), 2 * one->strandCount());
    EXPECT_EQ(std::set<StrandId>(both->ids().begin(), both->ids().end()).size(), both->strandCount());  // Merge keeps them apart
    GraphDocument doc = loadGraphJson(referenceGraph(path, "closestPoint"));
    ExternalInputs externals;
    setGroomExternals(externals, scalp, nullptr, nullptr);
    EvalContext ctx;
    ctx.externals = &externals;
    const NodeId ref = doc.node("ref");
    const GuidesPtr out = valueAs<GuidesPtr>(doc.graph.evaluateOutput(ref, doc.graph.outputIndex(ref, "guides"), ctx));
    ASSERT_TRUE(out);
    ASSERT_EQ(out->size(), 2u);
    for (const Guide& g : out->guides()) {
        EXPECT_TRUE(scalp->contains(g.root));
        EXPECT_LT((g.points.front() - scalp->position(g.root)).length(), 1e-4f);
    }
}

TEST_F(ReferenceTest, SaysWhatIsWrong) {
    const MeshPtr scalp = uvSphere();
    {
        auto groom = host(referenceGraph("nowhere.groom", "world"), scalp);
        const ViewResult& view = groom->evaluate(ViewRequest{});
        EXPECT_FALSE(view.strands);
        ASSERT_FALSE(view.errors.empty());
        EXPECT_NE(view.errors.front().find("no groom file at 'nowhere.groom'"), std::string::npos) << view.errors.front();
    }
    {
        // A file whose graph references itself.
        const std::string path = (dir_ / "loop.groom").generic_u8string();
        write("loop.groom", referenceGraph(path, "world"), scalp);
        auto groom = host(referenceGraph(path, "world"), scalp);
        const ViewResult& view = groom->evaluate(ViewRequest{});
        EXPECT_FALSE(view.strands);
        ASSERT_FALSE(view.errors.empty());
        EXPECT_NE(view.errors.front().find("references itself"), std::string::npos) << view.errors.front();
    }
    {
        // No path: nothing, with a note.
        auto groom = host(referenceGraph("", "world"), scalp);
        const ViewResult& view = groom->evaluate(ViewRequest{});
        EXPECT_TRUE(view.errors.empty());
        bool said = false;
        for (const auto& [node, note] : groom->nodeNotes()) said = said || node == "ref";
        EXPECT_TRUE(said);
    }
}

}  // namespace
}  // namespace groomlab
