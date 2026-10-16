# SPDX-License-Identifier: Apache-2.0
"""Renders a groom in RenderMan through the GroomLab RenderMan procedural, the scene of
arnold_compare.py, for a side-by-side with Arnold (and the calibration of renderman_look.h).

    python tests/data/app/renderman_compare.py --groom head.groom --scalp head_scalp.obj \
        --camera "eye x y z target x y z fov 40" --out renderman.png [--prman prman.exe] [--procedural dir]

The same camera, lights (the app's "studio" preset: the key, fill and rim as distant lights, the
studio environment as a dome from a lat-long .hdr written here; or the key alone, the
environment alone, one distant light) and scalp as arnold_compare.py, in scene-linear
Rec.709-sRGB (standalone prman's rendering space, arnold_compare.py's --linear-srgb). The hair:
the recommended network (hosts/renderman/examples/groom.rib), PxrHairColor and PxrMarschnerHair
reading the procedural's primvars (`--pigment groom`), or the RGB through PxrHairColor's
Artistic mode (`--pigment rgb`). Writes the linear EXR beside the PNG (sRGB, no tone map).
Arnold's distant lights' intensities are irradiance; PxrDistantLight's is the radiance of its
disk, so each is divided by the disk's solid angle.
"""

import argparse
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from arnold_compare import light_direction, normalized, cross, read_obj, studio_radiance, write_hdr  # noqa: E402

RMANTREE = os.environ.get("RMANTREE", r"C:\Program Files\Pixar\RenderManProServer-27.3")


def rib_path(path):
    return os.path.abspath(path).replace("\\", "/")


def hair_network(pigment):
    """The hosts/renderman/examples/groom.rib network: PxrHairColor (the pigment, Physical; the
    RGB, Artistic) mixed by rman_rgbShare into PxrMarschnerHair, its lobes from the primvars."""
    def primvar(name, var, kind):
        return 'Pattern "PxrPrimvar" "%s" "string varname" ["%s"] "string type" ["%s"]\n' % (name, var, kind)
    rib = [primvar("pv_melanin", "rman_melanin", "float"), primvar("pv_redness", "rman_redness", "float"),
           primvar("pv_dye", "rman_dye", "color"), primvar("pv_color", "rman_color", "color"),
           primvar("pv_share", "rman_rgbShare", "float"), primvar("pv_gainR", "rman_specularGainR", "float"),
           primvar("pv_coneR", "rman_coneAngleR", "float"), primvar("pv_coneTT", "rman_coneAngleTT", "float"),
           primvar("pv_coneTRT", "rman_coneAngleTRT", "float"),
           primvar("pv_opacity", "opacity", "float").replace("\n", ' "float defaultFloat" [1]\n')]
    rib.append('Pattern "PxrHairColor" "pigment" "int mode" [0] "reference float melanin" ["pv_melanin:resultF"]'
               ' "reference float redness" ["pv_redness:resultF"] "reference color dye" ["pv_dye:resultRGB"]'
               ' "float randomMelanin" [0] "float strayDensity" [0]\n')
    rib.append('Pattern "PxrHairColor" "rgb" "int mode" [1] "reference color Color" ["pv_color:resultRGB"]'
               ' "color randomColor" [0 0 0] "float strayDensity" [0]\n')
    lobes = {"pigment": "0", "rgb": "1", "groom": None}[pigment]
    for out in ("resultDiff", "resultTT", "resultTRT"):
        mixer = '"float mixer" [%s]' % lobes if lobes else '"reference float mixer" ["pv_share:resultF"]'
        rib.append('Pattern "PxrMix" "mix_%s" "reference color color1" ["pigment:%s"] "reference color color2" ["rgb:%s"] %s\n'
                   % (out, out, out, mixer))
    rib.append('Bxdf "PxrMarschnerHair" "hair" "reference color diffuseColor" ["mix_resultDiff:resultRGB"]'
               ' "reference color specularColorTT" ["mix_resultTT:resultRGB"]'
               ' "reference color specularColorTRT" ["mix_resultTRT:resultRGB"]'
               ' "reference float specularGainR" ["pv_gainR:resultF"]'
               ' "reference float specularConeAngleR" ["pv_coneR:resultF"]'
               ' "reference float specularConeAngleTT" ["pv_coneTT:resultF"]'
               ' "reference float specularConeAngleTRT" ["pv_coneTRT:resultF"]'
               ' "reference float presence" ["pv_opacity:resultF"]'
               ' "float specularGainTT" [2] "float specularGainTRT" [1.2]'
               ' "float specularOffset" [-3] "float specularIor" [1.55]\n')
    return "".join(rib)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--groom", required=True)
    p.add_argument("--scalp", default="")
    p.add_argument("--camera", required=True, help='"eye x y z target x y z fov f" as the app prints it')
    p.add_argument("--out", required=True, help="the PNG (the EXR beside it)")
    p.add_argument("--size", default="1280x720")
    p.add_argument("--prman", default=os.path.join(RMANTREE, "bin", "prman.exe"))
    p.add_argument("--procedural", default="", help="the folder holding groomlab_renderman.dll")
    p.add_argument("--samples", type=int, default=64)
    p.add_argument("--density", type=float, default=1.0)
    p.add_argument("--pigment", default="groom", choices=["groom", "pigment", "rgb"],
                   help="groom: by rman_rgbShare, as the procedural sets it; pigment or rgb: that look alone")
    p.add_argument("--hide-scalp", action="store_true")
    p.add_argument("--lights", default="studio", choices=["studio", "key", "env", "distant"])
    p.add_argument("--distant", default="0 0 -1 3.14159265", help='"dx dy dz intensity" (arnold_compare.py\'s)')
    args = p.parse_args()

    words = args.camera.split()
    eye = [float(w) for w in words[2:5]]
    target = [float(w) for w in words[6:9]]
    fov_vertical = float(words[10])
    width, height = (int(v) for v in args.size.split("x"))
    out_dir = os.path.dirname(os.path.abspath(args.out)) or "."
    exr = os.path.splitext(os.path.abspath(args.out))[0] + ".exr"

    rib = ['##RenderMan RIB\nversion 3.04\n']
    if args.procedural:
        rib.append('Option "searchpath" "string procedural" ["%s:@"]\n' % rib_path(args.procedural))
    rib.append('Format %d %d 1\n' % (width, height))
    rib.append('Display "%s" "openexr" "rgba"\n' % exr.replace("\\", "/"))
    rib.append('Hider "raytrace" "int maxsamples" [%d] "int minsamples" [%d]\n' % (args.samples, max(4, args.samples // 4)))
    rib.append('Integrator "PxrPathTracer" "pt" "int maxIndirectBounces" [10]\n')
    # (PxrPerspective's fov spans the image's shorter side: the vertical one here.)
    rib.append('Projection "PxrPerspective" "float fov" [%f]\n' % (fov_vertical if width >= height else
               math.degrees(2.0 * math.atan(math.tan(math.radians(fov_vertical) * 0.5) * width / height))))
    # The camera: x right, y up, z forward (RenderMan's left-handed camera space).
    f = normalized([t - e for t, e in zip(target, eye)])
    r = normalized(cross(f, [0.0, 1.0, 0.0]))
    u = cross(r, f)
    dot = lambda a, b: sum(x * y for x, y in zip(a, b))
    m = [r[0], u[0], f[0], 0, r[1], u[1], f[1], 0, r[2], u[2], f[2], 0, -dot(r, eye), -dot(u, eye), -dot(f, eye), 1]
    rib.append('Transform [%s]\n' % " ".join("%.6f" % v for v in m))
    rib.append('WorldBegin\n')

    lights = [] if args.lights == "env" else [("key", -35.0, 35.0, (1.0, 0.97, 0.92), 2.0)]
    if args.lights == "distant":
        d = [float(v) for v in args.distant.split()]
        lights = [("distant", [-d[0], -d[1], -d[2]], (1.0, 1.0, 1.0), d[3])]
    if args.lights == "studio":
        lights += [("fill", 55.0, 10.0, (0.85, 0.9, 1.0), 0.5), ("rim", 160.0, 30.0, (1.0, 1.0, 1.0), 1.5)]
    angle = 0.5
    solid_angle = 2.0 * math.pi * (1.0 - math.cos(math.radians(angle) * 0.5))
    for light in lights:
        name, color, intensity = light[0], light[-2], light[-1]
        toward_light = light[1] if len(light) == 4 else light_direction(light[1], light[2])
        travel = [-c for c in toward_light]  # PxrDistantLight shines down its +Z
        z = normalized(travel)
        x = normalized(cross([0.0, 1.0, 0.0], z) if abs(z[1]) < 0.99 else cross([1.0, 0.0, 0.0], z))
        y = cross(z, x)
        mat = x + [0] + y + [0] + z + [0] + [0, 0, 0, 1]
        rib.append('AttributeBegin\n  ConcatTransform [%s]\n' % " ".join("%.6f" % v for v in mat))
        rib.append('  Light "PxrDistantLight" "%s" "float intensity" [%f] "color lightColor" [%f %f %f]'
                   ' "float angleExtent" [%f]\nAttributeEnd\n' % (name, intensity / solid_angle, color[0], color[1],
                                                                 color[2], angle))
    if args.lights in ("studio", "env"):
        hdr = os.path.join(out_dir, "studio_env.hdr")
        write_hdr(hdr, 256, 128, studio_radiance)
        tex = os.path.join(out_dir, "studio_env.tex")  # PxrDomeLight reads textures: txmake's
        subprocess.check_call([os.path.join(os.path.dirname(args.prman), "txmake"), "-envlatl", hdr, tex])
        # PxrDomeLight's lat-long is Z-up: turned as Arnold's skydome maps it (checked on the bald
        # scalp under the environment alone), the same radiance at 0.4.
        rib.append('AttributeBegin\n  Attribute "visibility" "int camera" [0]\n  Rotate -90 1 0 0\n  Rotate 90 0 0 1\n')
        rib.append('  Light "PxrDomeLight" "sky" "float intensity" [0.4] "string lightColorMap" ["%s"]'
                   ' "int enableTemperature" [0]\n' % rib_path(tex))
        rib.append('AttributeEnd\n')

    if args.scalp and not args.hide_scalp:
        verts, faces = read_obj(args.scalp)
        rib.append('AttributeBegin\n  Bxdf "PxrDiffuse" "scalp" "color diffuseColor" [0.36 0.29 0.25]\n')
        rib.append('  PointsPolygons [%s] [%s] "P" [%s]\nAttributeEnd\n' % (
            " ".join(str(len(fc)) for fc in faces), " ".join(" ".join(str(i) for i in fc) for fc in faces),
            " ".join("%.4f %.4f %.4f" % tuple(v) for v in verts)))
    rib.append('AttributeBegin\n  Attribute "identifier" "string name" ["groom"]\n')
    rib.append("  " + hair_network(args.pigment).replace("\n", "\n  ").rstrip() + "\n")
    rib.append('  Procedural2 "DynamicLoad" "DSOBound" "string dsoname" ["groomlab_renderman"]'
               ' "string filename" ["%s"] "float density" [%f]\nAttributeEnd\n' % (rib_path(args.groom), args.density))
    rib.append('WorldEnd\n')

    scene = os.path.join(out_dir, "renderman_compare.rib")
    with open(scene, "w") as fh:
        fh.write("".join(rib))
    env = dict(os.environ, RMANTREE=os.path.dirname(os.path.dirname(os.path.abspath(args.prman))))
    cmd = [args.prman, "-progress", scene]
    print(" ".join(cmd))
    code = subprocess.call(cmd, env=env)
    if code == 0:
        tool = os.path.join(os.path.dirname(args.prman), "rmanoiiotool.exe" if os.name == "nt" else "rmanoiiotool")
        code = subprocess.call([tool, exr, "--ch", "R,G,B", "--colorconvert", "linear", "sRGB", "-d", "uint8", "-o",
                                os.path.abspath(args.out)], env=env)
    sys.exit(code)


if __name__ == "__main__":
    main()
