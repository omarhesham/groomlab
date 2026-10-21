// SPDX-License-Identifier: Apache-2.0
// Properties > Inputs > Scalp Subdivision (GroomDocument::setScalpSubdivision): the scalp's
// Catmull-Clark levels refine the surface the hair grows on, the viewport draws, the brushes and
// the Transfer and Photos windows' picks hit; one undo step; kept in the groom file. A coarse
// cube is the scalp, so the smoothed surface is far from the cage. The offscreen platform; the
// pictures of level 0 and 2 go to build/probe/app_subdiv_*.png.

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <QAction>
#include <QComboBox>
#include <QDir>
#include <QImage>
#include <QLabel>
#include <rhi/qrhi.h>

#include <gtest/gtest.h>

#include "brush_session.h"
#include "camera.h"
#include "groom_document.h"
#include "groomlab/subdivision.h"
#include "look.h"
#include "main_window.h"
#include "marker_view.h"
#include "mesh_io.h"
#include "offscreen.h"
#include "renderer.h"
#include "tool_view.h"

namespace groomlab::app {
namespace {

// A cube 10 across about the origin, six quads (the smoothed one is far inside its corners).
std::shared_ptr<const ScalpMesh> cube() {
    return parseObjMesh(
        "v -5 -5 -5\nv 5 -5 -5\nv 5 5 -5\nv -5 5 -5\nv -5 -5 5\nv 5 -5 5\nv 5 5 5\nv -5 5 5\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "f 5/1 6/2 7/3 8/4\nf 2/1 1/2 4/3 3/4\nf 6/1 2/2 3/3 7/4\nf 1/1 5/2 8/3 4/4\nf 8/1 7/2 3/3 4/4\nf 1/1 2/2 6/3 5/4\n",
        "cube.obj");
}

std::string headGraph() {
    std::ifstream in(std::string(GROOMLAB_TEST_DATA) + "/app/head_graph.json");
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// A groom on the cube with the head sample's look, a few strands.
std::unique_ptr<GroomDocument> cubeGroom() {
    auto doc = GroomDocument::newOnMesh(cube(), "cube.obj");
    EXPECT_EQ(doc->replaceGraph(headGraph()), "");
    return doc;
}

const ViewResult& evaluate(GroomDocument& doc, float density = 0.02f) {
    ViewRequest request;
    request.density = density;
    return doc.evaluate(request);
}

float distanceTo(const ScalpMesh& mesh, const Vec3f& p) { return (mesh.closestPoint(p).position - p).length(); }

TEST(AppSubdivision, LevelsRefineTheScalpTheHairGrowsOnAsOneUndoStep) {
    auto doc = cubeGroom();
    EXPECT_EQ(doc->scalpSubdivision(), 0);
    const MeshPtr cage = doc->file().data.scalp;
    ASSERT_EQ(cage->triangleCount(), 12u);
    EXPECT_EQ(evaluate(*doc).scalp->triangleCount(), 12u) << "off: the scalp as given";

    // Each level makes four times the triangles; the count the dock shows is the session's.
    std::size_t before = 12;
    for (int level = 1; level <= kMaxSubdivisionLevels; ++level) {
        ASSERT_EQ(doc->setScalpSubdivision(level), "");
        EXPECT_EQ(doc->scalpSubdivision(), level);
        EXPECT_EQ(doc->scalpLevels(), level) << "the Scalp node's Match Render follows it";
        const ViewResult& view = evaluate(*doc);
        ASSERT_TRUE(view.scalp);
        EXPECT_EQ(view.scalp->subdivisionLevel(), level);
        EXPECT_EQ(view.scalp->triangleCount(), before * 4) << "level " << level;
        EXPECT_EQ(GroomDocument::refinedTriangleCount(*cage, level), view.scalp->triangleCount());
        EXPECT_EQ(doc->session().cageScalp()->triangleCount(), 12u) << "the maps' mesh stays the cage";
        before = view.scalp->triangleCount();
    }
    EXPECT_EQ(doc->undoText(), "Undo scalp subdivision");
    EXPECT_NE(doc->setScalpSubdivision(4), "") << "levels are off and 1 to 3";
    EXPECT_NE(doc->setScalpSubdivision(-1), "");

    // Undo: one step each, the scalp back to the level before; redo again.
    ASSERT_EQ(doc->undo(), "");
    EXPECT_EQ(doc->scalpSubdivision(), 2);
    EXPECT_EQ(evaluate(*doc).scalp->triangleCount(), 12u * 16u);
    ASSERT_EQ(doc->undo(), "");
    ASSERT_EQ(doc->undo(), "");
    EXPECT_EQ(doc->scalpSubdivision(), 0);
    EXPECT_EQ(evaluate(*doc).scalp->triangleCount(), 12u);
    EXPECT_FALSE(doc->canUndo());
    ASSERT_EQ(doc->redo(), "");
    ASSERT_EQ(doc->redo(), "");
    EXPECT_EQ(doc->scalpSubdivision(), 2);
    EXPECT_EQ(evaluate(*doc).scalp->triangleCount(), 12u * 16u);

    // The hair's roots lie on the smoothed surface, well inside the cube's corners.
    const ViewResult& view = evaluate(*doc);
    ASSERT_TRUE(view.strands);
    ASSERT_GT(view.strands->strandCount(), 20u);
    float onRefined = 0.0f, offCage = 0.0f;
    for (std::size_t s = 0; s < view.strands->strandCount(); ++s) {
        const Vec3f root = view.strands->strandPoints(s)[0];
        onRefined = std::max(onRefined, distanceTo(*view.scalp, root));
        offCage = std::max(offCage, distanceTo(*cage, root));
    }
    EXPECT_LT(onRefined, 1e-3f) << "every root on the refined scalp";
    EXPECT_GT(offCage, 0.5f) << "the corners' roots far inside the cube";

    // The views' mesh (Transfer, Guides from Photos): the file's scalp refined, kept per level.
    const MeshPtr shown = doc->refinedFileScalp();
    ASSERT_TRUE(shown);
    EXPECT_EQ(shown->triangleCount(), 12u * 16u);
    EXPECT_EQ(doc->refinedFileScalp(), shown) << "kept";
    ASSERT_EQ(doc->setScalpSubdivision(1), "");
    EXPECT_EQ(doc->refinedFileScalp()->triangleCount(), 48u);
    ASSERT_EQ(doc->setScalpSubdivision(2), "");
    EXPECT_EQ(doc->refinedFileScalp(), shown) << "back to a level made before: the same mesh";
}

TEST(AppSubdivision, SavedWithTheGroomAndReadBack) {
    auto doc = cubeGroom();
    ASSERT_EQ(doc->setScalpSubdivision(2), "");
    EXPECT_TRUE(doc->dirty());
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "groomlab_app_subdivision_save";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "cube.groom").u8string();
    doc->save(path);
    // The file's scalp "subdivision", what every host's reader takes as the render level.
    const GroomFile file = readGroomFile(path);
    ASSERT_TRUE(file.data.scalp);
    EXPECT_EQ(file.data.scalp->renderSubdivision(), 2);
    EXPECT_EQ(file.data.scalp->renderSubdivisionFrom(), "Scalp Subdivision");
    auto again = GroomDocument::open(path);
    EXPECT_EQ(again->scalpSubdivision(), 2);
    EXPECT_EQ(evaluate(*again).scalp->triangleCount(), 12u * 16u);
    // Off: nothing written, read back off.
    ASSERT_EQ(again->setScalpSubdivision(0), "");
    again->save(path);
    EXPECT_EQ(readGroomFile(path).data.scalp->renderSubdivision(), 0);
    EXPECT_EQ(GroomDocument::open(path)->scalpSubdivision(), 0);
    std::filesystem::remove_all(dir);
}

TEST(AppSubdivision, BrushesAndPlacedGuidesHitTheSmoothedSurface) {
    auto doc = cubeGroom();
    evaluate(*doc);
    OrbitCamera camera;
    camera.frame(doc->sceneBounds());
    const int w = 800, h = 600;
    BrushSession session;
    session.setDocument(doc.get());
    session.setView(camera, w, h);
    // The +z face's middle: on the cage at z = 5.
    const Vec3f faceMiddle{0.0f, 0.0f, 5.0f};
    float x = 0.0f, y = 0.0f;
    ASSERT_TRUE(session.view().project(faceMiddle, x, y));
    const QPointF at(x, h - y);
    session.settings().setMode("place");
    session.hover(at);
    ASSERT_TRUE(session.ringValid());
    EXPECT_LT((session.ringCenter() - faceMiddle).length(), 1e-3f) << "off: the cage";

    ASSERT_EQ(doc->setScalpSubdivision(2), "");
    const MeshPtr refined = evaluate(*doc).scalp;
    ASSERT_EQ(refined->subdivisionLevel(), 2);
    session.hover(at);
    ASSERT_TRUE(session.ringValid());
    const Vec3f hit = session.ringCenter();
    EXPECT_LT(distanceTo(*refined, hit), 1e-3f) << "the ring sits on the smoothed surface";
    EXPECT_LT(hit.z, 4.9f) << "behind the cage's face";

    // Place: the new guide's root on the smoothed surface too.
    const std::size_t count = doc->session().hierarchy() ? doc->session().hierarchy()->guides().size() : 0;
    ASSERT_TRUE(session.stroke({at})) << session.lastError();
    evaluate(*doc);
    const auto& guides = doc->session().hierarchy()->guides();
    ASSERT_EQ(guides.size(), count + 1);
    const Vec3f root = guides.back().points.front();
    EXPECT_LT(distanceTo(*refined, root), 1e-3f);
    EXPECT_LT((root - hit).length(), 0.05f);
}

// The window side of a mesh view, as the Transfer and Guides from Photos windows are.
class Host : public MarkerViewHost {
public:
    const MarkerLists& markerLists() const override { return lists_; }
    bool markerEditing() const override { return true; }
    const MarkerHover& hover() const override { return hover_; }
    void setHover(MarkerHover hover) override { hover_ = hover; }
    std::size_t addPicked(MarkerSide side, const MarkerPoint& picked) override {
        lists_.sides[MarkerLists::index(side)].push_back(picked);
        return lists_.count(side) - 1;
    }
    std::vector<std::size_t> deletePoint(MarkerSide, std::size_t) override { return {}; }
    void movePicked(MarkerSide, std::size_t, const MarkerPoint&, bool) override {}
    void edited(bool) override {}
    QRhi* rhi() override { return nullptr; }

    MarkerLists lists_;
    MarkerHover hover_;
};

TEST(AppSubdivision, TransferAndPhotoPicksHitTheSmoothedSurfaceAndStoreOnTheScalp) {
    auto doc = cubeGroom();
    ASSERT_EQ(doc->setScalpSubdivision(2), "");
    evaluate(*doc);
    const MeshPtr cage = doc->file().data.scalp;
    const MeshPtr shown = doc->refinedFileScalp();
    Host host;
    MarkerView view(&host, MarkerSide::Old);
    view.resize(400, 300);
    view.setMesh(cage, shown);
    EXPECT_EQ(view.mesh(), cage);
    EXPECT_EQ(view.shownMesh(), shown);
    QPointF at;
    ASSERT_TRUE(view.project({1.0f, 1.5f, 5.0f}, &at));
    MarkerPoint hit;
    ASSERT_TRUE(view.pick(at, &hit));
    EXPECT_LT(distanceTo(*shown, hit.point), 1e-3f) << "the point on the smoothed surface";
    EXPECT_LT(hit.point.z, 4.9f);
    ASSERT_TRUE(cage->contains(hit.root)) << "stored on the scalp itself";
    EXPECT_LT((view.shownPoint(hit) - hit.point).length(), 1e-3f) << "drawn where it was picked";
    // A point read back on the scalp's own surface (a stored marker's) draws on the smoothed one.
    MarkerPoint stored = hit;
    stored.point = cage->position(hit.root);
    EXPECT_LT(distanceTo(*shown, view.shownPoint(stored)), 1e-3f);
    // Without a level: the mesh itself.
    view.setMesh(cage);
    EXPECT_EQ(view.shownMesh(), cage);
    ASSERT_TRUE(view.pick(at, &hit));
    EXPECT_NEAR(hit.point.z, 5.0f, 1e-3f);
}

TEST(AppSubdivision, PropertiesComboSetsTheLevelAndShowsTheTriangles) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "groomlab_app_subdivision_props";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "cube.groom").u8string();
    cubeGroom()->save(path);
    MainWindow window;
    window.resize(1280, 800);
    window.show();
    ASSERT_TRUE(window.openGroom(QString::fromStdString(path)));
    auto* combo = window.findChild<QComboBox*>(QStringLiteral("scalpSubdivision"));
    auto* count = window.findChild<QLabel*>(QStringLiteral("scalpSubdivisionTriangles"));
    ASSERT_TRUE(combo);
    ASSERT_TRUE(count);
    EXPECT_EQ(combo->count(), 4);
    EXPECT_EQ(combo->currentText(), QStringLiteral("Off"));
    EXPECT_EQ(count->text(), QStringLiteral("12 triangles"));
    combo->setCurrentIndex(2);
    EXPECT_EQ(window.document()->scalpSubdivision(), 2);
    EXPECT_EQ(count->text(), QStringLiteral("192 triangles"));
    EXPECT_EQ(window.document()->undoText(), "Undo scalp subdivision");
    // Edit > Undo puts the combo back, Redo the level again.
    const auto edit = [&](const QString& name) -> QAction* {
        for (QAction* a : window.findChildren<QAction*>()) {
            if (a->text() == name && a->isEnabled()) return a;
        }
        return nullptr;
    };
    QAction* undo = edit(QStringLiteral("Undo scalp subdivision"));
    ASSERT_TRUE(undo);
    undo->trigger();
    EXPECT_EQ(window.document()->scalpSubdivision(), 0);
    EXPECT_EQ(combo->currentText(), QStringLiteral("Off"));
    EXPECT_EQ(count->text(), QStringLiteral("12 triangles"));
    QAction* redo = edit(QStringLiteral("Redo scalp subdivision"));
    ASSERT_TRUE(redo);
    redo->trigger();
    EXPECT_EQ(combo->currentIndex(), 2);
    window.close();
    std::filesystem::remove_all(dir);
}

// Level 0 and 2 drawn offscreen (the viewport's renderer, the refined scalp the session hands it).
TEST(AppSubdivision, DrawsTheSmoothedScalpOffscreen) {
    std::unique_ptr<QRhi> rhi = createOffscreenRhi();
    ASSERT_TRUE(rhi);
    auto doc = cubeGroom();
    const std::filesystem::path probe = std::filesystem::u8path(std::string(GROOMLAB_TEST_DATA)) / ".." / ".." / "build" / "probe";
    std::filesystem::create_directories(probe);
    QImage images[2];
    int k = 0;
    for (const int level : {0, 2}) {
        ASSERT_EQ(doc->setScalpSubdivision(level), "");
        const ViewResult& view = evaluate(*doc, 0.05f);
        ASSERT_TRUE(view.scalp);
        ASSERT_TRUE(view.strands);
        GroomRenderer renderer;
        renderer.setScalp(view.scalp);
        renderer.setHairLook(doc->session().hairLook());
        renderer.setHairStrands(view.strands, view.versions.strands);
        renderer.setLookdev(true);
        LightingSetup lighting = lightingPreset("studio");
        lighting.quality = Quality::Interactive;
        lighting.background = Background::Solid;
        renderer.setLighting(lighting);
        OrbitCamera camera;
        camera.frame(doc->sceneBounds());
        const OffscreenResult shot = renderOffscreen(renderer, camera, QSize(480, 360), 1, rhi.get());
        renderer.releaseResources();
        if (rhi->backend() == QRhi::Null) GTEST_SKIP() << "no GPU device: nothing to picture";
        ASSERT_FALSE(shot.image.isNull());
        images[k++] = shot.image;
        const std::string file = (probe / ("app_subdiv_" + std::to_string(level) + ".png")).u8string();
        EXPECT_TRUE(shot.image.save(QString::fromStdString(file)));
    }
    EXPECT_NE(images[0], images[1]) << "the smoothed cube draws otherwise";
}

}  // namespace
}  // namespace groomlab::app
