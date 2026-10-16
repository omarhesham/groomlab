// SPDX-License-Identifier: Apache-2.0
#include "export_panel.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QSpinBox>
#include <QVBoxLayout>

#include "theme.h"

namespace groomlab::app {

namespace {

struct TargetInfo {
    const char* name;
    const char* title;
    const char* extension;
    const char* description;
};
const TargetInfo kTargets[] = {
    {"alembic", "Alembic curves", "abc", "Every strand as Alembic curves with its ID, channels, color and root UV: Maya, Houdini, "
                                         "Blender, Katana and any renderer read it."},
    {"usd", "USD curves", "usda", "BasisCurves in a text layer with the same primvars; usdview, Maya, Houdini and Solaris open it."},
    {"unreal", "Unreal groom", "abc", "Unreal Engine's groom importer: the strands with the primary guides it simulates, each groom "
                                      "group its own object with groom_* attributes. Import as a Groom."},
    {"cards", "Cards (glTF)", "glb", "Hair cards baked at this frame as glTF with the maps embedded (base color and coverage, "
                                     "occlusion), plus the hair cap. Unreal, Unity, Blender and Godot import it; FBX is not written "
                                     "(the Maya host writes FBX through Maya)."},
    {"unity", "Unity strands", "abc", "Alembic curves for Unity's hair package: root UVs per point, every root width positive, "
                                      "resampled to a fixed point count, by strand group."},
};

// A thin glass bar filling from the left.
class ProgressBar : public QWidget {
public:
    explicit ProgressBar(QWidget* parent) : QWidget(parent) { setFixedHeight(8); }
    void set(double fraction) {
        fraction_ = std::clamp(fraction, 0.0, 1.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(theme::track());
        painter.drawRoundedRect(rect(), 4.0, 4.0);
        if (fraction_ > 0.0) {
            painter.setBrush(theme::emerald());
            painter.drawRoundedRect(QRectF(0.0, 0.0, std::max(8.0, width() * fraction_), height()), 4.0, 4.0);
        }
    }

private:
    double fraction_ = 0.0;
};

QLabel* captionLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setFont(theme::uiFont(12, 700));
    label->setStyleSheet(QStringLiteral("color: #8F8B84;"));
    return label;
}

}  // namespace

const char* ExportPanel::targetName(Target target) { return kTargets[static_cast<int>(target)].name; }

bool ExportPanel::targetFromName(const std::string& name, Target* target) {
    for (int i = 0; i < static_cast<int>(Target::Count); ++i) {
        if (name == kTargets[i].name) {
            *target = static_cast<Target>(i);
            return true;
        }
    }
    return false;
}

ExportPanel::ExportPanel(QWidget* parent) : GlassPanel(parent, 24.0) {
    setFixedWidth(316);
    // The rows scroll when the panel is placed shorter than them (a small screen, large text).
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* body = new QWidget;
    outer->addWidget(new FitScrollArea(body, this));
    auto* layout = new QVBoxLayout(body);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* title = new QHBoxLayout();
    title->setSpacing(10);
    auto* dot = new QLabel(this);
    dot->setFixedSize(9, 9);
    dot->setStyleSheet(QStringLiteral("background: %1; border-radius: 4px;").arg(theme::emerald().name()));
    title->addWidget(dot);
    auto* titleLabel = new QLabel(tr("Export"), this);
    titleLabel->setFont(theme::uiFont(15, 800));
    titleLabel->setStyleSheet(QStringLiteral("color: #F3F1EC;"));
    title->addWidget(titleLabel, 1);
    layout->addLayout(title);

    // ---- The targets as a column of pills (radio-like).
    auto* targetGrid = new QGridLayout();
    targetGrid->setSpacing(6);
    for (int i = 0; i < static_cast<int>(Target::Count); ++i) {
        auto* b = new RoundButton(this);
        b->setText(tr(kTargets[i].title));
        b->setDiameter(32);
        b->setRadius(11.0);
        b->setPadding(12);
        b->setFontSize(13);
        b->setFontWeight(700);
        b->setFill(theme::chip(), theme::emerald());
        b->setTextColor(theme::textSoft(), theme::ink());
        b->setCheckable(true);
        b->setToolTip(tr(kTargets[i].description));
        connect(b, &QAbstractButton::clicked, this, [this, i] {
            setTarget(static_cast<Target>(i));
        });
        targets_.push_back(b);
        targetGrid->addWidget(b, i / 2, i % 2);
    }
    layout->addLayout(targetGrid);
    description_ = new QLabel(this);
    description_->setWordWrap(true);
    description_->setFont(theme::uiFont(12, 500));
    description_->setStyleSheet(QStringLiteral("color: #B8B4AC;"));
    layout->addWidget(description_);

    // ---- The frames and the density.
    auto* frames = new QGridLayout();
    frames->setHorizontalSpacing(8);
    frames->setVerticalSpacing(6);
    range_ = new QCheckBox(tr("Frame range"), this);
    range_->setToolTip(tr("Off: the current frame alone (a static groom); on: every frame of the range (a cache)"));
    frames->addWidget(range_, 0, 0, 1, 4);
    auto spin = [this](int min, int max, const QString& tip) {
        auto* s = new QSpinBox(this);
        s->setRange(min, max);
        s->setFont(theme::monoFont(12));
        s->setButtonSymbols(QAbstractSpinBox::NoButtons);
        s->setToolTip(tip);
        return s;
    };
    first_ = spin(-100000, 1000000, tr("The first frame"));
    last_ = spin(-100000, 1000000, tr("The last frame"));
    step_ = new QDoubleSpinBox(this);
    step_->setRange(0.01, 1000.0);
    step_->setDecimals(2);
    step_->setValue(1.0);
    step_->setFont(theme::monoFont(12));
    step_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    step_->setToolTip(tr("Frames between samples (0.5: twice a frame)"));
    frames->addWidget(captionLabel(tr("From"), this), 1, 0);
    frames->addWidget(first_, 1, 1);
    frames->addWidget(captionLabel(tr("to"), this), 1, 2);
    frames->addWidget(last_, 1, 3);
    frames->addWidget(captionLabel(tr("Step"), this), 2, 0);
    frames->addWidget(step_, 2, 1);
    layout->addLayout(frames);
    density_ = new ValueSlider(this);
    density_->setStacked(true);
    density_->setCaption(tr("Density"));
    density_->setRange(0.01, 1.0);
    density_->setValue(1.0);
    density_->setFill(theme::yellow());
    density_->setFormat([](double v) { return QString::number(std::lround(v * 100.0)) + QStringLiteral("%"); });
    density_->setToolTip(tr("The share of the strands written (a lighter groom for a game's lower LOD)"));
    layout->addWidget(density_);

    // ---- Each target's own options. Unreal's and Unity's are the Maya host's export's
    // (engine_export.h): groom groups, the feathers' shafts and a lighter groom of barbs.
    engineOptions_ = new QWidget(this);
    auto* engineLayout = new QGridLayout(engineOptions_);
    engineLayout->setContentsMargins(0, 0, 0, 0);
    engineLayout->setHorizontalSpacing(8);
    engineLayout->setVerticalSpacing(6);
    groups_ = new QCheckBox(tr("Groom groups"), engineOptions_);
    groups_->setChecked(true);
    groups_->setToolTip(tr("Each groom group (the groupId channel) its own object: its own material, LODs and physics in Unreal, "
                           "a strand group in Unity"));
    engineLayout->addWidget(groups_, 0, 0, 1, 2);
    shafts_ = new QComboBox(engineOptions_);
    shafts_->addItem(tr("auto"), 0);
    shafts_->addItem(tr("as strands"), 1);
    shafts_->addItem(tr("left out"), 2);
    shafts_->setToolTip(tr("The feathers' shafts: auto leaves them out when the feathers have a rachis mesh (it is their geometry)"));
    engineLayout->addWidget(captionLabel(tr("Shafts"), engineOptions_), 1, 0);
    engineLayout->addWidget(shafts_, 1, 1);
    barbDensity_ = new QDoubleSpinBox(engineOptions_);
    barbDensity_->setRange(0.01, 1.0);
    barbDensity_->setDecimals(2);
    barbDensity_->setSingleStep(0.05);
    barbDensity_->setValue(1.0);
    barbDensity_->setFont(theme::monoFont(12));
    barbDensity_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    barbDensity_->setToolTip(tr("The share of the feathers' barbs written (the same barbs every frame): a lighter groom"));
    engineLayout->addWidget(captionLabel(tr("Barb density"), engineOptions_), 2, 0);
    engineLayout->addWidget(barbDensity_, 2, 1);
    barbWidth_ = new QDoubleSpinBox(engineOptions_);
    barbWidth_->setRange(0.01, 100.0);
    barbWidth_->setDecimals(2);
    barbWidth_->setValue(1.0);
    barbWidth_->setFont(theme::monoFont(12));
    barbWidth_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    barbWidth_->setToolTip(tr("Each barb kept this many times as wide, so the vanes keep their coverage"));
    engineLayout->addWidget(captionLabel(tr("Barb width"), engineOptions_), 3, 0);
    engineLayout->addWidget(barbWidth_, 3, 1);
    layout->addWidget(engineOptions_);

    unrealOptions_ = new QWidget(this);
    auto* unrealLayout = new QHBoxLayout(unrealOptions_);
    unrealLayout->setContentsMargins(0, 0, 0, 0);
    unrealLayout->setSpacing(12);
    guides_ = new QCheckBox(tr("Guides"), unrealOptions_);
    guides_->setChecked(true);
    guides_->setToolTip(tr("The primary guides as the guides Unreal simulates"));
    featherGuides_ = new QCheckBox(tr("Feather guides"), unrealOptions_);
    featherGuides_->setToolTip(tr("A feather group's guides are its shafts (with groom groups)"));
    iridescenceAo_ = new QCheckBox(tr("Iridescence as AO"), unrealOptions_);
    iridescenceAo_->setToolTip(tr("FeatherColor's iridescence in groom_ao too, where Unreal's Hair Attributes node gives it to a material"));
    unrealLayout->addWidget(guides_);
    unrealLayout->addWidget(featherGuides_);
    unrealLayout->addWidget(iridescenceAo_);
    unrealLayout->addStretch(1);
    layout->addWidget(unrealOptions_);

    unityOptions_ = new QWidget(this);
    auto* unityLayout = new QGridLayout(unityOptions_);
    unityLayout->setContentsMargins(0, 0, 0, 0);
    unityLayout->setHorizontalSpacing(8);
    unityLayout->setVerticalSpacing(6);
    scale_ = new QDoubleSpinBox(unityOptions_);
    scale_->setRange(0.0001, 1000.0);
    scale_->setDecimals(4);
    scale_->setValue(1.0);
    scale_->setFont(theme::monoFont(12));
    scale_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    scale_->setToolTip(tr("Points and widths times this (1 keeps centimeters, what Unity's importer expects; 0.01 writes meters)"));
    points_ = new QSpinBox(unityOptions_);
    points_->setRange(0, 128);
    points_->setValue(0);
    points_->setSpecialValueText(tr("as they are"));
    points_->setFont(theme::monoFont(12));
    points_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    points_->setToolTip(tr("Every strand resampled to this many points (3 to 128), so Unity can leave Resample Curves off"));
    unityLayout->addWidget(captionLabel(tr("Scale"), unityOptions_), 0, 0);
    unityLayout->addWidget(scale_, 0, 1);
    unityLayout->addWidget(captionLabel(tr("Points per strand"), unityOptions_), 1, 0);
    unityLayout->addWidget(points_, 1, 1);
    barbPoints_ = new QSpinBox(unityOptions_);
    barbPoints_->setRange(0, 128);
    barbPoints_->setValue(0);
    barbPoints_->setSpecialValueText(tr("as the rest"));
    barbPoints_->setFont(theme::monoFont(12));
    barbPoints_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    barbPoints_->setToolTip(tr("The feather groups' strands resampled to this many points (with groom groups)"));
    unityLayout->addWidget(captionLabel(tr("Barb points"), unityOptions_), 2, 0);
    unityLayout->addWidget(barbPoints_, 2, 1);
    layout->addWidget(unityOptions_);

    cardOptions_ = new QWidget(this);
    auto* cardLayout = new QVBoxLayout(cardOptions_);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(8);
    auto* resolutionRow = new QHBoxLayout();
    resolutionRow->setSpacing(8);
    resolutionRow->addWidget(captionLabel(tr("Resolution"), cardOptions_));
    resolution_ = new QComboBox(cardOptions_);
    resolution_->addItem(tr("the node's"), 0);
    for (const int size : {512, 1024, 2048, 4096}) resolution_->addItem(QStringLiteral("%1").arg(size), size);
    resolutionRow->addWidget(resolution_, 1);
    binary_ = new QCheckBox(tr("Binary .glb"), cardOptions_);
    binary_->setChecked(true);
    binary_->setToolTip(tr("One .glb with the buffer and images inside; off, a .gltf with its .bin and PNGs beside it"));
    resolutionRow->addWidget(binary_);
    cardLayout->addLayout(resolutionRow);
    occlusion_ = new ValueSlider(cardOptions_);
    occlusion_->setStacked(true);
    occlusion_->setCaption(tr("Occlusion"));
    occlusion_->setRange(0.0, 1.0);
    occlusion_->setValue(1.0);
    occlusion_->setFill(theme::purple());
    occlusion_->setFormat([](double v) { return QString::number(v, 'f', 2); });
    cardLayout->addWidget(occlusion_);
    layout->addWidget(cardOptions_);

    // ---- The output and the button.
    auto* pathRow = new QHBoxLayout();
    pathRow->setSpacing(6);
    path_ = new QLineEdit(this);
    path_->setPlaceholderText(tr("Output file"));
    path_->setFont(theme::monoFont(12));
    pathRow->addWidget(path_, 1);
    browse_ = new RoundButton(this);
    browse_->setText(QStringLiteral("..."));
    browse_->setDiameter(28);
    browse_->setRadius(9.0);
    browse_->setPadding(10);
    browse_->setFontSize(13);
    browse_->setFill(theme::chip(), theme::chip());
    browse_->setToolTip(tr("Choose the output file"));
    pathRow->addWidget(browse_);
    layout->addLayout(pathRow);
    export_ = new RoundButton(this);
    export_->setText(tr("Export"));
    export_->setIcon(QStringLiteral(":/groomlab/icons/ui_export.svg"), 16);
    export_->setIconColor(theme::ink(), theme::ink());
    export_->setDiameter(38);
    export_->setRadius(13.0);
    export_->setPadding(16);
    export_->setFontSize(14);
    export_->setFill(theme::emerald(), theme::emerald());
    export_->setTextColor(theme::ink(), theme::ink());
    layout->addWidget(export_);
    progressBar_ = new ProgressBar(this);
    progressBar_->hide();
    layout->addWidget(progressBar_);
    status_ = new QLabel(this);
    status_->setWordWrap(true);
    status_->setFont(theme::uiFont(12, 500));
    status_->setStyleSheet(QStringLiteral("color: #B8B4AC;"));
    layout->addWidget(status_);

    // ---- Save, as before.
    auto* saveRow = new QHBoxLayout();
    saveRow->setSpacing(8);
    auto saveButton = [this](const QString& text, const QString& dim) {
        auto* b = new RoundButton(this);
        b->setText(text);
        b->setDimText(dim);
        b->setDiameter(32);
        b->setRadius(11.0);
        b->setPadding(12);
        b->setFontSize(13);
        b->setFill(theme::chip(), theme::chip());
        b->setTextColor(theme::text(), theme::text());
        return b;
    };
    RoundButton* save = saveButton(tr("Save"), QStringLiteral("Ctrl+S"));
    RoundButton* saveAs = saveButton(tr("Save as..."), QStringLiteral("Ctrl+Shift+S"));
    saveRow->addWidget(save);
    saveRow->addWidget(saveAs);
    saveRow->addStretch(1);
    layout->addLayout(saveRow);
    connect(save, &QAbstractButton::clicked, this, &ExportPanel::saveRequested);
    connect(saveAs, &QAbstractButton::clicked, this, &ExportPanel::saveAsRequested);

    connect(browse_, &QAbstractButton::clicked, this, &ExportPanel::browseRequested);
    connect(export_, &QAbstractButton::clicked, this, [this] {
        if (busy_) emit cancelRequested();
        else emit exportRequested();
    });
    connect(range_, &QCheckBox::toggled, this, [this] { refresh(); });
    connect(binary_, &QCheckBox::toggled, this, [this] { setTarget(target_); });  // the extension follows
    setTarget(Target::Alembic);
}

void ExportPanel::setTarget(Target target) {
    target_ = target;
    // The path keeps its stem and takes the target's extension.
    const QString path = path_->text();
    if (!path.isEmpty()) {
        const QFileInfo info(path);
        const QString base = info.completeBaseName();
        QString stem = base;
        for (const TargetInfo& t : kTargets) {
            const QString suffix = QStringLiteral("_") + QLatin1String(t.name);
            if (stem.endsWith(suffix)) stem.chop(suffix.size());
        }
        const QString dir = info.path();
        path_->setText((dir.isEmpty() || dir == QStringLiteral(".") ? QString() : dir + QStringLiteral("/")) + stem + QStringLiteral("_") +
                       QLatin1String(targetName(target)) + QStringLiteral(".") + defaultExtension());
    }
    refresh();
}

QString ExportPanel::defaultExtension() const {
    if (target_ == Target::Cards && !binary_->isChecked()) return QStringLiteral("gltf");
    return QLatin1String(kTargets[static_cast<int>(target_)].extension);
}

ExportOptions ExportPanel::options() const {
    ExportOptions o;
    const bool range = range_->isChecked() && range_->isEnabled();
    o.frames.first = range ? first_->value() : frame_;
    o.frames.last = range ? std::max(first_->value(), last_->value()) : frame_;
    o.frames.step = range ? step_->value() : 1.0;
    o.density = static_cast<float>(density_->value());
    o.groups = groups_->isChecked();
    o.guides = guides_->isChecked();
    const int shafts = shafts_->currentData().toInt();
    if (shafts != 0) o.shafts = shafts == 1;
    o.featherGuides = featherGuides_->isChecked();
    o.iridescenceAo = iridescenceAo_->isChecked();
    o.barbDensity = static_cast<float>(barbDensity_->value());
    o.barbWidthScale = static_cast<float>(barbWidth_->value());
    o.barbPoints = barbPoints_->value();
    o.scale = static_cast<float>(scale_->value());
    o.pointsPerStrand = points_->value();
    o.resolution = resolution_->currentData().toInt();
    o.occlusion = static_cast<float>(occlusion_->value());
    o.binary = binary_->isChecked();
    return o;
}

QString ExportPanel::outputPath() const { return path_->text().trimmed(); }

void ExportPanel::setOutputPath(const QString& path) { path_->setText(path); }

void ExportPanel::setDocumentState(bool open, double frame, bool animated, double first, double last, const QString& groomPath) {
    updating_ = true;
    frame_ = frame;
    range_->setEnabled(open && animated);
    if (!animated) range_->setChecked(false);
    first_->setValue(static_cast<int>(std::lround(first)));
    last_->setValue(static_cast<int>(std::lround(last)));
    export_->setEnabled(open);
    // The output beside the groom: "<stem>_<target>.<ext>" (an unsaved groom: the user picks).
    const QString stem = groomPath.isEmpty() ? QString() : QFileInfo(groomPath).completeBaseName();
    if (stem != groomStem_) {
        groomStem_ = stem;
        if (!stem.isEmpty()) {
            path_->setText(QFileInfo(groomPath).path() + QStringLiteral("/") + stem + QStringLiteral("_") + QLatin1String(targetName(target_)) +
                           QStringLiteral(".") + defaultExtension());
        }
    }
    updating_ = false;
    refresh();
}

void ExportPanel::setProgress(std::size_t done, std::size_t count) {
    done_ = done;
    count_ = count;
    static_cast<ProgressBar*>(progressBar_)->set(count > 0 ? static_cast<double>(done) / static_cast<double>(count) : 0.0);
    progressBar_->setVisible(busy_ || done > 0);
    if (busy_) status_->setText(tr("Frame %1 of %2...").arg(done).arg(count));
}

void ExportPanel::setBusy(bool busy) {
    busy_ = busy;
    export_->setText(busy ? tr("Cancel") : tr("Export"));
    export_->setFill(busy ? theme::chip() : theme::emerald(), busy ? theme::chip() : theme::emerald());
    export_->setTextColor(busy ? theme::text() : theme::ink(), busy ? theme::text() : theme::ink());
    progressBar_->setVisible(busy);
    for (RoundButton* b : targets_) b->setEnabled(!busy);
    if (busy) static_cast<ProgressBar*>(progressBar_)->set(0.0);
}

void ExportPanel::setStatus(const QString& text, bool error) {
    status_->setText(text);
    status_->setStyleSheet(error ? QStringLiteral("color: #FF7A7C;") : QStringLiteral("color: #B8B4AC;"));
}

void ExportPanel::refresh() {
    for (int i = 0; i < static_cast<int>(targets_.size()); ++i) targets_[static_cast<std::size_t>(i)]->setChecked(i == static_cast<int>(target_));
    description_->setText(tr(kTargets[static_cast<int>(target_)].description));
    const bool cards = target_ == Target::Cards;
    const bool range = range_->isChecked() && range_->isEnabled() && !cards;
    range_->setVisible(!cards);
    first_->setEnabled(range);
    last_->setEnabled(range);
    step_->setEnabled(range);
    engineOptions_->setVisible(target_ == Target::Unreal || target_ == Target::Unity);
    unrealOptions_->setVisible(target_ == Target::Unreal);
    unityOptions_->setVisible(target_ == Target::Unity);
    cardOptions_->setVisible(cards);
    density_->setVisible(!cards);
    adjustSize();
}

}  // namespace groomlab::app
