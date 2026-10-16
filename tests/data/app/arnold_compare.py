# SPDX-License-Identifier: Apache-2.0
"""Renders a groom in Arnold the way the app's lookdev shows it, for a side-by-side.

    python tests/data/app/arnold_compare.py --groom head.groom --scalp head_scalp.obj \
        --camera "eye x y z target x y z fov 40" --out arnold.png [--kick kick.exe] [--procedural dir]

The scene matches the app's "studio" lighting preset (the key, fill and rim as distant
lights, the generated studio environment as a skydome from a lat-long .hdr written here) and
its "groom" material (standard_hair reading the strands' melanin and melaninRedness channels
through user data, Arnold's defaults otherwise, the roughness the groom's HairColor look
gives; `--pigment rgb` reads the groom's RGB instead, hair_color, over no melanin, as
create_hair_shader does for bands and feather patterns). `--iridescence r,g,b,shift` adds
create_hair_shader's iridescence network (the structural color off a ramp on the camera ray's
angle to root_normal, mixed in by the iridescence user data). The scalp is the OBJ as a
polymesh with the viewport's scalp color (`--hide-scalp`: none). The output is sRGB without a
tone map: render the app's side with --tonemap none to compare.
"""

import colorsys

import argparse
import math
import os
import struct
import subprocess
import sys


def normalized(v):
    n = math.sqrt(sum(c * c for c in v)) or 1.0
    return [c / n for c in v]


def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def light_direction(azimuth, elevation):
    a, e = math.radians(azimuth), math.radians(elevation)
    return normalized([math.sin(a) * math.cos(e), math.sin(e), math.cos(a) * math.cos(e)])


# ---- The studio environment, as look.cpp makes it ---------------------------------------------

def disk(d, center, radius, soft):
    c = max(-1.0, min(1.0, sum(x * y for x, y in zip(d, center))))
    angle = math.acos(c)
    return 1.0 - max(0.0, min(1.0, (angle - radius) / max(soft, 1e-4)))


def mix(a, b, t):
    return [x + (y - x) * t for x, y in zip(a, b)]


def studio_radiance(d):
    up = d[1]
    c = mix([0.08, 0.08, 0.085], [0.35, 0.35, 0.36], 0.5 + 0.5 * up)
    left = disk(d, light_direction(-50.0, 45.0), 0.25, 0.2)
    right = disk(d, light_direction(70.0, 20.0), 0.15, 0.15)
    back = disk(d, light_direction(175.0, 35.0), 0.3, 0.25)
    for k in range(3):
        c[k] += 4.0 * left + 2.0 * right * (1.1 if k == 2 else 0.95) + 1.5 * back
    return c


def env_direction(u, v):
    phi, theta = (u - 0.5) * 2.0 * math.pi, v * math.pi
    return [math.sin(theta) * math.sin(phi), math.cos(theta), -math.sin(theta) * math.cos(phi)]


def write_hdr(path, width, height, radiance):
    """A Radiance .hdr (RGBE, uncompressed) lat-long image of `radiance(direction)`."""
    with open(path, "wb") as f:
        f.write(b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n")
        f.write(("-Y %d +X %d\n" % (height, width)).encode())
        for y in range(height):
            row = bytearray()
            for x in range(width):
                r, g, b = radiance(env_direction((x + 0.5) / width, (y + 0.5) / height))
                m = max(r, g, b)
                if m < 1e-32:
                    row += b"\0\0\0\0"
                    continue
                mant, exp = math.frexp(m)
                scale = mant * 256.0 / m
                row += struct.pack("BBBB", min(255, int(r * scale)), min(255, int(g * scale)), min(255, int(b * scale)), exp + 128)
            f.write(bytes(row))


# ---- The scene --------------------------------------------------------------------------------

def read_obj(path):
    verts, faces = [], []
    for line in open(path):
        parts = line.split()
        if not parts:
            continue
        if parts[0] == "v":
            verts.append([float(p) for p in parts[1:4]])
        elif parts[0] == "f":
            faces.append([int(p.split("/")[0]) - 1 for p in parts[1:]])
    return verts, faces


def light_matrix(toward):
    # Arnold's distant light shines along its -Z: Z toward the light.
    z = normalized(toward)
    up = [1.0, 0.0, 0.0] if abs(z[1]) > 0.9 else [0.0, 1.0, 0.0]
    x = normalized(cross(up, z))
    y = cross(z, x)
    return x + [0.0] + y + [0.0] + z + [0.0] + [0.0, 0.0, 0.0, 1.0]


def matrix_text(m):
    return "\n ".join(" ".join("%.6f" % v for v in m[i:i + 4]) for i in range(0, 16, 4))


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--groom", required=True)
    p.add_argument("--scalp", required=True)
    p.add_argument("--camera", required=True, help='"eye x y z target x y z fov f" as the app prints it')
    p.add_argument("--out", required=True)
    p.add_argument("--size", default="1280x720")
    p.add_argument("--kick", default=r"C:\Program Files\Autodesk\Arnold\maya2022\bin\kick.exe")
    p.add_argument("--procedural", default="", help="the folder holding groomlab_procedural.dll")
    p.add_argument("--aa", type=int, default=6)
    p.add_argument("--melanin", type=float, default=0.35)
    p.add_argument("--redness", type=float, default=0.2)
    p.add_argument("--roughness", type=float, default=0.3)
    p.add_argument("--pigment", default="groom", choices=["groom", "constant", "rgb"],
                   help="groom: the strands' melanin channels through user data (the app's groom material); "
                        "constant: --melanin and --redness for every strand (the app's presets); "
                        "rgb: the groom's RGB (hair_color) as the base color over no melanin")
    p.add_argument("--iridescence", default="", help="r,g,b,shift: FeatherColor's iridescenceColor and iridescenceShift")
    p.add_argument("--specular-tint", type=float, default=1.0, help="HairColor's specular / 0.3, at most 1")
    p.add_argument("--hide-scalp", action="store_true")
    p.add_argument("--lights", default="studio", choices=["studio", "key", "env", "distant"],
                   help="the app's preset: studio, its key alone, or its environment alone; distant: one "
                        "distant light of --distant's")
    p.add_argument("--distant", default="0 0 -1 3.14159265",
                   help='"dx dy dz intensity" for --lights distant: the way the light travels and its intensity '
                        "(a Viewport 2.0 light's direction as groomLabGraphStats -hairShading gives it, at pi times "
                        "its intensity: Maya's default light is 0 -0.2425 -0.9701 at 1, seen down -Z)")
    p.add_argument("--linear-srgb", "--ocio-linear", dest="linear_srgb", action="store_true",
                   help="render in scene-linear Rec.709-sRGB (Arnold's OCIO config) instead of kick's default ACEScg, "
                        "so the constants mean what the app's and the viewport's do (compare with the app's --tonemap none)")
    p.add_argument("--single-scattering", action="store_true",
                   help="no indirect rays (GI depths 0): each strand lit straight by the lights through the hairs in "
                        "their way (Arnold's shadow rays), its lobes alone (a calibration of the lobes and the "
                        "shadows' count apart from the multiple scattering)")
    args = p.parse_args()

    words = args.camera.split()
    eye = [float(w) for w in words[2:5]]
    target = [float(w) for w in words[6:9]]
    fov_vertical = float(words[10])
    width, height = (int(v) for v in args.size.split("x"))
    fov_horizontal = math.degrees(2.0 * math.atan(math.tan(math.radians(fov_vertical) * 0.5) * width / height))

    out_dir = os.path.dirname(os.path.abspath(args.out)) or "."
    hdr = os.path.join(out_dir, "studio_env.hdr")
    write_hdr(hdr, 256, 128, studio_radiance)
    verts, faces = read_obj(args.scalp)

    lights = [] if args.lights == "env" else [("key", -35.0, 35.0, (1.0, 0.97, 0.92), 2.0)]
    if args.lights == "distant":
        d = [float(v) for v in args.distant.split()]
        lights = [("distant", [-d[0], -d[1], -d[2]], (1.0, 1.0, 1.0), d[3])]
    if args.lights == "studio":
        lights += [("fill", 55.0, 10.0, (0.85, 0.9, 1.0), 0.5), ("rim", 160.0, 30.0, (1.0, 1.0, 1.0), 1.5)]
    ass = []
    depths = (0, 0, 0) if args.single_scattering else (2, 4, 8)
    ass.append("options {\n xres %d\n yres %d\n AA_samples %d\n GI_diffuse_depth %d\n GI_specular_depth %d\n"
               " GI_transmission_depth %d\n GI_diffuse_samples 2\n GI_specular_samples 2\n GI_transmission_samples 2\n"
               " outputs \"RGBA RGBA gaussian_filter png_driver\"\n camera \"cam\"\n%s}\n" %
               ((width, height, args.aa) + depths + (" color_manager cm\n" if args.linear_srgb else "",)))
    if args.linear_srgb:
        ocio = os.path.join(os.path.dirname(args.kick), "..", "ocio", "configs", "arnold", "config.ocio")
        ass.append("color_manager_ocio { name cm config \"%s\" color_space_linear \"scene-linear Rec.709-sRGB\" }\n" %
                   os.path.abspath(ocio).replace("\\", "/"))
    ass.append("gaussian_filter { name gaussian_filter }\n")
    ass.append("driver_png { name png_driver filename \"%s\" }\n" % args.out.replace("\\", "/"))
    ass.append("persp_camera {\n name cam\n position %f %f %f\n look_at %f %f %f\n up 0 1 0\n fov %f\n}\n" %
               (eye[0], eye[1], eye[2], target[0], target[1], target[2], fov_horizontal))
    for light in lights:
        name, color, intensity = light[0], light[-2], light[-1]
        toward = light[1] if len(light) == 4 else light_direction(light[1], light[2])
        ass.append("distant_light {\n name %s\n matrix\n %s\n color %f %f %f\n intensity %f\n angle 0.5\n}\n" %
                   (name, matrix_text(light_matrix(toward)), color[0], color[1], color[2], intensity))
    if args.lights in ("studio", "env"):
        # (In linear sRGB the .hdr's values are the rendering space's as they are: Raw.)
        ass.append("image { name studio_image filename \"%s\"%s }\n" %
                   (hdr.replace("\\", "/"), " color_space \"Raw\"" if args.linear_srgb else ""))
        ass.append("skydome_light {\n name sky\n color studio_image\n intensity 0.4\n format \"latlong\"\n camera 0\n}\n")
    # The hair: Arnold's defaults but the pigment, read per strand from the procedural's user data.
    tint = args.specular_tint
    if args.pigment == "rgb" or args.iridescence:
        ass.append("user_data_rgb { name color_data attribute \"hair_color\" default 0.3 0.2 0.12 }\n")
        base, specular = "color_data", "%f %f %f" % (tint, tint, tint)
        if args.iridescence:
            # create_hair_shader's network (__init__.py's _iridescence_network), node for node.
            r, g, b, shift = (float(v) for v in args.iridescence.split(","))
            keys = (0.0, 0.25, 0.5, 0.75, 1.0)  # |cos|: 0 edge-on .. 1 facing
            colors = []
            for at in keys:
                h, s, v = colorsys.rgb_to_hsv(r, g, b)
                colors.append(colorsys.hsv_to_rgb((h + shift * (1.0 - at)) % 1.0, s, v))
            ass.append("state_vector { name ir_ray variable \"Rd\" }\n")
            ass.append("user_data_rgb { name ir_normal attribute \"root_normal\" default 0 1 0 }\n")
            ass.append("dot { name ir_facing input1 ir_ray input2 ir_normal }\n")
            ass.append("abs { name ir_abs input ir_facing }\n")
            ass.append("ramp_rgb {\n name ir_ramp\n type \"custom\"\n input ir_abs.r\n position 5 1 FLOAT %s\n"
                       " color 5 1 RGB %s\n interpolation 5 1 INT 1 1 1 1 1\n}\n" %
                       (" ".join("%g" % k for k in keys), " ".join("%f %f %f" % c for c in colors)))
            ass.append("user_data_float { name ir_amount attribute \"iridescence\" default 0 }\n")
            ass.append("mix_rgba { name ir_base input1 color_data input2 ir_ramp mix ir_amount }\n")
            ass.append("multiply { name ir_tinted input1 ir_ramp input2 %f %f %f }\n" % (tint, tint, tint))
            ass.append("mix_rgba { name ir_specular input1 %f %f %f 1 input2 ir_tinted mix ir_amount }\n" % (tint, tint, tint))
            base, specular = "ir_base", "ir_specular"
        ass.append("standard_hair {\n name hair_shader\n base_color %s\n melanin 0\n specular_tint %s\n"
                   " roughness %f\n ior 1.55\n shift 3\n}\n" % (base, specular, args.roughness))
    elif args.pigment == "groom":
        ass.append("user_data_float { name melanin_data attribute \"melanin\" default %f }\n" % args.melanin)
        ass.append("user_data_float { name redness_data attribute \"melaninRedness\" default %f }\n" % args.redness)
        ass.append("standard_hair {\n name hair_shader\n melanin melanin_data\n melanin_redness redness_data\n"
                   " roughness %f\n ior 1.55\n shift 3\n}\n" % args.roughness)
    else:
        # The app's material presets: one pigment for every strand.
        ass.append("standard_hair {\n name hair_shader\n melanin %f\n melanin_redness %f\n"
                   " roughness %f\n ior 1.55\n shift 3\n}\n" % (args.melanin, args.redness, args.roughness))
    if args.single_scattering:
        # (standard_hair's own extra bounces, past the GI depths, off too)
        ass[-1] = ass[-1].replace("standard_hair {\n", "standard_hair {\n extra_depth 0\n")
    ass.append("groomlab_procedural {\n name groom\n filename \"%s\"\n mode \"ribbon\"\n shader \"hair_shader\"\n}\n" %
               os.path.abspath(args.groom).replace("\\", "/"))
    ass.append("standard_surface { name scalp_shader base 1 base_color 0.36 0.29 0.25 specular 0 }\n")
    if not args.hide_scalp:
        ass.append("polymesh {\n name scalp\n nsides %d 1 UINT\n %s\n vidxs %d 1 UINT\n %s\n vlist %d 1 POINT\n %s\n shader \"scalp_shader\"\n}\n" %
                   (len(faces), " ".join(str(len(f)) for f in faces),
                    sum(len(f) for f in faces), " ".join(" ".join(str(i) for i in f) for f in faces),
                    len(verts), " ".join("%.4f %.4f %.4f" % tuple(v) for v in verts)))
    scene = os.path.join(out_dir, "arnold_compare.ass")
    with open(scene, "w") as f:
        f.write("".join(ass))
    cmd = [args.kick, "-i", scene, "-dw", "-dp", "-nostdin", "-v", "1"]
    if args.procedural:
        cmd += ["-l", args.procedural]
    print(" ".join(cmd))
    sys.exit(subprocess.call(cmd))


if __name__ == "__main__":
    main()
