// SPDX-License-Identifier: Apache-2.0
#pragma once

// Help > Check for Updates: the app's side of the release feed tools/publish_app.py writes,
// https://downloads.getgroomlab.com/app/latest.json ({version, file, url, sha256, size, date, notes,
// channel, platform}). One plain GET of that file (User-Agent "GroomLab-App/<version>"): nothing about
// the machine or the user is sent. It only tells: the newer version, its notes and its download
// link (the Maya plugin's groomlab/updates.py installs too; the app's zip is replaced by hand).
// GROOMLAB_NO_UPDATES=1 turns it off.

#include <functional>

#include <QByteArray>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace groomlab::app {

struct UpdateInfo {
    bool ok = false;     // latest.json was read
    bool newer = false;  // it names a newer version than this one
    QString current, latest, url, notes;
    QString message;  // one line: what the check found (or why it found nothing)
};

// Semantic versions (core's semver.h: "0.1.2", "v1.0.0-beta.10", "+build" ignored; a pre-release
// sorts before its release): <0, 0 or >0; 0 when either is not a version.
int compareVersions(const QString& a, const QString& b);

// latest.json's text against the running version.
UpdateInfo parseLatest(const QByteArray& json, const QString& current);

class UpdateChecker : public QObject {
    Q_OBJECT
public:
    using Done = std::function<void(const UpdateInfo&)>;

    explicit UpdateChecker(QObject* parent = nullptr);

    static QString feedUrl();     // the latest.json read
    static bool updatesEnabled();  // false under GROOMLAB_NO_UPDATES
    // Reads the feed; `done` runs once, on this thread (at once when updates are off).
    void check(const QString& current, Done done);

private:
    QNetworkAccessManager* network_ = nullptr;
};

}  // namespace groomlab::app
