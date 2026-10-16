// SPDX-License-Identifier: Apache-2.0
#pragma once

// The look dock: the hair material (aiStandardHair's parameters and the presets), the
// surfaces' materials (the scalp and the props: a general PBR material, skin or an eye, with
// their maps and presets) and the lighting (the lights, the environment, exposure, tone
// mapping, the background, the quality mode, its presets). Every edit goes to the viewport at
// once; the groom's own HairColor highlight (specular, roughness, tilt) is applied over the hair
// material while its pigment comes from the groom.

#include <functional>
#include <utility>
#include <vector>

#include <QString>
#include <QWidget>

#include "look.h"

class QCheckBox;
class QComboBox;
class QFormLayout;
class QGroupBox;
class QLineEdit;

namespace groomlab::app {

class ValueSlider;

class LookPanel : public QWidget {
    Q_OBJECT
public:
    explicit LookPanel(QWidget* parent = nullptr);

    const HairMaterial& material() const { return material_; }
    const LightingSetup& lighting() const { return lighting_; }
    void setMaterial(const HairMaterial& material);
    void setLighting(const LightingSetup& lighting);
    void applyMaterialPreset(const QString& name);
    void applyLightingPreset(const QString& name);
    // The surfaces to edit (their names and materials, the scalp first); the panel keeps
    // copies and reports each edit with the surface's index.
    void setSurfaces(const std::vector<std::pair<QString, SurfaceMaterial>>& surfaces);
    const SurfaceMaterial* surfaceMaterial(int index) const;
    // A preset onto one surface (by index), as the combo does.
    void applySurfacePreset(int index, const QString& name);

signals:
    void materialChanged();
    void lightingChanged();
    void surfaceChanged(int index, const SurfaceMaterial& material);

private:
    enum class Target { Hair, Lighting, Surface };
    struct LightRow {
        QCheckBox* enabled = nullptr;
        ValueSlider* azimuth = nullptr;
        ValueSlider* elevation = nullptr;
        ValueSlider* intensity = nullptr;
        QCheckBox* shadows = nullptr;
        QWidget* host = nullptr;
    };
    // A row of the surface form shown for some kinds alone (bit 1 default, 2 skin, 4 eye).
    struct SurfaceRow {
        QWidget* editor = nullptr;
        int kinds = 7;
    };
    // A slider bound to a float: `get` reads it, `set` writes it and the panel emits.
    ValueSlider* slider(QFormLayout* form, const QString& label, double min, double max, double step, int decimals,
                        std::function<double()> get, std::function<void(double)> set, Target target, int kinds = 7);
    QLineEdit* pathEdit(QFormLayout* form, const QString& label, std::function<std::string()> get,
                        std::function<void(const std::string&)> set, int kinds = 7);
    void refreshMaterial();  // the editors take material_'s values
    void refreshLighting();
    void refreshSurface();
    void readLights();       // lighting_.lights from the rows
    SurfaceMaterial* currentSurface();
    void surfaceEdited();

    HairMaterial material_;
    LightingSetup lighting_;
    std::vector<std::pair<QString, SurfaceMaterial>> surfaces_;
    bool updating_ = false;
    std::vector<std::function<void()>> materialRefreshers_, lightingRefreshers_, surfaceRefreshers_;
    std::vector<SurfaceRow> surfaceRows_;
    QComboBox* materialPreset_ = nullptr;
    QComboBox* pigment_ = nullptr;
    QComboBox* lightingPreset_ = nullptr;
    QComboBox* environment_ = nullptr;
    QComboBox* toneMap_ = nullptr;
    QComboBox* background_ = nullptr;
    QComboBox* quality_ = nullptr;
    QCheckBox* ambientOcclusion_ = nullptr;
    QComboBox* surface_ = nullptr;
    QComboBox* surfacePreset_ = nullptr;
    QComboBox* surfaceKind_ = nullptr;
    QFormLayout* surfaceForm_ = nullptr;
    std::vector<LightRow> lightRows_;
};

}  // namespace groomlab::app
