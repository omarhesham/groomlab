// SPDX-License-Identifier: Apache-2.0
#include "guides_panel.h"

#include <algorithm>
#include <map>
#include <set>

#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QScrollArea>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "groom_document.h"
#include "groomlab/curve.h"
#include "groomlab/flow.h"
#include "groomlab/guide_selection.h"
#include "groomlab/parts.h"

namespace groomlab::app {

namespace {

// A group name not yet taken: base, base1, base2...
std::string uniqueName(const GuideHierarchy& h, const std::string& base) {
    if (h.findGroup(base) == kNoGroup) return base;
    for (int k = 1;; ++k) {
        const std::string name = base + std::to_string(k);
        if (h.findGroup(name) == kNoGroup) return name;
    }
}

}  // namespace

GuidesPanel::GuidesPanel(QWidget* parent) : QWidget(parent) {
    // The rows in a scroll area: a short window scrolls them rather than squeezing the buttons
    // and the tree (which keeps room for a few groups).
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* host = new QWidget(scroll);
    scroll->setWidget(host);
    outer->addWidget(scroll);
    auto* layout = new QVBoxLayout(host);
    layout->setContentsMargins(6, 6, 6, 6);
    tree_ = new QTreeWidget(host);
    tree_->setMinimumHeight(132);
    tree_->setHeaderLabels({tr("Group"), tr("Guides")});
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    tree_->setToolTip(tr("The guide groups: untick to hide one, double-click to rename"));
    layout->addWidget(tree_, 1);
    connect(tree_, &QTreeWidget::itemChanged, this, &GuidesPanel::itemChanged);

    selection_ = new QLabel(tr("No guides selected"), this);
    layout->addWidget(selection_);

    auto* buttons = new QGridLayout();
    addRoot_ = new QPushButton(tr("Add group"), this);
    addRoot_->setToolTip(tr("A new root group (primary guides)"));
    addChild_ = new QPushButton(tr("Add child group"), this);
    addChild_->setToolTip(tr("A new group under the current one (its guides follow the parent's)"));
    remove_ = new QPushButton(tr("Remove group"), this);
    remove_->setToolTip(tr("Removes the current group, its sub-groups and their guides"));
    select_ = new QPushButton(tr("Select group"), this);
    select_->setToolTip(tr("Selects the current group's guides"));
    generate_ = new QPushButton(tr("Generate children"), this);
    generate_->setToolTip(tr("Children for every guide of the current group, inside their tubes, into a child group"));
    count_ = new QSpinBox(this);
    count_->setRange(1, 64);
    count_->setValue(5);
    count_->setPrefix(tr("x "));
    count_->setToolTip(tr("Children per parent guide"));
    delete_ = new QPushButton(tr("Delete selected guides"), this);
    delete_->setToolTip(tr("Removes the selected guides with their descendants (Delete in the viewport)"));
    buttons->addWidget(addRoot_, 0, 0);
    buttons->addWidget(addChild_, 0, 1);
    buttons->addWidget(remove_, 1, 0);
    buttons->addWidget(select_, 1, 1);
    buttons->addWidget(generate_, 2, 0);
    buttons->addWidget(count_, 2, 1);
    buttons->addWidget(delete_, 3, 0, 1, 2);
    // The shape edits the core's hierarchy offers: mirror, symmetry, promote, resample.
    auto* mirror = new QPushButton(tr("Mirror group +X → -X"), this);
    mirror->setToolTip(tr("Copies the current root group's guides on the +X side over the -X side (their descendants too)"));
    auto* sync = new QPushButton(tr("Sync symmetry"), this);
    sync->setToolTip(tr("Makes the whole groom symmetric across x = 0 from the +X side: every root group, children with their parents"));
    auto* promote = new QPushButton(tr("Promote group"), this);
    promote->setToolTip(tr("Moves the current group one level up: its guides follow their grandparents (primaries at the root)"));
    auto* resample = new QPushButton(tr("Resample"), this);
    resample->setToolTip(tr("The selected (else every) guide resampled along its smooth curve to this many segments"));
    segments_ = new QSpinBox(this);
    segments_->setRange(1, 256);
    segments_->setValue(16);
    segments_->setSuffix(tr(" seg"));
    buttons->addWidget(mirror, 4, 0);
    buttons->addWidget(sync, 4, 1);
    buttons->addWidget(promote, 5, 0, 1, 2);
    buttons->addWidget(resample, 6, 0);
    buttons->addWidget(segments_, 6, 1);
    layout->addLayout(buttons);
    connect(mirror, &QPushButton::clicked, this, [this] { mirrorGroup(currentGroup()); });
    connect(sync, &QPushButton::clicked, this, [this] { symmetrySync(); });
    connect(promote, &QPushButton::clicked, this, [this] { promoteGroup(currentGroup()); });
    connect(resample, &QPushButton::clicked, this, [this] { resampleGuides(segments_->value()); });

    // The parts and flow the brushes laid (parts.h, flow.h): listed by name, removed, cleared.
    auto* overlays = new QGroupBox(tr("Parts and flow"), this);
    auto* overlaysLayout = new QGridLayout(overlays);
    parts_ = new QListWidget(overlays);
    parts_->setFixedHeight(72);
    parts_->setToolTip(tr("The parts the Part brush drew (a Part node parts the hair along them)"));
    overlaysLayout->addWidget(parts_, 0, 0, 1, 2);
    removePart_ = new QPushButton(tr("Remove part"), overlays);
    clearParts_ = new QPushButton(tr("Clear parts"), overlays);
    overlaysLayout->addWidget(removePart_, 1, 0);
    overlaysLayout->addWidget(clearParts_, 1, 1);
    flow_ = new QLabel(tr("No flow"), overlays);
    clearFlow_ = new QPushButton(tr("Clear flow"), overlays);
    clearFlow_->setToolTip(tr("Removes every flow arrow and sink the Flow brush laid"));
    overlaysLayout->addWidget(flow_, 2, 0);
    overlaysLayout->addWidget(clearFlow_, 2, 1);
    layout->addWidget(overlays);
    connect(removePart_, &QPushButton::clicked, this, [this] {
        if (parts_->currentItem()) removePart(parts_->currentItem()->text().toStdString());
    });
    connect(clearParts_, &QPushButton::clicked, this, [this] { clearParts(); });
    connect(clearFlow_, &QPushButton::clicked, this, [this] { clearFlow(); });

    connect(addRoot_, &QPushButton::clicked, this, [this] { addGroup({}); });
    connect(addChild_, &QPushButton::clicked, this, [this] { addGroup(currentGroup()); });
    connect(remove_, &QPushButton::clicked, this, [this] { removeGroup(currentGroup()); });
    connect(select_, &QPushButton::clicked, this, [this] { selectGroup(currentGroup()); });
    connect(generate_, &QPushButton::clicked, this, [this] { generateChildren(currentGroup(), count_->value()); });
    connect(delete_, &QPushButton::clicked, this, [this] { deleteSelected(); });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem*, QTreeWidgetItem*) {
        const bool has = !currentGroup().empty();
        addChild_->setEnabled(has);
        remove_->setEnabled(has);
        select_->setEnabled(has);
        generate_->setEnabled(has);
    });
    setEnabled(false);
}

void GuidesPanel::setDocument(GroomDocument* document) {
    document_ = document;
    setEnabled(document_ != nullptr);
    refresh();
}

std::string GuidesPanel::groupOf(QTreeWidgetItem* item) const {
    return item ? item->data(0, Qt::UserRole).toString().toStdString() : std::string();
}

std::string GuidesPanel::currentGroup() const { return groupOf(tree_->currentItem()); }

void GuidesPanel::refresh() {
    updating_ = true;
    const std::string current = currentGroup();
    tree_->clear();
    QTreeWidgetItem* select = nullptr;
    if (document_) {
        const HierarchyPtr h = document_->session().authoredHierarchy();
        if (h) {
            // Guides per group, then the groups as a tree (parents before children in the
            // hierarchy's order, so a child's item finds its parent's).
            std::map<GroupId, int> counts;
            for (const HierarchyGuide& g : h->guides()) ++counts[g.group];
            std::map<GroupId, QTreeWidgetItem*> items;
            const std::set<std::string>& hidden = document_->hiddenGroups();
            for (const GuideGroup& group : h->groups()) {
                const auto parent = items.find(group.parent);
                auto* item = parent == items.end() ? new QTreeWidgetItem(tree_) : new QTreeWidgetItem(parent->second);
                item->setText(0, QString::fromStdString(group.name));
                item->setText(1, QString::number(counts[group.id]));
                item->setData(0, Qt::UserRole, QString::fromStdString(group.name));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
                item->setCheckState(0, hidden.count(group.name) ? Qt::Unchecked : Qt::Checked);
                item->setExpanded(true);
                items[group.id] = item;
                if (group.name == current) select = item;
            }
        }
        const std::size_t n = document_->selection().size();
        selection_->setText(n == 0 ? tr("No guides selected") : tr("%n guide(s) selected", "", static_cast<int>(n)));
        delete_->setEnabled(n > 0);
    }
    // The parts by name and the flow's count, as the brushes left them.
    parts_->clear();
    for (const std::string& name : partNames()) parts_->addItem(QString::fromStdString(name));
    removePart_->setEnabled(parts_->count() > 0);
    clearParts_->setEnabled(parts_->count() > 0);
    std::size_t pins = 0, sinks = 0;
    if (document_) {
        try {
            const FlowSet flow = FlowSet::fromJson(document_->storedText(StrokeField::Flow));
            pins = flow.pins.size();
            sinks = flow.sinks.size();
        } catch (const std::exception&) {
        }
    }
    flow_->setText(pins + sinks == 0 ? tr("No flow") : tr("%1 arrows, %2 sinks").arg(pins).arg(sinks));
    clearFlow_->setEnabled(pins + sinks > 0);
    if (select) tree_->setCurrentItem(select);
    else if (tree_->topLevelItemCount() > 0) tree_->setCurrentItem(tree_->topLevelItem(0));
    const bool has = !currentGroup().empty();
    addChild_->setEnabled(has);
    remove_->setEnabled(has);
    select_->setEnabled(has);
    generate_->setEnabled(has);
    updating_ = false;
}

void GuidesPanel::itemChanged(QTreeWidgetItem* item, int column) {
    if (updating_ || !document_ || column != 0) return;
    const std::string name = groupOf(item);
    const bool hidden = item->checkState(0) == Qt::Unchecked;
    if (document_->hiddenGroups().count(name) != (hidden ? 1u : 0u)) {
        document_->setGroupHidden(name, hidden);
        emit displayChanged();
    }
    const std::string text = item->text(0).toStdString();
    if (text != name) {
        if (!renameGroup(name, text)) {
            updating_ = true;
            item->setText(0, QString::fromStdString(name));
            updating_ = false;
        }
    }
}

bool GuidesPanel::addGroup(const std::string& parent) {
    if (!document_) return false;
    std::string error = document_->editGuides("add group", [&](GuideHierarchy& h) {
        GroupId parentId = kNoGroup;
        if (!parent.empty()) {
            parentId = h.findGroup(parent);
            if (parentId == kNoGroup) throw std::invalid_argument("no group named " + parent);
        }
        h.addGroup(uniqueName(h, parent.empty() ? "group" : parent + "_children"), parentId);
    });
    if (!error.empty()) {
        emit message(tr("Cannot add a group: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::removeGroup(const std::string& name) {
    if (!document_ || name.empty()) return false;
    const std::string error = document_->editGuides("remove group", [&](GuideHierarchy& h) {
        const GroupId id = h.findGroup(name);
        if (id == kNoGroup) throw std::invalid_argument("no group named " + name);
        h.removeGroup(id);
    });
    if (!error.empty()) {
        emit message(tr("Cannot remove the group: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::renameGroup(const std::string& name, const std::string& to) {
    if (!document_ || name.empty() || to.empty() || name == to) return false;
    const std::string error = document_->editGuides("rename group", [&](GuideHierarchy& h) {
        const GroupId id = h.findGroup(name);
        if (id == kNoGroup) throw std::invalid_argument("no group named " + name);
        h.renameGroup(id, to);
    });
    if (!error.empty()) {
        emit message(tr("Cannot rename the group: %1").arg(QString::fromStdString(error)));
        return false;
    }
    if (document_->hiddenGroups().count(name)) {
        document_->setGroupHidden(name, false);
        document_->setGroupHidden(to, true);
    }
    emit edited();
    return true;
}

bool GuidesPanel::generateChildren(const std::string& parent, int count) {
    if (!document_ || parent.empty()) return false;
    const std::string error = document_->editGuides("generate children", [&](GuideHierarchy& h) {
        const GroupId id = h.findGroup(parent);
        if (id == kNoGroup) throw std::invalid_argument("no group named " + parent);
        if (h.guidesInGroup(id).empty()) throw std::invalid_argument("the group " + parent + " has no guides to be parents");
        ChildGenerationParams params;
        params.count = static_cast<std::size_t>(std::max(count, 1));
        // The group's existing child group when it has one (the hierarchy replaces each
        // parent's children there), else a new one named after it.
        std::string childName;
        for (const GuideGroup& g : h.groups()) {
            if (g.parent == id) {
                childName = g.name;
                break;
            }
        }
        if (childName.empty()) childName = uniqueName(h, parent + "_children");
        h.generateChildren(id, childName, params);
    });
    if (!error.empty()) {
        emit message(tr("Cannot generate children: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::deleteSelected() {
    if (!document_ || document_->selection().empty()) return false;
    const std::vector<GuideId> ids = document_->selection();
    const std::string error = document_->editGuides("delete guides", [&](GuideHierarchy& h) { h.removeGuides(ids); });
    if (!error.empty()) {
        emit message(tr("Cannot delete the guides: %1").arg(QString::fromStdString(error)));
        return false;
    }
    document_->setSelection({});
    emit edited();
    return true;
}

void GuidesPanel::selectGroup(const std::string& name) {
    if (!document_ || name.empty()) return;
    const HierarchyPtr h = document_->session().authoredHierarchy();
    if (!h) return;
    document_->setSelection(guidesAt(*h, -1, name));
    emit displayChanged();
}

// ---- Shape edits ------------------------------------------------------------------------------------

bool GuidesPanel::mirrorGroup(const std::string& name, bool fromPositive) {
    if (!document_ || name.empty()) return false;
    const std::string error = document_->editGuides("mirror group", [&](GuideHierarchy& h) {
        const GroupId id = h.findGroup(name);
        if (id == kNoGroup) throw std::invalid_argument("no group named " + name);
        if (h.groupDepth(id) != 0) throw std::invalid_argument(name + " is not a root group (mirror its root group: the children follow)");
        h.mirrorGroup(id, 0, 0.0f, fromPositive);
    });
    if (!error.empty()) {
        emit message(tr("Cannot mirror the group: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::symmetrySync(bool fromPositive) {
    if (!document_) return false;
    const std::string error = document_->editGuides("symmetry sync", [&](GuideHierarchy& h) {
        if (h.guides().empty()) throw std::invalid_argument("the groom has no guides");
        h.mirrorAll(0, 0.0f, fromPositive);
    });
    if (!error.empty()) {
        emit message(tr("Cannot sync the symmetry: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::promoteGroup(const std::string& name) {
    if (!document_ || name.empty()) return false;
    const std::string error = document_->editGuides("promote group", [&](GuideHierarchy& h) {
        const GroupId id = h.findGroup(name);
        if (id == kNoGroup) throw std::invalid_argument("no group named " + name);
        if (h.groupDepth(id) == 0) throw std::invalid_argument(name + " is a root group already");
        h.promoteGroup(id);
    });
    if (!error.empty()) {
        emit message(tr("Cannot promote the group: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::resampleGuides(int segments) {
    if (!document_) return false;
    const std::vector<GuideId> selected = document_->selection();
    const std::string error = document_->editGuides("resample guides", [&](GuideHierarchy& h) {
        std::vector<GuideId> ids = selected;
        if (ids.empty()) {
            for (const HierarchyGuide& g : h.guides()) ids.push_back(g.id);
        }
        if (ids.empty()) throw std::invalid_argument("the groom has no guides to resample");
        for (const GuideId id : ids) {
            const HierarchyGuide* g = h.guide(id);
            if (!g || g->points.size() < 2) continue;
            // Along the smooth curve through its points, as the viewport draws it (Maya's -resample).
            h.setPoints(id, resampleSmoothCurve(g->points.data(), g->points.size(), static_cast<std::size_t>(std::clamp(segments, 1, 256)) + 1));
        }
    });
    if (!error.empty()) {
        emit message(tr("Cannot resample the guides: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

// ---- Parts and flow -------------------------------------------------------------------------------

std::vector<std::string> GuidesPanel::partNames() const {
    std::vector<std::string> names;
    if (!document_) return names;
    try {
        for (const Part& p : PartSet::fromJson(document_->storedText(StrokeField::Parts)).parts) names.push_back(p.name);
    } catch (const std::exception&) {
    }
    return names;
}

bool GuidesPanel::removePart(const std::string& name) {
    if (!document_ || name.empty()) return false;
    const std::string error = document_->editStoredText("remove part", StrokeField::Parts, [&](std::string& text) {
        PartSet parts = PartSet::fromJson(text);
        const auto it = std::find_if(parts.parts.begin(), parts.parts.end(), [&](const Part& p) { return p.name == name; });
        if (it == parts.parts.end()) throw std::invalid_argument("no part named " + name);
        parts.parts.erase(it);
        text = parts.empty() ? std::string() : parts.toJson();
    });
    if (!error.empty()) {
        emit message(tr("Cannot remove the part: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::clearParts() {
    if (!document_) return false;
    const std::string error = document_->editStoredText("clear parts", StrokeField::Parts, [](std::string& text) { text.clear(); });
    if (!error.empty()) {
        emit message(tr("Cannot clear the parts: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

bool GuidesPanel::clearFlow() {
    if (!document_) return false;
    const std::string error = document_->editStoredText("clear flow", StrokeField::Flow, [](std::string& text) { text.clear(); });
    if (!error.empty()) {
        emit message(tr("Cannot clear the flow: %1").arg(QString::fromStdString(error)));
        return false;
    }
    emit edited();
    return true;
}

}  // namespace groomlab::app
