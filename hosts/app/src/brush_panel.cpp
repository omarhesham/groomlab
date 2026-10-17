// SPDX-License-Identifier: Apache-2.0
#include "brush_panel.h"

#include <algorithm>
#include <stdexcept>

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include "groomlab/brush_catalog.h"

#include "brush_session.h"
#include "glass.h"
#include "groom_document.h"
#include "param_editors.h"
#include "theme.h"

namespace groomlab::app {

namespace {

QFormLayout* formOf(QGroupBox* box) { return static_cast<QFormLayout*>(box->layout()); }

}  // namespace

BrushPanel::BrushPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget(scroll);
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(6, 6, 6, 6);

    // The brush itself is picked in the rail (which names every brush); this panel holds the
    // settings of the active one.
    auto* brush = new QGroupBox(tr("Brush"), host);
    new QFormLayout(brush);
    slider(brush, tr("Radius"), 5.0, 400.0, 5.0, 2000.0, 0, [this] { return session_->settings().screenRadius; },
         [this](double v) { session_->settings().setScreenRadius(static_cast<float>(v)); },
         withShortcut(tr("The brush circle in pixels (middle-drag sets it too)"), "brush.radius"))
        ->setFormat([](double v) { return QString::number(std::lround(v)) + QStringLiteral(" px"); });
    slider(brush, tr("Strength"), 0.0, 2.0, -10.0, 10.0, 2, [this] { return session_->settings().brush.strength; },
         [this](double v) { session_->settings().brush.strength = static_cast<float>(v); },
         withShortcut(tr("Comb: the fraction of the drag; others: the effect per stamp (Shift+middle-drag sets it too)"), "brush.strength"));
    // Falloff as the catalog names it in both hosts: 0 hard to the edge, 1 soft from the center
    // (the core's hardness is 1 - falloff).
    const groomlab::BrushSettingSpec* falloff = brushSettingSpec("falloff");
    slider(brush, QString::fromStdString(falloff->label), 0.0, 1.0, 0.0, 1.0, 2,
           [this] { return 1.0 - session_->settings().brush.hardness; },
           [this](double v) { session_->settings().brush.hardness = static_cast<float>(1.0 - v); }, QString::fromStdString(falloff->hint));
    // The curves: the falloff drawn instead of the slider's, and the strength root to tip.
    curve(brush, "falloffProfile", "[[0, 1], [0.5, 0.56], [1, 0]]",
          [this] { return session_->settings().falloffProfileJson(); },
          [this](const std::string& v) { session_->settings().setFalloffProfile(v); });
    curve(brush, "strengthProfile", "[[0, 0.25], [1, 1]]",
          [this] { return session_->settings().strengthProfileJson(); },
          [this](const std::string& v) { session_->settings().setStrengthProfile(v); });
    check(brush, tr("Pen pressure"), [this] { return session_->settings().pressure; },
          [this](bool on) { session_->settings().pressure = on; }, tr("A pen's pressure scales the strength"));
    symmetry_ = new QComboBox(brush);
    // Off, World X, Y, Z (a world plane at the center), Topology (across the scalp's own surface).
    symmetry_->addItems({tr("Off"), tr("World X"), tr("World Y"), tr("World Z"), tr("Topology")});
    symmetry_->setObjectName(QStringLiteral("symmetry"));
    symmetry_->setToolTip(withShortcut(QString::fromStdString(brushSettingSpec("symmetry")->hint) +
                                           tr(" (World X, Y, Z: across the plane where that coordinate is the center)"),
                                       "brush.symmetry"));
    formOf(brush)->addRow(tr("Symmetry"), symmetry_);
    connect(symmetry_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || !session_) return;
        static const char* names[] = {"none", "x", "y", "z", "topology"};
        session_->settings().setSymmetry(names[std::clamp(index, 0, 4)]);
        emit settingsChanged();
    });
    refreshers_.push_back([this] {
        const BrushSettings& b = session_->settings().brush;
        symmetry_->setCurrentIndex(b.mirrorTopology ? 4 : b.mirrorAxis + 1);
    });
    slider(brush, tr("Symmetry center"), -10.0, 10.0, -1e6, 1e6, 3, [this] { return session_->settings().brush.mirrorCenter; },
         [this](double v) { session_->settings().brush.mirrorCenter = static_cast<float>(v); });
    const groomlab::BrushSettingSpec* frontFacing = brushSettingSpec("frontFacingOnly");
    check(brush, QString::fromStdString(frontFacing->label), [this] { return session_->settings().maskByScalp; },
          [this](bool on) { session_->settings().maskByScalp = on; }, QString::fromStdString(frontFacing->hint));
    level_ = new QSpinBox(brush);
    level_->setRange(-1, 7);
    level_->setSpecialValueText(tr("every level"));
    level_->setToolTip(tr("The hierarchy level brushed (0 primaries; Shift+click a guide solos it and picks its level)"));
    formOf(brush)->addRow(tr("Level"), level_);
    connect(level_, &QSpinBox::valueChanged, this, [this](int v) {
        if (updating_ || !session_) return;
        session_->settings().brush.level = v;
        emit settingsChanged();
    });
    refreshers_.push_back([this] { level_->setValue(session_->settings().brush.level); });
    group_ = new QComboBox(brush);
    group_->addItem(tr("(by level)"), QString());
    group_->setToolTip(tr("Brush only this guide group instead of a level"));
    formOf(brush)->addRow(tr("Group"), group_);
    connect(group_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || !session_) return;
        session_->settings().brush.group = group_->itemData(index).toString().toStdString();
        emit settingsChanged();
    });
    refreshers_.push_back([this] {
        const QString group = QString::fromStdString(session_->settings().brush.group);
        int index = group_->findData(group);
        if (index < 0 && !group.isEmpty()) {
            group_->addItem(group, group);
            index = group_->count() - 1;
        }
        group_->setCurrentIndex(std::max(index, 0));
    });
    check(brush, tr("Selected only"), [this] { return session_->settings().selectedOnly; },
          [this](bool on) { session_->settings().selectedOnly = on; }, tr("Brush the selected guides alone"));
    check(brush, tr("Propagate"), [this] { return session_->settings().brush.propagate; },
          [this](bool on) { session_->settings().brush.propagate = on; }, tr("Also edit locked descendants"));
    check(brush, tr("Collide"), [this] { return session_->settings().brush.collide; },
          [this](bool on) { session_->settings().brush.collide = on; }, tr("Keep edited guides out of the scalp"));
    const groomlab::BrushSettingSpec* stretch = brushSettingSpec("stretch");
    check(brush, QString::fromStdString(stretch->label), [this] { return session_->settings().brush.stretch; },
          [this](bool on) { session_->settings().brush.stretch = on; }, QString::fromStdString(stretch->hint));
    slider(brush, tr("Sculpt density"), 0.01, 1.0, 0.01, 1.0, 2, [this] { return session_->sculptDensity; },
         [this](double v) { session_->sculptDensity = static_cast<float>(v); },
         tr("The fraction of the strands shown while a stroke is live (the release shows them all)"));
    layout->addWidget(brush);

    auto* targets = new QGroupBox(tr("Targets"), host);
    new QFormLayout(targets);
    check(targets, tr("Guides"), [this] { return session_->settings().brushGuides; },
          [this](bool on) { session_->settings().brushGuides = on; }, tr("The shaping brushes act on guides"));
    check(targets, tr("Parts"), [this] { return session_->settings().brushParts; },
          [this](bool on) { session_->settings().brushParts = on; }, tr("... and on part lines"));
    check(targets, tr("Flow"), [this] { return session_->settings().brushFlow; },
          [this](bool on) { session_->settings().brushFlow = on; }, tr("... and on flow arrows"));
    check(targets, tr("Erase guides"), [this] { return session_->settings().eraseGuides; },
          [this](bool on) { session_->settings().eraseGuides = on; });
    check(targets, tr("Erase parts"), [this] { return session_->settings().eraseParts; },
          [this](bool on) { session_->settings().eraseParts = on; });
    check(targets, tr("Erase flow"), [this] { return session_->settings().eraseFlow; },
          [this](bool on) { session_->settings().eraseFlow = on; });
    layout->addWidget(targets);

    auto* place = new QGroupBox(tr("Place, Draw, Paint"), host);
    new QFormLayout(place);
    slider(place, tr("Length"), 0.01, 50.0, 0.01, 1e4, 3, [this] { return session_->settings().place.length; },
         [this](double v) { session_->settings().place.length = static_cast<float>(v); },
         tr("New guides' length (standing, or without neighbors to match)"));
    auto* segments = new QSpinBox(place);
    segments->setRange(2, 200);
    formOf(place)->addRow(tr("Segments"), segments);
    connect(segments, &QSpinBox::valueChanged, this, [this](int v) {
        if (updating_ || !session_) return;
        session_->settings().setSegments(v);
        emit settingsChanged();
    });
    refreshers_.push_back([this, segments] { segments->setValue(session_->settings().place.segments); });
    slider(place, tr("Spacing"), 0.0, 10.0, 0.0, 1e4, 3, [this] { return session_->settings().place.spacing; },
         [this](double v) { session_->settings().place.spacing = static_cast<float>(v); },
         tr("Place many: no new root closer than this to another of the group"));
    check(place, tr("Match neighbors"), [this] { return session_->settings().place.matchNeighbors; },
          [this](bool on) { session_->settings().place.matchNeighbors = on; },
          tr("New guides interpolate the nearest guides' shape instead of standing straight"));
    const groomlab::BrushSettingSpec* solo = brushSettingSpec("soloNewGuides");
    check(place, QString::fromStdString(solo->label), [this] { return session_->settings().place.soloNewGuides; },
          [this](bool on) { session_->settings().place.soloNewGuides = on; }, QString::fromStdString(solo->hint))
        ->setObjectName(QStringLiteral("soloNewGuides"));
    // Beside Freeze: every guide brushable again (one undo step).
    auto* unfreeze = new QPushButton(tr("Unfreeze All"), place);
    unfreeze->setObjectName(QStringLiteral("unfreezeAll"));
    unfreeze->setToolTip(tr("Clear the freeze mask: every guide can be brushed again (Shift+click off a guide with a "
                            "brush does the same)"));
    formOf(place)->addRow(unfreeze);
    connect(unfreeze, &QPushButton::clicked, this, [this] {
        if (!session_ || !session_->document()) return;
        session_->document()->editGuides("unfreeze all", [](GuideHierarchy& h) { unfreezeAll(h); });
        emit settingsChanged();
    });
    attribute_ = new QLineEdit(place);
    attribute_->setToolTip(tr("Paint: the per-guide attribute painted"));
    formOf(place)->addRow(tr("Attribute"), attribute_);
    connect(attribute_, &QLineEdit::editingFinished, this, [this] {
        if (updating_ || !session_) return;
        session_->settings().brush.attribute = attribute_->text().toStdString();
        emit settingsChanged();
    });
    refreshers_.push_back([this] { attribute_->setText(QString::fromStdString(session_->settings().brush.attribute)); });
    slider(place, tr("Value"), 0.0, 1.0, -1e4, 1e4, 3, [this] { return session_->settings().brush.value; },
         [this](double v) { session_->settings().brush.value = static_cast<float>(v); }, tr("Paint: the value painted toward"));
    check(place, tr("Pointwise"), [this] { return session_->settings().brush.pointwise; },
          [this](bool on) { session_->settings().brush.pointwise = on; },
          tr("Paint a per-point layer along the guide instead of one value per guide"));
    partName_ = new QLineEdit(place);
    partName_->setToolTip(tr("Part: the new part's name (made unique)"));
    formOf(place)->addRow(tr("Part name"), partName_);
    connect(partName_, &QLineEdit::editingFinished, this, [this] {
        if (updating_ || !session_) return;
        session_->settings().partName = partName_->text().toStdString();
        emit settingsChanged();
    });
    refreshers_.push_back([this] { partName_->setText(QString::fromStdString(session_->settings().partName)); });
    flowSink_ = new QComboBox(place);
    flowSink_->addItems({tr("arrows"), tr("whorl"), tr("whorlcw"), tr("converge"), tr("part")});
    flowSink_->setToolTip(tr("Flow: arrows along the stroke, or a sink of this kind where clicked"));
    formOf(place)->addRow(tr("Flow"), flowSink_);
    connect(flowSink_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || !session_) return;
        session_->settings().flowSink = index == 0 ? std::string() : flowSink_->itemText(index).toStdString();
        emit settingsChanged();
    });
    refreshers_.push_back([this] {
        const int index = flowSink_->findText(QString::fromStdString(session_->settings().flowSink));
        flowSink_->setCurrentIndex(std::max(index, 0));
    });
    layout->addWidget(place);
    layout->addStretch(1);

    scroll->setWidget(host);
    outer->addWidget(scroll);
    setEnabled(false);
}

ValueSlider* BrushPanel::slider(QGroupBox* box, const QString& label, double min, double max, double limitMin, double limitMax,
                                int decimals, std::function<double()> get, std::function<void(double)> set, const QString& tip) {
    auto* editor = new ValueSlider(box);
    editor->setStacked(true);
    editor->setCaption(label);
    editor->setRange(min, max);
    editor->setLimits(limitMin, limitMax);
    editor->setDecimals(decimals);
    editor->setFill(theme::blue());
    if (!tip.isEmpty()) editor->setToolTip(tip);
    formOf(box)->addRow(editor);  // the slider carries its caption: the row's whole width
    connect(editor, &ValueSlider::valueChanged, this, [this, set](double v) {
        if (updating_ || !session_) return;
        set(v);
        emit settingsChanged();
    });
    refreshers_.push_back([editor, get] { editor->setValue(get()); });
    return editor;
}

QCheckBox* BrushPanel::check(QGroupBox* box, const QString& label, std::function<bool()> get, std::function<void(bool)> set,
                             const QString& tip) {
    auto* editor = new QCheckBox(box);
    if (!tip.isEmpty()) editor->setToolTip(tip);
    formOf(box)->addRow(label, editor);
    connect(editor, &QCheckBox::toggled, this, [this, set](bool on) {
        if (updating_ || !session_) return;
        set(on);
        emit settingsChanged();
    });
    refreshers_.push_back([editor, get] { editor->setChecked(get()); });
    return editor;
}

void BrushPanel::curve(QGroupBox* box, const std::string& setting, const std::string& start,
                       std::function<std::string()> get, std::function<void(const std::string&)> set) {
    const groomlab::BrushSettingSpec* spec = brushSettingSpec(setting);
    auto* on = new QCheckBox(box);
    on->setObjectName(QString::fromStdString(setting));
    auto* editor = new RampEditor(box);
    editor->setObjectName(QString::fromStdString(setting) + QStringLiteral("Curve"));
    editor->setValue(nlohmann::json::parse(start));
    editor->setVisible(false);
    if (spec) {
        on->setToolTip(QString::fromStdString(spec->hint));
        editor->setToolTip(QString::fromStdString(spec->hint));
    }
    formOf(box)->addRow(spec ? QString::fromStdString(spec->label) : QString::fromStdString(setting), on);
    formOf(box)->addRow(editor);
    auto push = [this, on, editor, set] {
        if (updating_ || !session_) return;
        try {
            set(on->isChecked() ? editor->value().dump() : std::string());
        } catch (const std::invalid_argument&) {
            return;
        }
        emit settingsChanged();
    };
    connect(on, &QCheckBox::toggled, this, [editor, push](bool checked) {
        editor->setVisible(checked);
        push();
    });
    connect(editor, &RampEditor::edited, this, [push](bool live) {
        if (!live) push();
    });
    refreshers_.push_back([on, editor, get] {
        const std::string text = get();
        on->setChecked(!text.empty());
        editor->setVisible(!text.empty());
        if (!text.empty()) editor->setValue(nlohmann::json::parse(text, nullptr, false));
    });
}

void BrushPanel::setSession(BrushSession* session) {
    session_ = session;
    setEnabled(session_ != nullptr);
    refresh();
}

void BrushPanel::setGroups(const std::vector<std::string>& names) {
    const QSignalBlocker blocker(group_);
    const QString current = group_->currentData().toString();
    group_->clear();
    group_->addItem(tr("(by level)"), QString());
    for (const std::string& name : names) group_->addItem(QString::fromStdString(name), QString::fromStdString(name));
    const int index = group_->findData(current);
    group_->setCurrentIndex(std::max(index, 0));
    if (index < 0 && session_ && !current.isEmpty()) {
        // The group went with its groom: back to the level.
        session_->settings().brush.group.clear();
    }
}

void BrushPanel::refresh() {
    if (!session_) return;
    updating_ = true;
    for (const auto& r : refreshers_) r();
    updating_ = false;
}

void BrushPanel::setMode(const QString& name) {
    if (!session_) return;
    try {
        session_->setTool(name.toStdString());
    } catch (const std::invalid_argument&) {
        return;
    }
    refresh();
    emit settingsChanged();
}

QString BrushPanel::mode() const { return session_ ? QString::fromStdString(session_->toolId()) : QString(); }

}  // namespace groomlab::app
