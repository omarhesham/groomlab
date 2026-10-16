# SPDX-License-Identifier: Apache-2.0
"""Writes the synthetic Yeti test scenes in this folder (plain Python, no Maya, no Yeti).

    python tests/data/yeti/make_scenes.py

yeti_scene.ma is written by hand in Yeti's format (importers/yeti_format.py describes it): a
sphere head, two grooms (one corrected by a corrective groom), guide curves, and two pgYetiMaya
graphs authored here node by node:

    hairYeti   Scatter (density from two channels of textures/density.png, through an
               expression) -> Grow -> Comb -> Attribute -> Clumping (clumps from a Scatter) ->
               Clumping (clumps at the groom's strands, through a Convert) -> Scraggle (a painted
               attribute) -> Displace (a texture that is missing) -> Collide (no equivalent) ->
               Width (a curve along the fibre) -> Shader; plus an Instance branch the output
               never reads
    fuzzYeti   Switch -> Merge of two chains: Scatter -> Guide (guide curves) -> Length, and
               Scatter -> Grow -> Comb -> Cut -> Direction

The image search path is ${GROOMLAB_YETI_TEST_ROOT}/textures (the test gives it as a root) and a
second folder under ${GROOMLAB_YETI_UNSET}, which nothing sets. Nothing here comes from a real
production scene: every value is made up for the tests.
"""

import math
import os
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "..", "hosts", "maya", "scripts", "groomlab", "importers"))

import yeti_format as yf  # noqa: E402

RADIUS = 5.0


def png(path, width, height, pixel):
    """An 8-bit RGB PNG; pixel(x, y) -> (r, g, b) in 0-255, y = 0 the top row."""
    rows = b"".join(b"\x00" + b"".join(struct.pack("BBB", *pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

    data = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + \
        chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(data)


def head_point(theta, phi, r=RADIUS):
    return (r * math.sin(theta) * math.cos(phi), r * math.cos(theta), r * math.sin(theta) * math.sin(phi))


def groom_strands(rings=5, per_ring=10, points=7, step=0.45, droop=0.06, lift=0.0):
    """Strands on the top of the head, out along the normal and drooping."""
    strands, frizz, clump = [], [], []
    for i in range(rings):
        theta = math.radians(8 + i * 14)
        for j in range(per_ring + i * 2):
            phi = 2 * math.pi * j / (per_ring + i * 2) + 0.2 * i
            root = head_point(theta, phi)
            n = [c / RADIUS for c in root]
            s = []
            for k in range(points):
                s.append((root[0] + n[0] * step * k, root[1] + n[1] * step * k - droop * k * k + lift * k,
                          root[2] + n[2] * step * k))
            strands.append(s)
            frizz.append(round(0.5 + 0.5 * math.cos(phi), 3))
            clump.append(round(0.4 + 0.6 * (i / float(rings - 1)), 3))
    return strands, {"frizzAmount": frizz, "clumpWeight": clump}


class GraphWriter(object):
    """A Yeti graph authored node by node: node(type, name, inputs, params) -> its output id."""

    def __init__(self):
        self.nodes = []
        self.count = 0

    def _id(self, kind):
        self.count += 1
        return "%s-%04d-0000-0000-%012d" % (kind, self.count, self.count)

    def node(self, type_, name, inputs=(), params=None, attach=None, config=None):
        out = self._id("o")
        self.nodes.append((self._id("n"), name, type_, list(inputs), out, dict(params or {}), dict(attach or {}),
                           config))
        return out

    def xml(self, root_output):
        root_id = next(n[0] for n in self.nodes if n[4] == root_output)
        lines = ['<yeti-graph version="2">', "\t<items>"]
        for nid, name, type_, inputs, out, params, attach, config in self.nodes:
            lines.append('\t\t<node id="%s" name="%s" type="%s">' % (nid, name, type_))
            if config:
                lines.append("\t\t\t<config>%s</config>" % config)
            if inputs:
                lines.append("\t\t\t<inputs>")
                for i, src in enumerate(inputs):
                    lines.append('\t\t\t\t<ref id="%s-in%d" value="%s" />' % (nid, i, src or "-"))
                lines.append("\t\t\t</inputs>")
            else:
                lines.append("\t\t\t<inputs />")
            lines.append('\t\t\t<outputs>\n\t\t\t\t<ref id="%s" />\n\t\t\t</outputs>' % out)
            for k, v in params.items():
                a = ' attachment="%s"' % attach[k] if k in attach else ""
                text = str(v).replace("&", "&amp;").replace('"', "&quot;").replace("<", "&lt;").replace("\n", "&#10;")
                lines.append('\t\t\t<param name="%s" id="%s-%s" value="%s"%s />' % (k, nid, k, text, a))
            lines.append('\t\t\t<metadata name="gui:pos" value="[0.00, 0.00, 0.00]" />')
            lines.append("\t\t</node>")
        lines.append("\t</items>")
        lines.append("\t<globals>\n\t\t<metadata name=\"root\" value=\"%s\" />\n\t</globals>" % root_id)
        lines.append("</yeti-graph>")
        return "\n".join(lines) + "\n"


def curve_config(keys):
    return "22 serialization::archive 17 " + " ".join("%.17e %.17e %d" % k for k in keys)


def hair_graph():
    g = GraphWriter()
    geo = g.node("import", "head", params={"geometry": "headShape", "type": 0, "ignore": "false"})
    groom = g.node("import", "hairGroom", params={"geometry": "hairGroomShape", "type": 1, "ignore": "false"})
    tex_a = g.node("texture", "densityTex", params={"file_name": "density.png", "channels": 1, "uCoord": "$s",
                                                   "vCoord": "1-$t", "frame": "$F"})
    tex_b = g.node("texture", "maskTex", params={"file_name": "density.png", "channels": 2, "uCoord": "$s",
                                                "vCoord": "1-$t", "frame": "$F"})
    dens = g.node("expression", "densityMask", params={"expression": "$f0 * clamp($f1, 0, 1)\n", "float0": 0,
                                                       "float1": 0, "float2": 0, "float3": 0},
                  attach={"float0": tex_a, "float1": tex_b})
    scatter = g.node("scatter", "scatterMain", [geo, None], {"densityMultiplier": 6, "density": 1, "seed": 7,
                                                             "baseDensity": 1, "tipDensity": 1, "faceSet": ""},
                     attach={"density": dens})
    grow = g.node("grow", "growMain", [scatter, groom], {"direction": "$N", "lengthMin": 0.8, "lengthMax": 1,
                                                         "lengthMultiplier": 1, "segmentCount": 12})
    comb = g.node("comb", "combMain", [grow, groom], {"weight": 1, "spread": 0})
    attr = g.node("attribute", "clumpAttr", [comb, groom], {"attribute": "clumpWeight", "default": "[1.00, 1.00, 1.00]",
                                                           "groomAttribute": "clumpWeight", "sampleGroom": 1})
    attr2 = g.node("attribute", "frizzAttr", [attr, groom], {"attribute": "frizzAmount",
                                                            "default": "[0.50, 0.50, 0.50]",
                                                            "groomAttribute": "frizzAmount"})
    # The clump curves: a sparser scatter grown from the groom.
    c_scatter = g.node("scatter", "scatterClumps", [geo, None], {"densityMultiplier": 0.25, "density": 1, "seed": 3},
                       attach={"density": dens})
    c_grow = g.node("grow", "growClumps", [c_scatter, groom], {"lengthMin": 1, "lengthMax": 1, "segmentCount": 12})
    c_comb = g.node("comb", "combClumps", [c_grow, groom], {"weight": 1})
    c_scraggle = g.node("scraggle", "scraggleClumps", [c_comb], {"scraggle": 0.3, "percent": 1})
    weight_curve = g.node("curve", "clumpAlong", params={"parameter": "$strandu"},
                          config=curve_config([(0.0, 0.2, 1), (1.0, 1.0, 1)]))
    weight = g.node("expression", "clumpWeightAlong", params={"expression": "(clumpWeight*$f0)", "float0": 0},
                    attach={"float0": weight_curve})
    clump1 = g.node("clumping", "clumpSmall", [attr2, c_scraggle],
                    {"weight": "clumpWeight", "weightRandom": 0, "clumpWeightRandom": 0.2, "attractionBase": 0.2,
                     "attractionTip": 1, "attractionBias": -0.5, "twist": 0.5, "twistRandom": 0, "flyAway": 0.1,
                     "lengthHandling": 0}, attach={"weight": weight})
    convert = g.node("convert", "groomCurves", [groom, geo], {"conversion": 0})
    clump2 = g.node("clumping", "clumpGuides", [clump1, convert],
                    {"weight": 0.5, "attractionBase": 1, "attractionTip": 1, "attractionBias": 0})
    scraggle = g.node("scraggle", "frizz", [clump2], {"scraggle": "frizzAmount", "scraggleMultiplier": 1.5,
                                                      "scraggleRandom": 0.3, "rootScraggle": 0, "tipScraggle": 1,
                                                      "scraggleFrequency": 1, "percent": 0.6, "space": 0})
    missing = g.node("texture", "bumpTex", params={"file_name": "${GROOMLAB_YETI_UNSET}/bump.png", "channels": 0})
    displace = g.node("displace", "bump", [scraggle], {"displacement": 0.2, "displacementMultiplier": 1},
                      attach={"displacement": missing})
    collide = g.node("collide", "collideHead", [displace, geo], {"distance": 0.1})
    width_curve = g.node("curve", "widthAlong", params={"parameter": "$strandu"},
                         config=curve_config([(0.0, 1.0, 2), (0.7, 0.8, 2), (1.0, 0.3, 2)]))
    width = g.node("width", "widthMain", [collide, None], {"baseWidth": 0.02, "tipWidth": 0.005, "baseParameter": 0,
                                                           "tipParameter": 1, "widthRandom": 0.1,
                                                           "widthMultiplier": 1},
                   attach={"widthMultiplier": width_curve})
    shader = g.node("shader", "shaderMain", [width], {"doColor": "true", "diffuseColor": "[0.30, 0.20, 0.10]"})
    # A branch the output never reads.
    pebble = g.node("transform", "pebbleXform", [geo], {"scale": "[1, 1, 1]"})
    g.node("instance", "pebbles", [scraggle, pebble], {"instanceTo": 1})
    return g.xml(shader)


def fuzz_graph():
    g = GraphWriter()
    geo = g.node("import", "head", params={"geometry": "headShape", "type": 0})
    curves = g.node("import", "guideCurves", params={"geometry": "guideCurves", "type": 2})
    groom = g.node("import", "fuzzGroom", params={"geometry": "fuzzGroomShape", "type": 1})
    s1 = g.node("scatter", "scatterA", [geo, None], {"densityMultiplier": 3, "density": 1, "seed": 11})
    guide = g.node("guide", "guideA", [s1, curves], {"maxNumberOfGuideInfluences": 4, "weight": 1})
    length = g.node("length", "lengthA", [guide], {"length": 0.6})
    s2 = g.node("scatter", "scatterB", [geo, None], {"densityMultiplier": 2, "density": 1, "seed": 12})
    grow = g.node("grow", "growB", [s2, groom], {"lengthMin": 1, "lengthMax": 1, "lengthMultiplier": 1.2})
    comb = g.node("comb", "combB", [grow, groom], {"weight": 0.5})
    cut = g.node("cut", "cutB", [comb], {"amount": 0.25})
    direction = g.node("direction", "directionB", [cut], {"weight": 1})
    merge = g.node("merge", "mergeAB", [length, direction], {"numInputs": 2})
    other = g.node("scraggle", "unusedScraggle", [s2], {"scraggle": 1})
    switch = g.node("switch", "pick", [merge, other], {"index": 0})
    shader = g.node("shader", "shaderFuzz", [switch], {"doColor": "false"})
    return g.xml(shader)


def ma_string(text):
    return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def guide_curves():
    """Four NURBS curves (degree 1) rising from the head's top."""
    lines = ['createNode transform -n "guideCurves";']
    for i in range(4):
        phi = i * math.pi / 2
        root = head_point(math.radians(30), phi)
        n = [c / RADIUS for c in root]
        pts = [(root[0] + n[0] * 0.7 * k, root[1] + n[1] * 0.7 * k, root[2] + n[2] * 0.7 * k) for k in range(4)]
        lines.append('createNode transform -n "guide%d" -p "guideCurves";' % i)
        lines.append('createNode nurbsCurve -n "guide%dShape" -p "guide%d";' % (i, i))
        cv = " ".join("%g %g %g" % p for p in pts)
        lines.append('\tsetAttr ".cc" -type "nurbsCurve" 1 3 0 no 3 4 0 1 2 3 4 %s;' % cv)
    return "\n".join(lines)


def scene():
    hair_strands, hair_attrs = groom_strands()
    corrected = [[(x, y, z + 0.04 * k) for k, (x, y, z) in enumerate(s)] for s in hair_strands]
    fuzz_strands, fuzz_attrs = groom_strands(rings=3, per_ring=6, points=4, step=0.25, droop=0.0)
    out = ['//Maya ASCII 2022 scene', '//Name: yeti_scene.ma', '//Synthetic: written by make_scenes.py',
           'requires maya "2022";',
           'requires -nodeType "pgYetiMaya" -nodeType "pgYetiGroom" -dataType "pgYetiMayaData"',
           '\t\t -dataType "pgYetiMayaStrandData" "pgYetiMaya" "4.0.0";',
           'currentUnit -l centimeter -a degree -t film;',
           'createNode transform -n "head";',
           'createNode mesh -n "headShape" -p "head";',
           '\tsetAttr -k off ".v";',
           'createNode polySphere -n "headSphere";',
           '\tsetAttr ".r" %g;' % RADIUS,
           '\tsetAttr ".sa" 24;',
           '\tsetAttr ".sh" 16;',
           guide_curves()]
    isp = "${GROOMLAB_YETI_TEST_ROOT}/textures;${GROOMLAB_YETI_UNSET}/more"
    for name, xml in (("hairYeti", hair_graph()), ("fuzzYeti", fuzz_graph())):
        out += ['createNode transform -n "%s";' % name,
                'createNode pgYetiMaya -n "%sShape" -p "%s";' % (name, name),
                '\tsetAttr -k off ".v";',
                '\tsetAttr ".pds" -type "pgYetiMayaData" %s;' % ma_string(yf.encode_graph(xml)),
                '\tsetAttr ".isp" -type "string" %s;' % ma_string(isp)]
    grooms = (("hairGroom", hair_strands, hair_attrs, None), ("hairGroomCorrective", corrected, hair_attrs,
                                                              "hairGroomShape"),
              ("fuzzGroom", fuzz_strands, fuzz_attrs, None))
    for name, strands, attrs, target in grooms:
        tokens = yf.format_strands(name + "Shape", strands, attrs, {"system_type": 0})
        lines = []
        for k in range(0, len(tokens), 18):
            lines.append(" ".join(tokens[k:k + 18]))
        out += ['createNode transform -n "%s";' % name,
                '\tsetAttr ".v" no;',
                'createNode pgYetiGroom -n "%sShape" -p "%s";' % (name, name),
                '\tsetAttr -k off ".v";',
                '\tsetAttr ".pat" -type "string" "%s";' % sorted(attrs)[0],
                '\tsetAttr ".pds" -type "pgYetiMayaStrandData" %s;' % "\n\t\t".join(lines)]
        if target:
            out += ['\tsetAttr ".cgb" yes;', '\tsetAttr ".cgt" -type "string" "%s";' % target]
    out += ['connectAttr "headSphere.out" "headShape.i";',
            'connectAttr "headShape.w" "hairYetiShape.ing" -na;',
            'connectAttr "hairGroomShape.od" "hairYetiShape.icd" -na;',
            'connectAttr "hairGroomCorrectiveShape.od" "hairYetiShape.icd" -na;',
            'connectAttr "headShape.w" "hairGroomShape.ing";',
            'connectAttr "headShape.w" "hairGroomCorrectiveShape.ing";',
            'connectAttr "headShape.w" "fuzzYetiShape.ing" -na;',
            'connectAttr "fuzzGroomShape.od" "fuzzYetiShape.icd" -na;',
            'connectAttr "headShape.w" "fuzzGroomShape.ing";',
            'connectAttr ":time1.o" "hairYetiShape.time";',
            'connectAttr ":time1.o" "fuzzYetiShape.time";',
            '// End of yeti_scene.ma']
    return "\n".join(out) + "\n"


def main():
    textures = os.path.join(HERE, "textures")
    if not os.path.isdir(textures):
        os.makedirs(textures)
    # R: 1 at the top of the image (V = 1) to 0 at the bottom; G: 1 on the left half, 0.5 on the right.
    png(os.path.join(textures, "density.png"), 32, 32,
        lambda x, y: (int(round(255 * (1.0 - y / 31.0))), 255 if x < 16 else 128, 0))
    with open(os.path.join(HERE, "yeti_scene.ma"), "w", newline="\n") as f:
        f.write(scene())
    print("wrote", os.path.join(HERE, "yeti_scene.ma"))


if __name__ == "__main__":
    main()
