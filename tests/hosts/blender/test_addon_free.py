# SPDX-License-Identifier: Apache-2.0
"""The add-on with no key anywhere: GroomLab is free for any use. The helper reports no license and
no watermark; exports (Alembic, USD) write their files; a saved .groom carries no "license" field,
and an old one that does still opens; Support GroomLab (Ko-fi) is in the GroomLab menu and About;
the panels draw and their operators run."""

import json
import os
import sys
import unittest

import bpy

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blender_util as bu  # noqa: E402

pkg, m = bu.enable()
groom, graph, process, menu = m["groom"], m["graph"], m["process"], m["menu"]
DONATE_URL = "https://ko-fi.com/getgroomlab"


class Recorder(bu.FakeLayout):
    """A FakeLayout that also records each operator's text and the properties set on it."""

    def _sub(self, *args, **kwargs):
        return Recorder(self.log)

    row = column = box = split = _sub

    def operator(self, idname, text="", **kwargs):
        bu.FakeLayout.operator(self, idname, **kwargs)

        class Props(object):
            pass
        props = Props()
        self.log.append(("op_text", idname, text, props))
        return props

    def menu(self, name, **kwargs):
        self.log.append(("menu", name))


def drawn(cls):
    log = []

    class Holder(object):
        layout = Recorder(log)

    for name in ("bl_idname", "_entries", "_key", "_conditions"):  # a menu built from the UI definitions
        if hasattr(cls, name):
            setattr(Holder, name, getattr(cls, name))
    cls.draw(Holder, bpy.context)
    return log


class FreeTest(unittest.TestCase):
    def test_free_for_any_use(self):
        for o in list(bpy.data.objects):
            bpy.data.objects.remove(o)
        head = bu.head_object()
        graph.presets()
        self.assertEqual(bpy.ops.groomlab.add_groom("EXEC_DEFAULT", preset="hair_basics", guides=30), {"FINISHED"})
        rt = groom.runtime(head)
        self.assertNotIn("learning", rt.last)
        hello = process.hello()
        self.assertNotIn("license", hello)
        for gone in ("license_status", "license_activate"):
            with self.assertRaises(Exception):
                process.call(gone)
        self.assertGreater(len(head.groomlab.hair.data.curves), 100)
        self.assertIn("about", dir(bpy.ops.groomlab))  # (hasattr is True for any name on bpy.ops)
        self.assertNotIn("license", dir(bpy.ops.groomlab))
        self.assertNotIn("license_popup", dir(bpy.ops.groomlab))
        self.assertFalse(hasattr(bpy.context.window_manager, "groomlab"))

        # Exports with no key: the files are written.
        abc = os.path.join(bu.TMP, "free.abc")
        self.assertEqual(bpy.ops.groomlab.export_alembic(filepath=abc, density=0.1), {"FINISHED"})
        self.assertGreater(os.path.getsize(abc), 0)
        usd = os.path.join(bu.TMP, "free.usda")
        self.assertEqual(bpy.ops.groomlab.export_usd(filepath=usd, density=0.1), {"FINISHED"})
        self.assertGreater(os.path.getsize(usd), 0)

        # A saved groom has no "license"; an old file with "license": "learning" opens as any other.
        path = os.path.join(bu.TMP, "free.groom")
        self.assertEqual(bpy.ops.groomlab.save_groom(filepath=path), {"FINISHED"})
        with open(path) as f:
            saved = json.load(f)
        self.assertNotIn("license", saved)
        saved["license"] = "learning"
        old = os.path.join(bu.TMP, "old_learning.groom")
        with open(old, "w") as f:
            json.dump(saved, f)
        bpy.context.view_layer.objects.active = head
        head.select_set(True)
        self.assertEqual(bpy.ops.groomlab.load_groom(filepath=old, use_file_scalp=True), {"FINISHED"})
        loaded = bpy.context.scene.groomlab.groom
        self.assertEqual(groom.runtime(loaded).errors, [])
        old_abc = os.path.join(bu.TMP, "old_learning.abc")
        groom.export(loaded, bpy.context.scene, "alembic", old_abc, 1, 1, density=0.1)
        self.assertGreater(os.path.getsize(old_abc), 0)

        # Support GroomLab: the GroomLab menu's Help (Maya's) and About, both to Ko-fi.
        self.assertEqual(menu.DONATE_URL, DONATE_URL)
        texts = [e[2] for e in drawn(bpy.types.GROOMLAB_MT_main) if e[0] == "op_text"]
        self.assertNotIn("License...", texts)
        log = drawn(bpy.types.GROOMLAB_MT_main_help)
        support = [e for e in log if e[0] == "op_text" and e[2] == "Support GroomLab"]
        self.assertEqual(len(support), 1)
        self.assertEqual(support[0][1], "wm.url_open")
        self.assertEqual(support[0][3].url, DONATE_URL)
        about = [e for e in drawn(menu.GROOMLAB_OT_about) if e[0] == "op_text" and e[2] == "Support GroomLab"]
        self.assertEqual(len(about), 1)
        self.assertEqual(about[0][3].url, DONATE_URL)
        source = [e for e in drawn(menu.GROOMLAB_OT_about) if e[0] == "op_text" and e[2] == "Source on GitHub"]
        self.assertEqual(len(source), 1)
        self.assertEqual(source[0][3].url, "https://github.com/thekesslereffect/groomlab")

        # The panels draw, with no Learning note; their operators run.
        bpy.context.scene.groomlab.groom = head
        g = head.groomlab
        g.node_index = [n.name for n in g.nodes].index("color")
        groups = sorted({p.group for p in g.params if p.group})
        self.assertTrue(groups)
        self.assertEqual(bpy.ops.groomlab.toggle_group(group=groups[0]), {"FINISHED"})
        self.assertIn(groups[0], g.open_groups)
        g.open_groups = "|".join(groups)
        g.show_advanced = True
        log = bu.draw_panels()
        self.assertIn(("operator", "groomlab.update"), log)
        # Presets and the node list are the Graph Editor's (its Presets menu and sidebar), as in Maya.
        self.assertNotIn(("list", "nodes"), log)
        self.assertFalse([e for e in log if e[0] == "operator" and "preset" in e[1]])
        self.assertFalse(any(kind == "label" and ("Learning" in text or "license" in text.lower()) for kind, text in log))
        graph.operator_specs()
        g.node_index = [n.name for n in g.nodes].index("noise")
        self.assertEqual(bpy.ops.groomlab.node_add(type="Gravity"), {"FINISHED"})
        self.assertEqual(g.nodes[g.node_index].type, "Gravity")
        self.assertEqual(bpy.ops.groomlab.node_remove(), {"FINISHED"})
        self.assertNotIn("Gravity", [n.type for n in g.nodes])
        # A graph with Jiggle: the node is marked, the groom still evaluates.
        graph.apply_preset(head, "wind_and_jiggle")
        groom.flush()
        jiggle = [n for n in g.nodes if n.type == "Jiggle"]
        self.assertTrue(jiggle and "not supported in Blender" in jiggle[0].unsupported)
        self.assertEqual(groom.runtime(head).errors, [])


if __name__ == "__main__":
    bu.run(FreeTest)
