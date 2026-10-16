# SPDX-License-Identifier: Apache-2.0
"""The downloads bucket (Cloudflare R2) the publish scripts upload to: publish_release.py (Maya),
publish_blender.py and publish_app.py.

Served from https://downloads.getgroomlab.com. The credentials come from the environment, never
from the repo: R2_ACCOUNT_ID, R2_ACCESS_KEY_ID, R2_SECRET_ACCESS_KEY (an R2 API token limited to
the bucket) and optionally R2_BUCKET (default groomlab-downloads). Standard library only: an
upload is a plain S3 PUT signed with AWS Signature V4, which R2 speaks.
"""

import datetime
import hashlib
import hmac
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

PUBLIC = "https://downloads.getgroomlab.com"
# Cache-Control of a versioned file (never changes) and of the moving ones (latest, the indexes).
IMMUTABLE = "public, max-age=31536000, immutable"
SHORT = "public, max-age=300"


def _sign(key, msg):
    return hmac.new(key, msg.encode("utf-8"), hashlib.sha256).digest()


def put(account, key_id, secret, bucket, name, body, content_type, cache_control):
    """PUT one object into the bucket (SigV4, region auto, service s3)."""
    host = "%s.r2.cloudflarestorage.com" % account
    path = "/%s/%s" % (bucket, urllib.parse.quote(name))
    now = datetime.datetime.now(datetime.timezone.utc)
    amz_date, day = now.strftime("%Y%m%dT%H%M%SZ"), now.strftime("%Y%m%d")
    payload = hashlib.sha256(body).hexdigest()
    headers = {
        "cache-control": cache_control,
        "content-type": content_type,
        "host": host,
        "x-amz-content-sha256": payload,
        "x-amz-date": amz_date,
    }
    signed = ";".join(sorted(headers))
    canonical = "\n".join(["PUT", path, "", "".join("%s:%s\n" % (k, headers[k]) for k in sorted(headers)), signed, payload])
    scope = "%s/auto/s3/aws4_request" % day
    to_sign = "\n".join(["AWS4-HMAC-SHA256", amz_date, scope, hashlib.sha256(canonical.encode()).hexdigest()])
    k = _sign(_sign(_sign(_sign(("AWS4" + secret).encode(), day), "auto"), "s3"), "aws4_request")
    signature = hmac.new(k, to_sign.encode(), hashlib.sha256).hexdigest()
    headers["authorization"] = "AWS4-HMAC-SHA256 Credential=%s/%s, SignedHeaders=%s, Signature=%s" % (key_id, scope, signed, signature)
    request = urllib.request.Request("https://%s%s" % (host, path), data=body, method="PUT",
                                     headers={k: v for k, v in headers.items() if k != "host"})
    with urllib.request.urlopen(request) as response:
        if response.status != 200:
            sys.exit("upload of %s failed: HTTP %d" % (name, response.status))


def version_key(version):
    """Sort key for "1.2.3" (a pre-release suffix sorts before its release)."""
    core, _, pre = version.partition("-")
    numbers = tuple(int(p) if p.isdigit() else 0 for p in core.split("."))
    return numbers + ((1,) if not pre else (0, pre))


def published_json(name):
    """A JSON file as the bucket serves it now (None when there is none yet)."""
    try:
        # A named client: Cloudflare turns away Python's default User-Agent.
        request = urllib.request.Request("%s/%s" % (PUBLIC, name), headers={"User-Agent": "GroomLab-publish/1"})
        with urllib.request.urlopen(request, timeout=10) as response:
            return json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        if e.code != 404:
            raise
        return None


def release_index(name, info):
    """The published release index `name` ({"releases": [...]}) with `info` in place of its
    version's entry, newest first (publishing a version again replaces its entry)."""
    releases = (published_json(name) or {}).get("releases", [])
    releases = [r for r in releases if r.get("version") != info["version"]] + [info]
    releases.sort(key=lambda r: version_key(r["version"]), reverse=True)
    return {"releases": releases}


def release_info(version, versioned, body, notes, **extra):
    """latest.json's fields for a zip uploaded as `versioned`."""
    info = {
        "version": version,
        "file": versioned,
        "url": "%s/%s" % (PUBLIC, versioned),
        "sha256": hashlib.sha256(body).hexdigest(),
        "size": len(body),
        "date": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "notes": notes,
    }
    info.update(extra)
    return info


def json_body(data):
    return (json.dumps(data, indent=2) + "\n").encode()


def read_notes(notes, notes_file):
    """--notes, or --notes-file's text (a BOM from Windows editors dropped), stripped."""
    if notes_file:
        with open(notes_file, encoding="utf-8-sig") as f:
            notes = f.read()
    return (notes or "").strip()


def upload(uploads, dry_run):
    """Uploads [(name, body, content type, cache control)] in order (the moving files last), or
    prints them under --dry-run. Returns whether anything was uploaded."""
    if dry_run:
        for name, data, _, cache in uploads:
            print("would upload %s/%s (%.1f MB, %s)" % (PUBLIC, name, len(data) / 1e6, cache))
        return False
    env = {k: os.environ.get(k) for k in ("R2_ACCOUNT_ID", "R2_ACCESS_KEY_ID", "R2_SECRET_ACCESS_KEY")}
    missing = [k for k, v in env.items() if not v]
    if missing:
        sys.exit("set %s in the environment (an R2 API token for the downloads bucket)" % ", ".join(missing))
    bucket = os.environ.get("R2_BUCKET", "groomlab-downloads")
    for name, data, content_type, cache in uploads:
        put(env["R2_ACCOUNT_ID"], env["R2_ACCESS_KEY_ID"], env["R2_SECRET_ACCESS_KEY"], bucket, name, data, content_type, cache)
        print("uploaded %s/%s" % (PUBLIC, name))
    return True
