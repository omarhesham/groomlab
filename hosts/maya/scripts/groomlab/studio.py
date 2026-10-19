# SPDX-License-Identifier: Apache-2.0
"""Studio lighting: a photo studio round the groom in one call, for showcase renders in Arnold.

    import groomlab
    groomlab.add_studio(groom)                      # the portrait look: softbox key upper left, fill, rim
    groomlab.add_studio(groom, preset="rim_drama")  # the same rig, relit in place
    groomlab.remove_studio()                        # gone; the head's material and the render settings back

(groomlab.add_studio is this module's studio(); groomlab.studio is the module.)

Everything goes under one group, `groomlab_studio`: a seamless cyclorama (a floor curving up into a
back wall), Arnold area lights (key, fill, rims, kicker, a background light), a dim sky dome, and
the camera `groomlab_studio_cam` framing the groom three-quarter on. The head (the groom's scalps
and colliders) can wear a matte light grey mannequin material, and the render settings can be set
for hair; what they were is kept on the group, so remove_studio puts it back.

Sizes and places are relative to the subject (the groom and its head) and turned for the scene's up
axis, so a head in centimetres, in metres or ten times too big gets the same picture. The lights
are not normalized: a light's brightness on the subject goes with the angle it covers, which stays
the same when everything scales, so exposures need no unit conversion.

Presets are data (PRESETS): add one by adding an entry.
"""

import json
import math

from maya import cmds

from groomlab.base import undo_chunk

GROUP = "groomlab_studio"
ORBIT = GROUP + "_orbit"
CAMERA = GROUP + "_cam"
LIGHTS = GROUP + "_lights"
DOME = GROUP + "_dome"
BACKDROP = GROUP + "_backdrop"
BACKDROP_MATERIAL = GROUP + "_backdropMtl"
MANNEQUIN_MATERIAL = GROUP + "_mannequinMtl"
STATE = "groomlabStudio"  # a string attribute on the group: what the studio changed, as JSON

# A light: `azimuth` in degrees round the subject from the camera's direction (positive toward the
# camera's right, 180 behind the subject), `elevation` above the subject (negative: below),
# `distance` from the subject and `size` (width, height) in subject sizes, `exposure` in stops
# (radiance: 0 is 1), a linear `color`, `spread` (1 soft, toward 0 a narrower beam), `target`
# "subject" (the hair) or "backdrop" (the wall behind it), and `backdrop` False for a light that
# leaves the backdrop unlit (a strip with a grid: Rim Drama's rims keep its floor dark).
# Each light is groomlab_studio_<name>.
#
# A preset: `backdrop` (the cyclorama's linear grey), `dome` (the sky dome's radiance), `camera`
# (`azimuth` from the subject's front toward its left side, which is the picture's right,
# `elevation`, `focal` in mm on a 36 mm film back), `lights`, and `turntable` (frames for the rig to
# turn the subject round once; 0 for none).
WARM = (1.0, 0.96, 0.91)
COOL = (0.86, 0.93, 1.0)
WHITE = (1.0, 1.0, 1.0)

PRESETS = {
    "portrait": {
        "label": "Portrait",
        "description": "A large softbox key from the upper left, a soft fill from the right, a rim from behind on "
                       "the right to lift the hair off the backdrop, on mid grey",
        "backdrop": 0.25, "dome": 0.04,
        "camera": {"azimuth": 30.0, "elevation": 4.0, "focal": 85.0},
        "lights": [
            {"name": "key", "azimuth": -62.0, "elevation": 40.0, "distance": 2.4, "size": (1.6, 1.2),
             "exposure": 2.6, "color": WARM, "spread": 0.35},
            {"name": "fill", "azimuth": 62.0, "elevation": 8.0, "distance": 2.6, "size": (1.4, 1.4),
             "exposure": 0.6, "color": WHITE, "spread": 1.0},
            {"name": "rim", "azimuth": 140.0, "elevation": 32.0, "distance": 2.0, "size": (0.5, 1.4),
             "exposure": 6.5, "color": COOL, "spread": 0.5},
            {"name": "kicker", "azimuth": -130.0, "elevation": 18.0, "distance": 2.0, "size": (0.4, 1.0),
             "exposure": 3.2, "color": WHITE, "spread": 0.6},
            {"name": "background", "azimuth": 180.0, "elevation": -10.0, "distance": 1.0, "size": (1.0, 1.0),
             "exposure": 3.5, "color": WHITE, "spread": 0.8, "target": "backdrop"},
        ],
        "turntable": 0,
    },
    "beauty": {
        "label": "Beauty",
        "description": "Clamshell: the key above the camera, a fill from below, two soft rims; even and flattering",
        "backdrop": 0.25, "dome": 0.05,
        "camera": {"azimuth": 15.0, "elevation": 3.0, "focal": 85.0},
        "lights": [
            {"name": "key", "azimuth": -6.0, "elevation": 42.0, "distance": 2.4, "size": (1.8, 1.0),
             "exposure": 3.0, "color": WARM, "spread": 0.9},
            {"name": "fill", "azimuth": 0.0, "elevation": -28.0, "distance": 2.2, "size": (1.6, 0.7),
             "exposure": 1.6, "color": WHITE, "spread": 1.0},
            {"name": "rim", "azimuth": 135.0, "elevation": 28.0, "distance": 2.0, "size": (0.6, 1.4),
             "exposure": 4.0, "color": WHITE, "spread": 0.7},
            {"name": "rim2", "azimuth": -135.0, "elevation": 28.0, "distance": 2.0, "size": (0.6, 1.4),
             "exposure": 4.0, "color": WHITE, "spread": 0.7},
            {"name": "background", "azimuth": 180.0, "elevation": -10.0, "distance": 1.0, "size": (1.2, 1.0),
             "exposure": 3.2, "color": WHITE, "spread": 0.9, "target": "backdrop"},
        ],
        "turntable": 0,
    },
    "rim_drama": {
        "label": "Rim Drama",
        "description": "A dark backdrop, a low fill and two strong rims from behind: the silhouette and every "
                       "flyaway show",
        "backdrop": 0.03, "dome": 0.01,
        "camera": {"azimuth": 30.0, "elevation": 3.0, "focal": 85.0},
        "lights": [
            {"name": "key", "azimuth": -70.0, "elevation": 30.0, "distance": 2.6, "size": (0.8, 1.0),
             "exposure": 3.2, "color": WARM, "spread": 0.6},
            {"name": "fill", "azimuth": 40.0, "elevation": 5.0, "distance": 2.8, "size": (1.2, 1.2),
             "exposure": 0.8, "color": WHITE, "spread": 1.0},
            {"name": "rim", "azimuth": 145.0, "elevation": 25.0, "distance": 1.8, "size": (0.4, 1.4),
             "exposure": 7.6, "color": COOL, "backdrop": False, "spread": 0.5},
            {"name": "rim2", "azimuth": -150.0, "elevation": 30.0, "distance": 1.8, "size": (0.4, 1.4),
             "exposure": 6.0, "color": WARM, "backdrop": False, "spread": 0.5},
        ],
        "turntable": 0,
    },
    "high_key": {
        "label": "High Key",
        "description": "A bright backdrop and soft, even light from the front; few shadows",
        "backdrop": 0.6, "dome": 0.25,
        "camera": {"azimuth": 25.0, "elevation": 4.0, "focal": 85.0},
        "lights": [
            {"name": "key", "azimuth": -30.0, "elevation": 30.0, "distance": 2.4, "size": (2.0, 1.6),
             "exposure": 2.4, "color": WHITE, "spread": 1.0},
            {"name": "fill", "azimuth": 50.0, "elevation": 10.0, "distance": 2.4, "size": (2.0, 1.6),
             "exposure": 1.6, "color": WHITE, "spread": 1.0},
            {"name": "rim", "azimuth": 150.0, "elevation": 35.0, "distance": 2.0, "size": (0.8, 1.4),
             "exposure": 3.5, "color": WHITE, "spread": 0.8},
            {"name": "background", "azimuth": 180.0, "elevation": 20.0, "distance": 1.2, "size": (2.0, 1.4),
             "exposure": 2.0, "color": WHITE, "spread": 1.0, "target": "backdrop"},
        ],
        "turntable": 0,
    },
    "turntable": {
        "label": "Turntable",
        "description": "The portrait lighting with the subject turning round once (the camera, lights and backdrop "
                       "orbit it) over 120 frames",
        "like": "portrait",
        "turntable": 120,
    },
}

# Render settings the studio sets for hair (Arnold), and the resolution.
ARNOLD_SETTINGS = (
    ("defaultArnoldRenderOptions.AASamples", 5),
    ("defaultArnoldRenderOptions.GIDiffuseSamples", 2),
    ("defaultArnoldRenderOptions.GISpecularSamples", 2),
    ("defaultArnoldRenderOptions.GITransmissionSamples", 2),
    ("defaultArnoldRenderOptions.GISpecularDepth", 2),
    ("defaultArnoldRenderOptions.GITransmissionDepth", 10),  # light hair lets light through many strands
)
RESOLUTION = (1920, 1080)
FILM_BACK = 1.4173  # inches: 36 mm across


def preset_names():
    """The presets in menu order."""
    order = ["portrait", "beauty", "rim_drama", "high_key", "turntable"]
    return [n for n in order if n in PRESETS] + sorted(n for n in PRESETS if n not in order)


def get_preset(name):
    """A preset with what it takes from the one it is `like` filled in."""
    if name not in PRESETS:
        raise ValueError("no studio preset %r (%s)" % (name, ", ".join(preset_names())))
    data = dict(PRESETS[name])
    if data.get("like"):
        base = get_preset(data["like"])
        base.update({k: v for k, v in data.items() if k != "like"})
        data = base
    return data


# -- Vectors --------------------------------------------------------------------------------------

def _add(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


def _sub(a, b):
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]


def _mul(a, s):
    return [a[0] * s, a[1] * s, a[2] * s]


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def _unit(a):
    length = math.sqrt(_dot(a, a))
    return _mul(a, 1.0 / length) if length > 1e-12 else [0.0, 0.0, 0.0]


def _axes():
    """(up, front): the scene's up axis and the way a subject faces (+z with y up, -y with z up,
    as Maya's front views and the samples have it)."""
    if cmds.upAxis(query=True, axis=True) == "z":
        return [0.0, 0.0, 1.0], [0.0, -1.0, 0.0]
    return [0.0, 1.0, 0.0], [0.0, 0.0, 1.0]


def _direction(azimuth, elevation, back, right, up):
    """A unit direction `azimuth` degrees round from `back` toward `right`, `elevation` up."""
    a, e = math.radians(azimuth), math.radians(elevation)
    flat = _add(_mul(back, math.cos(a)), _mul(right, math.sin(a)))
    return _unit(_add(_mul(flat, math.cos(e)), _mul(up, math.sin(e))))


def _aim_matrix(position, target, up, sx=1.0, sy=1.0):
    """A world matrix (Maya's row order) at `position` whose -Z looks at `target`, X scaled by `sx`
    and Y by `sy`: a camera's or an area light's (both face -Z)."""
    z = _unit(_sub(position, target))
    x = _cross(up, z)
    if _dot(x, x) < 1e-10:  # straight up or down: any side will do
        x = _cross([1.0, 0.0, 0.0] if abs(z[0]) < 0.9 else [0.0, 1.0, 0.0], z)
    x = _unit(x)
    y = _cross(z, x)
    x, y = _mul(x, sx), _mul(y, sy)
    return x + [0.0] + y + [0.0] + z + [0.0] + list(position) + [1.0]


# -- The subject ----------------------------------------------------------------------------------

def _box(nodes):
    nodes = [n for n in nodes if n and cmds.objExists(n)]
    return list(cmds.exactWorldBoundingBox(nodes)) if nodes else None


def _union(a, b):
    if not a:
        return b
    if not b:
        return a
    return [min(a[i], b[i]) for i in range(3)] + [max(a[i + 3], b[i + 3]) for i in range(3)]


def _transform(shape):
    return cmds.listRelatives(shape, parent=True, fullPath=True)[0]


def head_meshes(groom):
    """The head: the meshes the mannequin material dresses and the floor goes under (the camera
    frames them only when there is no hair yet): the groom's scalp, its other scalps and its
    colliders (their visible shapes, long names)."""
    import groomlab
    shapes = []
    found = [groomlab.scalp_mesh(groom)] + [s["mesh"] for s in groomlab.scalps(groom)]
    found += [c["mesh"] for c in groomlab.colliders(groom)]
    for shape in found:
        if not shape or not cmds.objExists(shape):
            continue
        for visible in cmds.listRelatives(_transform(shape), shapes=True, type="mesh", noIntermediate=True,
                                          fullPath=True) or []:
            if visible not in shapes:
                shapes.append(visible)
    return shapes


def _hair_box(groom):
    """The world bounds of the strands the viewport draws, a few stray hairs left out (0.3% of the
    points at each end of each axis, from a sample of them); the node's bounds when none are drawn."""
    try:
        xyz = cmds.groomLabGraphStats(groom, points=True) or []
    except RuntimeError:
        xyz = []
    count = len(xyz) // 3
    if count < 1:
        return _box([_transform(groom)])
    step = max(1, count // 20000)
    low, high = [], []
    for k in range(3):
        values = sorted(xyz[k::3 * step])
        cut = int(len(values) * 0.003)
        low.append(values[cut])
        high.append(values[len(values) - 1 - cut])
    return low + high


def subject(groom):
    """{"box": what the camera frames (the hair's world bounds; the head's without hair), "hair":
    its center, "size": its largest extent, "bottom": the lowest corner of the hair and head
    meshes (the floor goes just under it)}."""
    def size_of(box):
        return max(box[3] - box[0], box[4] - box[1], box[5] - box[2]) if box else 0.0
    hair_box = _hair_box(groom)
    head_box = _box([_transform(s) for s in head_meshes(groom)])
    everything = _union(hair_box, head_box)
    if not everything or size_of(everything) <= 0.0:
        raise ValueError("%s has no bounds to light" % groom)
    box = hair_box if size_of(hair_box) > 1e-3 * size_of(everything) else everything
    corners = [[everything[0 + 3 * a], everything[1 + 3 * b], everything[2 + 3 * c]]
               for a in (0, 1) for b in (0, 1) for c in (0, 1)]
    up = _axes()[0]
    return {"box": box, "hair": [(box[i] + box[i + 3]) * 0.5 for i in range(3)], "size": size_of(box),
            "bottom": min(_dot(c, up) for c in corners)}


# -- State ----------------------------------------------------------------------------------------

def exists():
    """Whether the scene has a studio rig."""
    return cmds.objExists(GROUP)


def state():
    """What the studio rig records ({} without one): its preset, groom, and what it changed."""
    if not exists() or not cmds.attributeQuery(STATE, node=GROUP, exists=True):
        return {}
    try:
        return json.loads(cmds.getAttr(GROUP + "." + STATE) or "{}")
    except ValueError:
        return {}


def _save_state(data):
    if not cmds.attributeQuery(STATE, node=GROUP, exists=True):
        cmds.addAttr(GROUP, longName=STATE, dataType="string")
    cmds.setAttr(GROUP + "." + STATE, json.dumps(data), type="string")


def current_preset():
    """The studio's preset, or None without a studio."""
    return state().get("preset")


# -- Materials ------------------------------------------------------------------------------------

def _assignments(shape):
    """[[shading group, [members]]] of a mesh: what it is assigned, whole or per face."""
    shape = cmds.ls(shape, long=True)[0]
    owners = (shape, _transform(shape))
    out = []
    for group in sorted(set(cmds.listConnections(shape, type="shadingEngine") or [])):
        members = [m for m in cmds.ls(cmds.sets(group, query=True) or [], long=True) if m.split(".")[0] in owners]
        if members:
            out.append([group, members])
    return out


def _restore_assignments(shape, assignments):
    if not cmds.objExists(shape):
        return
    if not assignments:
        assignments = [["initialShadingGroup", [shape]]]
    for group, members in assignments:
        members = [m for m in members if cmds.objExists(m)]
        if members and cmds.objExists(group):
            cmds.sets(members, edit=True, forceElement=group)


def _surface(name, color, roughness, specular):
    """An aiStandardSurface in its own shading group (made or updated); returns the group."""
    if not cmds.objExists(name):
        cmds.shadingNode("aiStandardSurface", asShader=True, name=name)
    group = name + "SG"
    if not cmds.objExists(group):
        group = cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name=group)
        cmds.connectAttr(name + ".outColor", group + ".surfaceShader", force=True)
    cmds.setAttr(name + ".base", 1.0)
    cmds.setAttr(name + ".baseColor", color[0], color[1], color[2], type="double3")
    cmds.setAttr(name + ".specular", specular)
    cmds.setAttr(name + ".specularRoughness", roughness)
    return group


def _dress(groom, mannequin, data):
    """The mannequin material on the head meshes (their own assignments kept in `data`), or each
    back as it was."""
    saved = data.setdefault("materials", {})
    wanted = head_meshes(groom) if (mannequin and groom) else []
    for shape in list(saved):
        if shape not in wanted:
            _restore_assignments(shape, saved.pop(shape))
    if not wanted:
        return
    group = _surface(MANNEQUIN_MATERIAL, (0.4, 0.4, 0.4), 0.5, 0.3)
    for shape in wanted:
        if shape not in saved:
            saved[shape] = _assignments(shape)
        cmds.sets(shape, edit=True, forceElement=group)


# -- Render settings ------------------------------------------------------------------------------

def _plug_value(plug):
    value = cmds.getAttr(plug)
    return value[0] if isinstance(value, list) else value


def _render_settings(on, resolution, data):
    """Arnold's samples for hair and the resolution, the values before kept in `data` (and put
    back when `on` is False)."""
    saved = data.setdefault("settings", {})
    if not on:
        _restore_settings(saved)
        data["settings"] = {}
        return
    import mtoa.core
    mtoa.core.createOptions()
    wanted = list(ARNOLD_SETTINGS)
    if resolution:
        width, height = int(resolution[0]), int(resolution[1])
        wanted += [("defaultResolution.width", width), ("defaultResolution.height", height),
                   ("defaultResolution.pixelAspect", 1.0),
                   ("defaultResolution.deviceAspectRatio", float(width) / float(height))]
    for plug, value in wanted:
        if not cmds.objExists(plug):
            continue
        if plug not in saved:
            saved[plug] = _plug_value(plug)
        if plug == "defaultResolution.deviceAspectRatio":
            cmds.setAttr("defaultResolution.lockDeviceAspectRatio", False)
        cmds.setAttr(plug, value)


def _restore_settings(saved):
    for plug, value in saved.items():
        if cmds.objExists(plug):
            if plug == "defaultResolution.deviceAspectRatio":
                cmds.setAttr("defaultResolution.lockDeviceAspectRatio", False)
            cmds.setAttr(plug, value)


def _render_aspect():
    width = cmds.getAttr("defaultResolution.width")
    height = cmds.getAttr("defaultResolution.height")
    return float(width) / float(max(height, 1))


# -- The rig --------------------------------------------------------------------------------------

def _set(node, attr, value):
    """Set an attribute when the node has it (MtoA versions differ in a few)."""
    if not cmds.attributeQuery(attr, node=node, exists=True):
        return
    if isinstance(value, (list, tuple)):
        cmds.setAttr(node + "." + attr, *value, type="double3")
    else:
        cmds.setAttr(node + "." + attr, value)


def _child(name, parent, make):
    """The rig's node `name` under `parent`, made by `make()` (returning a transform) when missing."""
    if not cmds.objExists(name):
        node = make()
        if cmds.ls(node)[0].split("|")[-1] != name:
            node = cmds.rename(node, name)
    current = cmds.listRelatives(name, parent=True, fullPath=True)
    if not current or current[0].split("|")[-1] != parent:
        cmds.parent(name, parent)
    return name


def _group(name, parent=None):
    if not cmds.objExists(name):
        cmds.group(empty=True, name=name, world=parent is None)
        if parent:
            cmds.parent(name, parent)
    return name


def _shape(node):
    return cmds.listRelatives(node, shapes=True, fullPath=True)[0]


class _Frame(object):
    """The studio's frame round the subject: the camera's direction and the sizes."""

    def __init__(self, info, camera_preset, aspect):
        self.up, front = _axes()
        self.size = info["size"]
        box = info["box"]
        self.hair = info["hair"]
        side = _cross(self.up, front)  # the subject's left: the picture's right from the front
        self.back = _direction(camera_preset.get("azimuth", 30.0), 0.0, front, side, self.up)  # toward the camera
        self.right = _cross(self.up, self.back)  # the camera's right
        self.focal = float(camera_preset.get("focal", 85.0))
        self.elevation = float(camera_preset.get("elevation", 4.0))
        corners = [[box[0 + 3 * a], box[1 + 3 * b], box[2 + 3 * c]] for a in (0, 1) for b in (0, 1) for c in (0, 1)]
        center = [(box[i] + box[i + 3]) * 0.5 for i in range(3)]
        self.bottom = min(_dot(c, self.up) for c in corners)
        self.top = max(_dot(c, self.up) for c in corners)
        width = max(_dot(c, self.right) for c in corners) - min(_dot(c, self.right) for c in corners)
        depth = max(_dot(c, self.back) for c in corners) - min(_dot(c, self.back) for c in corners)
        # The camera: the subject's height (with headroom above, a little room below) fills the
        # frame's height, or its width the frame's width, whichever needs it further away; moved
        # back by half the subject's depth, so its nearer side fits too.
        half_h = math.atan(0.5 * FILM_BACK / aspect * 25.4 / self.focal)
        half_w = math.atan(0.5 * FILM_BACK * 25.4 / self.focal)
        # Long hair or a body below the head: a portrait shows the top of it (at most 1.6 times
        # as tall as it is wide), the head and shoulders.
        self.bottom = max(self.bottom, self.top - 1.6 * width)
        height = self.top - self.bottom
        headroom, footroom = 0.10, 0.04
        frame_height = 1.1 * height / (1.0 - headroom - footroom)
        frame_width = width * 1.25
        distance = max(0.5 * frame_height / math.tan(half_h), 0.5 * frame_width / math.tan(half_w))
        self.distance = distance + 0.5 * depth
        mid_up = self.top + headroom * frame_height - 0.5 * frame_height
        self.look = _add(_sub(center, _mul(self.up, _dot(center, self.up))), _mul(self.up, mid_up))
        self.half_h, self.half_w = half_h, half_w
        self.floor = min(self.bottom, info["bottom"]) - 0.02 * self.size

    def camera_position(self):
        return _add(self.look, _mul(_direction(0.0, self.elevation, self.back, self.right, self.up), self.distance))

    def on_floor(self, point):
        """`point` moved along the up axis onto the floor."""
        return _add(point, _mul(self.up, self.floor - _dot(point, self.up)))


def _place_camera(frame, aspect):
    """groomlab_studio_cam: made or moved, framing the subject; returns its transform."""
    def make():
        return cmds.camera(name=CAMERA)[0]
    camera = _child(CAMERA, ORBIT, make)
    shape = _shape(camera)
    cmds.setAttr(shape + ".focalLength", frame.focal)
    cmds.setAttr(shape + ".horizontalFilmAperture", FILM_BACK)
    cmds.setAttr(shape + ".verticalFilmAperture", FILM_BACK / aspect)
    cmds.setAttr(shape + ".filmFit", 1)  # horizontal: the render shows what the gate shows
    cmds.setAttr(shape + ".nearClipPlane", frame.size * 0.01)
    cmds.setAttr(shape + ".farClipPlane", frame.size * 200.0)
    cmds.setAttr(shape + ".centerOfInterest", frame.distance)
    position = frame.camera_position()
    cmds.xform(camera, worldSpace=True, matrix=_aim_matrix(position, frame.look, frame.up))
    return camera


def _light(name, kind):
    """A rig light's transform (made when missing, under the lights group)."""
    def make():
        node = cmds.shadingNode(kind, asLight=True)
        if cmds.nodeType(node) != "transform":
            node = cmds.listRelatives(node, parent=True)[0]
        cmds.rename(_shape(node), name + "Shape")
        return cmds.rename(node, name)
    node = _child(name, LIGHTS, make)
    if cmds.nodeType(_shape(node)) != kind:  # a light of another kind under this name: made again
        cmds.delete(node)
        node = _child(name, LIGHTS, make)
    return node


def _place_lights(frame, look, wall_at):
    """The preset's area lights and the dome: made, moved and set; the rig's other lights go."""
    names = []
    for spec in look.get("lights", []):
        name = GROUP + "_" + spec["name"]
        node = _light(name, "aiAreaLight")
        shape = _shape(node)
        target = wall_at if spec.get("target") == "backdrop" else frame.hair
        direction = _direction(spec["azimuth"], spec["elevation"], frame.back, frame.right, frame.up)
        position = _add(frame.hair, _mul(direction, spec["distance"] * frame.size))
        width, height = spec.get("size", (1.0, 1.0))
        cmds.xform(node, worldSpace=True, matrix=_aim_matrix(position, target, frame.up, 0.5 * width * frame.size,
                                                             0.5 * height * frame.size))
        cmds.setAttr(shape + ".intensity", 1.0)
        _set(shape, "aiExposure", float(spec.get("exposure", 0.0)))
        _set(shape, "color", tuple(spec.get("color", WHITE)))
        _set(shape, "aiNormalize", False)  # brightness by the angle the light covers: scale-free
        _set(shape, "aiSpread", float(spec.get("spread", 1.0)))
        _set(shape, "aiRoundness", 0.0)
        _set(shape, "aiSamples", 2)
        _set(shape, "aiCamera", 0.0)  # never seen by the camera, only in reflections
        if not spec.get("backdrop", True):  # a gridded strip: lights the hair, not the backdrop behind or below
            cmds.lightlink(light=shape, object=BACKDROP, b=True)
        names.append(name)
    dome = _light(DOME, "aiSkyDomeLight")
    shape = _shape(dome)
    cmds.setAttr(shape + ".intensity", 1.0)
    _set(shape, "aiExposure", 0.0)
    level = float(look.get("dome", 0.05))
    _set(shape, "color", (level, level, level))
    _set(shape, "camera", 0.0)  # the backdrop is the background
    _set(shape, "aiSamples", 1)
    names.append(DOME)
    for child in cmds.listRelatives(LIGHTS, children=True, type="transform") or []:
        if child not in names:
            cmds.delete(child)
    return names


def _profile(floor_front, floor_back, radius, wall_top):
    """The cyclorama's cross-section as (along, up) points from its front edge to the wall's top:
    a floor, a quarter circle of `radius` curving up, a wall. The cove gets most of the points, so
    it shades as one smooth curve (no line where the floor meets the wall)."""
    points = [(floor_front + (floor_back - floor_front) * k / 8.0, 0.0) for k in range(8)]
    for k in range(32):
        a = 0.5 * math.pi * (k / 32.0 - 1.0)  # -90 degrees at the floor, 0 at the wall
        points.append((floor_back + radius * math.cos(a), radius + radius * math.sin(a)))
    wall = max(wall_top - radius, 0.0)
    points += [(floor_back + radius, radius + wall * k / 8.0) for k in range(9)]
    return points


def _place_backdrop(frame, look):
    """The cyclorama: a floor under the subject curving up into a wall behind it, wide and tall
    enough to fill the camera's view; made again each time (a few hundred vertices). Returns its
    transform and the point on the wall behind the subject (where a background light aims)."""
    behind = [-(math.cos(math.radians(s["azimuth"])) * math.cos(math.radians(s["elevation"])) * s["distance"])
              for s in look.get("lights", [])]
    radius = 1.5 * frame.size
    # The wall behind every light, and the cove too, so no light sits close to the backdrop.
    wall_distance = max(2.4 * frame.size, max([0.0] + behind) * frame.size + radius + 0.4 * frame.size)
    reach = frame.distance + wall_distance  # the camera to the wall
    look_height = _dot(frame.look, frame.up) - frame.floor
    wall_top = look_height + 1.6 * reach * math.tan(frame.half_h) + frame.size
    width = 2.0 * 1.6 * reach * math.tan(frame.half_w) + 2.0 * frame.size
    front = frame.distance * 1.3  # under and past the camera
    points = _profile(-front, wall_distance - radius, radius, wall_top)

    if cmds.objExists(BACKDROP):
        cmds.delete(BACKDROP)
    mesh = cmds.polyPlane(name=BACKDROP, width=1.0, height=1.0, subdivisionsX=4, subdivisionsY=len(points) - 1,
                          axis=(0, 1, 0), constructionHistory=False)[0]
    cmds.parent(mesh, ORBIT)
    mesh = BACKDROP
    base = frame.on_floor(frame.hair)
    away = _mul(frame.back, -1.0)  # from the subject toward the wall
    across = _mul(frame.right, -1.0)  # the plane's x: keeps its normal facing the subject
    count = cmds.polyEvaluate(mesh, vertex=True)
    rest = cmds.xform(mesh + ".vtx[*]", query=True, objectSpace=True, translation=True)
    rows = len(points) - 1
    for i in range(count):
        x, z = rest[3 * i], rest[3 * i + 2]
        along, up = points[int(round((z + 0.5) * rows))]
        p = _add(_add(base, _mul(away, along)), _add(_mul(frame.up, up), _mul(across, x * width)))
        cmds.xform("%s.vtx[%d]" % (mesh, i), worldSpace=True, translation=p)
    cmds.polySoftEdge(mesh, angle=180, constructionHistory=False)
    shape = _shape(mesh)
    for attr in ("castsShadows",):
        cmds.setAttr(shape + "." + attr, False)  # nothing under it to shadow; keeps the cove clean
    level = float(look.get("backdrop", 0.18))
    group = _surface(BACKDROP_MATERIAL, (level, level, level), 0.8, 0.05)  # paper: matte, a faint sheen
    cmds.sets(mesh, edit=True, forceElement=group)
    wall_at = _add(_add(base, _mul(away, wall_distance)), _mul(frame.up, _dot(frame.hair, frame.up) - frame.floor))
    return mesh, wall_at


def _cameras_renderable(studio_camera, data):
    """Only the studio's camera renders (batch renders and Render Sequence); the others' flags kept."""
    saved = data.setdefault("renderable", {})
    for shape in cmds.ls(type="camera", long=True) or []:
        mine = studio_camera and _transform(shape).split("|")[-1] == studio_camera
        if not mine and shape not in saved:
            saved[shape] = bool(cmds.getAttr(shape + ".renderable"))
        cmds.setAttr(shape + ".renderable", bool(mine))


def _turn(look, data, frames):
    """The turntable: the rig (camera, lights, backdrop) orbiting the subject once, as if the subject
    turned on a stand; the playback and render range set (the ones before kept)."""
    up = _axes()[0]
    axis = "rotateZ" if up[2] else "rotateY"
    cmds.cutKey(ORBIT, attribute=("rotateX", "rotateY", "rotateZ"), clear=True)
    cmds.setAttr(ORBIT + ".rotate", 0.0, 0.0, 0.0)
    count = int(frames if frames is not None else look.get("turntable", 0) or 0)
    if count <= 0:
        _restore_range(data)
        return
    saved = data.setdefault("range", {})
    if not saved:
        saved.update({"min": cmds.playbackOptions(query=True, minTime=True),
                      "max": cmds.playbackOptions(query=True, maxTime=True),
                      "start": cmds.getAttr("defaultRenderGlobals.startFrame"),
                      "end": cmds.getAttr("defaultRenderGlobals.endFrame")})
    start = saved["min"]
    end = start + count
    cmds.setKeyframe(ORBIT, attribute=axis, time=start, value=0.0, inTangentType="linear", outTangentType="linear")
    cmds.setKeyframe(ORBIT, attribute=axis, time=end, value=360.0, inTangentType="linear", outTangentType="linear")
    cmds.playbackOptions(minTime=start, maxTime=end - 1)
    cmds.setAttr("defaultRenderGlobals.startFrame", start)
    cmds.setAttr("defaultRenderGlobals.endFrame", end - 1)


def _restore_range(data):
    saved = data.pop("range", None)
    if saved:
        cmds.playbackOptions(minTime=saved["min"], maxTime=saved["max"])
        cmds.setAttr("defaultRenderGlobals.startFrame", saved["start"])
        cmds.setAttr("defaultRenderGlobals.endFrame", saved["end"])


def _ensure_arnold():
    if not cmds.pluginInfo("mtoa", query=True, loaded=True):
        try:
            cmds.loadPlugin("mtoa", quiet=True)
        except RuntimeError:
            raise RuntimeError("studio lighting needs Arnold (MtoA): load mtoa in the Plug-in Manager")


def studio(groom=None, preset="portrait", mannequin=True, camera=True, render_settings=True,
           resolution=RESOLUTION, frames=None):
    """Build the studio round `groom` (the active groom, else the studio's own), or relight the
    scene's studio in place with another `preset` (preset_names()).

    `mannequin`: the head meshes (head_meshes: the scalps and colliders) wear a matte light grey;
    `camera`: groomlab_studio_cam frames the subject three-quarter on, and is the only camera that
    renders; `render_settings`: Arnold's samples for hair and `resolution` (None keeps the scene's).
    `frames`: the turntable's length (the preset's when None). What the studio changes is kept,
    and remove_studio() puts it back; turning an option off on a later call puts its part back
    too. One undo step. Returns {"group", "camera" (or None), "lights", "backdrop", "preset"}."""
    import groomlab
    look = get_preset(preset)
    _ensure_arnold()
    data = state()
    groom = groom or groomlab.active_groom() or data.get("groom")
    if groom and cmds.objExists(groom):
        groom = cmds.ls(groom, long=True)[0]
        if cmds.nodeType(groom) == "transform":
            shapes = cmds.listRelatives(groom, shapes=True, type=groomlab.GROOM_TYPE, fullPath=True)
            groom = shapes[0] if shapes else None
    else:
        groom = None
    if not groom or cmds.nodeType(groom) != groomlab.GROOM_TYPE:
        raise ValueError("select a groom: the studio lights and frames a groom")
    with undo_chunk("studio"):
        _group(GROUP)
        _group(ORBIT, GROUP)
        _group(LIGHTS, ORBIT)
        cmds.setAttr(ORBIT + ".rotate", 0.0, 0.0, 0.0)
        cmds.cutKey(ORBIT, attribute=("rotateX", "rotateY", "rotateZ"), clear=True)
        if data.get("groom") and data["groom"] != groom:
            _dress(None, False, data)  # another groom: the last one's head back as it was
        _render_settings(render_settings, resolution, data)
        info = subject(groom)
        aspect = _render_aspect()
        frame = _Frame(info, look.get("camera", {}), aspect)
        cmds.xform(ORBIT, worldSpace=True, translation=frame.on_floor(frame.hair), rotation=(0.0, 0.0, 0.0))
        backdrop, wall_at = _place_backdrop(frame, look)
        lights = _place_lights(frame, look, wall_at)
        cam = None
        if camera:
            cam = _place_camera(frame, aspect)
            _cameras_renderable(CAMERA, data)
        else:
            if cmds.objExists(CAMERA):
                cmds.delete(CAMERA)
            _restore_renderable(data)
        _dress(groom, mannequin, data)
        _turn(look, data, frames)
        data.update({"preset": preset, "groom": groom})
        _save_state(data)
    return {"group": GROUP, "camera": cam, "lights": lights, "backdrop": backdrop, "preset": preset}


def _restore_renderable(data):
    for shape, renderable in data.pop("renderable", {}).items():
        if cmds.objExists(shape):
            cmds.setAttr(shape + ".renderable", renderable)


def remove_studio():
    """Remove the studio rig and its materials; the head meshes' materials, the render settings,
    the cameras' renderable flags and the playback range back as they were. One undo step.
    Returns whether there was a studio."""
    if not exists():
        return False
    data = state()
    with undo_chunk("remove studio"):
        if not cmds.about(batch=True):  # views through the studio camera go back to the perspective one
            for panel in cmds.getPanel(type="modelPanel") or []:
                camera = cmds.modelPanel(panel, query=True, camera=True) or ""
                if camera.split("|")[-1] in (CAMERA, CAMERA + "Shape"):
                    cmds.lookThru(panel, "persp")
        _dress(None, False, data)
        _restore_settings(data.get("settings", {}))
        _restore_renderable(data)
        _restore_range(data)
        cmds.delete(GROUP)
        for material in (MANNEQUIN_MATERIAL, BACKDROP_MATERIAL):
            for node in (material + "SG", material):
                if cmds.objExists(node):
                    cmds.delete(node)
    return True


def look_through(panel=None):
    """The viewport (the given model panel, else the one with focus) looks through the studio camera.
    Returns the camera, or None without a studio camera."""
    if not cmds.objExists(CAMERA):
        return None
    if panel:
        cmds.lookThru(panel, CAMERA)
    else:
        cmds.lookThru(CAMERA)
    return CAMERA
