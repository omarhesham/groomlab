# SPDX-License-Identifier: Apache-2.0
"""Renders HairColor's color patterns in the standalone app (lookdev, refined) for a look at them:
banded and tipped fur on a patch, single feathers (barred with a downy base, spotted, an eye spot,
iridescent from two angles). No Maya: the app's command line (GroomLab.exe --screenshot) on
scalps this script writes. (The feathers as game cards: the card fidelity harness's `barred`
groom, tools/bench/card_fidelity, draws them in Maya's viewport beside their strands.)

    python tools/bench/color_patterns/render_color_patterns.py --app build/app [--out folder] [--frames 32]
        [--arnold build/maya]

Writes <out>/<name>.png (default: tools/bench/color_patterns/renders) and the scalps, graphs and
groom files beside them in <out>/work. With --arnold (a build holding the groomlab procedural)
the shots are rendered in Arnold too, <out>/<name>_arnold.png: kick, the same camera and the app's
studio lights (tests/data/app/arnold_compare.py), the hair shader create_hair_shader makes.
"""

import argparse
import copy
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


def patch_obj(path, size, cells, y=0.0):
    """A square patch in the XZ plane (normal +Y) with UVs, `size` cm across, `cells` quads a side."""
    lines = ["# GroomLab color patterns: a patch, cm, y up"]
    n = cells + 1
    for j in range(n):
        for i in range(n):
            lines.append("v %.5f %.5f %.5f" % ((i / cells - 0.5) * size, y, (0.5 - j / cells) * size))
    for j in range(n):
        for i in range(n):
            lines.append("vt %.5f %.5f" % (i / cells, j / cells))
    for j in range(cells):
        for i in range(cells):
            a = j * n + i + 1
            b, c, d = a + 1, a + n + 1, a + n
            lines.append("f %d/%d %d/%d %d/%d %d/%d" % (a, a, b, b, c, c, d, d))
    with open(path, "w", newline="\n") as f:
        f.write("\n".join(lines) + "\n")


def graph(nodes, chain, extra=()):
    """A graph of (name, type, params) nodes, `chain` each fed by the one before, Scalp to Scatter."""
    out = {"version": 1, "nodes": [{"name": "scalp", "type": "Scalp"}], "connections": [], "output": "output"}
    for name, kind, params in nodes:
        out["nodes"].append({"name": name, "type": kind, "params": params} if params else {"name": name, "type": kind})
    out["nodes"].append({"name": "output", "type": "Output"})
    out["connections"].append({"from": "scalp", "to": "scatter", "input": "scalp"})
    for a, b in zip(chain, chain[1:] + ["output"]):
        out["connections"].append({"from": a, "to": b, "input": "shafts" if b == "propagate" else "strands"})
    out["connections"].extend(extra)
    return out


# A look: the color nodes' params by node, chained in this order (HairColor with the highlight,
# then Bands or FeatherColor over its colors).
LOOK_NODES = (("hair", "color", "HairColor"), ("bands", "bands", "Bands"), ("feathers", "feathers", "FeatherColor"))

AGOUTI = {"hair": {"melanin": 0.62, "redness": 0.35, "melaninRandom": 0.08, "tipColor": [0.02, 0.018, 0.015],
                   "tipStrength": 1.0, "tipProfile": [[0, 0], [0.88, 0], [0.94, 1], [1, 1]],
                   "specular": 0.35, "roughness": 0.45},
          "bands": {"melanin": 0.1, "redness": 0.75, "randomness": 0.08,
                    "profile": [[0, 0], [0.4, 0], [0.5, 1], [0.66, 1], [0.72, 0], [1, 0]], "offsetRandom": 0.08,
                    "widthRandom": 0.08, "guards": 0.4, "undercoatDye": [0.85, 0.82, 0.8]}}


def look_nodes(look):
    """The look's color nodes as (name, type, params), and their names in chain order."""
    nodes = [(name, kind, look[key]) for key, name, kind in LOOK_NODES if key in look]
    return nodes, [n[0] for n in nodes]


def fur(look):
    colors, color_chain = look_nodes(look)
    nodes = [("scatter", "Scatter", {"density": 420.0, "length": 2.2, "seed": 3}),
             ("length", "Length", {"trimMax": 0.45}),
             ("comb", "Gravity", {"strength": 0.55, "direction": [0.0, 0.0, -1.0]}),
             ("clump", "Clump", {"count": 400, "strength": 0.35, "profile": [[0, 0], [1, 1]]}),
             ("noise", "Noise", {"amplitude": 0.04, "frequency": 5.0})] + colors + [
             ("width", "Width", {"width": 0.006, "profile": [[0, 1], [1, 0.2]]}),
             ("resample", "Resample", {"segments": 16, "mode": "even"})]
    chain = ["scatter", "length", "comb", "clump", "noise"] + color_chain + ["width", "resample"]
    return graph(nodes, chain, [])


BARRED = {"hair": {"melanin": 0.82, "redness": 0.1, "dye": [0.8, 0.85, 0.75], "melaninRandom": 0.05,
                   "specular": 0.55, "roughness": 0.35},
          "feathers": {"pattern": "bars", "vanes": "left", "melanin": 0.0, "dye": [0.95, 0.95, 0.92], "count": 6.0,
                       "width": 0.42, "angle": 12.0, "sharpness": 0.75, "offsetRandom": 0.25,
                       "downColor": [0.92, 0.91, 0.88], "downStrength": 1.0, "iridescence": 0.85,
                       "iridescenceOn": "rightVane", "iridescenceColor": [0.04, 0.45, 0.12], "iridescenceShift": 0.22}}


def feather(look):
    """One feather standing up from a tiny patch (its vane facing +Z), a downy base."""
    nodes = [("scatter", "Scatter", {"density": 17.0, "length": 9.0, "randomness": 0.0}),
             ("resampleShaft", "Resample", {"segments": 24, "mode": "even"}),
             ("propagate", "Propagate", {"count": 90, "start": 0.04, "end": 1.0, "segments": 10, "length": 2.6,
                                         "lengthProfile": [[0, 0.35], [0.45, 1], [0.9, 0.65], [1, 0.4]],
                                         "angle": 55.0, "angleProfile": [[0, 1.2], [0.7, 0.9], [1, 0.4]],
                                         "curl": 18.0, "droop": 6.0, "splits": 3.0, "splitAmount": 0.35,
                                         "lengthRandom": 0.06, "angleRandom": 2.0, "shaftWidth": 0.05,
                                         "down": 0.3, "downCount": 30, "downLength": 0.9, "downCurl": 160.0,
                                         "downFrizz": 22.0})] + look_nodes(look)[0] + [
             ("width", "Width", {"width": 0.012, "profile": [[0, 1], [1, 0.4]]})]
    chain = ["scatter", "resampleShaft", "propagate"] + look_nodes(look)[1] + ["width"]
    return graph(nodes, chain)


def run(app, args, log):
    print(" ".join(args), file=log, flush=True)
    result = subprocess.run([app] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=1800)
    print(result.stdout, file=log, flush=True)
    if result.returncode != 0:
        sys.exit("GroomLab failed (%d): %s" % (result.returncode, " ".join(args)))


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--app", default=os.path.join(REPO, "build", "app"), help="the app's build folder")
    parser.add_argument("--out", default=os.path.join(HERE, "renders"))
    parser.add_argument("--frames", type=int, default=32)
    parser.add_argument("--size", default="1280x960")
    parser.add_argument("--only", default="", help="comma-separated names to render")
    parser.add_argument("--arnold", default="", help="a build folder with the groomlab procedural: Arnold renders too")
    args = parser.parse_args()
    app = os.path.join(os.path.abspath(args.app), "hosts", "app", "RelWithDebInfo", "GroomLab.exe")
    out = os.path.abspath(args.out)
    work = os.path.join(out, "work")
    os.makedirs(work, exist_ok=True)
    patch = os.path.join(work, "fur_patch.obj")
    root = os.path.join(work, "feather_root.obj")
    patch_obj(patch, 8.0, 24)
    patch_obj(root, 0.2, 1)
    only = set(n for n in args.only.split(",") if n)
    log = open(os.path.join(work, "log.txt"), "w")
    common = ["--size", args.size, "--frames", str(args.frames), "--lighting", "studio", "--quality", "refine",
              "--tonemap", "aces"]

    def shot(name, scalp, g, extra):
        if only and name not in only:
            return
        path = os.path.join(work, name + ".json")
        with open(path, "w", newline="\n") as f:
            json.dump(g, f, indent=1)
        run(app, [scalp, "--graph", path, "--screenshot", os.path.join(out, name + ".png")] + common + extra, log)
        if args.arnold:
            arnold(name, scalp, path, g, extra)

    def arnold(name, scalp, path, g, extra):
        # The same groom saved, its camera as the app prints it, the network create_hair_shader builds.
        groom = os.path.join(work, name + ".groom")
        log_lines = subprocess.run([app, scalp, "--graph", path, "--save", groom, "--screenshot",
                                    os.path.join(work, name + "_camera.png"), "--size", args.size, "--frames", "1"] + extra,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True).stdout
        camera = next(l.split("GroomLab: camera ")[1] for l in log_lines.splitlines() if "GroomLab: camera " in l)
        render = next((n.get("params", {}) for n in g["nodes"] if n["type"] == "HairColor"), {})  # the highlight
        sheen = next((n.get("params", {}) for n in g["nodes"] if n["type"] == "FeatherColor"), {})
        cmd = [sys.executable, os.path.join(REPO, "tests", "data", "app", "arnold_compare.py"), "--groom", groom,
               "--scalp", scalp, "--camera", "camera " + camera.strip(), "--out", os.path.join(out, name + "_arnold.png"),
               "--size", args.size, "--procedural",
               os.path.join(os.path.abspath(args.arnold), "hosts", "arnold", "procedurals", "RelWithDebInfo"),
               "--pigment", "rgb", "--roughness", str(0.1 + 0.4 * render.get("roughness", 0.5)),
               "--specular-tint", str(min(1.0, render.get("specular", 0.3) / 0.3))]
        if sheen.get("iridescence", 0.0):
            c = sheen.get("iridescenceColor", [0.05, 0.55, 0.2])
            cmd += ["--iridescence", "%g,%g,%g,%g" % (c[0], c[1], c[2], sheen.get("iridescenceShift", 0.3))]
        if "--hide-scalp" in extra:
            cmd.append("--hide-scalp")
        print(" ".join(cmd), file=log, flush=True)
        result = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        print(result.stdout, file=log, flush=True)

    fur_view = ["--look-at", "1.5,0.8,0", "--distance", "7", "--orbit", "50", "5"]
    shot("fur_agouti", patch, fur(AGOUTI), fur_view)
    tipped = {"hair": dict(AGOUTI["hair"], melanin=0.12, redness=0.6, tipColor=[0.03, 0.025, 0.02],
                           tipProfile=[[0, 0], [0.7, 0], [0.85, 1], [1, 1]])}
    shot("fur_tipped", patch, fur(tipped), fur_view)
    shaded = dict(tipped, hair=dict(tipped["hair"], tipProfile=[[0, 0], [1, 1]], tipStrength=0.95, melanin=0.08))
    shot("fur_shaded", patch, fur(shaded), fur_view)

    feather_view = ["--look-at", "0,5.2,0", "--distance", "17", "--hide-scalp"]
    shot("feather_barred", root, feather(BARRED), feather_view + ["--orbit", "-30", "-20"])
    spots = dict(BARRED, hair=dict(BARRED["hair"], melanin=0.75, redness=0.3, dye=[1.0, 1.0, 1.0]),
                 feathers=dict(BARRED["feathers"], pattern="spots", vanes="both", count=9.0, width=0.55, sharpness=0.8,
                               dye=[0.95, 0.93, 0.85], iridescence=0.0))
    shot("feather_spotted", root, feather(spots), feather_view + ["--orbit", "-30", "-20"])
    eye = dict(BARRED, hair=dict(BARRED["hair"], melanin=0.25, redness=0.55, dye=[0.75, 0.62, 0.3]),
               feathers=dict(BARRED["feathers"], pattern="eye", vanes="both", offset=0.66, width=0.5, angle=0.0,
                             sharpness=0.85, offsetRandom=0.0, melanin=0.95, dye=[0.2, 0.3, 1.0],
                             profile=[[0, 1], [0.38, 1], [0.42, 0], [0.62, 0], [0.66, 1], [1, 1]],
                             iridescence=0.6, iridescenceOn="pattern", iridescenceColor=[0.05, 0.25, 0.9]))
    shot("feather_eye", root, feather(eye), feather_view + ["--orbit", "-30", "-20"])
    iridescent = dict(BARRED, hair=dict(BARRED["hair"], melanin=0.9),
                      feathers=dict(BARRED["feathers"], pattern="none", iridescence=1.0, iridescenceOn="all",
                                    iridescenceColor=[0.05, 0.5, 0.15], iridescenceShift=0.35))
    shot("feather_iridescent_front", root, feather(iridescent), feather_view + ["--orbit", "-30", "-20"])
    shot("feather_iridescent_turned", root, feather(iridescent), feather_view + ["--orbit", "30", "-5"])

    print("renders in", out)


if __name__ == "__main__":
    main()
