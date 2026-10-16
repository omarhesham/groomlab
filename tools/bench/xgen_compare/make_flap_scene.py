# SPDX-License-Identifier: Apache-2.0
"""A groom on a thin flap standing on a floor (an ear on a head, a fold of a shell), for looking
at Interpolate's followSurface: the flap's two faces are 0.3 cm apart, close in a straight line
and far along the skin.

The flap is 6 cm high and 10 cm long, the floor 16 x 10 cm; guides stand on a 1 cm grid: on the
flap's -z face combed out and down (red hair), on its +z face combed along it (blond), on
the floor straight up and a little toward +x (black). Interpolate (each guide's "side", 0, 1
or 2, blended into the strands), HairColor by that side, Width, Resample: a strand blending
guides of two sides shows a color between theirs.

    python make_flap_scene.py --out flap.groom [--density 120] [--follow off]
    GroomLab.exe flap.groom --screenshot out.png --size 1600x1000 --orbit 35 15 --distance 30
"""

import argparse
import json
import math

HEIGHT, THICKNESS, DEPTH, HALF_WIDTH, STEP = 6.0, 0.3, 10.0, 8.0, 0.25


def profile():
    """The flap's cross-section in (y, z), from the floor on the -z side round to the +z side."""
    pts = []

    def line(y0, z0, y1, z1):
        n = max(1, int(math.ceil(math.hypot(y1 - y0, z1 - z0) / STEP)))
        for i in range(0 if not pts else 1, n + 1):
            t = i / n
            pts.append((y0 + (y1 - y0) * t, z0 + (z1 - z0) * t))

    r = 0.5 * THICKNESS
    line(0.0, -HALF_WIDTH, 0.0, -r)
    line(0.0, -r, HEIGHT, -r)
    arc = max(4, int(math.ceil(math.pi * r / (STEP * 0.25))))
    for i in range(1, arc + 1):
        a = math.pi * i / arc
        pts.append((HEIGHT + r * math.sin(a), -r * math.cos(a)))
    line(HEIGHT, r, 0.0, r)
    line(0.0, r, 0.0, HALF_WIDTH)
    return pts


def mesh():
    prof = profile()
    columns = int(math.ceil(DEPTH / STEP))
    points = []
    for c in range(columns + 1):
        x = DEPTH * c / columns - 0.5 * DEPTH
        for y, z in prof:
            points.append((x, y, z))
    rows = len(prof)
    tris = []
    for c in range(columns):
        for i in range(rows - 1):
            a, b, d, e = c * rows + i, c * rows + i + 1, (c + 1) * rows + i, (c + 1) * rows + i + 1
            tris.append((a, b, d))
            tris.append((b, e, d))
    return points, tris


def closest_on_triangle(p, a, b, c):
    """Barycentric (u, v) of the closest point (position = a(1-u-v) + b u + c v), by brute force
    over a projection onto the plane clamped into the triangle (good enough on a fine mesh)."""
    def sub(x, y):
        return (x[0] - y[0], x[1] - y[1], x[2] - y[2])

    def dot(x, y):
        return x[0] * y[0] + x[1] * y[1] + x[2] * y[2]

    ab, ac, ap = sub(b, a), sub(c, a), sub(p, a)
    d00, d01, d11 = dot(ab, ab), dot(ab, ac), dot(ac, ac)
    d20, d21 = dot(ap, ab), dot(ap, ac)
    den = d00 * d11 - d01 * d01
    if den == 0:
        return 0.0, 0.0
    u = (d11 * d20 - d01 * d21) / den
    v = (d00 * d21 - d01 * d20) / den
    u, v = max(0.0, u), max(0.0, v)
    if u + v > 1.0:
        s = u + v
        u, v = u / s, v / s
    return u, v


def root_on(points, tris, p):
    best = None
    for t, (i, j, k) in enumerate(tris):
        a, b, c = points[i], points[j], points[k]
        u, v = closest_on_triangle(p, a, b, c)
        q = tuple(a[n] * (1 - u - v) + b[n] * u + c[n] * v for n in range(3))
        d = sum((q[n] - p[n]) ** 2 for n in range(3))
        if best is None or d < best[0]:
            best = (d, t, u, v, q)
    return best[1], best[2], best[3], best[4]


def guides(points, tris):
    r = 0.5 * THICKNESS
    out = []
    specs = []
    for ix in range(9):
        x = -4.0 + ix
        for iy in range(5):
            y = 0.6 + iy * 1.1
            specs.append(((x, y, -r), (0.0, -2.5, -3.5), 0.0))   # out of the -z face and down
            specs.append(((x, y, r), (4.0, -0.5, 0.4), 1.0))     # along the +z face
        for z in (-6.0, -4.0, -2.0, 2.0, 4.0, 6.0):
            specs.append(((x, 0.0, z), (0.8, 4.0, 0.0), 2.0))    # up out of the floor
    for gid, (p, d, side) in enumerate(specs, start=1):
        face, u, v, q = root_on(points, tris, p)
        pts = []
        for k in range(5):
            t = k / 4.0
            pts.extend((q[0] + d[0] * t, q[1] + d[1] * t, q[2] + d[2] * t))
        out.append({"id": gid, "group": 1, "root": [face, u, v], "frameUp": [1.0, 0.0, 0.0],
                    "tube": {"radius": [0.5, 0.5], "twist": 0.0}, "points": pts, "attributes": {"side": side}})
    return {"groups": [{"id": 1, "name": "primary", "parent": 0}], "guides": out}


def graph(density, follow):
    return {
        "nodes": [
            {"name": "scalp", "type": "Scalp"},
            {"name": "guides", "type": "Guides"},
            {"name": "scatter", "type": "Scatter", "params": {"density": density, "length": 4.0}},
            {"name": "interpolate", "type": "Interpolate", "params": {"followSurface": follow}},
            {"name": "color", "type": "HairColor", "params": {
                "melanin": {"expression": "side < 0.5 ? 0.35 : (side < 1.5 ? 0.03 : 0.95)"},
                "redness": {"expression": "max(0, 1 - side)"}, "melaninRandom": 0.0, "rednessRandom": 0.0}},
            {"name": "width", "type": "Width", "params": {"width": 0.02}},
            {"name": "resample", "type": "Resample", "params": {"segments": 11, "mode": "even"}},
            {"name": "output", "type": "Output"},
        ],
        "connections": [
            {"from": "scalp", "to": "scatter", "input": "scalp"},
            {"from": "scatter", "to": "interpolate", "input": "strands"},
            {"from": "guides", "to": "interpolate", "input": "guides"},
            {"from": "scalp", "to": "interpolate", "input": "scalp"},
            {"from": "interpolate", "to": "color", "input": "strands"},
            {"from": "color", "to": "width", "input": "strands"},
            {"from": "width", "to": "resample", "input": "strands"},
            {"from": "resample", "to": "output", "input": "strands"},
        ],
        "output": "output",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--out", required=True)
    parser.add_argument("--density", type=float, default=120.0)
    parser.add_argument("--follow", choices=["on", "off"], default="on")
    parser.add_argument("--obj", help="also the scalp as an OBJ")
    args = parser.parse_args()
    points, tris = mesh()
    groom = {
        "format": "groomlab.groom",
        "version": 1,
        "framesPerSecond": 24.0,
        "units": "cm",
        "source": "tools/bench/xgen_compare/make_flap_scene.py",
        "graph": graph(args.density, args.follow == "on"),
        "hierarchy": guides(points, tris),
        "scalp": {"points": [c for p in points for c in p], "triangles": [v for t in tris for v in t]},
    }
    with open(args.out, "w") as f:
        json.dump(groom, f)
    if args.obj:
        with open(args.obj, "w") as f:
            for p in points:
                f.write("v %r %r %r\n" % p)
            for t in tris:
                f.write("f %d %d %d\n" % (t[0] + 1, t[1] + 1, t[2] + 1))
    print("%s: %d vertices, %d triangles, %d guides" % (args.out, len(points), len(tris), len(groom["hierarchy"]["guides"])))


if __name__ == "__main__":
    main()
