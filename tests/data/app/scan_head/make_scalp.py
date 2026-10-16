# SPDX-License-Identifier: Apache-2.0
"""Cuts the scalp (where hair grows) out of head.obj: the faces above a hairline that
runs high over the brow and low at the nape, without the ears. Writes head_scalp.obj (with
the UVs: the groom's maps could use them) beside it."""
import math
import os
import sys

import numpy as np

d = sys.argv[1]
FRONT, SIDE, BACK = (float(v) for v in (sys.argv[2:5] if len(sys.argv) > 4 else (13.2, 8.5, 2.5)))  # the hairline's height, cm

lines = open(os.path.join(d, "head.obj")).read().splitlines()
verts, uvs, normals, faces = [], [], [], []
for line in lines:
    if line.startswith("v "):
        verts.append([float(x) for x in line.split()[1:4]])
    elif line.startswith("vt "):
        uvs.append([float(x) for x in line.split()[1:3]])
    elif line.startswith("vn "):
        normals.append([float(x) for x in line.split()[1:4]])
    elif line.startswith("f "):
        faces.append([tuple(int(k) if k else 0 for k in c.split("/")) for c in line.split()[1:]])
verts = np.array(verts)


def hairline(x, z):
    # 0 at the front (+z), 1 at the back; the sides half way.
    phi = math.atan2(abs(x), z)  # 0 front .. pi back
    t = phi / math.pi
    if t < 0.5:
        return FRONT + (SIDE - FRONT) * (t / 0.5)
    return SIDE + (BACK - SIDE) * ((t - 0.5) / 0.5)


def in_hair(p):
    x, y, z = p
    if y < hairline(x, z):
        return False
    # The ears: out to the sides, below the temples.
    if abs(x) > 6.8 and y < 11.0 and -5.0 < z < 2.5:
        return False
    return True


kept = [f for f in faces if all(in_hair(verts[c[0] - 1]) for c in f)]
used = sorted({c[0] for f in kept for c in f})
remap = {v: i + 1 for i, v in enumerate(used)}
uv_used = sorted({c[1] for f in kept for c in f})
uv_remap = {v: i + 1 for i, v in enumerate(uv_used)}
area = 0.0
for f in kept:
    a, b, c = (verts[k[0] - 1] for k in f)
    area += 0.5 * np.linalg.norm(np.cross(b - a, c - a))
with open(os.path.join(d, "head_scalp.obj"), "w") as out:
    out.write("# The scalp of the Lee Perry-Smith head scan (Infinite Realities, CC BY 3.0): the faces above the hairline, make_scalp.py\n")
    for v in used:
        out.write("v %.5f %.5f %.5f\n" % tuple(verts[v - 1]))
    for t in uv_used:
        out.write("vt %.6f %.6f\n" % tuple(uvs[t - 1]))
    for f in kept:
        out.write("f " + " ".join("%d/%d" % (remap[c[0]], uv_remap[c[1]]) for c in f) + "\n")
print("scalp: %d of %d faces, %d vertices, %.0f cm2" % (len(kept), len(faces), len(used), area))
