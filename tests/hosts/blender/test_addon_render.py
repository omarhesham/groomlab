# SPDX-License-Identifier: Apache-2.0
"""Final renders of a groom (CPU, small).

- Cycles gets every strand (render density) while the viewport keeps its preview density before
  and after, and the hair shows in the image. GROOMLAB_RENDER_PNG, when set, keeps a copy.
- The hair carries what Maya's Arnold shader reads (per-strand and per-point color, melanin,
  iridescence, root normals) and its material is wired for the HairColor's look (shading.py): the
  RGB (HairColor's, in either mode), the iridescence ramp.
- Width: sparse strands of a known width cover what their projected area says (Cycles' and
  Arnold's coverage agree within 0.5% on the same .groom; overlaps where the strands cross take
  a little off).
- Color: under a neutral light (a sun and a gray world, an ortho camera), a melanin groom and a
  solid-dyed one render the average color kick renders for the same .groom with Maya's shader
  (create_hair_shader, Arnold 7.0, rendering in linear sRGB): the references below were measured
  that way. GROOMLAB_RENDER_KEEP, when set, keeps the renders and the .groom files there."""

import math
import os
import shutil
import sys
import unittest

import bpy
import numpy as np
from mathutils import Vector

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blender_util as bu  # noqa: E402

pkg, m = bu.enable()
groom, graph, process = m["groom"], m["graph"], m["process"]
import importlib  # noqa: E402
shading = importlib.import_module(bu.MODULE + ".shading")
seen = {}
KEEP = os.environ.get("GROOMLAB_RENDER_KEEP", "")

# Arnold's average hair color (linear sRGB) for these grooms in neutral_scene(), rows 50-200 from
# the bottom of a 200 px render (kick's unlicensed watermark sits below), measured with kick when
# Arnold rendered the melanin by aiStandardHair's melanin and the dye as its base color as it is.
# Since then HairColor's RGB is the base color, to the power 1 / ARNOLD_BASE_COLOR_GAMMA (the melanin
# within 5% of Arnold's own in build/probe/haircolor/kick_melanin_vs_base.py, a dye a little lighter
# than before by design): within 10% of these.
ARNOLD = {"melanin": (0.0599, 0.0359, 0.0208), "dyed": (0.3561, 0.2431, 0.1485)}


def count_strands(scene, *args):
    """Runs after the add-on's render_pre handler: what the render gets."""
    for hair in bpy.data.objects:
        if hair.type == "CURVES" and hair.groomlab.role == "HAIR":
            seen["render"] = len(hair.data.curves)


def pixels(path):
    img = bpy.data.images.load(path)
    try:
        return np.array(img.pixels[:], np.float32).reshape(img.size[1], img.size[0], 4)
    finally:
        bpy.data.images.remove(img)


def clear():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    for data in (bpy.data.meshes, bpy.data.hair_curves, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for d in list(data):
            if d.users == 0:
                data.remove(d)


def neutral_scene(scene, ortho=0.30, res=200, samples=32, sun=3.0, world=0.25):
    """The comparison's scene: an ortho camera facing the head (Y), a sun from the upper left in
    front (strength = Arnold's distant light intensity), a uniform gray world (= a skydome's
    color), Cycles with enough bounces for hair, transparent film, Standard view."""
    cam = bpy.data.objects.new("Cam", bpy.data.cameras.new("Cam"))
    scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = ortho
    cam.data.clip_end = 10.0
    cam.location = (0.0, -1.0, 0.02)
    cam.rotation_euler = (math.radians(90.0), 0.0, 0.0)
    scene.camera = cam
    if sun > 0.0:
        light = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        light.data.energy = sun
        light.data.angle = math.radians(0.5)
        az, el = math.radians(-35.0), math.radians(35.0)
        toward = Vector((math.sin(az) * math.cos(el), -math.cos(az) * math.cos(el), math.sin(el)))
        light.rotation_euler = toward.to_track_quat("Z", "Y").to_euler()
        scene.collection.objects.link(light)
    w = bpy.data.worlds.new("Neutral")
    w.use_nodes = True
    w.node_tree.nodes["Background"].inputs[0].default_value = (world, world, world, 1.0)
    scene.world = w
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    scene.cycles.max_bounces = 32
    scene.cycles.transmission_bounces = 16
    scene.cycles.glossy_bounces = 8
    scene.cycles.diffuse_bounces = 4
    scene.render.film_transparent = True
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    scene.view_settings.exposure = 0.0
    scene.view_settings.gamma = 1.0
    scene.render.resolution_x = scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.render.filter_size = 1.5
    scene.render.image_settings.file_format = "OPEN_EXR"
    scene.render.image_settings.color_depth = "32"


def render(scene, name):
    path = os.path.join(KEEP or bu.TMP, name + ".exr")
    scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    return pixels(path)


def new_groom(preset="hair_basics", params=()):
    clear()
    head = bu.head_object()
    graph.presets()
    bpy.ops.groomlab.add_groom("EXEC_DEFAULT", preset=preset, guides=60)
    groom.flush()
    for node, param, value in params:
        graph.set_param(head, node, param, value)
    groom.flush()
    return head


def links_into(node, socket):
    return [l.from_node for l in node.id_data.links if l.to_node == node and l.to_socket.name == socket]


class RenderTest(unittest.TestCase):
    def test_cycles_renders_every_strand(self):
        clear()
        scene = bpy.context.scene
        head = bu.head_object()
        graph.presets()
        bpy.ops.groomlab.add_groom("EXEC_DEFAULT", preset="hair_basics", guides=60)
        head.groomlab.preview_density = 0.05
        groom.flush()
        hair = head.groomlab.hair
        self.assertIsNone(head.groomlab.guides)  # the guides live in the helper (drawn as overlays): renders show the hair
        preview = len(hair.data.curves)
        rt = groom.runtime(head)
        full = process.call("evaluate", session=rt.session, mode="render")["strandCount"]
        groom.sync(head, force_eval=True)
        self.assertLess(preview * 5, full)

        cam_data = bpy.data.cameras.new("Cam")
        cam = bpy.data.objects.new("Cam", cam_data)
        scene.collection.objects.link(cam)
        cam.location = (0.0, -0.55, 0.06)
        cam.rotation_euler = (math.radians(85.0), 0.0, 0.0)
        scene.camera = cam
        sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
        sun.data.energy = 4.0
        sun.rotation_euler = (math.radians(40.0), math.radians(20.0), 0.0)
        scene.collection.objects.link(sun)
        world = scene.world or bpy.data.worlds.new("World")
        scene.world = world
        world.color = (0.3, 0.3, 0.3)
        scene.render.engine = "CYCLES"
        scene.cycles.device = "CPU"
        scene.cycles.samples = 4
        scene.cycles.use_denoising = False
        scene.render.resolution_x = scene.render.resolution_y = 128
        scene.render.resolution_percentage = 100
        scene.render.image_settings.file_format = "PNG"

        bpy.app.handlers.render_pre.append(count_strands)
        try:
            with_hair = os.path.join(bu.TMP, "render_hair.png")
            scene.render.filepath = with_hair
            bpy.ops.render.render(write_still=True)
        finally:
            bpy.app.handlers.render_pre.remove(count_strands)
        self.assertEqual(seen.get("render"), full)
        self.assertEqual(len(hair.data.curves), preview)  # the preview is back
        self.assertEqual(rt.mode, "preview")

        hair.hide_render = True
        bald = os.path.join(bu.TMP, "render_bald.png")
        scene.render.filepath = bald
        bpy.ops.render.render(write_still=True)
        a, b = pixels(with_hair), pixels(bald)
        changed = np.abs(a[..., :3] - b[..., :3]).max(axis=2) > 0.05
        self.assertGreater(changed.mean(), 0.03, "the hair barely shows in the render")
        out = os.environ.get("GROOMLAB_RENDER_PNG")
        if out:
            shutil.copy(with_hair, out)

    def test_the_material_follows_the_hair_color_look(self):
        head = new_groom()
        self.assertEqual(bpy.context.scene.render.hair_type, "STRIP")  # EEVEE renders the strands' width
        hair = head.groomlab.hair
        attrs = {a.name: a for a in hair.data.attributes}
        self.assertEqual((attrs["melanin"].domain, attrs["groomlab_color"].domain), ("CURVE", "CURVE"))
        self.assertNotIn(shading.POINT_COLOR, attrs)  # one color per strand: no per-point colors
        mat = hair.data.materials[0]
        nodes = mat.node_tree.nodes
        outputs = sorted(n.target for n in nodes if n.bl_idname == "ShaderNodeOutputMaterial")
        self.assertEqual(outputs, ["CYCLES", "EEVEE"])
        groups = [n for n in nodes if n.bl_idname == "ShaderNodeGroup"]
        self.assertEqual([g.node_tree.name for g in groups], [shading.GROUP])  # the RGB alone
        gamma = links_into(groups[0], "Color")  # create_hair_shader's Arnold gamma, then the strands' RGB
        self.assertEqual([n.bl_idname for n in gamma], ["ShaderNodeGamma"])
        self.assertAlmostEqual(gamma[0].inputs["Gamma"].default_value, 1.0 / shading.ARNOLD_BASE_COLOR_GAMMA, places=5)
        self.assertEqual([n.attribute_name for n in links_into(gamma[0], "Color")], ["groomlab_color"])
        self.assertAlmostEqual(groups[0].inputs["Roughness"].default_value, 0.3, places=5)  # 0.1 + 0.4 x 0.5
        hairs = [n for n in nodes if n.bl_idname == "ShaderNodeBsdfHairPrincipled"]
        self.assertEqual(sorted(h.parametrization for h in hairs), ["COLOR", "COLOR", "MELANIN"])  # + the highlight
        self.assertAlmostEqual(hairs[0].inputs["Roughness"].default_value, 0.4, places=5)

        # A tip color and iridescence (a FeatherColor after it): the RGB per point, the iridescence
        # ramp over it.
        graph.set_param(head, "color", "tipStrength", 0.6)
        sheen, error = graph.add_node(head, "FeatherColor", after="color")
        self.assertEqual(error, "")
        for param, value in (("pattern", "none"), ("iridescence", 0.5)):
            graph.set_param(head, sheen, param, value)
        groom.flush()
        attrs = {a.name: a for a in hair.data.attributes}
        self.assertEqual(attrs[shading.POINT_COLOR].domain, "POINT")
        self.assertEqual(attrs["iridescence"].domain, "POINT")
        self.assertEqual(attrs[shading.ROOT_NORMAL].domain, "CURVE")
        self.assertNotIn("dyeAmount", attrs)
        nodes = mat.node_tree.nodes
        groups = {n.label: n for n in nodes if n.bl_idname == "ShaderNodeGroup"}
        self.assertEqual(set(groups), {"RGB"})
        ramp = [n for n in nodes if n.bl_idname == "ShaderNodeValToRGB"]
        self.assertEqual(len(ramp), 1)
        self.assertEqual(len(ramp[0].color_ramp.elements), 5)
        names = {n.attribute_name for n in nodes if n.bl_idname == "ShaderNodeAttribute"}
        self.assertTrue({shading.POINT_COLOR, "iridescence", shading.ROOT_NORMAL} <= names, names)

        # A material of the user's is left alone.
        own = bpy.data.materials.new("Mine")
        hair.data.materials[0] = own
        graph.set_param(head, "color", "tipStrength", 0.2)
        groom.flush()
        self.assertEqual(hair.data.materials[0], own)
        self.assertFalse(own.node_tree and any(n.bl_idname == "ShaderNodeGroup" for n in own.node_tree.nodes))

    def test_the_hair_color_group_computes_arnolds_absorption(self):
        """The node group's Cycles output is shading.cycles_color (the fitted absorption as a
        direct color): rendered as an emission filling an ortho camera's frame."""
        clear()
        scene = bpy.context.scene
        bpy.ops.mesh.primitive_plane_add(size=2.0, location=(0.0, 0.0, 0.0), rotation=(math.radians(90.0), 0.0, 0.0))
        plane = bpy.context.active_object
        mat = bpy.data.materials.new("GroupCheck")
        plane.data.materials.append(mat)
        if mat.node_tree is None:
            mat.use_nodes = True
        nodes, links = mat.node_tree.nodes, mat.node_tree.links
        nodes.clear()
        group = nodes.new("ShaderNodeGroup")
        group.node_tree = shading.hair_color_group()
        emit = nodes.new("ShaderNodeEmission")
        links.new(group.outputs["Cycles"], emit.inputs["Color"])
        out = nodes.new("ShaderNodeOutputMaterial")
        links.new(emit.outputs[0], out.inputs["Surface"])
        neutral_scene(scene, ortho=0.5, res=8, samples=1, sun=0.0, world=0.0)
        scene.render.film_transparent = False
        for melanin, redness, color, ra in ((0.4, 0.2, (1.0, 1.0, 1.0), 0.3), (0.0, 0.0, (0.6, 0.3, 0.15), 0.22),
                                            (0.75, 0.8, (0.9, 0.8, 0.5), 0.42)):
            group.inputs["Melanin"].default_value = melanin
            group.inputs["Redness"].default_value = redness
            group.inputs["Color"].default_value = color + (1.0,)
            group.inputs["Roughness"].default_value = ra
            got = render(scene, "group")[4, 4, :3]
            np.testing.assert_allclose(got, shading.cycles_color(melanin, redness, color, ra), rtol=1e-3, atol=1e-6)

    def test_the_strands_render_as_wide_as_they_are(self):
        width = 0.007  # cm: 70 microns, 0.08 px at 600 px over 50 cm
        head = new_groom(params=(("scatter", "density", 1.0), ("clump", "strength", 0.0), ("noise", "amplitude", 0.0),
                                 ("width", "width", width), ("width", "profile", [[0, 1], [1, 1]])))
        scene = bpy.context.scene
        groom.sync(head, force_eval=True, mode="render")
        pos, counts, radius, _ = bu.hair_arrays(head.groomlab.hair)
        np.testing.assert_allclose(radius, width / 100.0 / 2.0, rtol=1e-5)  # cm in, meters' radius out
        head.hide_render = True
        neutral_scene(scene, ortho=0.5, res=600, samples=16, sun=0.0, world=1.0)
        if KEEP:
            groom.save_groom(head, os.path.join(KEEP, "width.groom"))
        alpha = render(scene, "width")[..., 3]
        offsets = np.concatenate([[0], np.cumsum(counts)])
        area = 0.0
        for i in range(len(counts)):
            p, r = pos[offsets[i]:offsets[i + 1]], radius[offsets[i]:offsets[i + 1]]
            area += (np.linalg.norm(np.diff(p[:, [0, 2]], axis=0), axis=1) * (r[:-1] + r[1:])).sum()
        expected = area / 0.25
        self.assertGreater(len(counts), 200)
        ratio = alpha.mean() / expected
        print("coverage %.5f, the strands' projected area %.5f (%.3f)" % (alpha.mean(), expected, ratio))
        self.assertGreater(ratio, 0.95)  # (overlaps take a little off)
        self.assertLess(ratio, 1.02)

    def assert_arnold_color(self, key, params, rtol=0.1):
        head = new_groom(params=params)
        scene = bpy.context.scene
        head.hide_render = True
        neutral_scene(scene)
        if KEEP:
            groom.save_groom(head, os.path.join(KEEP, key + ".groom"))
        img = render(scene, "color_" + key)[50:]
        color = img[..., :3].reshape(-1, 3).sum(0) / img[..., 3].sum()
        print("%s: hair color %s, Arnold %s" % (key, np.round(color, 4), ARNOLD[key]))
        if not any(ARNOLD[key]):
            return
        np.testing.assert_allclose(color, ARNOLD[key], rtol=rtol)

    def test_a_melanin_groom_renders_arnolds_color(self):
        # The color, width and density Arnold was measured with (set here: hair_basics' own may change;
        # Scatter's default density was 200 then).
        self.assert_arnold_color("melanin", (("scatter", "density", 200.0), ("color", "melanin", 0.4), ("color", "redness", 0.2),
                                             ("color", "melaninRandom", 0.0), ("width", "profile", [[0, 1], [1, 0.3]])))

    def test_a_dyed_groom_renders_arnolds_color(self):
        # (A dye renders lighter than it was measured, by design: Arnold now takes it through
        # ARNOLD_BASE_COLOR_GAMMA, as the viewport shades it; blue the most, about 13%.)
        self.assert_arnold_color("dyed", (("scatter", "density", 200.0), ("color", "melanin", 0.0), ("color", "redness", 0.0),
                                          ("color", "dye", [0.6, 0.3, 0.15])), rtol=0.15)


if __name__ == "__main__":
    bu.run(RenderTest)
