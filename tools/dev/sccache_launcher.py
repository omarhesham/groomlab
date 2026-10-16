# SPDX-License-Identifier: Apache-2.0
"""One compile through sccache, with this source tree's own cache (the compiler launcher the build
sets when GROOMLAB_COMPILER_CACHE (opt-in) finds sccache: cmake/GroomLabBuildSpeed.cmake).

    python sccache_launcher.py --sccache=<exe> --root=<source tree> --build=<build folder>
                               [--seed-root=<tree>] [--includes-prefix=<text>] -- <compiler> <args...>

Each source tree (the main checkout, every worktree) keeps its cache in <root>/build/.sccache with a
server of its own (one port per tree): sccache's local cache takes one server at a time, and the
server's settings (the cache folder, the base directory) are fixed when it starts. The tree's root is
stripped from the cache keys (SCCACHE_BASEDIRS), so a worktree whose cache was seeded from the main
checkout's hits for every file the two trees have alike.

A hit replays the compile's output as the tree that compiled it saw it, /showIncludes included: the
headers of that tree. Ninja reads its dependencies from those lines, so the seed tree's paths are
mapped to this tree's (and its build folder to this one), or an edit of a header here would not
rebuild the objects that came from the cache.

When sccache itself fails (its server did not start, say), the compiler runs alone: the cache is
never what breaks a build.
"""

import hashlib
import os
import subprocess
import sys


def parse(argv):
    opts = {}
    i = 0
    while i < len(argv) and argv[i] != "--":
        key, _, value = argv[i][2:].partition("=")
        opts[key] = value
        i += 1
    return opts, argv[i + 1:]


def norm(path):
    return os.path.normpath(path).rstrip("\\/") + os.sep


def rewrite(data, prefix, root, seed, build):
    """The /showIncludes lines naming the seed tree's files, naming this tree's instead."""
    if seed is None or prefix not in data:
        return data
    root_l, seed_l = root.lower(), seed.lower()
    seed_build = (seed + "build" + os.sep).lower()
    own_build_rel = None
    if build.lower().startswith((root + "build" + os.sep).lower()):
        own_build_rel = build[len(root):]  # build\<folder>\
    lines = data.split(b"\n")
    for n, raw in enumerate(lines):
        if not raw.startswith(prefix):
            continue
        text = raw[len(prefix):].decode("latin-1")
        path = text.lstrip(" ")
        low = path.lower()
        if low.startswith(root_l) or not low.startswith(seed_l):
            continue
        if own_build_rel and low.startswith(seed_build):
            rest = path[len(seed_build):]
            sep = rest.find(os.sep)
            new = root + own_build_rel + rest[sep + 1:] if sep >= 0 else root + path[len(seed):]
        else:
            new = root + path[len(seed):]
        lines[n] = prefix + (text[:len(text) - len(path)] + new).encode("latin-1")
    return b"\n".join(lines)


def main():
    opts, command = parse(sys.argv[1:])
    root = norm(opts["root"])
    build = norm(opts["build"])
    seed = norm(opts["seed-root"]) if opts.get("seed-root") else None
    if seed is not None and seed.lower() == root.lower():
        seed = None
    prefix = opts.get("includes-prefix", "Note: including file:").rstrip(" ").encode("latin-1")

    env = os.environ.copy()
    env["SCCACHE_DIR"] = os.path.join(root, "build", ".sccache")
    env["SCCACHE_BASEDIRS"] = root.rstrip(os.sep)
    digest = int(hashlib.md5(root.lower().encode("utf-8")).hexdigest(), 16)
    env["SCCACHE_SERVER_PORT"] = str(4300 + digest % 600)
    env.setdefault("SCCACHE_CACHE_SIZE", "20G")

    proc = subprocess.run([opts["sccache"]] + command, env=env, capture_output=True)
    if proc.returncode != 0 and b"sccache: error" in proc.stderr and not command[0].startswith("-"):
        # sccache's own failure, not the compile's: compile without it.
        proc = subprocess.run(command, capture_output=True)
    sys.stdout.buffer.write(rewrite(proc.stdout, prefix, root, seed, build))
    sys.stderr.buffer.write(rewrite(proc.stderr, prefix, root, seed, build))
    sys.stdout.flush()
    sys.stderr.flush()
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
