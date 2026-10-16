# SPDX-License-Identifier: Apache-2.0
"""Writes head.obj + head.mtl (centimeters, y up) in the asset cache from the converted GLB OBJ,
referencing the 4k albedo (Unity package copy), the 1k tangent normal and the 1k specular map."""
import os
import sys

d = sys.argv[1]
scale = 4.6  # the GLB's units to cm: the eyes come 6.3 cm apart then (the scan is a bust: crown to mid-chest 37 cm)
src = os.path.join(d, "LeePerrySmith.obj")
lines = open(src).read().splitlines()
ys = []
with open(os.path.join(d, "head.obj"), "w") as f:
    f.write("# Lee Perry-Smith head scan, Infinite Realities, CC BY 3.0 (three.js's LeePerrySmith.glb, scaled to cm)\n")
    f.write("mtllib head.mtl\n")
    f.write("o head\nusemtl skin\n")
    for line in lines:
        if line.startswith("v "):
            x, y, z = (float(v) * scale for v in line.split()[1:4])
            ys.append(y)
            f.write("v %.5f %.5f %.5f\n" % (x, y, z))
        elif line.startswith("#"):
            continue
        else:
            f.write(line + "\n")
with open(os.path.join(d, "head.mtl"), "w") as f:
    f.write("newmtl skin\n")
    f.write("Kd 1 1 1\n")
    f.write("map_Kd unitypackage/lambertian.jpg\n")
    f.write("map_Bump Infinite-Level_02_Tangent_SmoothUV.jpg\n")
    f.write("map_Ks Map-SPEC.jpg\n")
print("y range", min(ys), max(ys))
