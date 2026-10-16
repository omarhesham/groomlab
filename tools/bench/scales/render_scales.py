# SPDX-License-Identifier: Apache-2.0
"""Renders the Dragon scales preset in Arnold, for a look at scales: a sphere 20 cm across (the
size the preset was made on) close up and whole, and a snake's body (a tube winding along +Z,
tapering to its tail, the preset combed toward the tail). No Maya: the groom files are written
here (the scalp and the preset's graph) and kick renders them through the groomlab procedural,
whose scales take the built-in plates' standard_surface reading the scaleColor user data.

    python tools/bench/scales/render_scales.py [--build build/ninja-maya2022] [--out folder]
        [--kick kick.exe] [--size 1280x720] [--aa 5]

Writes <out>/<shot>.png (default: tools/bench/scales/renders, where JPEG copies of the three are
kept) and the groom files and scenes in
<out>/work.
"""

import argparse
import json
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PRESET = os.path.join(REPO, "hosts", "maya", "presets", "dragon_scales.json")


def sphere(radius, rings, segments):
    """(points, triangles, uvs per corner) of a lat-long sphere, outward."""
    points, uvs_at = [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2.0 * math.pi * s / segments
            points.append((radius * math.sin(theta) * math.cos(phi), radius * math.cos(theta), radius * math.sin(theta) * math.sin(phi)))
            uvs_at.append((s / segments, 1.0 - r / rings))
    at = lambda r, s: r * (segments + 1) + s
    tris = []
    for r in range(rings):
        for s in range(segments):
            if r > 0:
                tris.append((at(r, s), at(r, s + 1), at(r + 1, s)))
            if r < rings - 1:
                tris.append((at(r, s + 1), at(r + 1, s + 1), at(r + 1, s)))
    return points, tris, [uvs_at[v] for t in tris for v in t]


def snake(length, radius, around, along, waves=1.25, amplitude=6.0):
    """A body winding along +Z (head at -Z), tapering to its tail: (points, triangles, uvs)."""
    def center(z):
        return (amplitude * math.sin(2.0 * math.pi * waves * (z / length + 0.5)), 0.0, z)
    points, uvs_at = [], []
    for i in range(along + 1):
        t = i / along
        z = -0.5 * length + length * t
        c = center(z)
        n = center(z + 1e-3)
        tangent = [(n[k] - c[k]) * 1e3 for k in range(3)]
        tl = math.sqrt(sum(x * x for x in tangent))
        tangent = [x / tl for x in tangent]
        side = [tangent[2], 0.0, -tangent[0]]  # up x tangent: across, level
        sl = math.sqrt(sum(x * x for x in side))
        side = [x / sl for x in side]
        up = [tangent[1] * side[2] - tangent[2] * side[1], tangent[2] * side[0] - tangent[0] * side[2],
              tangent[0] * side[1] - tangent[1] * side[0]]
        r = radius * (0.25 + 0.75 * min(1.0, 1.6 * (1.0 - t))) * min(1.0, 0.6 + 4.0 * t)
        for j in range(around + 1):
            a = 2.0 * math.pi * j / around
            d = [math.cos(a) * side[k] + math.sin(a) * up[k] for k in range(3)]
            points.append(tuple(c[k] + r * d[k] for k in range(3)))
            uvs_at.append((j / around, t))
    at = lambda i, j: i * (around + 1) + j
    tris = []
    for i in range(along):
        for j in range(around):
            tris.append((at(i, j), at(i + 1, j), at(i + 1, j + 1)))
            tris.append((at(i, j), at(i + 1, j + 1), at(i, j + 1)))
    # Outward: the first triangle's normal away from the axis, else every one flipped.
    a, b, c3 = (points[v] for v in tris[0])
    e1, e2 = [b[k] - a[k] for k in range(3)], [c3[k] - a[k] for k in range(3)]
    normal = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
    outward = [a[k] - center(a[2])[k] for k in range(3)]
    if sum(normal[k] * outward[k] for k in range(3)) < 0:
        tris = [(t[0], t[2], t[1]) for t in tris]
    return points, tris, [uvs_at[v] for t in tris for v in t]


def preset_graph(comb_direction=None):
    with open(PRESET, encoding="utf-8") as f:
        preset = json.load(f)
    graph = {k: preset[k] for k in ("nodes", "connections", "output")}
    graph["version"] = 1
    for node in graph["nodes"]:
        node.pop("ui", None)
        if comb_direction and node["name"] == "combDown":
            node["params"]["direction"] = list(comb_direction)
    return graph


def write_groom(path, mesh, graph):
    points, tris, uvs = mesh
    groom = {"format": "groomlab.groom", "version": 1, "framesPerSecond": 24.0, "source": "render_scales",
             "graph": graph,
             "scalp": {"points": [c for p in points for c in p], "triangles": [v for t in tris for v in t],
                       "uvs": [c for uv in uvs for c in uv], "colorSets": {}},
             "guides": [], "hierarchy": {"version": 1, "groups": [], "guides": []},
             "guideCache": None, "scalpCache": None}
    with open(path, "w", encoding="utf-8") as f:
        json.dump(groom, f)


def render(kick, procedurals, out, groom, mesh, eye, target, fov, size, aa):
    points, tris, _ = mesh
    width, height = size
    fov_x = math.degrees(2.0 * math.atan(math.tan(math.radians(fov) * 0.5) * width / height))
    ass = [
        "options {\n xres %d\n yres %d\n AA_samples %d\n GI_diffuse_depth 1\n GI_specular_depth 2\n"
        " outputs \"RGBA RGBA gaussian_filter png_driver\"\n camera \"cam\"\n}\n" % (width, height, aa),
        "gaussian_filter { name gaussian_filter }\n",
        "driver_png { name png_driver filename \"%s\" }\n" % out.replace("\\", "/"),
        "persp_camera {\n name cam\n position %f %f %f\n look_at %f %f %f\n up 0 1 0\n fov %f\n}\n" % (eye + target + (fov_x,)),
        # A key from the upper left, a dim fill from the right, a soft sky.
        "distant_light {\n name key\n direction -0.5 -0.7 -0.45\n intensity 2.5\n angle 2\n}\n",
        "distant_light {\n name fill\n direction 0.8 -0.2 -0.5\n intensity 0.4\n color 0.8 0.9 1\n}\n",
        "skydome_light {\n name sky\n color 0.35 0.37 0.4\n intensity 1\n camera 0\n}\n",
        "standard_hair { name hair_shader }\n",
        "groomlab_procedural {\n name groom\n filename \"%s\"\n shader \"hair_shader\"\n}\n" % groom.replace("\\", "/"),
        "standard_surface { name skin_shader base 1 base_color 0.04 0.05 0.03 specular 0.2 }\n",
        "polymesh {\n name body\n nsides %d 1 UINT\n %s\n vidxs %d 1 UINT\n %s\n vlist %d 1 VECTOR\n %s\n smoothing on\n"
        " shader \"skin_shader\"\n}\n" % (len(tris), " ".join("3" for _ in tris), 3 * len(tris),
                                          " ".join("%d %d %d" % t for t in tris), len(points),
                                          " ".join("%.4f %.4f %.4f" % p for p in points)),
    ]
    scene = os.path.splitext(groom)[0] + ".ass"
    with open(scene, "w") as f:
        f.write("".join(ass))
    cmd = [kick, "-i", scene, "-dw", "-dp", "-nostdin", "-v", "2", "-l", procedurals]
    print(" ".join(cmd))
    return subprocess.call(cmd)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--build", default=os.path.join(REPO, "build", "ninja-maya2022"))
    p.add_argument("--out", default=os.path.join(HERE, "renders"))
    p.add_argument("--kick", default=r"C:\Program Files\Autodesk\Arnold\maya2022\bin\kick.exe")
    p.add_argument("--size", default="1280x720")
    p.add_argument("--aa", type=int, default=5)
    args = p.parse_args()
    procedurals = os.path.join(args.build, "hosts", "arnold", "procedurals", "RelWithDebInfo")
    work = os.path.join(args.out, "work")
    os.makedirs(work, exist_ok=True)
    size = tuple(int(v) for v in args.size.split("x"))

    ball = sphere(10.0, 64, 128)
    write_groom(os.path.join(work, "sphere.groom"), ball, preset_graph())
    body = snake(70.0, 3.2, 48, 240)
    write_groom(os.path.join(work, "snake.groom"), body, preset_graph(comb_direction=(0.0, 0.0, 1.0)))
    shots = [
        ("sphere_closeup", "sphere", ball, (7.0, 12.0, 13.0), (1.5, 6.5, 6.5), 28.0),
        ("sphere_mid", "sphere", ball, (14.0, 18.0, 34.0), (0.0, 0.0, 0.0), 38.0),
        ("snake_mid", "snake", body, (24.0, 20.0, -4.0), (2.0, -1.0, 6.0), 45.0),
    ]
    failed = 0
    for name, groom, mesh, eye, target, fov in shots:
        failed += render(args.kick, procedurals, os.path.join(args.out, name + ".png"), os.path.join(work, groom + ".groom"),
                         mesh, eye, target, fov, size, args.aa) != 0
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
