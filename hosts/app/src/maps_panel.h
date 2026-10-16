// SPDX-License-Identifier: Apache-2.0
#pragma once

// The maps panel: the groom's painted maps (vertex maps and texture maps on the scalp, guide
// layers on the guides), made, renamed and removed through the document (one undo step each),
// one of them shown on the scalp, and painted: the Paint brush set on the picked map (its
// kind, its file, the value or color and channels painted toward) and the viewport put in brush
// mode, so the strokes go through the same brush session as every other brush. A new map may be
// for a setting (Density, Width... or any mappable param): made, linked and painted at once
// (paintParamRequested).

#include <string>
#include <utility>
#include <vector>

#include <QWidget>

#include "groomlab/scalp_paint.h"

class QCheckBox;
class QComboBox;
class QLineEdit;
class QLabel;
class QListWidget;
class QSpinBox;

namespace groomlab::app {

class ValueSlider;

class BrushSession;
class ColorEditor;
class GroomDocument;
class RoundButton;

class MapsPanel : public QWidget {
    Q_OBJECT
public:
    explicit MapsPanel(QWidget* parent = nullptr);

    void setDocument(GroomDocument* document);  // null: nothing to list
    void setSession(BrushSession* session);     // the brush the Paint button sets up
    void refresh();                             // the list from the document (after an edit, undo, a stroke)
    std::string selectedMap() const;
    void selectMap(const std::string& name);
    // What the panel would make: the new-map form's fields (tests and scripted runs set them).
    void setNewMapFields(const QString& name, const QString& kind, bool color, double value, int resolution);
    // The form's For: a setting the new map is for (a common one, or one under Other: any mappable
    // param of the graph), or none ("" ""): a blank map. False when node.param is not offered.
    bool setNewMapTarget(const QString& node, const QString& param);
    std::pair<std::string, std::string> newMapTarget() const;
    // The value a gray map is painted toward (the Paint box's slider).
    void setPaintValue(double value);
    double newMapFill() const;  // the form's fill (1.0 until set: a mask that lets everything through)
    bool hintShown() const;     // the one-line hint while the groom has no maps
    // The buttons as functions: "" or the error shown.
    std::string createFromFields();
    std::string renameSelected(const QString& to);
    std::string removeSelected();
    // Sets the brush session up to paint the selected map and asks the host for brush mode.
    bool paintSelected();
    // Shows the selected map on the scalp (or none).
    void showSelected(bool show);

signals:
    void edited();          // the document changed (maps made, renamed, removed): evaluate, undo, redisplay
    void paintRequested();  // the brush is set on a map: the host enters brush mode
    void displayChanged();  // the map shown on the scalp changed: the host redisplays
    void brushChanged();    // the brush's paint settings changed: the host refreshes the brush panels
    void message(const QString& text);
    // Create with For set to a setting: the host makes (or finds) the map node.param reads, links
    // it and paints it (MainWindow::paintParam).
    void paintParamRequested(const std::string& node, const std::string& param, const std::string& kind);

private:
    void refreshTargets();      // the For choices from the document's graph
    void targetChanged();       // the form follows For (named for the setting, gray, on the scalp)
    void refreshRows();         // the paint and show rows for the picked map
    void applyPaintSettings();  // the value, color and channels to the session
    GroomDocument* document_ = nullptr;
    BrushSession* session_ = nullptr;
    QListWidget* list_ = nullptr;
    QLabel* hint_ = nullptr;  // shown while the list is empty
    QComboBox* newFor_ = nullptr;    // nothing, a common setting, or Other (newOther_)
    QComboBox* newOther_ = nullptr;  // every mappable param of the graph
    QLineEdit* newName_ = nullptr;
    QComboBox* newKind_ = nullptr;
    QCheckBox* newColor_ = nullptr;
    ValueSlider* newValue_ = nullptr;
    QSpinBox* newResolution_ = nullptr;
    QLineEdit* renameTo_ = nullptr;
    QCheckBox* show_ = nullptr;
    QComboBox* channel_ = nullptr;
    ValueSlider* paintValue_ = nullptr;
    ColorEditor* paintColor_ = nullptr;
    ValueSlider* paintAlpha_ = nullptr;
    std::vector<QCheckBox*> paintChannels_;
    QWidget* colorRow_ = nullptr;
    RoundButton* paint_ = nullptr;
    bool updating_ = false;
};

}  // namespace groomlab::app
