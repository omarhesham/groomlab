# SPDX-License-Identifier: Apache-2.0
"""Shared setup for the GroomLab vs XGen Interactive Grooming benchmark (both workers import it).

One scene for both systems: the same scalp, the same strand count, points per strand, length and
look parameters, the same deformer. Each system has an adapter with the same verbs (create,
tweak, force evaluation, exports), so the workers time the same steps on both.

Runs inside Maya (mayapy or the GUI); nothing here starts processes.
"""

import ctypes
import ctypes.wintypes
import json
import math
import os
import time

from maya import cmds

# -- The shared look -----------------------------------------------------------------------------

HEAD_RADIUS = 10.0              # cm: a head-sized ellipsoid (scaled 1, 1.1, 1.2)
HEAD_SUBDIV = (160, 100)        # polySphere subdivisions before the lower part is cut away
HAIR_LENGTH = 15.0              # cm, straight along the normal (XGen's default hair; GroomLab's guides)
POINTS = 16                     # points per strand (XGen cvCount 16; GroomLab Resample 15 segments, even)
WIDTH = 0.01                    # cm at the root
CLUMPS = 400                    # clump centers on the whole scalp
CLUMP = 0.8                     # clump strength (XGen clump, GroomLab Clump.strength)
NOISE = 0.5                     # cm (XGen noise magnitude, GroomLab Noise.amplitude)
NOISE_FREQUENCY = 1.0
CURL = 0.5                      # XGen: the clump's curl (no per-strand coil modifier in 2022)
CURL_RADIUS = 0.3               # GroomLab Curl: cm
CURL_FREQUENCY = 0.5            # GroomLab Curl: turns per cm (7.5 a strand on 16 points; tolerance 0: those points, as measured since the start)
GUIDE_SPACING = 1.6             # GroomLab guides: about one per 1.6 x 1.6 cm of scalp (~350)
FRAMES = (1, 100)               # the deforming scalp's animation
PLAY = (1, 50)                  # the frames playback and per-frame evaluation time


def wall():
    return time.perf_counter()


# -- Process memory (Windows) --------------------------------------------------------------------

class _Counters(ctypes.Structure):
    _fields_ = [("cb", ctypes.wintypes.DWORD), ("PageFaultCount", ctypes.wintypes.DWORD),
                ("PeakWorkingSetSize", ctypes.c_size_t), ("WorkingSetSize", ctypes.c_size_t),
                ("QuotaPeakPagedPoolUsage", ctypes.c_size_t), ("QuotaPagedPoolUsage", ctypes.c_size_t),
                ("QuotaPeakNonPagedPoolUsage", ctypes.c_size_t), ("QuotaNonPagedPoolUsage", ctypes.c_size_t),
                ("PagefileUsage", ctypes.c_size_t), ("PeakPagefileUsage", ctypes.c_size_t),
                ("PrivateUsage", ctypes.c_size_t)]


def memory():
    """(working set, private bytes) of this process, in MB."""
    c = _Counters()
    c.cb = ctypes.sizeof(c)
    process = ctypes.windll.kernel32.GetCurrentProcess()
    ctypes.windll.psapi.GetProcessMemoryInfo(process, ctypes.byref(c), c.cb)
    return c.WorkingSetSize / 1048576.0, c.PrivateUsage / 1048576.0


# -- The scalp -----------------------------------------------------------------------------------

def make_head(name="head"):
    """The scalp both systems grow on: a sphere cut below 15% of its radius under the equator,
    scaled to a head's proportions, history deleted. Keeps polySphere's UVs. ~17k triangles."""
    import maya.api.OpenMaya as om
    sph = cmds.polySphere(radius=HEAD_RADIUS, subdivisionsX=HEAD_SUBDIV[0], subdivisionsY=HEAD_SUBDIV[1],
                          name=name)[0]
    sel = om.MSelectionList()
    sel.add(sph)
    it = om.MItMeshPolygon(sel.getDagPath(0))
    below = []
    while not it.isDone():
        if it.center().y < -0.15 * HEAD_RADIUS:
            below.append(it.index())
        it.next()
    cmds.delete(["%s.f[%d]" % (sph, i) for i in below])
    cmds.setAttr(sph + ".scale", 1.0, 1.1, 1.2)
    cmds.makeIdentity(sph, apply=True, scale=True)
    cmds.delete(sph, constructionHistory=True)
    return sph


def head_info(head):
    return {"triangles": cmds.polyEvaluate(head, triangle=True), "faces": cmds.polyEvaluate(head, face=True),
            "vertices": cmds.polyEvaluate(head, vertex=True), "area": cmds.polyEvaluate(head, worldArea=True),
            "uvs": cmds.polyEvaluate(head, uvcoord=True)}


def add_bend(head):
    """The deforming scalp: a bend deformer keyed over FRAMES (rest at the first frame)."""
    bend, handle = cmds.nonLinear(head, type="bend", lowBound=-1, highBound=1, curvature=0.0)
    cmds.setAttr(handle + ".translateY", -0.2 * HEAD_RADIUS)
    cmds.setAttr(handle + ".scale", 1.3 * HEAD_RADIUS, 1.3 * HEAD_RADIUS, 1.3 * HEAD_RADIUS)
    first, last = FRAMES
    # The bend's curvature reads in degrees here (0.5 moved the crown 0.06 cm): +-40 bends the
    # crown about 5 cm sideways and back.
    cmds.setKeyframe(bend, attribute="curvature", time=first, value=0.0)
    cmds.setKeyframe(bend, attribute="curvature", time=(first + last) / 2.0, value=40.0)
    cmds.setKeyframe(bend, attribute="curvature", time=last, value=-40.0)
    cmds.playbackOptions(minTime=first, maxTime=last, animationStartTime=first, animationEndTime=last)
    cmds.currentTime(first)
    return bend


def set_camera():
    """The same view for the viewport, the orbit and the renders."""
    cmds.setAttr("persp.translate", 0.0, 22.0, 62.0)
    cmds.setAttr("persp.rotate", -18.0, 0.0, 0.0)
    cmds.setAttr("perspShape.focalLength", 35.0)
    cmds.setAttr("perspShape.nearClipPlane", 0.1)
    cmds.setAttr("perspShape.farClipPlane", 10000.0)


def surface_normal_samples(head, spacing):
    """(point, normal) about every `spacing` on the scalp: a vertex per cell of a 3D grid."""
    import maya.api.OpenMaya as om
    sel = om.MSelectionList()
    sel.add(head)
    fn = om.MFnMesh(sel.getDagPath(0))
    points = fn.getPoints(om.MSpace.kWorld)
    cells = {}
    for i, p in enumerate(points):
        key = (int(math.floor(p.x / spacing)), int(math.floor(p.y / spacing)), int(math.floor(p.z / spacing)))
        cells.setdefault(key, i)
    out = []
    for i in sorted(cells.values()):
        n = fn.getVertexNormal(i, True, om.MSpace.kWorld)
        out.append(((points[i].x, points[i].y, points[i].z), (n.x, n.y, n.z)))
    return out


# -- XGen Interactive Grooming -------------------------------------------------------------------

class XGenIGS(object):
    """XGen Interactive Groom Splines: xgmSplineBase -> scale -> sculpt (the default chain) ->
    clump -> noise -> xgmSplineDescription. Modifiers are added the way the Interactive Groom
    Editor adds them (xgIgSplineEditorTreeModel.addModifier): at the end of the chain, and the
    clump modifier gets the base's outMeshData (without it the clump has no effect)."""

    name = "xgen"

    def __init__(self):
        if not cmds.pluginInfo("xgenToolkit", query=True, loaded=True):
            cmds.loadPlugin("xgenToolkit")
        self.shape = self.transform = self.base = self.clump = self.noise = None

    def create(self, head, count, name="xgHair"):
        cmds.select(head, replace=True)
        area = cmds.xgmCreateSplineDescription(query=True, area=True)
        self.area = area
        self.transform = cmds.xgmCreateSplineDescription(
            createDefaultHair=True, name=name, density=count / area, length=HAIR_LENGTH, widthScale=WIDTH,
            cvCount=POINTS, generatorSeed=0)
        self.shape = cmds.ls(self.transform, dag=True, type="xgmSplineDescription", long=True)[0]
        self.transform = cmds.listRelatives(self.shape, parent=True, fullPath=True)[0]
        self.base = [n for n in cmds.listHistory(self.shape) if cmds.nodeType(n) == "xgmSplineBase"][0]
        self.clump = self._insert("xgmModifierClump", "clump")
        cmds.connectAttr(self.base + ".outMeshData", self.clump + ".inMeshData")
        self.noise = self._insert("xgmModifierNoise", "noise")
        cmds.setAttr(self.clump + ".pointDensity", CLUMPS / area)
        cmds.setAttr(self.clump + ".clump", CLUMP)
        cmds.setAttr(self.clump + ".curl", CURL)
        # Clump and noise ramp root to tip, as GroomLab's default profiles.
        for node, ramp in ((self.clump, "clumpScale"), (self.noise, "magnitudeScale")):
            cmds.setAttr("%s.%s[0].%s_Position" % (node, ramp, ramp), 0.0)
            cmds.setAttr("%s.%s[0].%s_FloatValue" % (node, ramp, ramp), 0.0)
            cmds.setAttr("%s.%s[1].%s_Position" % (node, ramp, ramp), 1.0)
            cmds.setAttr("%s.%s[1].%s_FloatValue" % (node, ramp, ramp), 1.0)
        cmds.setAttr(self.noise + ".magnitude", NOISE)
        cmds.setAttr(self.noise + ".frequency", NOISE_FREQUENCY)
        return self.shape

    def _insert(self, node_type, name):
        node = cmds.createNode(node_type, name=name)
        src = cmds.listConnections(self.shape + ".inSplineData", source=True, destination=False, plugs=True)[0]
        cmds.disconnectAttr(src, self.shape + ".inSplineData")
        cmds.connectAttr(src, node + ".inSplineData")
        cmds.connectAttr(node + ".outSplineData", self.shape + ".inSplineData")
        return node

    def evaluate(self):
        """Forces the description's spline data (the whole modifier chain); IGS computes eagerly
        on a pull (a second pull of the same data is free; the time scales with the count)."""
        cmds.dgeval(self.shape + ".outSplineData")

    def count(self):
        return cmds.xgmSplineQuery(self.shape, splineCount=True)

    def set_clump(self, value):
        cmds.setAttr(self.clump + ".clump", value)

    def set_noise(self, value):
        cmds.setAttr(self.noise + ".magnitude", value)

    def delete(self):
        nodes = [self.transform, self.base, self.clump, self.noise]
        nodes += [n for n in cmds.listHistory(self.shape) or [] if cmds.nodeType(n).startswith("xgm")]
        cmds.delete([n for n in set(nodes) if n and cmds.objExists(n)])

    def export_alembic(self, path, start, end):
        cmds.select(self.transform, replace=True)
        cmds.xgmSplineCache(export=True, j='-file "%s" -df ogawa -fr %d %d -step 1 -obj %s'
                            % (path.replace("\\", "/"), start, end, self.transform.split("|")[-1]))
        return end - start + 1

    def assign(self, shading_group):
        cmds.sets(self.shape, edit=True, forceElement=shading_group)

    def info(self):
        return {"modifiers": ["xgmSplineBase", "xgmModifierScale", "xgmModifierSculpt", "xgmModifierClump",
                              "xgmModifierNoise"],
                "clumpPointDensity": cmds.getAttr(self.clump + ".pointDensity"),
                "cvCount": cmds.getAttr(self.base + ".cvCount"),
                "openCL": bool(cmds.xgmSplineQuery(isCLReady=True))}


# -- GroomLab ------------------------------------------------------------------------------------

def groomlab_graph(density, length, curl=True):
    """Scatter -> Interpolate (guides) -> Clump -> Noise -> Curl -> Width -> Resample.
    curl=False: without Curl, the operators XGen runs here (clump, noise) and no more."""
    graph = {
        "nodes": [
            {"name": "scalp", "type": "Scalp"},
            {"name": "guides", "type": "Guides"},
            {"name": "scatter", "type": "Scatter", "params": {"density": density, "length": length}},
            {"name": "interpolate", "type": "Interpolate"},
            {"name": "clump", "type": "Clump", "params": {"count": CLUMPS, "strength": CLUMP}},
            {"name": "noise", "type": "Noise", "params": {"amplitude": NOISE, "frequency": NOISE_FREQUENCY}},
            {"name": "curl", "type": "Curl", "params": {"radius": CURL_RADIUS, "frequency": CURL_FREQUENCY, "tolerance": 0.0}},
            {"name": "width", "type": "Width", "params": {"width": WIDTH}},
            {"name": "resample", "type": "Resample", "params": {"segments": POINTS - 1, "mode": "even"}},
            {"name": "output", "type": "Output"},
        ],
        "connections": [
            {"from": "scalp", "to": "scatter", "input": "scalp"},
            {"from": "scatter", "to": "interpolate", "input": "strands"},
            {"from": "guides", "to": "interpolate", "input": "guides"},
            {"from": "scalp", "to": "interpolate", "input": "scalp"},
            {"from": "interpolate", "to": "clump", "input": "strands"},
            {"from": "clump", "to": "noise", "input": "strands"},
            {"from": "noise", "to": "curl", "input": "strands"},
            {"from": "curl", "to": "width", "input": "strands"},
            {"from": "width", "to": "resample", "input": "strands"},
            {"from": "resample", "to": "output", "input": "strands"},
        ],
        "output": "output",
    }
    if not curl:
        graph["nodes"] = [n for n in graph["nodes"] if n["name"] != "curl"]
        graph["connections"] = [c for c in graph["connections"] if "curl" not in (c["from"], c["to"])]
        graph["connections"].append({"from": "noise", "to": "width", "input": "strands"})
    return graph


class GroomLabGroom(object):
    """A groomLabShape on the scalp with guides straight along the normals (the look XGen's
    default hair has), the graph above, previewDensity 1 (the viewport shows every strand, as
    XGen's does). follow=True: followScalp on (needs the deformer to exist first)."""

    name = "groomlab"

    def __init__(self, plugin=None, follow=False, curl=True):
        import groomlab  # noqa: F401  (the caller put the scripts on sys.path)
        if plugin and not cmds.pluginInfo(os.path.basename(plugin), query=True, loaded=True):
            cmds.loadPlugin(plugin)
        self.gl = groomlab
        self.follow = follow
        self.curl = curl
        self.shape = self.transform = None
        self.guide_count = 0

    def guide_curves(self, head):
        curves = []
        for (p, n) in surface_normal_samples(head, GUIDE_SPACING):
            pts = [(p[0] + n[0] * HAIR_LENGTH * k / 3.0, p[1] + n[1] * HAIR_LENGTH * k / 3.0,
                    p[2] + n[2] * HAIR_LENGTH * k / 3.0) for k in range(4)]
            curves.append(cmds.curve(degree=1, point=pts))
        return curves

    def create(self, head, count, curves=None, name="groomLab1"):
        """curves: guide curves made beforehand (inputs, as the scalp; not timed), deleted here."""
        gl = self.gl
        area = cmds.polyEvaluate(head, worldArea=True)
        self.shape = gl.create_groom(head, name=name)
        self.transform = cmds.listRelatives(self.shape, parent=True, fullPath=True)[0]
        if curves:
            ids = gl.guides_from_curves(self.shape, curves, tube_radius=0.8)
            self.guide_count = len(ids or [])
        cmds.setAttr(self.shape + ".previewDensity", 1.0)
        cmds.setAttr(self.shape + ".displayGuides", False)  # XGen draws no guides either
        gl.set_graph(self.shape, groomlab_graph(count / area, HAIR_LENGTH, self.curl))
        if self.follow:
            gl.set_follow_scalp(self.shape, True)
        return self.shape

    def evaluate(self):
        """Forces the shape's evaluation (outStrandCount: the strands at previewDensity = 1)."""
        return cmds.getAttr(self.shape + ".outStrandCount")

    def count(self):
        return cmds.getAttr(self.shape + ".outStrandCount")

    def set_clump(self, value):
        self.gl.set_param(self.shape, "clump", "strength", value)

    def set_noise(self, value):
        self.gl.set_param(self.shape, "noise", "amplitude", value)

    def delete(self):
        cmds.delete(self.transform)

    def export_alembic(self, path, start, end):
        return self.gl.export_alembic(self.shape, path, start, end)

    def assign(self, shading_group):
        self.gl.assign_shading_group(self.shape, shading_group)

    def draw_times(self):
        """The last evaluation's time in compute and the last viewport update's breakdown
        (groomLabGraphStats -drawTimes: report-only), or None on a build without it."""
        try:
            return json.loads(cmds.groomLabGraphStats(self.shape, drawTimes=True))
        except Exception:
            return None

    def info(self):
        stats = json.loads(cmds.groomLabGraphStats(self.shape) or "{}")
        return {"guides": self.guide_count, "curl": self.curl, "followScalp": bool(cmds.getAttr(self.shape + ".followScalp")),
                "points": stats.get("points"), "strands": stats.get("strands"),
                "lastEvaluationNodeMs": stats.get("times"), "version": cmds.groomLabVersion()}


def system(name, plugin=None):
    if name == "xgen":
        return XGenIGS()
    if name == "groomlab":
        return GroomLabGroom(plugin)
    if name in ("groomlab_follow", "groomlab_nocurl", "groomlab_gpu"):
        g = GroomLabGroom(plugin, follow=name == "groomlab_follow", curl=name != "groomlab_nocurl")
        g.name = name
        return g
    raise ValueError(name)


def settle():
    """The same session state for both: Evaluation Manager parallel, cached playback off."""
    try:
        cmds.evaluationManager(mode="parallel")
    except Exception:
        pass
    try:
        cmds.evaluator(name="cache", enable=False)
    except Exception:
        pass
