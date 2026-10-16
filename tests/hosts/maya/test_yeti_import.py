# SPDX-License-Identifier: Apache-2.0
"""Importing Yeti grooms without Yeti (groomlab.import_yeti), on a synthetic scene written by hand
in Yeti's format (tests/data/yeti/make_scenes.py): the format reader, the graph's nodes as
GroomLab nodes, guides and guide layers from the groom, textures found with the image search path
and the Roots and copied, missing files reported, (the Import window's layout: maya.ui_scaling).

Usage: mayapy test_yeti_import.py <GroomLab.mll> <maya/scripts dir> <tests/data/yeti dir> <work dir>
"""

import json
import math
import os
import shutil
import sys
import unittest

PLUGIN_PATH = os.path.abspath(sys.argv[1])
SCRIPTS_DIR = os.path.abspath(sys.argv[2])
DATA_DIR = os.path.abspath(sys.argv[3])
WORK_DIR = os.path.abspath(sys.argv[4])
sys.argv = sys.argv[:1]
sys.path.insert(0, SCRIPTS_DIR)

import maya.standalone  # noqa: E402

maya.standalone.initialize(name="python")

from maya import cmds  # noqa: E402

import groomlab  # noqa: E402
from groomlab.importers import yeti as gy  # noqa: E402
from groomlab.importers import yeti_format as yf  # noqa: E402

ROOTS = "GROOMLAB_YETI_TEST_ROOT=%s" % DATA_DIR
STATE = {}


def setUpModule():
    cmds.loadPlugin(PLUGIN_PATH)
    shutil.rmtree(WORK_DIR, ignore_errors=True)
    os.makedirs(WORK_DIR)
    os.environ.pop("GROOMLAB_YETI_UNSET", None)
    os.environ.pop("GROOMLAB_YETI_TEST_ROOT", None)


def open_scene(name="scene"):
    folder = os.path.join(WORK_DIR, name)
    if not os.path.isdir(folder):
        os.makedirs(folder)
    path = os.path.join(folder, "yeti_scene.ma")
    shutil.copy(os.path.join(DATA_DIR, "yeti_scene.ma"), path)
    cmds.file(path, open=True, force=True)
    return path


def imported():
    """The synthetic scene imported once (shared by the tests that only read the result)."""
    if "report" not in STATE or not cmds.objExists(STATE["report"].grooms[0]["groom"] or "-"):
        open_scene("shared")
        STATE["report"] = groomlab.import_yeti(roots=ROOTS, verbose=False)
        STATE["by_name"] = dict((g["name"], g) for g in STATE["report"].grooms)
        records, _ = gy.read_scene_records()
        strands = yf.parse_strands(records["hairGroomCorrectiveShape"].attr_tokens("pds")).strands
        lengths = sorted(sum(math.sqrt(sum((a - b) ** 2 for a, b in zip(p, q))) for p, q in zip(s, s[1:]))
                         for s in strands)
        STATE["guide_length"] = lengths[len(lengths) // 2]
    return STATE["report"], STATE["by_name"]


def _median_length(groom):
    from groomlab.importers.foreign import median_strand_length
    return median_strand_length(groom)


def lines(entry, section):
    return ["%s: %s" % (i["what"], i["detail"]) for i in entry[section]]


def node(groom, type_, index=0):
    return [n for n in groomlab.get_graph(groom)["nodes"] if n["type"] == type_][index]


def named(groom, name):
    return next(n for n in groomlab.get_graph(groom)["nodes"] if n["name"] == name)


def chain_types(groom, end=None):
    """The node types from the Scatter to `end` (default: the Output), along the strands inputs."""
    graph = groomlab.get_graph(groom)
    types = dict((n["name"], n["type"]) for n in graph["nodes"])
    feeds = dict(((c["to"], c["input"]), c["from"]) for c in graph["connections"])
    out, name = [], end or graph["output"]
    while name:
        out.append(types[name])
        name = feeds.get((name, "strands"))
    return list(reversed(out))


class Format(unittest.TestCase):
    """yeti_format alone: no Maya needed."""

    def test_strands_round_trip(self):
        strands = [[(0, 0, 0), (0, 1, 0), (0, 2, 0.5)], [(1, 0, 0), (1, 1, 0)]]
        tokens = yf.format_strands("g", strands, {"curly": [0.25, 1.0]}, {"system_type": 0})
        s = yf.parse_strands(tokens)
        self.assertEqual(s.count, 2)
        self.assertEqual(s.strands[0][2], (0.0, 2.0, 0.5))
        self.assertEqual(s.attributes["curly"], [0.25, 1.0])
        self.assertEqual(s.painted(), ["curly"])
        self.assertEqual(s.constants["system_type"], 0)
        self.assertNotIn("N", s.painted())

    def test_strands_errors(self):
        with self.assertRaises(yf.FormatError):
            yf.parse_strands("XYZ g 1 P 2 1 1 0 0 0")
        with self.assertRaises(yf.FormatError):  # numVertices adds up to more points than P has
            yf.parse_strands("PGYS10 g 2 P 2 1 1 0 0 0 numVertices 0 0 1 2")
        with self.assertRaises(yf.FormatError):  # a type the reader does not know
            yf.parse_strands("PGYS10 g 1 P 7 1 1 0 0 0")

    def test_graph(self):
        xml = ('<yeti-graph version="2"><items><group label="g"><node id="a" name="tex" type="texture"><inputs />'
               '<outputs><ref id="oa" /></outputs><param name="file_name" value="d.png" /></node></group>'
               '<node id="b" name="w" type="width"><inputs><ref id="i" value="-" /></inputs><outputs><ref id="ob" />'
               '</outputs><param name="widthMultiplier" value="1" attachment="oa" /></node>'
               '<node id="c" name="cv" type="curve"><config>22 serialization::archive 17 0 0.5 1 1 1 2</config>'
               '<inputs /><outputs><ref id="oc" /></outputs></node></items>'
               '<globals><metadata name="root" value="b" /></globals></yeti-graph>')
        g = yf.parse_graph(yf.decode_graph(yf.encode_graph(xml)))
        self.assertEqual(g.root_node().name, "w")
        w = g.root_node()
        self.assertEqual(w.inputs, [None])
        self.assertEqual(g.node_of_output(w.params["widthMultiplier"].attachment).name, "tex")
        self.assertEqual(yf.curve_keys(g.nodes["c"].config), [(0.0, 0.5, 1), (1.0, 1.0, 2)])
        with self.assertRaises(yf.FormatError):
            yf.parse_graph("<not-yeti />")

    def test_ma_both_ways(self):
        """The .ma as saved (unquoted strand tokens) and as Maya writes an unknown node back (quoted)."""
        text = ('createNode pgYetiGroom -n "gShape" -p "g";\n\tsetAttr ".pat" -type "string" "a";\n'
                '\tsetAttr ".pds" -type "pgYetiMayaStrandData" PGYS10 gShape 2 P 2 1 2 0 0 0 0 1 0 '
                'numVertices 0 0 1 2;\n'
                'createNode pgYetiGroom -n "hShape";\n\tsetAttr ".pds" -type "pgYetiMayaStrandData"   "PGYS10" '
                '"hShape" 2 "P" 2 1 2 0 0 0 0 1 0 "numVertices" 0 0 1 2;\n'
                'createNode transform -n "x";\n\tsetAttr ".t" 1 2 3;\n'
                'connectAttr "gShape.od" "yShape.icd" -na;\n')
        nodes, connections = yf.read_ma(text)
        self.assertEqual(sorted(nodes), ["gShape", "hShape"])
        self.assertEqual(nodes["gShape"].attr_text("pat"), "a")
        for name in ("gShape", "hShape"):
            self.assertEqual(yf.parse_strands(nodes[name].attr_tokens("pds")).count, 1)
        self.assertEqual(connections, [("gShape.od", "yShape.icd")])

    def test_vectors(self):
        self.assertEqual(yf.to_vector("[0.30, 0.20, 0.10]"), (0.3, 0.2, 0.1))
        self.assertEqual(yf.to_vector(".1 * [1.00, 2.00, 3.00]"), (0.1, 0.2, 0.30000000000000004))
        self.assertIsNone(yf.to_vector("$N"))

    def test_test_scene_is_current(self):
        """yeti_scene.ma is what make_scenes.py writes (edit the script, not the scene)."""
        sys.path.insert(0, DATA_DIR)
        import make_scenes
        with open(os.path.join(DATA_DIR, "yeti_scene.ma")) as f:
            self.assertEqual(f.read(), make_scenes.scene())


class Import(unittest.TestCase):
    def test_lists_without_yeti(self):
        open_scene("listing")
        self.assertFalse(gy.yeti_available())
        found = groomlab.yeti_nodes()
        self.assertEqual(sorted(d["name"] for d in found), ["fuzzYeti", "hairYeti"])
        hair = next(d for d in found if d["name"] == "hairYeti")
        self.assertEqual(hair["scalp"].split("|")[-1], "headShape")
        self.assertEqual(sorted(g.split("|")[-1] for g in hair["grooms"]), ["hairGroomCorrectiveShape",
                                                                            "hairGroomShape"])
        records, how = gy.read_scene_records()
        self.assertIn("hairYetiShape", records)
        self.assertIn("exported", how)

    def test_grooms_made(self):
        report, by_name = imported()
        self.assertEqual(sorted(by_name), ["fuzzYeti_1", "fuzzYeti_2", "hairYeti"])
        for g in report.grooms:
            self.assertTrue(g["groom"], g["name"])
            from groomlab.groom import errors
            self.assertEqual(errors(g["groom"]), [], g["name"])

    def test_hair_graph(self):
        _, by_name = imported()
        groom = by_name["hairYeti"]["groom"]
        self.assertEqual(chain_types(groom), ["Scatter", "Interpolate", "Transform", "Clump", "Twist", "Clump", "Noise",
                                              "Displace", "HairColor", "Width", "Resample", "Output"])
        scatter = node(groom, "Scatter")["params"]
        self.assertEqual(scatter["seed"], 7)
        self.assertEqual(scatter["density"]["max"], 6.0)  # Yeti's density: Scatter samples up to it
        self.assertIn('map("density", "r")', scatter["density"]["expression"])
        self.assertIn('map("density", "g")', scatter["density"]["expression"])
        length = node(groom, "Transform")["params"]
        self.assertAlmostEqual(length["scale"], 0.9)
        self.assertAlmostEqual(length["scaleRandom"], 0.2 / 1.8)  # 0.9 +- 0.1
        clump = node(groom, "Clump", 0)
        self.assertEqual(clump["params"]["centersFrom"], "inputCurves")
        self.assertAlmostEqual(clump["params"]["strengthRandom"], 0.2)
        # Its clump curves: a branch of their own (Scatter, Interpolate, the curves' own Scraggle).
        conns = groomlab.get_graph(groom)["connections"]
        centers = next(c["from"] for c in conns if c["to"] == clump["name"] and c["input"] == "centers")
        self.assertEqual(chain_types(groom, centers), ["Scatter", "Interpolate", "Noise"])
        branch_scatter = named(groom, "clumpScatter")["params"]
        self.assertEqual(branch_scatter["seed"], 3)
        self.assertAlmostEqual(branch_scatter["density"]["max"], 0.25)
        self.assertIn("strength", clump["paramInputs"])  # the clumpWeight guide layer, through a Map node
        keys = clump["params"]["profile"]["keys"]
        self.assertAlmostEqual(keys[0][1], 0.2 * 0.2, places=4)  # attraction base x the weight's curve at the root
        self.assertAlmostEqual(keys[-1][1], 1.0, places=4)
        self.assertIn("rand(", clump["params"]["affect"]["expression"])  # flyAway
        guides_clump = node(groom, "Clump", 1)
        self.assertEqual(guides_clump["params"]["centersFrom"], "inputCurves")
        self.assertAlmostEqual(guides_clump["params"]["strength"], 0.5)
        self.assertIn({"from": "guides", "to": guides_clump["name"], "input": "centers"}, conns)
        noise = named(groom, "noise")["params"]
        self.assertAlmostEqual(noise["fraction"], 0.6)
        self.assertIn('map("frizzAmount")', noise["amplitude"]["expression"])
        self.assertGreater(noise["amplitude"]["max"], 0.0)
        width = node(groom, "Width")["params"]
        self.assertAlmostEqual(width["width"], 0.02)
        self.assertAlmostEqual(width["randomness"], 0.1)
        wkeys = width["profile"]["keys"]
        self.assertAlmostEqual(wkeys[0][1], 1.0, places=3)
        self.assertAlmostEqual(wkeys[-1][1], 0.25 * 0.3, places=3)  # tip/base width x the curve at the tip
        color = node(groom, "HairColor")["params"]
        self.assertEqual(color["mode"], "color")
        self.assertEqual([round(c, 3) for c in color["color"]], [0.3, 0.2, 0.1])
        self.assertEqual(node(groom, "Resample")["params"]["segments"], 12)

    def test_guides_and_layers(self):
        _, by_name = imported()
        groom = by_name["hairYeti"]["groom"]
        records, _ = gy.read_scene_records()
        corrective = yf.parse_strands(records["hairGroomCorrectiveShape"].attr_tokens("pds"))
        data = json.loads(cmds.getAttr(groom + ".guideData"))
        guides = data["guides"]
        self.assertEqual(len(guides), corrective.count)
        kinds = dict((m["name"], m["kind"]) for m in groomlab.maps(groom))
        self.assertEqual(kinds.get("clumpWeight"), "guides")
        self.assertEqual(kinds.get("frizzAmount"), "guides")
        values = sorted(g["attributes"]["frizzAmount"] for g in guides)
        self.assertEqual(values, sorted(round(v, 5) for v in corrective.attributes["frizzAmount"]))
        # The corrective groom's strands are the guides: its tips sit further along z.
        ids = groomlab.list_guides(groom) if hasattr(groomlab, "list_guides") else None
        first = cmds.groomLabGuides(groom, guidePoints=str(guides[0]["id"]))
        tip = first[-3:]
        best = min(corrective.strands, key=lambda s: sum((a - b) ** 2 for a, b in zip(s[0], first[:3])))
        self.assertLess(math.sqrt(sum((a - b) ** 2 for a, b in zip(best[-1], tip))), 0.05)
        del ids

    def test_strand_count(self):
        """Yeti's density x the scalp's area x the density map's mean (R: 0.5 on average, G: 0.75)."""
        _, by_name = imported()
        groom = by_name["hairYeti"]["groom"]
        from groomlab.review import groom_stats
        cmds.setAttr(groom + ".previewDensity", 1.0)
        count = groom_stats(groom)["strands"]
        area = cmds.polyEvaluate("headShape", worldArea=True)
        expected = 6 * area * 0.5 * 0.75
        self.assertLess(abs(count - expected) / expected, 0.15, (count, expected))
        # The clumps (on their clump curves) keep the strands' lengths: measured against the strands as
        # Interpolate and Grow's scale make them. Without the Displace (its texture is missing, so it
        # reads white and pushes the strands out) and the Twist (a few wide clumps on this small head:
        # strands winding about them grow longer).
        graph = groomlab.get_graph(groom)

        def length_with(off_types):
            off = json.loads(json.dumps(graph))
            for n in off["nodes"]:
                if n["type"] in off_types:
                    n.setdefault("params", {})["affect"] = 0.0
            groomlab.set_graph(groom, off)
            return _median_length(groom)

        try:
            grown = length_with(("Clump", "Twist", "Noise", "Displace"))
            clumped = length_with(("Twist", "Displace"))
        finally:
            groomlab.set_graph(groom, graph)
        self.assertLess(abs(clumped / grown - 1.0), 0.12, (clumped, grown))
        self.assertLess(abs(grown / (0.9 * STATE["guide_length"]) - 1.0), 0.3, (grown, STATE["guide_length"]))

    def test_textures_copied_and_missing(self):
        report, by_name = imported()
        entry = by_name["hairYeti"]
        self.assertEqual(len(entry["files"]), 1)  # one file read by two channels: copied once
        copy = entry["files"][0]["copy"]
        self.assertTrue(os.path.isfile(copy), copy)
        self.assertIn("/masks/", copy.replace("\\", "/"))
        self.assertNotIn(DATA_DIR.replace("\\", "/").lower(), copy.replace("\\", "/").lower())
        stored = [m["texture"] for m in groomlab.maps(entry["groom"]) if m["kind"] == "texture"]
        self.assertTrue(stored and all(DATA_DIR.replace("\\", "/").lower() not in s.lower() for s in stored))
        self.assertEqual(len(entry["missing"]), 1)
        self.assertEqual(entry["missing"][0]["unset"], ["GROOMLAB_YETI_UNSET"])
        self.assertIn("GROOMLAB_YETI_UNSET", report.unset_variables)
        # The missing texture drives Displace's distance: it reads white, Yeti's multiplier 1.
        self.assertAlmostEqual(node(entry["groom"], "Displace")["params"]["distance"], 1.0)

    def test_report(self):
        _, by_name = imported()
        hair = by_name["hairYeti"]
        skipped = "\n".join(lines(hair, "skipped"))
        self.assertIn("collideHead (collide): no GroomLab equivalent", skipped)
        self.assertIn("nodes not on the way to the output: 1 instance, 1 transform", skipped)
        self.assertNotIn("scraggle", skipped)  # the clump curves' own Scraggle is in their branch
        approximated = "\n".join(lines(hair, "approximated"))
        self.assertIn("corrective groom hairGroomCorrectiveShape", approximated)
        self.assertIn("strands: GroomLab grows", approximated)
        mapped = "\n".join(lines(hair, "mapped"))
        self.assertIn("groom attribute frizzAmount", mapped)
        fuzz = by_name["fuzzYeti_1"]
        self.assertIn("pick (switch)", "\n".join(lines(fuzz, "approximated")))

    def test_merge_chains(self):
        _, by_name = imported()
        a, b = by_name["fuzzYeti_1"]["groom"], by_name["fuzzYeti_2"]["groom"]
        self.assertEqual(len(json.loads(cmds.getAttr(a + ".guideData"))["guides"]), 4)  # the guide curves
        self.assertEqual(node(a, "Interpolate")["params"]["neighbors"], 4)
        self.assertAlmostEqual(node(a, "Transform")["params"]["scale"], 0.6)
        self.assertEqual(node(a, "Scatter")["params"]["seed"], 11)
        self.assertAlmostEqual(node(b, "Transform")["params"]["scale"], 1.2)
        self.assertAlmostEqual(node(b, "Length")["params"]["keep"], 0.75)
        self.assertEqual(len(json.loads(cmds.getAttr(b + ".guideData"))["guides"]), 24)
        self.assertIn("comb weight 0.5", "\n".join(lines(by_name["fuzzYeti_2"], "approximated")))

    def test_pick_nodes_and_max_guides(self):
        open_scene("pick")
        report = groomlab.import_yeti(nodes="hairYeti", roots=ROOTS, max_guides=20, verbose=False)
        self.assertEqual([g["name"] for g in report.grooms], ["hairYeti"])
        guides = json.loads(cmds.getAttr(report.grooms[0]["groom"] + ".guideData"))["guides"]
        self.assertLessEqual(len(guides), 20)
        self.assertGreater(len(guides), 8)
        self.assertTrue(all("frizzAmount" in g["attributes"] for g in guides))
        with self.assertRaises(ValueError):
            groomlab.import_yeti(nodes="nope", verbose=False)

    def test_without_roots(self):
        """No root for the image search path: the density texture is missing, the import goes on and
        names the variable; the density reads white."""
        open_scene("noroots")
        report = groomlab.import_yeti(nodes=["hairYeti"], verbose=False)
        entry = report.grooms[0]
        self.assertTrue(entry["groom"])
        self.assertIn("GROOMLAB_YETI_TEST_ROOT", report.unset_variables)
        from groomlab.groom import errors
        self.assertEqual(errors(entry["groom"]), [])
        self.assertEqual(node(entry["groom"], "Scatter")["params"]["density"]["expression"], "1 * 1")

    def test_expressions(self):
        open_scene("expressions")
        records, _ = gy.read_scene_records()
        d = next(x for x in groomlab.yeti_nodes() if x["name"] == "hairYeti")
        r = gy.YetiReader(d, records["hairYetiShape"], records, gy._base_resolver({}, []))
        r.groom = yf.parse_strands(records["hairGroomShape"].attr_tokens("pds"))
        text, maps, per_point = r.translate("mix(0.2, 1, $s) * clumpWeight + $strandu + noise($P * 2)", None)
        self.assertEqual(text, 'mix(0.2, 1, $u) * (map("clumpWeight")) + $t + noise($P * 2)')
        self.assertTrue(per_point)
        self.assertEqual([m.name for m in maps], ["clumpWeight"])
        self.assertIsNone(r.translate("vertexCount * 2", None))  # no GroomLab equivalent
        v = r.text_value("0.5 * clumpWeight", None)
        self.assertEqual((v.const, [f.kind for f in v.factors]), (0.5, ["layer"]))
        self.assertEqual(r.text_value("2.5", None).const, 2.5)
        self.assertEqual(gy.attraction_profile(0.0, 1.0, 0.0, samples=3), [(0.0, 0.0), (0.5, 0.5), (1.0, 1.0)])


if __name__ == "__main__":
    result = unittest.main(exit=False, verbosity=2).result
    sys.stdout.flush()
    sys.stderr.flush()
    maya.standalone.uninitialize()
    sys.exit(0 if result.wasSuccessful() else 1)
