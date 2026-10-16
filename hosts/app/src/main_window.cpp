// SPDX-License-Identifier: Apache-2.0
#include "main_window.h"

#include <algorithm>
#include <thread>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QListWidget>
#include <QMenu>
#include <QTimer>
#include <QUrl>

#include "brush_panel.h"
#include "cards_panel.h"
#include "export_panel.h"
#include "glass.h"
#include "guides_panel.h"
#include "node_graph.h"
#include "overlay.h"
#include "theme.h"
#include "update_check.h"
#include "viewport.h"


namespace groomlab::app {

MainWindow::MainWindow() {
    setWindowTitle(QStringLiteral("GroomLab"));
    resize(1440, 900);
    theme::loadFonts();
    viewport_ = new Viewport();
    host_ = new OverlayHost(viewport_, this);
    setCentralWidget(host_);
    graph_ = host_->graph();
    viewport_->setBrushSession(&brush_);
    graph_->setViewport(viewport_);
    // The GPU look preview runs in the viewport's frames (and the headless benchmarks'), on by
    // default where the device has an engine.
    gpu_.setEnabled(GpuLookPreview::preference(true));
    viewport_->renderer().setGpuPreview(&gpu_);
    buildMenu();
    buildPanels();
    wireOverlay();
    wireTimelineCardsAndExport();
    // The stats line follows the frames at most a few times a second and once they stop: the
    // floating widgets sit over the viewport's texture, and while one of them changes Qt paints
    // and uploads the whole window's widgets again (about 10 ms at 1440p), so a line changed every
    // frame would cost every frame that much.
    statsTimer_ = new QTimer(this);
    statsTimer_->setSingleShot(true);
    statsTimer_->setInterval(kStatsIdleMs);
    connect(statsTimer_, &QTimer::timeout, this, [this] { refreshStats(); });
    connect(viewport_, &Viewport::frameDrawn, this, [this] {
        scheduleStats();
        // While frames follow each other (a refine's), the timer below may wait behind them:
        // a commit evaluated lands after this frame (a posted call goes before timers).
        if (commits_.ready()) {
            QMetaObject::invokeMethod(this, [this] {
                if (commits_.ready() && !gpu_.dragging()) landCommit(true);
            }, Qt::QueuedConnection);
        }
        // (A dab evaluated on the worker the same way.)
        if (dabs_.ready()) {
            QMetaObject::invokeMethod(this, [this] {
                if (dabs_.ready()) landDab(true);
            }, Qt::QueuedConnection);
        }
    });
    // A dab on the worker lands once evaluated (polled as the commits are).
    dabTimer_ = new QTimer(this);
    dabTimer_->setInterval(2);
    dabTimer_->setTimerType(Qt::PreciseTimer);  // (a coarse one waits for Windows' 15.6 ms tick)
    connect(dabTimer_, &QTimer::timeout, this, [this] {
        if (dabs_.ready()) landDab(true);
    });
    // A commit on the worker lands once evaluated (polled: the worker knows nothing of Qt).
    commitTimer_ = new QTimer(this);
    commitTimer_->setInterval(4);
    commitTimer_->setTimerType(Qt::PreciseTimer);  // (a coarse one waits for Windows' 15.6 ms tick)
    connect(commitTimer_, &QTimer::timeout, this, [this] {
        // (Not under a GPU drag, whose runs read the session's document: once it ends.)
        if (commits_.ready() && !gpu_.dragging()) landCommit(true);
    });
    // The worker's loops leave a quarter of the cores to the drawing (as Maya's commits do).
    commits_.setThreads(std::max(1u, std::thread::hardware_concurrency() * 3 / 4));
    dabs_.setThreads(std::max(1u, std::thread::hardware_concurrency() * 3 / 4));
    // The brush: each dab evaluates the preview and shows it; the release shows the committed
    // stroke and makes it undoable. With dabs off the UI thread (setAsyncDabs), a CPU stroke's
    // dabs after its press evaluate on a worker and each result shows as it lands.
    connect(viewport_, &Viewport::strokeStarted, this, [this] { pressShown_ = false; });
    connect(viewport_, &Viewport::strokeDabbed, this, [this] {
        if (pressShown_ && dabAsync()) return;
        QElapsedTimer timer;
        timer.start();
        reevaluate(true);
        lastDabMs_ = timer.nsecsElapsed() / 1e6;
        pressShown_ = brush_.stroking();
    });
    // The release commits what the tool holds: no dab after it. One still on the worker is not
    // waited for (the commit evaluates the view's document, not the stroke's): it lands unshown,
    // or the next evaluation here takes it in first.
    connect(viewport_, &Viewport::strokeReleasing, this, [this] { dabWanted_ = false; });
    connect(viewport_, &Viewport::strokeEnded, this, [this] { guidesEdited(); });
    connect(viewport_, &Viewport::brushSettingsChanged, this, [this] {
        brushPanel_->refresh();
        refreshBrushBar();
    });
    connect(viewport_, &Viewport::selectionChanged, this, [this] {
        redisplay();
        guidesPanel_->refresh();
    });
    connect(viewport_, &Viewport::deleteRequested, this, [this] { guidesPanel_->deleteSelected(); });
    connect(viewport_, &Viewport::message, this, [this](const QString& text) { report(text); });
    connect(viewport_, &Viewport::toolModeChanged, this, [this](Viewport::ToolMode mode) {
        cameraMode_->setChecked(mode == Viewport::ToolMode::Camera);
        brushMode_->setChecked(mode == Viewport::ToolMode::Brush);
        selectMode_->setChecked(mode == Viewport::ToolMode::Select);
        refreshBrushBar();
        if (mode == Viewport::ToolMode::Brush) {
            report(tr("Brush: drag to stroke, Ctrl the opposite, Shift smooths, Shift+click a guide solos it (off a guide: "
                      "unfreeze all); "
                      "B+drag or middle-drag sets the radius; Alt orbits; Esc leaves"), false);
            if (document_) reevaluate();  // warms the stroke document
        } else if (mode == Viewport::ToolMode::Select) {
            report(tr("Select: click or box guides (the rest freeze; nothing picked unfreezes all); Shift toggles, Ctrl "
                      "deselects, Ctrl+Shift adds; Delete removes them"), false);
        }
    });
    refreshTitle();
    refreshBrushBar();
    refreshStats();
    viewport_->setFocus();
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenu() {
    // One menu behind the top-left button; its shortcuts work from anywhere in the window.
    menu_ = new QMenu(this);
    auto add = [this](QMenu* menu, const QString& text, const QKeySequence& key, auto slot) {
        QAction* a = menu->addAction(text, this, slot);
        if (!key.isEmpty()) {
            a->setShortcut(key);
            a->setShortcutContext(Qt::WindowShortcut);
            addAction(a);
        }
        return a;
    };
    QMenu* file = menu_->addMenu(tr("&File"));
    add(file, tr("&Open Groom..."), QKeySequence::Open, [this] { chooseOpen(); });
    add(file, tr("&New Groom on Mesh..."), QKeySequence::New, [this] { chooseMesh(); });
    file->addSeparator();
    save_ = add(file, tr("&Save"), QKeySequence::Save, [this] { save(); });
    saveAs_ = add(file, tr("Save &As..."), QKeySequence::SaveAs, [this] { saveAs(); });
    add(file, tr("&Reload from File"), QKeySequence(Qt::CTRL | Qt::Key_R), [this] { reloadGroom(); });
    export_ = add(file, tr("&Export..."), QKeySequence(Qt::CTRL | Qt::Key_E), [this] { exportAlembic(); });
    file->addSeparator();
    add(file, tr("&Quit"), QKeySequence::Quit, [] { qApp->quit(); });

    QMenu* edit = menu_->addMenu(tr("&Edit"));
    undo_ = add(edit, tr("&Undo"), QKeySequence::Undo, [this] { undo(); });
    redo_ = add(edit, tr("&Redo"), QKeySequence::Redo, [this] { redo(); });
    auto* redoToo = new QAction(this);  // Ctrl+Shift+Z as well as Ctrl+Y
    redoToo->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z));
    connect(redoToo, &QAction::triggered, this, [this] { redo(); });
    addAction(redoToo);

    QMenu* view = menu_->addMenu(tr("&View"));
    add(view, tr("&Frame All"), QKeySequence(Qt::Key_F), [this] {
        // F over the graph fits the graph; anywhere else the camera frames the scene.
        if (graphShown() && graph_->hasFocus()) graph_->fitAll();
        else frameAll();
    });
    graphAction_ = add(view, tr("Node &Graph"), QKeySequence(Qt::Key_Tab), [this] {
        // Tab shows the graph; shown, the graph's own Tab opens its search (the view has the focus).
        if (!graphShown()) setGraphShown(true);
        else graph_->openSearch();
    });
    graphAction_->setCheckable(true);
    presetsMenu_ = view->addMenu(tr("Graph &Presets"));
    presetsMenu_->setToolTipsVisible(true);  // each preset's description
    connect(presetsMenu_, &QMenu::aboutToShow, this, [this] { fillPresetsMenu(); });
    QMenu* modes = view->addMenu(tr("&Mode"));
    auto* modeGroup = new QActionGroup(this);
    static const char* const modeNames[] = {"Groom", "Look", "Cards", "Export"};
    for (int i = 0; i < 4; ++i) {
        QAction* a = modes->addAction(tr(modeNames[i]), this, [this, i] { setUiMode(QLatin1String(modeNames[i])); });
        a->setCheckable(true);
        a->setChecked(i == 0);
        modeGroup->addAction(a);
    }
    QMenu* panels = view->addMenu(tr("&Panels"));
    panels->addAction(tr("&Guide Groups"), this, [this] { showUiPanel(QStringLiteral("guides")); });
    panels->addAction(tr("&Nodes"), this, [this] { showUiPanel(QStringLiteral("nodes")); });
    panels->addAction(tr("&Maps"), this, [this] { showUiPanel(QStringLiteral("maps")); });
    panels->addAction(tr("&Correctives"), this, [this] { showUiPanel(QStringLiteral("correctives")); });
    panels->addAction(tr("&Look"), this, [this] { showUiPanel(QStringLiteral("look")); });
    panels->addAction(tr("&Display"), this, [this] { showUiPanel(QStringLiteral("display")); });
    panels->addAction(tr("&Brush Settings"), this, [this] { showUiPanel(QStringLiteral("brush")); });

    // The tools: the viewport's modes (its keys B, S and Escape work when it has the focus).
    QMenu* tools = menu_->addMenu(tr("&Tools"));
    auto* toolGroup = new QActionGroup(this);
    toolGroup->setExclusive(true);
    cameraMode_ = tools->addAction(tr("&Camera"), this, [this] { viewport_->setToolMode(Viewport::ToolMode::Camera); });
    brushMode_ = tools->addAction(tr("&Brush"), this, [this] { viewport_->setToolMode(Viewport::ToolMode::Brush); });
    selectMode_ = tools->addAction(tr("&Select Guides"), this, [this] { viewport_->setToolMode(Viewport::ToolMode::Select); });
    for (QAction* a : {cameraMode_, brushMode_, selectMode_}) {
        a->setCheckable(true);
        toolGroup->addAction(a);
    }
    cameraMode_->setChecked(true);
    tools->addSeparator();
    tools->addAction(tr("&Delete Selected Guides"), this, [this] { guidesPanel_->deleteSelected(); });
    tools->addSeparator();
    // The utilities every host offers over the core: the groom onto another mesh (Maya's
    // Transfer Groom to Scalp), the turntable (the camera turning on its own).
    tools->addAction(tr("&Transfer Groom to Mesh..."), this, [this] {
        if (!document_) {
            report(tr("No groom open to transfer"));
            return;
        }
        openTransferWindow();  // the new mesh is picked there (New Scalp...)
    });
    tools->addAction(tr("&Guides from Reference (experimental)..."), this, [this] {
        if (!document_) {
            report(tr("No groom open: open one to grow guides from photos of it"));
            return;
        }
        openReferenceWindow();
    });
    QAction* turntable = tools->addAction(tr("T&urntable"), this, [this](bool on) { setTurntable(on); });
    turntable->setCheckable(true);
    turntable_ = new QTimer(this);
    turntable_->setInterval(16);
    connect(turntable_, &QTimer::timeout, this, [this] { adjustCamera(1.0f, 0.0f, 0.0f); });

    // Help: the docs (the web docs, else the docs folder beside the executable or in the source
    // tree), the shortcuts and About as floating pages over the viewport (no dialogs).
    QMenu* help = menu_->addMenu(tr("&Help"));
    help->addAction(tr("&Docs"), this, [this] { openDocs(); });
    help->addAction(tr("&Keyboard Shortcuts"), this, [this] { showHelp(QStringLiteral("shortcuts")); });
    help->addAction(tr("Check for &Updates..."), this, [this] { checkForUpdates(); });
    help->addAction(tr("&Support GroomLab"), this, [this] { openDonate(); })->setObjectName(QStringLiteral("supportGroomLab"));
    help->addAction(tr("&About GroomLab"), this, [this] { showHelp(QStringLiteral("about")); });
    host_->setMenu(menu_);
    refreshUndoActions();
}

void MainWindow::openDocs() {
    // The web docs (the app's page); without a browser the docs folder shipped beside the
    // executable, else the source tree's.
    if (QDesktopServices::openUrl(QUrl(QStringLiteral("https://getgroomlab.com/docs/app")))) {
        report(tr("Opened the docs at getgroomlab.com/docs/app"), false);
        return;
    }
    const QString sourceDocs = QDir(QString::fromUtf8(GROOMLAB_SHIPPED_PRESETS)).absoluteFilePath(QStringLiteral("../../../docs"));
    for (const QString& dir : {QCoreApplication::applicationDirPath() + QStringLiteral("/docs"), QDir::cleanPath(sourceDocs)}) {
        if (QFileInfo(dir + QStringLiteral("/USER_GUIDE.md")).isFile()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
            report(tr("Opened the docs folder %1").arg(dir), false);
            return;
        }
    }
    report(tr("No browser opened the docs: see https://getgroomlab.com/docs/app"));
}

void MainWindow::openDonate() {
    // GroomLab is free for any use, funded by donations.
    if (QDesktopServices::openUrl(QUrl(QString::fromLatin1(kDonateUrl)))) {
        report(tr("Opened %1: thank you for supporting GroomLab").arg(QString::fromLatin1(kDonateUrl)), false);
        return;
    }
    report(tr("No browser opened: support GroomLab at %1").arg(QString::fromLatin1(kDonateUrl)));
}

void MainWindow::checkForUpdates() {
    const QString current = QCoreApplication::applicationVersion().isEmpty() ? QStringLiteral("0") : QCoreApplication::applicationVersion();
    const auto show = [this](const QString& text) {
        help_->setText(text);
        host_->setPanelTitle(OverlayHost::Panel::Help, tr("Updates"), theme::blue());
        host_->showPanel(OverlayHost::Panel::Help, true);
    };
    show(tr("<p>Checking for updates...</p>"));
    if (!updates_) updates_ = new UpdateChecker(this);
    updates_->check(current, [this, show](const UpdateInfo& info) {
        QString text = QStringLiteral("<p>%1.</p>").arg(info.message.toHtmlEscaped());
        if (info.newer) {
            if (!info.notes.isEmpty()) {
                text += QStringLiteral("<p><b>%1</b><br>%2</p>")
                            .arg(tr("What's new"), info.notes.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
            }
            text += tr("<p><a style='color:#7FB2FF' href='%1'>Download GroomLab %2</a>, close GroomLab and unzip it in "
                       "place of this folder (your settings are kept).</p>")
                        .arg(info.url.toHtmlEscaped(), info.latest.toHtmlEscaped());
        }
        show(text);
        report(info.message, !info.ok);
    });
}

bool MainWindow::showHelp(const QString& page) {
    QString title, text;
    if (page == QStringLiteral("shortcuts")) {
        title = tr("Keyboard shortcuts");
        // The docs site reads this table (web/components/app-shortcuts.tsx): keep each row on one line.
        text = tr("<table cellspacing='0' cellpadding='3'>"
                  "<tr><td><b>Ctrl+O</b></td><td>Open a groom</td></tr>"
                  "<tr><td><b>Ctrl+N</b></td><td>New groom on a mesh</td></tr>"
                  "<tr><td><b>Ctrl+S / Ctrl+Shift+S</b></td><td>Save / Save as</td></tr>"
                  "<tr><td><b>Ctrl+R</b></td><td>Reload from the file</td></tr>"
                  "<tr><td><b>Ctrl+E</b></td><td>Export</td></tr>"
                  "<tr><td><b>Ctrl+Z / Ctrl+Y</b></td><td>Undo / Redo</td></tr>"
                  "<tr><td><b>Tab</b></td><td>The node graph (over it: add a node)</td></tr>"
                  "<tr><td><b>Esc</b></td><td>Close the graph; leave the brush</td></tr>"
                  "<tr><td><b>F</b></td><td>Frame all (over the graph: fit it)</td></tr>"
                  "<tr><td><b>B</b></td><td>The brush (B+drag: the radius)</td></tr>"
                  "<tr><td><b>S</b></td><td>Select guides</td></tr>"
                  "<tr><td><b>Shift+middle-drag</b></td><td>The brush strength</td></tr>"
                  "<tr><td><b>Ctrl / Shift</b> (stroke)</td><td>The opposite effect / smooth</td></tr>"
                  "<tr><td><b>Page Up / Page Down</b></td><td>The brush level</td></tr>"
                  "<tr><td><b>Alt+drag</b></td><td>Orbit (with any tool)</td></tr>"
                  "<tr><td><b>Delete</b></td><td>The selected guides, node or wire</td></tr>"
                  "<tr><td><b>Ctrl+D / Ctrl+L</b> (graph)</td><td>Duplicate the node / tidy the layout</td></tr>"
                  "</table>");
    } else if (page == QStringLiteral("about")) {
        title = tr("About GroomLab");
        text = tr("<p><b>GroomLab</b> %1</p><p>Hair, fur and feather grooming: the Maya plugin and this standalone app share "
                  "one core, and a .groom file travels between them.</p><p>Free for any use, commercial included. "
                  "<a style='color:#7FB2FF' href='%2'>Support GroomLab</a> with a donation.</p><p>Open source (Apache-2.0): "
                  "<a style='color:#7FB2FF' href='%3'>github.com/thekesslereffect/groomlab</a></p><p>Fonts: Figtree and "
                  "JetBrains Mono (SIL OFL). Qt 6 (LGPL). See THIRD_PARTY_NOTICES.txt beside the executable.</p>")
                   .arg(QCoreApplication::applicationVersion().isEmpty() ? tr("(development build)") : QCoreApplication::applicationVersion(),
                        QString::fromLatin1(kDonateUrl), QString::fromLatin1(kRepoUrl));
    } else {
        report(tr("No help page '%1' (shortcuts, about)").arg(page));
        return false;
    }
    help_->setText(text);
    host_->setPanelTitle(OverlayHost::Panel::Help, title, theme::blue());
    host_->showPanel(OverlayHost::Panel::Help, true);
    return true;
}

void MainWindow::refreshNodes() {
    const QString current = nodes_->currentItem() ? nodes_->currentItem()->data(Qt::UserRole).toString() : QString();
    nodes_->blockSignals(true);
    nodes_->clear();
    int select = -1;
    if (document_) {
        for (const GroomDocument::NodeInfo& node : document_->nodes()) {
            auto* item = new QListWidgetItem(QString::fromStdString(node.name + "  (" + node.type + ")" + (node.bypass ? "  bypassed" : "")));
            item->setData(Qt::UserRole, QString::fromStdString(node.name));
            nodes_->addItem(item);
            if (QString::fromStdString(node.name) == current) select = nodes_->count() - 1;
        }
    }
    nodes_->blockSignals(false);
    if (select >= 0) nodes_->setCurrentRow(select);
}

void MainWindow::refreshUndoActions() {
    const bool has = document_ != nullptr;
    const bool canUndo = has && document_->canUndo(), canRedo = has && document_->canRedo();
    undo_->setEnabled(canUndo);
    redo_->setEnabled(canRedo);
    undo_->setText(canUndo ? QString::fromStdString(document_->undoText()) : tr("Undo"));
    redo_->setText(canRedo ? QString::fromStdString(document_->redoText()) : tr("Redo"));
    save_->setEnabled(has);
    saveAs_->setEnabled(has);
    export_->setEnabled(has);
    host_->setUndoRedo(canUndo, canRedo, undo_->text() + QStringLiteral(" (Ctrl+Z)"), redo_->text() + QStringLiteral(" (Ctrl+Y)"));
    refreshTitle();
}

void MainWindow::refreshTitle() {
    QString title = QStringLiteral("GroomLab");
    QString name;
    bool dirty = false;
    if (document_) {
        name = document_->path().empty() ? tr("untitled.groom") : QFileInfo(QString::fromStdString(document_->path())).fileName();
        dirty = document_->dirty();
        title += QStringLiteral(" - ") + name;
        if (dirty) title += QStringLiteral(" *");
    }
    setWindowTitle(title);
    host_->setFileName(name, dirty);
    refreshExportState();
}

void MainWindow::frameAll() {
    if (!document_ && props_.empty()) return;
    viewport_->frameAll(sceneBounds());
    viewport_->syncView();
}

void MainWindow::undo() {
    if (!document_ || !document_->canUndo()) return;
    const std::string error = document_->undo();
    if (!error.empty()) report(QString::fromStdString(error));
    graphEdited();
    refreshGroups();
}

void MainWindow::redo() {
    if (!document_ || !document_->canRedo()) return;
    const std::string error = document_->redo();
    if (!error.empty()) report(QString::fromStdString(error));
    graphEdited();
    refreshGroups();
}

void MainWindow::setPreviewDensity(double density) { density_->setValue(density); }

void MainWindow::setPointBudget(double thousands) { budget_->setValue(thousands); }

}  // namespace groomlab::app
