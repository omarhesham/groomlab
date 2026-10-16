# SPDX-License-Identifier: Apache-2.0
"""Write compatibility fixtures for the current stored formats (run with mayapy).

Usage: mayapy make_compat_fixtures.py <GroomLab.mll> <hosts/maya/scripts dir> <out dir, e.g. tests/data/compat/v1>

Builds a groom that uses every stored feature (hierarchy with generated and
locked children, texture/color set/attribute maps, ramps, the common
operators) and writes graph.json and guides.json (as stored on the node) and
groom.groom. Then run the core tests with GROOMLAB_UPDATE_GOLDEN=1 once to
record expected.json. Stored formats change in place (no migrations): after an
intended format change, rewrite the fixtures with this script and re-record.
"""

import json
import os
import sys

PLUGIN, SCRIPTS, OUT = (os.path.abspath(a) for a in sys.argv[1:4])
sys.path.insert(0, SCRIPTS)

import maya.standalone  # noqa: E402

maya.standalone.initialize(name="python")

from maya import cmds  # noqa: E402

import groomlab  # noqa: E402

cmds.loadPlugin(PLUGIN)
cmds.file(new=True, force=True)
os.makedirs(OUT, exist_ok=True)

scalp = cmds.polySphere(radius=1, subdivisionsX=24, subdivisionsY=12, name="scalp")[0]
cmds.polyColorPerVertex(scalp, rgb=(0.2, 0.2, 0.2), colorDisplayOption=True)
cmds.polyColorPerVertex(scalp + ".vtx[0:120]", rgb=(1.0, 0.6, 0.3))
groom = groomlab.create_groom(scalp)
curves = [cmds.curve(degree=1, point=[(0, 1, 0), (0.2, 1.6, 0.1), (0.5, 2.1, 0.2)]),
          cmds.curve(degree=1, point=[(1, 0, 0), (1.6, -0.2, 0.1), (2.0, -0.7, 0.2)]),
          cmds.curve(degree=1, point=[(0, 0, 1), (-0.1, 0.4, 1.6), (-0.3, 0.2, 2.1)])]
groomlab.guides_from_curves(groom, curves, tube_radius=0.2, tube_tip_radius=0.1)
groomlab.generate_children(groom, "primary", name="children", count=4, twist=0.3, seed=5)
groomlab.lock_guides(groom, groomlab.guide_ids(groom, "children")[:2])

texture = os.path.relpath(os.path.join(OUT, "..", "..", "textures", "density.pgm"), OUT)  # tests/data/textures
texture = texture.replace("\\", "/")
nodes = [
    {"name": "scalp", "type": "Scalp"},
    {"name": "guides", "type": "Guides"},
    {"name": "primaries", "type": "Guides", "params": {"level": 0}},
    {"name": "scatter", "type": "Scatter",
     "params": {"density": groomlab.texture_map(texture, component="luminance", max=400.0)}},
    {"name": "resample", "type": "Resample", "params": {"segments": 9}},  # the strands' segments
    {"name": "interpolate", "type": "Interpolate"},
    {"name": "clump", "type": "Clump", "params": {"count": 25, "strength": groomlab.color_set_map("colorSet1", max=0.9),
                                                  "profile": [[0.0, 0.0], [0.6, 1.0], [1.0, 0.8]]}},
    {"name": "clump2", "type": "Clump", "params": {"strength": 0.3, "centersFrom": "inputCurves"}},
    {"name": "noise", "type": "Noise", "params": {"amplitude": 0.03, "profile": [[0.0, 0.0], [1.0, 1.0]]}},
    {"name": "curl", "type": "Curl", "params": {"radius": 0.02}},
    {"name": "frizz", "type": "Noise", "params": {"fraction": 0.2, "frequency": 8.0, "octaves": 1}},
    {"name": "attr", "type": "Attribute", "params": {"name": "len", "value": {"expression": "rand", "min": 0.6, "max": 1.0}}},
    {"name": "length", "type": "Length", "params": {"keep": groomlab.attribute_map("len")}},
    {"name": "comb", "type": "Bend", "params": {"angle": 20.0, "along": "direction", "direction": [0.0, -1.0, 0.2]}},
    {"name": "collide", "type": "Collide", "params": {"offset": 0.01}},
    {"name": "more", "type": "Scatter", "params": {"density": 20.0, "seed": 3}},
    {"name": "merge", "type": "Merge"},
    {"name": "switch", "type": "Switch", "params": {"index": 0}},
    {"name": "width", "type": "Width", "params": {"width": 0.01, "profile": [[0.0, 1.0], [1.0, 0.2]]}},
    {"name": "output", "type": "Output"},
]
links = [("scalp", "scatter", "scalp"), ("scatter", "interpolate", "strands"), ("guides", "interpolate", "guides"),
         ("scalp", "interpolate", "scalp"), ("interpolate", "clump", "strands"), ("clump", "clump2", "strands"), ("primaries", "clump2", "centers"),
         ("clump2", "noise", "strands"), ("noise", "curl", "strands"), ("curl", "frizz", "strands"),
         ("frizz", "attr", "strands"), ("attr", "length", "strands"), ("length", "comb", "strands"),
         ("comb", "collide", "strands"), ("scalp", "collide", "scalp"), ("scalp", "more", "scalp"),
         ("collide", "merge", "a"), ("more", "merge", "b"), ("merge", "switch", "a"),
         ("switch", "width", "strands"), ("width", "output", "strands")]
graph = {"version": 1, "nodes": nodes, "output": "output",
         "connections": [{"from": a, "to": b, "input": i} for a, b, i in links]}
groomlab.set_graph(groom, graph)

with open(os.path.join(OUT, "graph.json"), "w") as f:
    f.write(cmds.getAttr(groom + ".graph"))
with open(os.path.join(OUT, "guides.json"), "w") as f:
    f.write(cmds.getAttr(groom + ".guideData"))
path = os.path.join(OUT, "groom.groom")
groomlab.export_groom(groom, path)
with open(path) as f:
    data = json.load(f)
data["source"] = "compat fixture"
data.pop("searchPaths", None)  # machine-specific; the fixture resolves relative to its folder
with open(path, "w") as f:
    json.dump(data, f)
print("wrote fixtures to", OUT, "; errors:", groomlab.errors(groom))
maya.standalone.uninitialize()
