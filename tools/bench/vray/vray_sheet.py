# SPDX-License-Identifier: Apache-2.0
"""A contact sheet of GroomLab looks in V-Ray (from the .vrscene export) beside Arnold, for review.

    python tools/bench/vray/vray_sheet.py --cli groomlab-cli.exe --procedural <groomlab_procedural dir> \
        --out build/probe/vray [--looks hair_basics,cinematic_hair,dyed_tips,blonde,red] [--size 480x270]

Each look is the lookdev head groom (tests/data/app/head_graph.json on head_scalp.obj) with a shipped
preset's look after its combing (tests/hosts/vray/test_vray.py head_graph), one of the pigments the
mapping was fitted at (brown, blonde, dark, red), or an RGB look (dyed_tips: blue with golden
tips). V-Ray: tests/data/app/vray_compare.py (the groom written by groomlab-cli as .vrscene);
Arnold: arnold_compare.py in linear sRGB, the
strands' melanin through user data (the app's groom material), an RGB look (a color node's) as
create_hair_shader gives it (--pigment rgb --base-color-gamma 1.084). The app's studio lights,
the scalp shown. Writes <out>/vray_vs_arnold.png (a row a look: V-Ray left, Arnold right),
<out>/<look>_vray.png and _arnold.png, and <out>/vray_vs_arnold.txt (the rows, and the hair's mean
linear color in each renderer with their ratio). --reuse keeps the renders already in <out>.
Standard library only.
"""

import argparse
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
APP = os.path.join(ROOT, "tests", "data", "app")
sys.path.insert(0, os.path.join(ROOT, "tests", "hosts", "vray"))
import test_vray as tv  # noqa: E402

PIGMENTS = {"brown": (0.35, 0.2), "blonde": (0.12, 0.45), "dark": (0.85, 0.1), "red": (0.4, 0.95)}
# RGB looks (HairColor's color mode: Arnold's base color, as create_hair_shader gives it): blue
# dyed hair with golden tips, the color varying along each strand.
RGB_LOOKS = {"dyed_tips": {"mode": "color", "color": [0.05, 0.1, 0.4], "tipColor": [0.8, 0.6, 0.2],
                           "tipStrength": 1.0, "tipProfile": [[0, 0], [0.45, 0], [1, 1]], "valueRandom": 0.1}}


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--cli", required=True)
    p.add_argument("--procedural", required=True, help="the folder holding groomlab_procedural.dll")
    p.add_argument("--kick", default=r"C:\Program Files\Autodesk\Arnold\maya2022\bin\kick.exe")
    p.add_argument("--vray", default=r"C:\Program Files\Chaos\V-Ray\Standalone for x64\bin\vray.exe")
    p.add_argument("--out", required=True)
    p.add_argument("--looks", default="hair_basics,cinematic_hair,dyed_tips,blonde,red")
    p.add_argument("--size", default="480x270")
    p.add_argument("--density", type=float, default=175.0, help="strands a cm2 (175: about 100k)")
    p.add_argument("--lights", default="studio", choices=["studio", "key", "env"])
    p.add_argument("--reuse", action="store_true", help="keep a look's renders already written")
    args = p.parse_args()
    done = lambda png: args.reuse and os.path.isfile(png) and os.path.getsize(png) > 0
    out = os.path.abspath(args.out)
    os.makedirs(out, exist_ok=True)
    tv.CAMERA = "camera eye 32 4 42 target 0 -3 0 fov 32"

    rows, report = [], []
    for look in args.looks.split(","):
        groom = os.path.join(out, look + ".groom")
        if look in PIGMENTS:
            m, r = PIGMENTS[look]
            graph = tv.head_graph(args.density, color={"melanin": m, "redness": r, "melaninRandom": 0.0})
        elif look in RGB_LOOKS:
            graph = tv.head_graph(args.density, color=RGB_LOOKS[look])
        else:
            graph = tv.head_graph(args.density, preset=look)
        tv.write_groom(groom, graph)
        vpng = os.path.join(out, look + "_vray.png")
        apng = os.path.join(out, look + "_arnold.png")
        if not done(vpng) and subprocess.call([
                sys.executable, os.path.join(APP, "vray_compare.py"), "--groom", groom, "--scalp", tv.SCALP, "--camera",
                tv.CAMERA, "--out", vpng, "--size", args.size, "--vray", args.vray, "--cli", args.cli, "--lights",
                args.lights, "--samples", "12"]):
            sys.exit("V-Ray failed on %s" % look)
        arnold = [sys.executable, os.path.join(APP, "arnold_compare.py"), "--groom", groom, "--scalp", tv.SCALP,
                  "--camera", tv.CAMERA, "--out", apng, "--size", args.size, "--kick", args.kick, "--procedural",
                  args.procedural, "--lights", args.lights, "--linear-srgb", "--aa", "6"]
        if look in RGB_LOOKS:
            arnold += ["--pigment", "rgb", "--base-color-gamma", "1.084"]
        if not done(apng) and subprocess.call(arnold):
            sys.exit("Arnold failed on %s" % look)
        w, h, v = tv.pixels(vpng)
        _, _, a = tv.pixels(apng)
        # The hair: V-Ray's alpha where it is lit hair or scalp; both renderers' mean over it.
        mask = [[p[3] > 200 for p in row] for row in v]
        vm, _ = tv.mean_linear(v, mask)
        am, _ = tv.mean_linear(a, mask)
        report.append("%-16s V-Ray %s  Arnold %s  ratio %s" % (look, " ".join("%.4f" % c for c in vm),
                      " ".join("%.4f" % c for c in am), " ".join("%.2f" % (x / max(y, 1e-6)) for x, y in zip(vm, am))))
        rows += [bytes(c for px in vr for c in px[:3]) + bytes(c for px in ar for c in px[:3]) for vr, ar in zip(v, a)]
    tv.write_png(os.path.join(out, "vray_vs_arnold.png"), 2 * w, len(rows), rows)
    with open(os.path.join(out, "vray_vs_arnold.txt"), "w") as f:
        f.write("Rows, top to bottom (V-Ray left, Arnold right), %s lights; the head and hair's mean linear color:\n"
                % args.lights)
        f.write("\n".join(report) + "\n")
    print("\n".join(report))


if __name__ == "__main__":
    main()
