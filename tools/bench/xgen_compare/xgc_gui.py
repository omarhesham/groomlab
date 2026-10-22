# SPDX-License-Identifier: Apache-2.0
"""GUI worker: one system, one strand count, one phase, in a fresh Maya window.

Launched by the driver as `maya.exe -script <generated .mel>`, which execs this file; the args
JSON's path is in XGC_ARGS. Like the GroomLab GUI tests: the work runs in deferred steps after
Maya's idle work, the window is pinned to one size with the perspective view alone, nothing
moves the mouse or opens a dialog, and Maya quits when done (XGC_ARGS' out holds the result).

static: creation to the first drawn frame, a redraw, tweaks to the redrawn frame, orbit fps,
        a brush dab (where the system's brush can be driven by script), a playblast.
anim:   playback fps on the deforming scalp (Maya's play, every frame, free-running, Evaluation
        Manager parallel, cached playback off), and stepping frames with a forced redraw.
"""

import json
import os
import sys
import traceback

import maya.mel
import maya.utils
from maya import cmds

with open(os.environ["XGC_ARGS"]) as _f:
    ARGS = json.load(_f)
sys.path.insert(0, os.path.dirname(os.path.abspath(ARGS["self"])))
sys.path.insert(0, ARGS["scripts"])
sys.path.insert(0, ARGS["python"])

import xgc_common as C  # noqa: E402

RESULT = {"args": ARGS, "errors": [], "dialogs": []}
WINDOW = (1600, 1000)


def write():
    with open(ARGS["out"], "w") as f:
        json.dump(RESULT, f, indent=1)


def median(xs):
    xs = sorted(xs)
    n = len(xs)
    return None if not n else (xs[n // 2] if n % 2 else 0.5 * (xs[n // 2 - 1] + xs[n // 2]))


def qt():
    import shiboken2
    import maya.OpenMayaUI as omui
    from PySide2 import QtWidgets
    return shiboken2, omui, QtWidgets


def pin_view():
    """As tests/hosts/maya/gui_support.py pin_view: the window at WINDOW, the perspective view alone."""
    shiboken2, omui, QtWidgets = qt()
    window = shiboken2.wrapInstance(int(omui.MQtUtil.mainWindow()), QtWidgets.QMainWindow)
    window.showNormal()
    window.setGeometry(40, 40, WINDOW[0], WINDOW[1])
    for dock in ("ChannelBoxLayerEditor", "AttributeEditor", "ToolSettings", "Outliner", "MayaHome"):
        if cmds.workspaceControl(dock, exists=True) and cmds.workspaceControl(dock, query=True, visible=True):
            cmds.workspaceControl(dock, edit=True, visible=False)
    maya.mel.eval('setNamedPanelLayout "Single Perspective View"')
    QtWidgets.QApplication.processEvents()
    view = omui.M3dView.active3dView()
    RESULT["view"] = [view.portWidth(), view.portHeight()]


def persp_panel():
    for p in cmds.getPanel(type="modelPanel") or []:
        if cmds.modelPanel(p, query=True, camera=True) in ("persp", "|persp", "perspShape"):
            return p
    return cmds.getPanel(withFocus=True)


def note_dialogs():
    _, _, QtWidgets = qt()
    for w in QtWidgets.QApplication.topLevelWidgets():
        if w.isVisible() and isinstance(w, QtWidgets.QDialog):
            RESULT["dialogs"].append("%s: %s" % (w.objectName(), w.windowTitle()))


def redraw():
    cmds.refresh(currentView=True, force=True)


def to_view(point):
    import maya.OpenMaya as om
    import maya.OpenMayaUI as omui
    view = omui.M3dView.active3dView()
    x, y = om.MScriptUtil(), om.MScriptUtil()
    x.createFromInt(0)
    y.createFromInt(0)
    xp, yp = x.asShortPtr(), y.asShortPtr()
    view.worldToView(om.MPoint(*point), xp, yp)
    return om.MScriptUtil.getShort(xp), om.MScriptUtil.getShort(yp)


def vram_mb():
    """The GPU's memory in use (whole device, nvidia-smi), or None."""
    import subprocess
    try:
        out = subprocess.run(["nvidia-smi", "--query-gpu=memory.used", "--format=csv,noheader,nounits"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True,
                             timeout=20, creationflags=0x08000000).stdout
        return float(out.split()[0])
    except Exception:
        return None


def load_system():
    t = C.wall()
    if ARGS["system"] == "xgen":
        cmds.loadPlugin("xgenToolkit")
    else:
        cmds.loadPlugin(ARGS["plugin"])
        scripts = ARGS["scripts"].replace("\\", "/")
        for proc in ("groomLabBrushProperties", "groomLabBrushValues", "AEgroomLabShapeTemplate"):
            maya.mel.eval('source "%s/%s.mel"' % (scripts, proc))
        import groomlab  # noqa: F401
    RESULT["plugin_load_s"] = C.wall() - t
    return C.system(ARGS["system"], ARGS["plugin"])


def setup_view():
    panel = persp_panel()
    cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded", displayLights="default", grid=False,
                     headsUpDisplay=False, shadows=False)
    cmds.setAttr("hardwareRenderingGlobals.ssaoEnable", False)
    cmds.setAttr("hardwareRenderingGlobals.multiSampleEnable", False)
    C.set_camera()
    cmds.lookThru(panel, "persp")


def create(sys_, head):
    curves = sys_.guide_curves(head) if ARGS["system"].startswith("groomlab") else None
    gpu0 = vram_mb()
    ws0, pv0 = C.memory()
    t = C.wall()
    if curves is not None:
        sys_.create(head, ARGS["count"], curves=curves)
        cmds.delete(curves)
    else:
        sys_.create(head, ARGS["count"])
    t_cmd = C.wall() - t
    redraw()
    first = C.wall() - t
    t2 = C.wall()
    redraw()
    again = C.wall() - t2
    ws1, pv1 = C.memory()
    gpu1 = vram_mb()
    out = {"to_first_frame_s": first, "commands_s": t_cmd, "redraw_s": again, "count": sys_.count(),
           "ws_delta_mb": ws1 - ws0, "private_delta_mb": pv1 - pv0, "ws_after_mb": ws1,
           "gpu_device_delta_mb": (gpu1 - gpu0) if gpu0 is not None and gpu1 is not None else None}
    if ARGS["system"] == "xgen":
        try:
            out["xgen_vram_used_mb"] = cmds.xgmSplineQuery(videoMemoryUsed=True)
            out["xgen_opencl"] = bool(cmds.xgmSplineQuery(isCLReady=True))
        except Exception as e:
            out["xgen_vram_used_mb"] = "error: %s" % e
    return out


def draw_times(sys_):
    """GroomLab's breakdown of the last redraw (compute, build, upload), or None."""
    return sys_.draw_times() if hasattr(sys_, "draw_times") else None


def tweaks(sys_):
    out = {"clump_s": [], "noise_s": [], "clump_draw": [], "noise_draw": []}
    for i in range(ARGS.get("tweaks", 7)):
        for key, setter, base in (("clump_s", sys_.set_clump, C.CLUMP), ("noise_s", sys_.set_noise, C.NOISE)):
            t = C.wall()
            setter(base * (0.7 + 0.1 * (i % 4)))
            redraw()
            out[key].append(C.wall() - t)
            times = draw_times(sys_)
            if times is not None:
                out[key.replace("_s", "_draw")].append(times)
    sys_.set_clump(C.CLUMP)
    sys_.set_noise(C.NOISE)
    redraw()
    out["clump_median_s"] = median(out["clump_s"])
    out["noise_median_s"] = median(out["noise_s"])
    return out


def gpu_preview(sys_):
    """groomlab_gpu (GROOMLAB_GPU_PREVIEW=1): the GPU look preview against the CPU, in one
    session. Every tweak is timed to its drawn frame with the GPU waited for (groomLabGpuPreview
    -sync: an event query on Viewport 2.0's device), the CPU's too, so both count the GPU's work.
    CPU: the param set in the graph (set_param), evaluated, uploaded, drawn. GPU: a drag's value
    (groomLabGpuPreview -node -param -value): no evaluation, the kernels write the viewport's buffers.
    Then the release (the drag ended, the value committed on the CPU), a drag right after a clump
    change (the strands entering Noise uploaded again), and the GPU's results checked against the
    CPU's (read back)."""
    shape = sys_.shape
    n = ARGS.get("tweaks", 7)
    out = {"api": cmds.groomLabGpuPreview(drawApi=True)}
    RESULT["gpu"] = out  # partial results kept if a step fails

    def sync():
        cmds.groomLabGpuPreview(sync=True)

    def stats():
        return json.loads(cmds.groomLabGpuPreview(shape, stats=True, wait=True) or "{}")

    def timed(change):
        t = C.wall()
        result = change()
        redraw()
        sync()
        return C.wall() - t, result

    redraw()
    sync()
    for key, setter, base in (("cpu_noise", sys_.set_noise, C.NOISE), ("cpu_clump", sys_.set_clump, C.CLUMP)):
        out[key + "_s"] = []
        out[key + "_draw"] = []
        for i in range(n):
            s, _ = timed(lambda: setter(base * (0.7 + 0.1 * (i % 4))))
            out[key + "_s"].append(s)
            out[key + "_draw"].append(draw_times(sys_))
        # The nodes' times in the last evaluation (what a GPU path would leave to the CPU).
        out[key + "_node_ms"] = (json.loads(cmds.groomLabGraphStats(shape) or "{}")).get("times")
        setter(base)
        redraw()
    sync()

    def drag(node, param, value):
        return cmds.groomLabGpuPreview(shape, node=node, param=param, value=value)

    gpu0 = vram_mb()
    s, took = timed(lambda: drag("noise", "amplitude", C.NOISE * 0.9))
    out["first_s"], out["first_taken"], out["first_stats"] = s, bool(took), stats()
    if not took:  # the GPU does not take it here (OpenGL): the CPU's numbers alone
        del out["first_s"]
        out["cpu_noise_median_s"] = median(out["cpu_noise_s"])
        out["cpu_clump_median_s"] = median(out["cpu_clump_s"])
        return out
    gpu1 = vram_mb()
    out["gpu_device_delta_mb"] = (gpu1 - gpu0) if gpu0 is not None and gpu1 is not None else None
    # Right after the first: the GPU may still run at idle clocks (they rise within a fraction of a
    # second of steady work, as a drag is): timed, then a second of untimed drags before the rest.
    out["cold_s"], out["cold_stats"] = [], []
    for i in range(n):
        s, _ = timed(lambda: drag("noise", "amplitude", C.NOISE * (0.7 + 0.1 * (i % 4))))
        out["cold_s"].append(s)
        out["cold_stats"].append(stats().get("gpu"))
    out["cold_median_s"] = median(out["cold_s"])
    t = C.wall()
    while C.wall() - t < 1.0:
        drag("noise", "amplitude", C.NOISE * (0.7 + 0.3 * (C.wall() - t)))
        redraw()
    sync()
    kernels = []
    # Phase 2: Clump (its centers as the CPU chose and made them) and Interpolate (a radius: the
    # binding the CPU keeps, the guides' shapes, the Clump's own centers made again from the strands).
    for key, node, param, base in (("gpu_noise", "noise", "amplitude", C.NOISE), ("gpu_curl", "curl", "radius", C.CURL_RADIUS),
                                   ("gpu_width", "width", "width", C.WIDTH), ("gpu_clump", "clump", "strength", C.CLUMP),
                                   ("gpu_interpolate", "interpolate", "limitDistance", 0.4 * C.HEAD_RADIUS)):
        # Each param its own drag: the last one's values dropped (not committed: the graph keeps
        # its own), so each drag shows its param's values over the graph's.
        cmds.groomLabGpuPreview(shape, end=True)
        out[key + "_s"] = []
        for i in range(n):
            s, took = timed(lambda: drag(node, param, base * (0.7 + 0.1 * (i % 4))))
            out[key + "_s"].append(s)
            st = stats()
            kernels.append(st)
            if not took:
                out.setdefault("refused", []).append("%s.%s: %s" % (node, param, st.get("why")))
        out[key + "_median_s"] = median(out[key + "_s"])
    out["stats"] = kernels
    for part in ("noise", "curl", "lines", "copy", "total"):
        out["kernels_%s_median_ms" % part] = median([k["gpu"][part + "Ms"] for k in kernels if "gpu" in k])
    # Per drag: its run's kernels (a Clump drag runs Clump and what follows, an Interpolate drag
    # everything from Interpolate on).
    per = len(kernels) // 5 if len(kernels) >= 5 else 0
    for index, key in enumerate(("noise", "curl", "width", "clump", "interpolate")):
        runs = [k["gpu"] for k in kernels[index * per:(index + 1) * per] if "gpu" in k]
        if runs:
            out["kernels_%s_drag_total_median_ms" % key] = median([r["totalMs"] for r in runs])
            out["kernels_%s_drag_median_ms" % key] = {part: median([r.get(part + "Ms", 0) for r in runs])
                                                      for part in ("interpolate", "clump", "noise", "curl", "width", "lines")}
    out["apply_cpu_median_ms"] = median([k["cpuMs"] for k in kernels if "cpuMs" in k])
    out["video_mb"] = kernels[-1].get("videoMB") if kernels else None
    out["display_mb"] = kernels[-1].get("displayMB") if kernels else None
    out["in_place"] = kernels[-1].get("inPlace") if kernels else None
    out["vp2_buffer"] = kernels[-1].get("vp2Buffer") if kernels else None
    out["draw_after_drag"] = draw_times(sys_)
    cmds.groomLabGpuPreview(shape, end=True)
    redraw()
    # The same value drawn by the GPU (a drag) and by the CPU (committed): the two frames compared,
    # beside what tells the scale: the same CPU frame captured twice, and the CPU at a value 1%
    # away (fine hair: a sub-pixel move changes many pixels).
    drag("noise", "amplitude", C.NOISE * 1.2)
    redraw()
    sync()
    gpu_shot = playblast("gpu_drag")
    cmds.groomLabGpuPreview(shape, end=True)
    sys_.set_noise(C.NOISE * 1.2)
    redraw()
    sync()
    cpu_shot = playblast("cpu_commit")
    cpu_again = playblast("cpu_commit_again")
    sys_.set_noise(C.NOISE * 1.2 * 1.01)
    redraw()
    sync()
    cpu_near = playblast("cpu_commit_1pc")
    # A clump change evaluated on the CPU and a drag in the same frame: the CPU writes the strands
    # (Maya's buffers keep a system-memory copy under DirectX 11) and the GPU then writes its own.
    sys_.set_noise(C.NOISE)
    redraw()
    sys_.set_clump(C.CLUMP * 0.9)
    drag("noise", "amplitude", C.NOISE * 1.2)
    redraw()
    sync()
    same_frame = playblast("gpu_same_frame_as_cpu")
    out["same_frame_stats"] = stats()
    cmds.groomLabGpuPreview(shape, end=True)
    sys_.set_noise(C.NOISE * 1.2)
    redraw()
    sync()
    same_frame_cpu = playblast("cpu_clump_and_noise")
    out["frames_compared"] = {"gpu_vs_cpu": compare_images(gpu_shot, cpu_shot),
                              "cpu_vs_same_cpu": compare_images(cpu_shot, cpu_again),
                              "cpu_vs_cpu_1pc_away": compare_images(cpu_shot, cpu_near),
                              "gpu_in_cpu_frame_vs_cpu": compare_images(same_frame, same_frame_cpu)}
    sys_.set_clump(C.CLUMP)
    sys_.set_noise(C.NOISE)
    redraw()
    # The release: the drag's values dropped, the last value committed on the CPU (as the Graph
    # Editor's slider release does: end_preview, then set_param).
    out["commit_s"] = []
    for i in range(3):
        drag("noise", "amplitude", C.NOISE * (0.8 + 0.1 * i))
        redraw()
        sync()
        s, _ = timed(lambda: (cmds.groomLabGpuPreview(shape, end=True), sys_.set_noise(C.NOISE * (0.8 + 0.1 * i))))
        out["commit_s"].append(s)
    out["commit_median_s"] = median(out["commit_s"])
    sys_.set_noise(C.NOISE)
    redraw()
    # The same for a Clump drag's release.
    out["clump_commit_s"] = []
    for i in range(3):
        drag("clump", "strength", C.CLUMP * (0.8 + 0.1 * i))
        redraw()
        sync()
        s, _ = timed(lambda: (cmds.groomLabGpuPreview(shape, end=True), sys_.set_clump(C.CLUMP * (0.8 + 0.1 * i))))
        out["clump_commit_s"].append(s)
    out["clump_commit_median_s"] = median(out["clump_commit_s"])
    sys_.set_clump(C.CLUMP)
    redraw()
    # A Clump drag's frame against the CPU's at the same value (the GPU's centers and choice are
    # the CPU's).
    drag("clump", "strength", C.CLUMP * 0.7)
    redraw()
    sync()
    clump_shot = playblast("gpu_clump_drag")
    cmds.groomLabGpuPreview(shape, end=True)
    sys_.set_clump(C.CLUMP * 0.7)
    redraw()
    sync()
    out["frames_compared"]["gpu_clump_vs_cpu"] = compare_images(clump_shot, playblast("cpu_clump_commit"))
    sys_.set_clump(C.CLUMP)
    redraw()
    # A clump tweak (CPU), then a noise drag: the strands entering Noise are new, uploaded again.
    sys_.set_clump(C.CLUMP * 0.9)
    redraw()
    sync()
    s, _ = timed(lambda: drag("noise", "amplitude", C.NOISE * 1.1))
    st = stats()
    out["after_clump_s"], out["after_clump_stats"] = s, st
    out["upload_ms"], out["upload_mb"] = st.get("uploadMs"), st.get("uploadMB")
    cmds.groomLabGpuPreview(shape, end=True)
    sys_.set_clump(C.CLUMP)
    sys_.set_noise(C.NOISE)
    redraw()
    sync()
    # A brush stroke (a comb over the crown, scripted), then a noise drag: the strands entering
    # Noise changed in the stroke's strands alone (incremental evaluation): only those go up.
    drag("noise", "amplitude", C.NOISE * 1.05)
    redraw()
    cmds.groomLabGpuPreview(shape, end=True)
    redraw()
    sync()
    import groomlab
    top = C.HEAD_RADIUS * 1.1
    groomlab.brush_stroke(sys_.shape, "comb", [(0.0, top, 0.0), (0.3 * C.HEAD_RADIUS, 0.95 * top, 0.0)],
                          radius=0.25 * C.HEAD_RADIUS, level=-1)
    redraw()
    sync()
    s, _ = timed(lambda: drag("noise", "amplitude", C.NOISE * 1.1))
    st = stats()
    out["after_stroke_s"], out["after_stroke_stats"] = s, st
    out["stroke_upload_ms"], out["stroke_upload_mb"] = st.get("uploadMs"), st.get("uploadMB")
    out["stroke_upload_strands"], out["stroke_upload_partial"] = st.get("uploadStrands"), st.get("uploadPartial")
    cmds.groomLabGpuPreview(shape, end=True)
    sys_.set_noise(C.NOISE)
    redraw()
    sync()
    # The GPU's results against the CPU's on the same strands (read back: a check only): from Noise
    # (Phase 1's), from Clump and from Interpolate.
    out["check"] = json.loads(cmds.groomLabGpuPreview(shape, check=True, **{"from": "noise"}) or "{}")
    out["check_clump"] = json.loads(cmds.groomLabGpuPreview(shape, check=True, **{"from": "clump"}) or "{}")
    out["check_interpolate"] = json.loads(cmds.groomLabGpuPreview(shape, check=True, **{"from": "interpolate"}) or "{}")
    out["cpu_noise_median_s"] = median(out["cpu_noise_s"])
    out["cpu_clump_median_s"] = median(out["cpu_clump_s"])
    return out


def compare_images(a, b):
    """Pixels that differ between two viewport captures: count, share and the largest channel step."""
    from PySide2 import QtGui
    ia, ib = QtGui.QImage(a), QtGui.QImage(b)
    if ia.isNull() or ib.isNull() or ia.size() != ib.size():
        return {"error": "could not compare %s and %s" % (a, b)}
    ia = ia.convertToFormat(QtGui.QImage.Format_RGB32)
    ib = ib.convertToFormat(QtGui.QImage.Format_RGB32)
    da, db = bytes(ia.constBits()[:ia.sizeInBytes()]), bytes(ib.constBits()[:ib.sizeInBytes()])
    differing, largest, steps = 0, 0, 0
    for i in range(0, len(da), 4):
        if da[i:i + 3] != db[i:i + 3]:
            differing += 1
            step = max(abs(da[i + k] - db[i + k]) for k in range(3))
            largest = max(largest, step)
            steps += step
    total = ia.width() * ia.height()
    return {"differing_pixels": differing, "share": differing / float(total), "largest_step": largest,
            "mean_step": steps / float(total), "images": [a, b]}


def orbit():
    frames = 60
    redraw()
    t = C.wall()
    for _ in range(frames):
        cmds.orbit("persp", rotationAngles=(0, 6))
        redraw()
    s = C.wall() - t
    C.set_camera()
    redraw()
    return {"frames": frames, "seconds": s, "fps": frames / s}


def idle_pause():
    """Maya's idle work, as between an artist's strokes (GroomLab's brush tool evaluates its
    groom's stroke document ahead then)."""
    # Qt's event loop run for a moment: Maya's idle work (its idle event, the deferred queue) runs
    # when the loop has nothing else to do.
    from PySide2 import QtCore
    loop = QtCore.QEventLoop()
    QtCore.QTimer.singleShot(400, loop.quit)
    loop.exec_()
    maya.utils.processIdleEvents()


def brush_dab(sys_):
    """One short stroke through the system's own interactive brush code, timed to the redraw."""
    x0, y0 = to_view((0.0, 1.05 * C.HEAD_RADIUS, 0.5 * C.HEAD_RADIUS))
    stroke = [(x0 - 40 + 10 * k, y0) for k in range(9)]
    if ARGS["system"].startswith("groomlab"):
        gl = sys_.gl
        cmds.select(sys_.transform, replace=True)
        ctx = gl.brush_tool("comb", radius=120, strength=1.0)
        # The session's first stroke, drawn, after Maya's idle work: an artist's first press (the
        # stroke document evaluated ahead in idle time, by a build that does; GROOMLAB_STROKE_WARMUP=0
        # turns that off).
        idle_pause()
        first = {}
        try:
            first["stroke_document_ready"] = json.loads(cmds.groomLabGraphStats(sys_.shape)).get("strokeDocument")
            pts = [(x, y + 12) for x, y in stroke]
            cmds.groomLabBrushCtx(ctx, edit=True, debugStroke="draw " + " ".join("%d %d" % p for p in pts))
            steps = [float(t) / 1000.0 for t in cmds.groomLabBrushCtx(ctx, query=True, debugStrokeTimes=True).split()]
            first.update({"first_press_s": steps[0], "first_release_s": steps[-1],
                          "first_steps_draw": json.loads(cmds.groomLabBrushCtx(ctx, query=True, debugStrokeDraws=True))})
        except Exception as e:  # a build without -debugStrokeTimes
            first["first_error"] = str(e)
        times = []
        for rep in range(5):
            before = cmds.getAttr(sys_.shape + ".guideData")
            pts = [(x + 4 * rep, y) for x, y in stroke]
            t = C.wall()
            cmds.groomLabBrushCtx(ctx, edit=True, debugStroke=" ".join("%d %d" % p for p in pts))
            redraw()
            times.append(C.wall() - t)
            changed = cmds.getAttr(sys_.shape + ".guideData") != before
        out = {"how": "groomLabBrushCtx -debugStroke (press, 9 drags, release; comb on ~%d guides)"
                      % sys_.guide_count, "stroke_s": times, "median_s": median(times), "changed": changed}
        out.update(first)
        # Per dab, as an artist feels it: the same strokes drawn after the press and every drag
        # ("draw": the viewport redrawn each time, the stroke's density while it is live), each
        # step timed to its redrawn frame (-debugStrokeTimes: press, dabs, release); Maya's idle work
        # between strokes, as an artist pauses between them.
        dabs, presses, releases = [], [], []
        try:
            for rep in range(5):
                idle_pause()
                pts = [(x + 4 * rep, y + 6) for x, y in stroke]
                cmds.groomLabBrushCtx(ctx, edit=True, debugStroke="draw " + " ".join("%d %d" % p for p in pts))
                steps = [float(t) / 1000.0 for t in cmds.groomLabBrushCtx(ctx, query=True, debugStrokeTimes=True).split()]
                if len(steps) >= 3:
                    presses.append(steps[0])
                    dabs.extend(steps[1:-1])
                    releases.append(steps[-1])
                try:  # each step's compute and viewport update (the last stroke's kept)
                    out["steps_draw"] = json.loads(cmds.groomLabBrushCtx(ctx, query=True, debugStrokeDraws=True))
                except Exception:
                    pass
            out.update({"dab_s": dabs, "dab_median_s": median(dabs), "press_s": presses, "press_median_s": median(presses),
                        "release_s": releases, "release_median_s": median(releases)})
        except Exception as e:  # a build without -debugStrokeTimes
            out["dab_error"] = str(e)
        cmds.setToolTo("selectSuperContext")
        return out
    # XGen: the grab/comb brushes are Maya contexts driven by mouse events only; their commands
    # (xgmCombBrushToolCmd -chunkId, the undo record) take no stroke. xgmBrushManip has a -stroke
    # flag: try it, and say whether the splines moved.
    try:
        maya.mel.eval("xgmSetCombBrushTool")
        cmds.select(sys_.transform, replace=True)
        before = cmds.xgmDataQueryHelperForTest(sys_.shape, position=True)
        t = C.wall()
        cmds.xgmBrushManip(stroke=(stroke[0][0], stroke[0][1], stroke[-1][0], stroke[-1][1]))
        redraw()
        s = C.wall() - t
        after = cmds.xgmDataQueryHelperForTest(sys_.shape, position=True)
        moved = before != after
        cmds.setToolTo("selectSuperContext")
        return {"how": "xgmBrushManip -stroke (probe)", "stroke_s": [s], "changed": moved,
                "scriptable": bool(moved)}
    except Exception as e:
        cmds.setToolTo("selectSuperContext")
        return {"how": "xgmBrushManip -stroke (probe)", "scriptable": False, "error": str(e)}


def playblast(name):
    path = os.path.join(ARGS["work"], name + ".png")
    cmds.playblast(frame=cmds.currentTime(query=True), format="image", completeFilename=path, viewer=False,
                   showOrnaments=False, widthHeight=(960, 600), percent=100, offScreen=False)
    return path


def playback(sys_):
    first, last = C.PLAY
    cmds.playbackOptions(minTime=first, maxTime=last, loop="once", playbackSpeed=0, maxPlaybackSpeed=0)
    out = {}
    for rep in range(2):
        if rep and out["play0"]["seconds"] > 40:
            break  # a slow first pass is enough (XGen at 500k)
        cmds.currentTime(first)
        redraw()
        evals0 = cmds.groomLabGraphStats(sys_.shape, evaluations=True) if ARGS["system"].startswith("groomlab") else None
        t = C.wall()
        cmds.play(wait=True, forward=True)
        s = C.wall() - t
        frames = last - first
        entry = {"seconds": s, "fps": frames / s}
        if evals0 is not None:
            entry["groom_evaluations"] = cmds.groomLabGraphStats(sys_.shape, evaluations=True) - evals0
        out["play%d" % rep] = entry
    cmds.currentTime(first)
    redraw()
    t = C.wall()
    for f in range(first + 1, last + 1):
        cmds.currentTime(f)
        redraw()
    s = C.wall() - t
    out["stepped"] = {"seconds": s, "fps": (last - first) / s}
    if draw_times(sys_) is not None:
        # Ten frames again, each timed with its breakdown (compute, build, upload).
        frames = []
        for f in range(first + 1, min(last, first + 10) + 1):
            t = C.wall()
            cmds.currentTime(f)  # draws the frame (the GUI's currentTime redraws)
            redraw()
            entry = draw_times(sys_)
            entry["frame_s"] = C.wall() - t
            frames.append(entry)
        out["stepped_draw"] = frames
    out["evaluationManager"] = cmds.evaluationManager(query=True, mode=True)
    return out


def run():
    try:
        pin_view()
        C.settle()
        setup_view()
        sys_ = load_system()
        note_dialogs()
        cmds.file(new=True, force=True)
        C.settle()
        setup_view()
        head = C.make_head()
        if ARGS["phase"] == "anim":
            C.add_bend(head)
        RESULT["creation"] = create(sys_, head)
        RESULT["info"] = sys_.info()
        RESULT["shot"] = playblast("look")
        if ARGS["phase"] == "static" and ARGS["system"] == "groomlab_gpu":
            RESULT["gpu"] = gpu_preview(sys_)
            # Brush dabs with the GPU preview on: each dab after the press drawn from the GPU.
            RESULT["brush"] = brush_dab(sys_)
            RESULT["brush"]["gpu_stats"] = json.loads(cmds.groomLabGpuPreview(sys_.shape, stats=True) or "{}")
        elif ARGS["phase"] == "static":
            RESULT["tweak"] = tweaks(sys_)
            RESULT["orbit"] = orbit()
            RESULT["brush"] = brush_dab(sys_)
        else:
            RESULT["playback"] = playback(sys_)
            cmds.currentTime(50)
            RESULT["shot_mid"] = playblast("look_frame50")
        note_dialogs()
    except Exception:
        RESULT["errors"].append(traceback.format_exc())
    write()
    cmds.evalDeferred(lambda: cmds.quit(force=True), lowestPriority=True)


maya.utils.executeDeferred(lambda: cmds.evalDeferred(run, lowestPriority=True))
