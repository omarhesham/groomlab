# SPDX-License-Identifier: Apache-2.0
"""GroomLab's V-Ray Standalone scenes and the V-Ray for Maya hook, without Maya or V-Ray: ctest
vray.scene (labels quick, vray).

    python test_vray_scene.py <hosts/maya/scripts>

groomlab.vray_scene: values (Maya's row-major matrices as V-Ray Transforms, keys over frames), the
camera's field of view per film fit, lights (rect radiance and normalize, dome, direct's pi), meshes
(triangles, normals, material), light links, settings, vray.exe's command line, image names, its
progress lines and finding vray.exe. groomlab.vray_hook (EXPERIMENTAL: V-Ray for Maya 2024+): the
post-translate script line added and removed, the motion-blur shutter's frames, the plugin names
V-Ray may have made from a groom, and the VRayScene plugin made through a stand-in vray.utils
(create, findByName, findByType, delete, Plugin.set: Chaos's documented calls). Standard library only.
"""

import math
import os
import re
import sys
import tempfile
import unittest

SCRIPTS = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "hosts", "maya", "scripts")
sys.argv = sys.argv[:1]
sys.path.insert(0, SCRIPTS)

# The groomlab package's __init__ needs Maya; its two V-Ray modules do not: a bare package to hold them.
import types  # noqa: E402

_package = types.ModuleType("groomlab")
_package.__path__ = [os.path.join(SCRIPTS, "groomlab")]
sys.modules["groomlab"] = _package

from groomlab import vray_hook, vray_scene as vs  # noqa: E402

DRIVE = "C:" if os.name == "nt" else ""  # an absolute path's root here: tests run on Windows and Linux

IDENTITY = vs.IDENTITY


def translated(m, t):
    out = list(m)
    out[12:15] = t
    return out


class ValuesTest(unittest.TestCase):
    def test_transform_takes_maya_rows_as_axes(self):
        # Maya's matrix rows are the axes' images; V-Ray's Matrix takes them as its columns, in order.
        m = [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1]
        self.assertEqual(vs.transform(m), "Transform(Matrix(Vector(0, 1, 0), Vector(-1, 0, 0), Vector(0, 0, 1)), "
                                          "Vector(5, 6, 7))")

    def test_unscaled(self):
        m, lengths = vs.unscaled([2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0.5, 0, 1, 2, 3, 1])
        self.assertEqual(lengths, [2.0, 3.0, 0.5])
        self.assertEqual(m[:11], [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1])
        self.assertEqual(m[12:15], [1, 2, 3])

    def test_keyed(self):
        self.assertEqual(vs.keyed(2.5, vs.number), "2.5")
        self.assertEqual(vs.keyed([(1, 2.0), (2, 2.0)], vs.number), "2")
        self.assertEqual(vs.keyed([(1, 2.0), (2, 3.0)], vs.number), "interpolate((1, 2), (2, 3))")

    def test_plugin_name(self):
        self.assertEqual(vs.plugin_name("char:hair|shape.1"), "char_hair_shape_1")
        self.assertEqual(vs.plugin_name("1hair"), "_1hair")
        self.assertEqual(vs.plugin_name(""), "node")


class CameraTest(unittest.TestCase):
    def test_film_fit(self):
        # A 36 x 24 mm back with a 50 mm lens: horizontal fit is the film's width whatever the image;
        # vertical fit the film's height times the image aspect; fill the side that covers the image.
        w, h = 36.0 / 25.4, 24.0 / 25.4
        horizontal = 2 * math.atan(18.0 / 50.0)
        self.assertAlmostEqual(vs.horizontal_fov(50, w, h, 16 / 9.0, "horizontal"), horizontal)
        self.assertAlmostEqual(vs.horizontal_fov(50, w, h, 16 / 9.0, "vertical"), 2 * math.atan(12.0 * 16 / 9.0 / 50.0))
        self.assertAlmostEqual(vs.horizontal_fov(50, w, h, 16 / 9.0, "fill"), horizontal)  # 1.5 <= 1.78: width
        self.assertAlmostEqual(vs.horizontal_fov(50, w, h, 1.0, "fill"), 2 * math.atan(12.0 / 50.0))  # square: height
        self.assertAlmostEqual(vs.horizontal_fov(50, w, h, 1.0, "overscan"), horizontal)

    def test_camera_plugin_drops_scale(self):
        text = vs.camera_plugin({"matrix": [2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 10, 1], "fov": 0.5})
        self.assertIn("RenderView groomlab_camera {", text)
        self.assertIn("transform=Transform(Matrix(Vector(1, 0, 0), Vector(0, 1, 0), Vector(0, 0, 1)), Vector(0, 0, 10));",
                      text)
        self.assertIn("fov=0.5;", text)


class LightTest(unittest.TestCase):
    def test_rect(self):
        # Arnold's quad spans -1..1 scaled by the transform: V-Ray's half sizes are the scales.
        m = [3, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 5, 0, 1]
        text = vs.light_plugin({"kind": "rect", "name": "key", "matrix": m, "color": (1, 0.5, 0.25), "intensity": 4.0,
                                "directional": 0.65, "invisible": True})
        for param in ("u_size=3;", "v_size=2;", "intensity=4;", "color=AColor(1, 0.5, 0.25, 1);", "directional=0.65;",
                      "invisible=1;", "affectReflections=1;"):
            self.assertIn(param, text)
        self.assertIn("Vector(0, 5, 0))", text)
        # Normalized (Arnold's default): the intensity spread over the area, 4 u v.
        text = vs.light_plugin({"kind": "rect", "name": "key", "matrix": m, "intensity": 24.0, "normalize": True})
        self.assertIn("intensity=1;", text)

    def test_dome_and_direct(self):
        text = vs.light_plugin({"kind": "dome", "name": "sky", "color": (0.05, 0.05, 0.05), "intensity": 2.0})
        self.assertIn("LightDome sky {", text)
        self.assertIn("dome_spherical=1;", text)
        self.assertIn("intensity=2;", text)
        text = vs.light_plugin({"kind": "direct", "name": "sun", "matrix": IDENTITY, "intensity": math.pi})
        self.assertIn("intensity=1;", text)  # V-Ray's intensity times pi is the irradiance

    def test_keyed_light(self):
        text = vs.light_plugin({"kind": "rect", "name": "rim", "intensity": 1.0,
                                "matrix": [(1, IDENTITY), (2, translated(IDENTITY, (1, 0, 0)))]})
        self.assertIn("transform=interpolate((1, Transform(", text)

    def test_default_lights_light_the_subject(self):
        # Camera at +Z looking down -Z: the key comes from the camera's side, above and to its left.
        lights = vs.default_lights(translated(IDENTITY, (0, 0, 50)))
        self.assertEqual([l["kind"] for l in lights], ["dome", "direct"])
        z = lights[1]["matrix"][8:11]  # toward the light
        self.assertGreater(z[2], 0.3)
        self.assertGreater(z[1], 0.3)
        self.assertLess(z[0], -0.2)


class MeshAndSceneTest(unittest.TestCase):
    def mesh(self, **extra):
        mesh = {"name": "mesh_floor", "points": [0, 0, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1], "triangles": [0, 1, 2, 0, 2, 3],
                "normals": [0, 1, 0], "face_normals": [0] * 6, "matrix": IDENTITY,
                "material": {"diffuse": (0.25, 0.25, 0.25), "reflect": (0.05, 0.05, 0.05), "roughness": 0.8, "ior": 1.5}}
        mesh.update(extra)
        return mesh

    def test_mesh(self):
        text = vs.mesh_plugins(self.mesh())
        for line in ("GeomStaticMesh mesh_floor@mesh {", "BRDFVRayMtl mesh_floor@brdf {", "Node mesh_floor@node {",
                     "geometry=mesh_floor@mesh;", "material=mesh_floor@mtl;", "reflect_glossiness=0.8;",
                     "option_use_roughness=1;", "diffuse=AColor(0.25, 0.25, 0.25, 1);"):
            self.assertIn(line, text)
        vertices = re.search(r"vertices=ListVector\((.*?)\);", text, re.S).group(1)
        self.assertEqual(vertices.count("Vector("), 4)
        self.assertEqual(re.sub(r"\s", "", re.search(r"faces=ListInt\((.*?)\);", text, re.S).group(1)), "0,1,2,0,2,3")

    def test_deforming_mesh_is_keyed(self):
        moved = [0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1]
        text = vs.mesh_plugins(self.mesh(points=[(1, self.mesh()["points"]), (2, moved)]))
        self.assertIn("vertices=interpolate((1, ListVector(", text)

    def test_scene(self):
        lights = [{"kind": "rect", "name": "rim", "matrix": IDENTITY}, {"kind": "dome", "name": "sky"}]
        text = vs.scene_text(320, 180, includes=[os.path.normpath(DRIVE + "/x/groom_hair.vrscene")], camera={"matrix": IDENTITY, "fov": 0.4},
                             lights=lights, meshes=[self.mesh(ignore_lights=["rim"], casts_shadows=False)],
                             quality="final", units={"meters_scale": 0.01, "fps": 24.0})
        self.assertTrue(text.startswith("// GroomLab"))
        self.assertIn(('#include "' + DRIVE + '/x/groom_hair.vrscene"'), text)
        self.assertNotIn("SettingsUnitsInfo", text)  # the groom's file brings its units
        self.assertIn("img_width=320;", text)
        self.assertIn("dmc_maxSubdivs=16;", text)
        self.assertIn("light_doDefaultLights=0;", text)
        self.assertIn("ignored_lights=List(List(rim, mesh_floor@node));", text)
        self.assertIn("ignored_shadow_lights=List(List(rim, mesh_floor@node), List(sky, mesh_floor@node));", text)
        # Without a groom the scene has its own units (centimeters, Y up).
        text = vs.scene_text(64, 36, camera={"matrix": IDENTITY, "fov": 0.4}, units={"meters_scale": 0.01, "fps": 24})
        self.assertIn("SettingsUnitsInfo groomlab_units {", text)
        self.assertIn("meters_scale=0.01;", text)
        self.assertIn("scene_upDir=Vector(0, 1, 0);", text)
        self.assertNotIn("SettingsLightLinker", text)


class RunTest(unittest.TestCase):
    def test_frames(self):
        self.assertEqual(vs.frame_list(1, 5, 2), [1, 3, 5])
        self.assertEqual(vs.frame_list(7), [7])
        self.assertEqual(vs.frames_argument([4]), "4")
        self.assertEqual(vs.frames_argument([1, 2, 3]), "1-3")
        self.assertEqual(vs.frames_argument([1, 3, 5]), "1-5,2")
        self.assertEqual(vs.frames_argument([1, 2, 5]), "1;2;5")

    def test_command_and_images(self):
        cmd = vs.command("vray.exe", "s.vrscene", "out/vray.png", [1, 2])
        for arg in ("-sceneFile=s.vrscene", "-imgFile=out/vray.png", "-display=0", "-autoClose=1", "-frames=1-2"):
            self.assertIn(arg, cmd)
        self.assertEqual(vs.images_for("out/vray.png", [7]), ["out/vray.png"])
        self.assertEqual(vs.images_for("out/vray.png", [1, 12]), ["out/vray.0001.png", "out/vray.0012.png"])

    def test_progress(self):
        log = ("[x] Starting frame 3.\n[x] Rendering image...: 25.00% [ 0h]\n[x] Rendering image...: 50.00% [ 0h]\n"
               "[x] Starting frame 4.\n")
        self.assertEqual(vs.progress_of(log), (4.0, 0.0))
        self.assertEqual(vs.progress_of(log + "Rendering image...: 10.00%"), (4.0, 10.0))
        self.assertEqual(vs.progress_of(""), (None, 0.0))

    def test_find_vray(self):
        folder = tempfile.mkdtemp(prefix="groomlab_vray_")
        exe = os.path.join(folder, "bin", vs.EXE)
        os.makedirs(os.path.dirname(exe))
        open(exe, "w").close()
        self.assertEqual(vs.find_vray(folder, environ={}), exe)                      # an install folder
        self.assertEqual(vs.find_vray(None, environ={vs.ENV_VAR: exe}), exe)        # the environment variable
        self.assertEqual(vs.find_vray(None, environ={"VRAY_PATH": os.path.dirname(exe)}), exe)
        # A setting that is gone falls through to the default install or the PATH (one or the other here).
        installed = vs.find_vray(None, environ={})
        self.assertEqual(vs.find_vray(os.path.join(folder, "nothing"), environ={"PATH": os.path.dirname(exe)}),
                         installed or exe)


class FakePlugin(object):
    def __init__(self, store, name, kind):
        self.store, self._name, self.kind, self.params = store, name, kind, {}

    def name(self):
        return self._name

    def set(self, param, value):
        self.params[param] = value


class FakeUtils(object):
    """A stand-in for vray.utils with the documented calls the hook makes."""

    def __init__(self, nodes=()):
        self.plugins = {n: FakePlugin(self, n, "Node") for n in nodes}
        self.calls = []

    def create(self, kind, name):
        self.calls.append(("create", kind, name))
        self.plugins[name] = FakePlugin(self, name, kind)
        return self.plugins[name]

    def findByName(self, pattern):
        return [p for n, p in self.plugins.items() if n == pattern]

    def findByType(self, kind):
        return [p for p in self.plugins.values() if p.kind == kind]

    def delete(self, plugin):
        self.calls.append(("delete", plugin.name()))
        del self.plugins[plugin.name()]


class HookTest(unittest.TestCase):
    """EXPERIMENTAL (V-Ray for Maya 2024+): the hook's logic without V-Ray."""

    def test_script_line(self):
        user = "print('mine')"
        added = vray_hook.add_line(user)
        self.assertEqual(added, user + "\n" + vray_hook.SCRIPT_LINE)
        self.assertEqual(vray_hook.add_line(added), added)  # once
        self.assertEqual(vray_hook.remove_line(added), user)
        self.assertEqual(vray_hook.add_line(""), vray_hook.SCRIPT_LINE)

    def test_shutter(self):
        self.assertEqual(vray_hook.shutter_frames(12), (12.0, 12.0, 1.0))
        # V-Ray's defaults (duration 1, interval center 0.5): the frame to the next.
        self.assertEqual(vray_hook.shutter_frames(12, True, 1.0, 0.5, 2), (12.0, 13.0, 1.0))
        # Centered on the frame, half a frame open, three keys.
        self.assertEqual(vray_hook.shutter_frames(12, True, 0.5, 0.0, 3), (11.75, 12.25, 0.25))
        self.assertEqual(vray_hook.shutter_frames(12, True, 1.0, 0.5, 1), (12.0, 13.0, 1.0))  # 2 keys at least

    def test_frame_tag(self):
        self.assertEqual(vray_hook.frame_tag(12), "0012")
        self.assertEqual(vray_hook.frame_tag(12.5), "0012_5")
        self.assertEqual(vray_hook.frame_tag(-3), "n0003")

    def test_translated_names(self):
        names = ["|char|hair|hairShape@node", "hairShape@node", "headShape@node", "hairy@node",
                 "groomlab_char_hair@vrayScene"]
        self.assertEqual(vray_hook.translated_by_vray(names, "|char|hair|hairShape"),
                         ["|char|hair|hairShape@node", "hairShape@node"])

    def test_place_groom(self):
        utils = FakeUtils(nodes=["hairShape@node", "headShape@node"])
        name = vray_hook.vrayscene_name("char_hair")
        self.assertEqual(name, "groomlab_char_hair@vrayScene")
        plugin = vray_hook.place_groom(utils, name, os.path.normpath(DRIVE + "/cache/hair.0001.vrscene"), ["hairShape@node"])
        self.assertEqual(plugin.kind, "VRayScene")
        self.assertEqual(plugin.params, {"filepath": DRIVE + "/cache/hair.0001.vrscene", "use_transform": False,
                                         "add_lights": False})
        self.assertEqual(sorted(utils.plugins), ["groomlab_char_hair@vrayScene", "headShape@node"])
        # A second translation (IPR, the next frame) finds the plugin again: no second one.
        again = vray_hook.place_groom(utils, name, DRIVE + "/cache/hair.0002.vrscene", [])
        self.assertIs(again, plugin)
        self.assertEqual([c for c in utils.calls if c[0] == "create"], [("create", "VRayScene", name)])
        self.assertEqual(plugin.params["filepath"], DRIVE + "/cache/hair.0002.vrscene")

    def test_without_maya(self):
        with self.assertRaises(vray_hook.VRayRequired) as caught:
            vray_hook.check()
        self.assertIn(vray_hook.REQUIRED, str(caught.exception))
        self.assertFalse(vray_hook.hook_enabled())
        self.assertFalse(vray_hook.disable_hook())


if __name__ == "__main__":
    unittest.main(verbosity=2)
