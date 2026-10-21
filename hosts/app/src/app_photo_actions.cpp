// SPDX-License-Identifier: Apache-2.0
// What the Groom Tools' Photos tab runs (engine/ui/lists.json's photos: the bar, the right-click
// menus, the point pairs' bar and menu, as Maya's tool_tabs.py ReferenceTab.ACTIONS), and the
// review HUD: the photos panel's edits (photos_panel.h), a rename's name asked over the viewport's
// prompt (never a dialog), a photo picked in the file dialog (an interactive window only, as Open).

#include <QFileDialog>
#include <QFileInfo>

#include "app_actions.h"
#include "main_window.h"
#include "overlay.h"
#include "photos_panel.h"

namespace groomlab::app {

namespace {

using ui::json;

const QString kPhotoFilter = QObject::tr("Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp);;All files (*.*)");

}  // namespace

void AppActions::addPhotoBindings(ui::Bindings& b, MainWindow& w) {
    MainWindow* m = &w;
    auto run = [](std::function<void()> fn) { return [fn = std::move(fn)](const json&, bool) { fn(); }; };
    auto groomOpen = [m] { return m->document_ != nullptr; };
    auto photo = [m] { return m->document_ && m->photos_->current() >= 0; };
    auto pair = [m] { return m->document_ && m->photos_->pickedPair() >= 0; };

    // + Import Photo...: a photo file as a reference camera, aimed at the groom.
    b["photo.import"] = {run([m] {
                             if (!m->document_) return;
                             if (!m->interactive_) {
                                 m->report(QObject::tr("Import Photo...: the photo is picked in a file dialog (an interactive window); "
                                                       "scripts call PhotosPanel::importPhoto"));
                                 return;
                             }
                             const QString path = QFileDialog::getOpenFileName(m, QObject::tr("Import Photo"), QString(), kPhotoFilter);
                             if (path.isEmpty()) return;
                             const int index = m->photos_->importPhoto(path);
                             if (index >= 0) {
                                 m->notify(QObject::tr("%1 imported: Look Through it, line it up (by eye, or Align by Points), then Lock it.")
                                               .arg(m->photos_->name(index)));
                             }
                         }),
                         {}, groomOpen};
    b["photo.lookThrough"] = {run([m] { m->lookThroughPhoto(m->photos_->current(), true); }), {}, photo};
    b["photo.rename"] = {run([m] {
                             const int index = m->photos_->current();
                             if (index < 0) return;
                             m->host_->showTextPrompt(QObject::tr("Rename Photo"), QObject::tr("The reference photo's new name:"),
                                                      m->photos_->name(index),
                                                      {{QObject::tr("Rename"), [m, index](const QString& name) { m->photos_->rename(index, name); }}});
                         }),
                         {}, photo};
    // Remove: the reference goes (its photo file stays on disk).
    b["photo.remove"] = {run([m] { m->photos_->remove(m->photos_->current()); }), {}, photo};
    // Align by Points: the picked photo's pairs.
    b["pairs.solve"] = {run([m] {
                            if (m->photos_->solve()) m->notify(m->photos_->solveText());
                        }),
                        {}, photo};
    b["pairs.clear"] = {run([m] { m->photos_->clearPairs(); }), {}, photo};
    b["pairs.delete"] = {run([m] { m->photos_->deletePair(m->photos_->pickedPair()); }), {}, pair};
    b["pairs.repickPhoto"] = {run([m] { m->photos_->repickPoint(m->photos_->pickedPair(), true); }), {}, pair};
    b["pairs.repickMesh"] = {run([m] { m->photos_->repickPoint(m->photos_->pickedPair(), false); }), {}, pair};

    // GroomLab > Review > Strand Count HUD: the stats line over the viewport.
    b["review.hud"] = {[m](const json&, bool on) { m->host_->setStatsWanted(on); }, [m](const json&) { return m->host_->statsWanted(); }};
}

}  // namespace groomlab::app
