// SPDX-License-Identifier: Apache-2.0
#include "correctives_panel.h"

#include <algorithm>
#include <cmath>

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "brush_session.h"
#include "glass.h"
#include "groom_document.h"
#include "groomlab/correctives.h"
#include "theme.h"

namespace groomlab::app {

namespace {

// A layer name not yet taken: base, base1, base2...
std::string uniqueLayerName(const Correctives& c, const std::string& base) {
    if (!c.layer(base)) return base;
    for (int k = 1;; ++k) {
        const std::string name = base + std::to_string(k);
        if (!c.layer(name)) return name;
    }
}

}  // namespace

CorrectivesPanel::CorrectivesPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);
    list_ = new QTreeWidget(this);
    list_->setHeaderLabels({tr("Layer"), tr("Weight"), tr("Frames")});
    list_->header()->setStretchLastSection(false);
    list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    list_->setRootIsDecorated(false);
    list_->setToolTip(tr("The corrective layers over the animated groom: untick to turn one off, double-click to rename"));
    layout->addWidget(list_, 1);
    connect(list_, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int column) {
        if (updating_ || !item) return;
        const std::string name = item->data(0, Qt::UserRole).toString().toStdString();
        if (column == 0) {
            const std::string text = item->text(0).toStdString();
            if (text != name && !renameLayer(name, text)) {
                updating_ = true;
                item->setText(0, QString::fromStdString(name));
                updating_ = false;
            }
            setLayerEnabled(name, item->checkState(0) == Qt::Checked);
        }
    });
    connect(list_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* item, QTreeWidgetItem*) {
        remove_->setEnabled(item != nullptr);
        weight_->setEnabled(item != nullptr);
        if (item) {
            const bool was = updating_;  // refresh may be the caller: its own quiet stays
            updating_ = true;
            weight_->setValue(item->data(1, Qt::UserRole).toDouble());
            updating_ = was;
        }
    });

    weight_ = new ValueSlider(this);
    weight_->setStacked(true);
    weight_->setCaption(tr("Weight"));
    weight_->setRange(0.0, 1.0);
    weight_->setLimits(0.0, 1.0);
    weight_->setDecimals(2);
    weight_->setFill(theme::purple());
    weight_->setToolTip(tr("How much of the current layer applies (its envelope's weight)"));
    layout->addWidget(weight_);
    connect(weight_, &ValueSlider::released, this, [this] {
        if (updating_) return;
        setLayerWeight(currentLayer(), static_cast<float>(weight_->value()));
    });

    auto* buttons = new QHBoxLayout();
    add_ = new QPushButton(tr("New layer"), this);
    add_->setToolTip(tr("A corrective on every frame"));
    fix_ = new QPushButton(tr("Fix this frame"), this);
    fix_->setToolTip(tr("A corrective full at the timeline's frame, fading in and out over six frames"));
    remove_ = new QPushButton(tr("Remove"), this);
    buttons->addWidget(add_);
    buttons->addWidget(fix_);
    buttons->addWidget(remove_);
    layout->addLayout(buttons);
    connect(add_, &QPushButton::clicked, this, [this] { addLayer(true); });
    connect(fix_, &QPushButton::clicked, this, [this] { addLayer(false); });
    connect(remove_, &QPushButton::clicked, this, [this] { removeLayer(currentLayer()); });

    auto* targetRow = new QHBoxLayout();
    auto* targetLabel = new QLabel(tr("Sculpt into"), this);
    target_box_ = new QComboBox(this);
    target_box_->setToolTip(tr("Where the brushes' strokes go: the guides themselves (the base groom), or a corrective layer at "
                               "the timeline's frame (a shot sculpt: the difference rides along with the motion)"));
    targetRow->addWidget(targetLabel);
    targetRow->addWidget(target_box_, 1);
    layout->addLayout(targetRow);
    connect(target_box_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_) return;
        const QString data = index <= 0 ? QString() : target_box_->itemData(index).toString();
        if (data.startsWith(QStringLiteral("node:"))) {
            setTargetNode(data.mid(5).toStdString());
        } else {
            setTargetLayer(data.toStdString());  // (no Sculpt node any more)
        }
    });
    clearNode_ = new QPushButton(tr("Clear Edits"), this);
    clearNode_->setToolTip(tr("Drop every stroke kept in the picked Sculpt node: its guides as it is fed them (one undo step)"));
    clearNode_->setVisible(false);
    layout->addWidget(clearNode_);
    connect(clearNode_, &QPushButton::clicked, this, [this] { clearNodeEdits(); });
    hint_ = new QLabel(tr("Strokes edit the guides."), this);
    hint_->setWordWrap(true);
    hint_->setFont(theme::uiFont(12, 500));
    hint_->setStyleSheet(QStringLiteral("color: #8F8B84;"));
    layout->addWidget(hint_);
    setEnabled(false);
}

void CorrectivesPanel::setDocument(GroomDocument* document) {
    document_ = document;
    target_.clear();
    setEnabled(document_ != nullptr);
    refresh();
    applyTarget();
}

void CorrectivesPanel::setSession(BrushSession* session) {
    session_ = session;
    applyTarget();
}

std::string CorrectivesPanel::currentLayer() const {
    QTreeWidgetItem* item = list_->currentItem();
    return item ? item->data(0, Qt::UserRole).toString().toStdString() : std::string();
}

std::vector<std::string> CorrectivesPanel::layerNames() const {
    std::vector<std::string> names;
    if (!document_) return names;
    try {
        for (const CorrectiveLayer& l : Correctives::fromJson(document_->storedText(StrokeField::Correctives)).layers) names.push_back(l.name);
    } catch (const std::exception&) {
    }
    return names;
}

void CorrectivesPanel::refresh() {
    updating_ = true;
    const std::string current = currentLayer();
    list_->clear();
    QTreeWidgetItem* select = nullptr;
    if (document_) {
        const double fps = document_->framesPerSecond();
        try {
            for (const CorrectiveLayer& l : Correctives::fromJson(document_->storedText(StrokeField::Correctives)).layers) {
                auto* item = new QTreeWidgetItem(list_);
                item->setText(0, QString::fromStdString(l.name));
                item->setData(0, Qt::UserRole, QString::fromStdString(l.name));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
                item->setCheckState(0, l.enabled ? Qt::Checked : Qt::Unchecked);
                item->setText(1, QString::number(std::lround(l.weight * 100.0f)) + QStringLiteral("%"));
                item->setData(1, Qt::UserRole, static_cast<double>(l.weight));
                QString frames;
                if (!l.weightCurve.empty()) frames = tr("%n key(s)", "", static_cast<int>(l.weightCurve.size()));
                else if (l.holdBefore && l.holdAfter) frames = tr("all frames");
                else frames = tr("frame %1 ± %2").arg(std::lround(l.time * fps)).arg(std::lround(std::max(l.fadeIn, l.fadeOut) * fps));
                item->setText(2, frames);
                item->setToolTip(0, tr("%n guide(s) with offsets", "", static_cast<int>(l.offsets.size())));
                if (l.name == current) select = item;
            }
        } catch (const std::exception&) {
        }
    }
    if (select) list_->setCurrentItem(select);
    else if (list_->topLevelItemCount() > 0) list_->setCurrentItem(list_->topLevelItem(0));
    remove_->setEnabled(list_->currentItem() != nullptr);
    weight_->setEnabled(list_->currentItem() != nullptr);
    if (list_->currentItem()) weight_->setValue(list_->currentItem()->data(1, Qt::UserRole).toDouble());
    // The target: the base groom, then every layer, then every Sculpt node; a layer gone takes the
    // target with it (a node gone took the document's).
    target_box_->clear();
    target_box_->addItem(tr("Base groom (the guides)"));
    bool found = false;
    for (int i = 0; i < list_->topLevelItemCount(); ++i) {
        const QString name = list_->topLevelItem(i)->data(0, Qt::UserRole).toString();
        target_box_->addItem(name, name);
        if (name.toStdString() == target_) {
            target_box_->setCurrentIndex(i + 1);
            found = true;
        }
    }
    if (!found && !target_.empty()) target_.clear();
    const std::string node = targetNode();
    if (document_) {
        for (const std::string& n : document_->sculptNodes()) {
            const std::size_t edited = document_->sculptedGuides(n);
            target_box_->addItem(edited ? tr("Node: %1 (%n guide(s) sculpted)", "", static_cast<int>(edited)).arg(QString::fromStdString(n))
                                        : tr("Node: %1").arg(QString::fromStdString(n)),
                                 QStringLiteral("node:") + QString::fromStdString(n));
            if (n == node) {
                target_box_->setCurrentIndex(target_box_->count() - 1);
                found = true;
            }
        }
    }
    if (!found) target_box_->setCurrentIndex(0);
    updating_ = false;
    applyTarget();
}

void CorrectivesPanel::setTargetLayer(const std::string& name) {
    if (document_ && !document_->sculptTarget().empty()) {  // the base groom or a layer: no Sculpt node
        document_->setSculptTarget({});
        target_ = name;
        updating_ = true;
        const int index = name.empty() ? 0 : target_box_->findData(QString::fromStdString(name));
        target_box_->setCurrentIndex(std::max(0, index));
        updating_ = false;
        applyTarget();
        emit targetChanged();
        emit edited();
        return;
    }
    if (name == target_) return;
    target_ = name;
    updating_ = true;
    const int index = name.empty() ? 0 : target_box_->findData(QString::fromStdString(name));
    target_box_->setCurrentIndex(std::max(0, index));
    updating_ = false;
    applyTarget();
    emit targetChanged();
}

std::string CorrectivesPanel::targetNode() const { return document_ ? document_->sculptTarget() : std::string(); }

void CorrectivesPanel::setTargetNode(const std::string& node) {
    if (!document_ || node == targetNode()) return;
    document_->setSculptTarget(node);
    if (!node.empty()) target_.clear();  // not a layer: the node's guides
    updating_ = true;
    const int index = node.empty() ? -1 : target_box_->findData(QStringLiteral("node:") + QString::fromStdString(node));
    if (index >= 0) target_box_->setCurrentIndex(index);
    updating_ = false;
    applyTarget();
    emit targetChanged();
    emit edited();  // the view draws the node's guides (or the guides again)
}

bool CorrectivesPanel::clearNodeEdits() {
    const std::string node = targetNode();
    if (!document_ || node.empty()) return false;
    const std::string error = document_->clearSculptEdits(node);
    if (!error.empty()) {
        emit message(tr("Cannot clear %1's edits: %2").arg(QString::fromStdString(node), QString::fromStdString(error)));
        return false;
    }
    refresh();
    emit edited();
    return true;
}

void CorrectivesPanel::applyTarget() {
    const bool shot = document_ && !target_.empty();
    const std::string node = targetNode();
    hint_->setText(!node.empty() ? tr("Strokes shape the guides node %1 hands on; they are kept in the node and follow "
                                      "the guides when what feeds it changes.").arg(QString::fromStdString(node))
                   : shot ? tr("Strokes sculpt the groom as shown at frame %1 into the layer %2.")
                                .arg(std::lround(document_->frame()))
                                .arg(QString::fromStdString(target_))
                          : tr("Strokes edit the guides."));
    if (clearNode_) clearNode_->setVisible(!node.empty());
    if (session_) session_->setShotSculpt(shot, document_ ? document_->time() : 0.0, document_ ? std::lround(document_->frame()) : 0, target_);
}

bool CorrectivesPanel::editLayers(const std::string& label, const std::function<void(Correctives&)>& edit) {
    if (!document_) return false;
    const std::string error = document_->editStoredText(label, StrokeField::Correctives, [&](std::string& text) {
        Correctives c = Correctives::fromJson(text);
        edit(c);
        text = c.layers.empty() ? std::string() : c.toJson();
    });
    if (!error.empty()) {
        emit message(tr("Cannot %1: %2").arg(QString::fromStdString(label), QString::fromStdString(error)));
        return false;
    }
    refresh();
    emit edited();
    return true;
}

bool CorrectivesPanel::addLayer(bool allFrames, double fadeFrames) {
    if (!document_) return false;
    const double seconds = document_->time();
    const long frame = std::lround(document_->frame());
    const double fade = std::max(0.0, fadeFrames) / document_->framesPerSecond();
    std::string made;
    const bool ok = editLayers("add corrective", [&](Correctives& c) {
        CorrectiveLayer layer;
        layer.name = made = uniqueLayerName(c, allFrames ? "layer" : "shot" + std::to_string(frame));
        layer.time = seconds;
        layer.holdBefore = layer.holdAfter = allFrames;
        layer.fadeIn = layer.fadeOut = allFrames ? 0.5 : fade;
        c.layers.push_back(std::move(layer));
    });
    if (ok) setTargetLayer(made);  // a new layer is what the next strokes sculpt into
    return ok;
}

bool CorrectivesPanel::removeLayer(const std::string& name) {
    if (name.empty()) return false;
    return editLayers("remove corrective", [&](Correctives& c) {
        const auto it = std::find_if(c.layers.begin(), c.layers.end(), [&](const CorrectiveLayer& l) { return l.name == name; });
        if (it == c.layers.end()) throw std::invalid_argument("no layer named " + name);
        c.layers.erase(it);
    });
}

bool CorrectivesPanel::setLayerWeight(const std::string& name, float weight) {
    if (name.empty()) return false;
    return editLayers("corrective weight", [&](Correctives& c) {
        CorrectiveLayer* l = c.layer(name);
        if (!l) throw std::invalid_argument("no layer named " + name);
        l->weight = std::clamp(weight, 0.0f, 1.0f);
    });
}

bool CorrectivesPanel::setLayerEnabled(const std::string& name, bool enabled) {
    if (name.empty()) return false;
    return editLayers(enabled ? "corrective on" : "corrective off", [&](Correctives& c) {
        CorrectiveLayer* l = c.layer(name);
        if (!l) throw std::invalid_argument("no layer named " + name);
        l->enabled = enabled;
    });
}

bool CorrectivesPanel::renameLayer(const std::string& name, const std::string& to) {
    if (name.empty() || to.empty() || name == to) return false;
    const bool retarget = target_ == name;  // the refresh after the edit drops a target that is gone
    const bool ok = editLayers("rename corrective", [&](Correctives& c) {
        if (c.layer(to)) throw std::invalid_argument("a layer named " + to + " exists");
        CorrectiveLayer* l = c.layer(name);
        if (!l) throw std::invalid_argument("no layer named " + name);
        l->name = to;
    });
    if (ok && retarget) setTargetLayer(to);
    return ok;
}

}  // namespace groomlab::app
