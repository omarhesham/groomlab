// SPDX-License-Identifier: Apache-2.0
#pragma once

// MainWindow's own helpers, shared by its source files (main_window*.cpp).

#include <algorithm>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QString>
#include <QStringList>

#include "app_shell.h"
#include "export_panel.h"
#include "main_window.h"

namespace groomlab::app {

// The preview's point budget by default (thousands of points; 0: no limit, every strand the
// density shows). The Display panel sets one: past it every Scatter thins to fit.
constexpr double kDefaultPointBudget = 0.0;


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

inline void rememberRecentFile(const QString& given) {
    // One spelling per file (absolute, forward slashes): the same file reached another way is not
    // listed twice.
    const QString path = QDir::cleanPath(QFileInfo(given).absoluteFilePath());
    QStringList recent = recentFiles();
    recent.erase(std::remove_if(recent.begin(), recent.end(),
                                [&](const QString& p) {
                                    return QDir::cleanPath(QFileInfo(p).absoluteFilePath()).compare(path, Qt::CaseInsensitive) == 0;
                                }),
                 recent.end());
    recent.prepend(path);
    while (recent.size() > 8) recent.removeLast();
    QSettings().setValue(QStringLiteral("recentFiles"), recent);
}

}  // namespace groomlab::app
