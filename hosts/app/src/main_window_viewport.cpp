// SPDX-License-Identifier: Apache-2.0
// MainWindow: surfaces and props, messages and stats, tools and strokes, look and camera.

#include "main_window_internal.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <memory>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QSignalBlocker>
#include <QTimer>

#include "brush_panel.h"
#include "cards_panel.h"
#include "export_panel.h"
#include "glass.h"
#include "look_panel.h"
#include "maps_panel.h"
#include "mesh_io.h"
#include "overlay.h"
#include "theme.h"
#include "viewport.h"

namespace groomlab::app {

// ---- Surfaces -----------------------------------------------------------------------------------

void MainWindow::refreshSurfaces() {
    std::vector<std::pair<QString, SurfaceMaterial>> surfaces{{tr("scalp"), viewport_->renderer().scalpMaterial()}};
    for (const SurfacePtr& p : props_) surfaces.emplace_back(QString::fromStdString(p->name), p->material);
    updatingLook_ = true;
    look_->setSurfaces(surfaces);
    host_->lookCard()->setSurfaces(surfaces);
    updatingLook_ = false;
}

void MainWindow::refreshDisplayChoices() {
    // The color sources (the scalp's color sets and the painted maps for the color set mode, the
    // strands' channels for the attribute mode) and the maps the scalp can show, from the groom.
    const QSignalBlocker blockSource(colorSource_), blockMap(scalpMap_), blockCards(cardsView_);
    const QString source = colorSource_->currentText();
    colorSource_->clear();
    const int mode = colorMode_->currentData().toInt();
    if (document_) {
        if (mode == kColorSetColors) {
            for (const PaintedMap& m : document_->maps()) {
                if (m.kind == "vertices") colorSource_->addItem(QString::fromStdString(m.name));
            }
            for (const std::string& set : document_->colorSetNames()) colorSource_->addItem(QString::fromStdString(set));
        } else {
            for (const std::string& channel : document_->strandChannelNames()) colorSource_->addItem(QString::fromStdString(channel));
        }
    }
    colorSource_->setEditText(source.isEmpty() ? QString::fromStdString(settings_.colorSource) : source);
    const QString shown = document_ ? QString::fromStdString(document_->scalpMapDisplay()) : QString();
    scalpMap_->clear();
    scalpMap_->addItem(tr("(none)"), QString());
    if (document_) {
        for (const PaintedMap& m : document_->maps()) scalpMap_->addItem(QString::fromStdString(m.name), QString::fromStdString(m.name));
    }
    scalpMap_->setCurrentIndex(std::max(0, scalpMap_->findData(shown)));
    if (document_) scalpMapChannel_->setCurrentText(QString::fromStdString(document_->scalpMapChannel()));
    const bool cards = document_ && document_->session().hasCardsNode();
    cardsView_->setEnabled(cards);
    if (!cards) cardsView_->setChecked(false);
}

void MainWindow::applyDisplayOptions() {
    const int mode = colorMode_->currentData().toInt();
    const bool modeChanged = mode != settings_.colorMode;
    settings_.colorMode = mode;
    settings_.colorSource = colorSource_->currentText().trimmed().toStdString();
    settings_.colorMin = static_cast<float>(colorMin_->value());
    settings_.colorMax = static_cast<float>(colorMax_->value());
    settings_.guidePoints = guidePoints_->currentIndex();
    settings_.guideWidth = static_cast<float>(guideWidth_->value());
    const bool cardsWanted = cardsView_->isChecked() && cardsView_->isEnabled();
    const bool cardsChanged = cardsWanted != settings_.cardsView;
    settings_.cardsView = cardsWanted;
    settings_.drawCards = cardsWanted;
    if (document_) document_->setScalpMapDisplay(scalpMap_->currentData().toString().toStdString(), scalpMapChannel_->currentText().toStdString());
    // The colored modes are the line path's (the builder colors the lines): the lookdev ribbons
    // step aside while one is on and come back with the solid mode.
    viewport_->renderer().setLookdev(lookdevWanted_ && mode == kSolidColors);
    if (modeChanged) refreshDisplayChoices();
    if (maps_) maps_->refresh();
    if (cardsChanged) reevaluate();  // the view builds the cards (ViewRequest::wantCards)
    else redisplay();
}

void MainWindow::applyProps() {
    // The props, and the textured cards among them while they show.
    std::vector<SurfacePtr> props = props_;
    if (cardSurface_) props.push_back(cardSurface_);
    viewport_->renderer().setProps(props);
    // Every prop is a collider by its name (a head loaded beside the scalp): a ColliderMesh node
    // naming it keeps the hair out of it, as Maya's collider inputs do. The groom file's own
    // colliders stay, a prop of the same name in their place.
    if (document_) {
        std::map<std::string, MeshPtr> meshes = document_->file().data.meshes;
        for (const SurfacePtr& p : props_) {
            if (p->mesh && !p->isEye) meshes[p->name] = p->mesh;
        }
        document_->session().setMeshes(std::move(meshes));
    }
}

Bounds MainWindow::sceneBounds() const {
    Bounds b = document_ ? document_->sceneBounds() : Bounds();
    for (const SurfacePtr& p : props_) {
        if (!p->mesh) continue;
        for (const Vec3f& v : p->mesh->points()) {
            if (!b.valid) {
                b.lo = b.hi = v;
                b.valid = true;
            } else {
                b.lo = {std::min(b.lo.x, v.x), std::min(b.lo.y, v.y), std::min(b.lo.z, v.z)};
                b.hi = {std::max(b.hi.x, v.x), std::max(b.hi.y, v.y), std::max(b.hi.z, v.z)};
            }
        }
    }
    return b;
}

bool MainWindow::addProp(const QString& path, const QString& materialPreset, int occlusionRays) {
    SurfacePtr surface;
    try {
        SurfaceMaterial base = surfacePreset(materialPreset.toStdString());
        if (base.presetName.empty() && !materialPreset.isEmpty()) {
            report(tr("No surface preset '%1' (clay, scalp, plastic, metal, skin, skin light, skin dark)").arg(materialPreset));
            return false;
        }
        surface = std::make_shared<Surface>(readObjSurface(path.toStdString(), base));
    } catch (const std::exception& e) {
        report(tr("Cannot load the prop %1: %2").arg(path, QString::fromUtf8(e.what())));
        return false;
    }
    props_.push_back(surface);
    if (occlusionRays > 0) {
        // Each prop's occlusion against every prop loaded so far, the new one among them: a
        // head's own sockets and nostrils, and what the props hide of each other.
        std::vector<MeshPtr> occluders;
        for (const SurfacePtr& p : props_) occluders.push_back(p->mesh);
        const Bounds b = sceneBounds();
        const float reach = b.valid ? (b.hi - b.lo).length() * 0.5f : 10.0f;
        for (const SurfacePtr& p : props_) {
            if (p->isEye) continue;
            p->vertexOcclusion = bakeVertexOcclusion(*p->mesh, occluders, occlusionRays, reach);
            ++p->version;
        }
    }
    applyProps();
    refreshSurfaces();
    frameAll();
    report(tr("Loaded the prop %1 (%2 triangles)").arg(path).arg(surface->mesh->triangleCount()), false);
    return true;
}

bool MainWindow::addEye(const QVector3D& center, const QVector3D& forward, float radius, const QString& materialPreset, int occlusionRays) {
    SurfaceMaterial material = surfacePreset(materialPreset.isEmpty() ? "eye brown" : materialPreset.toStdString());
    if (material.kind != SurfaceKind::Eye) {
        report(tr("No eye preset '%1' (eye brown, eye blue, eye green)").arg(materialPreset));
        return false;
    }
    auto eye = std::make_shared<Surface>(makeEyeSurface({center.x(), center.y(), center.z()}, {forward.x(), forward.y(), forward.z()},
                                                        {0.0f, 1.0f, 0.0f}, radius, material));
    eye->name = "eye " + std::to_string(std::count_if(props_.begin(), props_.end(), [](const SurfacePtr& p) { return p->isEye; }) + 1);
    if (occlusionRays > 0) {
        // The lids: what the head hides of the eye from each of its points.
        std::vector<MeshPtr> occluders;
        for (const SurfacePtr& p : props_) {
            if (!p->isEye) occluders.push_back(p->mesh);
        }
        eye->vertexOcclusion = bakeVertexOcclusion(*eye->mesh, occluders, occlusionRays, radius * 4.0f);
    }
    props_.push_back(eye);
    applyProps();
    refreshSurfaces();
    viewport_->renderer().invalidateAccumulation();
    viewport_->update();
    return true;
}

bool MainWindow::setPropParam(const QString& text) {
    const int equals = text.indexOf(QLatin1Char('='));
    if (props_.empty() || equals <= 0) {
        report(tr("Expected name=value on a loaded prop, got %1").arg(text));
        return false;
    }
    SurfacePtr prop = props_.back();
    if (!setSurfaceParam(prop->material, text.left(equals).toStdString(), text.mid(equals + 1).toStdString())) {
        report(tr("Cannot set %1 on %2").arg(text, QString::fromStdString(prop->name)));
        return false;
    }
    viewport_->renderer().propsChanged();
    refreshSurfaces();
    viewport_->update();
    return true;
}

void MainWindow::setScalpMaterial(const QString& preset) {
    const SurfaceMaterial m = surfacePreset(preset.toStdString());
    if (m.presetName.empty()) {
        report(tr("No surface preset '%1'").arg(preset));
        return;
    }
    viewport_->renderer().setScalpMaterial(m);
    refreshSurfaces();
    viewport_->update();
}

// ---- Messages and stats -----------------------------------------------------------------------

void MainWindow::report(const QString& text, bool error) {
    lastReport_ = text;
    std::fprintf(stderr, "GroomLab: %s\n", qPrintable(text));
    if (error && isVisible()) host_->toast(text);
}

void MainWindow::scheduleStats() {
    if (!statsAge_.isValid() || statsAge_.elapsed() >= kStatsMaxAgeMs) {
        refreshStats();
        return;
    }
    statsTimer_->start();  // (again: once the frames stop)
}

void MainWindow::refreshStats() {
    statsAge_.start();
    if (statsTimer_) statsTimer_->stop();
    const GroomRenderer& r = viewport_->renderer();
    const ViewResult* view = document_ ? &document_->lastView() : nullptr;
    const std::size_t strands = view && view->strands ? view->strands->strandCount() : 0;
    const bool gpu = r.lookdevActive();
    host_->setStats(strands, lastDabMs_ > 0.0 ? lastDabMs_ : lastEvaluateMs_, gpu ? tr("GPU") : tr("LINES"),
                    gpu ? theme::purple() : theme::textMuted());
    const bool refining = r.lookdev() && r.lighting().quality == Quality::Refine;
    host_->setRefine(refining, r.accumulatedFrames(), std::clamp(r.lighting().refineFrames, 1, LookdevRenderer::kMaxAccumulatedFrames),
                     viewport_->lastFrameMs());
}

void MainWindow::refreshBrushBar() {
    const BrushToolSettings& s = brush_.settings();
    BrushBar::Values v;
    v.mode = brush_.toolId();
    v.radius = s.screenRadius;
    v.strength = s.brush.strength;
    v.hardness = s.brush.hardness;
    v.mirrorAxis = s.brush.mirrorTopology ? 0 : s.brush.mirrorAxis;
    v.pressure = s.pressure;
    host_->bar()->setValues(v);
    const Viewport::ToolMode mode = viewport_->toolMode();
    host_->setTool(mode == Viewport::ToolMode::Brush ? v.mode : std::string(), mode == Viewport::ToolMode::Select);
}

void MainWindow::setTurntable(bool on) {
    if (on) turntable_->start();
    else turntable_->stop();
}

bool MainWindow::turntable() const { return turntable_->isActive(); }

bool MainWindow::setToolMode(const QString& name) {
    const QString n = name.toLower();
    if (n == QStringLiteral("camera")) viewport_->setToolMode(Viewport::ToolMode::Camera);
    else if (n == QStringLiteral("brush")) viewport_->setToolMode(Viewport::ToolMode::Brush);
    else if (n == QStringLiteral("select")) viewport_->setToolMode(Viewport::ToolMode::Select);
    else {
        report(tr("No tool mode '%1' (camera, brush, select)").arg(name));
        return false;
    }
    return true;
}

bool MainWindow::setBrushText(const QString& text) {
    const int equals = text.indexOf(QLatin1Char('='));
    if (equals <= 0) {
        report(tr("Expected name=value for the brush, got %1").arg(text));
        return false;
    }
    const QString name = text.left(equals).trimmed().toLower(), value = text.mid(equals + 1).trimmed();
    BrushToolSettings& s = brush_.settings();
    bool ok = true;
    auto number = [&](float* out) {
        const float v = value.toFloat(&ok);
        if (ok) *out = v;
    };
    auto flag = [&](bool* out) {
        const QString v = value.toLower();
        ok = v == QStringLiteral("1") || v == QStringLiteral("0") || v == QStringLiteral("true") || v == QStringLiteral("false") ||
             v == QStringLiteral("on") || v == QStringLiteral("off");
        if (ok) *out = v == QStringLiteral("1") || v == QStringLiteral("true") || v == QStringLiteral("on");
    };
    try {
        if (name == QStringLiteral("mode")) brush_.setTool(value.toStdString());
        else if (name == QStringLiteral("radius")) number(&s.screenRadius);
        else if (name == QStringLiteral("strength")) number(&s.brush.strength);
        else if (name == QStringLiteral("hardness")) number(&s.brush.hardness);
        else if (name == QStringLiteral("strengthprofile")) s.setStrengthProfile(value.toStdString());
        else if (name == QStringLiteral("falloffprofile")) s.setFalloffProfile(value.toStdString());
        else if (name == QStringLiteral("symmetry")) s.setSymmetry(value.toStdString());
        else if (name == QStringLiteral("symmetrycenter")) number(&s.brush.mirrorCenter);
        else if (name == QStringLiteral("level")) s.brush.level = value.toInt(&ok);
        else if (name == QStringLiteral("group")) s.brush.group = value.toStdString();
        else if (name == QStringLiteral("pressure")) flag(&s.pressure);
        else if (name == QStringLiteral("maskbyscalp")) flag(&s.maskByScalp);
        else if (name == QStringLiteral("selectedonly")) flag(&s.selectedOnly);
        else if (name == QStringLiteral("collide")) flag(&s.brush.collide);
        else if (name == QStringLiteral("stretch")) flag(&s.brush.stretch);
        else if (name == QStringLiteral("sculptdensity")) number(&brush_.sculptDensity);
        else if (name == QStringLiteral("length")) number(&s.place.length);
        else if (name == QStringLiteral("spacing")) number(&s.place.spacing);
        else if (name == QStringLiteral("solonewguides")) flag(&s.place.soloNewGuides);
        else if (name == QStringLiteral("attribute")) s.brush.attribute = value.toStdString();
        else if (name == QStringLiteral("value")) number(&s.brush.value);
        else ok = false;
    } catch (const std::invalid_argument& e) {
        report(tr("Cannot set the brush's %1: %2").arg(name, QString::fromUtf8(e.what())));
        return false;
    }
    if (!ok) {
        report(tr("Cannot set the brush's %1 to %2").arg(name, value));
        return false;
    }
    brushPanel_->refresh();
    viewport_->refreshOverlay();
    refreshBrushBar();
    return true;
}

bool MainWindow::applyStroke(const std::vector<QPointF>& points, const QSize& size, bool ctrl, bool shift, bool drawDabs) {
    if (!document_) {
        report(tr("No groom open to stroke"));
        return false;
    }
    if (points.empty()) {
        report(tr("A stroke needs at least one point"));
        return false;
    }
    if (drawDabs && !ensureHeadless(size)) return false;
    if (drawDabs) {
        headlessFrame();  // the strands before the stroke drawn (uploads done)
        warmStroke();     // as the window does between strokes in brush mode: the press makes again only what it changes
    }
    brush_.setView(viewport_->camera(), size.width(), size.height());
    const unsigned modifiers = (ctrl ? kStrokeCtrl : 0u) | (shift ? kStrokeShift : 0u);
    // As the mouse: each point evaluates the preview and builds its display, timed (the dab's
    // latency is what the artist waits for; the frame's draw comes on top, counted when the
    // dabs are drawn).
    QElapsedTimer timer;
    double total = 0.0, slowest = 0.0, evaluated = 0.0, pressMs = 0.0, drawn = 0.0;
    int dabs = 0, gpuDabs = 0;
    std::vector<double> dabMs;
    const std::size_t evaluations0 = document_->session().evaluationCount(), incrementals0 = document_->session().incrementalCount();
    auto dab = [&](bool press) {
        if (!brush_.stroking()) return;
        timer.start();
        reevaluate(true);
        if (drawDabs) {
            // The frame as the artist sees it: the GPU waited for. A run put off to the next
            // frame (the press's upload landing first) is drawn there and counted here.
            headlessFrame();
            if (headless_->renderer.wantsAnotherFrame() && !headless_->renderer.gpuRanLastFrame()) headlessFrame();
            if (headless_->renderer.gpuRanLastFrame()) ++gpuDabs;
        }
        const double ms = timer.nsecsElapsed() / 1e6;
        if (press) {
            pressMs = ms;
        } else {
            total += ms;
            evaluated += lastEvaluateMs_;
            slowest = std::max(slowest, ms);
            dabMs.push_back(ms);
            ++dabs;
        }
    };
    if (!brush_.press(points.front(), 1.0f, modifiers)) {
        report(tr("Cannot start the stroke: %1").arg(QString::fromStdString(brush_.lastError())));
        return false;
    }
    dab(true);
    for (std::size_t i = 1; i < points.size(); ++i) {
        brush_.move(points[i], 1.0f);
        if (!brush_.lastError().empty()) break;
        dab(false);
    }
    timer.start();
    brush_.release();
    const double releaseMs = timer.nsecsElapsed() / 1e6;
    for (const std::string& warning : brush_.takeWarnings()) report(QString::fromStdString(warning));
    if (!brush_.lastError().empty()) {
        report(tr("The stroke was dropped: %1").arg(QString::fromStdString(brush_.lastError())));
        guidesEdited();
        return false;
    }
    if (dabs > 0) lastDabMs_ = total / dabs;
    timer.start();
    guidesEdited();
    if (drawDabs) headlessFrame();
    const double fullViewMs = timer.nsecsElapsed() / 1e6;
    const ViewResult& view = document_->lastView();
    const GroomSession& session = document_->session();
    report(tr("Stroke (%1): %2 dabs, %3 ms a dab (%4 evaluating, the slowest dab %5), the commit %6 ms and the full view %7 ms; "
              "%8 strands; %9 node evaluations, %10 of them incremental")
               .arg(QString::fromStdString(brush_.settings().modeName()))
               .arg(dabs)
               .arg(dabs > 0 ? total / dabs : 0.0, 0, 'f', 1)
               .arg(dabs > 0 ? evaluated / dabs : 0.0, 0, 'f', 1)
               .arg(slowest, 0, 'f', 1)
               .arg(releaseMs, 0, 'f', 1)
               .arg(fullViewMs, 0, 'f', 1)
               .arg(view.strands ? view.strands->strandCount() : 0)
               .arg(session.evaluationCount() - evaluations0)
               .arg(session.incrementalCount() - incrementals0),
           false);
    if (drawDabs) {
        std::sort(dabMs.begin(), dabMs.end());
        const double median = dabMs.empty() ? 0.0 : dabMs[dabMs.size() / 2];
        const GroomRenderer& r = headless_->renderer;
        const char* path = r.lookdevActive() ? "lookdev" : "lines";
        const bool partial = r.lookdevActive() ? r.lookdevRenderer().lastUploadPartial() : r.lastUploadPartial();
        const std::size_t uploaded = r.lookdevActive() ? r.lookdevRenderer().lastUploadStrands() : r.lastUploadStrands();
        const double uploadMs = r.lookdevActive() ? r.lookdevRenderer().lastUploadMs() : r.lastUploadMs();
        std::printf("GroomLab: benchmark stroke (%s, %s): press %.2f ms to the drawn frame; %d dabs, median %.2f ms [%.2f-%.2f] to the "
                    "drawn frame (%d drawn by the GPU); release + full view %.2f ms; the last upload: %zu strands%s in %.2f ms\n",
                    brush_.settings().modeName().c_str(), path, pressMs, dabs, median, dabMs.empty() ? 0.0 : dabMs.front(),
                    dabMs.empty() ? 0.0 : dabMs.back(), gpuDabs, releaseMs + fullViewMs, uploaded, partial ? " (partial)" : "", uploadMs);
        (void)drawn;
    }
    viewport_->syncView();
    return true;
}

void MainWindow::redisplay() {
    if (!document_) return;
    const DisplayFrame& frame = document_->updateDisplay(settings_);
    viewport_->renderer().setHairLook(document_->session().hairLook());
    // The coverage at the render's density: the density the shown strands were evaluated at.
    const float density = settings_.coverageDensity ? document_->lastView().density : 1.0f;
    viewport_->renderer().setWidthCoverage(settings_.widthCoverage, settings_.ribbonWidthScale, density);
    viewport_->renderer().setClumpLod(settings_.clumpLod, settings_.clumpLodError);
    applyLook();
    viewport_->setFrame(frame);
    // The headless view sees the same frame (its rebuilt flags hold until the next update).
    if (headless_) {
        headless_->renderer.setHairLook(document_->session().hairLook());
        headless_->renderer.setWidthCoverage(settings_.widthCoverage, settings_.ribbonWidthScale, density);
        headless_->renderer.setClumpLod(settings_.clumpLod, settings_.clumpLodError);
        headless_->renderer.setFrame(frame);
    }
}

void MainWindow::setGpuPreview(bool on) {
    if (gpuPreview_ && gpuPreview_->isChecked() != on) {
        gpuPreview_->setChecked(on);  // comes back here through the toggle
        return;
    }
    gpu_.setEnabled(on);
    viewport_->update();
}

void MainWindow::applyLook() {
    HairMaterial material = look_->material();
    if (document_ && material.pigment != HairMaterial::Pigment::Material) {
        // The groom's HairColor highlight, as create_hair_shader maps it to the Arnold shader.
        const HairLook look = document_->session().hairLook();
        applyGroomLook(material, look.specular, look.roughness, look.tilt);
        material.iridescenceColor = look.iridescenceColor;
        material.iridescenceShift = look.iridescenceShift;
        // Colored by more than the pigment (a map, bands, a feather pattern...): its RGB, as
        // create_hair_shader gives Arnold the base color over no melanin.
        if (look.colorsByRgb && material.pigment == HairMaterial::Pigment::GroomMelanin) material.pigment = HairMaterial::Pigment::GroomColor;
    }
    viewport_->renderer().setMaterial(material);
    viewport_->renderer().setLighting(look_->lighting());
    viewport_->update();
    scheduleStats();
}

void MainWindow::adjustCamera(float yawDegrees, float pitchDegrees, float dolly) {
    viewport_->camera().orbit(yawDegrees, pitchDegrees);
    if (dolly > 0.0f) viewport_->camera().dolly(dolly);
    viewport_->renderer().invalidateAccumulation();
    viewport_->syncView();
    viewport_->update();
}

void MainWindow::lookAt(const QVector3D& target, float distance) {
    viewport_->camera().setTarget(target);
    if (distance > 0.0f) viewport_->camera().setDistance(distance);
    viewport_->renderer().invalidateAccumulation();
    viewport_->syncView();
    viewport_->update();
}

void MainWindow::applyMaterialPreset(const QString& name) { look_->applyMaterialPreset(name); }

void MainWindow::applyLightingPreset(const QString& name) { look_->applyLightingPreset(name); }

void MainWindow::setToneMap(const QString& name) {
    LightingSetup lighting = look_->lighting();
    const QString n = name.toLower();
    lighting.toneMap = n == QStringLiteral("none") ? ToneMap::None : n == QStringLiteral("agx") ? ToneMap::AgX : ToneMap::ACES;
    look_->setLighting(lighting);
    host_->lookCard()->setLighting(lighting);
    applyLook();
}

void MainWindow::setQuality(const QString& name) {
    LightingSetup lighting = look_->lighting();
    lighting.quality = name.compare(QStringLiteral("interactive"), Qt::CaseInsensitive) == 0 ? Quality::Interactive : Quality::Refine;
    look_->setLighting(lighting);
    host_->lookCard()->setLighting(lighting);
    applyLook();
}

void MainWindow::setInteractiveQuality(const QString& name) {
    LightingSetup lighting = look_->lighting();
    lighting.interactiveQuality = name.compare(QStringLiteral("full"), Qt::CaseInsensitive) == 0 ? InteractiveQuality::Full : InteractiveQuality::Adaptive;
    look_->setLighting(lighting);
    host_->lookCard()->setLighting(lighting);
    applyLook();
}

void MainWindow::setShowScalp(bool on) {
    scalp_->setChecked(on);  // the toggle reaches the renderer
}

void MainWindow::setLookdev(bool on) {
    if (lookdev_->isChecked() != on) {
        lookdev_->setChecked(on);  // comes back here through the toggle
        return;
    }
    lookdevWanted_ = on;
    viewport_->renderer().setLookdev(on && settings_.colorMode == kSolidColors);
    viewport_->update();
}

}  // namespace groomlab::app
