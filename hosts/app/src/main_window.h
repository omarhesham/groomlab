// SPDX-License-Identifier: Apache-2.0
#pragma once

// The main window: the viewport fills it edge to edge and everything else floats over it
// (overlay.h): the menu button with every File, Edit, View and Tools action (and their
// shortcuts, which work from anywhere), the mode switcher (Groom, Look, Cards, Export), the
// brush rail and the brush bar, the panel buttons that open the guides, nodes, look and
// display panels on demand, the look card, the cards and export panels, the timeline (shown
// for an animated groom: each frame it names is evaluated and drawn, frames the clock passes
// meanwhile are dropped), the node graph (node_graph.h) and the stats. Errors go to stderr
// and a toast over the viewport, never a dialog: a scripted run (--screenshot, --ui-screenshot,
// an export) must not stop on one; file dialogs open only on a click.

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <QElapsedTimer>
#include <QImage>
#include <QPixmap>
#include <QMainWindow>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVector3D>

#include "groomlab/async_evaluation.h"

#include "brush_session.h"
#include "gpu_preview.h"
#include "groom_document.h"
#include "look.h"
#include "offscreen.h"
#include "renderer.h"
#include "surface.h"
#include "timeline.h"

class QAction;
class QCloseEvent;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QMenu;
class QSpinBox;
class QTimer;

namespace groomlab::app {

// GroomLab is free for any use, funded by donations: Help > Support GroomLab and About link here.
inline constexpr const char* kDonateUrl = "https://ko-fi.com/getgroomlab";
inline constexpr const char* kDiscordUrl = "https://discord.gg/sr2rxjRVkm";  // the GroomLab community
// GroomLab is open source (Apache-2.0): About links to the source.
inline constexpr const char* kRepoUrl = "https://github.com/thekesslereffect/groomlab";

class BrushPanel;
class UpdateChecker;
class CorrectivesPanel;
class GuidesPanel;
class LookPanel;
class MapsPanel;
class NodeGraphView;
class OverlayHost;
class ParamPanel;
class ValueSlider;
class Viewport;
class TransferWindow;
class ReferenceWindow;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();
    ~MainWindow() override;

    // A .groom file, or a mesh (.obj, .abc) a new groom is made on. False (and a message) on error.
    bool openPath(const QString& path);
    bool openGroom(const QString& path);
    bool newGroomOnMesh(const QString& meshPath);
    // "Start from a preset" with no groom open: a new groom on `meshPath` (empty: a file dialog,
    // only in a window someone sits at) with the preset applied. False when either fails.
    bool startFromPreset(const QString& nameOrPath, const QString& meshPath = {});
    void setPreviewDensity(double density);
    // The preview's point budget (thousands of points; 0: no limit), as the Display panel sets it.
    void setPointBudget(double thousands);
    // Commits evaluated off the UI thread (a stroke's release, a drag's end, an edit): the
    // window someone sits at (main.cpp); off (the default: tests, scripted runs), every
    // evaluation is done when the call that asked for it returns.
    // With them, the viewport's hair buffers for a landing commit are built off the UI thread too
    // (LookdevRenderer::setBuildAhead).
    void setAsyncCommits(bool on);
    bool asyncCommits() const { return asyncCommits_; }
    // A CPU brush stroke's dabs evaluated off the UI thread (GroomSession::detachStroke): the
    // window someone sits at (main.cpp); off (the default: tests, scripted runs), each dab is
    // evaluated and shown before the mouse event that made it returns.
    void setAsyncDabs(bool on) {
        if (!on) landDab(true);
        asyncDabs_ = on;
    }
    bool asyncDabs() const { return asyncDabs_; }
    // A window someone sits at (main.cpp): unsaved edits are not discarded without asking (closing,
    // Quit, Open, New, a dropped or recent file, Reload: confirmDiscard's prompt), and info messages
    // (saved, opened, a preset applied, hints) show as toasts. Off (the default: tests, scripted
    // runs, --ui-screenshot), nothing asks and only errors toast.
    void setInteractive(bool on) { interactive_ = on; }
    // The rails' names beside their icons (View > Rail Labels; saved as ui/railLabels). A first
    // run (no saved choice) shows them for that run alone: firstRunRailLabels, for the window
    // someone sits at.
    void setRailLabels(bool shown, bool save = true);
    bool railLabels() const;
    void firstRunRailLabels();
    bool interactive() const { return interactive_; }
    // `proceed` now when nothing would be lost (no groom, no unsaved edit, not interactive); else
    // the glass prompt "Unsaved changes" over the viewport (never a dialog, the window stays usable):
    // Save (then `proceed` once saved), Discard (`proceed`), Cancel. `doing` ends its sentence
    // ("Save them before closing?").
    void confirmDiscard(const QString& doing, std::function<void()> proceed);
    // File > Reload from File: the groom read again, asked first when it has unsaved edits (they
    // and the undo history go).
    void askReload();
    // An export or a simulation is running ("" none: what busyJob() names). It runs on this thread
    // with the window's events in between, so its Cancel takes a click: meanwhile every other
    // click, key and shortcut is held back (eventFilter), Esc cancels, closing the window stops
    // the job first and then closes, and the entry points that would run another job, open a
    // file, undo or bake refuse with a message.
    bool busy() const { return !job_.isEmpty(); }
    const QString& busyJob() const { return job_; }
    // Stops the running job (as its Cancel button does); false when none runs.
    bool cancelJob();
    // Whether a dab is still evaluating on the worker, and how many have landed.
    bool dabPending() const { return dabs_.pending(); }
    std::uint64_t dabsLanded() const { return dabsLanded_; }
    // Whether a commit is still evaluating on the worker (what the window shows is from before it).
    bool commitPending() const { return commits_.pending(); }
    // "node.param=value" (a JSON value, or a bare number or word) on the open groom; false (and
    // a message) when refused.
    bool setParamText(const QString& text);
    // A node bypassed by name; false (and a message) when refused.
    bool setBypassText(const QString& node);
    // A graph JSON file (a preset's) replacing the open groom's graph; false (and a message) on error.
    bool loadGraphFile(const QString& path);
    // A graph preset by name (the shipped and the user's: graph_presets.h) or by file path onto the
    // open groom, as the Presets menu does: fitted to the scalp, the maps it reads made, one undo
    // step. False (and a message) when refused.
    bool applyPreset(const QString& nameOrPath);
    // The preset inserted into the open groom's graph as one Compound after the graph's selected
    // node (before the Output when none is), as the Presets menu's Insert after selected node
    // does (insertPresetFile). False (and a message) when refused.
    bool insertPreset(const QString& nameOrPath);
    // Paint this param (the param panels' Paint action, the maps panel's New map for a setting):
    // the map it reads, or one made (`kind`: vertices or texture) and linked as one undo step
    // (GroomDocument::paintParam), picked, then the Paint brush on it toward 1 (Ctrl: toward 0)
    // in brush mode. False (and a message) when refused.
    bool paintParam(const QString& node, const QString& param, const QString& kind = QStringLiteral("vertices"));
    // The open groom's graph saved as a user preset (the Presets menu's Save as Preset); false
    // (and a message) when refused (a bad name, a file in the way unless `overwrite`).
    bool savePreset(const QString& name, bool overwrite = false);
    // Copy Graph and Paste Graph: the whole graph as text on the clipboard, and back.
    void copyGraph();
    void pasteGraph();
    // Saves the open groom to `path`; false (and a message) on error.
    bool saveTo(const QString& path);
    // The open groom read again from its file (File > Reload: another host's edits, the app's
    // own save); false (and a message) without a file or on error.
    bool reloadGroom();
    // The groom moved onto the mesh at `path` (Tools > Transfer Groom to Mesh: the core's
    // transfer, as Maya's utility): guides, parts and flow re-rooted, the maps projected; false
    // (and a message) on error. `fit`: bounds, else closest.
    bool transferToMesh(const QString& path, bool fit = true, bool maps = true);
    // The same by any method (transfer.h); a marker method without markers in `options` uses
    // the groom's stored ones (its file's "transferMarkers").
    bool transferToMesh(const QString& path, const TransferOptions& options, bool maps = true);
    // Marker pairs from a JSON file (transfer.h's format: Maya's transferMarkers attribute, a groom
    // file's "transferMarkers"); false (and a message) when it cannot be read.
    bool readTransferMarkers(const QString& path, std::vector<TransferMarker>* markers);
    // Tools > Transfer Groom to Mesh: the transfer window (transfer_window.h: two views, marker
    // points clicked on the groom's scalp and the new mesh, the method and Transfer), made once and
    // raised; never modal.
    TransferWindow* openTransferWindow();
    TransferWindow* transferWindow() const { return transferWindow_; }
    // Tools > Guides from Reference (experimental): the Reference window (reference_window.h: the photo
    // and the scalp side by side, landmarks, flow arrows, mask touch-ups, Generate), made once and
    // raised again; it follows the open groom.
    ReferenceWindow* openReferenceWindow();
    ReferenceWindow* referenceWindow() const { return referenceWindow_; }
    // The last message reported (an error's, a transfer's), for windows that show it themselves.
    const QString& lastReport() const { return lastReport_; }
    // Tools > Turntable: the camera turns a degree a frame while on.
    void setTurntable(bool on);
    bool turntable() const;
    // The camera as a line ("camera eye x y z target x y z fov f"): a render elsewhere matches it.
    QString cameraDescription() const;

    // ---- Time, exports, cards (scripted runs and the panels alike) --------------------------
    // The frame the groom is evaluated at (the timeline follows); false without a groom.
    bool setFrame(double frame);
    bool setFollowScalp(bool on);
    // The simulation (the timeline's Simulate, Bake and Clear): the Simulate nodes stepped to
    // `lastFrame` (the timeline's last when below 0) into the cache, or baked beside the groom
    // (GroomDocument::bakeSimulation); false (and a message) without a Simulate node or on failure.
    bool simulateTo(double lastFrame = -1.0);
    bool bakeSimulation(double lastFrame = -1.0);
    void invalidateSimulation();
    // An export of the open groom to `path`: `target` "alembic", "usd", "unreal", "unity" or
    // "cards" (ExportPanel::targetName); the export panel shows the progress and the result,
    // the stderr line says what was written. False (and a message) when refused or failed.
    bool exportTo(const QString& target, const QString& path, const ExportOptions& options);
    bool setExportTarget(const QString& target);
    // Bakes the cards' maps to <directory>/<name>_*.tga (the groom's default place when
    // empty); the cards panel shows the atlas and the viewport the textured cards.
    bool bakeCardsTo(const QString& directory, const QString& name, int resolution = 0, float occlusion = 1.0f);
    // Cards mode's preview: the cards instead of the strands.
    void setShowCards(bool on);
    void applyMaterialPreset(const QString& name);
    void applyLightingPreset(const QString& name);
    // "interactive" or "refine" (the lighting's quality mode).
    void setQuality(const QString& name);
    // "adaptive" or "full" (how a stream of changes draws: LightingSetup::interactiveQuality).
    void setInteractiveQuality(const QString& name);
    // "none", "aces" or "agx".
    void setToneMap(const QString& name);
    void setLookdev(bool on);
    void setShowScalp(bool on);
    // The GPU look preview (gpu_preview.h; the Display panel's toggle): drags and brush dabs run
    // on the GPU where the device has an engine. On by default (GROOMLAB_APP_GPU_PREVIEW forces it).
    void setGpuPreview(bool on);
    bool gpuPreview() const { return gpu_.enabled(); }
    GpuLookPreview& gpuPreviewSession() { return gpu_; }
    // Turns the camera (degrees) and multiplies its distance, from where frame-all put it.
    void adjustCamera(float yawDegrees, float pitchDegrees, float dolly);
    // Points the camera at a scene point from `distance` away (0: keeps the distance).
    void lookAt(const QVector3D& target, float distance);
    GroomDocument* document() { return document_.get(); }

    // ---- The UI --------------------------------------------------------------------------------
    // "groom", "look", "cards" or "export" (the mode switcher); false for another word.
    bool setUiMode(const QString& name);
    void setGraphShown(bool shown);
    bool graphShown() const;
    // "guides", "nodes", "maps", "look", "display" or "brush": opens that floating panel; false
    // for another word.
    bool showUiPanel(const QString& name);
    // Help > Keyboard Shortcuts or About as a floating page ("shortcuts", "about"); Docs opens
    // the docs folder (beside the executable, else the source tree's) or the web docs.
    bool showHelp(const QString& page);
    void openDocs();
    // Help > Check for Updates: reads the app's release feed (update_check.h) and shows what it found
    // in the Help page (the newer version's notes and download link); nothing is installed.
    void checkForUpdates();
    // Help > Support GroomLab: the donations page (kDonateUrl) in the browser.
    void openDonate();
    // View > Theme: saves the theme (QSettings ui/theme) for the next start, when the window is
    // built in its colors; false (nothing saved) for a theme that isn't embedded.
    bool chooseTheme(const QString& name);
    // "material", "lighting" or "render": the look card's tab (Look mode); "more" after a comma
    // ("material,more") opens the material's full settings. False for another word.
    bool setUiLookTab(const QString& name);
    // Selects a node in the graph (shown if it was not), so its params float beside it; false
    // (and a message) for no such node.
    bool selectUiNode(const QString& name);
    // The rails held in their hover state, the names shown ("rail", "panels" or "all": the
    // scripted screenshot's --ui-hover), or "labels": the names kept shown as on a first run
    // (Rail Labels, not saved); false for another word.
    bool setUiHover(const QString& name);
    // A menu laid out for a scripted screenshot ("presets": the Presets pill's menu, its Add-ons
    // submenu and the first add-on's Apply / Insert, drawn by grabUi as they would pop up; a
    // real popup closes when the window is not the active one); false for another word.
    bool setUiMenu(const QString& name);
    // A prompt or a toast shown for a scripted screenshot ("unsaved", "reload", "info", "error");
    // false for another word.
    bool setUiPrompt(const QString& name);
    // The graph shown with its Add Node search open and `text` typed (a scripted screenshot's
    // --ui-search).
    bool setUiSearch(const QString& text);
    // The display options as "name=value" (colorMode solid|colorSet|attribute|random|clump|density,
    // colorSource, colorMin, colorMax, guides, guidePoints, scalpMap, scalpMapChannel, cardsView,
    // lookdev, scalp); false (and a message) when refused.
    bool setDisplayText(const QString& text);
    MapsPanel* mapsPanel() { return maps_; }
    OverlayHost* overlay() { return host_; }
    // The window as shown, with the floating UI over the viewport's frame: the viewport's
    // texture with the pieces rendered over it (QWidget::grab cannot see a QRhiWidget's frame).
    // `scene`, when given, stands in for the viewport's frame.
    QImage grabUi(const QImage& scene = QImage());
    // grabUi after the next frame the viewport draws (and a moment for the layout to settle),
    // then `done` with it: the scripted --ui-screenshot. Without a window system
    // (QT_QPA_PLATFORM=offscreen: the viewport never draws) the scene comes from an offscreen
    // device instead, so the UI can be captured with no window on the desktop.
    void captureUi(const std::function<void(const QImage&)>& done);
    // One step of an interaction (mode: orbit, hover, stroke, param) on every frame the window
    // draws, N frames; reports the time between frames on screen, in the input's handling and
    // in the viewport's render.
    void benchmarkLive(int frames, const QString& mode, const std::function<void(const QString&)>& done);

    // ---- Tools ------------------------------------------------------------------------------
    BrushSession& brush() { return brush_; }
    // "camera", "brush" or "select" (the viewport's mode); false for another word.
    bool setToolMode(const QString& name);
    // "name=value" on the brush settings (mode, radius, strength, hardness, symmetry, level,
    // group, pressure, maskByScalp, selectedOnly, collide, stretch, sculptDensity); false (and
    // a message) when refused.
    bool setBrushText(const QString& text);
    // Alt+Shift+X: symmetry on (World X, or the mode it had) or off, said in a toast.
    void toggleSymmetry();
    // A stroke through the brush session from points in pixels of a view `size` big (y down
    // from the top, as an image), as the mouse would make it: the groom evaluated and shown
    // after it. False (and a message) when none could be made. `drawDabs`: each dab (and the
    // press and the release) is drawn offscreen at `size` as an artist would see it, and the
    // times to the drawn frame printed (a headless benchmark).
    bool applyStroke(const std::vector<QPointF>& points, const QSize& size, bool ctrl = false, bool shift = false, bool drawDabs = false);
    // The widget position of a world point in a view `size` big (y down), as the strokes take
    // them; false when behind the camera.
    bool projectToView(const QVector3D& point, const QSize& size, QPointF* out);

    // ---- Headless benchmarks ------------------------------------------------------------------
    // A drag of node.param from `from` to `to` in `steps` values, each step previewed as the
    // sliders do (the GPU's when the preview is on and takes it, else the CPU's) and drawn
    // offscreen at `size`, then the release (the value committed on the CPU) drawn: the times
    // to each drawn frame printed. False (and a message) when the param cannot be set.
    bool benchmarkDrag(const QString& node, const QString& param, double from, double to, int steps, const QSize& size);
    // `frames` frames at `size`, the camera turning 6 degrees a frame: the frames per second.
    double benchmarkOrbit(const QSize& size, int frames);

    // ---- Surfaces -------------------------------------------------------------------------
    // A mesh drawn beside the groom (an OBJ with its material library's maps) shaded by a
    // surface preset (surfacePresetNames: clay, skin, ...); its occlusion baked by
    // `occlusionRays` rays against every prop (0: none). False (and a message) on error.
    bool addProp(const QString& path, const QString& materialPreset, int occlusionRays = 32);
    // An eyeball at `center` looking along `forward`, of `radius`, shaded by an eye preset,
    // its occlusion (the lids) baked against the props.
    bool addEye(const QVector3D& center, const QVector3D& forward, float radius, const QString& materialPreset,
                int occlusionRays = 32);
    // An eye from "cx,cy,cz,fx,fy,fz,r" (the command line's --eye), the default eye preset.
    bool addEyeText(const QString& text);
    // File > Add Prop Mesh... (an .obj beside the groom, clay), File > Add Eye... (a glass prompt
    // with the eye's center, direction and radius, guessed in front of the scene) and File > Save
    // Image... (the view as the viewport draws it, to a PNG): the file dialogs only in a window
    // someone sits at (never in a scripted run).
    void chooseProp();
    void askEye();
    void chooseSaveImage();
    // The view rendered at the viewport's size (Look mode: refined over several frames) and
    // written as an image; false (and a message) on error.
    bool saveImage(const QString& path);
    // "name=value" on the last prop's (or eye's) material (setSurfaceParam's names); false
    // (and a message) when refused or without a prop.
    bool setPropParam(const QString& text);
    // The scalp's surface preset.
    void setScalpMaterial(const QString& preset);
    const std::vector<SurfacePtr>& props() const { return props_; }

    // The scene as the viewport shows it, rendered without the window (offscreen.h): `frames`
    // frames, the last one returned (refine mode accumulates them), `msPerFrame` the time of
    // the frames after the first (`gpuMsPerFrame` the GPU's own time of them, 0 without
    // timestamps). Null when the device gives no pixels.
    QImage renderOffscreen(const QSize& size, int frames, double* msPerFrame = nullptr, QString* api = nullptr,
                           int* surfaceDraws = nullptr, double* firstFrameMs = nullptr, double* gpuMsPerFrame = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;  // asks first when the groom has unsaved edits
    // Tab in a text field moves the focus (the window's Tab shortcut, the node graph, waits).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildMenu();
    void buildPanels();
    void wireOverlay();
    // The presets menu (the Graph pill's Presets, and View > Graph Presets): a submenu per
    // category, then Save as Preset, Copy Graph and Paste Graph; filled when it opens.
    void fillPresetsMenu();
    void showPresetsMenu();  // above the Presets pill
    void wireTimelineCardsAndExport();
    void setDocument(std::unique_ptr<GroomDocument> document);
    // The timeline's range, fps, frame and Follow chip from the document (shown when it animates).
    void refreshTimeline();
    // A frame the timeline named: the document evaluated there and shown (quiet: no stderr line).
    void showFrame(double frame);
    // The timeline's sim controls and its strip of simulated frames (shown with a Simulate node).
    void refreshSimulation();
    bool runSimulation(double lastFrame, bool bake);
    // The cards panel's stats and the viewport's textured cards from the last view.
    void refreshCards();
    void bakeCards();   // the panel's Bake button: beside the groom
    void runExport();   // the panel's Export button
    void chooseExportPath();
    void refreshExportState();
    // The groom evaluated and shown; `quiet`: a stroke's dab (no stderr line, no warm-up).
    void reevaluate(bool quiet = false);
    // Commits off the UI thread (setAsyncCommits): reevaluate detaches a commit's evaluation
    // (GroomSession::detach) onto a worker and keeps drawing what it shows; landCommit takes it
    // in once evaluated (shown, unless `show` is false: the caller evaluates next), settleCommit
    // waits for it where the session's results are needed (a slider's drag starting).
    bool commitAsync(const ViewRequest& request);
    void landCommit(bool show);
    void settleCommit();
    void paramDragged(bool on);
    // The view request the window evaluates (density, budget, cards), before a stroke's stopAt.
    ViewRequest viewRequest() const;
    // Dabs off the UI thread (setAsyncDabs): dabAsync hands a CPU stroke's dab to the worker
    // (or, one evaluating, marks one wanted); startDab detaches it (GroomSession::detachStroke);
    // landDab takes it in once evaluated (shown unless `show` is false) and starts the wanted one.
    bool dabAsync();
    bool startDab();
    void landDab(bool show);
    // The stroke document evaluated ahead at the brush's density (BrushSession::warm), so a
    // stroke's press makes again only what it changes: between strokes in brush mode, and
    // before a scripted stroke that draws its dabs.
    void warmStroke();
    void redisplay();
    void applyLook();
    void refreshNodes();
    // The look card's groom color: the last HairColor node's melanin, redness and roughness (the
    // last node of `type`, nearest the output in flow order).
    std::string lastNodeOfType(const char* type) const;
    std::string hairColorNode() const { return lastNodeOfType("HairColor"); }
    void refreshGroomColor();
    void refreshGroups();  // the brush panel's group list and the guides panel
    void refreshUndoActions();
    void refreshTitle();
    void refreshStats();
    // refreshStats once the frames stop (kStatsIdleMs without one), or now when the line is
    // older than kStatsMaxAgeMs.
    void scheduleStats();
    QTimer* statsTimer_ = nullptr;
    QElapsedTimer statsAge_;
    static constexpr int kStatsIdleMs = 150;
    static constexpr int kStatsMaxAgeMs = 500;
    void refreshBrushBar();  // the bar and the rail from the brush settings and the tool mode
    void refreshSurfaces();  // the look panel's surface list: the scalp and the props
    void refreshDisplayChoices();  // the color sources and the maps the display panel offers
    void applyDisplayOptions();    // the display panel's choices into the settings and the renderer
    void applyProps();       // the props (and the textured cards) to the renderer
    Bounds sceneBounds() const;  // the document's with the props'
    // An error (stderr and a red toast) or, `error` false, a line for stderr only (evaluations, timings).
    void report(const QString& text, bool error = true);
    // A result or a hint the artist should see (saved, opened, a preset applied, the brush's keys):
    // stderr and a brief info toast in an interactive window.
    void notify(const QString& text);
    // A long step about to block the window (opening, baking, transferring): the line toasts and
    // paints before the work starts (an interactive window), stderr either way.
    void working(const QString& text);
    // The running job's start and end (busy()): `cancel` is the control whose clicks still get
    // through (the export panel's Cancel, the timeline's).
    void beginJob(const QString& what, QObject* cancel);
    void endJob();
    // Refuses `doing` while a job runs ("Wait for the export to finish (or Cancel it) before ...").
    bool refuseWhileBusy(const QString& doing);
    // The window's events between a job's steps (user input filtered by eventFilter).
    void pumpJobEvents();
    void chooseOpen();
    void chooseMesh();
    void save();
    void saveAs();
    void exportAlembic();
    void frameAll();
    void undo();
    void redo();
    void guidesEdited();  // a stroke committed or the guides panel edited: shown, undoable
    void graphEdited();   // the graph view or a param panel edited the graph: shown, undoable
    // The offscreen view the headless benchmarks draw into (made on first use at `size`, fed
    // what the viewport's renderer has, redisplayed with it from then on), and one frame of it.
    bool ensureHeadless(const QSize& size);
    OffscreenResult headlessFrame(bool readback = false);

    std::unique_ptr<GroomDocument> document_;
    std::vector<std::pair<QPixmap, QPoint>> uiMenus_;  // setUiMenu's menus as grabbed, and where grabUi draws them (host coordinates)
    GpuLookPreview gpu_;
    struct Headless {
        std::unique_ptr<OffscreenView> view;
        GroomRenderer renderer;
    };
    std::unique_ptr<Headless> headless_;
    DetachedRunner commits_;
    QTimer* commitTimer_ = nullptr;
    bool asyncCommits_ = false;
    bool landing_ = false;       // landCommit's evaluation (never detached again)
    QElapsedTimer commitClock_;  // since the last commit was detached
    double lastCommitMs_ = 0.0;  // from its detach to its landing (the live benchmark)
    std::uint64_t commitsLanded_ = 0;
    bool paramDragging_ = false;  // a slider's drag is live (its steps evaluate here)
    DetachedRunner dabs_;
    QTimer* dabTimer_ = nullptr;
    bool asyncDabs_ = false;
    bool pressShown_ = false;   // the live stroke's press was evaluated here (its dabs may go to the worker)
    bool dabWanted_ = false;    // a dab came while one evaluated: the next goes from the stroke as it is
    bool landingDab_ = false;   // landDab's evaluation (never detached again)
    QElapsedTimer dabClock_;    // since the last dab was detached
    double lastDabRunMs_ = 0.0;  // the last landed dab's evaluation on the worker
    std::uint64_t dabsLanded_ = 0;
    std::optional<ViewRequest> lastRequest_;  // the last evaluation's (time included): a commit's view must match it
    std::string viewNode_;  // the graph's View Up to Here ("" the output): what the viewport shows
    std::vector<SurfacePtr> props_;
    // (appearanceColors: the frame's lines draw without a hair BSDF; the lookdev reads the base colors itself.)
    DisplaySettings settings_ = [] {
        DisplaySettings s;
        s.appearanceColors = true;
        return s;
    }();
    BrushSession brush_;
    Viewport* viewport_ = nullptr;
    OverlayHost* host_ = nullptr;
    NodeGraphView* graph_ = nullptr;
    QListWidget* nodes_ = nullptr;
    ParamPanel* params_ = nullptr;       // the nodes panel's
    ParamPanel* graphParams_ = nullptr;  // the graph's selected node's
    LookPanel* look_ = nullptr;
    BrushPanel* brushPanel_ = nullptr;
    QLabel* help_ = nullptr;  // the Help pages' text
    UpdateChecker* updates_ = nullptr;  // Help > Check for Updates (made on first use)
    GuidesPanel* guidesPanel_ = nullptr;
    CorrectivesPanel* correctives_ = nullptr;
    TransferWindow* transferWindow_ = nullptr;  // Tools > Transfer Groom to Mesh (a child window)
    ReferenceWindow* referenceWindow_ = nullptr;  // Tools > Guides from Reference (a child window)
    QString lastReport_;
    QTimer* turntable_ = nullptr;  // the camera turning while Tools > Turntable is on
    MapsPanel* maps_ = nullptr;
    ValueSlider* density_ = nullptr;
    ValueSlider* budget_ = nullptr;
    QCheckBox* guides_ = nullptr;
    QCheckBox* tubes_ = nullptr;  // the guides' tubes (tube guides) as shaded surfaces
    QCheckBox* lighting_ = nullptr;
    QCheckBox* widthCoverage_ = nullptr;
    QCheckBox* coverageDensity_ = nullptr;
    QCheckBox* clumpLod_ = nullptr;
    QCheckBox* scalp_ = nullptr;
    QCheckBox* lookdev_ = nullptr;
    QCheckBox* gpuPreview_ = nullptr;
    // Display options: the strand colors, the guides, the map on the scalp, the cards.
    QComboBox* colorMode_ = nullptr;
    QComboBox* colorSource_ = nullptr;
    QDoubleSpinBox* colorMin_ = nullptr;
    QDoubleSpinBox* colorMax_ = nullptr;
    QComboBox* guidePoints_ = nullptr;
    ValueSlider* guideWidth_ = nullptr;
    QComboBox* scalpMap_ = nullptr;
    QComboBox* scalpMapChannel_ = nullptr;
    QCheckBox* cardsView_ = nullptr;
    // Scaling with the scalp (GroomDocument::setScaleSource): what it follows and the groom's scale
    // ("1.00x"), refreshed with the display choices; the session's scale notice last shown.
    QComboBox* scaleSource_ = nullptr;
    const void* scaleNoticeSession_ = nullptr;
    std::uint64_t scaleNoticeShown_ = 0;
    QLabel* groomScale_ = nullptr;
    bool lookdevWanted_ = true;  // the Lookdev box: the colored modes draw lines while they are on
    QMenu* menu_ = nullptr;
    QMenu* presetsMenu_ = nullptr;
    QAction* undo_ = nullptr;
    QAction* redo_ = nullptr;
    QAction* save_ = nullptr;
    QAction* saveAs_ = nullptr;
    QAction* export_ = nullptr;
    QAction* saveImage_ = nullptr;
    QAction* cameraMode_ = nullptr;
    QAction* brushMode_ = nullptr;
    QAction* selectMode_ = nullptr;
    QAction* graphAction_ = nullptr;
    QAction* railLabels_ = nullptr;
    // The actions that need a groom open (disabled without one), Reload a file too.
    QAction* reload_ = nullptr;
    QAction* transfer_ = nullptr;
    QAction* reference_ = nullptr;
    QAction* deleteGuides_ = nullptr;
    bool interactive_ = false;
    bool closing_ = false;  // the prompt let the window close (or Quit): no asking again
    QString evaluation_;  // the last evaluation's summary (stderr)
    double lastEvaluateMs_ = 0.0;
    double lastDabMs_ = 0.0;  // the last stroke's time per dab (0: none yet)
    std::string strokeCpuWhy_;  // why the last stroke's dabs could not run on the GPU ("" they did)
    bool framed_ = false;     // the camera framed the first groom
    bool updatingLook_ = false;
    bool groomColorDragging_ = false;  // the look card's slider drags the HairColor node's param
    nlohmann::json groomColorBefore_;
    // The textured cards (card_preview.h): a surface drawn as a prop while the cards show with
    // their bake; made again when the cards or the bake change.
    SurfacePtr cardSurface_;
    std::uint64_t cardSurfaceVersion_ = 0;
    std::string cardSurfaceMap_;
    bool exportCancel_ = false;  // the running job's Cancel was asked for (an export's or a simulation's)
    QString job_;                // the running job ("export", "simulation"; "" none)
    QObject* jobCancel_ = nullptr;  // the control that cancels it
    bool closeAfterJob_ = false;    // the window was closed during the job: closed once it stops
    QAction* symmetryAction_ = nullptr;  // Alt+Shift+X
    std::string lastSymmetry_ = "x";     // what Alt+Shift+X turns on again (World X at first)
    std::size_t shownFrames_ = 0;  // frames the timeline showed (playback's own count)
};

}  // namespace groomlab::app
