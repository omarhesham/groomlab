// SPDX-License-Identifier: Apache-2.0
#pragma once

// The floating UI over the viewport: the whole window is the viewport, edge to edge, and every
// control floats over it as glass (glass.h). OverlayHost is the window's central widget: the
// viewport fills it and the pieces are its children, placed by theme::layoutOverlay on every
// resize; where no piece is, the mouse reaches the viewport. The pieces:
//
//   top-left     the menu button (every File, Edit, View and Tools action), the logo, the file
//                name (a yellow dot when edited), drawn on the viewport, no panel
//   top-center   the mode switcher (Groom, Look, Cards, Export)
//   top-right    undo, redo, Render (the refine quality: the viewport converges on the picture)
//   left         the brush rail (Groom): the Select tool, then every brush by category, the
//                active one filled and named beside it; under the mouse the rail widens and
//                names every tool
//   right        round buttons that open floating panels on demand (guide groups, nodes,
//                lighting and look, display, frame all); under the mouse each grows its name
//   bottom       the active brush's settings as one pill (Groom); the stats (Groom, left)
//   Look mode    the look card (material and lighting, the hair color presets, the main
//                sliders), the Graph pill (left) and the refine stats (right)
//   the graph    while it shows (any mode): the Graph pill, the Presets pill and a close
//                button at the bottom left (the stats above them); a click on a mode closes it
//   Cards mode   the cards panel (cards_panel.h) at the right: the Cards node, the preview,
//                the LOD, its settings, the bake and the atlas
//   Export mode  the export panel (export_panel.h) at the right: the targets, the frames and
//                options, the output, progress; save and save as
//   timeline     one pill at the bottom in every mode (above the brush bar in Groom) when the
//                groom animates (timeline.h): transport, scrub, range, fps, loop, follow
//
// The node graph (node_graph.h) is another child of the host, between the viewport and the
// pieces, shown by Tab or the Graph pill.

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <QColor>
#include <QString>
#include <QWidget>

#include "glass.h"
#include "look.h"

class QBoxLayout;
class QCheckBox;
class QComboBox;
class QLabel;
class QMenu;
class QScrollArea;
class QVariantAnimation;

namespace groomlab::app {

class ColorEditor;
class CardsPanel;
class ExportPanel;
class Timeline;
class Viewport;
class NodeGraphView;

// ---- BrushRail ---------------------------------------------------------------------------------
// The slim rail of circle buttons. Under the mouse it widens to the right (150 ms) and names
// each tool beside its icon; it floats over the viewport, so nothing else moves. The tooltips stay.
class BrushRail : public GlassPanel {
    Q_OBJECT
public:
    explicit BrushRail(QWidget* parent = nullptr);
    int buttonCount() const;   // the Select tool and every brush
    int dividerCount() const;  // between the groups
    // Lays the buttons out at a size with the gaps and dividers (theme::layoutOverlay's
    // railButton, railGap, railDivider); the rail's size follows.
    void setButtonSize(int size, int gap = 4, int divider = 9);
    // The active tool: a brush mode ("" none, the camera), or the Select tool.
    void setActive(const std::string& mode, bool select);
    // Where the active button's row is (host coordinates), for the label beside it.
    QRect activeRect() const;
    // The names shown (the hover state); `animate` false lands at once (a screenshot).
    void setExpanded(bool expanded, bool animate = true);
    bool expanded() const { return expanded_; }
    int collapsedWidth() const { return collapsedWidth_; }
    int expandedWidth() const;

signals:
    void brushChosen(const std::string& mode);
    void selectChosen();
    void expansionChanged();  // the width changed (the host hides its own label meanwhile)

protected:
    void paintEvent(QPaintEvent* event) override;  // the dividers and the names over the glass
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Entry {
        RoundButton* button = nullptr;
        std::string mode;  // "" for Select
        QString name;
        bool dividerAfter = false;
    };
    void applyExpansion();  // the width from expand_
    std::vector<Entry> entries_;
    int buttonSize_ = 44;
    int gap_ = 4, divider_ = 9;
    bool laidOut_ = false;  // setButtonSize ran once
    int collapsedWidth_ = 56;
    int labelWidth_ = 0;  // the widest name
    int active_ = -1;
    std::string toolMode_;
    bool toolSelect_ = false;
    bool expanded_ = false;
    qreal expand_ = 0.0;  // 0 collapsed .. 1 the names fully shown
    QVariantAnimation* animation_ = nullptr;
};

// ---- PanelRail ----------------------------------------------------------------------------------
// The right-hand column of round glass buttons (the panels, frame all). Under the mouse it
// grows a label to the left of each button (150 ms), a glass pill with the panel's name,
// without moving the buttons; it floats, so nothing else moves. A label is part of its
// button's target: it lights with it and a click on it clicks the button. The tooltips stay.
class PanelRail : public QWidget {
    Q_OBJECT
public:
    explicit PanelRail(QWidget* parent = nullptr);
    // Takes the button (reparented) with the name shown beside it.
    void addButton(RoundButton* button, const QString& name);
    int buttonCount() const { return static_cast<int>(entries_.size()); }
    // Places the rail with its buttons' column at `right` (host coordinates) from `top` down;
    // expanded, it reaches further left.
    void place(int right, int top);
    // A button's vertical center in host coordinates (a panel opens beside it).
    int buttonCenterY(int index) const;
    void setExpanded(bool expanded, bool animate = true);
    bool expanded() const { return expanded_; }
    int collapsedWidth() const;
    int expandedWidth() const;

protected:
    void paintEvent(QPaintEvent* event) override;  // the label pills
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;  // a button's hover

private:
    struct Entry {
        RoundButton* button = nullptr;
        QString name;
    };
    void applyExpansion();
    int rowAt(const QPoint& at) const;  // the row under a point of the rail (-1 none)
    int hovered_ = -1;
    int pressed_ = -1;
    std::vector<Entry> entries_;
    int right_ = 0, top_ = 0;
    int labelWidth_ = 0;
    bool expanded_ = false;
    qreal expand_ = 0.0;
    QVariantAnimation* animation_ = nullptr;
};

// ---- BrushBar ------------------------------------------------------------------------------------
// The active brush's settings as one pill: the name with its category dot, the radius and
// strength sliders, the falloff (hardness) button, the Sym X chip, the pen pressure toggle and
// a "..." that opens the full brush panel.
class BrushBar : public GlassPanel {
    Q_OBJECT
public:
    explicit BrushBar(QWidget* parent = nullptr);
    struct Values {
        std::string mode;
        float radius = 60.0f;
        float strength = 1.0f;
        float hardness = 0.5f;
        int mirrorAxis = -1;
        bool pressure = true;
    };
    void setValues(const Values& values);  // no signals
    const Values& values() const { return values_; }

signals:
    // The user changed something (values() has it); `live` while a slider drags.
    void edited(bool live);
    void moreRequested();

private:
    void refresh();
    Values values_;
    QLabel* dot_ = nullptr;
    QLabel* name_ = nullptr;
    ValueSlider* radius_ = nullptr;
    ValueSlider* strength_ = nullptr;
    RoundButton* falloff_ = nullptr;
    RoundButton* symmetry_ = nullptr;
    RoundButton* pressure_ = nullptr;
    RoundButton* more_ = nullptr;
    bool updating_ = false;
};

// ---- LookCard ------------------------------------------------------------------------------------
// Look mode's card, three tabs. Material: the hair color presets as swatches, melanin, redness
// and roughness, and behind "More settings" every aiStandardHair parameter (the pigment's
// source, randomize, the dye, the azimuthal roughness, IOR, shift, the lobes' weights and
// tints, diffuse, multiple scattering, the width scale and shadow density) and the surfaces
// (the scalp, the props, the eyes: a preset, the kind, its parameters with the skin's
// scattering controls). Lighting: the presets, each light's azimuth, elevation, intensity,
// color and shadows, the environment, its rotation and intensity, exposure, the tone map, the
// background. Render: the quality, the refine sample count, the resolution scale, the
// interactive quality (adaptive or full). The card
// scrolls when a tab is taller than the view; "All settings" opens the full look panel (the
// surfaces' texture files live there).
class LookCard : public GlassPanel {
    Q_OBJECT
public:
    enum class Tab { Material, Lighting, Render };

    explicit LookCard(QWidget* parent = nullptr);
    void setMaterial(const HairMaterial& material);  // no signals
    void setLighting(const LightingSetup& lighting);
    const HairMaterial& material() const { return material_; }
    const LightingSetup& lighting() const { return lighting_; }
    // The groom's own look: its last HairColor node's melanin, redness and roughness, which the
    // swatches and the main sliders edit (saved with the groom). Without a HairColor node (`node`
    // empty) they edit the viewport material instead, and say so.
    // HairColor's mode: the pigment (melanin, redness) or a picked RGB color (`picked`: mode color,
    // `color` rendered as it is); its tip color and how far the tips go to it, in either mode.
    struct GroomColor {
        std::string node;
        float melanin = 0.35f, redness = 0.2f, roughness = 0.5f;
        bool picked = false;
        RGB color{0.8f, 0.8f, 0.8f};
        RGB tipColor{0.55f, 0.42f, 0.28f};
        float tipStrength = 0.0f;
        float hueRandom = 0.0f, saturationRandom = 0.0f;  // each strand's hue and saturation spread
    };
    void setGroomColor(const GroomColor& color);  // no signals
    const GroomColor& groomColor() const { return groomColor_; }
    // The pieces (tests): the main sliders and the swatches (0 the groom's own pigment, then
    // black, brown, blonde, red, white, dyed).
    ValueSlider* melaninSlider() const { return melanin_; }
    ValueSlider* rednessSlider() const { return redness_; }
    ValueSlider* roughnessSlider() const { return roughness_; }
    Segmented* groomModeSwitch() const { return groomMode_; }  // 0 natural (melanin), 1 a picked color
    ColorEditor* groomColorEditor() const { return groomPicked_; }
    ColorEditor* tipColorEditor() const { return tipColor_; }
    ValueSlider* tipStrengthSlider() const { return tipStrength_; }
    ValueSlider* hueRandomSlider() const { return hueRandom_; }
    ValueSlider* saturationRandomSlider() const { return saturationRandom_; }
    RoundButton* swatch(int index) const { return swatches_.at(static_cast<std::size_t>(index)); }
    QString colorNote() const;
    // The surfaces to edit (the scalp first); each edit comes back with the surface's index.
    void setSurfaces(const std::vector<std::pair<QString, SurfaceMaterial>>& surfaces);
    const SurfaceMaterial* surfaceMaterial(int index) const;
    void setTab(Tab tab);
    Tab tab() const { return static_cast<Tab>(tabs_->current()); }
    void setMoreShown(bool shown);  // the material's full settings and the surfaces
    bool moreShown() const { return moreShown_; }
    // The card's height for its content, at most `maxHeight` (the rest scrolls).
    void setMaxHeight(int maxHeight);
    QSize sizeHint() const override;

signals:
    void materialEdited();
    void lightingEdited();
    void surfaceEdited(int index, const SurfaceMaterial& material);
    void moreRequested();  // the full look panel
    // The groom's HairColor node edited: one param from a slider (`live` while it drags; the
    // drag's end comes with live false), or a swatch's melanin, redness and roughness together.
    void groomColorEdited(const std::string& param, double value, bool live);
    void groomSwatchChosen(const std::string& preset);
    void groomModeChosen(const std::string& mode);  // HairColor's mode: melanin or color
    // A color param of the groom's HairColor (color, tipColor) picked; `live` while it drags.
    void groomColorPicked(const std::string& param, const RGB& color, bool live);

private:
    enum class Target { Material, Lighting, Surface };
    void refreshGroomColor();
    struct LightRow {
        QWidget* host = nullptr;
        QCheckBox* enabled = nullptr;
        ValueSlider* azimuth = nullptr;
        ValueSlider* elevation = nullptr;
        ValueSlider* intensity = nullptr;
        ColorEditor* color = nullptr;
        QCheckBox* shadows = nullptr;
    };
    // A surface row shown for some kinds alone (bit 1 default, 2 skin, 4 eye).
    struct SurfaceRow {
        QWidget* widget = nullptr;
        int kinds = 7;
    };
    ValueSlider* slider(QBoxLayout* into, const QString& caption, const QColor& fill, double min, double max, int decimals,
                        std::function<double()> get, std::function<void(double)> set, Target target, int kinds = 7);
    ColorEditor* color(QBoxLayout* into, const QString& caption, std::function<RGB()> get, std::function<void(const RGB&)> set,
                       Target target, int kinds = 7);
    QLabel* caption(QBoxLayout* into, const QString& text, int kinds = 7, Target target = Target::Material);
    void edited(Target target);
    void refresh();
    void refreshSurfaceRows();
    SurfaceMaterial* currentSurface();
    HairMaterial material_;
    LightingSetup lighting_;
    GroomColor groomColor_;
    QLabel* colorNote_ = nullptr;  // "viewport only" without a HairColor node
    std::vector<std::pair<QString, SurfaceMaterial>> surfaces_;
    Segmented* tabs_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* pages_ = nullptr;
    QWidget* materialPage_ = nullptr;
    QWidget* lightingPage_ = nullptr;
    QWidget* renderPage_ = nullptr;
    QWidget* more_ = nullptr;
    RoundButton* moreButton_ = nullptr;
    bool moreShown_ = false;
    int maxHeight_ = 0;
    std::vector<RoundButton*> swatches_;
    std::vector<RoundButton*> lightingChips_;
    ValueSlider* melanin_ = nullptr;
    ValueSlider* redness_ = nullptr;
    ValueSlider* roughness_ = nullptr;
    Segmented* groomMode_ = nullptr;
    QWidget* groomPickedRow_ = nullptr;
    ColorEditor* groomPicked_ = nullptr;
    QWidget* tipColorRow_ = nullptr;
    ColorEditor* tipColor_ = nullptr;
    ValueSlider* tipStrength_ = nullptr;
    ValueSlider* hueRandom_ = nullptr;
    ValueSlider* saturationRandom_ = nullptr;
    QComboBox* pigment_ = nullptr;
    QComboBox* surface_ = nullptr;
    QComboBox* surfacePreset_ = nullptr;
    QComboBox* surfaceKind_ = nullptr;
    Segmented* sssMode_ = nullptr;
    std::vector<SurfaceRow> surfaceRows_;
    std::vector<LightRow> lightRows_;
    QComboBox* environment_ = nullptr;
    Segmented* toneMap_ = nullptr;
    Segmented* background_ = nullptr;
    QCheckBox* ambientOcclusion_ = nullptr;
    Segmented* quality_ = nullptr;
    Segmented* interactiveQuality_ = nullptr;
    std::vector<std::function<void()>> materialRefreshers_, lightingRefreshers_, surfaceRefreshers_;
    bool updating_ = false;
};

// ---- OverlayHost ---------------------------------------------------------------------------------
class OverlayHost : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Groom, Look, Cards, Export };
    // The first six have a button in the right rail (in this order). Help is the Help menu's
    // floating page (the shortcuts, About, updates), centered.
    enum class Panel { Guides, Nodes, Maps, Correctives, Look, Display, Brush, Node, Help, Count };

    explicit OverlayHost(Viewport* viewport, QWidget* parent = nullptr);

    Viewport* viewport() const { return viewport_; }
    void setMode(Mode mode);  // shows the mode's pieces; no signal
    Mode mode() const { return mode_; }
    void setMenu(QMenu* menu);  // what the top-left button pops up
    // The file name ("" no groom open: the empty-state card shows in the middle, the brush rail
    // and bar dim until one is).
    void setFileName(const QString& name, bool dirty);
    bool groomOpen() const { return !name_.isEmpty(); }
    // The empty-state card's recent files (at most five shown), newest first.
    void setRecentFiles(const QStringList& paths);
    GlassPanel* emptyCard() const { return emptyCard_; }
    int recentCount() const { return static_cast<int>(recentButtons_.size()); }
    RoundButton* graphPill() const { return graphPill_; }
    RoundButton* presetsPill() const { return presetsPill_; }
    void setUndoRedo(bool canUndo, bool canRedo, const QString& undoText, const QString& redoText);
    // The brush rail's label: the tool's name beside the active button ("Comb" with the key).
    void setTool(const std::string& brushMode, bool select);
    // The stats line (Groom): strands, the time per dab (0: the evaluation's), the path's badge.
    void setStats(std::size_t strands, double msPerDab, const QString& badge, const QColor& badgeColor);
    // The refine line (Look): frames accumulated of the target, or the time per frame.
    void setRefine(bool refining, int frames, int target, double msPerFrame);
    void setGraphShown(bool shown, int nodeCount);
    void setNodeCount(int nodeCount);  // the Graph pill's "N nodes" (after a preset, an edit)
    // A floating panel's content (the ordinary panels); shown and hidden by the right rail's
    // buttons, the brush bar's "..." and the node graph's selection.
    void setPanelContent(Panel panel, const QString& title, const QColor& dot, QWidget* content);
    void showPanel(Panel panel, bool show);
    void togglePanel(Panel panel);
    bool panelShown(Panel panel) const;
    // A panel's title row changed (the node panel takes the node's name, the help page its
    // subject): the floating panel is remade around the same content.
    void setPanelTitle(Panel panel, const QString& title, const QColor& dot);
    void setNodePanelTitle(const QString& title, const QColor& dot) { setPanelTitle(Panel::Node, title, dot); }
    FloatingPanel* panel(Panel panel) const { return panels_[static_cast<std::size_t>(panel)]; }
    // A message over the viewport for a few seconds (errors; the stderr line stays the record).
    void toast(const QString& text);
    // The node graph view (made by the host, wired by the window).
    NodeGraphView* graph() const { return graph_; }
    // The rails' hover state held (a screenshot of the names), no animation.
    void setRailsExpanded(bool brushRail, bool panelRail);
    // Where the Presets pill is (host coordinates): the window pops its menu there.
    QRect presetsPillRect() const;
    // The pieces the window wires.
    BrushRail* rail() const { return rail_; }
    PanelRail* panelRail() const { return panelRail_; }
    BrushBar* bar() const { return bar_; }
    LookCard* lookCard() const { return lookCard_; }
    CardsPanel* cardsPanel() const { return cardsPanel_; }
    ExportPanel* exportPanel() const { return exportPanel_; }
    Timeline* timeline() const { return timeline_; }
    Segmented* modes() const { return modes_; }
    // The timeline shows (the groom animates): placed in every mode; hidden otherwise.
    void setTimelineShown(bool shown);
    bool timelineShown() const { return timelineShown_; }

signals:
    void undoRequested();
    void redoRequested();
    void renderRequested();
    void frameAllRequested();
    void graphToggleRequested();
    void graphCloseRequested();  // the close button beside the Graph pill, or a mode clicked while the graph shows
    void presetsRequested();     // the Presets pill (and the empty card's "Start from a preset"): the window pops the presets menu
    void modeChanged(Mode mode);
    // The empty-state card: Open groom, New groom on mesh, a recent file; and a .groom, .obj or
    // .abc dropped on the window (any time).
    void openRequested();
    void newRequested();
    void fileDropped(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void relayout();
    void placePanel(Panel panel);
    void refreshToolLabel();
    void refreshEmptyCard();

    Viewport* viewport_;
    NodeGraphView* graph_ = nullptr;
    bool built_ = false;  // every piece exists: relayout may place them
    Mode mode_ = Mode::Groom;
    QMenu* menu_ = nullptr;
    // Top-left.
    RoundButton* menuButton_ = nullptr;
    QLabel* logo_ = nullptr;
    ShadowText* fileName_ = nullptr;
    QString name_;
    bool dirty_ = false;
    // Top-center and top-right.
    Segmented* modes_ = nullptr;
    RoundButton* undo_ = nullptr;
    RoundButton* redo_ = nullptr;
    RoundButton* render_ = nullptr;
    // Left.
    BrushRail* rail_ = nullptr;
    GlassPanel* toolLabel_ = nullptr;
    QLabel* toolName_ = nullptr;
    std::string toolMode_;
    bool toolSelect_ = false;
    // The middle, with no groom open.
    GlassPanel* emptyCard_ = nullptr;
    QBoxLayout* recentLayout_ = nullptr;
    QLabel* recentCaption_ = nullptr;
    std::vector<RoundButton*> recentButtons_;
    QStringList recentFiles_;
    // Right.
    PanelRail* panelRail_ = nullptr;
    std::vector<RoundButton*> panelButtons_;  // the rail's, in Panel's order (then frame all)
    std::vector<FloatingPanel*> panels_;
    std::vector<QString> panelTitles_;
    std::vector<QColor> panelDots_;
    // Bottom.
    BrushBar* bar_ = nullptr;
    ShadowText* stats_ = nullptr;
    ShadowText* refine_ = nullptr;
    RoundButton* graphPill_ = nullptr;
    RoundButton* presetsPill_ = nullptr;  // with the graph: the presets menu
    RoundButton* graphClose_ = nullptr;   // ... and the close button
    int nodeCount_ = 0;
    bool graphShown_ = false;
    GlassPanel* toast_ = nullptr;
    QLabel* toastText_ = nullptr;
    // Look, Cards, Export, the timeline.
    LookCard* lookCard_ = nullptr;
    CardsPanel* cardsPanel_ = nullptr;
    ExportPanel* exportPanel_ = nullptr;
    Timeline* timeline_ = nullptr;
    bool timelineShown_ = false;
};

}  // namespace groomlab::app
