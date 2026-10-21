# SPDX-License-Identifier: Apache-2.0
"""GroomLab's RenderMan scenes (groomlab.prman_scene, the shared groomlab.render_common), without
Maya: ctest renderman.scene (labels quick, renderman), and with prman renderman.scene_render.

    python test_prman_scene.py <hosts/maya/scripts> [prman]

The RIB text: the camera (Maya's matrix inverted and Z flipped, PxrPerspective's fov across the
shorter side, orthographic), lights (PxrRectLight turned and scaled from Maya's quad, normalize,
Arnold's spread as emissionFocus and gain, PxrDistantLight's solid angle, the dome, light groups and
camera visibility), meshes (triangles with normals per corner, Catmull-Clark faces, archives per
frame for a deforming one), the PxrSurface, light links and shadowless meshes, the grooms' hair
network and procedural call (frame, overrides, motion keys), frames and images, finding prman,
its command line, progress and errors. With prman: the conventions rendered (a floor under each kind
of light at the radiance Arnold gives it, the camera's framing). Standard library only.
"""

import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

SCRIPTS = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "hosts", "maya", "scripts")
PRMAN = sys.argv[2] if len(sys.argv) > 2 and os.path.isfile(sys.argv[2]) else ""
sys.argv = sys.argv[:1]
sys.path.insert(0, SCRIPTS)

# The groomlab package's __init__ needs Maya; these modules do not: a bare package to hold them.
import types  # noqa: E402

_package = types.ModuleType("groomlab")
_package.__path__ = [os.path.join(SCRIPTS, "groomlab")]
sys.modules["groomlab"] = _package

from groomlab import prman_scene as ps, render_common as rc  # noqa: E402

DRIVE = "C:" if os.name == "nt" else ""  # an absolute path's root here: tests run on Windows and Linux

IDENTITY = rc.IDENTITY


def translated(m, t):
    out = list(m)
    out[12:15] = t
    return out


def floats(text):
    return [float(v) for v in text.split()]


# A Maya light (or camera) at (0, 5, 0) facing down: its -Z along -Y.
DOWN = [1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 5, 0, 1]


class CommonTest(unittest.TestCase):
    def test_inverse_rigid(self):
        m = [0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 5, 6, 7, 1]
        inv = rc.inverse_rigid(m)
        # p * m * inv = p, row vectors.
        p = [1.0, 2.0, 3.0]
        q = [sum(p[k] * m[4 * k + j] for k in range(3)) + m[12 + j] for j in range(3)]
        back = [sum(q[k] * inv[4 * k + j] for k in range(3)) + inv[12 + j] for j in range(3)]
        for a, b in zip(back, p):
            self.assertAlmostEqual(a, b)

    def test_at(self):
        samples = [(1, "a"), (3, "b"), (5, "c")]
        self.assertEqual([rc.at(samples, f) for f in (0, 1, 2, 3, 6)], ["a", "a", "a", "b", "c"])
        self.assertEqual(rc.at("x", 4), "x")

    def test_images_and_names(self):
        self.assertEqual(rc.images_for("/r/prman.png", [1]), ["/r/prman.png"])
        self.assertEqual(rc.images_for("/r/prman.png", [1, 2]), ["/r/prman.0001.png", "/r/prman.0002.png"])
        self.assertEqual(rc.safe_name("char:hair|shape.1"), "char_hair_shape_1")


class CameraTest(unittest.TestCase):
    def test_perspective(self):
        # A camera at (0, 0, 10) looking down -Z, 16:9, 60 degrees across: the fov the vertical side's.
        horizontal = math.radians(60.0)
        text = ps.camera_text({"matrix": translated(IDENTITY, (0, 0, 10)), "fov": horizontal}, 1, 1600, 900)
        fov = float(re.search(r'"float fov" \[([0-9.e-]+)\]', text).group(1))
        self.assertAlmostEqual(math.tan(math.radians(fov) / 2), math.tan(horizontal / 2) * 900 / 1600, 6)
        self.assertIn("Scale 1 1 -1\n", text)
        m = floats(re.search(r"ConcatTransform \[(.*?)\]", text).group(1))
        self.assertEqual(m[12:15], [0, 0, -10])
        # Portrait: the horizontal side is the shorter one.
        text = ps.camera_text({"matrix": IDENTITY, "fov": horizontal}, 1, 900, 1600)
        self.assertAlmostEqual(float(re.search(r'"float fov" \[([0-9.e-]+)\]', text).group(1)), 60.0, 5)

    def test_scale_dropped_and_keyed(self):
        scaled = [2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 0, 10, 1]
        text = ps.camera_text({"matrix": [(1, scaled), (2, translated(scaled, (4, 0, 10)))], "fov": [(1, 0.5), (2, 0.5)]},
                              2, 100, 100)
        m = floats(re.search(r"ConcatTransform \[(.*?)\]", text).group(1))
        self.assertEqual(m[:3], [1, 0, 0])
        self.assertEqual(m[12:15], [-4, 0, -10])  # frame 2's

    def test_orthographic(self):
        text = ps.camera_text({"matrix": IDENTITY, "fov": 0.0, "orthographic": True, "ortho_width": 30.0}, 1, 200, 100)
        self.assertIn('Projection "orthographic"', text)
        self.assertIn("ScreenWindow -15 15 -7.5 7.5", text)


class LightTest(unittest.TestCase):
    def test_rect(self):
        m = [3, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 5, 0, 1]
        text = ps.light_text({"kind": "rect", "name": "key", "matrix": m, "color": (1, 0.5, 0.25), "intensity": 4.0,
                              "invisible": True}, 1)
        self.assertIn('Light "PxrRectLight" "key" "float intensity" [4] "color lightColor" [1 0.5 0.25]', text)
        # Maya's quad shines down -Z over -1..1; PxrRectLight down +Z over -0.5..0.5.
        self.assertIn("ConcatTransform [3 0 0 0 0 2 0 0 0 0 1 0 0 5 0 1]\n  Rotate 180 0 1 0\n  Scale 2 2 1\n", text)
        self.assertIn('Attribute "visibility" "int camera" [0]', text)
        self.assertIn('Attribute "grouping" "string membership" ["key"]', text)
        self.assertIn('"float emissionFocus" [0]', text)
        # Normalized: the intensity over the area (2u x 2v).
        text = ps.light_text({"kind": "rect", "name": "key", "matrix": m, "intensity": 24.0, "normalize": True}, 1)
        self.assertIn('"float intensity" [1]', text)
        # Seen by the camera, no shadows.
        text = ps.light_text({"kind": "rect", "name": "k", "matrix": m, "invisible": False, "shadows": False}, 1)
        self.assertNotIn('"int camera" [0]', text)
        self.assertIn('"int enableShadows" [0]', text)

    def test_spread(self):
        self.assertEqual(ps.spread_to_focus(1.0), (0.0, 1.0))
        focus, gain = ps.spread_to_focus(0.5)
        self.assertAlmostEqual(focus, 1.0, delta=0.1)
        self.assertAlmostEqual(gain, 1.5, delta=0.1)
        focus, gain = ps.spread_to_focus(0.35)
        self.assertAlmostEqual(focus, 3.0, delta=0.2)
        self.assertAlmostEqual(gain, 2.6, delta=0.1)
        text = ps.light_text({"kind": "rect", "name": "k", "matrix": IDENTITY, "intensity": 2.0, "spread": 0.5}, 1)
        self.assertIn('"float intensity" [%s]' % ps.number(2.0 * ps.spread_to_focus(0.5)[1]), text)

    def test_dome_and_distant(self):
        text = ps.light_text({"kind": "dome", "name": "sky", "color": (0.05, 0.05, 0.05), "intensity": 2.0}, 1)
        self.assertIn('Light "PxrDomeLight" "sky" "float intensity" [2] "color lightColor" [0.05 0.05 0.05]', text)
        text = ps.light_text({"kind": "direct", "name": "sun", "matrix": DOWN, "intensity": math.pi}, 1)
        # Arnold's intensity is the irradiance; PxrDistantLight's the radiance of its disk.
        radiance = float(re.search(r'"float intensity" \[([0-9.e+-]+)\]', text).group(1))
        solid_angle = 2 * math.pi * (1 - math.cos(math.radians(ps.DISTANT_ANGLE) / 2))
        self.assertAlmostEqual(radiance * solid_angle, math.pi, 4)
        self.assertIn("Rotate 180 0 1 0", text)
        with self.assertRaises(ValueError):
            ps.light_text({"kind": "spot", "name": "x"}, 1)

    def test_keyed(self):
        text = ps.light_text({"kind": "rect", "name": "rim", "intensity": 1.0,
                              "matrix": [(1, IDENTITY), (2, translated(IDENTITY, (1, 0, 0)))]}, 2)
        self.assertIn("ConcatTransform [1 0 0 0 0 1 0 0 0 0 1 0 1 0 0 1]", text)


SQUARE = {"name": "mesh_floorShape", "points": [-1, 0, -1, 1, 0, -1, 1, 0, 1, -1, 0, 1], "triangles": [0, 3, 2, 0, 2, 1],
          "normals": [0, 1, 0], "face_normals": [0] * 6, "polygons": ([4], [0, 3, 2, 1]), "matrix": IDENTITY,
          "material": {"diffuse": (0.25, 0.25, 0.25), "reflect": (0.05, 0.05, 0.05), "roughness": 0.8, "ior": 1.5}}


class MeshTest(unittest.TestCase):
    def test_geometry(self):
        text = ps.mesh_geometry_text(SQUARE, SQUARE["points"])
        self.assertTrue(text.startswith("PointsPolygons [3 3]\n  [0 3 2 0 2 1]\n"), text)
        self.assertEqual(len(floats(re.search(r'"facevarying normal N" \[(.*?)\]', text, re.S).group(1))), 18)
        text = ps.mesh_geometry_text(dict(SQUARE, subdivide=True), SQUARE["points"])
        self.assertTrue(text.startswith('SubdivisionMesh "catmull-clark" [4]\n  [0 3 2 1]\n'), text)
        self.assertIn('["interpolateboundary"] [1 0] [2] []', text)

    def test_material(self):
        text = ps.material_text("m", SQUARE["material"])
        self.assertIn('Bxdf "PxrSurface" "m" "color diffuseColor" [0.25 0.25 0.25]', text)
        self.assertIn('"color specularEdgeColor" [0.05 0.05 0.05]', text)
        self.assertIn('"color specularFaceColor" [0.002 0.002 0.002]', text)  # 0.04 of it: IOR 1.5 facing
        self.assertIn('"float specularRoughness" [0.8]', text)
        self.assertIn('"color specularEdgeColor" [0 0 0]', ps.material_text("m", {"diffuse": (1, 1, 1)}))

    def test_mesh_attributes_and_archives(self):
        folder = tempfile.mkdtemp()
        try:
            mesh = dict(SQUARE, primary_visibility=False, casts_shadows=False, ignore_lights=["rimShape", "rim2Shape"])
            text = ps.mesh_text(mesh, 1, folder)
            self.assertIn('Attribute "visibility" "int camera" [0]', text)
            self.assertIn('Attribute "visibility" "int transmission" [0]', text)
            self.assertIn('Attribute "lighting" "string excludesubset" ["rimShape rim2Shape"]', text)
            self.assertIn('ReadArchive "%s/mesh_floorShape.rib"' % folder.replace("\\", "/"), text)
            self.assertEqual([os.path.basename(p) for p in ps.write_mesh_archives(SQUARE, folder, [1, 2])],
                             ["mesh_floorShape.rib"])
            moving = dict(SQUARE, points=[(1, SQUARE["points"]), (2, [p * 2 for p in SQUARE["points"]])])
            self.assertEqual([os.path.basename(p) for p in ps.write_mesh_archives(moving, folder, [1, 2])],
                             ["mesh_floorShape.0001.rib", "mesh_floorShape.0002.rib"])
            self.assertIn("mesh_floorShape.0002.rib", ps.mesh_text(moving, 2, folder))
            with open(os.path.join(folder, "mesh_floorShape.0002.rib")) as f:
                self.assertIn('"P" [-2 0 -2 2 0 -2', f.read())
        finally:
            shutil.rmtree(folder)


GROOM = {"name": "groom_hairShape", "file": DRIVE + "/shots/hair.groom", "density": 0.5, "width_scale": 1.0,
         "length_scale": 1.2, "tilt": 2.5, "matrix": IDENTITY}


class GroomTest(unittest.TestCase):
    def test_network(self):
        text = ps.hair_network("g", 2.5)
        for handle in ("g_pv_melanin", "g_pigment", "g_rgb", "g_mix_diff", "g_mix_tt", "g_mix_trt", "g_hair"):
            self.assertIn('"%s"' % handle, text)
        self.assertIn('"float specularOffset" [-2.5]', text)
        self.assertIn('"string varname" ["rman_rgbShare"]', text)
        self.assertIn('"float defaultFloat" [1.75]', text)  # the R gain's default, as groom.rib's
        self.assertIn('"color defaultColor" [1 1 1]', text)

    def test_procedural(self):
        text = ps.groom_text(GROOM, 12)
        self.assertIn('Procedural2 "DynamicLoad" "DSOBound"', text)
        self.assertIn(('"string filename" ["' + DRIVE + '/shots/hair.groom"]'), text)
        self.assertIn('"float frame" [12]', text)
        self.assertIn('"float density" [0.5]', text)
        self.assertIn('"float lengthScale" [1.2]', text)
        self.assertNotIn("widthScale", text)  # 1: left out
        self.assertNotIn("motionKeys", text)
        self.assertIn('"string name" ["groom_hairShape"]', text)
        self.assertIn('Bxdf "PxrMarschnerHair" "groom_hairShape_hair"', text)
        text = ps.groom_text(GROOM, 12, ps.MOTION)
        self.assertIn('"float motionStart" [0] "float motionEnd" [0.5] "int motionKeys" [2]', text)


class SceneTest(unittest.TestCase):
    def scene(self, frames, **kwargs):
        camera = {"matrix": [(f, translated(IDENTITY, (f, 0, 30))) for f in frames], "fov": 0.8}
        lights = [{"kind": "rect", "name": "key", "matrix": DOWN, "intensity": 2.0},
                  {"kind": "dome", "name": "sky", "intensity": 0.1}]
        return ps.scene_text(320, 180, frames, DRIVE + "/renders/prman.png", camera, lights, [SQUARE], [GROOM],
                             procedurals=DRIVE + "/GroomLab/procedurals/renderman", folder=DRIVE + "/renders", **kwargs)

    def test_one_frame(self):
        text = self.scene([7], quality="draft")
        self.assertTrue(text.startswith("##RenderMan RIB"))
        self.assertIn(('Option "searchpath" "string procedural" ["' + DRIVE + '/GroomLab/procedurals/renderman:@"]'), text)
        self.assertEqual(text.count("FrameBegin"), 1)
        self.assertIn("FrameBegin 7\nFormat 320 180 1\n", text)
        self.assertIn(('Display "' + DRIVE + '/renders/prman.png" "png" "rgba"'), text)
        samples, minimum, variance, bounces = ps.QUALITY["draft"]
        self.assertIn('"int maxsamples" [%d] "int minsamples" [%d]' % (samples, minimum), text)
        self.assertIn('Integrator "PxrPathTracer" "groomlab_pt" "int maxIndirectBounces" [%d]' % bounces, text)
        self.assertNotIn("Shutter", text)
        world = text[text.index("WorldBegin"):text.index("WorldEnd")]
        self.assertEqual(world.count('Light "'), 2)
        self.assertIn(('ReadArchive "' + DRIVE + '/renders/mesh_floorShape.rib"'), world)
        self.assertIn('"float frame" [7]', world)

    def test_frames(self):
        text = self.scene([1, 2, 3], motion_blur=True)
        self.assertEqual(re.findall(r"FrameBegin (\d+)", text), ["1", "2", "3"])
        self.assertEqual(re.findall(r'Display "(.*?)"', text),
                         [DRIVE + "/renders/prman.0001.png", DRIVE + "/renders/prman.0002.png", DRIVE + "/renders/prman.0003.png"])
        self.assertEqual(text.count("Shutter 0 0.5"), 3)
        self.assertEqual(re.findall(r'"float frame" \[(\d+)\]', text), ["1", "2", "3"])
        # The camera's frame 3 position (it moved along X).
        last = text[text.rindex("FrameBegin"):]
        self.assertIn("0 0 1 0 -3 0 -30 1]", last)


class RunTest(unittest.TestCase):
    def test_command_and_environment(self):
        self.assertEqual(ps.command("prman", "s.rib"), ["prman", "-Progress", "s.rib"])
        self.assertEqual(ps.command("prman", "s.rib", 4), ["prman", "-Progress", "-t:4", "s.rib"])
        prman = os.path.join("C:\\", "Pixar", "RenderManProServer-27.3", "bin", "prman.exe")
        self.assertEqual(ps.environment(prman, {})["RMANTREE"], os.path.abspath(os.path.join("C:\\", "Pixar",
                                                                                         "RenderManProServer-27.3")))

    def test_progress_and_errors(self):
        self.assertEqual(ps.progress_of(""), (None, 0.0))
        self.assertEqual(ps.progress_of("R90000    0%\nR90000   45%\n"), (None, 45.0))
        self.assertEqual(ps.progress_of("R90000   45%\nR90000  100%\n"), (None, 0.0))  # between frames
        self.assertTrue(ps.PrmanJob.is_error("R56001 {ERROR} cannot open file"))
        self.assertTrue(ps.PrmanJob.is_error("X00001 {SEVERE} out of memory"))
        self.assertFalse(ps.PrmanJob.is_error("R90000  45%"))

    def test_find_prman(self):
        folder = tempfile.mkdtemp()
        try:
            tree = os.path.join(folder, "RenderManProServer-27.3")
            os.makedirs(os.path.join(tree, "bin"))
            exe = os.path.join(tree, "bin", ps.EXE)
            open(exe, "w").close()
            for given in (exe, os.path.dirname(exe), tree):
                self.assertEqual(ps.find_prman(given, {}), os.path.abspath(exe))
            self.assertEqual(ps.find_prman(None, {ps.ENV_VAR: tree}), os.path.abspath(exe))
            self.assertEqual(ps.find_prman(None, {"RMANTREE": tree}), os.path.abspath(exe))
            # On the PATH, after the default install (none here).
            default_paths, ps.default_paths = ps.default_paths, lambda: []
            try:
                self.assertEqual(ps.find_prman(None, {"PATH": os.path.dirname(exe)}), os.path.abspath(exe))
                self.assertIsNone(ps.find_prman(None, {}))
            finally:
                ps.default_paths = default_paths
            self.assertEqual(ps.rmantree(exe), os.path.abspath(tree))
        finally:
            shutil.rmtree(folder)


# ---------------------------------------------------------------------------
# Rendered (with prman): the conventions against what Arnold gives

def read_png(path):
    """An 8-bit RGB(A) PNG's pixels: (width, height, rows of (r, g, b))."""
    with open(path, "rb") as f:
        data = f.read()
    at, chunks, width = 8, [], 0
    while at < len(data):
        length, kind = struct.unpack(">I4s", data[at:at + 8])
        body = data[at + 8:at + 8 + length]
        if kind == b"IHDR":
            width, height, depth, color_type = struct.unpack(">IIBB", body[:10])
            channels = {2: 3, 6: 4}[color_type]
        elif kind == b"IDAT":
            chunks.append(body)
        at += 12 + length
    raw = zlib.decompress(b"".join(chunks))
    stride = width * channels
    rows, prev = [], bytearray(stride)
    for y in range(height):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b, c = prev[i], prev[i - channels] if i >= channels else 0
            if kind == 1:
                line[i] = (line[i] + a) & 255
            elif kind == 2:
                line[i] = (line[i] + b) & 255
            elif kind == 3:
                line[i] = (line[i] + (a + b) // 2) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append([tuple(line[x * channels:x * channels + 3]) for x in range(width)])
        prev = line
    return width, height, rows


def linear(v):
    v /= 255.0
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


@unittest.skipUnless(PRMAN, "no prman")
class RenderTest(unittest.TestCase):
    """A white floor under one light, the camera straight above: the floor's radiance in the middle
    against the light's (Arnold's) units."""

    def floor(self, light, name):
        folder = tempfile.mkdtemp()
        try:
            floor = {"name": "floor", "points": [-50, 0, -50, 50, 0, -50, 50, 0, 50, -50, 0, 50],
                     "triangles": [0, 3, 2, 0, 2, 1], "normals": [0, 1, 0], "face_normals": [0] * 6, "matrix": IDENTITY,
                     "material": {"diffuse": (1.0, 1.0, 1.0)}}
            ps.write_mesh_archives(floor, folder, [1])
            camera = {"matrix": [1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 20, 0, 1], "fov": math.radians(20)}
            image = os.path.join(folder, name + ".png")
            text = ps.scene_text(48, 48, [1], image, camera, [light], [floor], quality="preview", folder=folder)
            scene = os.path.join(folder, name + ".rib")
            with open(scene, "w") as f:
                f.write(text)
            result = subprocess.run(ps.command(PRMAN, scene), env=ps.environment(PRMAN), cwd=folder,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, universal_newlines=True,
                                    timeout=300)
            self.assertEqual(result.returncode, 0, result.stdout[-2000:])
            _, _, rows = read_png(image)
            middle = [rows[y][x] for y in range(20, 28) for x in range(20, 28)]
            return sum(linear(p[0]) for p in middle) / len(middle)
        finally:
            shutil.rmtree(folder, ignore_errors=True)  # prman may hold a file a moment longer

    def irradiance_of_square(self, side, distance, radiance, n=100):
        total = 0.0
        for i in range(n):
            for j in range(n):
                x, y = ((i + 0.5) / n - 0.5) * side, ((j + 0.5) / n - 0.5) * side
                r2 = x * x + y * y + distance * distance
                total += radiance * distance * distance / (r2 * r2) * (side / n) ** 2
        return total

    def test_rect_light(self):
        # A 2x2 Maya quad (scale 1) 5 above the floor, radiance 10: the floor's radiance E / pi.
        got = self.floor({"kind": "rect", "name": "key", "matrix": DOWN, "intensity": 10.0}, "rect")
        want = self.irradiance_of_square(2.0, 5.0, 10.0) / math.pi
        self.assertAlmostEqual(got / want, 1.0, delta=0.05)

    def test_distant_and_dome(self):
        # Arnold's distant light of intensity pi/2 lights a white floor to 0.5; a dome of 0.5 too.
        got = self.floor({"kind": "direct", "name": "sun", "matrix": DOWN, "intensity": math.pi / 2}, "distant")
        self.assertAlmostEqual(got, 0.5, delta=0.03)
        got = self.floor({"kind": "dome", "name": "sky", "intensity": 0.5}, "dome")
        self.assertAlmostEqual(got, 0.5, delta=0.03)


if __name__ == "__main__":
    unittest.main(verbosity=2)
