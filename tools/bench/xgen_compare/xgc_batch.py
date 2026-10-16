# SPDX-License-Identifier: Apache-2.0
"""Batch worker (mayapy): one system, one strand count, one phase, in a fresh process.

    mayapy xgc_batch.py <args.json>

args: system (xgen | groomlab | groomlab_follow), count, phase (static | anim), out (result JSON),
work (a folder for scenes and exports), plugin (GroomLab.mll), scripts (hosts/maya/scripts),
extensions (the GroomLab MtoA translator folder), tweaks (repeats), warm (warm creations).

static: plugin load, creation (cold: the first in this process; warm: again after deleting it),
        working set and private bytes before/after, clump and noise tweaks (set + force
        evaluation), saved .ma/.mb sizes and side files, the Arnold .ass export (static frame).
anim:   the scalp bent over frames 1-100: per-frame evaluation over frames 1-50 (frame change + force, no drawing),
        the Alembic export of 24 frames, the Arnold .ass export at frame 25 with 2-key motion blur.
The .ass files are rendered by the driver (kick), not here.
"""

import json
import os
import sys
import traceback

with open(sys.argv[1]) as _f:
    ARGS = json.load(_f)
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ARGS["scripts"])
os.environ["MTOA_EXTENSIONS_PATH"] = os.pathsep.join(
    p for p in [ARGS.get("extensions", ""), os.environ.get("MTOA_EXTENSIONS_PATH", "")] if p)

import maya.standalone  # noqa: E402

maya.standalone.initialize(name="python")

from maya import cmds  # noqa: E402

import xgc_common as C  # noqa: E402

RESULT = {"args": ARGS, "errors": []}


def write():
    with open(ARGS["out"], "w") as f:
        json.dump(RESULT, f, indent=1)


def median(xs):
    xs = sorted(xs)
    n = len(xs)
    return None if not n else (xs[n // 2] if n % 2 else 0.5 * (xs[n // 2 - 1] + xs[n // 2]))


def load_system():
    t = C.wall()
    if ARGS["system"] == "xgen":
        cmds.loadPlugin("xgenToolkit")
    else:
        cmds.loadPlugin(ARGS["plugin"])
        import groomlab  # noqa: F401
    RESULT["plugin_load_s"] = C.wall() - t
    return C.system(ARGS["system"], ARGS["plugin"])


def make_inputs(sys_, head):
    """Inputs made before the clock starts: GroomLab's guide curves (XGen has none)."""
    return sys_.guide_curves(head) if ARGS["system"].startswith("groomlab") else None


def create_timed(sys_, head, label):
    curves = make_inputs(sys_, head)
    ws0, pv0 = C.memory()
    t = C.wall()
    if curves is not None:
        sys_.create(head, ARGS["count"], curves=curves)
        cmds.delete(curves)
    else:
        sys_.create(head, ARGS["count"])
    t_cmd = C.wall() - t
    sys_.evaluate()
    total = C.wall() - t
    ws1, pv1 = C.memory()
    return {"label": label, "create_s": total, "commands_s": t_cmd, "first_eval_s": total - t_cmd,
            "count": sys_.count(), "ws_before_mb": ws0, "ws_after_mb": ws1, "ws_delta_mb": ws1 - ws0,
            "private_delta_mb": pv1 - pv0}


def tweaks(sys_):
    out = {"clump_s": [], "noise_s": []}
    for i in range(ARGS.get("tweaks", 7)):
        for key, setter, base in (("clump_s", sys_.set_clump, C.CLUMP), ("noise_s", sys_.set_noise, C.NOISE)):
            value = base * (0.7 + 0.1 * (i % 4))
            t = C.wall()
            setter(value)
            sys_.evaluate()
            out[key].append(C.wall() - t)
        # Back to the shared look.
    sys_.set_clump(C.CLUMP)
    sys_.set_noise(C.NOISE)
    sys_.evaluate()
    out["clump_median_s"] = median(out["clump_s"])
    out["noise_median_s"] = median(out["noise_s"])
    return out


def files_under(folder):
    out = {}
    for root, _, names in os.walk(folder):
        for n in names:
            p = os.path.join(root, n)
            out[os.path.relpath(p, folder).replace("\\", "/")] = os.path.getsize(p)
    return out


def arnold_setup(sys_):
    t = C.wall()
    if not cmds.pluginInfo("mtoa", query=True, loaded=True):
        cmds.loadPlugin("mtoa")
    import mtoa.core
    mtoa.core.createOptions()
    RESULT.setdefault("mtoa_load_s", C.wall() - t)
    cmds.setAttr("defaultResolution.width", 960)
    cmds.setAttr("defaultResolution.height", 540)
    cmds.setAttr("defaultArnoldRenderOptions.AASamples", 3)
    cmds.setAttr("defaultArnoldRenderOptions.GIDiffuseSamples", 2)
    cmds.setAttr("defaultArnoldRenderOptions.GISpecularSamples", 2)
    cmds.setAttr("defaultArnoldRenderOptions.GITransmissionSamples", 2)
    if not cmds.ls(type="aiSkyDomeLight"):
        cmds.shadingNode("aiSkyDomeLight", asLight=True)
        key = cmds.directionalLight(intensity=2.0)
        cmds.setAttr(cmds.listRelatives(key, parent=True)[0] + ".rotate", -40, 30, 0)
    shader = cmds.shadingNode("aiStandardHair", asShader=True, name="benchHair")
    cmds.setAttr(shader + ".melanin", 0.4)
    sg = cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name="benchHairSG")
    cmds.connectAttr(shader + ".outColor", sg + ".surfaceShader")
    sys_.assign(sg)
    C.set_camera()


def export_ass(path, motion_blur):
    cmds.setAttr("defaultArnoldRenderOptions.motion_blur_enable", bool(motion_blur))
    if motion_blur:
        cmds.setAttr("defaultArnoldRenderOptions.motion_steps", 2)
    t = C.wall()
    cmds.arnoldExportAss(filename=path, camera="perspShape")
    s = C.wall() - t
    return {"path": path, "export_s": s, "size_bytes": os.path.getsize(path), "motion_blur": bool(motion_blur)}


def phase_static(sys_):
    work = ARGS["work"]
    cmds.file(new=True, force=True)
    C.settle()
    head = C.make_head()
    RESULT["head"] = C.head_info(head)
    RESULT["creation_cold"] = create_timed(sys_, head, "cold")
    RESULT["info"] = sys_.info()
    RESULT["tweak"] = tweaks(sys_)
    warm = []
    for i in range(ARGS.get("warm", 2)):
        sys_.delete()
        cmds.flushUndo()
        warm.append(create_timed(sys_, head, "warm%d" % i))
    RESULT["creation_warm"] = warm
    # Saved scene sizes: a folder of its own, so side files show.
    for kind, ext in (("mayaAscii", "ma"), ("mayaBinary", "mb")):
        folder = os.path.join(work, "scene_" + ext)
        os.makedirs(folder, exist_ok=True)
        path = os.path.join(folder, "bench.%s" % ext)
        cmds.workspace(folder, openWorkspace=True)
        cmds.file(rename=path)
        t = C.wall()
        cmds.file(save=True, type=kind, force=True)
        RESULT.setdefault("scene", {})[ext] = {"save_s": C.wall() - t, "size_bytes": os.path.getsize(path),
                                               "files": files_under(folder)}
    arnold_setup(sys_)
    RESULT["ass_static"] = export_ass(os.path.join(work, "static.ass"), False)
    if ARGS.get("reopen", True):
        # Opening the saved scene: the time to a fully evaluated groom again.
        path = os.path.join(work, "scene_mb", "bench.mb")
        cmds.file(new=True, force=True)
        t = C.wall()
        cmds.file(path, open=True, force=True)
        t_open = C.wall() - t
        if ARGS["system"] == "xgen":
            shape = cmds.ls(type="xgmSplineDescription", long=True)[0]
            cmds.dgeval(shape + ".outSplineData")
        else:
            shape = cmds.ls(type="groomLabShape", long=True)[0]
            cmds.getAttr(shape + ".outStrandCount")
        RESULT["reopen"] = {"open_s": t_open, "open_and_eval_s": C.wall() - t}


def phase_anim(sys_):
    work = ARGS["work"]
    cmds.file(new=True, force=True)
    C.settle()
    head = C.make_head()
    C.add_bend(head)
    RESULT["head"] = C.head_info(head)
    RESULT["creation_cold"] = create_timed(sys_, head, "cold, deforming scalp")
    RESULT["info"] = sys_.info()
    first, last = C.PLAY
    times = []
    for f in range(first, last + 1):
        t = C.wall()
        cmds.currentTime(f)
        sys_.evaluate()
        times.append(C.wall() - t)
    RESULT["frames"] = {"per_frame_s": times, "median_s": median(times[1:]), "eval_fps": (len(times) - 1) / sum(times[1:])}
    cmds.currentTime(first)
    path = os.path.join(work, "anim.abc")
    t = C.wall()
    n = sys_.export_alembic(path, first, first + 23)
    RESULT["alembic"] = {"frames": n, "export_s": C.wall() - t, "size_bytes": os.path.getsize(path)}
    arnold_setup(sys_)
    cmds.currentTime(25)  # mid-swing: the scalp moves fastest here
    RESULT["ass_motion"] = export_ass(os.path.join(work, "motion.ass"), True)


def main():
    try:
        sys_ = load_system()
        ws, pv = C.memory()
        RESULT["ws_after_load_mb"] = ws
        (phase_static if ARGS["phase"] == "static" else phase_anim)(sys_)
        RESULT["ws_end_mb"] = C.memory()[0]
    except Exception:
        RESULT["errors"].append(traceback.format_exc())
    write()


main()
os.sys.stdout.flush()
cmds.quit(force=True)
