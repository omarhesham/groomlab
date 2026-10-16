# SPDX-License-Identifier: Apache-2.0
"""Publish a packaged GroomLab release to the downloads bucket (Cloudflare R2).

    python tools/publish_release.py [--zip dist/GroomLab-<version>-maya-win64.zip]
                                    [--notes "..." | --notes-file notes.txt] [--dry-run]

Without --zip it runs tools/package_release.py first. The zip holds a build for every Maya it was
packaged with (GroomLab/maya_versions.json). It uploads five objects:

    maya/GroomLab-<version>-Maya-Windows.zip       the release, kept forever
    maya/GroomLab-Maya-Windows-latest.zip          the latest release
    maya/GroomLab-Maya2022-Windows-latest.zip      the same, under the name the website's "Download
                                                    free" links to (web/site.config.ts DOWNLOAD_URL)
    maya/latest.json                               {version, file, url, sha256, size, date, notes,
                                                    maya, mayaVersions, mayaBeta, platform}
    maya/releases.json                             every release, newest first: the latest.json of
                                                    each (the website's Download page lists them;
                                                    publishing a version again replaces its entry)

latest.json is what GroomLab > Check for Updates reads (groomlab/updates.py): `notes` is the short
changelog it shows (a few lines; --notes or --notes-file, empty by default), `mayaVersions` the Maya
versions the zip has a build for (an install in another Maya is not offered it), `mayaBeta` those
built but not yet tested in their Maya (Check for Updates says so), `maya` the tested one (2022): the
only field releases before mayaVersions read, and they all run in Maya 2022.

The bucket, its credentials (R2_ACCOUNT_ID, R2_ACCESS_KEY_ID, R2_SECRET_ACCESS_KEY, optionally
R2_BUCKET) and the signed upload: tools/downloads_bucket.py, shared with publish_blender.py and
publish_app.py.
"""

import argparse
import glob
import json
import os
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from downloads_bucket import IMMUTABLE, PUBLIC, SHORT, json_body, read_notes, release_index, release_info, upload  # noqa: E402

SUFFIX = "-maya-win64.zip"


def maya_builds(zip_path, version):
    """GroomLab/maya_versions.json of a packaged zip: {"2022": {"status": "tested"...}, ...}."""
    with zipfile.ZipFile(zip_path) as z:
        try:
            return json.loads(z.read("GroomLab-%s/GroomLab/maya_versions.json" % version).decode("utf-8"))
        except KeyError:
            sys.exit("%s has no GroomLab/maya_versions.json: package it with tools/package_release.py" % zip_path)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--zip", help="a zip made by tools/package_release.py (default: package one now)")
    parser.add_argument("--notes", default="", help="what's new, a few lines (Check for Updates in Maya shows them)")
    parser.add_argument("--notes-file", help="the notes from a UTF-8 text file instead of --notes")
    parser.add_argument("--dry-run", action="store_true", help="show what would be uploaded, upload nothing")
    args = parser.parse_args()
    notes = read_notes(args.notes, args.notes_file)

    zip_path = args.zip
    if not zip_path:
        subprocess.check_call([sys.executable, os.path.join(ROOT, "tools", "package_release.py")])
        zips = glob.glob(os.path.join(ROOT, "dist", "GroomLab-*" + SUFFIX))
        if not zips:
            sys.exit("package_release.py made no zip in dist/")
        zip_path = max(zips, key=os.path.getmtime)
    base = os.path.basename(zip_path)
    if not (base.startswith("GroomLab-") and base.endswith(SUFFIX)):
        sys.exit("%s is not a GroomLab release zip" % base)
    version = base[len("GroomLab-"):-len(SUFFIX)]
    builds = maya_builds(zip_path, version)
    if builds.get("2022", {}).get("status") != "tested":
        sys.exit("%s has no tested Maya 2022 build" % base)
    years = sorted(builds)
    beta = [y for y in years if builds[y].get("status") != "tested"]

    with open(zip_path, "rb") as f:
        body = f.read()
    versioned = "maya/GroomLab-%s-Maya-Windows.zip" % version
    latest = "maya/GroomLab-Maya-Windows-latest.zip"
    info = release_info(version, versioned, body, notes, maya="2022", mayaVersions=years, mayaBeta=beta,
                        platform="windows")
    uploads = [
        (versioned, body, "application/zip", IMMUTABLE),
        (latest, body, "application/zip", SHORT),
        ("maya/GroomLab-Maya2022-Windows-latest.zip", body, "application/zip", SHORT),
        ("maya/latest.json", json_body(info), "application/json", SHORT),
        ("maya/releases.json", json_body(release_index("maya/releases.json", info)), "application/json", SHORT),
    ]
    if not upload(uploads, args.dry_run):
        print(json.dumps(info, indent=2))
        return
    print("GroomLab %s is live: %s/%s" % (version, PUBLIC, latest))


if __name__ == "__main__":
    main()
