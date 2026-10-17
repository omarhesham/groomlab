// SPDX-License-Identifier: Apache-2.0
#pragma once

// MainWindow's own helpers, shared by its source files (main_window*.cpp).

#include <QApplication>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "export_panel.h"
#include "main_window.h"

namespace groomlab::app {

// The preview's point budget by default (thousands of points): every Scatter thins to fit, so a
// preset that grows a groom to hundreds of thousands of strands keeps the viewport and the brush
// responsive (Maya previews at 10% of the density); the Display panel raises it, 0 lifts it.
constexpr double kDefaultPointBudget = 300.0;


// The wait cursor over the window for as long as it lives (a long step on this thread: opening,
// baking, transferring, an export), restored on every way out.
struct BusyCursor {
    BusyCursor() { QApplication::setOverrideCursor(Qt::WaitCursor); }
    ~BusyCursor() { QApplication::restoreOverrideCursor(); }
    BusyCursor(const BusyCursor&) = delete;
    BusyCursor& operator=(const BusyCursor&) = delete;
};

// The recent files (the empty-state card's), newest first, in the user's settings.
inline QStringList recentFiles() { return QSettings().value(QStringLiteral("recentFiles")).toStringList(); }

inline void rememberRecentFile(const QString& path) {
    QStringList recent = recentFiles();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 8) recent.removeLast();
    QSettings().setValue(QStringLiteral("recentFiles"), recent);
}

}  // namespace groomlab::app
