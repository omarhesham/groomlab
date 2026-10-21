# SPDX-License-Identifier: Apache-2.0
"""The app's RenderMan renders (Render > Render Frame / Turntable in RenderMan), as ctest runs them
when prman is installed (label renderman): GroomLab.exe on the head groom renders a frame and a
small turntable without a window (--render-prman, --prman-turntable) through GroomLab's RenderMan
procedural (beside the app's build), and the images have the hair:

- the frame: the groom's .groom and the stage's RIB beside the PNG, the hair covering the head
  region where the app's own screenshot of the same view has it (the camera carried over), brown
  (red over blue) as the groom's look;
- the turntable: one image per frame, the views different.

    python prman_render.py <GroomLab.exe> <prman> <work folder>

GROOMLAB_PRMAN points the app at prman. vray_render.py's PNG reader.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from vray_render import DATA, hair_pixels, read_png  # noqa: E402


def run(exe, args, prman):
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", GROOMLAB_PRMAN=prman)
    proc = subprocess.run([exe] + args, capture_output=True, text=True, env=env, timeout=900)
    if proc.returncode != 0:
        sys.stderr.write(proc.stdout + proc.stderr)
        raise SystemExit("GroomLab %s failed with %d" % (" ".join(args[-4:]), proc.returncode))
    return proc.stdout + proc.stderr


def main(exe, prman, work):
    os.makedirs(work, exist_ok=True)
    groom = [os.path.join(DATA, "head_scalp.obj"), "--graph", os.path.join(DATA, "head_graph.json"), "--size", "160x120"]
    frame = os.path.join(work, "frame.png")
    view = os.path.join(work, "view.png")
    log = run(exe, groom + ["--render-prman", frame], prman)
    assert "(RenderMan)" in log, log
    for path in (frame, os.path.join(work, "frame_groom.groom"), os.path.join(work, "frame_scene.rib")):
        assert os.path.isfile(path), path
    run(exe, groom + ["--screenshot", view, "--tonemap", "none", "--frames", "32"], prman)  # refined: whole strands

    w, h, rows = read_png(frame)
    assert (w, h) == (160, 120), (w, h)
    background = rows[0][0]
    hair = hair_pixels(rows, background)
    assert len(hair) > 0.1 * w * h, "the hair covers %d of %d pixels" % (len(hair), w * h)
    r = sum(p[0] for _, _, p in hair) / len(hair)
    b = sum(p[2] for _, _, p in hair) / len(hair)
    assert r > 1.15 * b, "brown hair (red over blue): r %.1f b %.1f" % (r, b)
    # Where the app draws it (the camera carried over): prman's hair is where the app's is. (Not the
    # union's share as V-Ray's test: PxrMarschnerHair leaves the thin, backlit tips darker than the
    # app does, so prman's mask is the smaller one.)
    _, _, app = read_png(view)
    mine = {(x, y) for x, y, _ in hair}
    theirs = {(x, y) for x, y, _ in hair_pixels(app, app[0][0])}
    inside = len(mine & theirs) / max(1, len(mine))
    covered = len(mine & theirs) / max(1, len(theirs))
    print("RenderMan frame: hair %d px, mean r %.1f b %.1f; inside the app's hair %.2f, covering %.2f of it"
          % (len(hair), r, b, inside, covered))
    assert inside > 0.85 and covered > 0.45, (inside, covered)

    turn = os.path.join(work, "turn.png")
    run(exe, groom + ["--render-prman", turn, "--prman-turntable", "3"], prman)
    images = [os.path.join(work, "turn.%04d.png" % k) for k in (1, 2, 3)]
    for path in images:
        assert os.path.isfile(path), path
    first, second = read_png(images[0])[2], read_png(images[1])[2]
    differ = sum(1 for y in range(h) for x in range(w) if max(abs(first[y][x][k] - second[y][x][k]) for k in range(3)) > 24)
    assert differ > 0.05 * w * h, "the turntable's frames differ in %d pixels" % differ
    print("prman render: ok")


if __name__ == "__main__":
    main(*sys.argv[1:4])
