// SPDX-License-Identifier: Apache-2.0
#pragma once

// The brush dock: the brush kinds (the core's modes and the Place, Draw, Part and Flow tools)
// as a list with GroomLab's icons, and the settings every stroke reads (BrushToolSettings,
// stroke.h): the radius in pixels, the strength, the hardness, pen pressure, symmetry, the
// level or group brushed, what Erase removes, Place's shape, the paint attribute, the density
// the strands step back to while a stroke is live. Every edit goes straight into the session's
// settings; the viewport's own edits (a radius drag, a picked level) come back through refresh.

#include <functional>
#include <string>
#include <vector>

#include <QString>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QSpinBox;

namespace groomlab::app {

class ValueSlider;

class BrushSession;

class BrushPanel : public QWidget {
    Q_OBJECT
public:
    explicit BrushPanel(QWidget* parent = nullptr);

    void setSession(BrushSession* session);  // null: nothing to edit
    // The guide groups the brush may target (the open groom's).
    void setGroups(const std::vector<std::string>& names);
    // The editors take the settings' values again (after a radius drag, a picked level).
    void refresh();
    // The brush kind by name (comb, smooth... place, draw, part, flow), as the rail picks it.
    void setMode(const QString& name);
    QString mode() const;

signals:
    void settingsChanged();

private:
    // A slider bound to a float: its span `min`..`max`, a typed value reaching `limitMin`..`limitMax`.
    ValueSlider* slider(QGroupBox* box, const QString& label, double min, double max, double limitMin, double limitMax, int decimals,
                        std::function<double()> get, std::function<void(double)> set, const QString& tip = {});
    QCheckBox* check(QGroupBox* box, const QString& label, std::function<bool()> get, std::function<void(bool)> set,
                     const QString& tip = {});
    // A brush curve of the catalog's (falloffProfile, strengthProfile): a checkbox over a curve
    // editor shown while it is ticked (starting at the keys of `start`, a ramp's JSON); `get` and
    // `set` are the settings' JSON ("" for none).
    void curve(QGroupBox* box, const std::string& setting, const std::string& start, std::function<std::string()> get,
               std::function<void(const std::string&)> set);

    BrushSession* session_ = nullptr;
    bool updating_ = false;
    QSpinBox* level_ = nullptr;
    QComboBox* group_ = nullptr;
    QComboBox* symmetry_ = nullptr;
    QLineEdit* attribute_ = nullptr;
    QLineEdit* partName_ = nullptr;
    QComboBox* flowSink_ = nullptr;
    std::vector<std::function<void()>> refreshers_;
};

}  // namespace groomlab::app
