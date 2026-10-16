// SPDX-License-Identifier: Apache-2.0
#pragma once

// The guides dock: the groom's guide groups as a tree (a check box shows or hides each; a
// double-click renames), the selection's count, and the edits the core's hierarchy offers
// through the document (each one undo step): add a root or child group, remove a group,
// generate children into a group, select a group's guides, delete the selected guides, mirror
// a root group across X, make the whole groom symmetric (Maya's Sync +X -> -X), promote a group
// a level, resample the selected (else every) guide to a segment count; and the parts and flow
// the brushes laid (listed, removed, cleared).

#include <string>
#include <vector>

#include <QString>
#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTreeWidget;
class QTreeWidgetItem;

namespace groomlab::app {

class GroomDocument;

class GuidesPanel : public QWidget {
    Q_OBJECT
public:
    explicit GuidesPanel(QWidget* parent = nullptr);

    void setDocument(GroomDocument* document);  // null: nothing to edit
    // The tree takes the document's groups, selection and hidden groups again.
    void refresh();
    // The group the tree has current ("" none).
    std::string currentGroup() const;

    // The edits, as the buttons do them; false (and a message) when refused.
    bool addGroup(const std::string& parent);  // "": a root group
    bool removeGroup(const std::string& name);
    bool renameGroup(const std::string& name, const std::string& to);
    bool generateChildren(const std::string& parent, int count);
    bool deleteSelected();
    void selectGroup(const std::string& name);
    // A root group mirrored across the plane x = 0 from the positive side (its guides there
    // copied over the other side's); the whole groom made symmetric the same way (every root
    // group, children with their parents); a group moved one level up; the selected (else every)
    // guide resampled along its smooth curve to `segments` segments.
    bool mirrorGroup(const std::string& name, bool fromPositive = true);
    bool symmetrySync(bool fromPositive = true);
    bool promoteGroup(const std::string& name);
    bool resampleGuides(int segments);
    // The parts the Part brush drew, by name; one removed, every one cleared; the flow cleared.
    std::vector<std::string> partNames() const;
    bool removePart(const std::string& name);
    bool clearParts();
    bool clearFlow();

signals:
    // The document's guides changed through this panel: the host evaluates and redraws.
    void edited();
    // The selection or the hidden groups changed: the host redraws.
    void displayChanged();
    void message(const QString& text);

private:
    void itemChanged(QTreeWidgetItem* item, int column);
    std::string groupOf(QTreeWidgetItem* item) const;

    GroomDocument* document_ = nullptr;
    bool updating_ = false;
    QSpinBox* segments_ = nullptr;
    QListWidget* parts_ = nullptr;
    QLabel* flow_ = nullptr;
    QPushButton* removePart_ = nullptr;
    QPushButton* clearParts_ = nullptr;
    QPushButton* clearFlow_ = nullptr;
    QTreeWidget* tree_ = nullptr;
    QLabel* selection_ = nullptr;
    QSpinBox* count_ = nullptr;
    QPushButton* addRoot_ = nullptr;
    QPushButton* addChild_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* generate_ = nullptr;
    QPushButton* select_ = nullptr;
    QPushButton* delete_ = nullptr;
};

}  // namespace groomlab::app
