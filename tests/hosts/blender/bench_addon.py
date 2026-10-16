# SPDX-License-Identifier: Apache-2.0
"""Where the add-on's time goes (run like an add-on test: run_blender_test.py <blender> <zip> bench_addon.py):
a groom made on the test head (0.16 m, Blender's default meters, Z up), then the interactions a user
makes, each timed and broken down: the add-on's Python, each helper request's round trip (IPC and the
helper's work), the helper's own evaluation time, writing the hair Curves, and Blender evaluating the
depsgraph after. GROOMLAB_BENCH_PROFILE=1 prints a cProfile of each interaction too."""

import cProfile
import collections
import importlib
import io
import os
import pstats
import sys
import time

import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import blender_util as bu  # noqa: E402
import test_helper as th  # noqa: E402

pkg, m = bu.enable()
groom, process, graph = m["groom"], m["process"], m["graph"]
tools = importlib.import_module(bu.MODULE + ".tools")
tool_ops = importlib.import_module(bu.MODULE + ".tool_ops")
display = importlib.import_module(bu.MODULE + ".display")
PROFILE = os.environ.get("GROOMLAB_BENCH_PROFILE") == "1"


class Meter(object):
    """Wraps the add-on's helper calls and hair writes: time per request, the helper's evaluation."""

    def __init__(self):
        self.calls = collections.OrderedDict()
        self.eval_ms = 0.0
        self.write_ms = 0.0
        self.writes = 0
        client = process.client()
        original_call = client.call

        def call(cmd, **args):
            t0 = time.perf_counter()
            try:
                r = original_call(cmd, **args)
            finally:
                ms = (time.perf_counter() - t0) * 1000.0
                n, total = self.calls.get(cmd, (0, 0.0))
                self.calls[cmd] = (n + 1, total + ms)
            if cmd == "evaluate":
                self.eval_ms += float(r.get("milliseconds") or 0.0)
            return r
        client.call = call
        original_write = groom.write_hair

        def write_hair(scalp, r):
            t0 = time.perf_counter()
            try:
                return original_write(scalp, r)
            finally:
                self.write_ms += (time.perf_counter() - t0) * 1000.0
                self.writes += 1
        groom.write_hair = write_hair

    def reset(self):
        self.calls.clear()
        self.eval_ms = self.write_ms = 0.0
        self.writes = 0


meter = Meter()
results = []


def measure(label, fn, scalp, repeat=1):
    """Runs fn `repeat` times; prints the mean breakdown per run."""
    meter.reset()
    prof = cProfile.Profile() if PROFILE else None
    t0 = time.perf_counter()
    dg = 0.0
    for _ in range(repeat):
        if prof:
            prof.enable()
        fn()
        if prof:
            prof.disable()
        t1 = time.perf_counter()
        bpy.context.view_layer.update()  # Blender evaluates the changed curves (what a redraw needs first)
        dg += time.perf_counter() - t1
    total = (time.perf_counter() - t0) * 1000.0 / repeat
    dg_ms = dg * 1000.0 / repeat
    ipc = sum(t for _, t in meter.calls.values()) / repeat
    hair = scalp.groomlab.hair
    s = len(hair.data.curves) if hair is not None else 0
    p = len(hair.data.points) if hair is not None else 0
    row = {"label": label, "total": total, "ipc": ipc, "eval": meter.eval_ms / repeat, "write": meter.write_ms / repeat,
           "depsgraph": dg_ms, "python": total - ipc - meter.write_ms / repeat - dg_ms, "strands": s, "points": p,
           "calls": dict((k, (n / float(repeat), t / repeat)) for k, (n, t) in meter.calls.items())}
    results.append(row)
    print("\n== %s (%d strands, %d points) ==" % (label, s, p))
    print("  total %.1f ms | helper requests %.1f ms (of which evaluation %.1f) | write hair %.1f ms (%d writes) | "
          "depsgraph %.1f ms | other python %.1f ms" % (total, ipc, row["eval"], row["write"], meter.writes / float(repeat),
                                                       dg_ms, row["python"]))
    for k, (n, t) in sorted(row["calls"].items(), key=lambda kv: -kv[1][1]):
        print("    %-16s x%-5.1f %8.2f ms" % (k, n, t))
    if prof:
        out = io.StringIO()
        pstats.Stats(prof, stream=out).sort_stats("cumulative").print_stats(25)
        print(out.getvalue())
    return row


def main():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o)
    scene = bpy.context.scene
    head = bu.head_object()
    subdiv = int(os.environ.get("GROOMLAB_BENCH_SUBDIV", "0"))
    if subdiv:  # a denser head, as a production scalp is (the test head has 1225 points)
        import bmesh
        bm = bmesh.new()
        bm.from_mesh(head.data)
        for _ in range(subdiv):
            bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=1, use_grid_fill=True)
        bm.to_mesh(head.data)
        bm.free()
        head.data.update()
    print("head: %d points, %d faces" % (len(head.data.vertices), len(head.data.polygons)))
    cam = th.Camera()

    class Region(object):
        width, height, x, y = cam.width, cam.height, 0, 0

    from mathutils import Matrix, Vector

    class RV3D(object):
        view_matrix = Matrix(cam.V.tolist())
        window_matrix = Matrix(cam.P.tolist())
        view_location = Vector(cam.target.tolist())

    region, rv3d = Region(), RV3D()
    view = tools.view_of(region, rv3d, head)

    measure("create groom (hair_basics, 60 starter guides)",
            lambda: bpy.ops.groomlab.add_groom("EXEC_DEFAULT", preset="hair_basics", guides=60), head)
    rt = groom.runtime(head)
    if os.environ.get("GROOMLAB_BENCH_DENSITY"):  # another preview density than the default
        head.groomlab.preview_density = float(os.environ["GROOMLAB_BENCH_DENSITY"])
    print("scalp size %.2f cm; preview density %.2f" % (groom.scalp_size_cm(groom.read_scalp(head)[0], scene),
                                                        head.groomlab.preview_density))
    r = process.call("evaluate", session=rt.session, mode="render")
    print("render strands %d, points %d, helper eval %.1f ms" % (r["strandCount"], r["pointCount"], r["milliseconds"]))
    r = process.call("evaluate", session=rt.session, density=float(head.groomlab.preview_density))
    print("preview strands %d, points %d, helper eval %.1f ms" % (r["strandCount"], r["pointCount"], r["milliseconds"]))
    rt.serial = None
    groom.sync(head, force_eval=True)

    # A comb stroke through the modal tool: press, 12 drags (a live evaluation each when due), release.
    tools.pick(bpy.context, "comb")
    pts = groom.read_scalp(head)[0]
    top = pts[pts[:, 2] > np.percentile(pts[:, 2], 70)]
    mids = [cam.pixel(p) for p in top[:: max(1, len(top) // 20)]]
    y = float(np.mean([q[1] for q in mids]))
    x0, x1 = min(q[0] for q in mids), max(q[0] for q in mids)
    path = th.line((x0, y), (x1, y + 20.0), 12)

    def stroke():
        tools.stroke_begin(head, view, path[0][0], path[0][1])
        for p in path[1:]:
            tools.groom_tools(head).last_live = 0.0  # every drag evaluates live (as a slow stroke does)
            tools.stroke_drag(head, [p])
        tools.stroke_end(head)
    measure("comb stroke (begin, 11 live drags, end)", stroke, head, repeat=3)

    def drag_only():
        tools.stroke_begin(head, view, path[0][0], path[0][1])
        meter.reset()
        t0 = time.perf_counter()
        for p in path[1:]:
            tools.groom_tools(head).last_live = 0.0
            tools.stroke_drag(head, [p])
            bpy.context.view_layer.update()
        ms = (time.perf_counter() - t0) * 1000.0 / (len(path) - 1)
        print("  per live drag: %.1f ms (helper eval %.1f, write %.1f)" % (ms, meter.eval_ms / (len(path) - 1),
                                                                        meter.write_ms / (len(path) - 1)))
        tools.stroke_cancel(head)
    drag_only()

    measure("stroke end alone (after_edit)", lambda: (tools.stroke_begin(head, view, path[0][0], path[0][1]),
                                                     tools.stroke_drag(head, [path[3]], live=False),
                                                     tools.stroke_end(head)), head, repeat=3)

    # A param change from the panel: set_param, then the debounced flush.
    nodes = process.call("list_nodes", session=rt.session)["nodes"]
    scatter = next(n["name"] for n in nodes if n["type"] == "Scatter")
    length = process.call("get_param", session=rt.session, node=scatter, param="length")["value"]
    flip = [length]

    def param():
        flip[0] = flip[0] * (1.1 if len(results) % 2 else 0.95)
        graph.set_param(head, scatter, "length", flip[0])
        groom.flush()
    measure("param change (Scatter length)", param, head, repeat=3)

    names = ["curly_hair", "hair_basics"]
    presets = [p[0] for p in graph.cached_presets()] or names
    chosen = ["curly", "hair_basics"]

    def preset():
        graph.apply_preset(head, chosen[0])
        groom.flush()
        chosen.reverse()
    print("presets used: %s" % chosen)
    measure("preset change", preset, head, repeat=2)

    # Render: every strand (render_pre), then back to the preview.
    measure("render on (full density)", lambda: groom.set_rendering(scene, True), head)
    measure("render off (preview back)", lambda: groom.set_rendering(scene, False), head)

    # A deforming scalp: a shape key animated, frame changes.
    head.shape_key_add(name="Basis")
    lift = head.shape_key_add(name="lift")
    for v in lift.data:
        v.co.z += 0.02
    lift.value = 0.0
    lift.keyframe_insert("value", frame=1)
    lift.value = 1.0
    lift.keyframe_insert("value", frame=20)
    scene.frame_set(1)
    groom.flush()
    frames = iter(range(2, 20))
    measure("frame change (shape-key scalp, follow scalp)", lambda: scene.frame_set(next(frames)), head, repeat=8)

    print("\nSUMMARY")
    for row in results:
        print("%-48s total %8.1f | requests %8.1f | eval %8.1f | write %7.1f | depsgraph %7.1f | python %7.1f | %d strands"
              % (row["label"], row["total"], row["ipc"], row["eval"], row["write"], row["depsgraph"], row["python"],
                 row["strands"]))


try:
    main()
finally:
    process.shutdown()
