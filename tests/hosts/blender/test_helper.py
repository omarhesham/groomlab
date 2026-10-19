# SPDX-License-Identifier: Apache-2.0
"""groomlab-helper (hosts/blender/helper, docs/BLENDER.md) driven as the Blender add-on drives it, with
no Blender: the protocol, arrays through shared memory, a preset groom on the test head evaluated to
the same bits as groomlab-cli evaluates the .groom it saved, guides from the host (flat and into a
guide hierarchy) and their incremental evaluation, params and presets, Blender's space (Z up,
meters) against the groom's (Y up, centimeters), every export with no key (GroomLab is free: an old
file's "license" ignored), and bad requests.

Environment (ctest blender.helper sets it): GROOMLAB_HELPER, GROOMLAB_HELPER_TESTHOOKS, GROOMLAB_CLI,
GROOMLAB_TEST_DATA (tests/data).
"""

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
import unittest

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from helper_client import HelperClient, HelperError  # noqa: E402

HELPER = os.environ.get("GROOMLAB_HELPER", "")
HELPER_TESTHOOKS = os.environ.get("GROOMLAB_HELPER_TESTHOOKS", "")
CLI = os.environ.get("GROOMLAB_CLI", "")
DATA = os.environ.get("GROOMLAB_TEST_DATA", os.path.join(HERE, "..", "..", "data"))

HEAD_OBJ = os.path.join(DATA, "app", "head_scalp.obj")
COMPAT_GROOM = os.path.join(DATA, "compat", "v1", "groom.groom")

M64 = (1 << 64) - 1
GOLDEN = 0x9E3779B97F4A7C15


# ---- The core's hash (random.h), as groomlab-cli hash prints it -----------------------------------

def hash64(x):
    x = (x + GOLDEN) & M64
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & M64
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & M64
    return x ^ (x >> 31)


def combine_all(h, values):
    for v in values:
        x = (h ^ ((v + GOLDEN + (h << 6) + (h >> 2)) & M64)) & M64
        x = (x + GOLDEN) & M64
        x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & M64
        x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & M64
        h = x ^ (x >> 31)
    return h


def strands_hash(ids, offsets, positions, widths, root_uvs):
    """groomlab-cli hash's frame hash of strands (ids, offsets, positions, widths, root UVs: bits)."""
    h = hash64(len(ids))
    h = combine_all(h, ids.astype(np.uint64).tolist())
    h = combine_all(h, offsets.astype(np.uint64).tolist())
    h = combine_all(h, np.ascontiguousarray(positions, dtype=np.float32).view(np.uint32).ravel().tolist())
    h = combine_all(h, np.ascontiguousarray(widths, dtype=np.float32).view(np.uint32).tolist())
    h = combine_all(h, np.ascontiguousarray(root_uvs, dtype=np.float32).view(np.uint32).ravel().tolist())
    return h


def file_hash(frame_hashes):
    return combine_all(hash64(len(frame_hashes)), frame_hashes)


def cli_hash(path):
    out = subprocess.run([CLI, "hash", path], check=True, capture_output=True, text=True).stdout
    return int(re.search(r"^all=([0-9a-f]+)", out, re.M).group(1), 16)


def cli_eval(groom, out, density=None, frame=1):
    args = [CLI, "eval", groom, "-o", out, "-f", str(frame)]
    if density is not None:
        args += ["--density", repr(density)]
    run = subprocess.run(args, capture_output=True, text=True)
    if run.returncode != 0:
        raise AssertionError("groomlab-cli eval failed: " + run.stdout + run.stderr)


# ---- The test head --------------------------------------------------------------------------------

def read_obj(path):
    """Points (Y up, cm), triangles and per-corner UVs of an OBJ of triangles."""
    points, uvs, faces, face_uvs = [], [], [], []
    with open(path) as f:
        for line in f:
            parts = line.split()
            if not parts:
                continue
            if parts[0] == "v":
                points.append([float(v) for v in parts[1:4]])
            elif parts[0] == "vt":
                uvs.append([float(v) for v in parts[1:3]])
            elif parts[0] == "f":
                corners = [c.split("/") for c in parts[1:]]
                faces.append([int(c[0]) - 1 for c in corners])
                face_uvs.append([uvs[int(c[1]) - 1] for c in corners])
    counts = np.array([len(f) for f in faces], dtype=np.int32)
    indices = np.array([i for f in faces for i in f], dtype=np.int32)
    corner_uvs = np.array([uv for f in face_uvs for uv in f], dtype=np.float32)
    return np.array(points, dtype=np.float32), counts, indices, corner_uvs


def to_blender(points_cm_yup):
    """Y up centimeters to Blender's Z up meters: (x, y, z) -> (x, -z, y) / 100."""
    p = np.asarray(points_cm_yup, dtype=np.float64)
    return (np.stack([p[:, 0], -p[:, 2], p[:, 1]], axis=1) / 100.0).astype(np.float32)


def to_groom(points_m_zup):
    p = np.asarray(points_m_zup, dtype=np.float64)
    return (np.stack([p[:, 0], p[:, 2], -p[:, 1]], axis=1) * 100.0).astype(np.float32)


HEAD = read_obj(HEAD_OBJ) if os.path.exists(HEAD_OBJ) else None


def env_for(**extra):
    env = dict(os.environ)
    env.pop("GROOMLAB_PRESET_PATH", None)
    env["GROOMLAB_USER_PRESETS"] = extra.pop("user_presets", os.path.join(tempfile.gettempdir(), "groomlab_no_user_presets"))
    env.pop("GROOMLAB_NODE_PATH", None)
    env["GROOMLAB_USER_NODES"] = extra.pop("user_nodes", os.path.join(tempfile.gettempdir(), "groomlab_no_user_nodes"))
    env.update(extra)
    return env


class HelperTestCase(unittest.TestCase):
    def setUp(self):
        if not HELPER or not os.path.exists(HELPER):
            self.skipTest("GROOMLAB_HELPER not built")
        self.tmp = tempfile.mkdtemp(prefix="groomlab_helper_")
        self.h = HelperClient(HELPER, env=env_for())

    def tearDown(self):
        if hasattr(self, "h"):
            code = self.h.close()
            self.assertEqual(code, 0, "the helper did not end cleanly")
        if hasattr(self, "tmp"):
            shutil.rmtree(self.tmp, ignore_errors=True)

    def head_session(self, preset="hair_basics", space=None):
        s = self.h.call("new_session", space=space or {"upAxis": "Z", "metersPerUnit": 1.0})["session"]
        points, counts, indices, uvs = HEAD
        r = self.h.call("set_scalp", session=s, **self.h.put(points=to_blender(points), faceCounts=counts,
                                                              faceIndices=indices, uvs=uvs))
        self.assertEqual(r["points"], len(points))
        if preset:
            self.h.call("apply_preset", session=s, name=preset)
        self.h.call("set_time", session=s, frame=1)
        return s

    def evaluate_hash(self, s, density):
        r = self.h.call("evaluate", session=s, density=density, space="groom", attributes=["rootUV"])
        self.assertEqual(r["errors"], [])
        a = r["arrays"]
        uvs = a["attributes"]["rootUV"]
        frame = strands_hash(self.h.array(a["ids"]), self.h.array(a["offsets"]), self.h.array(a["positions"]),
                             self.h.array(a["widths"]), self.h.array(uvs))
        return file_hash([frame]), r

    def assert_matches_cli(self, s, density, name):
        """The session's strands (groom space, from shared memory) are the bits groomlab-cli evaluates
        from the .groom the helper saves of it."""
        ours, r = self.evaluate_hash(s, density)
        groom = os.path.join(self.tmp, name + ".groom")
        self.h.call("save_groom", session=s, path=groom)
        abc = os.path.join(self.tmp, name + "_cli.abc")
        cli_eval(groom, abc, density=density)
        self.assertEqual("%016x" % ours, "%016x" % cli_hash(abc))
        return r


class ProtocolTest(HelperTestCase):
    def test_hello_names_the_protocol_and_the_version(self):
        r = self.h.call("hello", protocol=1)
        self.assertEqual(r["protocol"], 1)
        self.assertEqual(r["groomSpace"], {"upAxis": "Y", "units": "cm"})
        self.assertEqual(r["features"], {"tools": 1, "state": 1})
        if CLI:
            out = subprocess.run([CLI, "--version"], capture_output=True, text=True).stdout
            self.assertIn(r["version"], out)
        with self.assertRaises(HelperError) as e:
            self.h.call("hello", protocol=99)
        self.assertEqual(e.exception.code, "protocol")
        out = subprocess.run([HELPER, "--version"], capture_output=True, text=True).stdout
        self.assertIn("protocol 1", out)

    def test_bad_requests_are_answered_and_the_helper_carries_on(self):
        r = self.h.send_line("this is not json")
        self.assertFalse(r["ok"])
        self.assertIsNone(r["id"])
        self.assertEqual(r["error"]["code"], "bad_request")
        self.assertEqual(self.h.send_line("[1, 2, 3]")["error"]["code"], "bad_request")
        self.assertEqual(self.h.send_line('{"id": 7}')["error"]["code"], "bad_request")
        self.assertEqual(self.h.send_line('{"id": 8, "cmd": 5}')["id"], 8)
        self.assertEqual(self.h.request("no_such_command")["error"]["code"], "unknown_command")
        self.assertEqual(self.h.request("evaluate", session=99)["error"]["code"], "no_session")
        s = self.h.call("new_session")["session"]
        self.assertEqual(self.h.request("set_scalp", session=s, points=[0, 0, 0], faceCounts=[3],
                                        faceIndices=[0, 1, 2])["error"]["code"], "bad_request")
        self.assertEqual(self.h.request("set_scalp", session=s, points={"shm": "groomlab_no_such_block", "shape": [3, 3]},
                                        faceCounts=[3], faceIndices=[0, 1, 2])["error"]["code"], "shared_memory")
        self.assertEqual(self.h.request("set_scalp", session=s, points={"shm": "bad name!", "shape": [3, 3]},
                                        faceCounts=[3], faceIndices=[0, 1, 2])["error"]["code"], "shared_memory")
        self.assertEqual(self.h.request("set_scalp", session=s, points=[0, 0, 0, 1, 0, 0, 0, 1, 0], faceCounts=[3],
                                        faceIndices=[0, 1, 2], uvs=[0, 0])["error"]["code"], "bad_request")
        self.assertEqual(self.h.request("set_graph", session=s, graph={"nodes": [{"name": "a", "type": "NoSuchOp"}]})
                         ["error"]["code"], "graph")
        self.assertEqual(self.h.request("load_groom", session=s, path=os.path.join(self.tmp, "none.groom"))
                         ["error"]["code"], "io")
        bad = os.path.join(self.tmp, "bad.groom")
        with open(bad, "w") as f:
            f.write("{not a groom")
        self.assertEqual(self.h.request("load_groom", session=s, path=bad)["error"]["code"], "parse")
        self.assertEqual(self.h.request("set_param", session=s, node="nope", param="x", value=1)["error"]["code"],
                         "not_found")
        # An array too short for its shape in a real block.
        refs = self.h.put(points=np.zeros((2, 3), np.float32))
        refs["points"]["shape"] = [1000, 3]
        self.assertEqual(self.h.request("set_scalp", session=s, faceCounts=[3], faceIndices=[0, 1, 2], **refs)
                         ["error"]["code"], "bad_request")
        self.assertTrue(self.h.call("ping")["pong"])
        self.assertIsNone(self.h.proc.poll())

    def test_the_helper_ends_with_its_input(self):
        self.h.proc.stdin.close()
        self.assertEqual(self.h.proc.wait(30), 0)


@unittest.skipIf(HEAD is None, "no test head")
class GroomTest(HelperTestCase):
    def test_a_preset_groom_is_what_the_cli_evaluates_from_its_saved_file(self):
        s = self.head_session()
        r = self.assert_matches_cli(s, 0.1, "preset")
        self.assertGreater(r["strandCount"], 1000)
        self.assertNotIn("learning", r)
        self.assertNotIn("watermark", r)
        # The saved file is Maya's own space, with no license tag.
        with open(os.path.join(self.tmp, "preset.groom")) as f:
            saved = json.load(f)
        self.assertEqual(saved.get("upAxis", "Y"), "Y")
        self.assertEqual(saved.get("units", "cm"), "cm")
        self.assertNotIn("license", saved)

        # The host's space: the same strands turned into Z up meters, widths too.
        groom = self.h.call("evaluate", session=s, space="groom")
        g_pos = self.h.array(groom["arrays"]["positions"])
        g_wid = self.h.array(groom["arrays"]["widths"])
        host = self.h.call("evaluate", session=s)
        self.assertTrue(host["sameTopology"])
        h_pos = self.h.array(host["arrays"]["positions"])
        np.testing.assert_allclose(h_pos, to_blender(g_pos), rtol=1e-6, atol=1e-9)
        np.testing.assert_allclose(self.h.array(host["arrays"]["widths"]), g_wid / 100.0, rtol=1e-6)
        counts = self.h.array(host["arrays"]["pointCounts"])
        offsets = self.h.array(host["arrays"]["offsets"])
        self.assertEqual(offsets.dtype, np.int32)
        self.assertEqual(int(counts.sum()), host["pointCount"])
        np.testing.assert_array_equal(np.diff(offsets), counts)
        names = {a["name"] for a in host["attributesAvailable"]}
        self.assertTrue({"rootUV", "color", "melanin"} <= names, names)
        colors = self.h.call("evaluate", session=s, attributes=["color", "melanin", "nope"])
        self.assertEqual(colors["arrays"]["attributes"]["color"]["shape"], [colors["strandCount"], 3])
        self.assertEqual(colors["missingAttributes"], ["nope"])

        # A render evaluation (every strand) exported by the helper is the CLI's export of the file.
        ours = os.path.join(self.tmp, "helper.abc")
        self.h.call("export", session=s, format="alembic", path=ours, frames={"first": 1, "last": 1})
        theirs = os.path.join(self.tmp, "cli_full.abc")
        cli_eval(os.path.join(self.tmp, "preset.groom"), theirs)
        self.assertEqual(cli_hash(ours), cli_hash(theirs))
        usd = os.path.join(self.tmp, "helper.usda")
        self.h.call("export", session=s, format="usd", path=usd)
        with open(usd) as f:
            self.assertIn("metersPerUnit = 0.01", f.read(4000))

    def test_the_hair_look_and_the_attributes_arnold_shades_by(self):
        """evaluate's look (what Maya's create_hair_shader builds the Arnold shader from) and the
        attributes the add-on's material reads: per-point color, iridescence, rootNormal. A HairColor
        colors by its RGB in either mode."""
        s = self.head_session()
        wanted = ["color", "pointColor", "melanin", "melaninRedness", "iridescence", "rootNormal"]
        r = self.h.call("evaluate", session=s, density=0.05, attributes=wanted)
        look = r["look"]
        self.assertEqual(look["node"], "color")
        self.assertTrue(look["colorsByRgb"])
        self.assertFalse(look["colorBlends"] or look["iridescent"])
        self.assertNotIn("bySource", look)
        self.assertEqual(look["dye"], [1.0, 1.0, 1.0])
        self.assertAlmostEqual(look["roughness"], 0.5)
        self.assertEqual(set(r["arrays"]["attributes"]), {"color", "melanin", "melaninRedness"})
        self.assertEqual(sorted(r["missingAttributes"]), ["iridescence", "pointColor", "rootNormal"])

        for param, value in (("tipStrength", 0.6), ("dye", [0.5, 0.6, 0.7])):
            self.h.call("set_param", session=s, node="color", param=param, value=value)
        # Iridescence: a FeatherColor after the HairColor.
        after = next(c for c in self.h.call("get_graph", session=s)["graph"]["connections"] if c["from"] == "color")
        sheen = self.h.call("edit_graph", session=s, op="add_node", type="FeatherColor")["name"]
        self.h.call("edit_graph", session=s, op="connect", **{"from": "color", "to": sheen, "input": "strands"})
        self.h.call("edit_graph", session=s, op="connect", **{"from": sheen, "to": after["to"], "input": after["input"]})
        for param, value in (("pattern", "none"), ("iridescence", 0.5)):
            self.h.call("set_param", session=s, node=sheen, param=param, value=value)
        r = self.h.call("evaluate", session=s, density=0.05, attributes=wanted)
        look = r["look"]
        self.assertTrue(look["colorsByRgb"] and look["iridescent"])
        self.assertAlmostEqual(look["dye"][2], 0.7, places=6)
        S, P = r["strandCount"], r["pointCount"]
        at = r["arrays"]["attributes"]
        self.assertEqual((at["pointColor"]["scope"], at["pointColor"]["shape"]), ("point", [P, 3]))
        self.assertEqual((at["iridescence"]["scope"], at["iridescence"]["shape"][0]), ("point", P))
        self.assertEqual((at["rootNormal"]["scope"], at["rootNormal"]["shape"]), ("strand", [S, 3]))
        host = self.h.array(at["rootNormal"]).copy()
        np.testing.assert_allclose(np.linalg.norm(host, axis=1), 1.0, atol=1e-4)
        # A direction: turned into the host's axes, not scaled.
        g = self.h.call("evaluate", session=s, density=0.05, space="groom", attributes=["rootNormal"])
        groom_normals = self.h.array(g["arrays"]["attributes"]["rootNormal"])
        np.testing.assert_allclose(host, np.stack([groom_normals[:, 0], -groom_normals[:, 2], groom_normals[:, 1]], 1),
                                   atol=1e-6)

    def test_guides_from_the_host_drive_the_groom_and_edit_incrementally(self):
        s = self.head_session()
        points = to_blender(HEAD[0])
        center = points.mean(axis=0)
        picked = [i for i in range(len(points)) if points[i, 2] > center[2] + 0.02][::40][:24]
        self.assertGreater(len(picked), 10)
        curves = []
        for i in picked:
            n = points[i] - center
            n /= np.linalg.norm(n)
            curves.append([points[i] + n * (0.12 * k / 4.0) + np.array([0, 0, -0.01 * k]) for k in range(5)])
        flat = np.array(curves, dtype=np.float32).reshape(-1, 3)
        counts = np.full(len(curves), 5, np.int32)
        r = self.h.call("set_guides", session=s, **self.h.put(points=flat, counts=counts))
        self.assertEqual(r["target"], "flat")
        self.assertEqual([a["id"] for a in r["added"]], list(range(1, len(curves) + 1)))
        self.assert_matches_cli(s, 0.1, "guides")

        # The guides come back with their IDs, rooted on the scalp (their roots were on it).
        g = self.h.call("get_guides", session=s)
        self.assertEqual(g["source"], "flat")
        ids = self.h.array(g["arrays"]["ids"])
        back = self.h.array(g["arrays"]["points"])
        np.testing.assert_array_equal(ids, np.arange(1, len(curves) + 1, dtype=np.uint64))
        np.testing.assert_allclose(back, flat, atol=1e-4)
        # Sent back unchanged (the host's round trip): nothing changes, the cache stays.
        same = self.h.call("set_guides", session=s, **self.h.put(points=back, counts=counts, ids=ids))
        self.assertFalse(same["updated"])

        # One guide sculpted: only it changes, the groom re-evaluates incrementally.
        self.h.call("evaluate", session=s, density=0.1)
        moved = back.copy()
        moved[5 * 3 + 1:5 * 3 + 5] += np.array([0.02, 0.01, 0.0], np.float32)
        r = self.h.call("set_guides", session=s, **self.h.put(points=moved, counts=counts, ids=ids))
        self.assertEqual((r["changed"], r["added"], r["removed"]), (1, [], 0))
        e = self.h.call("evaluate", session=s, density=0.1)
        self.assertTrue(e["sameTopology"])
        self.assertGreater(e["nodesIncremental"], 0, e)
        self.assert_matches_cli(s, 0.1, "guides_edited")

        # A guide removed and a new one drawn: IDs stay stable, the new one gets the next.
        keep = np.ones(len(curves), bool)
        keep[0] = False
        pts = np.concatenate([moved.reshape(len(curves), 5, 3)[keep].reshape(-1, 3), flat[:5] + 0.001])
        r = self.h.call("set_guides", session=s, **self.h.put(points=pts, counts=np.full(len(curves), 5, np.int32),
                                                               ids=np.concatenate([ids[keep], [0]]).astype(np.uint64)))
        self.assertEqual(r["removed"], 1)
        self.assertEqual(r["added"], [{"index": len(curves) - 1, "id": len(curves) + 1}])
        self.assert_matches_cli(s, 0.1, "guides_redrawn")

        # The scalp remeshed (its triangles in another order): the guides are rooted on it again.
        p, c, i, uv = HEAD
        tris = i.reshape(-1, 3)[::-1].ravel().copy()
        tri_uvs = uv.reshape(-1, 3, 2)[::-1].reshape(-1, 2).copy()
        r = self.h.call("set_scalp", session=s, **self.h.put(points=to_blender(p), faceCounts=c, faceIndices=tris,
                                                              uvs=tri_uvs))
        self.assertEqual(r["guidesRerooted"], len(curves))
        self.assert_matches_cli(s, 0.1, "guides_remeshed")

    def test_guides_kept_at_rest_follow_a_deforming_scalp(self):
        """Blender's guide curves stay on the scalp's rest pose (set_guides pose "rest"): the session
        is handed them carried to the scalp as it deforms (update_scalp), and following the scalp
        carries the strands grown at rest."""
        points, counts, indices, uvs = HEAD
        rest = to_blender(points)
        s = self.h.call("new_session")["session"]
        self.h.call("set_scalp", session=s, **self.h.put(points=rest, restPoints=rest, faceCounts=counts,
                                                          faceIndices=indices, uvs=uvs))
        self.h.call("apply_preset", session=s, name="hair_basics")
        self.h.call("set_time", session=s, frame=1)
        center = rest.mean(axis=0)
        picked = [i for i in range(len(rest)) if rest[i, 2] > center[2] + 0.02][::60][:16]
        curves = []
        for i in picked:
            n = rest[i] - center
            n /= np.linalg.norm(n)
            curves.append([rest[i] + n * (0.1 * k / 4.0) for k in range(5)])
        flat = np.array(curves, dtype=np.float32).reshape(-1, 3)
        gcounts = np.full(len(curves), 5, np.int32)
        r = self.h.call("set_guides", session=s, pose="rest", **self.h.put(points=flat, counts=gcounts))
        self.assertEqual(r["pose"], "rest")
        self.assertEqual(self.h.call("set_options", session=s, followScalp=True)["followScalp"], True)
        self.assertTrue(self.h.call("groom_info", session=s)["followScalp"])
        at_rest = self.h.call("evaluate", session=s, density=0.05)
        self.assertFalse(at_rest["following"])
        base = self.h.array(at_rest["arrays"]["positions"])
        base_ids = self.h.array(at_rest["arrays"]["ids"])
        rested = self.h.array(self.h.call("get_guides", session=s, pose="rest")["arrays"]["points"])

        # The head turned 30 degrees about Z and moved: a rigid motion of the whole scalp.
        a = np.radians(30.0)
        rot = np.array([[np.cos(a), -np.sin(a), 0], [np.sin(a), np.cos(a), 0], [0, 0, 1]])
        move = np.array([0.05, -0.02, 0.1])
        turned = (rest.astype(np.float64) @ rot.T + move).astype(np.float32)
        self.h.call("update_scalp", session=s, **self.h.put(points=turned, restPoints=rest))
        g = self.h.call("get_guides", session=s)
        self.assertEqual(g["pose"], "now")
        np.testing.assert_allclose(self.h.array(g["arrays"]["points"]), rested @ rot.T + move, atol=2e-5)
        g = self.h.call("get_guides", session=s, pose="rest")
        self.assertEqual(g["pose"], "rest")
        np.testing.assert_array_equal(self.h.array(g["arrays"]["points"]), rested)
        followed = self.h.call("evaluate", session=s, density=0.05)
        self.assertTrue(followed["following"])
        np.testing.assert_array_equal(self.h.array(followed["arrays"]["ids"]), base_ids)
        # (Within the scalp's distance field's own accuracy: Interpolate's clearance lifts a point by
        # the field's blend or, where the field cannot tell, the mesh's answer, which a rounding of
        # the point may switch between.)
        np.testing.assert_allclose(self.h.array(followed["arrays"]["positions"]), base @ rot.T + move, atol=6e-5)
        # Not following: grown on the turned scalp from the carried guides; the roots are turned.
        self.h.call("set_options", session=s, followScalp=False)
        grown = self.h.call("evaluate", session=s, density=0.05)
        offsets = self.h.array(grown["arrays"]["offsets"])[:-1]
        np.testing.assert_allclose(self.h.array(grown["arrays"]["positions"])[offsets],
                                   base[offsets] @ rot.T + move, atol=2e-5)
        # Back at rest: the same strands as before, bit for bit.
        self.h.call("set_options", session=s, followScalp=True)
        self.h.call("update_scalp", session=s, **self.h.put(points=rest, restPoints=rest))
        back = self.h.call("evaluate", session=s, density=0.05)
        np.testing.assert_array_equal(self.h.array(back["arrays"]["positions"]), base)
        # A sculpt at rest while the scalp is turned: stored at rest, handed turned.
        edited = rested.copy()
        edited[4] += np.array([0.0, 0.0, -0.02], np.float32)
        self.h.call("update_scalp", session=s, **self.h.put(points=turned, restPoints=rest))
        ids = self.h.array(g["arrays"]["ids"])
        r = self.h.call("set_guides", session=s, pose="rest", **self.h.put(points=edited, counts=gcounts, ids=ids))
        self.assertEqual(r["changed"], 1)
        now = self.h.array(self.h.call("get_guides", session=s)["arrays"]["points"])
        np.testing.assert_allclose(now[4], edited[4] @ rot.T + move, atol=2e-5)
        self.assertEqual(self.h.request("set_guides", session=s, pose="later", points=[0, 0, 0, 0, 0, 1], counts=[2])
                         ["error"]["code"], "bad_request")

    def test_a_groom_files_hierarchy_guides_are_edited_by_id(self):
        s = self.h.call("new_session")["session"]
        info = self.h.call("load_groom", session=s, path=COMPAT_GROOM)
        self.assertEqual(info["fileUpAxis"], "Y")
        self.assertEqual(info["hierarchyGuides"], 15)
        self.h.call("set_time", session=s, frame=1)
        # Loaded as it is: the CLI's strands of the same file.
        ours, _ = self.evaluate_hash(s, 1.0)
        abc = os.path.join(self.tmp, "compat_cli.abc")
        cli_eval(COMPAT_GROOM, abc)
        self.assertEqual(ours, cli_hash(abc))

        g = self.h.call("get_guides", session=s, source="hierarchy")
        self.assertEqual(g["guides"], 5)  # the 3 primaries and 2 locked children: the others follow them
        ids = self.h.array(g["arrays"]["ids"])
        counts = self.h.array(g["arrays"]["counts"])
        pts = self.h.array(g["arrays"]["points"])
        r = self.h.call("set_guides", session=s, target="hierarchy", **self.h.put(points=pts, counts=counts, ids=ids))
        self.assertFalse(r["updated"])
        tip = int(counts[0]) - 1
        pts[tip] += np.array([0.003, 0.0, 0.002], np.float32)
        r = self.h.call("set_guides", session=s, target="hierarchy", **self.h.put(points=pts, counts=counts, ids=ids))
        self.assertEqual((r["changed"], r["removed"]), (1, 0))
        info = self.h.call("groom_info", session=s)
        self.assertEqual(info["hierarchyGuides"], 15)  # the children are still there
        self.assert_matches_cli(s, 1.0, "compat_edited")

    def test_a_groom_file_with_other_scalps_and_a_reference(self):
        # A groom file with another scalp (its own Scalp and Scatter merged in) and a Reference node
        # bringing in another groom file: the helper's strands are the CLI's, and its save keeps the
        # other scalp.
        with open(COMPAT_GROOM) as f:
            groom = json.load(f)
        ear = {"name": "ear", "points": [], "triangles": []}
        for z in range(5):
            for x in range(5):
                ear["points"] += [30.0 + 2.0 * x, 0.0, 2.0 * z]
        for z in range(4):
            for x in range(4):
                a = z * 5 + x
                ear["triangles"] += [a, a + 5, a + 1, a + 5, a + 6, a + 1]
        groom["scalps"] = [ear]
        groom["searchPaths"] = [os.path.dirname(os.path.abspath(COMPAT_GROOM)).replace("\\", "/")]  # its maps
        graph = groom["graph"]
        output = graph.get("output", "output")
        feed = next(c for c in graph["connections"] if c["to"] == output and c["input"] == "strands")
        library = os.path.abspath(COMPAT_GROOM).replace("\\", "/")
        graph["nodes"] += [{"name": "earScalp", "type": "Scalp", "params": {"input": "ear"}},
                           {"name": "earScatter", "type": "Scatter", "params": {"density": 2.0, "seed": 5}},
                           {"name": "library", "type": "Reference", "params": {"path": library, "bind": "world"}},
                           {"name": "earMerge", "type": "Merge"}, {"name": "libraryMerge", "type": "Merge"}]
        graph["connections"].remove(feed)
        graph["connections"] += [{"from": "earScalp", "to": "earScatter", "input": "scalp"},
                                 {"from": feed["from"], "to": "earMerge", "input": "a"},
                                 {"from": "earScatter", "to": "earMerge", "input": "b"},
                                 {"from": "earMerge", "to": "libraryMerge", "input": "a"},
                                 {"from": "library", "to": "libraryMerge", "input": "b"},
                                 {"from": "libraryMerge", "to": output, "input": "strands"}]
        path = os.path.join(self.tmp, "scalps.groom")
        with open(path, "w") as f:
            json.dump(groom, f)
        s = self.h.call("new_session")["session"]
        self.h.call("load_groom", session=s, path=path)
        self.h.call("set_time", session=s, frame=1)
        ours, r = self.evaluate_hash(s, 1.0)
        abc = os.path.join(self.tmp, "scalps_cli.abc")
        cli_eval(path, abc)
        self.assertEqual(ours, cli_hash(abc))
        plain, _ = self.evaluate_hash(self.compat_session(), 1.0)
        self.assertNotEqual(ours, plain)
        saved = os.path.join(self.tmp, "scalps_saved.groom")
        self.h.call("save_groom", session=s, path=saved)
        with open(saved) as f:
            self.assertEqual([x["name"] for x in json.load(f).get("scalps", [])], ["ear"])

    def compat_session(self):
        s = self.h.call("new_session")["session"]
        self.h.call("load_groom", session=s, path=COMPAT_GROOM)
        self.h.call("set_time", session=s, frame=1)
        return s

    def test_params_graph_edits_and_presets(self):
        s = self.head_session()
        nodes = {n["name"]: n for n in self.h.call("list_nodes", session=s)["nodes"]}
        self.assertEqual(nodes["noise"]["type"], "Noise")
        spec = self.h.call("node_spec", session=s, node="noise")["spec"]
        amplitude = [p for p in spec["params"] if p["name"] == "amplitude"][0]
        self.assertIn("unit", amplitude)
        self.assertTrue(any("group" in p for p in spec["params"]))
        self.assertTrue(any(p.get("advanced") for p in spec["params"]))
        before = self.h.call("get_param", session=s, node="noise", param="amplitude")["value"]
        self.h.call("set_param", session=s, node="noise", param="amplitude", value=before * 2)
        self.assertEqual(self.h.call("get_param", session=s, node="noise", param="amplitude")["value"], before * 2)
        with self.assertRaises(HelperError) as e:
            self.h.call("set_param", session=s, node="noise", param="amplitude", value="loud")
        self.assertEqual(e.exception.code, "type_mismatch")
        self.assert_matches_cli(s, 0.1, "param")

        # Graph edits as an editor makes them, and a preset inserted as a compound.
        made = self.h.call("edit_graph", session=s, op="add_node", type="Gravity")["name"]
        self.assertEqual(self.h.call("edit_graph", session=s, op="connect", **{"from": "noise", "to": made, "input": "strands"})
                         ["graphError"], "")
        # A link into a picking Clump's centers sets it to take them (GraphEdit::connectByHand).
        clump = self.h.call("edit_graph", session=s, op="add_node", type="Clump")["name"]
        spots = self.h.call("edit_graph", session=s, op="add_node", type="ToGuides")["name"]
        self.h.call("edit_graph", session=s, op="connect", **{"from": spots, "to": clump, "input": "centers"})
        self.assertEqual(self.h.call("get_param", session=s, node=clump, param="centersFrom")["value"], "inputPoints")
        for n in (clump, spots):
            self.h.call("edit_graph", session=s, op="remove_node", name=n)
        inserted = self.h.call("insert_preset", session=s, name="frizz", after="noise")
        self.assertTrue(inserted["node"])
        names = [n["name"] for n in self.h.call("list_nodes", session=s)["nodes"]]
        self.assertIn(inserted["node"], names)
        graph = self.h.call("get_graph", session=s)["graph"]
        self.h.call("set_graph", session=s, graph=graph)
        self.assert_matches_cli(s, 0.1, "inserted")
        presets = {p["name"] for p in self.h.call("list_presets")["presets"]}
        self.assertTrue({"hair_basics", "frizz", "curly"} <= presets)

        # The node library (Maya's and the app's): the shipped compound nodes listed, one added as
        # the Compound holding its copy, evaluated by the core as the CLI evaluates the saved groom.
        library = {n["id"]: n for n in self.h.call("library_nodes")["nodes"]}
        self.assertTrue({"groomlab.soft_frizz", "groomlab.grey_streaks", "groomlab.windswept_ends"} <= set(library))
        self.assertEqual(library["groomlab.soft_frizz"]["type"], "node:groomlab.soft_frizz")
        frizz = self.h.call("edit_graph", session=s, op="add_node", type="node:groomlab.soft_frizz")["name"]
        self.assertEqual(frizz, "softFrizz")
        self.h.call("edit_graph", session=s, op="connect", **{"from": inserted["node"], "to": frizz, "input": "strands"})
        graph = self.h.call("get_graph", session=s)["graph"]
        node = next(n for n in graph["nodes"] if n["name"] == frizz)
        self.assertEqual(node["type"], "Compound")
        self.assertEqual(node["params"]["definition"]["id"], "groomlab.soft_frizz")
        self.assertFalse(self.h.call("edit_graph", session=s, op="update_library_node", name=frizz)["updated"])
        self.assert_matches_cli(s, 0.1, "library node")
        # A shipped script node: listed as one, added as the Script node holding its file, its
        # params the header's, evaluated by the core as the CLI evaluates the saved groom.
        self.assertEqual(library["groomlab.wave"]["kind"], "script")
        wave = self.h.call("edit_graph", session=s, op="add_node", type="node:groomlab.wave")["name"]
        self.assertEqual(wave, "wave")
        self.h.call("edit_graph", session=s, op="connect", **{"from": frizz, "to": wave, "input": "strands"})
        graph = self.h.call("get_graph", session=s)["graph"]
        node = next(n for n in graph["nodes"] if n["name"] == wave)
        self.assertEqual(node["type"], "Script")
        self.assertIn("per point", node["params"]["definition"]["script"])
        spec = self.h.call("node_spec", session=s, node=wave)["spec"]
        self.assertIn("amplitude", [p["name"] for p in spec["params"]])
        self.assert_matches_cli(s, 0.1, "script node")

        # A preset fits the scalp: the same on a head ten times smaller, lengths scaled.
        big = self.h.call("get_param", session=s, node="scatter", param="length")["value"]
        small = self.h.call("new_session", space={"upAxis": "Z", "metersPerUnit": 0.1})["session"]
        p, c, i, uv = HEAD
        self.h.call("set_scalp", session=small, **self.h.put(points=to_blender(p), faceCounts=c, faceIndices=i))
        fit = self.h.call("apply_preset", session=small, name="hair_basics")
        self.assertAlmostEqual(fit["fitFactor"], 0.1 * self.scalp_size() / 20.0, 4)  # hair_basics was made on a 20 cm head
        self.assertAlmostEqual(self.h.call("get_param", session=small, node="scatter", param="length")["value"], big * 0.1, 4)

    def scalp_size(self):
        p = HEAD[0]
        return float((p.max(axis=0) - p.min(axis=0)).max())


class SpaceTest(HelperTestCase):
    @unittest.skipIf(HEAD is None, "no test head")
    def test_blender_space_round_trips_through_a_maya_groom(self):
        points, counts, indices, uvs = HEAD
        blender = to_blender(points)
        s = self.h.call("new_session", space={"upAxis": "Z", "metersPerUnit": 1.0})["session"]
        self.h.call("set_scalp", session=s, **self.h.put(points=blender, faceCounts=counts, faceIndices=indices, uvs=uvs))
        got = self.h.call("get_scalp", session=s)
        self.assertTrue(got["scalp"]["facesExact"])
        np.testing.assert_allclose(self.h.array(got["arrays"]["points"]), blender, rtol=1e-6, atol=1e-9)
        np.testing.assert_array_equal(self.h.array(got["arrays"]["faceCounts"]), counts)
        np.testing.assert_array_equal(self.h.array(got["arrays"]["faceIndices"]), indices)
        np.testing.assert_array_equal(self.h.array(got["arrays"]["uvs"]), uvs.reshape(-1, 2))
        groom_points = self.h.array(self.h.call("get_scalp", session=s, space="groom")["arrays"]["points"])
        np.testing.assert_array_equal(groom_points, to_groom(blender))

        # Saved as Maya keeps grooms: Y up, centimeters.
        path = os.path.join(self.tmp, "space.groom")
        self.assertEqual(self.h.call("save_groom", session=s, path=path)["upAxis"], "Y")
        with open(path) as f:
            saved = json.load(f)
        self.assertEqual(saved.get("upAxis", "Y"), "Y")
        self.assertEqual(saved.get("units", "cm"), "cm")
        np.testing.assert_array_equal(np.array(saved["scalp"]["points"], np.float32).reshape(-1, 3), groom_points)
        np.testing.assert_allclose(groom_points, points, rtol=1e-6, atol=1e-5)

        # Opened again in Blender's space: the same points, the same groom-space bits.
        t = self.h.call("new_session")["session"]
        self.h.call("load_groom", session=t, path=path)
        np.testing.assert_allclose(self.h.array(self.h.call("get_scalp", session=t)["arrays"]["points"]), blender, rtol=1e-6)
        np.testing.assert_array_equal(
            self.h.array(self.h.call("get_scalp", session=t, space="groom")["arrays"]["points"]), groom_points)

        # Saved Z up (a Houdini or Blender pipeline's choice) and read back: the quarter turn is exact.
        zpath = os.path.join(self.tmp, "space_z.groom")
        self.h.call("save_groom", session=t, path=zpath, upAxis="Z")
        with open(zpath) as f:
            zsaved = json.load(f)
        self.assertEqual(zsaved["upAxis"], "Z")
        np.testing.assert_array_equal(np.array(zsaved["scalp"]["points"], np.float32).reshape(-1, 3),
                                      np.stack([groom_points[:, 0], -groom_points[:, 2], groom_points[:, 1]], axis=1))
        u = self.h.call("new_session")["session"]
        self.assertEqual(self.h.call("load_groom", session=u, path=zpath)["fileUpAxis"], "Z")
        np.testing.assert_array_equal(
            self.h.array(self.h.call("get_scalp", session=u, space="groom")["arrays"]["points"]), groom_points)

        # Y up centimeters is no conversion at all: the bits as given.
        v = self.h.call("new_session", space={"upAxis": "Y", "metersPerUnit": 0.01})["session"]
        self.h.call("set_scalp", session=v, **self.h.put(points=points, faceCounts=counts, faceIndices=indices))
        np.testing.assert_array_equal(self.h.array(self.h.call("get_scalp", session=v)["arrays"]["points"]), points)


@unittest.skipIf(HEAD is None, "no test head")
class FreeTest(HelperTestCase):
    def test_every_export_with_no_key(self):
        # GroomLab is free for any use: no license in the protocol, every export writes its file,
        # and an old groom saved in the former Learning mode ("license": "learning") is one like any.
        hello = self.h.call("hello")
        self.assertNotIn("license", hello)
        for gone in ("license_status", "license_reload", "license_activate", "license_check", "license_deactivate"):
            self.assertEqual(self.h.request(gone)["error"]["code"], "unknown_command", gone)
        s = self.head_session()
        r = self.h.call("evaluate", session=s, density=0.05)
        self.assertNotIn("learning", r)
        self.assertNotIn("watermark", r)
        for fmt in ("alembic", "usd"):
            out = os.path.join(self.tmp, "x." + ("abc" if fmt == "alembic" else "usda"))
            self.h.call("export", session=s, format=fmt, path=out, density=0.05)
            self.assertGreater(os.path.getsize(out), 0, fmt)
        path = os.path.join(self.tmp, "free.groom")
        saved = self.h.call("save_groom", session=s, path=path)
        self.assertNotIn("license", saved)
        self.assertNotIn("learning", saved)
        with open(path) as f:
            text = json.load(f)
        self.assertNotIn("license", text)
        self.assertNotIn("license", self.h.call("get_state", session=s))
        text["license"] = "learning"
        old = os.path.join(self.tmp, "old_learning.groom")
        with open(old, "w") as f:
            json.dump(text, f)
        t = self.h.call("new_session")["session"]
        info = self.h.call("load_groom", session=t, path=old)
        self.assertNotIn("learningFile", info)
        self.assertNotIn("license", info)
        self.h.call("export", session=t, format="alembic", path=os.path.join(self.tmp, "old.abc"), density=0.05)
        self.assertGreater(os.path.getsize(os.path.join(self.tmp, "old.abc")), 0)
        resaved = os.path.join(self.tmp, "resaved.groom")
        self.h.call("save_groom", session=t, path=resaved)
        with open(resaved) as f:
            self.assertNotIn("license", json.load(f))
        # groomlab-cli exports the old file too.
        if CLI:
            cli_eval(old, os.path.join(self.tmp, "cli.abc"), density=0.05)
            self.assertGreater(os.path.getsize(os.path.join(self.tmp, "cli.abc")), 0)


# ---- GroomLab's tools in the host (docs/BLENDER.md, "Tools") ----------------------------------------

CORE_PYTHON = os.environ.get("GROOMLAB_CORE_PYTHON", "")
CORE_PYTHONPATH = os.environ.get("GROOMLAB_CORE_PYTHONPATH", "")
FROZEN = [0.45, 0.6, 0.85]  # display.h DisplayTheme::frozen


class Camera(object):
    """A Blender viewport's matrices (Z up, meters; column vectors, rows first as bpy prints them):
    a perspective camera at `eye` looking at `target`, over a region of width x height pixels."""

    def __init__(self, eye=(0.0, -0.30, 0.30), target=(0.0, 0.0, 0.02), width=800, height=600, fov=40.0):
        eye, target = np.asarray(eye, np.float64), np.asarray(target, np.float64)
        up = np.array([0.0, 0.0, 1.0])
        fw = target - eye
        fw /= np.linalg.norm(fw)
        r = np.cross(fw, up)
        r /= np.linalg.norm(r)
        u = np.cross(r, fw)
        self.V = np.array([[r[0], r[1], r[2], -r.dot(eye)], [u[0], u[1], u[2], -u.dot(eye)],
                           [-fw[0], -fw[1], -fw[2], fw.dot(eye)], [0, 0, 0, 1.0]])
        t = 1.0 / np.tan(np.radians(fov) / 2.0)
        n, f = 0.01, 100.0
        self.P = np.array([[t * height / width, 0, 0, 0], [0, t, 0, 0], [0, 0, -(f + n) / (f - n), -2 * f * n / (f - n)], [0, 0, -1.0, 0]])
        self.width, self.height = width, height
        self.eye, self.target = eye, target

    def view(self, target=True):
        v = {"viewMatrix": self.V.tolist(), "projectionMatrix": self.P.tolist(), "width": self.width, "height": self.height}
        if target:
            v["target"] = self.target.tolist()
        return v

    def groom_matrix(self):
        """The world-to-clip matrix of groom space (Y up cm) in the core's convention (row vectors, rows first)."""
        k = 0.01
        C = np.array([[k, 0, 0, 0], [0, 0, -k, 0], [0, k, 0, 0], [0, 0, 0, 1.0]])
        return (self.P @ self.V @ C).T.ravel()

    def pixel(self, p):
        clip = self.P @ self.V @ np.append(np.asarray(p, np.float64), 1.0)
        ndc = clip[:3] / clip[3]
        return [float((ndc[0] + 1.0) * 0.5 * self.width), float((ndc[1] + 1.0) * 0.5 * self.height)]

    def pixel_size(self, p):
        """Meters per pixel at point p (along the view)."""
        d = np.dot(np.asarray(p) - self.eye, (self.target - self.eye) / np.linalg.norm(self.target - self.eye))
        return 2.0 * d * (1.0 / self.P[1][1]) / self.height


def crown_points(count):
    """Scalp vertices on the top of the head toward the camera (Blender space), left to right."""
    pts = to_blender(HEAD[0]).astype(np.float64)
    candidates = pts[(pts[:, 2] > 0.055) & (pts[:, 1] < -0.01)]
    candidates = candidates[np.argsort(candidates[:, 0])]
    picks = np.linspace(0, len(candidates) - 1, count).round().astype(int)
    return [candidates[i] for i in picks]


def line(a, b, count, pressure=1.0):
    return [[a[0] + (b[0] - a[0]) * k / (count - 1), a[1] + (b[1] - a[1]) * k / (count - 1), pressure] for k in range(count)]


# The settings every compared stroke sets, in the helper's names (host units) and the bindings'
# (groom units: centimeters, Y up). Meters to centimeters are exact for these values.
BASE = {"radius": 50.0, "strength": 1.0, "falloff": 1.0, "symmetry": "none", "symmetryCenter": 0.0, "selectedOnly": False,
        "stretch": False, "collide": True, "frontFacingOnly": True, "pressure": True, "level": 0, "group": "",
        "length": 0.04, "segments": 8, "matchNeighbors": False, "spacing": 0.01, "targets": "guides",
        "partName": "part", "flowSink": "", "smoothStrength": 1.0, "strengthProfile": None, "falloffProfile": None}
BASE_REF = {"screen_radius": 50.0, "strength": 1.0, "hardness": 0.0, "symmetry": "none", "symmetry_center": 0.0,
            "selected_only": False, "stretch": False, "collide": True, "mask_by_scalp": True, "pressure": True, "level": 0,
            "group": "", "length": 4.0, "segments": 8, "match_neighbors": False, "spacing": 1.0, "targets": "guides",
            "part_name": "part", "flow_sink": "", "smooth_strength": 1.0}


@unittest.skipIf(HEAD is None, "no test head")
class ToolsTest(HelperTestCase):
    """The helper's tools against the core's own BrushTool (stroke_reference.py) doing the same strokes."""

    def setUp(self):
        HelperTestCase.setUp(self)
        self.cam = Camera()
        self.s = self.h.call("new_session")["session"]
        points, counts, indices, uvs = HEAD
        self.h.call("set_scalp", session=self.s, **self.h.put(points=to_blender(points), faceCounts=counts,
                                                               faceIndices=indices, uvs=uvs))
        info = self.h.call("new_groom", session=self.s)
        self.assertEqual(info["guides"], "hierarchy")
        self.assertEqual(info["groom"]["hierarchyGuides"], 0)
        self.assertEqual(info["groom"]["flatGuides"], 0)
        self.steps = []      # what the reference replays
        self.after = []      # the helper's texts after each step
        self.start = None    # the groom file and texts the steps start from

    # ---- Driving the helper as the add-on will ----

    def call(self, cmd, **args):
        return self.h.call(cmd, session=self.s, **args)

    def texts(self):
        return self.call("get_state", texts=True, textures=False)["texts"]

    def begin(self):
        """The groom the compared steps start from, saved for the reference."""
        self.start = os.path.join(self.tmp, "start%d.groom" % len(self.after))
        self.call("save_groom", path=self.start)
        self.start_maps = self.texts()["scalpMaps"]
        self.steps, self.after = [], []

    def stroke(self, tool, events, settings=None, ref=None, ctrl=False, shift=False, view=None):
        """A stroke through tool_set, stroke_begin, stroke_drag and stroke_end; recorded for the reference."""
        host_settings = dict(BASE)
        host_settings.update(settings or {})
        ref_settings = dict(BASE_REF)
        ref_settings.update(ref or {})
        if tool == "freeze":
            for k in ("attribute", "value", "paintKind"):
                host_settings.pop(k, None)
            ref_settings.update(mode="paint", attribute="freeze", value=1.0, paint_kind="guides")
        else:
            ref_settings["mode"] = tool
        self.call("tool_set", tool=tool, settings=host_settings)
        x, y, p = events[0]
        r = self.call("stroke_begin", view=view or self.cam.view(), x=x, y=y, pressure=p, ctrl=ctrl, shift=shift)
        self.assertEqual(r["view"]["width"], self.cam.width)
        if len(events) > 1:
            self.call("stroke_drag", points=events[1:])
        end = self.call("stroke_end")
        self.steps.append({"op": "stroke", "settings": ref_settings, "events": events, "ctrl": ctrl, "shift": shift})
        self.after.append(self.texts())
        return end

    def step(self, op, **extra):
        d = {"op": op}
        d.update(extra)
        self.steps.append(d)
        self.after.append(self.texts())

    def assert_matches_reference(self, fields=("guideData", "partData", "flowData", "scalpMaps")):
        """The helper's texts after each step are the core's own strokes' (the bindings' BrushTool)."""
        if not CORE_PYTHON or not os.path.exists(CORE_PYTHON):
            return  # no bindings built: the helper's own checks stand
        matrix = self.call("hover", view=self.cam.view(), x=1.0, y=1.0, overlay=False)["view"]["groomMatrix"]
        spec = {"groom": self.start, "matrix": matrix, "width": self.cam.width, "height": self.cam.height,
                "scalpMaps": self.start_maps, "steps": self.steps}
        spec_path = os.path.join(self.tmp, "reference_spec.json")
        out_path = os.path.join(self.tmp, "reference_out.json")
        with open(spec_path, "w") as f:
            json.dump(spec, f)
        env = dict(os.environ)
        env["PYTHONPATH"] = CORE_PYTHONPATH
        env.pop("PYTHONHOME", None)
        run = subprocess.run([CORE_PYTHON, os.path.join(HERE, "stroke_reference.py"), spec_path, out_path],
                             capture_output=True, text=True, env=env)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
        with open(out_path) as f:
            ref = json.load(f)
        self.assertEqual(len(ref), len(self.after))
        for i, (ours, theirs) in enumerate(zip(self.after, ref)):
            for field in fields:
                self.assertEqual(ours[field], theirs[field], "step %d (%s): %s differs from the core's" %
                                 (i, self.steps[i]["op"], field))

    def guides(self):
        r = self.call("get_guides", source="hierarchy")
        a = r["arrays"]
        ids, counts, pts = self.h.array(a["ids"]), self.h.array(a["counts"]), self.h.array(a["points"])
        out, at = {}, 0
        for gid, n in zip(ids.tolist(), counts.tolist()):
            out[gid] = pts[at:at + n]
            at += n
        return out

    def place_guides(self, count=5):
        pixels = [self.cam.pixel(p) for p in crown_points(count)]
        for px in pixels:
            self.stroke("place", [[px[0], px[1], 1.0]])
        return pixels

    def comb_events(self, dx=120.0, count=12):
        g = self.guides()
        mids = [self.cam.pixel(pts[len(pts) // 2]) for pts in g.values()]
        xs = [m[0] for m in mids]
        y = float(np.mean([m[1] for m in mids]))
        return line((min(xs) - 30.0, y), (max(xs) + dx, y + 25.0), count)

    # ---- Tests ----

    def test_tool_catalog_and_settings_in_host_units(self):
        cat = self.h.call("tool_catalog")
        ids = [t["id"] for t in cat["tools"]]
        for tool in ("place", "draw", "part", "flow", "comb", "slide", "smooth", "straighten", "lift", "twist", "curl",
                     "attract", "shape", "cut", "length", "erase", "paint", "select", "freeze"):
            self.assertIn(tool, ids)
        self.assertIn("falloff", [s["id"] for s in cat["settings"]])
        r = self.call("tool_set", tool="comb", settings={"radius": 42, "falloff": 0.25, "symmetry": "y", "symmetryCenter": 0.01,
                                                        "length": 0.03, "frontFacingOnly": False, "targets": ["guides", "parts"]})
        st = r["settings"]
        self.assertEqual(r["tool"], "comb")
        self.assertEqual(st["radius"], 42)
        self.assertAlmostEqual(st["hardness"], 0.75)
        self.assertEqual(st["symmetry"], "y")  # the host's axis back, through the groom's z
        self.assertAlmostEqual(st["symmetryCenter"], 0.01, places=7)
        self.assertAlmostEqual(st["length"], 0.03, places=7)
        self.assertFalse(st["frontFacingOnly"])
        self.assertEqual(st["targets"], "guides,parts")
        self.assertIn("radius", r["shows"])
        self.assertEqual(self.call("tool_set", tool="freeze")["settings"]["attribute"], "freeze")
        self.assertEqual(self.call("tool_set", tool="paint")["settings"]["attribute"], "clump")  # Paint's map back
        self.assertEqual(self.call("tool_set", tool="select")["tool"], "select")
        for bad in ({"tool": "nope"}, {"settings": {"nope": 1}}, {"settings": {"symmetry": "w"}}, {"settings": {"radius": "x"}}):
            with self.assertRaises(HelperError) as e:
                self.call("tool_set", **bad)
            self.assertEqual(e.exception.code, "bad_request")
        with self.assertRaises(HelperError) as e:
            self.call("stroke_begin", view=self.cam.view(), x=1, y=1)  # the Select tool does not stroke
        self.assertEqual(e.exception.code, "state")
        self.call("tool_set", tool="comb")
        with self.assertRaises(HelperError) as e:
            self.h.call("stroke_begin", session=self.h.call("new_session")["session"], x=1, y=1)
        self.assertEqual(e.exception.code, "state")  # no view yet

    def test_the_view_is_the_hosts_turned_into_the_grooms(self):
        top = crown_points(3)[1]
        px = self.cam.pixel(top)
        r = self.call("hover", view=self.cam.view(), x=px[0], y=px[1])
        np.testing.assert_allclose(r["view"]["groomMatrix"], self.cam.groom_matrix(), rtol=1e-9, atol=1e-12)
        self.assertFalse(r["view"]["orthographic"])
        np.testing.assert_allclose(r["view"]["eye"], self.cam.eye, atol=1e-5)
        ring = r["ring"]
        self.assertTrue(ring["valid"])
        self.assertTrue(ring["onScalp"])
        # The ring sits on the scalp under the cursor, as wide as 50 px there (the radius set).
        self.assertLess(np.linalg.norm(np.asarray(ring["center"]) - top), 0.004)
        self.call("tool_set", settings={"radius": 50})
        r = self.call("hover", x=px[0], y=px[1])
        expected = 50.0 * self.cam.pixel_size(top)
        self.assertAlmostEqual(r["ring"]["radius"] / expected, 1.0, delta=0.08)
        o = r["overlay"]
        self.assertGreater(o["vertexCount"], 40)
        ring_pts = self.h.array(o["arrays"]["positions"])
        self.assertLess(abs(np.linalg.norm(ring_pts - np.asarray(r["ring"]["center"]), axis=1).mean() - r["ring"]["radius"]), 1e-5)
        colors = self.h.array(o["arrays"]["colors"])
        self.assertEqual(colors.shape[1], 4)
        # Off the scalp: at the view's target depth.
        r = self.call("hover", x=5.0, y=5.0)
        self.assertTrue(r["ring"]["valid"])
        self.assertFalse(r["ring"]["onScalp"])
        self.assertEqual(self.call("hover_clear")["overlay"]["version"], r["overlay"]["version"] + 1)

    def test_place_comb_and_stretch_match_the_core(self):
        self.begin()
        self.place_guides(5)
        info = self.call("guides_info")
        self.assertEqual(info["guides"], 5)
        self.assertEqual(info["groups"][0]["name"], "primary")
        before = self.guides()
        # Standing 4 cm along the normal (length 0.04 m).
        for pts in before.values():
            self.assertAlmostEqual(float(np.linalg.norm(pts[-1] - pts[0])), 0.04, delta=0.002)
        self.stroke("comb", self.comb_events())
        combed = self.guides()
        moved = [gid for gid in before if np.abs(before[gid] - combed[gid]).max() > 1e-4]
        self.assertGreaterEqual(len(moved), 3)
        lengths = [np.linalg.norm(np.diff(combed[g], axis=0), axis=1).sum() for g in moved]
        np.testing.assert_allclose(lengths, 0.04, rtol=0.02)  # comb keeps lengths
        self.stroke("comb", self.comb_events(dx=200.0), settings={"stretch": True}, ref={"stretch": True})
        stretched = self.guides()
        lengths = [np.linalg.norm(np.diff(stretched[g], axis=0), axis=1).sum() for g in moved]
        self.assertGreater(max(lengths), 0.0405)  # stretch: longer with the drag
        # Draw: a guide shaped like the path from a scalp point.
        a = self.cam.pixel(crown_points(7)[3])
        self.stroke("draw", line(a, (a[0] + 40.0, a[1] + 60.0), 8))
        self.assertEqual(self.call("guides_info")["guides"], 6)
        self.assert_matches_reference()
        u = self.call("undo_state")
        self.assertEqual(u["undoDepth"], 8)
        self.assertEqual(u["undoLabel"], "draw")

    def test_brush_pack_curves_symmetry_and_param_brushes(self):
        """The brush pack through the helper, as the core strokes it: the curves, Direction (with
        topological symmetry) and Noise, and the Density brush wiring its map on its first stroke."""
        self.place_guides(5)
        self.begin()
        curves = {"strengthProfile": [[0, 0], [1, 1]], "falloffProfile": [[0, 1], [1, 0]]}
        r = self.call("tool_set", tool="comb", settings=curves)
        self.assertEqual(r["settings"]["strengthProfile"], [[0, 0], [1, 1]])
        self.assertIn("strengthProfile", r["shows"])
        self.stroke("comb", self.comb_events(), settings=curves,
                    ref={"strength_profile": [[0, 0], [1, 1]], "falloff_profile": [[0, 1], [1, 0]]})
        before = self.guides()
        self.stroke("direction", self.comb_events(dx=160.0), settings={"symmetry": "topology"}, ref={"symmetry": "topology"})
        self.assertEqual(self.call("tool_get")["settings"]["symmetry"], "topology")
        self.stroke("noise", self.comb_events(), settings={"strength": 0.8}, ref={"strength": 0.8})
        after = self.guides()
        moved = [g for g in before if np.abs(after[g] - before[g]).max() > 1e-4]
        self.assertTrue(moved)
        for g in moved:  # turned and jittered, lengths kept
            np.testing.assert_allclose(np.linalg.norm(np.diff(after[g], axis=0), axis=1).sum(),
                                       np.linalg.norm(np.diff(before[g], axis=0), axis=1).sum(), rtol=0.02)
        before = after
        # Density: no brush any more (the error names the new way); New Map > For a setting
        # (map_for_param) makes a white map, links it to Scatter's density (the add-on's node tree
        # follows: graphChanged) and puts the Paint tool on it toward 1, one undo step; Ctrl paints
        # hair away. Asked again, the same map.
        with self.assertRaises(HelperError) as e:
            self.call("tool_set", tool="density")
        self.assertIn("+ New Map > for a setting", str(e.exception))
        self.assertEqual([p["id"] for p in self.call("tool_catalog")["paintable"]][:2], ["density", "width"])
        r = self.call("map_for_param", node="scatter", param="density", kind="vertices")
        self.assertEqual((r["name"], r["kind"]), ("scatter_density", "vertices"))
        self.assertTrue(r["made"])
        self.assertTrue(r["graphChanged"])
        self.assertEqual(r["tool"]["mode"], "paint")
        self.assertEqual(r["tool"]["settings"]["attribute"], "scatter_density")
        self.assertEqual(r["tool"]["settings"]["value"], 1.0)
        self.step("map_for_param", node="scatter", param="density")
        again = self.call("map_for_param", node="scatter", param="density", kind="texture")
        self.assertEqual(again["name"], "scatter_density")
        self.assertFalse(again["made"])
        self.assertEqual(again["undo"]["undoDepth"], r["undo"]["undoDepth"])
        paint = dict(BASE, attribute="scatter_density", paintKind="vertices", value=1.0)
        self.call("tool_set", tool="paint", settings=paint)
        top = self.cam.pixel(crown_points(3)[1])
        events = line((top[0] - 40.0, top[1]), (top[0] + 40.0, top[1]), 9)
        r = self.call("stroke_begin", view=self.cam.view(), x=events[0][0], y=events[0][1], ctrl=True)
        self.assertFalse(r["graphChanged"])
        self.assertEqual(r["mode"], "paint")
        self.call("stroke_drag", points=events[1:])
        self.call("stroke_end")
        self.steps.append({"op": "stroke", "settings": dict(BASE_REF, mode="paint", attribute="scatter_density",
                                                            paint_kind="vertices", value=1.0),
                           "events": events, "ctrl": True, "shift": False})
        self.after.append(self.texts())
        maps = {m["name"]: m for m in self.call("maps_list")["maps"]}
        self.assertIn("scatter_density", maps)
        self.assertEqual(maps["scatter_density"]["kind"], "vertices")
        scatter = [n for n in self.call("get_graph")["graph"]["nodes"] if n["type"] == "Scatter"][0]
        self.assertEqual(scatter["params"]["density"]["colorSet"], "scatter_density")
        values = json.loads(self.texts()["scalpMaps"])["scatter_density"]["values"]
        self.assertLess(min(values), 0.6)
        self.assertEqual(max(values), 1.0)
        for gid, pts in self.guides().items():
            np.testing.assert_array_equal(pts, before[gid])  # a map stroke leaves the guides
        self.assert_matches_reference()
        # Undo: the paint, then the map and its link in one step (graphChanged again).
        self.call("undo")
        self.step("undo")
        r = self.call("undo")
        self.step("undo")
        self.assertTrue(r["graphChanged"])
        self.assertNotIn("scatter_density", [m["name"] for m in self.call("maps_list")["maps"]])
        # Solo: Place with soloNewGuides freezes every other guide in its step; Shift+click a guide
        # solos it; a solo selection freezes the rest; a pick of nothing unfreezes all.
        spot = self.cam.pixel(crown_points(7)[5])
        self.stroke("place", [[spot[0], spot[1], 1.0]], settings={"soloNewGuides": True}, ref={"solo_new_guides": True})
        info = self.call("guides_info")
        self.assertEqual(info["frozen"], info["guides"] - 1)
        g = self.guides()
        first = sorted(g)[0]
        tip = self.cam.pixel(g[first][len(g[first]) // 2])
        self.call("tool_set", tool="comb", settings=dict(BASE))
        self.call("stroke_begin", view=self.cam.view(), x=tip[0], y=tip[1], shift=True)
        end = self.call("stroke_end")
        self.assertEqual(end["released"], "shift_click")
        self.assertTrue(end["committed"])
        self.assertEqual(self.call("undo_state")["undoLabel"], "solo")
        self.assertEqual(self.call("guides_info")["frozen"], info["guides"] - 1)
        r = self.call("select", view=self.cam.view(), lasso=[[2.0, 2.0]], mode="replace", solo=True)
        self.assertTrue(r["froze"])
        self.assertEqual(self.call("guides_info")["frozen"], 0)
        r = self.call("select", view=self.cam.view(), lasso=[tip], mode="replace", solo=True)
        self.assertTrue(r["froze"])
        self.assertGreater(self.call("guides_info")["frozen"], 0)
        self.assert_matches_reference()

    def test_symmetry_in_the_hosts_axes(self):
        self.place_guides(5)
        self.begin()
        # x in Blender is x in the groom; y is the groom's -z (its center negated).
        self.stroke("comb", self.comb_events(), settings={"symmetry": "x", "symmetryCenter": 0.0},
                    ref={"symmetry": "x", "symmetry_center": 0.0})
        center = float(np.float32(-(0.01 * 100.0)))
        self.stroke("smooth", self.comb_events(), settings={"symmetry": "y", "symmetryCenter": 0.01, "strength": 0.6},
                    ref={"symmetry": "z", "symmetry_center": center, "strength": 0.6})
        self.stroke("lift", self.comb_events(), settings={"symmetry": "x", "strength": -0.5},
                    ref={"symmetry": "x", "strength": -0.5})
        # Place with symmetry makes a mirrored pair; a comb on the left one alone moves its mirror
        # image across x = 0 the mirrored way.
        before = self.guides()
        spot = self.cam.pixel(to_blender(HEAD[0])[np.argmin(np.abs(to_blender(HEAD[0])[:, 0] + 0.045) +
                                                             np.abs(to_blender(HEAD[0])[:, 2] - 0.05) +
                                                             np.abs(to_blender(HEAD[0])[:, 1] + 0.04))])
        self.stroke("place", [[spot[0], spot[1], 1.0]], settings={"symmetry": "x"}, ref={"symmetry": "x"})
        g = self.guides()
        pair = [gid for gid in g if gid not in before]
        self.assertEqual(len(pair), 2)
        left, right = sorted(pair, key=lambda gid: g[gid][0][0])
        self.assertLess(abs(g[left][0][0] + g[right][0][0]), 1e-4)
        mid = self.cam.pixel(g[left][len(g[left]) // 2])
        self.stroke("comb", line(mid, (mid[0] - 10.0, mid[1] + 45.0), 8), settings={"symmetry": "x", "radius": 30.0},
                    ref={"symmetry": "x", "screen_radius": 30.0})
        after = self.guides()
        moved_left = after[left] - g[left]
        moved_right = after[right] - g[right]
        self.assertGreater(np.abs(moved_left).max(), 2e-3)
        np.testing.assert_allclose(moved_right[:, 0], -moved_left[:, 0], atol=1.5e-3)
        np.testing.assert_allclose(moved_right[:, 1:], moved_left[:, 1:], atol=1.5e-3)
        self.assert_matches_reference()

    def test_freeze_holds_guides_and_draws_them_frozen(self):
        pixels = self.place_guides(5)
        self.begin()
        # Freeze the two leftmost guides (a small hard brush along their middles).
        g = self.guides()
        order = sorted(g, key=lambda gid: g[gid][0][0])
        mids = [self.cam.pixel(g[gid][len(g[gid]) // 2]) for gid in order[:2]]
        self.stroke("freeze", line(mids[0], mids[1], 10), settings={"radius": 25.0, "falloff": 0.0},
                    ref={"screen_radius": 25.0, "hardness": 1.0})
        info = self.call("guides_info")
        self.assertGreaterEqual(info["frozen"], 2)
        self.assertIn("freeze", info["layers"])
        frozen_layer = [m for m in self.call("maps_list")["maps"] if m["name"] == "freeze"]
        self.assertTrue(frozen_layer[0]["freezeMask"])
        before = self.guides()
        self.stroke("comb", self.comb_events())
        after = self.guides()
        held = [gid for gid in before if np.array_equal(before[gid], after[gid])]
        self.assertGreaterEqual(len(held), 2)
        self.assertLess(len(held), len(before))
        self.assert_matches_reference()
        # Drawn in the theme's frozen blue, per vertex.
        d = self.call("display", settings={"guidePoints": "ends"})
        guides = d["items"]["guides"]
        self.assertTrue(guides["changed"])
        self.assertTrue(guides["perVertexColors"])
        colors = self.h.array(guides["arrays"]["colors"])
        frozen_rows = np.all(np.abs(colors[:, :3] - FROZEN) < 1e-6, axis=1)
        self.assertGreater(frozen_rows.sum(), 0)
        self.assertLess(frozen_rows.sum(), len(colors))
        self.assertGreater(d["items"]["dots"]["count"], 0)
        # Unfreeze all: one undo step, nothing frozen.
        self.call("guides_edit", op="unfreeze_all")
        self.assertEqual(self.call("guides_info")["frozen"], 0)
        self.call("undo")
        self.assertGreaterEqual(self.call("guides_info")["frozen"], 2)

    def test_lasso_select_and_selected_only(self):
        self.place_guides(6)
        self.begin()
        g = self.guides()
        tips = {gid: self.cam.pixel(pts[-1]) for gid, pts in g.items()}
        xs = sorted(p[0] for p in tips.values())
        split = (xs[2] + xs[3]) / 2.0
        ys = [p[1] for p in tips.values()]
        lasso = [[split, min(ys) - 80.0], [self.cam.width - 1.0, min(ys) - 80.0], [self.cam.width - 1.0, max(ys) + 80.0],
                 [split, max(ys) + 80.0]]
        r = self.call("select", view=self.cam.view(), lasso=lasso, mode="replace")
        picked = set(r["picked"])
        right = {gid for gid, p in tips.items() if p[0] > split}
        clearly_left = {gid for gid, pts in g.items() if max(self.cam.pixel(q)[0] for q in pts) < split - 3.0}
        self.assertTrue(right <= picked, (right, picked))
        self.assertFalse(picked & clearly_left)
        self.assertEqual(set(self.call("selection_get")["selection"]), picked)
        # A box (two corners) adds; a click toggles one off.
        one = sorted(right)[0]
        r = self.call("select", lasso=[tips[one]], mode="toggle")
        self.assertNotIn(one, r["selection"])
        r = self.call("select", lasso=[tips[one]], mode="add")
        self.assertIn(one, r["selection"])
        selection = r["selection"]
        self.step("select", ids=selection, soft_radius=0.0)
        before = self.guides()
        self.stroke("comb", self.comb_events(), settings={"selectedOnly": True}, ref={"selected_only": True})
        after = self.guides()
        changed = {gid for gid in before if not np.array_equal(before[gid], after[gid])}
        self.assertTrue(changed)
        self.assertTrue(changed <= set(selection), (changed, selection))
        self.assert_matches_reference()
        # The selection draws over the guides; a display with what the host holds sends nothing again.
        d = self.call("display")
        self.assertGreater(d["items"]["selectedGuides"]["vertexCount"], 0)
        self.assertEqual(d["items"]["selectedGuides"]["color"], [1.0, 1.0, 1.0, 1.0])
        have = {name: item["version"] for name, item in d["items"].items()}
        have["brush"] = d["brush"]["overlay"]["version"]
        again = self.call("display", generation=d["generation"], have=have)
        self.assertFalse(any(item["changed"] for item in again["items"].values()))
        self.assertNotIn("arrays", again["items"]["guides"])
        self.call("selection_set", ids=[])
        again = self.call("display", generation=d["generation"], have=have)
        self.assertTrue(again["items"]["selectedGuides"]["changed"])
        self.assertEqual(again["items"]["selectedGuides"]["vertexCount"], 0)
        self.assertFalse(again["items"]["guides"]["changed"])
        # Another generation (a new helper's builder): everything again.
        fresh = self.call("display", generation="another", have=have)
        self.assertTrue(fresh["items"]["guides"]["changed"])

    def test_part_and_flow_strokes(self):
        self.place_guides(3)
        self.begin()
        pts = crown_points(9)
        a, b = self.cam.pixel(pts[1]), self.cam.pixel(pts[7])
        self.stroke("part", line(a, b, 14), settings={"partName": "middle"}, ref={"part_name": "middle"})
        parts = json.loads(self.after[-1]["partData"])
        self.assertTrue(parts)
        self.assertIn("middle", json.dumps(parts))
        c = self.cam.pixel(pts[2])
        d = self.cam.pixel(pts[6])
        self.stroke("flow", line((c[0], c[1] - 30.0), (d[0], d[1] - 30.0), 14))
        self.assertTrue(json.loads(self.after[-1]["flowData"]))
        self.stroke("flow", [[c[0], c[1] - 30.0, 1.0]], settings={"flowSink": "whorl"}, ref={"flow_sink": "whorl"})
        # Combing parts and flow too.
        self.stroke("comb", self.comb_events(), settings={"targets": "guides,parts,flow"}, ref={"targets": "guides,parts,flow"})
        self.assert_matches_reference()
        # Bound to the cage (the scalp: faces and barycentrics), drawn over the guides.
        self.call("evaluate", density=0.05)
        disp = self.call("display")
        self.assertGreater(disp["items"]["parts"]["vertexCount"], 0)
        self.assertGreater(disp["items"]["flowArrows"]["vertexCount"], 0)
        saved = os.path.join(self.tmp, "parts.groom")
        self.call("save_groom", path=saved)
        with open(saved) as f:
            data = json.load(f)
        self.assertTrue(data.get("parts"))
        self.assertTrue(data.get("flow"))

    def test_paint_vertex_and_texture_maps(self):
        self.place_guides(3)
        made = self.call("map_create", name="mask", kind="vertices", value=0.0)
        self.assertEqual(made["undo"]["undoLabel"], "create map mask")
        tex = self.call("map_create", name="tint", kind="texture", color=True, fill=[0.2, 0.3, 0.4], resolution=64)
        layer = self.call("map_create", name="layer", kind="guides", value=0.5)
        self.assertEqual(layer["kind"], "guides")
        names = {m["name"]: m for m in self.call("maps_list")["maps"]}
        self.assertEqual(names["mask"]["kind"], "vertices")
        self.assertEqual(names["tint"]["channels"], 4)
        self.assertEqual(names["layer"]["kind"], "guides")
        with self.assertRaises(HelperError):
            self.call("map_create", name="mask")  # taken
        with self.assertRaises(HelperError):
            self.call("map_create", name="9bad")
        tint_file = names["tint"]["file"]
        with open(tint_file, "rb") as f:
            tint_before = f.read()
        self.begin()
        pts = crown_points(5)
        events = line(self.cam.pixel(pts[0]), self.cam.pixel(pts[4]), 12)
        self.stroke("paint", events, settings={"attribute": "mask", "paintKind": "vertices", "value": 1.0},
                    ref={"attribute": "mask", "paint_kind": "vertices", "value": 1.0})
        maps = json.loads(self.after[-1]["scalpMaps"])
        self.assertGreater(max(maps["mask"]["values"]), 0.5)
        self.stroke("paint", events, settings={"attribute": "layer", "paintKind": "guides", "value": 1.0},
                    ref={"attribute": "layer", "paint_kind": "guides", "value": 1.0})
        self.assert_matches_reference()
        # A texture map: its file painted, undo and redo writing its tiles back.
        self.call("tool_set", tool="paint", settings={"attribute": "tint", "paintKind": "texture", "paintColor": [1, 0, 0],
                                                      "paintChannels": "rgb"})
        self.call("stroke_begin", view=self.cam.view(), x=events[0][0], y=events[0][1])
        self.call("stroke_drag", points=events[1:])
        end = self.call("stroke_end")
        self.assertTrue(end["committed"])
        with open(tint_file, "rb") as f:
            tint_after = f.read()
        self.assertNotEqual(tint_after, tint_before)
        self.assertTrue(self.call("undo")["done"])
        with open(tint_file, "rb") as f:
            self.assertEqual(f.read(), tint_before)
        self.assertEqual(self.call("undo_state")["redoLabel"], "paint")
        self.call("redo")
        with open(tint_file, "rb") as f:
            self.assertEqual(f.read(), tint_after)
        # The map shown on the scalp, per vertex.
        disp = self.call("display", map={"name": "mask"})
        self.assertGreater(disp["items"]["scalpMap"]["vertexCount"], 0)
        disp = self.call("display", map={"name": "tint", "channel": "rgb"})
        self.assertGreater(disp["items"]["scalpMap"]["vertexCount"], 0)
        # Saved, a file the CLI evaluates with its maps; removed, one undo step.
        self.call("map_remove", name="mask")
        self.assertNotIn("mask", [m["name"] for m in self.call("maps_list")["maps"]])
        self.call("undo")
        self.assertIn("mask", [m["name"] for m in self.call("maps_list")["maps"]])

    def test_a_vertex_map_shows_again_after_undo_and_a_reload(self):
        # The map shown on the scalp follows its values through undo and redo, and a groom file
        # loaded again into the live session (Open Groom File on the same mesh) shows its map at
        # once: the session's tools are made again then, and their answers' shared memory must not
        # take a name the add-on still holds open (it did: "exists already", the old map left shown).
        self.place_guides(3)
        self.call("map_create", name="mask", kind="vertices", value=0.0)

        def shown(have=None):
            # (Every array read, so the client holds each block open as the add-on does.)
            self.call("evaluate", density=0.1)
            r = self.call("display", map={"name": "mask"}, have=have or {}, strandColors={"mode": "random"})
            item = r["items"]["scalpMap"]
            colors = self.h.array(item["arrays"]["colors"]) if item["changed"] else None
            if r["strandColors"]["changed"]:
                self.h.array(r["strandColors"]["colors"])
            return item["version"], colors, r

        blank_version, blank, _ = shown()
        pts = crown_points(5)
        events = line(self.cam.pixel(pts[0]), self.cam.pixel(pts[4]), 12)
        self.stroke("paint", events, settings={"attribute": "mask", "paintKind": "vertices", "value": 1.0})
        painted_version, painted, _ = shown({"scalpMap": blank_version})
        self.assertIsNotNone(painted)
        self.assertGreater(painted.sum(), blank.sum())
        self.call("undo")
        version, colors, _ = shown({"scalpMap": painted_version})
        np.testing.assert_array_equal(colors, blank)
        self.call("redo")
        version, colors, _ = shown({"scalpMap": version})
        np.testing.assert_array_equal(colors, painted)
        path = os.path.join(self.tmp, "mask.groom")
        self.call("save_groom", path=path)
        self.call("undo")  # the session's map blank again; the file's painted
        self.h.array(self.call("hover", view=self.cam.view(), x=events[0][0], y=events[0][1])["overlay"]["arrays"]["positions"])
        self.call("load_groom", path=path)
        version, colors, r = shown({"scalpMap": version})
        self.assertIsNotNone(colors)
        np.testing.assert_array_equal(colors, painted)
        self.assertTrue(r["strandColors"]["changed"])
        self.assertTrue(self.call("hover", view=self.cam.view(), x=events[0][0], y=events[0][1])["ring"]["valid"])
        # The state given back to the live session (set_state): the same.
        state = self.call("get_state", scalp="maps")
        self.call("set_state", groom=state["groom"], textures=state["textures"], tools=state["tools"], keepScalp=True)
        version, colors, _ = shown()
        np.testing.assert_array_equal(colors, painted)

    def test_undo_and_redo_restore_exactly(self):
        initial = self.texts()
        self.place_guides(4)
        self.stroke("comb", self.comb_events())
        self.stroke("twist", self.comb_events(), settings={"strength": 0.5})
        self.call("guides_edit", op="generate_children", count=3)
        info = self.call("guides_info")
        self.assertEqual(info["levels"], 2)
        self.assertEqual(info["guides"], 4 + 12)
        self.after.append(self.texts())
        self.stroke("erase", self.comb_events(), settings={"radius": 25.0})
        history = [initial] + self.after
        state = self.call("undo_state")
        self.assertEqual(state["undoDepth"], len(history) - 1)
        self.assertEqual(state["undoLabel"], "erase")
        for i in range(len(history) - 1, 0, -1):
            self.assertTrue(self.call("undo")["done"])
            self.assertEqual(self.texts(), history[i - 1], "undo to step %d" % (i - 1))
        self.assertFalse(self.call("undo")["done"])
        self.assertFalse(self.call("undo_state")["canUndo"])
        for i in range(1, len(history)):
            self.assertTrue(self.call("redo")["done"])
            self.assertEqual(self.texts(), history[i], "redo to step %d" % i)
        self.assertFalse(self.call("redo")["done"])
        # A new edit drops what could be redone.
        self.call("undo")
        self.call("guides_edit", op="add_group", name="extra")
        self.assertFalse(self.call("undo_state")["canRedo"])
        # Shift+click picks the level of the guide under the cursor.
        g = self.guides()
        px = self.cam.pixel(list(g.values())[0][-1])
        self.call("tool_set", tool="comb", settings={"level": 1})
        self.call("stroke_begin", view=self.cam.view(), x=px[0], y=px[1], shift=True)
        end = self.call("stroke_end")
        self.assertEqual(end["released"], "shift_click")
        self.assertIn(end["level"], (0, 1))
        self.assertIn(end["group"], ("primary", "primary_children"))

    def test_guide_edits_and_hidden_groups(self):
        self.place_guides(4)
        r = self.call("guides_edit", op="generate_children", count=2)
        self.assertEqual(r["group"], "primary_children")
        self.assertEqual(r["added"], 8)
        self.call("selection_set", group="primary_children")
        self.assertEqual(self.call("selection_get")["count"], 8)
        self.call("guides_edit", op="lock")
        self.call("guides_edit", op="freeze")
        self.assertEqual(self.call("guides_info")["frozen"], 8)
        r = self.call("guides_edit", op="rename_group", group="primary_children", name="kids")
        self.assertEqual([g["name"] for g in r["guides"]["groups"]], ["primary", "kids"])
        self.call("set_hidden_groups", groups=["kids"])
        self.assertTrue(self.call("guides_info")["groups"][1]["hidden"])
        disp = self.call("display")
        hidden_count = disp["items"]["guides"]["vertexCount"]
        self.call("set_hidden_groups", groups=[])
        self.assertGreater(self.call("display")["items"]["guides"]["vertexCount"], hidden_count)
        r = self.call("guides_edit", op="delete_guides")
        self.assertEqual(r["removed"], 8)
        self.assertEqual(self.call("selection_get")["count"], 0)
        self.assertEqual(self.call("guides_info")["guides"], 4)
        with self.assertRaises(HelperError):
            self.call("guides_edit", op="remove_group", group="nope")
        with self.assertRaises(HelperError):
            self.call("guides_edit", op="nope")

    def test_strand_colors_and_display_while_stroking(self):
        self.place_guides(5)
        self.call("evaluate", density=0.1)
        r = self.call("evaluate", density=0.1)
        d = self.call("display", strandColors={"mode": "random"})
        c = d["strandColors"]
        self.assertTrue(c["changed"])
        colors = self.h.array(c["colors"])
        self.assertEqual(colors.shape, (r["strandCount"], 3))
        self.assertGreater(colors.std(), 0.01)
        again = self.call("display", generation=d["generation"], have={"strandColors": c["version"]},
                          strandColors={"mode": "random"})
        self.assertFalse(again["strandColors"]["changed"])
        per_point = self.call("display", strandColors={"mode": "density", "perPoint": True})["strandColors"]
        self.assertEqual(self.h.array(per_point["colors"]).shape, (r["pointCount"], 3))
        self.assertEqual(self.call("display", strandColors={"mode": "solid"})["strandColors"]["mode"], "solid")
        # During a stroke the guides show the live preview (no evaluation needed), the strands
        # evaluated at the stroke's density.
        d = self.call("display")
        version = d["items"]["guides"]["version"]
        ev = self.comb_events()
        self.call("tool_set", tool="comb", settings=BASE)
        self.call("stroke_begin", view=self.cam.view(), x=ev[0][0], y=ev[0][1])
        drag = self.call("stroke_drag", points=ev[1:])
        self.assertTrue(drag["changed"])
        self.assertTrue(drag["ring"]["valid"])
        live = self.call("display")
        self.assertTrue(live["stroking"])
        self.assertNotEqual(live["items"]["guides"]["version"], version)
        during = self.call("evaluate", density=0.1, sculptDensity=0.35)
        self.assertLess(during["strandCount"], r["strandCount"])
        self.call("stroke_end")
        after = self.call("evaluate", density=0.1)
        self.assertEqual(after["strandCount"], r["strandCount"])

    def test_state_round_trip(self):
        self.place_guides(4)
        self.stroke("comb", self.comb_events())
        self.call("map_create", name="tint", kind="texture", color=True, fill=[0.5, 0.1, 0.1], resolution=32)
        self.call("map_create", name="mask", kind="vertices", value=0.25)
        self.call("selection_set", all=True)
        self.call("set_hidden_groups", groups=["primary"])
        state = self.call("get_state")
        self.assertEqual(state["format"], "groomlab.groom")
        self.assertEqual([t["name"] for t in state["textures"]], ["tint"])
        groom = json.loads(state["groom"])
        self.assertEqual(groom["upAxis"], "Y")
        self.assertIn("mask", groom["scalp"]["colorSets"])
        ours = self.texts()
        h1, _ = self.evaluate_hash(self.s, 0.1)
        # Another session (another helper's, after a crash): the same groom from the state.
        t = self.h.call("new_session")["session"]
        points, counts, indices, uvs = HEAD
        self.h.call("set_scalp", session=t, **self.h.put(points=to_blender(points), faceCounts=counts, faceIndices=indices, uvs=uvs))
        info = self.h.call("set_state", session=t, groom=state["groom"], textures=state["textures"], tools=state["tools"])
        self.assertEqual(info["hierarchyGuides"], 4)
        theirs = self.h.call("get_state", session=t, texts=True, textures=True)
        for field in ("guideData", "partData", "flowData"):
            self.assertEqual(theirs["texts"][field], ours[field])
        # A vertex map travels as the scalp's color set: per face corner, so a point no face uses
        # (the test head has some) comes back 0, as in every host.
        used = np.unique(HEAD[2])
        theirs_mask = np.asarray(json.loads(theirs["texts"]["scalpMaps"])["mask"]["values"])
        ours_mask = np.asarray(json.loads(ours["scalpMaps"])["mask"]["values"])
        np.testing.assert_array_equal(theirs_mask[used], ours_mask[used])
        self.assertEqual(theirs["textures"][0]["png"], state["textures"][0]["png"])
        self.assertNotEqual(json.loads(theirs["texts"]["scalpMaps"])["tint"]["texture"], json.loads(ours["scalpMaps"])["tint"]["texture"])
        self.assertEqual(theirs["tools"]["hiddenGroups"], ["primary"])
        self.assertEqual(sorted(theirs["tools"]["selection"]), sorted(state["tools"]["selection"]))
        self.h.call("set_time", session=t, frame=1)
        self.call("set_time", frame=1)
        h2, _ = self.evaluate_hash(t, 0.1)
        h1, _ = self.evaluate_hash(self.s, 0.1)
        self.assertEqual(h1, h2)
        # The saved file evaluates to the same bits in groomlab-cli.
        self.assert_matches_cli(t, 0.1, "restored")

    def test_new_groom_fits_and_a_small_state(self):
        # new_groom on a scalp: the default graph in proportion to it (Maya's create_groom(fit=True)),
        # by the factor a preset is fitted with; without a scalp, the graph as authored.
        bare = self.h.call("new_session")["session"]
        self.h.call("new_groom", session=bare)
        scatter = [n["name"] for n in self.call("list_nodes")["nodes"] if n["type"] == "Scatter"][0]
        authored = self.h.call("get_param", session=bare, node=scatter, param="length")["value"]
        fitted = self.call("get_param", node=scatter, param="length")["value"]
        self.h.call("close_session", session=bare)
        factor = self.call("apply_preset", name="hair_basics")["fitFactor"]
        self.assertNotAlmostEqual(factor, 1.0, places=3)
        self.assertAlmostEqual(fitted / authored, factor, places=5)
        # The state the add-on keeps after each edit: no scalp (the host feeds its own) until a map
        # painted on its vertices lives in its color sets.
        self.place_guides(2)
        self.assertFalse(json.loads(self.call("get_state", scalp="maps")["groom"]).get("scalp"))
        self.assertTrue(json.loads(self.call("get_state")["groom"]).get("scalp"))
        self.call("map_create", name="mask", kind="vertices", value=0.25)
        self.assertIn("mask", json.loads(self.call("get_state", scalp="maps")["groom"])["scalp"]["colorSets"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
