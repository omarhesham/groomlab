# SPDX-License-Identifier: Apache-2.0
"""Writes an OBJ (positions, uvs, normals, triangles) from a GLB's first mesh primitive."""
import json
import os
import struct
import sys

import numpy as np

src, dst = sys.argv[1], sys.argv[2]
data = open(src, "rb").read()
magic, version, length = struct.unpack_from("<III", data, 0)
assert magic == 0x46546C67
offset = 12
chunks = {}
while offset < length:
    clen, ctype = struct.unpack_from("<II", data, offset)
    chunks[ctype] = data[offset + 8 : offset + 8 + clen]
    offset += 8 + clen
gltf = json.loads(chunks[0x4E4F534A])
bin_ = chunks[0x004E4942]
print(json.dumps({k: gltf.get(k) for k in ("asset", "materials", "images", "textures", "samplers")}, indent=1)[:2000])

ctypes = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
ncomp = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def accessor(i):
    a = gltf["accessors"][i]
    bv = gltf["bufferViews"][a["bufferView"]]
    dt = np.dtype(ctypes[a["componentType"]])
    n = ncomp[a["type"]]
    start = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    stride = bv.get("byteStride", dt.itemsize * n)
    if stride == dt.itemsize * n:
        arr = np.frombuffer(bin_, dtype=dt, count=a["count"] * n, offset=start).reshape(a["count"], n)
    else:
        arr = np.vstack([np.frombuffer(bin_, dtype=dt, count=n, offset=start + k * stride) for k in range(a["count"])])
    return arr


for mi, mesh in enumerate(gltf["meshes"]):
    for pi, prim in enumerate(mesh["primitives"]):
        attrs = prim["attributes"]
        pos = accessor(attrs["POSITION"]).astype(np.float64)
        uv = accessor(attrs["TEXCOORD_0"]).astype(np.float64) if "TEXCOORD_0" in attrs else None
        nrm = accessor(attrs["NORMAL"]).astype(np.float64) if "NORMAL" in attrs else None
        idx = accessor(prim["indices"]).reshape(-1, 3) if "indices" in prim else np.arange(len(pos)).reshape(-1, 3)
        print("mesh", mi, "prim", pi, "verts", len(pos), "tris", len(idx), "uv", uv is not None, "normals", nrm is not None)
        print("bounds", pos.min(axis=0), pos.max(axis=0))
        # The node's transform, when the mesh is placed by one.
        for node in gltf.get("nodes", []):
            if node.get("mesh") == mi:
                print("node", {k: v for k, v in node.items() if k != "mesh"})
        with open(dst, "w") as f:
            f.write("# Lee Perry-Smith head scan, Infinite Realities, CC BY 3.0 (from three.js's LeePerrySmith.glb)\n")
            for p in pos:
                f.write("v %.6f %.6f %.6f\n" % tuple(p))
            if uv is not None:
                for t in uv:
                    # glTF's v would run down from the image's top; this GLB's (three.js's
                    # export) already runs up from the bottom as OBJ's does: kept as is.
                    f.write("vt %.6f %.6f\n" % (t[0], t[1]))
            if nrm is not None:
                for n in nrm:
                    f.write("vn %.5f %.5f %.5f\n" % tuple(n))
            for t in idx:
                a, b, c = (int(k) + 1 for k in t)
                if uv is not None and nrm is not None:
                    f.write("f %d/%d/%d %d/%d/%d %d/%d/%d\n" % (a, a, a, b, b, b, c, c, c))
                elif uv is not None:
                    f.write("f %d/%d %d/%d %d/%d\n" % (a, a, b, b, c, c))
                else:
                    f.write("f %d %d %d\n" % (a, b, c))
        print("wrote", dst)
        break
    break
