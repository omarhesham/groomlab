# SPDX-License-Identifier: Apache-2.0
"""Scales against their baked skin maps, in Arnold: the Dragon scales preset on a sphere 20 cm
across (render_scales.py's), rendered as real geometry through the groomlab procedural and as the
bare sphere with the maps groomlab-cli bake-scales bakes from the same groom file (a
standard_surface reading the color map, a normal_map node reading the normal map, the height map
as displacement on the subdivided sphere), from a mid and a far distance. The point: past some
distance the maps stand in for the scales.

    python tools/bench/scales/render_scale_maps.py [--build build/ninja-maya2022] [--out folder]
        [--kick kick.exe] [--size 960x540] [--aa 5] [--resolution 4096]

Writes <out>/maps_<shot>_<real|baked>.png (default: build/bench/scale_maps), a grid of them all,
<out>/maps_compare.png (rows mid and far, columns real and baked; with Pillow), and the groom
file, maps and scenes in <out>/work; a JPEG copy of the grid is kept in
tools/bench/scales/renders/maps_compare.jpg. Both scenes render in Arnold's default ACEScg: the
color map is read raw and decoded with a 2.2 gamma, so its texels are the numbers the scales'
user data holds, read as the real scales' are.
"""

import argparse
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import render_scales  # noqa: E402

SKIN = (0.04, 0.05, 0.03)  # the body's own color, between the scales (linear)


def scene_head(out, eye, target, fov, size, aa):
    width, height = size
    fov_x = math.degrees(2.0 * math.atan(math.tan(math.radians(fov) * 0.5) * width / height))
    return [
        "options {\n xres %d\n yres %d\n AA_samples %d\n GI_diffuse_depth 1\n GI_specular_depth 2\n"
        " outputs \"RGBA RGBA gaussian_filter png_driver\"\n camera \"cam\"\n}\n" % (width, height, aa),
        "gaussian_filter { name gaussian_filter }\n",
        "driver_png { name png_driver filename \"%s\" }\n" % out.replace("\\", "/"),
        "persp_camera {\n name cam\n position %f %f %f\n look_at %f %f %f\n up 0 1 0\n fov %f\n}\n" % (eye + target + (fov_x,)),
        "distant_light {\n name key\n direction -0.5 -0.7 -0.45\n intensity 2.5\n angle 2\n}\n",
        "distant_light {\n name fill\n direction 0.8 -0.2 -0.5\n intensity 0.4\n color 0.8 0.9 1\n}\n",
        "skydome_light {\n name sky\n color 0.35 0.37 0.4\n intensity 1\n camera 0\n}\n",
    ]


def body(mesh, shader, extra=""):
    points, tris, uvs = mesh
    return ("polymesh {\n name body\n nsides %d 1 UINT\n %s\n vidxs %d 1 UINT\n %s\n vlist %d 1 VECTOR\n %s\n"
            " uvlist %d 1 VECTOR2\n %s\n uvidxs %d 1 UINT\n %s\n smoothing on\n shader \"%s\"\n%s}\n"
            % (len(tris), " ".join("3" for _ in tris), 3 * len(tris), " ".join("%d %d %d" % t for t in tris),
               len(points), " ".join("%.5f %.5f %.5f" % p for p in points),
               len(uvs), " ".join("%.6f %.6f" % uv for uv in uvs), 3 * len(tris),
               " ".join(str(i) for i in range(3 * len(tris))), shader, extra))


def kick(kick_exe, procedurals, scene_path, lines):
    with open(scene_path, "w") as f:
        f.write("".join(lines))
    cmd = [kick_exe, "-i", scene_path, "-dw", "-dp", "-nostdin", "-v", "2", "-l", procedurals]
    print(" ".join(cmd))
    return subprocess.call(cmd)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--build", default=os.path.join(render_scales.REPO, "build", "ninja-maya2022"))
    p.add_argument("--out", default=os.path.join(render_scales.REPO, "build", "bench", "scale_maps"))
    p.add_argument("--kick", default=r"C:\Program Files\Autodesk\Arnold\maya2022\bin\kick.exe")
    p.add_argument("--size", default="960x540")
    p.add_argument("--aa", type=int, default=5)
    p.add_argument("--resolution", type=int, default=4096)
    args = p.parse_args()
    procedurals = os.path.join(args.build, "hosts", "arnold", "procedurals", "RelWithDebInfo")
    cli = os.path.join(args.build, "engine", "cli", "RelWithDebInfo", "groomlab-cli.exe")
    work = os.path.join(args.out, "work")
    os.makedirs(work, exist_ok=True)
    size = tuple(int(v) for v in args.size.split("x"))

    ball = render_scales.sphere(10.0, 64, 128)
    groom = os.path.join(work, "sphere.groom")
    render_scales.write_groom(groom, ball, render_scales.preset_graph())
    prefix = os.path.join(work, "sphere_")
    out = subprocess.run([cli, "bake-scales", groom, "-o", prefix, "--resolution", str(args.resolution),
                          "--background", "%g,%g,%g" % SKIN], stdout=subprocess.PIPE, universal_newlines=True)
    print(out.stdout)
    if out.returncode != 0:
        sys.exit(1)
    max_height = float(out.stdout.split("max height ")[1].split(",")[0])
    maps = {m: (prefix + m + ".png").replace("\\", "/") for m in ("ScaleNormal", "ScaleHeight", "ScaleColor")}

    shots = [("mid", (14.0, 18.0, 34.0), 38.0), ("far", (42.0, 54.0, 102.0), 38.0)]
    failed = 0
    images = []
    for name, eye, fov in shots:
        real = os.path.join(args.out, "maps_%s_real.png" % name)
        lines = scene_head(real, eye, (0.0, 0.0, 0.0), fov, size, args.aa)
        lines += ["standard_hair { name hair_shader }\n",
                  "groomlab_procedural {\n name groom\n filename \"%s\"\n shader \"hair_shader\"\n}\n" % groom.replace("\\", "/"),
                  "standard_surface { name skin_shader base 1 base_color %g %g %g specular 0.2 }\n" % SKIN,
                  body(ball, "skin_shader")]
        failed += kick(args.kick, procedurals, os.path.join(work, "maps_%s_real.ass" % name), lines) != 0
        baked = os.path.join(args.out, "maps_%s_baked.png" % name)
        lines = scene_head(baked, eye, (0.0, 0.0, 0.0), fov, size, args.aa)
        # The plates' own shader (setDefaultScaleShader's): base 1, specular 0.6 at roughness 0.35;
        # here the color map is the scales over the skin, the normal map their relief.
        lines += [# The color map's texels decoded to the numbers the scales' user data holds (both then read
                  # as the rendering space's, as the real scales' are; sRGB's curve as a 2.2 gamma).
                  "image { name color_texels filename \"%s\" color_space \"Raw\" }\n" % maps["ScaleColor"],
                  "color_correct { name color_map input color_texels gamma 0.4545 }\n",
                  "image { name normal_image filename \"%s\" color_space \"Raw\" }\n" % maps["ScaleNormal"],
                  "image { name height_image filename \"%s\" color_space \"Raw\" }\n" % maps["ScaleHeight"],
                  "normal_map { name normals input normal_image }\n",
                  "standard_surface { name baked_shader base 1 base_color color_map specular 0.6 specular_roughness 0.35"
                  " normal normals }\n",
                  body(ball, "baked_shader", " subdiv_type \"catclark\"\n subdiv_iterations 3\n disp_map \"height_image\"\n"
                       " disp_height %g\n disp_zero_value 0\n disp_padding %g\n" % (max_height, max_height))]
        failed += kick(args.kick, procedurals, os.path.join(work, "maps_%s_baked.ass" % name), lines) != 0
        images.append((name, real, baked))

    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow not found: no grid")
        sys.exit(1 if failed else 0)
    w, h = size
    grid = Image.new("RGB", (2 * w, 2 * h))
    for row, (name, real, baked) in enumerate(images):
        for col, (path, label) in enumerate(((real, "real scales"), (baked, "baked maps"))):
            if not os.path.exists(path):
                continue
            grid.paste(Image.open(path).convert("RGB"), (col * w, row * h))
            ImageDraw.Draw(grid).text((col * w + 12, row * h + 10), "%s, %s" % (label, name), fill=(235, 235, 235))
    grid.save(os.path.join(args.out, "maps_compare.png"))
    grid.save(os.path.join(HERE, "renders", "maps_compare.jpg"), quality=90)
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
