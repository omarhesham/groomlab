// SPDX-License-Identifier: Apache-2.0
// MainWindow: the UI, graph presets and params driven by name (scripted runs and the tests).

#include "main_window_internal.h"

#include <algorithm>
#include <map>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QTimer>

#include "brush_panel.h"
#include "cards_panel.h"
#include "export_panel.h"
#include "graph_presets.h"
#include "maps_panel.h"
#include "node_graph.h"
#include "overlay.h"
#include "param_panel.h"
#include "theme.h"
#include "viewport.h"

namespace groomlab::app {

// ---- The UI ----------------------------------------------------------------------------------

bool MainWindow::setUiMode(const QString& name) {
    const QString n = name.toLower();
    OverlayHost::Mode mode;
    if (n == QStringLiteral("groom")) mode = OverlayHost::Mode::Groom;
    else if (n == QStringLiteral("look")) mode = OverlayHost::Mode::Look;
    else if (n == QStringLiteral("cards")) mode = OverlayHost::Mode::Cards;
    else if (n == QStringLiteral("export")) mode = OverlayHost::Mode::Export;
    else {
        report(tr("No UI mode '%1' (groom, look, cards, export)").arg(name));
        return false;
    }
    if (mode != host_->mode()) {
        host_->setMode(mode);
        if (mode != OverlayHost::Mode::Groom) viewport_->setToolMode(Viewport::ToolMode::Camera);
        refreshStats();
    }
    return true;
}

void MainWindow::setGraphShown(bool shown) {
    if (shown && !document_) {
        report(tr("No groom open: nothing to show in the graph"));
        return;
    }
    graph_->setDocument(shown ? document_.get() : nullptr);
    if (shown) {  // the badges from the last evaluation
        graph_->setNodeTimes(document_->nodeTimes());
        graph_->setNodeNotes(document_->nodeNotes());
    }
    graphAction_->setChecked(shown);
    host_->setGraphShown(shown, shown ? graph_->nodeCount() : (document_ ? static_cast<int>(document_->nodes().size()) : 0));
}

bool MainWindow::graphShown() const { return graph_->isVisible(); }

bool MainWindow::showUiPanel(const QString& name) {
    const QString n = name.toLower();
    OverlayHost::Panel panel;
    if (n == QStringLiteral("guides")) panel = OverlayHost::Panel::Guides;
    else if (n == QStringLiteral("nodes")) panel = OverlayHost::Panel::Nodes;
    else if (n == QStringLiteral("maps")) panel = OverlayHost::Panel::Maps;
    else if (n == QStringLiteral("correctives")) panel = OverlayHost::Panel::Correctives;
    else if (n == QStringLiteral("look")) panel = OverlayHost::Panel::Look;
    else if (n == QStringLiteral("display")) panel = OverlayHost::Panel::Display;
    else if (n == QStringLiteral("brush")) panel = OverlayHost::Panel::Brush;
    else {
        report(tr("No panel '%1' (guides, nodes, maps, correctives, look, display, brush)").arg(name));
        return false;
    }
    if (panel == OverlayHost::Panel::Brush || panel == OverlayHost::Panel::Guides || panel == OverlayHost::Panel::Display ||
        panel == OverlayHost::Panel::Nodes || panel == OverlayHost::Panel::Maps || panel == OverlayHost::Panel::Correctives) {
        setUiMode(QStringLiteral("groom"));
    }
    host_->showPanel(panel, true);
    return true;
}

bool MainWindow::setUiLookTab(const QString& name) {
    const QStringList words = name.toLower().split(QLatin1Char(','), Qt::SkipEmptyParts);
    const QString n = words.isEmpty() ? QString() : words.first().trimmed();
    LookCard::Tab tab;
    if (n == QStringLiteral("material")) tab = LookCard::Tab::Material;
    else if (n == QStringLiteral("lighting")) tab = LookCard::Tab::Lighting;
    else if (n == QStringLiteral("render")) tab = LookCard::Tab::Render;
    else {
        report(tr("No look tab '%1' (material, lighting, render; ',more' opens the full settings)").arg(name));
        return false;
    }
    setUiMode(QStringLiteral("look"));
    host_->lookCard()->setTab(tab);
    host_->lookCard()->setMoreShown(words.contains(QStringLiteral("more")));
    return true;
}

bool MainWindow::setUiHover(const QString& name) {
    const QString n = name.toLower();
    if (n != QStringLiteral("rail") && n != QStringLiteral("panels") && n != QStringLiteral("all")) {
        report(tr("No hover state '%1' (rail, panels, all)").arg(name));
        return false;
    }
    host_->setRailsExpanded(n != QStringLiteral("panels"), n != QStringLiteral("rail"));
    return true;
}

bool MainWindow::setUiMenu(const QString& name) {
    if (name.toLower() != QStringLiteral("presets")) {
        report(tr("No menu '%1' (presets)").arg(name));
        return false;
    }
    // The menu popped up for real above the pill, the Add-ons submenu and its first preset's
    // opened as the keys open them (Right: the active item's submenu, its first item active;
    // Down: Insert after selected node), each grabbed at once: a popup closes when the window
    // is not the active one, so grabUi draws the grabs where the menus were.
    uiMenus_.clear();
    showPresetsMenu();
    QApplication::processEvents();
    auto key = [](QWidget* w, int k) {
        QKeyEvent press(QEvent::KeyPress, k, Qt::NoModifier);
        QApplication::sendEvent(w, &press);
        QApplication::processEvents();
    };
    auto keep = [this](QMenu* menu) {
        if (menu->isVisible()) uiMenus_.emplace_back(menu->grab(), host_->mapFromGlobal(menu->pos()));
    };
    QAction* addons = nullptr;
    for (QAction* a : presetsMenu_->actions()) {
        if (a->menu() && a->text() == QStringLiteral("Add-ons")) addons = a;
    }
    if (addons && !addons->menu()->actions().empty()) {
        presetsMenu_->setActiveAction(addons);
        key(presetsMenu_, Qt::Key_Right);
        QMenu* sub = addons->menu();
        if (QMenu* first = sub->actions().front()->menu()) {
            sub->setActiveAction(sub->actions().front());
            key(sub, Qt::Key_Right);
            key(first, Qt::Key_Down);
            keep(presetsMenu_);
            keep(sub);
            keep(first);
        }
    }
    if (uiMenus_.empty()) keep(presetsMenu_);
    return !uiMenus_.empty();
}

// ---- Graph presets --------------------------------------------------------------------------------

void MainWindow::fillPresetsMenu() {
    // A submenu per category (presetCategories first, in order), then the presets with no
    // category (the user's own), each titled, its description as the tooltip.
    presetsMenu_->clear();
    std::map<std::string, std::vector<PresetInfo>> byCategory;
    for (const PresetInfo& info : availablePresets()) byCategory[info.category].push_back(info);
    std::vector<std::string> order;
    for (const std::string& c : presetCategories()) {
        if (byCategory.count(c)) order.push_back(c);
    }
    for (const auto& [category, infos] : byCategory) {
        if (!category.empty() && std::find(order.begin(), order.end(), category) == order.end()) order.push_back(category);
    }
    // Each preset is a submenu: Apply (the graph replaced) beside Insert after selected node (the
    // preset's look added as one Compound), its description the tooltip of both.
    auto addPreset = [this](QMenu* menu, const PresetInfo& info) {
        QMenu* sub = menu->addMenu(QString::fromStdString(info.title));
        sub->setToolTipsVisible(true);
        const QString about = QString::fromStdString(info.description.empty() ? info.name : info.description);
        sub->menuAction()->setToolTip(about);
        sub->menuAction()->setData(QString::fromStdString(info.name));
        QAction* apply = sub->addAction(tr("Apply"), this, [this, path = info.path] { applyPreset(QString::fromStdString(path)); });
        apply->setToolTip(tr("Replace this groom's graph with the preset, sizes fitted to this scalp, the maps it reads made: one undo step"));
        QAction* insert = sub->addAction(tr("Insert after selected node"), this, [this, path = info.path] { insertPreset(QString::fromStdString(path)); });
        insert->setToolTip(tr("Keep this graph and add the preset's look to it as one Compound after the selected node (before the "
                              "Output when none is selected), fitted to this scalp: one undo step"));
        apply->setEnabled(document_ != nullptr);
        insert->setEnabled(document_ != nullptr);
    };
    for (const std::string& category : order) {
        QMenu* sub = presetsMenu_->addMenu(QString::fromStdString(category));
        sub->setToolTipsVisible(true);
        for (const PresetInfo& info : byCategory[category]) addPreset(sub, info);
    }
    if (byCategory.count(std::string())) {
        if (!order.empty()) presetsMenu_->addSeparator();
        for (const PresetInfo& info : byCategory[std::string()]) addPreset(presetsMenu_, info);
    }
    presetsMenu_->addSeparator();
    QAction* save = presetsMenu_->addAction(tr("Save as Preset..."), this, [this] {
        if (!graphShown()) setGraphShown(true);
        graph_->promptName(tr("Save this graph as a preset"), tr("Preset name"), [this](const QString& name) { savePreset(name); });
    });
    save->setToolTip(tr("This groom's graph, its layout and its scalp's size as a preset in your preset folder (%1)")
                         .arg(QString::fromStdString(presetUserDir())));
    save->setEnabled(document_ != nullptr);
    presetsMenu_->addSeparator();
    QAction* copy = presetsMenu_->addAction(tr("Copy Graph"), this, [this] { copyGraph(); });
    copy->setToolTip(tr("Put this groom's whole graph on the clipboard as text, to share in a chat or forum or save as a .json preset"));
    copy->setEnabled(document_ != nullptr);
    QAction* paste = presetsMenu_->addAction(tr("Paste Graph"), this, [this] { pasteGraph(); });
    paste->setToolTip(tr("Replace this groom's graph with one copied as text (Copy Graph, or a preset's JSON), sizes fitted to this scalp: one undo step"));
    paste->setEnabled(document_ != nullptr);
}

void MainWindow::showPresetsMenu() {
    fillPresetsMenu();
    const QRect pill = host_->presetsPillRect();
    const QSize size = presetsMenu_->sizeHint();
    presetsMenu_->popup(host_->mapToGlobal(QPoint(pill.left(), pill.top() - 6 - size.height())));
}

bool MainWindow::applyPreset(const QString& nameOrPath) {
    if (!document_) {
        report(tr("No groom open to apply the preset %1 to").arg(nameOrPath));
        return false;
    }
    std::string path = nameOrPath.toStdString();
    if (!QFileInfo(nameOrPath).isFile()) {
        path = presetPath(nameOrPath.toStdString(), presetDirectories());
        if (path.empty()) {
            report(tr("No preset named %1").arg(nameOrPath));
            return false;
        }
    }
    PresetApplied applied;
    const std::string error = applyPresetFile(*document_, path, &applied);
    if (!error.empty()) {
        report(tr("Cannot apply the preset %1: %2").arg(nameOrPath, QString::fromStdString(error)));
        return false;
    }
    graphEdited();
    if (graphShown()) {
        graph_->refresh();
        graph_->fitAll();
    }
    report(tr("Applied the preset %1").arg(QString::fromStdString(presetMessage(applied))), false);
    return true;
}

bool MainWindow::insertPreset(const QString& nameOrPath) {
    if (!document_) {
        report(tr("No groom open to insert the preset %1 into").arg(nameOrPath));
        return false;
    }
    std::string path = nameOrPath.toStdString();
    if (!QFileInfo(nameOrPath).isFile()) {
        path = presetPath(nameOrPath.toStdString(), presetDirectories());
        if (path.empty()) {
            report(tr("No preset named %1").arg(nameOrPath));
            return false;
        }
    }
    const std::string after = graphShown() ? graph_->selectedNode() : std::string();
    PresetApplied applied;
    const std::string error = insertPresetFile(*document_, path, after, &applied);
    if (!error.empty()) {
        report(tr("Cannot insert the preset %1: %2").arg(nameOrPath, QString::fromStdString(error)));
        return false;
    }
    graphEdited();
    if (graphShown()) {
        graph_->refresh();
        graph_->selectNode(applied.nodeMade);
        graph_->fitAll();
    }
    report(tr("Inserted %1 as %2 after %3 (Ctrl+Z undoes it)")
               .arg(QString::fromStdString(presetMessage(applied)), QString::fromStdString(applied.nodeMade),
                    after.empty() ? tr("the last node") : QString::fromStdString(after)),
           false);
    return true;
}

bool MainWindow::paintParam(const QString& node, const QString& param, const QString& kind) {
    if (!document_) {
        report(tr("No groom open: nothing to paint"));
        return false;
    }
    std::string map;
    bool made = false;
    const std::string error = document_->paintParam(node.toStdString(), param.toStdString(), &map, kind.toStdString(), &made);
    if (!error.empty()) {
        report(tr("Cannot paint %1.%2: %3").arg(node, param, QString::fromStdString(error)));
        return false;
    }
    if (made) graphEdited();
    // The Paint brush on the map (picked), toward 1: strokes paint the param back to its value,
    // Ctrl toward 0 (on a density map, no hair), as Maya's starts it.
    maps_->refresh();
    maps_->selectMap(map);
    maps_->setPaintValue(1.0);
    if (!maps_->paintSelected()) return false;  // brush mode (paintRequested)
    report(tr("%1 %2, which %3.%4 reads: Ctrl paints it toward 0%5, strokes back toward the value")
               .arg(made ? tr("Made") : tr("Painting"), QString::fromStdString(map), node, param,
                    param == QStringLiteral("density") ? tr(" (no hair there)") : QString()),
           false);
    return true;
}

bool MainWindow::savePreset(const QString& name, bool overwrite) {
    if (!document_) {
        report(tr("No groom open to save as a preset"));
        return false;
    }
    std::string path;
    const std::string error = savePresetFile(*document_, name.toStdString(), overwrite, &path);
    if (!error.empty()) {
        report(tr("Cannot save the preset %1: %2").arg(name, QString::fromStdString(error)));
        return false;
    }
    report(tr("Saved the preset %1 to %2").arg(name, QString::fromStdString(path)), false);
    return true;
}

void MainWindow::copyGraph() {
    if (!document_) {
        report(tr("No groom open to copy the graph of"));
        return;
    }
    QApplication::clipboard()->setText(QString::fromStdString(graphAsText(*document_)));
    report(tr("Copied the graph as text (%n nodes): paste it with Presets > Paste Graph on any groom, or save it as a .json preset", "",
              static_cast<int>(document_->nodes().size())),
           false);
}

void MainWindow::pasteGraph() {
    if (!document_) {
        report(tr("No groom open to paste a graph onto"));
        return;
    }
    PresetApplied applied;
    const std::string error = pasteGraphText(*document_, QApplication::clipboard()->text().toStdString(), &applied);
    if (!error.empty()) {
        report(QString::fromStdString(error));
        return;
    }
    graphEdited();
    if (graphShown()) {
        graph_->refresh();
        graph_->fitAll();
    }
    report(tr("Pasted %1 (Ctrl+Z undoes it)").arg(QString::fromStdString(presetMessage(applied))), false);
}

bool MainWindow::selectUiNode(const QString& name) {
    if (!document_) {
        report(tr("No groom open: no node '%1' to select").arg(name));
        return false;
    }
    bool found = false;
    for (const GroomDocument::NodeInfo& n : document_->nodes()) found = found || QString::fromStdString(n.name) == name;
    if (!found) {
        report(tr("No node '%1' in the graph").arg(name));
        return false;
    }
    if (!graphShown()) setGraphShown(true);
    graph_->selectNode(name.toStdString());
    return true;
}

bool MainWindow::setDisplayText(const QString& text) {
    const int equals = text.indexOf(QLatin1Char('='));
    if (equals <= 0) {
        report(tr("Expected name=value for the display, got %1").arg(text));
        return false;
    }
    const QString name = text.left(equals).trimmed().toLower(), value = text.mid(equals + 1).trimmed();
    bool ok = true;
    auto flag = [&] {
        const QString v = value.toLower();
        ok = v == QStringLiteral("1") || v == QStringLiteral("0") || v == QStringLiteral("true") || v == QStringLiteral("false") ||
             v == QStringLiteral("on") || v == QStringLiteral("off");
        return v == QStringLiteral("1") || v == QStringLiteral("true") || v == QStringLiteral("on");
    };
    if (name == QStringLiteral("colormode")) {
        static const char* const modes[] = {"solid", "colorset", "attribute", "random", "clump", "density"};
        ok = false;
        for (int i = 0; i < 6; ++i) {
            if (value.toLower() == QLatin1String(modes[i])) {
                colorMode_->setCurrentIndex(i);
                ok = true;
            }
        }
    } else if (name == QStringLiteral("colorsource")) {
        colorSource_->setEditText(value);
    } else if (name == QStringLiteral("colormin")) {
        colorMin_->setValue(value.toDouble(&ok));
    } else if (name == QStringLiteral("colormax")) {
        colorMax_->setValue(value.toDouble(&ok));
    } else if (name == QStringLiteral("guides")) {
        guides_->setChecked(flag());
    } else if (name == QStringLiteral("widthcoverage")) {
        widthCoverage_->setChecked(flag());
    } else if (name == QStringLiteral("coveragedensity")) {
        coverageDensity_->setChecked(flag());
    } else if (name == QStringLiteral("clumplod")) {
        clumpLod_->setChecked(flag());
    } else if (name == QStringLiteral("clumploderror")) {
        settings_.clumpLodError = std::clamp(value.toFloat(&ok), 0.01f, 0.45f);
        redisplay();
    } else if (name == QStringLiteral("guidepoints")) {
        guidePoints_->setCurrentIndex(std::clamp(value.toInt(&ok), 0, 2));
    } else if (name == QStringLiteral("scalpmap")) {
        const int index = scalpMap_->findData(value);
        ok = index >= 0 || value.isEmpty();
        scalpMap_->setCurrentIndex(std::max(0, index));
    } else if (name == QStringLiteral("scalpmapchannel")) {
        ok = scalpMapChannel_->findText(value) >= 0;
        if (ok) scalpMapChannel_->setCurrentText(value);
    } else if (name == QStringLiteral("cardsview")) {
        cardsView_->setChecked(flag());
    } else if (name == QStringLiteral("lookdev")) {
        setLookdev(flag());
    } else if (name == QStringLiteral("scalp")) {
        setShowScalp(flag());
    } else {
        ok = false;
    }
    if (!ok) {
        report(tr("Cannot set the display's %1 to %2").arg(name, value));
        return false;
    }
    return true;
}

QImage MainWindow::grabUi(const QImage& scene) {
    // The viewport's frame, then every other child of the host painted over it in stacking
    // order (the glass composes with alpha as on screen).
    // The frame is in device pixels (a 150% screen: 1.5 per logical pixel); the pieces render
    // in logical ones, so the image carries the ratio and the painter scales them to match.
    const qreal ratio = host_->devicePixelRatioF();
    const QSize pixels = host_->size() * ratio;
    QImage image = scene.isNull() ? viewport_->grabFramebuffer() : scene;
    if (image.isNull()) {
        image = QImage(pixels, QImage::Format_ARGB32_Premultiplied);
        image.fill(theme::backdrop());
    }
    image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    if (image.size() != pixels) image = image.scaled(pixels, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    image.setDevicePixelRatio(ratio);
    QPainter painter(&image);
    for (QObject* child : host_->children()) {
        auto* w = qobject_cast<QWidget*>(child);
        if (!w || w == viewport_ || !w->isVisible()) continue;
        w->render(&painter, w->pos(), QRegion(), QWidget::DrawChildren);
    }
    // The menus a scripted screenshot laid out (setUiMenu), as they would pop up.
    for (const auto& [pixmap, at] : uiMenus_) painter.drawPixmap(at, pixmap);
    return image;
}

void MainWindow::captureUi(const std::function<void(const QImage&)>& done) {
    if (QGuiApplication::platformName() == QLatin1String("offscreen")) {
        QTimer::singleShot(500, this, [this, done] {
            const QImage scene = renderOffscreen(host_->size() * host_->devicePixelRatioF(), 1);
            done(grabUi(scene));
        });
        return;
    }
    auto* once = new QObject(this);
    connect(viewport_, &Viewport::frameDrawn, once, [this, once, done] {
        once->deleteLater();
        QTimer::singleShot(500, this, [this, done] { done(grabUi()); });
    });
}

bool MainWindow::setParamText(const QString& text) {
    if (!document_) {
        report(tr("No groom open for %1").arg(text));
        return false;
    }
    const int dot = text.indexOf(QLatin1Char('.')), equals = text.indexOf(QLatin1Char('='));
    if (dot <= 0 || equals <= dot + 1) {
        report(tr("Expected node.param=value, got %1").arg(text));
        return false;
    }
    const std::string node = text.left(dot).toStdString(), name = text.mid(dot + 1, equals - dot - 1).toStdString();
    const QString valueText = text.mid(equals + 1);
    nlohmann::json value;
    try {
        value = nlohmann::json::parse(valueText.toStdString());
    } catch (const nlohmann::json::exception&) {
        value = valueText.toStdString();  // a bare word: a string
    }
    const std::string error = document_->setParam(node, name, value);
    if (!error.empty()) {
        report(tr("Cannot set %1: %2").arg(text, QString::fromStdString(error)));
        return false;
    }
    graphEdited();
    return true;
}

bool MainWindow::setBypassText(const QString& node) {
    if (!document_) {
        report(tr("No groom open to bypass %1").arg(node));
        return false;
    }
    const std::string error = document_->setBypass(node.toStdString(), true);
    if (!error.empty()) {
        report(tr("Cannot bypass %1: %2").arg(node, QString::fromStdString(error)));
        return false;
    }
    graphEdited();
    return true;
}

bool MainWindow::loadGraphFile(const QString& path) {
    if (!document_) {
        report(tr("No groom open for the graph %1").arg(path));
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        report(tr("Cannot read the graph %1").arg(path));
        return false;
    }
    const std::string error = document_->replaceGraph(file.readAll().toStdString());
    if (!error.empty()) {
        report(tr("Cannot use the graph %1: %2").arg(path, QString::fromStdString(error)));
        return false;
    }
    params_->setDocument(document_.get());
    graphParams_->setDocument(document_.get());
    if (graphShown()) graph_->setDocument(document_.get());
    refreshNodes();
    reevaluate();
    refreshUndoActions();
    refreshGroups();
    return true;
}

bool MainWindow::saveTo(const QString& path) {
    if (!document_) {
        report(tr("No groom open to save as %1").arg(path));
        return false;
    }
    try {
        document_->save(path.toStdString());
    } catch (const std::exception& e) {
        report(tr("Cannot save %1: %2").arg(path, QString::fromUtf8(e.what())));
        return false;
    }
    report(tr("Saved %1").arg(path), false);
    refreshTitle();
    return true;
}

QString MainWindow::cameraDescription() const {
    const OrbitCamera& c = viewport_->camera();
    const QVector3D eye = c.eye(), target = c.target();
    return QStringLiteral("camera eye %1 %2 %3 target %4 %5 %6 fov %7")
        .arg(eye.x()).arg(eye.y()).arg(eye.z()).arg(target.x()).arg(target.y()).arg(target.z()).arg(c.fovDegrees());
}

}  // namespace groomlab::app
