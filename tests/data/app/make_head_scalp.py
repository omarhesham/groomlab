# SPDX-License-Identifier: Apache-2.0
"""Writes head_scalp.obj beside this script: a head-like scalp for the app's lookdev checks.

An ellipsoid skull (8 x 10 x 9 cm half-axes, y up) cut below the hairline (a plane tilted so
the cap reaches lower at the back than over the brow), with latitude-longitude UVs, about 2k
triangles. Generated, so nothing to license; small, so it can live in the repository.

    python tests/data/app/make_head_scalp.py
"""

import math
import os

RX, RY, RZ = 8.0, 10.0, 9.0   # half-axes, cm
RINGS, SEGMENTS = 24, 48      # the cap's resolution


def main():
    verts, uvs, faces = [], [], []
    index = {}
    # Rings from the crown down to the hairline. The hairline: higher at the front (brow) than
    # at the back (nape): the cap's lowest latitude depends on the azimuth.
    for r in range(RINGS + 1):
        for s in range(SEGMENTS + 1):
            u = s / SEGMENTS
            phi = u * 2.0 * math.pi
            # Where the hair stops, as a polar angle: 70 degrees at the brow, 125 at the nape.
            back = 0.5 - 0.5 * math.cos(phi)  # 0 front (+z), 1 back
            cut = math.radians(70.0 + 55.0 * back)
            theta = cut * r / RINGS
            x = RX * math.sin(theta) * math.sin(phi)
            y = RY * math.cos(theta)
            z = RZ * math.sin(theta) * math.cos(phi)
            index[(r, s)] = len(verts) + 1
            verts.append((x, y - 2.0, z))  # the ellipsoid's center 2 cm below the origin: the crown near y = 8
            uvs.append((u, 1.0 - theta / math.pi))
    for r in range(RINGS):
        for s in range(SEGMENTS):
            a, b = index[(r, s)], index[(r, s + 1)]
            c, d = index[(r + 1, s + 1)], index[(r + 1, s)]
            if r == 0:
                faces.append((a, d, c))  # the pole ring: triangles (outward: counterclockwise seen from outside)
            else:
                faces.append((a, c, b))
                faces.append((a, d, c))
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "head_scalp.obj")
    with open(path, "w") as f:
        f.write("# A head-like scalp (an ellipsoid cut at the hairline), cm, y up: make_head_scalp.py\n")
        for v in verts:
            f.write("v %.4f %.4f %.4f\n" % v)
        for t in uvs:
            f.write("vt %.5f %.5f\n" % t)
        for a, b, c in faces:
            f.write("f %d/%d %d/%d %d/%d\n" % (a, a, b, b, c, c))
    print("wrote %s: %d vertices, %d triangles" % (path, len(verts), len(faces)))


if __name__ == "__main__":
    main()
