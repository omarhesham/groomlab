# SPDX-License-Identifier: Apache-2.0
"""The GroomLab vs XGen benchmark's scene as a .groom file for the standalone app, without Maya.

The same scalp xgc_common.make_head builds (a radius-10 sphere of 160 x 100 subdivisions, the
faces below 15% of the radius under the equator cut away, scaled 1, 1.1, 1.2), the same guides
(GroomLabGroom.guide_curves: one vertex per 1.6 cm grid cell, a straight guide of 15 cm along
its normal, tube radius 0.8) and the same graph (groomlab_graph: Scatter density = count / area
-> Interpolate -> Clump 400 centers, 0.8 -> Noise 0.5 / 1 -> Curl 0.3 / 0.5 -> Width 0.01 ->
Resample 15 even), so the app's numbers (GroomLab.exe --benchmark-drag, --draw-dabs,
--benchmark-orbit, --live-benchmark) compare with the Maya rows of results/*.md.

    python make_app_scene.py --count 100000 --out bench_100k.groom
    python make_app_scene.py --count 500000 --out bench_500k.groom

Maya's polySphere vertex order is not reproduced exactly (which vertex a cell's guide sits on
may differ by a neighbour); the guide count and spacing are the bench's.
"""

import argparse
import json
import math

HEAD_RADIUS = 10.0
HEAD_SUBDIV = (160, 100)
HEAD_SCALE = (1.0, 1.1, 1.2)
HAIR_LENGTH = 15.0
POINTS = 16
WIDTH = 0.01
CLUMPS = 400
CLUMP = 0.8
NOISE = 0.5
NOISE_FREQUENCY = 1.0
CURL_RADIUS = 0.3
CURL_FREQUENCY = 0.5  # (7.5 turns a strand on 16 points; tolerance 0: those points, as the bench measures them)
GUIDE_SPACING = 1.6
TUBE_RADIUS = 0.8


def make_head():
    """The sphere's vertices ring by ring from the bottom (Maya's polySphere: the poles last),
    its quads (two triangles each here) and pole triangles; the faces whose center lies below
    -0.15 R cut away, the scaling applied, unused vertices dropped."""
    sx, sy = HEAD_SUBDIV
    r = HEAD_RADIUS
    verts = []
    for j in range(1, sy):
        theta = math.pi * j / sy  # from the bottom pole
        y = -r * math.cos(theta)
        ring = r * math.sin(theta)
        for i in range(sx):
            phi = 2.0 * math.pi * i / sx
            verts.append((ring * math.sin(phi), y, ring * math.cos(phi)))
    bottom = len(verts)
    verts.append((0.0, -r, 0.0))
    top = len(verts)
    verts.append((0.0, r, 0.0))
    faces = []  # polygons as vertex index lists
    for i in range(sx):
        faces.append([bottom, i, (i + 1) % sx])
    for j in range(sy - 2):
        for i in range(sx):
            a, b = j * sx + i, j * sx + (i + 1) % sx
            c, d = (j + 1) * sx + (i + 1) % sx, (j + 1) * sx + i
            faces.append([a, b, c, d])
    last = (sy - 2) * sx
    for i in range(sx):
        faces.append([top, last + (i + 1) % sx, last + i])
    kept = []
    for f in faces:
        cy = sum(verts[v][1] for v in f) / len(f)
        if cy >= -0.15 * r:
            kept.append(f)
    used = sorted({v for f in kept for v in f})
    remap = {v: k for k, v in enumerate(used)}
    points = [(verts[v][0] * HEAD_SCALE[0], verts[v][1] * HEAD_SCALE[1], verts[v][2] * HEAD_SCALE[2]) for v in used]
    triangles = []
    for f in kept:
        f = [remap[v] for v in f]
        if len(f) == 3:
            triangles.append(tuple(f))
        else:
            triangles.append((f[0], f[1], f[2]))
            triangles.append((f[0], f[2], f[3]))
    return points, triangles


def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def length(a):
    return math.sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2])


def normalized(a):
    n = length(a)
    return (a[0] / n, a[1] / n, a[2] / n) if n > 0 else (0.0, 1.0, 0.0)


def area_and_normals(points, triangles):
    """The surface area (polyEvaluate worldArea) and area-weighted vertex normals."""
    normals = [(0.0, 0.0, 0.0)] * len(points)
    area = 0.0
    for a, b, c in triangles:
        n = cross(sub(points[b], points[a]), sub(points[c], points[a]))
        area += 0.5 * length(n)
        for v in (a, b, c):
            normals[v] = (normals[v][0] + n[0], normals[v][1] + n[1], normals[v][2] + n[2])
    return area, [normalized(n) for n in normals]


def guide_vertices(points):
    """surface_normal_samples: the first vertex (by index) of every GUIDE_SPACING grid cell, by index."""
    cells = {}
    for i, p in enumerate(points):
        key = (int(math.floor(p[0] / GUIDE_SPACING)), int(math.floor(p[1] / GUIDE_SPACING)), int(math.floor(p[2] / GUIDE_SPACING)))
        cells.setdefault(key, i)
    return sorted(cells.values())


def hierarchy(points, triangles, normals):
    """Primary guides straight along the normals, rooted on a triangle at their vertex (the
    barycentric weight all on that corner), as groomLabGuides -fromCurves makes them."""
    first_triangle = {}
    for t, tri in enumerate(triangles):
        for corner, v in enumerate(tri):
            first_triangle.setdefault(v, (t, corner))
    guides = []
    for gid, v in enumerate(guide_vertices(points), start=1):
        p, n = points[v], normals[v]
        t, corner = first_triangle[v]
        u, w = (0.0, 0.0) if corner == 0 else (1.0, 0.0) if corner == 1 else (0.0, 1.0)
        pts = []
        for k in range(4):
            pts.extend((p[0] + n[0] * HAIR_LENGTH * k / 3.0, p[1] + n[1] * HAIR_LENGTH * k / 3.0, p[2] + n[2] * HAIR_LENGTH * k / 3.0))
        up = cross(n, (0.0, 1.0, 0.0))
        up = normalized(up) if length(up) > 1e-6 else (1.0, 0.0, 0.0)
        guides.append({"id": gid, "group": 1, "root": [t, u, w], "frameUp": list(up),
                       "tube": {"shape": "round", "width": [TUBE_RADIUS, TUBE_RADIUS]}, "points": pts})
    return {"groups": [{"id": 1, "name": "primary", "parent": 0}], "guides": guides}


def groomlab_graph(density, length_cm):
    return {
        "nodes": [
            {"name": "scalp", "type": "Scalp"},
            {"name": "guides", "type": "Guides"},
            {"name": "scatter", "type": "Scatter", "params": {"density": density, "length": length_cm}},
            {"name": "interpolate", "type": "Interpolate"},
            {"name": "clump", "type": "Clump", "params": {"count": CLUMPS, "strength": CLUMP}},
            {"name": "noise", "type": "Noise", "params": {"amplitude": NOISE, "frequency": NOISE_FREQUENCY}},
            {"name": "curl", "type": "Curl", "params": {"radius": CURL_RADIUS, "frequency": CURL_FREQUENCY, "tolerance": 0.0}},
            {"name": "width", "type": "Width", "params": {"width": WIDTH}},
            {"name": "resample", "type": "Resample", "params": {"segments": POINTS - 1, "mode": "even"}},
            {"name": "output", "type": "Output"},
        ],
        "connections": [
            {"from": "scalp", "to": "scatter", "input": "scalp"},
            {"from": "scatter", "to": "interpolate", "input": "strands"},
            {"from": "guides", "to": "interpolate", "input": "guides"},
            {"from": "scalp", "to": "interpolate", "input": "scalp"},
            {"from": "interpolate", "to": "clump", "input": "strands"},
            {"from": "clump", "to": "noise", "input": "strands"},
            {"from": "noise", "to": "curl", "input": "strands"},
            {"from": "curl", "to": "width", "input": "strands"},
            {"from": "width", "to": "resample", "input": "strands"},
            {"from": "resample", "to": "output", "input": "strands"},
        ],
        "output": "output",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("--count", type=int, default=100000, help="strands (Scatter density = count / area)")
    parser.add_argument("--out", required=True, help="the .groom file to write")
    parser.add_argument("--obj", help="also the scalp as an OBJ")
    args = parser.parse_args()
    points, triangles = make_head()
    area, normals = area_and_normals(points, triangles)
    h = hierarchy(points, triangles, normals)
    groom = {
        "format": "groomlab.groom",
        "version": 1,
        "framesPerSecond": 24.0,
        "units": "cm",
        "source": "tools/bench/xgen_compare/make_app_scene.py (xgc_common.make_head, %d strands)" % args.count,
        "graph": groomlab_graph(args.count / area, HAIR_LENGTH),
        "hierarchy": h,
        "scalp": {"points": [c for p in points for c in p], "triangles": [v for t in triangles for v in t]},
    }
    with open(args.out, "w") as f:
        json.dump(groom, f)
    if args.obj:
        with open(args.obj, "w") as f:
            for p in points:
                f.write("v %r %r %r\n" % p)
            for t in triangles:
                f.write("f %d %d %d\n" % (t[0] + 1, t[1] + 1, t[2] + 1))
    print("%s: %d vertices, %d triangles, area %.1f cm^2, density %.3f strands/cm^2, %d guides"
          % (args.out, len(points), len(triangles), area, args.count / area, len(h["guides"])))


if __name__ == "__main__":
    main()
