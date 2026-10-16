# SPDX-License-Identifier: Apache-2.0
"""Renders the scanned head's skin in Arnold the way the app's lookdev shows it, side by side.

    python tests/data/app/scan_head/arnold_skin_compare.py --build build/wt [--view front] [--size 1280x720]

For each view (front, key, ear, closeup, or --view all) the app renders the head (no eyes,
the skin preset, --tonemap none) and prints its camera; the same camera, lights and head go
into a .ass with a standard_surface (randomwalk subsurface, the albedo as the base and
subsurface color, the mean free paths the app's skin preset uses, the same F0 and
roughness, the scan's normal and specular maps) and kick renders it (no license: a
watermark, fine). The two frames and their difference go side by side into
%USERPROFILE%/groomlab-deps/assets/renders/arnold_skin_<view>.png, and the script
prints how close they are (the mean absolute difference over the head's pixels, in
8-bit sRGB, and the mean color of the face in each).

The lights follow arnold_compare.py (the app's studio preset as distant lights and the
generated studio environment as a skydome; "key" is the key alone over black).
Head scan: Lee Perry-Smith / Infinite Realities, CC BY 3.0.
"""

import argparse
import math
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
from arnold_compare import light_direction, light_matrix, matrix_text, studio_radiance, write_hdr  # noqa: E402

# The app's skin preset, as standard_surface takes it.
MEAN_FREE_PATH = (0.367, 0.137, 0.068)  # cm: Jensen 2001's skin
SPECULAR_IOR = 1.4                     # F0 0.028: ((n - 1) / (n + 1))^2
ROUGHNESS = 0.45                       # between the app's two lobes (0.4 and 0.65 at a quarter)

VIEWS = {
    # name: (the app's arguments for the view, the lighting preset)
    "front": (["--orbit", "-25", "-12", "--dolly", "0.42"], "studio"),
    "key": (["--orbit", "-25", "-12", "--dolly", "0.42"], "key"),
    "ear": (["--look-at", "-7.5,5,0", "--distance", "14", "--orbit", "150", "0"], "key"),
    "closeup": (["--look-at", "1.5,4.5,10", "--distance", "17", "--orbit", "-20", "-5"], "studio"),
}


def read_obj(path):
    """Positions, normals, uvs and faces as (v, vt, vn) index triples (0-based, -1 when absent)."""
    verts, normals, uvs, faces = [], [], [], []
    for line in open(path):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            verts.append([float(p) for p in parts[1:4]])
        elif parts[0] == "vn":
            normals.append([float(p) for p in parts[1:4]])
        elif parts[0] == "vt":
            uvs.append([float(p) for p in parts[1:3]])
        elif parts[0] == "f":
            face = []
            for p in parts[1:]:
                idx = p.split("/")
                v = int(idx[0]) - 1
                vt = int(idx[1]) - 1 if len(idx) > 1 and idx[1] else -1
                vn = int(idx[2]) - 1 if len(idx) > 2 and idx[2] else -1
                face.append((v, vt, vn))
            faces.append(face)
    return verts, normals, uvs, faces


def polymesh(name, verts, normals, uvs, faces, shader):
    text = ["polymesh {\n name %s\n" % name]
    text.append(" nsides %d 1 UINT\n %s\n" % (len(faces), " ".join(str(len(f)) for f in faces)))
    corners = sum(len(f) for f in faces)
    text.append(" vidxs %d 1 UINT\n %s\n" % (corners, " ".join(" ".join(str(c[0]) for c in f) for f in faces)))
    if normals and all(c[2] >= 0 for f in faces for c in f):
        text.append(" nidxs %d 1 UINT\n %s\n" % (corners, " ".join(" ".join(str(c[2]) for c in f) for f in faces)))
        text.append(" nlist %d 1 VECTOR\n %s\n" % (len(normals), " ".join("%.5f %.5f %.5f" % tuple(n) for n in normals)))
    if uvs and all(c[1] >= 0 for f in faces for c in f):
        text.append(" uvidxs %d 1 UINT\n %s\n" % (corners, " ".join(" ".join(str(c[1]) for c in f) for f in faces)))
        text.append(" uvlist %d 1 VECTOR2\n %s\n" % (len(uvs), " ".join("%.6f %.6f" % tuple(uv) for uv in uvs)))
    text.append(" vlist %d 1 POINT\n %s\n" % (len(verts), " ".join("%.4f %.4f %.4f" % tuple(v) for v in verts)))
    text.append(" smoothing on\n shader \"%s\"\n}\n" % shader)
    return "".join(text)


def read_mtl(obj_path):
    """The maps the OBJ's material library names (map_Kd, map_Bump, map_Ks), as absolute paths."""
    folder = os.path.dirname(os.path.abspath(obj_path))
    maps = {}
    mtl = None
    for line in open(obj_path):
        if line.startswith("mtllib"):
            mtl = os.path.join(folder, line.split(None, 1)[1].strip())
            break
    if mtl and os.path.exists(mtl):
        for line in open(mtl):
            parts = line.split(None, 1)
            if len(parts) == 2 and parts[0].startswith("map_"):
                maps[parts[0]] = os.path.join(folder, parts[1].strip())
    return maps


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--build", default="build/app")
    p.add_argument("--view", default="front", help="front, key, ear, closeup or all")
    p.add_argument("--size", default="1280x720")
    p.add_argument("--frames", type=int, default=64)
    p.add_argument("--aa", type=int, default=5)
    p.add_argument("--kick", default=r"C:\Program Files\Autodesk\Arnold\maya2022\bin\kick.exe")
    p.add_argument("--assets", default=os.path.join(os.environ.get("USERPROFILE", ""), "groomlab-deps", "assets"))
    p.add_argument("--skip-arnold", action="store_true", help="reuse the Arnold frame already rendered")
    p.add_argument("--no-maps", action="store_true", help="the skin preset's flat color, no maps (a check of the shading alone)")
    p.add_argument("--sss-type", default="randomwalk_v2", help="Arnold's subsurface_type: randomwalk_v2 (the ground truth), randomwalk, diffusion")
    p.add_argument("--suffix", default="", help="added to the output names (a variant beside the main comparison)")
    args = p.parse_args()

    root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))
    exe = os.path.join(root, args.build, "hosts", "app", "RelWithDebInfo", "GroomLab.exe")
    head = os.path.join(args.assets, "lee_perry_smith", "head.obj")
    out_dir = os.path.join(args.assets, "renders")
    os.makedirs(out_dir, exist_ok=True)
    width, height = (int(v) for v in args.size.split("x"))
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_ASSUME_STDERR_HAS_CONSOLE="1")
    views = list(VIEWS) if args.view == "all" else [args.view]
    verts, normals, uvs, faces = read_obj(head)
    maps = {} if args.no_maps else read_mtl(head)
    hdr = os.path.join(out_dir, "studio_env.hdr")
    if not os.path.exists(hdr):
        write_hdr(hdr, 256, 128, studio_radiance)

    for view in views:
        view_args, lighting = VIEWS[view]
        app_png = os.path.join(out_dir, "arnold_skin_%s%s_app.png" % (view, args.suffix))
        arnold_png = os.path.join(out_dir, "arnold_skin_%s%s_arnold.png" % (view, args.suffix))
        cmd = [exe, "--prop", head, "--prop-material", "skin", "--lighting", lighting, "--tonemap", "none"] + view_args + [
            "--screenshot", app_png, "--size", args.size, "--frames", str(args.frames)]
        if args.no_maps:
            for name in ("albedoMap", "normalMap", "specularMap"):
                cmd += ["--prop-set", name + "="]
            cmd += ["--prop-set", "detailScale=0"]
        print(" ".join(cmd))
        text = subprocess.run(cmd, env=env, capture_output=True, text=True).stdout
        m = re.search(r"camera eye (\S+) (\S+) (\S+) target (\S+) (\S+) (\S+) fov (\S+)", text)
        if not m:
            print(text)
            sys.exit("the app printed no camera")
        eye = [float(m.group(i)) for i in (1, 2, 3)]
        target = [float(m.group(i)) for i in (4, 5, 6)]
        fov_vertical = float(m.group(7))
        fov_horizontal = math.degrees(2.0 * math.atan(math.tan(math.radians(fov_vertical) * 0.5) * width / height))

        if not args.skip_arnold or not os.path.exists(arnold_png):
            lights = [("key", -35.0, 35.0, (1.0, 0.97, 0.92), 2.0)]
            if lighting == "studio":
                lights += [("fill", 55.0, 10.0, (0.85, 0.9, 1.0), 0.5), ("rim", 160.0, 30.0, (1.0, 1.0, 1.0), 1.5)]
            ass = []
            ass.append("options {\n xres %d\n yres %d\n AA_samples %d\n GI_diffuse_depth 2\n GI_specular_depth 2\n"
                       " GI_sss_samples 3\n GI_diffuse_samples 2\n GI_specular_samples 2\n"
                       " outputs \"RGBA RGBA gaussian_filter png_driver\"\n camera \"cam\"\n color_manager cm\n}\n" % (width, height, args.aa))
            # Rendered in linear sRGB (Arnold's default is ACEScg, which would read the .ass's
            # constants, the lights' colors and the mean free paths, as ACEScg values).
            ocio = os.path.join(os.path.dirname(args.kick), "..", "ocio", "configs", "arnold", "config.ocio")
            ass.append("color_manager_ocio { name cm config \"%s\" color_space_linear \"scene-linear Rec.709-sRGB\" }\n" %
                       os.path.abspath(ocio).replace("\\", "/"))
            ass.append("gaussian_filter { name gaussian_filter }\n")
            ass.append("driver_png { name png_driver filename \"%s\" }\n" % arnold_png.replace("\\", "/"))
            ass.append("persp_camera {\n name cam\n position %f %f %f\n look_at %f %f %f\n up 0 1 0\n fov %f\n}\n" %
                       (eye[0], eye[1], eye[2], target[0], target[1], target[2], fov_horizontal))
            for name, az, el, color, intensity in lights:
                ass.append("distant_light {\n name %s\n matrix\n %s\n color %f %f %f\n intensity %f\n angle 0.5\n}\n" %
                           (name, matrix_text(light_matrix(light_direction(az, el))), color[0], color[1], color[2], intensity))
            if lighting == "studio":
                ass.append("image { name studio_image filename \"%s\" }\n" % hdr.replace("\\", "/"))
                ass.append("skydome_light {\n name sky\n color studio_image\n intensity 0.4\n format \"latlong\"\n camera 0\n}\n")
            # The maps: the albedo as sRGB, the normal and specular maps as data.
            albedo = maps.get("map_Kd")
            normal = maps.get("map_Bump")
            spec = maps.get("map_Ks")
            if albedo:
                ass.append("image { name albedo filename \"%s\" color_space \"sRGB\" }\n" % albedo.replace("\\", "/"))
            if normal:
                ass.append("image { name normal_image filename \"%s\" color_space \"Raw\" }\n" % normal.replace("\\", "/"))
                ass.append("normal_map { name normal_shader input normal_image tangent_space on }\n")
            if spec:
                ass.append("image { name spec_image filename \"%s\" color_space \"Raw\" }\n" % spec.replace("\\", "/"))
            base_color = "albedo" if albedo else "0.75 0.52 0.42"
            ass.append("standard_surface {\n name skin_shader\n base 1\n base_color %s\n specular %s\n specular_roughness %f\n"
                       " specular_IOR %f\n subsurface 1\n subsurface_color %s\n subsurface_radius %f %f %f\n subsurface_scale 1\n"
                       " subsurface_type \"%s\"\n%s}\n" %
                       (base_color, "spec_image" if spec else "1", ROUGHNESS, SPECULAR_IOR, base_color,
                        MEAN_FREE_PATH[0], MEAN_FREE_PATH[1], MEAN_FREE_PATH[2], args.sss_type,
                        " normal normal_shader\n" if normal else ""))
            ass.append(polymesh("head", verts, normals, uvs, faces, "skin_shader"))
            scene = os.path.join(out_dir, "arnold_skin_%s%s.ass" % (view, args.suffix))
            with open(scene, "w") as f:
                f.write("".join(ass))
            cmd = [args.kick, "-i", scene, "-dw", "-dp", "-nostdin", "-v", "1"]
            print(" ".join(cmd))
            if subprocess.call(cmd) != 0:
                sys.exit("kick failed")

        # The side-by-side, and how close they are.
        from PIL import Image, ImageChops, ImageDraw
        a = Image.open(app_png).convert("RGB")
        b = Image.open(arnold_png).convert("RGB").resize(a.size)
        diff = ImageChops.difference(a, b)
        # Over the head alone: where either frame is not the backdrop (black for the key
        # views, the app's gradient otherwise: the Arnold frame's alpha-less black stands in).
        pa, pb = a.load(), b.load()
        total, count, mean_a, mean_b = 0.0, 0, [0.0, 0.0, 0.0], [0.0, 0.0, 0.0]
        for y in range(0, a.height, 2):
            for x in range(0, a.width, 2):
                qb = pb[x, y]
                if max(qb) < 8:
                    continue  # the backdrop in Arnold's frame (black: no environment background)
                qa = pa[x, y]
                total += sum(abs(qa[k] - qb[k]) for k in range(3)) / 3.0
                count += 1
                for k in range(3):
                    mean_a[k] += qa[k]
                    mean_b[k] += qb[k]
        if count:
            mean_a = [v / count for v in mean_a]
            mean_b = [v / count for v in mean_b]
        strip = Image.new("RGB", (a.width * 3, a.height + 24), (0, 0, 0))
        strip.paste(a, (0, 24))
        strip.paste(b, (a.width, 24))
        strip.paste(diff, (2 * a.width, 24))
        d = ImageDraw.Draw(strip)
        d.text((8, 6), "GroomLab app (%s view, %s)" % (view, lighting), fill=(255, 255, 255))
        d.text((a.width + 8, 6), "Arnold standard_surface %s (kick, unlicensed watermark)" % args.sss_type, fill=(255, 255, 255))
        d.text((2 * a.width + 8, 6), "difference  -  head scan: Lee Perry-Smith / Infinite Realities, CC BY 3.0", fill=(255, 255, 255))
        out = os.path.join(out_dir, "arnold_skin_%s%s.png" % (view, args.suffix))
        strip.save(out)
        print("%s: mean |app - arnold| over the head %.1f / 255 (app mean rgb %s, arnold %s); wrote %s" %
              (view, total / max(count, 1), " ".join("%.0f" % v for v in mean_a), " ".join("%.0f" % v for v in mean_b), out))


if __name__ == "__main__":
    main()
