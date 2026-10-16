// SPDX-License-Identifier: Apache-2.0
#pragma once

// The correctives panel (Maya's Sculpt tab): the shot-sculpt layers the groom holds
// (correctives.h), each with its weight and whether it is on; new layers on every frame or
// fixed to the timeline's frame with a fade; one removed; and the layer the brushes sculpt into.
// With a layer picked, strokes reshape the groom as shown at the current frame and record the
// difference as that layer's offsets (the core's shot sculpting through the brush session), so a
// correction on one frame rides along with the motion around it. Every edit goes to the document
// as one undo step (the correctives text, as a stroke's). Keyed weights (Maya's animCurves) stay
// Maya's: here a layer's weight is its envelope's. "Sculpt into" also lists the graph's Sculpt
// nodes ("Node: sculpt1"): picked, strokes shape that node's guides and are kept in the node
// (sculpt.h; the document's sculpt target), and Clear Edits drops them.

#include <functional>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace groomlab {
class Correctives;
}

namespace groomlab::app {

class BrushSession;
class GroomDocument;
class ValueSlider;

class CorrectivesPanel : public QWidget {
    Q_OBJECT
public:
    explicit CorrectivesPanel(QWidget* parent = nullptr);

    void setDocument(GroomDocument* document);  // null: nothing to edit
    void setSession(BrushSession* session);     // whose strokes sculpt into the target layer
    // The list takes the document's layers again; the brush session is told the target and time.
    void refresh();
    std::vector<std::string> layerNames() const;
    // The layer strokes sculpt into ("" the base groom: ordinary strokes).
    const std::string& targetLayer() const { return target_; }
    void setTargetLayer(const std::string& name);
    // The Sculpt node strokes shape ("" none: the base groom or a layer): the document's target.
    std::string targetNode() const;
    void setTargetNode(const std::string& node);
    bool clearNodeEdits();  // the targeted Sculpt node's edits dropped (one undo step)

    // The edits, as the buttons do them; false (and a message) when refused. A new layer is on
    // every frame (`allFrames`), or full at the document's frame fading over `fadeFrames` frames
    // each side; its name is made unique ("layer", "shot<frame>").
    bool addLayer(bool allFrames, double fadeFrames = 6.0);
    bool removeLayer(const std::string& name);
    bool setLayerWeight(const std::string& name, float weight);
    bool setLayerEnabled(const std::string& name, bool enabled);
    bool renameLayer(const std::string& name, const std::string& to);

signals:
    void edited();          // the document's correctives changed: the host evaluates and redraws
    void targetChanged();   // the brushes sculpt into another layer
    void message(const QString& text);

private:
    std::string currentLayer() const;
    void applyTarget();  // the session's shot sculpt from the target and the document's time
    bool editLayers(const std::string& label, const std::function<void(Correctives&)>& edit);

    GroomDocument* document_ = nullptr;
    BrushSession* session_ = nullptr;
    QTreeWidget* list_ = nullptr;
    ValueSlider* weight_ = nullptr;
    QComboBox* target_box_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* fix_ = nullptr;
    QPushButton* remove_ = nullptr;
    QPushButton* clearNode_ = nullptr;
    QLabel* hint_ = nullptr;
    std::string target_;
    bool updating_ = false;
};

}  // namespace groomlab::app
