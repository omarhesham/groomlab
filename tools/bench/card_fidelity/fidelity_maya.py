# SPDX-License-Identifier: Apache-2.0
"""The Maya side of the card fidelity harness (run_fidelity.py starts it in a Maya window).

Builds each test groom on the repository's head scalp (tests/data/app/head_scalp.obj), then
draws it in Viewport 2.0 from the same cameras twice: its strands, and its game cards at each
level of detail (cards_preset, bake_cards, the cards view), each over a black and a white
background (the head a holdout, useBackground, so it hides the hair behind it and counts as no
hair), which gives the metrics each pixel's coverage and color. Writes into FIDELITY_OUT:

    <groom>/<view>_<what>_<bg>.png   what: strands, hero, mid, low; bg: k (black), w (white)
    <groom>/points.bin               the viewport's strand points (float32 xyz), for root/tip regions
    meta.json                        cameras, image size, counts, card budgets, timings

Reads FIDELITY_OUT, FIDELITY_GROOMS (long,fur,curly), FIDELITY_LODS (hero,mid,low),
FIDELITY_SIZE (pixels, square), FIDELITY_CAP (1: the hair cap shows with the cards, as the
exports carry it; 0: none), GROOMLAB_PLUGIN, GROOMLAB_SCRIPTS, FIDELITY_SCALP. Never opens a
dialog; quits Maya when done (exit code 1 and the error in meta.json on a failure).
"""

import json
import os
import sys
import time
import traceback

import maya.mel
import maya.utils
from maya import cmds

OUT = os.environ.get("FIDELITY_OUT", "")
SIZE = int(os.environ.get("FIDELITY_SIZE", "720"))
GROOMS = [g for g in os.environ.get("FIDELITY_GROOMS", "long,fur,curly").split(",") if g]
LODS = [l for l in os.environ.get("FIDELITY_LODS", "hero,mid,low").split(",") if l]
CAP = os.environ.get("FIDELITY_CAP", "1") == "1"  # the hair cap (bake_scalp) under the cards

# Unit directions from the head's center to the camera, and the camera's up.
VIEWS = (
    ("front", (0.0, 0.0, 1.0), (0.0, 1.0, 0.0)),
    ("threeq", (0.7071, 0.0, 0.7071), (0.0, 1.0, 0.0)),
    ("side", (1.0, 0.0, 0.0), (0.0, 1.0, 0.0)),
    ("back", (0.0, 0.0, -1.0), (0.0, 1.0, 0.0)),
    ("top", (0.0, 1.0, 0.0), (0.0, 0.0, -1.0)),
)
FOCAL = 50.0       # mm, on a 1 x 1 inch film back: tan(half the view) = 12.7 / 50
APERTURE = 1.0
SEGMENTS = 15      # every test graph ends its strands with Resample at this many segments


# -- The test grooms ----------------------------------------------------------------------------

def _graph(nodes, chain, extra=()):
    """A graph from (name, type, params) nodes; `chain` the strands' path to the output, each node
    fed by the one before it (Scalp and Guides wired to the nodes that take them)."""
    out = {"version": 1, "nodes": [{"name": "scalp", "type": "Scalp"}, {"name": "guides", "type": "Guides"}],
           "connections": [], "output": "output"}
    for name, kind, params in nodes:
        out["nodes"].append({"name": name, "type": kind, "params": params} if params else {"name": name, "type": kind})
    out["nodes"].append({"name": "output", "type": "Output"})
    con = out["connections"]
    con.append({"from": "scalp", "to": "scatter", "input": "scalp"})
    for a, b in zip(chain, chain[1:] + ["output"]):
        con.append({"from": a, "to": b, "input": "strands"})
    con.append({"from": "guides", "to": "interpolate", "input": "guides"})
    con.append({"from": "scalp", "to": "interpolate", "input": "scalp"})
    con.extend(extra)
    return out


def _clumped(name_clump, count, strength, profile):
    """A Clump picking about `count` centers of its own."""
    return [(name_clump, "Clump", {"count": count, "strength": strength, "profile": profile})], []


def long_graph():
    """The user's kind of graph: Interpolate, Clump twice, Noise twice, Flyaway, Length,
    HairColor (a root-to-tip blend), Width, Resample, Cards. Shoulder-length hair combed down."""
    big, big_extra = _clumped("bigClump", 140, 0.55, [[0, 0], [0.5, 0.6], [1, 1]])
    small, small_extra = _clumped("smallClump", 700, 0.45, [[0, 0], [0.4, 0.5], [1, 1]])
    nodes = [("scatter", "Scatter", {"density": 160.0, "length": 22.0}), ("interpolate", "Interpolate", None),
             ("combAlong", "Bend", {"angle": 56.0, "along": "direction", "direction": [0, -1, 0]}),
             ("combDown", "Gravity", {"strength": 0.85}),
             ("collide", "Collide", {"offset": 0.08, "maxDepth": 30.0, "iterations": 3})] + big + small + [
             ("noise", "Noise", {"amplitude": 0.3, "frequency": 2.0}),
             ("noiseFine", "Noise", {"amplitude": 0.06, "frequency": 12.0, "seed": 3}),
             ("flyaway", "Flyaway", {"fraction": 0.03, "stray": 0.8}),
             ("length", "Length", {"length": 1.0, "trimMax": 0.15}),
             ("color", "HairColor", {"melanin": 0.4, "redness": 0.25, "melaninRandom": 0.12, "tipColor": [0.42, 0.3, 0.18],
                                     "tipStrength": 0.6}),
             ("width", "Width", {"width": 0.008, "profile": [[0, 1], [1, 0.3]]}),
             ("resample", "Resample", {"segments": SEGMENTS, "mode": "even"}),
             ("cards", "Cards", None)]
    chain = ["scatter", "interpolate", "combAlong", "combDown", "collide", "bigClump", "smallClump", "noise", "noiseFine",
             "flyaway", "length", "color", "width", "resample", "cards"]
    return _graph(nodes, chain, big_extra + small_extra)


def fur_graph():
    """Short fur: dense, 1.6 cm, leaning back along the scalp, small clumps."""
    clump, extra = _clumped("centers", "clump", "combAlong", 1500, 0.5, [[0, 0], [1, 1]])
    nodes = [("scatter", "Scatter", {"density": 260.0, "length": 1.6}), ("interpolate", "Interpolate", None),
             ("combAlong", "Bend", {"angle": 51.0, "along": "direction", "direction": [0, -0.3, -1]})] + clump + [
             ("noise", "Noise", {"amplitude": 0.05, "frequency": 6.0}),
             ("color", "HairColor", {"melanin": 0.25, "redness": 0.5, "melaninRandom": 0.15, "tipColor": [0.6, 0.5, 0.38],
                                     "tipStrength": 0.7}),
             ("width", "Width", {"width": 0.006, "profile": [[0, 1], [1, 0.2]]}),
             ("resample", "Resample", {"segments": SEGMENTS, "mode": "even"}),
             ("cards", "Cards", None)]
    chain = ["scatter", "interpolate", "combAlong", "clump", "noise", "color", "width", "resample", "cards"]
    return _graph(nodes, chain, extra)


def curly_graph():
    """Curly: long helix curls coherent per clump (ringlets), falling down the head."""
    clump, extra = _clumped("centers", "clump", "collide", 260, 0.7, [[0, 0.2], [0.3, 0.8], [1, 1]])
    nodes = [("scatter", "Scatter", {"density": 140.0, "length": 20.0}), ("interpolate", "Interpolate", None),
             ("combAlong", "Bend", {"angle": 56.0, "along": "direction", "direction": [0, -1, 0]}),
             ("combDown", "Gravity", {"strength": 0.7}),
             ("collide", "Collide", {"offset": 0.08, "maxDepth": 30.0, "iterations": 3})] + clump + [
             ("curl", "Curl", {"radius": 0.7, "frequency": 0.45, "randomness": 0.3, "coherence": "clump"}),
             ("noise", "Noise", {"amplitude": 0.08, "frequency": 6.0}),
             ("color", "HairColor", {"melanin": 0.75, "redness": 0.15, "melaninRandom": 0.1, "tipColor": [0.2, 0.12, 0.07],
                                     "tipStrength": 0.5}),
             ("width", "Width", {"width": 0.009, "profile": [[0, 1], [1, 0.4]]}),
             ("resample", "Resample", {"segments": SEGMENTS, "mode": "even"}),
             ("cards", "Cards", None)]
    chain = ["scatter", "interpolate", "combAlong", "combDown", "collide", "clump", "curl", "noise", "color", "width",
             "resample", "cards"]
    return _graph(nodes, chain, extra)


def banded_graph():
    """Agouti fur: the short fur with banded guard hairs (a dark root, a pale band, a black tip,
    the bands shifted hair to hair) over a plain undercoat (HairColor's tip, a Bands node)."""
    clump, extra = _clumped("centers", "clump", "combAlong", 1500, 0.5, [[0, 0], [1, 1]])
    nodes = [("scatter", "Scatter", {"density": 260.0, "length": 1.8}), ("interpolate", "Interpolate", None),
             ("length", "Length", {"trimMax": 0.4}),
             ("combAlong", "Bend", {"angle": 51.0, "along": "direction", "direction": [0, -0.3, -1]})] + clump + [
             ("noise", "Noise", {"amplitude": 0.05, "frequency": 6.0}),
             ("color", "HairColor", {"melanin": 0.62, "redness": 0.35, "melaninRandom": 0.08,
                                     "tipColor": [0.02, 0.018, 0.015], "tipStrength": 1.0,
                                     "tipProfile": [[0, 0], [0.88, 0], [0.94, 1], [1, 1]]}),
             ("bands", "Bands", {"melanin": 0.1, "redness": 0.75, "randomness": 0.08,
                                 "profile": [[0, 0], [0.4, 0], [0.5, 1], [0.66, 1], [0.72, 0], [1, 0]],
                                 "offsetRandom": 0.08, "guards": 0.4, "undercoatDye": [0.85, 0.82, 0.8]}),
             ("width", "Width", {"width": 0.006, "profile": [[0, 1], [1, 0.2]]}),
             ("resample", "Resample", {"segments": SEGMENTS, "mode": "even"}),
             ("cards", "Cards", None)]
    chain = ["scatter", "interpolate", "length", "combAlong", "clump", "noise", "color", "bands", "width", "resample",
             "cards"]
    return _graph(nodes, chain, extra)


def barred_graph():
    """Barred feathers lying on the head: contour vanes with a downy base, white bars across the
    left vane and an iridescent green right one (a FeatherColor node), carded per feather."""
    nodes = [("scatter", "Scatter", {"density": 0.35, "length": 5.0, "seed": 11}), ("interpolate", "Interpolate", None),
             ("combAlong", "Bend", {"angle": 84.0, "along": "direction", "direction": [0, -0.4, -1]}),
             ("propagate", "Propagate", {"count": 40, "start": 0.06, "end": 1.0, "segments": 8, "length": 1.4,
                                         "lengthProfile": [[0, 0.35], [0.45, 1], [0.9, 0.65], [1, 0.4]],
                                         "angle": 55.0, "angleProfile": [[0, 1.2], [0.7, 0.9], [1, 0.4]], "curl": 18.0,
                                         "droop": 8.0, "splits": 3.0, "splitAmount": 0.4, "lengthRandom": 0.08,
                                         "down": 0.3, "downCount": 18, "downLength": 0.9}),
             ("color", "HairColor", {"melanin": 0.55, "redness": 0.15, "dye": [0.6, 0.62, 0.55], "melaninRandom": 0.05}),
             ("feathers", "FeatherColor", {"pattern": "bars", "vanes": "left", "dye": [0.95, 0.95, 0.92],
                                           "count": 6.0, "width": 0.42, "angle": 12.0,
                                           "offsetRandom": 0.25, "downColor": [0.92, 0.91, 0.88], "downStrength": 1.0,
                                           "iridescence": 0.85, "iridescenceOn": "rightVane",
                                           "iridescenceColor": [0.04, 0.45, 0.12], "iridescenceShift": 0.22}),
             ("width", "Width", {"width": 0.012, "profile": [[0, 1], [1, 0.4]]}),
             ("resample", "Resample", {"segments": SEGMENTS, "mode": "even"}),
             ("cards", "Cards", None)]
    chain = ["scatter", "interpolate", "combAlong", "propagate", "color", "feathers", "width", "resample", "cards"]
    graph = _graph(nodes, chain)
    # Propagate takes shafts on its own input name.
    for c in graph["connections"]:
        if c["to"] == "propagate":
            c["input"] = "shafts"
    return graph


GRAPHS = {"long": long_graph, "fur": fur_graph, "curly": curly_graph, "banded": banded_graph, "barred": barred_graph}


# -- The scene ----------------------------------------------------------------------------------

def _holdout(meshes):
    """The meshes drawn as the background (useBackground): they hide what is behind them and
    read as no hair in both passes."""
    shader = cmds.shadingNode("useBackground", asShader=True, name="fidelityHoldout")
    group = cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name=shader + "SG")
    cmds.connectAttr(shader + ".outColor", group + ".surfaceShader")
    for mesh in meshes:
        cmds.sets(mesh, edit=True, forceElement=group)


def build_scene(kind):
    import groomlab
    cmds.file(new=True, force=True)
    scalp = cmds.file(os.environ["FIDELITY_SCALP"], i=True, type="OBJ", returnNewNodes=True,
                      options="mo=0", ignoreVersion=True)
    scalp = [n for n in scalp if cmds.nodeType(n) == "transform"][0]
    box = cmds.exactWorldBoundingBox(scalp)
    # The skull under the cap (the scalp is the hair's part of it): the ellipsoid the cap was cut
    # from (make_head_scalp.py: 8 x 10 x 9 cm half-axes, the crown at the cap's top), a little in.
    head = cmds.polySphere(radius=1.0, subdivisionsX=64, subdivisionsY=48, name="fidelityHead")[0]
    cmds.setAttr(head + ".scale", 7.9, 9.9, 8.9)
    cmds.setAttr(head + ".translate", 0.5 * (box[0] + box[3]), box[4] - 10.0, 0.5 * (box[2] + box[5]))
    _holdout([scalp, head])
    groom = groomlab.create_groom(scalp, name="fidelity_" + kind)
    groomlab.set_graph(groom, GRAPHS[kind]())
    cmds.setAttr(groom + ".previewDensity", 1.0)
    cmds.setAttr(groom + ".previewPointBudget", 0.0)
    cmds.setAttr(groom + ".displayGuides", False)
    return groom, scalp


def set_background(rgb):
    cmds.displayPref(displayGradient=False)
    cmds.displayRGBColor("background", *rgb)


def frame(groom):
    """Center of the groom's strands' box and how far a camera must frame (half its largest side,
    a little more), and the points."""
    xyz = cmds.groomLabGraphStats(groom, points=True) or []
    lo = [min(xyz[k::3]) for k in range(3)]
    hi = [max(xyz[k::3]) for k in range(3)]
    center = [0.5 * (a + b) for a, b in zip(lo, hi)]
    radius = 0.55 * max(b - a for a, b in zip(lo, hi))
    return center, radius, xyz


def make_cameras(center, radius):
    """A camera per view, the whole groom in frame from every side."""
    tan_half = (APERTURE * 25.4 * 0.5) / FOCAL
    distance = radius / tan_half + radius
    cams = {}
    for name, direction, up in VIEWS:
        eye = [c + d * distance for c, d in zip(center, direction)]
        cam = cmds.camera(name="fidelity_" + name, focalLength=FOCAL, horizontalFilmAperture=APERTURE,
                          verticalFilmAperture=APERTURE, filmFit="fill", nearClipPlane=0.5,
                          farClipPlane=distance * 4.0, overscan=1.0)[0]
        cmds.viewPlace(cam, eye=eye, lookAt=center, up=up)
        cams[name] = {"camera": cam, "eye": eye, "target": center, "up": list(up), "tanHalf": tan_half}
    return cams


def panel():
    """The model panel shown (the single perspective view)."""
    models = cmds.getPanel(type="modelPanel") or []
    return next(p for p in cmds.getPanel(visiblePanels=True) or [] if p in models)


def shoot(view_panel, cams, folder, what):
    for name, cam in cams.items():
        cmds.lookThru(view_panel, cam["camera"])
        for bg, rgb in (("k", (0.0, 0.0, 0.0)), ("w", (1.0, 1.0, 1.0))):
            set_background(rgb)
            cmds.refresh(force=True)
            cmds.playblast(frame=[1], format="image", compression="png", viewer=False, showOrnaments=False,
                           offScreen=True, percent=100, widthHeight=(SIZE, SIZE), forceOverwrite=True,
                           completeFilename=os.path.join(folder, "%s_%s_%s.png" % (name, what, bg)))


def run_groom(kind, view_panel, meta):
    import groomlab
    folder = os.path.join(OUT, kind)
    if not os.path.isdir(folder):
        os.makedirs(folder)
    started = time.time()
    groom, scalp = build_scene(kind)
    groomlab.compare_cards(groom, False)
    center, radius, xyz = frame(groom)
    strands = cmds.getAttr(groom + ".outStrandCount")
    entry = {"strands": strands, "points": len(xyz) // 3, "segments": SEGMENTS, "center": center, "radius": radius,
             "errors": groomlab.errors(groom), "evaluateSeconds": time.time() - started, "lods": {}}
    import array
    array.array("f", xyz).tofile(open(os.path.join(folder, "points.bin"), "wb"))
    cams = make_cameras(center, radius)
    entry["views"] = {name: {k: v for k, v in cam.items() if k != "camera"} for name, cam in cams.items()}
    # Both sides lit, as compare_cards sets for the cards: the strands drawn the same way.
    cmds.modelEditor(view_panel, edit=True, twoSidedLighting=True, displayAppearance="smoothShaded",
                     displayLights="default", grid=False, headsUpDisplay=False, manipulators=False)
    shoot(view_panel, cams, folder, "strands")
    if CAP:
        # The hair cap the exports carry (scalp=True, cap=True): it shows with the cards.
        groomlab.bake_scalp(groom, directory=os.path.join(folder, "maps_cap"), name=kind)
    for lod in LODS:
        t0 = time.time()
        groomlab.cards_preset(groom, lod)
        groomlab.compare_cards(groom, True)
        stats = groomlab.card_stats(groom)
        t1 = time.time()
        paths = groomlab.bake_cards(groom, directory=os.path.join(folder, "maps_" + lod), name=kind + "_" + lod)
        t2 = time.time()
        cmds.refresh(force=True)
        shoot(view_panel, cams, folder, lod)
        stats = groomlab.card_stats(groom)
        size = 0
        for p in paths:
            if p.endswith("_Color.tga"):
                with open(p, "rb") as f:
                    header = bytearray(f.read(18))
                size = header[12] | header[13] << 8  # the TGA's width
        entry["lods"][lod] = {"cards": stats["cards"], "triangles": stats["triangles"], "textures": stats["textures"],
                              "atlas": size, "buildSeconds": t1 - t0, "bakeSeconds": t2 - t1,
                              "errors": groomlab.errors(groom)}
        groomlab.compare_cards(groom, False)
    entry["seconds"] = time.time() - started
    meta["grooms"][kind] = entry


def main():
    meta = {"size": SIZE, "grooms": {}, "lods": LODS, "views": [v[0] for v in VIEWS], "cap": CAP}
    import maya.api.OpenMayaRender as omr
    meta["drawAPI"] = {omr.MRenderer.kDirectX11: "DirectX 11", omr.MRenderer.kOpenGLCoreProfile: "OpenGL Core Profile",
                       omr.MRenderer.kOpenGL: "OpenGL"}.get(omr.MRenderer.drawAPI(), str(omr.MRenderer.drawAPI()))
    code = 0
    try:
        maya.mel.eval('setNamedPanelLayout "Single Perspective View"')
        view_panel = panel()
        for kind in GROOMS:
            run_groom(kind, view_panel, meta)
    except Exception:
        meta["error"] = traceback.format_exc()
        code = 1
    with open(os.path.join(OUT, "meta.json"), "w") as f:
        json.dump(meta, f, indent=1)
    cmds.quit(force=True, exitCode=code)


def _load():
    try:
        scripts = os.environ["GROOMLAB_SCRIPTS"]
        if scripts not in sys.path:
            sys.path.insert(0, scripts)
        cmds.loadPlugin(os.environ["GROOMLAB_PLUGIN"])
        import groomlab  # noqa: F401
    except Exception:
        with open(os.path.join(OUT, "meta.json"), "w") as f:
            json.dump({"error": traceback.format_exc()}, f)
        cmds.quit(force=True, exitCode=1)
        return
    cmds.evalDeferred(main, lowestPriority=True)


maya.utils.executeDeferred(_load)
