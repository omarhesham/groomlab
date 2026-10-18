// SPDX-License-Identifier: Apache-2.0
// MainWindow: its panels, built and wired to the document, the viewport and the floating UI.

#include "main_window_internal.h"

#include <QFileInfo>

#include <cmath>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QVBoxLayout>

#include "brush_panel.h"
#include "cards_panel.h"
#include "correctives_panel.h"
#include "export_panel.h"
#include "glass.h"
#include "guides_panel.h"
#include "look_panel.h"
#include "maps_panel.h"
#include "node_graph.h"
#include "overlay.h"
#include "param_panel.h"
#include "theme.h"
#include "viewport.h"

namespace groomlab::app {

void MainWindow::buildPanels() {
    // The ordinary panels, each the content of a floating glass panel shown on demand.
    auto* nodesHost = new QWidget();
    auto* nodesLayout = new QVBoxLayout(nodesHost);
    nodesLayout->setContentsMargins(0, 0, 0, 0);
    auto* split = new QSplitter(Qt::Vertical, nodesHost);
    nodes_ = new QListWidget(split);
    params_ = new ParamPanel(split);
    split->addWidget(nodes_);
    split->addWidget(params_);
    split->setStretchFactor(1, 1);
    nodesLayout->addWidget(split);
    host_->setPanelContent(OverlayHost::Panel::Nodes, tr("Node List"), theme::blue(), nodesHost);
    connect(nodes_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item, QListWidgetItem*) {
        params_->showNode(item ? item->data(Qt::UserRole).toString().toStdString() : std::string());
    });
    connect(params_, &ParamPanel::edited, this, [this] { graphEdited(); });
    connect(params_, &ParamPanel::dragging, this, [this](bool on) { paramDragged(on); });
    connect(params_, &ParamPanel::message, this, [this](const QString& text) { report(text); });
    connect(params_, &ParamPanel::paintParamRequested, this,
            [this](const std::string& node, const std::string& name) { paintParam(QString::fromStdString(node), QString::fromStdString(name)); });

    graphParams_ = new ParamPanel();
    host_->setPanelContent(OverlayHost::Panel::Node, tr("Node Settings"), theme::blue(), graphParams_);
    connect(graphParams_, &ParamPanel::edited, this, [this] { graphEdited(); });
    connect(graphParams_, &ParamPanel::dragging, this, [this](bool on) { paramDragged(on); });
    connect(graphParams_, &ParamPanel::message, this, [this](const QString& text) { report(text); });
    connect(graphParams_, &ParamPanel::paintParamRequested, this,
            [this](const std::string& node, const std::string& name) { paintParam(QString::fromStdString(node), QString::fromStdString(name)); });

    // The display options in a scroll area (the sliders ask more height than a short view gives).
    auto* displayScroll = new QScrollArea();
    displayScroll->setWidgetResizable(true);
    displayScroll->setFrameShape(QFrame::NoFrame);
    displayScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* display = new QWidget(displayScroll);
    auto* form = new QFormLayout(display);
    form->setContentsMargins(0, 0, 8, 0);
    displayScroll->setWidget(display);
    // The sliders carry their captions (a row's whole width); a drag's end evaluates.
    density_ = new ValueSlider(display);
    density_->setStacked(true);
    density_->setCaption(tr("Preview density"));
    density_->setRange(0.001, 1.0);
    density_->setLimits(0.001, 1.0);
    density_->setDecimals(3);
    density_->setStep(0.01);
    density_->setValue(1.0);
    density_->setFill(theme::purple());
    density_->setToolTip(tr("The preview density: the fraction of the strands evaluated and drawn"));
    form->addRow(density_);
    budget_ = new ValueSlider(display);
    budget_->setStacked(true);
    budget_->setCaption(tr("Point budget"));
    budget_->setInteger(true);
    budget_->setRange(0.0, 2000.0);
    budget_->setLimits(0.0, 1000000.0);
    budget_->setStep(50.0);
    budget_->setValue(kDefaultPointBudget);  // sensible by default (Maya previews at 10%): brushing stays responsive on a big groom
    budget_->setFill(theme::purple());
    budget_->setFormat([](double v) { return v < 0.5 ? tr("no limit") : QString::number(std::lround(v)) + tr(" k points"); });
    budget_->setToolTip(tr("Points the preview may hold (thousands); each Scatter thins to fit"));
    form->addRow(budget_);
    lookdev_ = new QCheckBox(display);
    lookdev_->setChecked(true);
    lookdev_->setToolTip(tr("The strands as shaded ribbons under the look's material and lights (off: lines)"));
    form->addRow(tr("Lookdev"), lookdev_);
    gpuPreview_ = new QCheckBox(display);
    gpuPreview_->setChecked(gpu_.enabled());
    gpuPreview_->setToolTip(tr("Slider, seed and profile drags of Noise, Curl, Width, Clump, Length and Interpolate, and brush "
                               "dabs, previewed by the GPU (Direct3D 11, OpenGL); the release commits on the CPU"));
    form->addRow(tr("GPU preview"), gpuPreview_);
    // The strand colors (Maya's strandColorMode): solid or lit under the look, a color set of the
    // scalp at each root, a strand channel over a range, a random color per strand, by clump,
    // by density. The colored modes draw lines (the lookdev ribbons take the material's pigment).
    colorMode_ = new QComboBox(display);
    colorMode_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    colorMode_->setMinimumContentsLength(8);
    colorMode_->addItem(tr("Solid / lit"), kSolidColors);
    colorMode_->addItem(tr("Color set"), kColorSetColors);
    colorMode_->addItem(tr("Attribute"), kAttributeColors);
    colorMode_->addItem(tr("Random"), kRandomColors);
    colorMode_->addItem(tr("Clump"), kClumpColors);
    colorMode_->addItem(tr("Density"), kDensityColors);
    colorMode_->setToolTip(tr("What colors the strands: the look (lit), a color set of the scalp at the root, a per-strand "
                              "channel over the range below (a ramp from blue to red), a random color per strand, the "
                              "clump each strand belongs to, or how dense the strands are"));
    form->addRow(tr("Strand colors"), colorMode_);
    colorSource_ = new QComboBox(display);
    colorSource_->setEditable(true);
    colorSource_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    colorSource_->setMinimumContentsLength(8);
    colorSource_->setToolTip(tr("The color set or the strand channel read (clumpId, melanin, length...)"));
    form->addRow(tr("Source"), colorSource_);
    auto* rangeHost = new QWidget(display);
    auto* rangeRow = new QHBoxLayout(rangeHost);
    rangeRow->setContentsMargins(0, 0, 0, 0);
    colorMin_ = new QDoubleSpinBox(rangeHost);
    colorMin_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);  // not the width of "-1000000.000"
    colorMin_->setRange(-1e6, 1e6);
    colorMin_->setDecimals(3);
    colorMin_->setKeyboardTracking(false);
    colorMax_ = new QDoubleSpinBox(rangeHost);
    colorMax_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    colorMax_->setRange(-1e6, 1e6);
    colorMax_->setDecimals(3);
    colorMax_->setValue(1.0);
    colorMax_->setKeyboardTracking(false);
    rangeRow->addWidget(colorMin_);
    rangeRow->addWidget(colorMax_);
    rangeHost->setToolTip(tr("The attribute's values shown from blue (min) to red (max)"));
    form->addRow(tr("Range"), rangeHost);
    guides_ = new QCheckBox(display);
    guides_->setChecked(settings_.guides);
    form->addRow(tr("Guides"), guides_);
    tubes_ = new QCheckBox(display);
    tubes_->setChecked(settings_.tubes);
    tubes_->setToolTip(tr("Each guide's tube, the volume its children and Interpolate's Fill Tubes fill: its shape "
                          "and sections along it"));
    form->addRow(tr("Tubes"), tubes_);
    guidePoints_ = new QComboBox(display);
    guidePoints_->addItems({tr("No dots"), tr("Roots and tips"), tr("Every point")});
    guidePoints_->setCurrentIndex(settings_.guidePoints);
    guidePoints_->setToolTip(tr("The guides' points as dots"));
    form->addRow(tr("Guide points"), guidePoints_);
    guideWidth_ = new ValueSlider(display);
    guideWidth_->setStacked(true);
    guideWidth_->setCaption(tr("Guide width"));
    guideWidth_->setRange(1.0, 12.0);
    guideWidth_->setLimits(1.0, 12.0);
    guideWidth_->setDecimals(1);
    guideWidth_->setStep(0.5);
    guideWidth_->setValue(settings_.guideWidth);
    guideWidth_->setFill(theme::emerald());
    form->addRow(guideWidth_);
    lighting_ = new QCheckBox(display);
    lighting_->setChecked(settings_.lighting);
    lighting_->setToolTip(tr("The line path's Kajiya-Kay shading (lookdev off)"));
    form->addRow(tr("Lit lines"), lighting_);
    widthCoverage_ = new QCheckBox(display);
    widthCoverage_->setChecked(settings_.widthCoverage);
    widthCoverage_->setToolTip(tr("The line path's strands a pixel wide, each point covering the share of the pixel its "
                                  "width covers: tapered tips fade, thin strands read light, dense hair builds up (off: "
                                  "opaque lines)"));
    form->addRow(tr("Width coverage"), widthCoverage_);
    coverageDensity_ = new QCheckBox(display);
    coverageDensity_->setChecked(settings_.coverageDensity);
    coverageDensity_->setToolTip(tr("Width coverage at the render's density: a thinned preview's strands each cover what the "
                                    "strands they stand for would, so the preview reads as dense as the render"));
    form->addRow(tr("Coverage at full density"), coverageDensity_);
    clumpLod_ = new QCheckBox(display);
    clumpLod_->setChecked(settings_.clumpLod);
    clumpLod_->setToolTip(tr("Level of detail for the line path's strands: each clump draws as few of its strands as cover its "
                             "pixels, each standing for the rest, and a clump thinner than a pixel as one line; chosen by the "
                             "camera's distance (the same while it orbits). Off: every strand"));
    form->addRow(tr("Clump LOD"), clumpLod_);
    scalp_ = new QCheckBox(display);
    scalp_->setChecked(true);
    form->addRow(tr("Scalp"), scalp_);
    // A painted map drawn on the scalp (the Maya plugin's show_map_on_scalp).
    scalpMap_ = new QComboBox(display);
    scalpMap_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    scalpMap_->setMinimumContentsLength(8);
    scalpMap_->setToolTip(tr("A painted map drawn on the scalp: a vertex map's values, a guide layer blended between the "
                             "guides, a texture at the scalp's vertices"));
    form->addRow(tr("Map on scalp"), scalpMap_);
    scalpMapChannel_ = new QComboBox(display);
    scalpMapChannel_->addItems({QStringLiteral("rgb"), QStringLiteral("r"), QStringLiteral("g"), QStringLiteral("b"), QStringLiteral("a")});
    scalpMapChannel_->setToolTip(tr("A color map shown in color, or one channel as gray"));
    form->addRow(tr("Map channel"), scalpMapChannel_);
    cardsView_ = new QCheckBox(display);
    cardsView_->setEnabled(false);
    cardsView_->setToolTip(tr("The Cards nodes' cards instead of the strands (when the graph has a Cards node)"));
    form->addRow(tr("Card view"), cardsView_);
    // Scaling with the scalp: the groom grows with its scalp (kept in the file); its scale now and
    // Set Scale Reference (the scalp's size now is 1x).
    scaleSource_ = new QComboBox(display);
    scaleSource_->addItem(tr("Surface Area"), static_cast<int>(ScaleSource::Area));
    scaleSource_->addItem(tr("Transform Only"), static_cast<int>(ScaleSource::Transform));
    scaleSource_->addItem(tr("Off"), static_cast<int>(ScaleSource::Off));
    scaleSource_->setToolTip(tr("What the groom's scale follows (it grows with its scalp like a child object: lengths, widths, "
                                "clumps and guide shapes scale with it, densities thin so the strand count stays). Surface "
                                "Area: the scalp's surface area (a remodel to another size scales the groom). Transform Only: "
                                "the scalp's transform in Maya (here, the scale the groom opened with: remodels and other "
                                "scalps leave it). Surface Area survives Freeze Transformations; Transform Only survives "
                                "remodelling. Off: sizes in scene units"));
    form->addRow(tr("Scale source"), scaleSource_);
    auto* scaleHost = new QWidget(display);
    auto* scaleRow = new QHBoxLayout(scaleHost);
    scaleRow->setContentsMargins(0, 0, 0, 0);
    groomScale_ = new QLabel(QStringLiteral("1.00x"), scaleHost);
    groomScale_->setToolTip(tr("What the groom is scaled by with its scalp: 1.00x at the size it was groomed at"));
    auto* setReference = new QPushButton(tr("Set Scale Reference"), scaleHost);
    setReference->setToolTip(tr("Make the scalp's size now the groom's 1x: the groom keeps the sizes its params say at this size"));
    scaleRow->addWidget(groomScale_);
    scaleRow->addWidget(setReference, 1);
    form->addRow(tr("Groom scale"), scaleHost);
    connect(scaleSource_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!document_) return;
        document_->setScaleSource(static_cast<ScaleSource>(scaleSource_->currentData().toInt()));
        refreshDisplayChoices();
        reevaluate();
    });
    connect(setReference, &QPushButton::clicked, this, [this] {
        if (!document_) return;
        document_->setScaleReference();
        refreshDisplayChoices();
        reevaluate();
    });
    host_->setPanelContent(OverlayHost::Panel::Display, tr("Display"), theme::purple(), displayScroll);
    connect(density_, &ValueSlider::released, this, [this] { reevaluate(); });
    connect(budget_, &ValueSlider::released, this, [this] { reevaluate(); });
    connect(lookdev_, &QCheckBox::toggled, this, [this](bool on) { setLookdev(on); });
    connect(gpuPreview_, &QCheckBox::toggled, this, [this](bool on) { setGpuPreview(on); });
    connect(guides_, &QCheckBox::toggled, this, [this](bool on) {
        settings_.guides = on;
        redisplay();
    });
    connect(tubes_, &QCheckBox::toggled, this, [this](bool on) {
        settings_.tubes = on;
        redisplay();
    });
    connect(lighting_, &QCheckBox::toggled, this, [this](bool on) {
        settings_.lighting = on;
        redisplay();
    });
    connect(widthCoverage_, &QCheckBox::toggled, this, [this](bool on) {
        settings_.widthCoverage = on;
        redisplay();
        viewport_->update();
    });
    connect(coverageDensity_, &QCheckBox::toggled, this, [this](bool on) {
        settings_.coverageDensity = on;
        redisplay();
        viewport_->update();
    });
    connect(clumpLod_, &QCheckBox::toggled, this, [this](bool on) {
        settings_.clumpLod = on;
        redisplay();
        viewport_->update();
    });
    connect(scalp_, &QCheckBox::toggled, this, [this](bool on) {
        viewport_->renderer().setShowScalp(on);
        viewport_->renderer().invalidateAccumulation();
        viewport_->update();
    });
    connect(colorMode_, &QComboBox::currentIndexChanged, this, [this](int) { applyDisplayOptions(); });
    connect(colorSource_, &QComboBox::currentTextChanged, this, [this](const QString&) { applyDisplayOptions(); });
    connect(colorMin_, &QDoubleSpinBox::valueChanged, this, [this](double) { applyDisplayOptions(); });
    connect(colorMax_, &QDoubleSpinBox::valueChanged, this, [this](double) { applyDisplayOptions(); });
    connect(guidePoints_, &QComboBox::currentIndexChanged, this, [this](int) { applyDisplayOptions(); });
    connect(guideWidth_, &ValueSlider::valueChanged, this, [this](double) { applyDisplayOptions(); });
    connect(scalpMap_, &QComboBox::currentIndexChanged, this, [this](int) { applyDisplayOptions(); });
    connect(scalpMapChannel_, &QComboBox::currentIndexChanged, this, [this](int) { applyDisplayOptions(); });
    connect(cardsView_, &QCheckBox::toggled, this, [this](bool) { applyDisplayOptions(); });

    // The maps: made, painted and shown through the document and the brush session.
    maps_ = new MapsPanel();
    maps_->setSession(&brush_);
    host_->setPanelContent(OverlayHost::Panel::Maps, tr("Maps"), theme::yellow(), maps_);
    connect(maps_, &MapsPanel::edited, this, [this] {
        guidesEdited();
        refreshDisplayChoices();
        params_->refresh();
        graphParams_->refresh();
    });
    connect(maps_, &MapsPanel::displayChanged, this, [this] {
        refreshDisplayChoices();
        redisplay();
    });
    connect(maps_, &MapsPanel::brushChanged, this, [this] {
        brushPanel_->refresh();
        refreshBrushBar();
    });
    connect(maps_, &MapsPanel::paintRequested, this, [this] {
        setUiMode(QStringLiteral("groom"));
        viewport_->setToolMode(Viewport::ToolMode::Brush);
        brushPanel_->refresh();
        viewport_->refreshOverlay();
        refreshBrushBar();
    });
    connect(maps_, &MapsPanel::message, this, [this](const QString& text) { report(text); });
    connect(maps_, &MapsPanel::paintParamRequested, this, [this](const std::string& node, const std::string& param, const std::string& kind) {
        paintParam(QString::fromStdString(node), QString::fromStdString(param), QString::fromStdString(kind));
    });

    look_ = new LookPanel();
    host_->setPanelContent(OverlayHost::Panel::Look, tr("Look Settings"), theme::yellow(), look_);
    brushPanel_ = new BrushPanel();
    host_->setPanelContent(OverlayHost::Panel::Brush, tr("Brush"), theme::blue(), brushPanel_);
    help_ = new QLabel();
    help_->setWordWrap(true);
    help_->setTextFormat(Qt::RichText);
    help_->setOpenExternalLinks(true);
    help_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    host_->setPanelContent(OverlayHost::Panel::Help, tr("Help"), theme::blue(), new FitScrollArea(help_));  // scrolls when short
    brushPanel_->setSession(&brush_);
    connect(brushPanel_, &BrushPanel::settingsChanged, this, [this] {
        viewport_->syncView();
        viewport_->refreshOverlay();
        refreshBrushBar();
    });
    guidesPanel_ = new GuidesPanel();
    host_->setPanelContent(OverlayHost::Panel::Guides, tr("Guide groups"), theme::emerald(), guidesPanel_);
    connect(guidesPanel_, &GuidesPanel::edited, this, [this] { guidesEdited(); });
    // The correctives: the layers, and the one the brushes sculpt into at the timeline's frame.
    correctives_ = new CorrectivesPanel();
    host_->setPanelContent(OverlayHost::Panel::Correctives, tr("Correctives"), theme::red(), correctives_);
    correctives_->setSession(&brush_);
    connect(correctives_, &CorrectivesPanel::edited, this, [this] { guidesEdited(); });
    connect(correctives_, &CorrectivesPanel::targetChanged, this, [this] {
        // A shot sculpt (or a Sculpt node) wants the brush: the layer (the node) takes the strokes
        // from here on.
        if ((!correctives_->targetLayer().empty() || !correctives_->targetNode().empty()) &&
            viewport_->toolMode() != Viewport::ToolMode::Brush) {
            setUiMode(QStringLiteral("groom"));
            viewport_->setToolMode(Viewport::ToolMode::Brush);
        }
        refreshBrushBar();
    });
    connect(correctives_, &CorrectivesPanel::message, this, [this](const QString& text) { report(text); });
    connect(guidesPanel_, &GuidesPanel::displayChanged, this, [this] {
        if (document_) settings_.hiddenGuideGroups = document_->hiddenGroupsText();
        redisplay();
        guidesPanel_->refresh();
    });
    connect(guidesPanel_, &GuidesPanel::message, this, [this](const QString& text) { report(text); });
    connect(look_, &LookPanel::materialChanged, this, [this] {
        if (updatingLook_) return;
        host_->lookCard()->setMaterial(look_->material());
        applyLook();
    });
    connect(look_, &LookPanel::lightingChanged, this, [this] {
        if (updatingLook_) return;
        host_->lookCard()->setLighting(look_->lighting());
        applyLook();
    });
    connect(look_, &LookPanel::surfaceChanged, this, [this](int index, const SurfaceMaterial& material) {
        if (updatingLook_) return;
        GroomRenderer& r = viewport_->renderer();
        if (index == 0) {
            r.setScalpMaterial(material);
        } else if (index > 0 && static_cast<std::size_t>(index) <= props_.size()) {
            props_[index - 1]->material = material;
            r.propsChanged();
        }
        refreshSurfaces();
        viewport_->update();
    });
    refreshSurfaces();
    host_->lookCard()->setMaterial(look_->material());
    host_->lookCard()->setLighting(look_->lighting());
    applyLook();
}

void MainWindow::wireOverlay() {
    connect(host_, &OverlayHost::undoRequested, this, [this] { undo(); });
    connect(host_, &OverlayHost::redoRequested, this, [this] { redo(); });
    connect(host_, &OverlayHost::frameAllRequested, this, [this] { frameAll(); });
    connect(host_, &OverlayHost::graphToggleRequested, this, [this] { setGraphShown(!graphShown()); });
    connect(host_, &OverlayHost::graphCloseRequested, this, [this] { setGraphShown(false); });
    connect(host_, &OverlayHost::presetsRequested, this, [this] { showPresetsMenu(); });
    // The empty-state card and a file dropped on the window.
    connect(host_, &OverlayHost::openRequested, this, [this] { chooseOpen(); });
    connect(host_, &OverlayHost::newRequested, this, [this] { chooseMesh(); });
    connect(host_, &OverlayHost::placeRequested, this, [this] {
        setUiMode(QStringLiteral("groom"));
        emit host_->rail()->brushChosen(std::string("place"));
    });
    // A dropped or recent file asks first when the open groom has unsaved edits.
    connect(host_, &OverlayHost::fileDropped, this, [this](const QString& path) {
        confirmDiscard(tr("opening %1").arg(QFileInfo(path).fileName()), [this, path] { openPath(path); });
    });
    host_->setRecentFiles(recentFiles());
    connect(host_, &OverlayHost::renderRequested, this, [this] {
        // Refine mode from a clean slate: the frames accumulate again until the picture converges.
        setQuality(QStringLiteral("refine"));
        viewport_->renderer().invalidateAccumulation();
        viewport_->update();
    });
    connect(host_, &OverlayHost::modeChanged, this, [this](OverlayHost::Mode mode) {
        // The brushes belong to Groom mode: elsewhere the mouse is the camera's. Cards mode
        // shows the cards (when the graph makes some); leaving it shows the strands again.
        if (mode != OverlayHost::Mode::Groom) viewport_->setToolMode(Viewport::ToolMode::Camera);
        const bool cards = mode == OverlayHost::Mode::Cards && document_ && !document_->cardsNodes().empty();
        if (host_->cardsPanel()->showCards() != cards) {
            host_->cardsPanel()->setShowCards(cards);
            reevaluate();
        } else {
            redisplay();  // the guides show in Groom mode alone
        }
        refreshStats();
    });
    // The rail picks a brush (Freeze among them) and the brush tool, or the Select tool.
    connect(host_->rail(), &BrushRail::brushChosen, this, [this](const std::string& mode) {
        try {
            brush_.setTool(mode);
        } catch (const std::invalid_argument& e) {
            report(QString::fromUtf8(e.what()));
            return;
        }
        viewport_->setToolMode(Viewport::ToolMode::Brush);
        brushPanel_->refresh();
        viewport_->refreshOverlay();
        refreshBrushBar();
    });
    connect(host_->rail(), &BrushRail::selectChosen, this, [this] {
        viewport_->setToolMode(viewport_->toolMode() == Viewport::ToolMode::Select ? Viewport::ToolMode::Camera : Viewport::ToolMode::Select);
    });
    // The bar edits the main settings; the full panel follows when the edit is done.
    connect(host_->bar(), &BrushBar::edited, this, [this](bool live) {
        const BrushBar::Values& v = host_->bar()->values();
        BrushToolSettings& s = brush_.settings();
        s.setScreenRadius(v.radius);
        s.brush.strength = v.strength;
        s.setHardness(v.hardness);
        // The Symmetry chip: off, or back on as it was (World X, Y, Z or Topology).
        if (v.mirrorAxis != (s.brush.mirrorTopology ? 3 : s.brush.mirrorAxis)) {
            static const char* const names[] = {"x", "y", "z", "topology"};
            s.setSymmetry(v.mirrorAxis >= 0 && v.mirrorAxis <= 3 ? names[v.mirrorAxis] : "none");
        }
        s.pressure = v.pressure;
        viewport_->syncView();
        viewport_->refreshOverlay();
        if (!live) brushPanel_->refresh();
    });
    connect(host_->bar(), &BrushBar::moreRequested, this, [this] { host_->togglePanel(OverlayHost::Panel::Brush); });
    // The look card edits the material and lighting; the full look panel follows.
    connect(host_->lookCard(), &LookCard::materialEdited, this, [this] {
        updatingLook_ = true;
        look_->setMaterial(host_->lookCard()->material());
        updatingLook_ = false;
        applyLook();
    });
    connect(host_->lookCard(), &LookCard::lightingEdited, this, [this] {
        updatingLook_ = true;
        look_->setLighting(host_->lookCard()->lighting());
        updatingLook_ = false;
        applyLook();
    });
    connect(host_->lookCard(), &LookCard::surfaceEdited, this, [this](int index, const SurfaceMaterial& material) {
        // The card's edit onto the surface, the full panel following.
        GroomRenderer& r = viewport_->renderer();
        if (index == 0) {
            r.setScalpMaterial(material);
        } else if (index > 0 && static_cast<std::size_t>(index) <= props_.size()) {
            props_[index - 1]->material = material;
            r.propsChanged();
        }
        updatingLook_ = true;
        std::vector<std::pair<QString, SurfaceMaterial>> surfaces{{tr("scalp"), r.scalpMaterial()}};
        for (const SurfacePtr& p : props_) surfaces.emplace_back(QString::fromStdString(p->name), p->material);
        look_->setSurfaces(surfaces);
        updatingLook_ = false;
        viewport_->update();
    });
    connect(host_->lookCard(), &LookCard::moreRequested, this, [this] { host_->togglePanel(OverlayHost::Panel::Look); });
    // The card's swatches and main sliders edit the groom's last HairColor node (saved with the
    // groom): a slider's drag previews each step and records one edit at its end, a swatch sets the
    // pigment as one edit.
    connect(host_->lookCard(), &LookCard::groomColorEdited, this, [this](const std::string& param, double value, bool live) {
        const std::string node = hairColorNode();
        if (!document_ || node.empty()) return;
        if (live && !groomColorDragging_) {
            groomColorDragging_ = true;
            groomColorBefore_ = document_->paramOrDefault(node, param);
        }
        const std::string error = document_->setParam(node, param, value, !live && !groomColorDragging_);
        if (!error.empty()) {
            report(QString::fromStdString(error));
            refreshGroomColor();
            return;
        }
        if (!live && groomColorDragging_) {
            groomColorDragging_ = false;
            document_->recordEdit(node, param, groomColorBefore_, document_->paramOrDefault(node, param));
        }
        if (live) reevaluate(true);
        else graphEdited();
    });
    connect(host_->lookCard(), &LookCard::groomSwatchChosen, this, [this](const std::string& preset) {
        const std::string node = hairColorNode();
        if (!document_ || node.empty()) return;
        const HairMaterial p = hairPreset(preset);
        const std::string error = document_->editGraph("hair color " + preset, [&](nlohmann::json& g) {
            for (nlohmann::json& n : g["nodes"]) {
                if (n.value("name", std::string()) != node) continue;
                n["params"]["mode"] = "melanin";  // a swatch is a pigment
                n["params"]["melanin"] = p.melanin;
                n["params"]["redness"] = p.melaninRedness;
            }
        });
        if (!error.empty()) {
            report(QString::fromStdString(error));
            refreshGroomColor();
            return;
        }
        graphEdited();
    });
    // HairColor's mode, and its picked colors (the hair's, the tips'): one edit each, a color
    // dragged in its picker previewed live and recorded once. A tip color picked while the tips
    // are at 0 strength raises them to 1, so the pick shows.
    connect(host_->lookCard(), &LookCard::groomModeChosen, this, [this](const std::string& mode) {
        const std::string node = hairColorNode();
        if (!document_ || node.empty()) return;
        const std::string error = document_->setParam(node, "mode", mode, true);
        if (!error.empty()) report(QString::fromStdString(error));
        graphEdited();
    });
    connect(host_->lookCard(), &LookCard::groomColorPicked, this, [this](const std::string& param, const RGB& c, bool live) {
        const std::string node = hairColorNode();
        if (!document_ || node.empty()) return;
        const nlohmann::json value = {c[0], c[1], c[2]};
        if (live && !groomColorDragging_) {
            groomColorDragging_ = true;
            groomColorBefore_ = document_->paramOrDefault(node, param);
        }
        std::string error = document_->setParam(node, param, value, !live && !groomColorDragging_);
        if (error.empty() && !live && groomColorDragging_) {
            groomColorDragging_ = false;
            document_->recordEdit(node, param, groomColorBefore_, document_->paramOrDefault(node, param));
        }
        if (error.empty() && !live && param == "tipColor") {
            const nlohmann::json strength = document_->paramOrDefault(node, "tipStrength");
            if (strength.is_number() && strength.get<double>() <= 0.0) error = document_->setParam(node, "tipStrength", 1.0, true);
        }
        if (!error.empty()) {
            report(QString::fromStdString(error));
            refreshGroomColor();
            return;
        }
        if (live) reevaluate(true);
        else graphEdited();
    });
    // The graph: a selected node's params in a floating panel; its edits shown and undoable.
    connect(graph_, &NodeGraphView::selectionChanged, this, [this](const std::string& node) {
        graphParams_->showNode(node);
        if (node.empty()) {
            host_->showPanel(OverlayHost::Panel::Node, false);
            return;
        }
        const nlohmann::json* spec = document_ ? document_->nodeSpec(node) : nullptr;
        const std::string category = spec ? spec->value("category", std::string()) : std::string();
        // "Node Settings" in the node's category color (the node's name and type head the panel).
        host_->setNodePanelTitle(tr("Node Settings"), theme::nodeCategoryColor(category));
        host_->showPanel(OverlayHost::Panel::Node, true);
        // The node in sight: the graph pans it out from under the panel.
        if (FloatingPanel* panel = host_->panel(OverlayHost::Panel::Node); panel && panel->isVisible()) {
            graph_->keepSelectedClearOf(panel->geometry());
        }
    });
    // View Up to Here (V): the viewport shows the graph's result at that node.
    connect(graph_, &NodeGraphView::viewNodeChanged, this, [this](const std::string& node) {
        if (node == viewNode_) return;
        viewNode_ = node;
        if (document_) reevaluate();
    });
    connect(graph_, &NodeGraphView::edited, this, [this] { graphEdited(); });
    connect(graph_, &NodeGraphView::message, this, [this](const QString& text) { report(text); });
    connect(graph_, &NodeGraphView::hideRequested, this, [this] { setGraphShown(false); });
}

void MainWindow::wireTimelineCardsAndExport() {
    // The timeline names frames; the window evaluates and draws them (frames the clock passes
    // while one is being made are never asked for: the clock is read again after each).
    Timeline* timeline = host_->timeline();
    connect(timeline, &Timeline::frameChanged, this, [this](double frame) { showFrame(frame); });
    connect(timeline, &Timeline::rangeChanged, this, [this](double, double) { refreshExportState(); });
    connect(timeline, &Timeline::playingChanged, this, [this](bool playing) {
        if (!playing && document_) {
            report(tr("Played %1 frames, %2 dropped").arg(shownFrames_).arg(host_->timeline()->droppedFrames()), false);
        }
        shownFrames_ = 0;
        refreshStats();
    });
    connect(timeline, &Timeline::followScalpChanged, this, [this](bool on) {
        if (!document_) return;
        document_->setFollowScalp(on);
        reevaluate();
        refreshTitle();
    });
    connect(timeline, &Timeline::simulateRequested, this, [this] { simulateTo(); });
    connect(timeline, &Timeline::bakeRequested, this, [this] { bakeSimulation(); });
    connect(timeline, &Timeline::invalidateRequested, this, [this] { invalidateSimulation(); });
    connect(timeline, &Timeline::simulationCancelRequested, this, [this] { cancelJob(); });

    CardsPanel* cards = host_->cardsPanel();
    connect(cards, &CardsPanel::edited, this, [this](bool live) {
        if (live) {
            reevaluate(true);
            refreshCards();
        } else {
            graphEdited();
        }
    });
    connect(cards, &CardsPanel::viewChanged, this, [this] { reevaluate(); });
    connect(cards, &CardsPanel::bakeRequested, this, [this] { bakeCards(); });
    connect(cards, &CardsPanel::addNodeRequested, this, [this] {
        if (!document_) return;
        std::string made;
        const std::string error = document_->addCardsNode(&made);
        if (!error.empty()) {
            report(tr("Cannot add a Cards node: %1").arg(QString::fromStdString(error)));
            return;
        }
        host_->cardsPanel()->setShowCards(true);
        graphEdited();
    });

    ExportPanel* exports = host_->exportPanel();
    connect(exports, &ExportPanel::exportRequested, this, [this] { runExport(); });
    connect(exports, &ExportPanel::cancelRequested, this, [this] { cancelJob(); });
    connect(exports, &ExportPanel::browseRequested, this, [this] { chooseExportPath(); });
    connect(exports, &ExportPanel::saveRequested, this, [this] { save(); });
    connect(exports, &ExportPanel::saveAsRequested, this, [this] { saveAs(); });
}

}  // namespace groomlab::app
