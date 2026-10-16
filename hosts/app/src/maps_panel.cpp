// SPDX-License-Identifier: Apache-2.0
#include "maps_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <nlohmann/json.hpp>

#include "brush_session.h"
#include "glass.h"
#include "groom_document.h"
#include "groomlab/stroke.h"
#include "param_editors.h"
#include "theme.h"

namespace groomlab::app {

namespace {

RoundButton* actionButton(const QString& text, const QColor& fill, const QColor& textColor, QWidget* parent) {
    auto* b = new RoundButton(parent);
    b->setText(text);
    b->setDiameter(30);
    b->setRadius(10.0);
    b->setPadding(12);
    b->setFontSize(12);
    b->setFontWeight(700);
    b->setFill(fill, fill);
    b->setTextColor(textColor, textColor);
    return b;
}

QString kindTitle(const std::string& kind) {
    if (kind == "vertices") return MapsPanel::tr("vertex map");
    if (kind == "texture") return MapsPanel::tr("texture map");
    return MapsPanel::tr("guide layer");
}

// The name rule every host keeps (scalp_maps.h validMapName), so a map made here reads in Maya.
QString nameRule() {
    return MapsPanel::tr("A map's name: letters, digits and _, not starting with a digit (as Maya names them)");
}

}  // namespace

MapsPanel::MapsPanel(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* host = new QWidget(scroll);
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    list_ = new QListWidget(host);
    list_->setFixedHeight(120);
    list_->setToolTip(tr("The groom's painted maps: vertex maps and texture maps on the scalp, guide layers on the guides"));
    layout->addWidget(list_);
    hint_ = new QLabel(tr("No painted maps yet: make one below, blank or for a setting (For: Density to paint hair away "
                          "with Ctrl; a preset makes the maps it reads)"),
                       host);
    hint_->setWordWrap(true);
    hint_->setStyleSheet(QStringLiteral("color: #8F8B84;"));
    layout->addWidget(hint_);
    connect(list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem*, QListWidgetItem*) {
        if (updating_) return;
        refreshRows();  // the paint and show rows follow the picked map (the list itself stays)
    });

    // ---- Rename and remove the picked map.
    auto* editRow = new QHBoxLayout();
    editRow->setSpacing(6);
    renameTo_ = new QLineEdit(host);
    renameTo_->setPlaceholderText(tr("new name"));
    renameTo_->setToolTip(nameRule());
    editRow->addWidget(renameTo_, 1);
    RoundButton* rename = actionButton(tr("Rename"), theme::chip(), theme::text(), host);
    RoundButton* remove = actionButton(tr("Remove"), theme::chip(), theme::redLight(), host);
    editRow->addWidget(rename);
    editRow->addWidget(remove);
    layout->addLayout(editRow);
    connect(rename, &QAbstractButton::clicked, this, [this] {
        const std::string error = renameSelected(renameTo_->text().trimmed());
        if (!error.empty()) emit message(QString::fromStdString(error));
    });
    connect(remove, &QAbstractButton::clicked, this, [this] {
        const std::string error = removeSelected();
        if (!error.empty()) emit message(QString::fromStdString(error));
    });

    // ---- A new map.
    auto* newBox = new QGroupBox(tr("New map"), host);
    auto* newForm = new QFormLayout(newBox);
    newFor_ = new QComboBox(newBox);
    newFor_->setObjectName(QStringLiteral("newMapFor"));
    newFor_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    newFor_->setMinimumContentsLength(12);
    newFor_->setToolTip(tr("A map for a setting is made white (the setting as it is), linked to it and painted at once, "
                           "one undo step: strokes paint toward the value, Ctrl toward 0 (on Density: no hair). When the "
                           "setting reads a map already, that map is picked instead."));
    newForm->addRow(tr("For"), newFor_);
    newOther_ = new QComboBox(newBox);
    newOther_->setObjectName(QStringLiteral("newMapOther"));
    newOther_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    newOther_->setMinimumContentsLength(12);
    newOther_->setToolTip(tr("Every mappable param of the graph"));
    newForm->addRow(tr("Setting"), newOther_);
    connect(newFor_, &QComboBox::currentIndexChanged, this, [this](int) { targetChanged(); });
    newName_ = new QLineEdit(newBox);
    newName_->setPlaceholderText(tr("density, mask, tint..."));
    newName_->setToolTip(nameRule());
    newForm->addRow(tr("Name"), newName_);
    newKind_ = new QComboBox(newBox);
    newKind_->addItem(tr("Vertex map (per scalp vertex)"), QStringLiteral("vertices"));
    newKind_->addItem(tr("Texture map (an image at the UVs)"), QStringLiteral("texture"));
    newKind_->addItem(tr("Guide layer (per guide)"), QStringLiteral("guides"));
    // The long names must not widen the panel: the combo fits its box, the popup shows them whole.
    newKind_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    newKind_->setMinimumContentsLength(12);
    newKind_->setToolTip(tr("Where the map lives: the scalp's vertices (saved with the groom), an image file beside the "
                            "groom (the finest detail; the scalp needs UVs), or the guides (children inherit)"));
    newForm->addRow(tr("Kind"), newKind_);
    newColor_ = new QCheckBox(tr("color (r g b a)"), newBox);
    newColor_->setToolTip(tr("A color map: painted hair color, or up to four masks packed in its channels"));
    newForm->addRow(QString(), newColor_);
    newValue_ = new ValueSlider(newBox);
    newValue_->setStacked(true);
    newValue_->setCaption(tr("Fill"));
    newValue_->setRange(0.0, 1.0);
    newValue_->setLimits(0.0, 1.0);
    newValue_->setDecimals(3);
    newValue_->setValue(1.0);  // a mask that lets everything through until painted (Maya's default too)
    newValue_->setFill(theme::yellow());
    newValue_->setToolTip(tr("What the map starts filled with"));
    newForm->addRow(newValue_);
    newResolution_ = new QSpinBox(newBox);
    newResolution_->setRange(16, 8192);
    newResolution_->setSingleStep(256);
    newResolution_->setValue(1024);
    newResolution_->setToolTip(tr("A texture map's image size (square)"));
    newForm->addRow(tr("Resolution"), newResolution_);
    RoundButton* create = actionButton(tr("Create"), theme::yellow(), theme::ink(), newBox);
    newForm->addRow(QString(), create);
    layout->addWidget(newBox);
    connect(newKind_, &QComboBox::currentIndexChanged, this, [this](int) {
        const bool texture = newKind_->currentData().toString() == QStringLiteral("texture");
        newResolution_->setEnabled(texture);
        newColor_->setEnabled(newKind_->currentData().toString() != QStringLiteral("guides"));
    });
    newResolution_->setEnabled(false);
    connect(create, &QAbstractButton::clicked, this, [this] {
        const std::string error = createFromFields();
        if (!error.empty()) emit message(QString::fromStdString(error));
    });
    refreshTargets();

    // ---- Painting the picked map.
    auto* paintBox = new QGroupBox(tr("Paint"), host);
    auto* paintForm = new QFormLayout(paintBox);
    paintValue_ = new ValueSlider(paintBox);
    paintValue_->setStacked(true);
    paintValue_->setCaption(tr("Value"));
    paintValue_->setRange(0.0, 1.0);
    paintValue_->setLimits(0.0, 1.0);
    paintValue_->setDecimals(3);
    paintValue_->setValue(1.0);
    paintValue_->setFill(theme::yellow());
    paintValue_->setToolTip(tr("The value a gray map is painted toward (Ctrl: toward 0, or toward 1 when the value is 0; "
                               "on a density map, Ctrl removes hair)"));
    paintForm->addRow(paintValue_);
    colorRow_ = new QWidget(paintBox);
    auto* colorLayout = new QVBoxLayout(colorRow_);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->setSpacing(4);
    paintColor_ = new ColorEditor(colorRow_);
    paintColor_->setToolTip(tr("The color a color map is painted toward"));
    colorLayout->addWidget(paintColor_);
    auto* channelsRow = new QHBoxLayout();
    channelsRow->setContentsMargins(0, 0, 0, 0);
    channelsRow->setSpacing(8);
    paintAlpha_ = new ValueSlider(colorRow_);
    paintAlpha_->setCaption(tr("a"));
    paintAlpha_->setRange(0.0, 1.0);
    paintAlpha_->setLimits(0.0, 1.0);
    paintAlpha_->setDecimals(2);
    paintAlpha_->setValue(1.0);
    paintAlpha_->setTrackWidth(60);
    paintAlpha_->setValueWidth(34);
    paintAlpha_->setFill(theme::yellow());
    paintAlpha_->setToolTip(tr("The alpha painted toward"));
    channelsRow->addWidget(paintAlpha_);
    for (const char* c : {"r", "g", "b", "a"}) {
        auto* box = new QCheckBox(QLatin1String(c), colorRow_);
        box->setChecked(true);
        box->setToolTip(tr("Paint this channel (the others keep their values: packed masks)"));
        channelsRow->addWidget(box);
        paintChannels_.push_back(box);
        connect(box, &QCheckBox::toggled, this, [this] { applyPaintSettings(); });
    }
    channelsRow->addStretch(1);
    colorLayout->addLayout(channelsRow);
    paintForm->addRow(tr("Color"), colorRow_);
    paint_ = actionButton(tr("Paint this map"), theme::yellow(), theme::ink(), paintBox);
    paint_->setToolTip(tr("The Paint brush on this map, the viewport in brush mode (B): drag to paint, Ctrl the opposite, "
                          "Shift smooths"));
    paintForm->addRow(QString(), paint_);
    layout->addWidget(paintBox);
    connect(paintValue_, &ValueSlider::valueChanged, this, [this] { applyPaintSettings(); });
    connect(paintAlpha_, &ValueSlider::valueChanged, this, [this] { applyPaintSettings(); });
    connect(paintColor_, &ColorEditor::edited, this, [this](bool) { applyPaintSettings(); });
    connect(paint_, &QAbstractButton::clicked, this, [this] { paintSelected(); });

    // ---- Showing the picked map on the scalp.
    auto* showRow = new QHBoxLayout();
    showRow->setSpacing(8);
    show_ = new QCheckBox(tr("Show on scalp"), host);
    show_->setToolTip(tr("The map drawn on the scalp (a vertex map's values, a guide layer blended between the guides, a "
                         "texture at the scalp's vertices)"));
    showRow->addWidget(show_);
    channel_ = new QComboBox(host);
    channel_->addItems({QStringLiteral("rgb"), QStringLiteral("r"), QStringLiteral("g"), QStringLiteral("b"), QStringLiteral("a")});
    channel_->setToolTip(tr("A color map shown in color, or one channel as gray"));
    showRow->addWidget(channel_);
    showRow->addStretch(1);
    layout->addLayout(showRow);
    connect(show_, &QCheckBox::toggled, this, [this](bool on) {
        if (updating_) return;
        showSelected(on);
    });
    connect(channel_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (updating_ || !show_->isChecked()) return;
        showSelected(true);
    });
    layout->addStretch(1);

    scroll->setWidget(host);
    outer->addWidget(scroll);
    setEnabled(false);
}

void MapsPanel::setDocument(GroomDocument* document) {
    document_ = document;
    setEnabled(document_ != nullptr);
    refresh();
}

void MapsPanel::setSession(BrushSession* session) { session_ = session; }

std::string MapsPanel::selectedMap() const {
    return list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toString().toStdString() : std::string();
}

void MapsPanel::selectMap(const std::string& name) {
    for (int i = 0; i < list_->count(); ++i) {
        if (list_->item(i)->data(Qt::UserRole).toString().toStdString() == name) {
            list_->setCurrentRow(i);
            return;
        }
    }
}

void MapsPanel::refresh() {
    updating_ = true;
    const std::string current = selectedMap();
    list_->clear();
    if (document_) {
        for (const PaintedMap& m : document_->maps()) {
            auto* item = new QListWidgetItem(QStringLiteral("%1   %2%3").arg(QString::fromStdString(m.name), kindTitle(m.kind),
                                                                            m.channels == kColorMap ? tr(", color") : QString()),
                                             list_);
            item->setData(Qt::UserRole, QString::fromStdString(m.name));
            if (m.kind == "texture") item->setToolTip(QString::fromStdString(m.texture));
            if (m.name == current) list_->setCurrentItem(item);
        }
        if (!list_->currentItem() && list_->count() > 0) list_->setCurrentRow(0);
    }
    hint_->setVisible(list_->count() == 0);
    updating_ = false;
    refreshTargets();
    refreshRows();
}

void MapsPanel::refreshTargets() {
    // The common settings (core's list), each enabled while the graph has a node of its type; then
    // Other: every mappable param of the graph. The choices kept across refreshes.
    const QVariant wasFor = newFor_->currentData(), wasOther = newOther_->currentData();
    {
        const QSignalBlocker blockFor(newFor_), blockOther(newOther_);
        newFor_->clear();
        newOther_->clear();
        newFor_->addItem(tr("Nothing yet (a blank map)"));
        std::string graphJson;
        if (document_) graphJson = document_->graph().dump();
        for (const PaintableSetting& s : paintableSettings()) {
            std::string node;
            try {
                if (document_) node = firstNodeOfType(graphJson, s.type);
            } catch (const std::exception&) {
            }
            newFor_->addItem(tr("Setting: %1").arg(QString::fromUtf8(s.label)),
                             node.empty() ? QVariant() : QVariant(QStringList{QString::fromStdString(node), QString::fromUtf8(s.param)}));
            const int row = newFor_->count() - 1;
            newFor_->setItemData(row, node.empty() ? tr("Add a %1 node first").arg(QString::fromUtf8(s.type)) : QString::fromUtf8(s.hint),
                                 Qt::ToolTipRole);
            if (node.empty()) newFor_->setItemData(row, 0, Qt::UserRole - 1);  // disabled: the graph has none
        }
        if (document_) {
            for (const nlohmann::json& n : document_->graph().value("nodes", nlohmann::json::array())) {
                const nlohmann::json* spec = document_->spec(n.value("type", std::string()));
                if (!spec || !spec->contains("params")) continue;
                for (const nlohmann::json& p : (*spec)["params"]) {
                    if (!p.value("mappable", false)) continue;
                    const QString node = QString::fromStdString(n.value("name", std::string()));
                    const QString param = QString::fromStdString(p.value("name", std::string()));
                    newOther_->addItem(node + QLatin1Char('.') + param, QStringList{node, param});
                    newOther_->setItemData(newOther_->count() - 1, QString::fromStdString(p.value("description", std::string())),
                                           Qt::ToolTipRole);
                }
            }
        }
        if (newOther_->count() > 0) newFor_->addItem(tr("Other setting..."), QStringLiteral("other"));
        const int f = wasFor.isValid() ? newFor_->findData(wasFor) : 0;
        newFor_->setCurrentIndex(std::max(0, f));
        if (wasOther.isValid()) newOther_->setCurrentIndex(std::max(0, newOther_->findData(wasOther)));
    }
    targetChanged();
}

void MapsPanel::targetChanged() {
    const bool other = newFor_->currentData().toString() == QStringLiteral("other");
    newOther_->setVisible(other);
    if (auto* form = qobject_cast<QFormLayout*>(newOther_->parentWidget()->layout())) {
        if (QWidget* label = form->labelForField(newOther_)) label->setVisible(other);
    }
    const bool forSetting = !newMapTarget().first.empty();
    // A setting's map is named for it (<node>_<param>), gray, and lives on the scalp.
    newName_->setEnabled(!forSetting);
    newColor_->setEnabled(!forSetting && newKind_->currentData().toString() != QStringLiteral("guides"));
    if (forSetting) {
        newColor_->setChecked(false);
        if (newKind_->currentData().toString() == QStringLiteral("guides")) newKind_->setCurrentIndex(newKind_->findData(QStringLiteral("vertices")));
    }
    newValue_->setEnabled(!forSetting);
}

bool MapsPanel::setNewMapTarget(const QString& node, const QString& param) {
    if (node.isEmpty()) {
        newFor_->setCurrentIndex(0);
        return true;
    }
    const QStringList wanted{node, param};
    int index = newFor_->findData(wanted);
    if (index < 0) {
        const int other = newOther_->findData(wanted);
        index = newFor_->findData(QStringLiteral("other"));
        if (other < 0 || index < 0) return false;
        newOther_->setCurrentIndex(other);
    }
    newFor_->setCurrentIndex(index);
    targetChanged();
    return true;
}

std::pair<std::string, std::string> MapsPanel::newMapTarget() const {
    QVariant data = newFor_->currentData();
    if (data.toString() == QStringLiteral("other")) data = newOther_->currentData();
    const QStringList pair = data.toStringList();
    if (pair.size() != 2) return {};
    return {pair[0].toStdString(), pair[1].toStdString()};
}

void MapsPanel::setPaintValue(double value) { paintValue_->setValue(value); }

void MapsPanel::refreshRows() {
    updating_ = true;
    const std::string picked = document_ ? selectedMap() : std::string();
    if (document_) {
        const PaintedMap* map = document_->map(picked);
        const bool color = map && map->channels == kColorMap;
        colorRow_->setVisible(color);
        paintValue_->setVisible(!color);  // the slider carries its caption
        if (QWidget* label = static_cast<QFormLayout*>(colorRow_->parentWidget()->layout())->labelForField(colorRow_)) label->setVisible(color);
        show_->setChecked(!picked.empty() && document_->scalpMapDisplay() == picked);
        channel_->setEnabled(color);
        channel_->setCurrentText(QString::fromStdString(document_->scalpMapChannel()));
    }
    const bool has = !picked.empty();
    paint_->setEnabled(has);
    show_->setEnabled(has);
    renameTo_->setEnabled(has);
    updating_ = false;
}

void MapsPanel::setNewMapFields(const QString& name, const QString& kind, bool color, double value, int resolution) {
    newName_->setText(name);
    newKind_->setCurrentIndex(std::max(0, newKind_->findData(kind)));
    newColor_->setChecked(color);
    newValue_->setValue(value);
    newResolution_->setValue(resolution);
}

double MapsPanel::newMapFill() const { return newValue_->value(); }

bool MapsPanel::hintShown() const { return hint_->isVisibleTo(const_cast<MapsPanel*>(this)); }

std::string MapsPanel::createFromFields() {
    if (!document_) return "no groom open";
    const std::string kind = newKind_->currentData().toString().toStdString();
    const auto [node, param] = newMapTarget();
    if (!node.empty()) {  // for a setting: the host makes or finds it, links it, paints it
        emit paintParamRequested(node, param, kind);
        return {};
    }
    const std::string name = newName_->text().trimmed().toStdString();
    const bool color = newColor_->isChecked() && kind != "guides";
    const auto v = static_cast<float>(newValue_->value());
    const std::string error = document_->createMap(name, kind, color, v, newResolution_->value(), PaintColor{v, v, v, 1.0f});
    if (!error.empty()) return error;
    newName_->clear();
    refresh();
    selectMap(name);
    emit edited();
    return {};
}

std::string MapsPanel::renameSelected(const QString& to) {
    if (!document_) return "no groom open";
    const std::string from = selectedMap();
    if (from.empty()) return "pick a map to rename";
    const std::string error = document_->renameMap(from, to.toStdString());
    if (!error.empty()) return error;
    if (document_->scalpMapDisplay() == from) document_->setScalpMapDisplay(to.toStdString(), document_->scalpMapChannel());
    if (session_ && session_->settings().brush.attribute == from) session_->settings().brush.attribute = to.toStdString();
    renameTo_->clear();
    refresh();
    selectMap(to.toStdString());
    emit edited();
    return {};
}

std::string MapsPanel::removeSelected() {
    if (!document_) return "no groom open";
    const std::string name = selectedMap();
    if (name.empty()) return "pick a map to remove";
    const std::string error = document_->removeMap(name);
    if (!error.empty()) return error;
    if (document_->scalpMapDisplay() == name) {
        document_->setScalpMapDisplay({});
        emit displayChanged();
    }
    refresh();
    emit edited();
    return {};
}

void MapsPanel::applyPaintSettings() {
    if (!session_ || updating_) return;
    BrushToolSettings& s = session_->settings();
    s.brush.value = static_cast<float>(paintValue_->value());
    const auto rgb = paintColor_->linear();
    s.paintColor = {rgb[0], rgb[1], rgb[2], static_cast<float>(paintAlpha_->value())};
    unsigned mask = 0;
    for (std::size_t i = 0; i < paintChannels_.size(); ++i) {
        if (paintChannels_[i]->isChecked()) mask |= 1u << i;
    }
    s.paintChannels = mask == 0 ? kAllChannels : mask;
    emit brushChanged();
}

bool MapsPanel::paintSelected() {
    if (!document_ || !session_) return false;
    const std::string name = selectedMap();
    const PaintedMap* map = document_->map(name);
    if (!map) {
        emit message(tr("Pick a map to paint"));
        return false;
    }
    BrushToolSettings& s = session_->settings();
    try {
        s.setMode("paint");
    } catch (const std::invalid_argument& e) {
        emit message(QString::fromUtf8(e.what()));
        return false;
    }
    s.brush.attribute = name;
    s.paintKind = map->kind;
    s.paintTexture = map->kind == "texture" ? document_->textureMapFile(name) : std::string();
    applyPaintSettings();
    emit paintRequested();
    return true;
}

void MapsPanel::showSelected(bool show) {
    if (!document_) return;
    const std::string name = selectedMap();
    document_->setScalpMapDisplay(show ? name : std::string(), channel_->currentText().toStdString());
    {
        const QSignalBlocker block(show_);
        show_->setChecked(show && !name.empty());
    }
    emit displayChanged();
}

}  // namespace groomlab::app
