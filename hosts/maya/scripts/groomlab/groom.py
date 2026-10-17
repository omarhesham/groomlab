# SPDX-License-Identifier: Apache-2.0
"""The groom node: creating one, its scalp and rest mesh, its inputs, the active groom, locking, the
GPU look preview and its errors."""

import os

from maya import cmds

from groomlab.base import GROOM_TYPE, ensure_plugin, undo_chunk, _original_shape, _scalp_shape, _shape_of
from groomlab.collision import colliders
from groomlab.extra_scalps import scalps
from groomlab.guides import add_guides, list_guide_groups


def create_groom(scalp=None, guides=None, name="groomLab1", fit=False):
    """Create a groom shape, optionally bound to a scalp mesh and guide curves.

    With `fit` (and a scalp), the groom stores the default graph with its defaults that measure
    something fitted to the scalp's size, as presets and nodes added in the Graph Editor are (the
    defaults are authored on a 20 cm head): a 2 m creature or a 2 cm test sphere gets the same
    hair, in proportion. Without, the graph is left empty (the default graph as authored). The
    menu's Create Groom fits.

    Returns the groom shape node name.
    """
    ensure_plugin()

    transform = cmds.createNode("transform", name=name)
    # Named from the transform's own name: createNode puts it in the current namespace already
    # (charA:hair gets charA:hairShape, not charA:charA:hairShape).
    leaf = transform.split("|")[-1].split(":")[-1]
    shape = cmds.createNode(GROOM_TYPE, name=leaf + "Shape", parent=transform)
    cmds.connectAttr("time1.outTime", shape + ".time")
    # The groom is grown in world space (from the scalp's world mesh): moving or
    # parenting its transform would move the hair off the scalp.
    cmds.setAttr(transform + ".inheritsTransform", False)
    for attr in ("translate", "rotate", "scale", "shear"):
        cmds.setAttr(transform + "." + attr, lock=True)

    if scalp:
        cmds.connectAttr(_shape_of(scalp, "mesh") + ".worldMesh[0]", shape + ".inMesh")
        if _original_shape(_shape_of(scalp, "mesh")):
            set_rest_mesh(shape)  # a deformed scalp: keep the same strands on every frame
        if fit:
            import groomlab_core
            from groomlab import presets
            size = presets.scalp_size(shape, scaled=False)  # just made: at 1x
            if size and abs(size / groomlab_core.DEFAULTS_SCALP_SIZE - 1.0) > 1e-6:
                cmds.setAttr(shape + ".graph", groomlab_core.default_graph_json_for(size), type="string")
    if guides:
        add_guides(shape, guides)
    return shape


# Nika, the sample head and shoulders shipped with GroomLab (CC BY 4.0, Nick Rutlinh): centimetres,
# y up, facing +z, 34.6 cm from the shoulders to the top of the head.
SAMPLE_HEAD = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "meshes", "nika_head.obj"))
SAMPLE_HEAD_HEIGHT_CM = 34.61


def add_sample_head(name="nika_head"):
    """Add Nika, the sample head and shoulders, to the scene at the origin (sized for the scene's
    units, turned for a z-up scene) and select her: Create Groom from Selection grows hair on her.
    One undo step. Returns the mesh's transform."""
    if not os.path.isfile(SAMPLE_HEAD):
        raise RuntimeError("the sample head is missing from the GroomLab install (%s)" % SAMPLE_HEAD)
    if not cmds.pluginInfo("objExport", query=True, loaded=True):
        cmds.loadPlugin("objExport", quiet=True)
    with undo_chunk("add sample head"):
        before = set(cmds.ls(type="transform", long=True))
        cmds.file(SAMPLE_HEAD, i=True, type="OBJ", options="mo=0", ignoreVersion=True, mergeNamespacesOnClash=False,
                  namespace="groomlab_sample")
        made = [t for t in cmds.ls(type="transform", long=True) if t not in before and cmds.listRelatives(t, shapes=True)]
        if not made:
            raise RuntimeError("the sample head did not import")
        head = cmds.rename(made[0], name)
        cmds.namespace(removeNamespace="groomlab_sample", mergeNamespaceWithRoot=True)
        # Sized in the scene's own units: whatever unit the import read the numbers in, she ends
        # up 34.6 cm tall.
        bbox = cmds.exactWorldBoundingBox(head)
        height = max(bbox[4] - bbox[1], bbox[5] - bbox[2])
        unit = cmds.currentUnit(query=True, linear=True)
        want = float(cmds.convertUnit(SAMPLE_HEAD_HEIGHT_CM, fromUnit="cm", toUnit=unit).rstrip("abcdefghijklmnopqrstuvwxyz"))
        if height > 0 and abs(height / want - 1.0) > 1e-3:
            cmds.setAttr(head + ".scale", want / height, want / height, want / height)
        if cmds.upAxis(query=True, axis=True) == "z":
            cmds.setAttr(head + ".rotateX", 90)
        cmds.makeIdentity(head, apply=True, translate=True, rotate=True, scale=True)
        cmds.delete(head, constructionHistory=True)
        cmds.select(head, replace=True)
    return head


def set_follow_scalp(groom, on=True):
    """Follow the scalp: grow the groom on its rest mesh and carry each strand to the scalp by
    its root (a rigid turn and move, no stretch), so a deforming scalp re-runs only the carrying.
    Needs a rest mesh (set_rest_mesh). Guide curves and simulated or cached guides keep their
    place; colliders, ImportCurves, Wind's direction and world-space noise are read at rest.
    One undo step."""
    cmds.setAttr(groom + ".followScalp", bool(on))


SCALE_SOURCES = ("area", "transform", "off")


def set_scale_source(groom, source="area"):
    """What the groom's scale follows. The groom grows with its scalp like a child object (lengths,
    widths, clump sizes and guide shapes times the scale, densities divided by its square so the
    strand count stays, curl frequencies divided by it); a deforming scalp keeps one scale.

    "area" (the default): sqrt(scalp area / reference), the area the scalp's world surface at the
    scene's start frame: scaling the scalp's transform, freezing it, re-meshing it at the same size
    and a rig scaled at its root all count, and editing or remodelling the mesh to another size
    scales the groom too (said once in the Script Editor and the view). "transform": the scalp's
    transform scale at the start frame (its world matrix's, times its rig's root joint's) over the
    reference: remodelling leaves the groom as it is, but Freeze Transformations resets the
    transform's scale, so the groom goes back with it. Surface Area survives Freeze
    Transformations; Transform Only survives remodelling. "off": the groom keeps its sizes in
    scene units."""
    if source not in SCALE_SOURCES:
        raise ValueError("scale source %r: one of %s" % (source, ", ".join(SCALE_SOURCES)))
    cmds.setAttr(groom + ".scaleSource", SCALE_SOURCES.index(source))


def scale_source(groom):
    """What the groom's scale follows: "area", "transform" or "off" (set_scale_source)."""
    return SCALE_SOURCES[cmds.getAttr(groom + ".scaleSource")]


def set_scale_with_scalp(groom, on=True):
    """Scale with the scalp (set_scale_source "area") or not ("off"); on by default. Turning it on
    keeps "transform" where the groom follows that."""
    if not on:
        set_scale_source(groom, "off")
    elif scale_source(groom) == "off":
        set_scale_source(groom, "area")


def groom_scale(groom):
    """What the groom is scaled by now (its groomScale: 1.0 at the size it was groomed at, or
    while its scale source is off)."""
    return cmds.getAttr(groom + ".groomScale")


def set_scale_reference(groom):
    """Make the scalp's size now the groom's 1x: the groom keeps the sizes its params and guides
    say at this size (groomScale reads 1.00 again). One undo step. Returns the reference area."""
    area = cmds.getAttr(groom + ".scaleArea")
    if not area or area <= 0.0:
        raise RuntimeError("%s has no scalp to measure" % groom)
    transform = cmds.getAttr(groom + ".scaleTransform")
    with undo_chunk("set_scale_reference"):
        cmds.setAttr(groom + ".scaleReference", area)
        if transform and transform > 0.0:
            cmds.setAttr(groom + ".scaleTransformReference", transform)
    return area


def store_scale_references():
    """Before a save: a groom without a scale reference (made before scaling with the scalp, or
    not saved since it was made) stores the one it uses (the first size it was evaluated at), so
    the groom reopens at the scale it had. The plugin calls it just before every save."""
    for groom in list_grooms():
        if cmds.referenceQuery(groom, isNodeReferenced=True):
            continue
        if cmds.getAttr(groom + ".scaleSource") == 2 or not scalp_mesh(groom):
            continue
        k = cmds.getAttr(groom + ".groomScale")
        if not k > 0.0:
            continue
        area = cmds.getAttr(groom + ".scaleArea")
        if cmds.getAttr(groom + ".scaleReference") <= 0.0 and area > 0.0:
            cmds.setAttr(groom + ".scaleReference", area / (k * k))
        transform = cmds.getAttr(groom + ".scaleTransform")
        if cmds.getAttr(groom + ".scaleTransformReference") <= 0.0 and transform > 0.0:
            cmds.setAttr(groom + ".scaleTransformReference", transform / k)


_IDENTITY = (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0)


def set_rest_mesh(groom, mesh=None):
    """Give the groom a rest (undeformed) scalp with the same vertices.

    Strands are then scattered and matched to guides and clumps on the rest
    pose, so a deforming scalp keeps the same strands on every frame (needed
    for motion blur and caches). `mesh` defaults to the scalp's original
    shape under its deformers, or else a hidden copy of the scalp at the
    current frame. Returns the rest mesh shape.

    The rest pose stays put however the mesh's transform (or a parent's: a head under an animated
    rig) moves: it is the mesh's own points (object space, its outMesh) placed where its transform
    puts them now, through a transformGeometry node holding that matrix as a value (none when the
    transform is the identity, as a skinned mesh's usually is). A world-space connection would move
    the rest pose every frame, and with it the strands' assignment and the rest pose's caches.
    """
    if mesh is None:
        scalp = _scalp_shape(groom)
        if not scalp:
            raise RuntimeError("%s has no scalp connected" % groom)
        mesh = _original_shape(scalp)
        if not mesh:
            copy = cmds.duplicate(cmds.listRelatives(scalp, parent=True)[0], name=groom + "Rest")[0]
            cmds.hide(copy)
            mesh = _shape_of(copy, "mesh")
    mesh = _shape_of(mesh, "mesh")
    _connect_rest(groom, mesh)
    return mesh


def _connect_rest(groom, mesh):
    """Connects the mesh shape's points, placed by its world matrix as it is now (a value, not a
    connection), to the groom's rest mesh, in place of what was there."""
    clear_rest_mesh(groom)
    matrix = tuple(cmds.getAttr(mesh + ".worldMatrix[0]"))
    if matrix == _IDENTITY:
        cmds.connectAttr(mesh + ".outMesh", groom + ".restMesh", force=True)
        return
    place = cmds.createNode("transformGeometry", name=groom.split("|")[-1].split(":")[-1] + "RestPlace",
                            skipSelect=True)
    cmds.addAttr(place, longName="groomLabRestPlace", attributeType="bool", defaultValue=True)
    cmds.setAttr(place + ".transform", *matrix, type="matrix")
    cmds.connectAttr(mesh + ".outMesh", place + ".inputGeometry")
    cmds.connectAttr(place + ".outputGeometry", groom + ".restMesh", force=True)


def _rest_place(node):
    """Whether `node` is a rest mesh's placement (_connect_rest's transformGeometry)."""
    return cmds.nodeType(node) == "transformGeometry" and cmds.attributeQuery("groomLabRestPlace", node=node, exists=True)


def clear_rest_mesh(groom):
    for plug in cmds.listConnections(groom + ".restMesh", plugs=True, source=True, destination=False) or []:
        cmds.disconnectAttr(plug, groom + ".restMesh")
        node = plug.split(".")[0]
        if _rest_place(node) and not cmds.listConnections(node + ".outputGeometry", source=False, destination=True):
            cmds.delete(node)  # the placement went with it


def scalp_mesh(groom):
    """The groom's scalp mesh shape, or None."""
    return _scalp_shape(groom)


def rest_mesh(groom):
    """The groom's rest mesh shape, or None."""
    sources = cmds.listConnections(groom + ".restMesh", source=True, destination=False, shapes=True) or []
    if sources and _rest_place(sources[0]):
        sources = cmds.listConnections(sources[0] + ".inputGeometry", source=True, destination=False, shapes=True) or []
    return _shape_of(sources[0], "mesh") if sources else None


def set_scalp(groom, mesh, fit=False):
    """Grow the groom on `mesh`. A groom with guides moves onto it
    (transfer_groom: guides re-rooted at the closest points, after fitting the
    old scalp's bounds onto the new one with `fit`, painted maps projected); an
    empty one is just connected. Returns what could not be carried (maps, color
    sets the graph reads that the mesh lacks)."""
    mesh = _shape_of(mesh, "mesh")
    current = scalp_mesh(groom)
    if current and cmds.ls(mesh, long=True) == cmds.ls(current, long=True):  # the same mesh, however named
        return []
    if current and list_guide_groups(groom):
        from groomlab.transfer import transfer_groom
        return transfer_groom(groom, mesh, fit=fit)
    with undo_chunk("set scalp"):
        cmds.connectAttr(mesh + ".worldMesh[0]", groom + ".inMesh", force=True)
        cmds.setAttr(groom + ".scalpTopology", "", type="string")  # this mesh is the groom's now
        clear_rest_mesh(groom)
        if _original_shape(mesh):
            set_rest_mesh(groom)
    return []


def guides_text(summary):
    """ "119 guides in 1 group" from an inputs_summary."""
    plural = lambda n, word: "%d %s%s" % (n, word, "" if n == 1 else "s")
    return "%s in %s" % (plural(summary["guides"], "guide"), plural(summary["groups"], "group"))


def inputs_summary(groom):
    """What the groom is made from, for display: {"scalp", "rest", "guides", "groups",
    "curves", "sim", "cache", "colliders", "scalps" (the other scalps, groomlab.scalps)}."""
    groups = list_guide_groups(groom)
    return {
        "scalp": scalp_mesh(groom),
        "rest": rest_mesh(groom),
        "guides": sum(g["count"] for g in groups),
        "groups": len(groups),
        "curves": len(cmds.listConnections(groom + ".guideCurves", source=True, destination=False) or []),
        "sim": len(cmds.listConnections(groom + ".simGuides", source=True, destination=False) or []),
        "cache": cmds.getAttr(groom + ".guideCache") or "",
        "colliders": colliders(groom),
        "scalps": scalps(groom),
    }


# ---------------------------------------------------------------------------
# The active groom


PIN_VAR, ACTIVE_VAR = "groomLabPinGroom", "groomLabActiveGroom"


def pin_groom(groom):
    """Make every GroomLab tool work on `groom` whatever is selected; None unpins."""
    if groom:
        cmds.optionVar(stringValue=(ACTIVE_VAR, groom), intValue=(PIN_VAR, 1))
    else:
        cmds.optionVar(intValue=(PIN_VAR, 0))


def pin_new_groom(groom):
    """A groom just made in the UI (Create Groom from Selection, Import Groom File): when a groom
    is pinned, the pin moves to the new one, so the tools and the Graph Editor work on it."""
    if cmds.optionVar(query=PIN_VAR) == 1:
        pin_groom(groom)


def pinned_groom():
    """The pinned groom, or None."""
    if cmds.optionVar(query=PIN_VAR) != 1:
        return None
    name = cmds.optionVar(query=ACTIVE_VAR) if cmds.optionVar(exists=ACTIVE_VAR) else ""
    return name if name and cmds.objExists(name) and cmds.nodeType(name) == GROOM_TYPE else None


def active_groom():
    """The groom tools work on: the pinned one, else the first selected one."""
    pinned = pinned_groom()
    if pinned:
        return pinned
    found = cmds.ls(selection=True, dag=True, type=GROOM_TYPE)
    return found[0] if found else None


def list_grooms():
    """Every groom in the scene."""
    return cmds.ls(type=GROOM_TYPE) or []


def lock_groom(groom, locked=True):
    """A locked groom ignores brushes and part strokes (finished work stays finished)."""
    cmds.setAttr(groom + ".locked", bool(locked))


# ---------------------------------------------------------------------------
# The GPU look preview

GPU_PREVIEW_VAR = "groomLabGpuPreview"


def gpu_preview(enabled=None):
    """Whether a drag of an Interpolate (radius), Length, Clump, Noise, Curl or Width number, and a
    brush stroke's dabs on guides, preview on the GPU (the default) rather than evaluating on the
    CPU; `enabled` sets it, for this session and the next ones (the optionVar groomLabGpuPreview,
    read when the plugin loads; GROOMLAB_GPU_PREVIEW=0 or 1 in the environment forces it). Returns
    the state. The release of a drag or a stroke always commits on the CPU."""
    if enabled is not None:
        cmds.optionVar(intValue=(GPU_PREVIEW_VAR, 1 if enabled else 0))
        cmds.groomLabGpuPreview(enable=bool(enabled))
    return bool(cmds.groomLabGpuPreview(isEnabled=True))


# ---------------------------------------------------------------------------
# Errors


def errors(groom):
    """Why the groom's last evaluation failed or was partial: a list of messages, empty when fine."""
    text = cmds.getAttr(groom + ".lastError") or ""
    return [line for line in text.splitlines() if line]
