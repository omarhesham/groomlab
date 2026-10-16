# SPDX-License-Identifier: Apache-2.0
"""Publish the standalone GroomLab app (the Windows beta) to the downloads bucket (Cloudflare R2).

    python tools/publish_app.py [--zip dist/GroomLab-App-<version>-beta-windows-x64.zip]
                                [--notes "..." | --notes-file notes.txt] [--dry-run]

Without --zip it runs tools/package_app.py first (which smoke-runs the zip from a clean folder).
It uploads four objects:

    app/GroomLab-App-<version>-beta-windows-x64.zip   the release, kept forever
    app/GroomLab-App-windows-latest.zip               what the website's app download links to
    app/latest.json                                   {version, file, url, sha256, size, date, notes,
                                                       channel, platform}: Help > Check for Updates
                                                       in the app reads it (update_check.h)
    app/releases.json                                 every release, newest first (the website's
                                                       Download page lists them; publishing a
                                                       version again replaces its entry)

The bucket, its credentials (R2_ACCOUNT_ID, R2_ACCESS_KEY_ID, R2_SECRET_ACCESS_KEY, optionally
R2_BUCKET) and the signed upload: tools/downloads_bucket.py, as for the Maya release.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from downloads_bucket import IMMUTABLE, PUBLIC, SHORT, json_body, read_notes, release_index, release_info, upload  # noqa: E402

ZIP_NAME = re.compile(r"GroomLab-App-(?P<version>[0-9][0-9A-Za-z.]*)-(?P<channel>beta)-(?P<platform>windows-x64)\.zip")


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--zip", help="a zip made by tools/package_app.py (default: package one now)")
    parser.add_argument("--notes", default="", help="what's new, a few lines (Check for Updates in the app shows them)")
    parser.add_argument("--notes-file", help="the notes from a UTF-8 text file instead of --notes")
    parser.add_argument("--dry-run", action="store_true", help="show what would be uploaded, upload nothing")
    args = parser.parse_args()
    notes = read_notes(args.notes, args.notes_file)

    zip_path = args.zip
    if not zip_path:
        subprocess.check_call([sys.executable, os.path.join(ROOT, "tools", "package_app.py")])
        zips = glob.glob(os.path.join(ROOT, "dist", "GroomLab-App-*-windows-x64.zip"))
        if not zips:
            sys.exit("package_app.py made no zip in dist/")
        zip_path = max(zips, key=os.path.getmtime)
    m = ZIP_NAME.fullmatch(os.path.basename(zip_path))
    if not m:
        sys.exit("%s is not a GroomLab app zip (GroomLab-App-<version>-beta-windows-x64.zip)" % os.path.basename(zip_path))
    version, channel, platform = m.group("version", "channel", "platform")
    with open(zip_path, "rb") as f:
        body = f.read()
    with zipfile.ZipFile(zip_path) as z:
        if "GroomLab-App-%s-%s/GroomLab.exe" % (version, channel) not in z.namelist():
            sys.exit("%s holds no GroomLab-App-%s-%s/GroomLab.exe" % (zip_path, version, channel))

    versioned = "app/GroomLab-App-%s-%s-%s.zip" % (version, channel, platform)
    latest = "app/GroomLab-App-windows-latest.zip"
    info = release_info(version, versioned, body, notes, channel=channel, platform=platform)
    uploads = [
        (versioned, body, "application/zip", IMMUTABLE),
        (latest, body, "application/zip", SHORT),
        ("app/latest.json", json_body(info), "application/json", SHORT),
        ("app/releases.json", json_body(release_index("app/releases.json", info)), "application/json", SHORT),
    ]
    if not upload(uploads, args.dry_run):
        print(json.dumps(info, indent=2))
        return
    print("GroomLab app %s (%s) is live: %s/%s" % (version, channel, PUBLIC, latest))


if __name__ == "__main__":
    main()
