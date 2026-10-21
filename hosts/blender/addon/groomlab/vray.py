# SPDX-License-Identifier: GPL-3.0-or-later
"""Final renders in V-Ray for Blender: each groom renders as GroomLab's V-Ray hair (vrscene_writer.h's
GeomMayaHair with the per-point colors and the BRDFHair4 fitted to the Arnold look), not as V-Ray's
conversion of the hair Curves object (its radii alone: no colors, Blender's material).

How: V-Ray for Blender renders a mesh object marked as a V-Ray Scene asset (obj.vray.VRayAsset
"Scene", the path in obj.data.vray.VRayScene) as a VRayScene plugin that loads a .vrscene at the
object's world transform. Each groom gets one such object (role "VRAY", an empty mesh parented to
the scalp, its local matrix turning the file's Y up into the scalp's Z up and undoing the scalp's
scale). Before a final render with V-Ray (render_pre), the helper writes the groom's .vrscene (format
"vrscene", units "scene" and the scalp's scale: the strands at full density in world units, Y up,
since V-Ray does not scale hair widths with a transform; the look's shader) for the frames the
render needs:
the current one, the scene's range when V-Ray renders an animation, one frame more on each side for
motion blur (the file's interpolate() keys; VRayScene plays them at V-Ray's frame). For that render
the V-Ray Scene object renders and the hair Curves object does not; render_complete/cancel put both
back (the V-Ray Scene object hidden in viewports and other renders: Cycles and EEVEE see nothing new).

V-Ray's interactive (viewport) render shows V-Ray's own conversion of the hair Curves (a preview).
The files go to the temp folder (groomlab_vray/<groom uid>.vrscene), written on the machine that
renders.
"""

import os
import tempfile
import threading

import bpy
from mathutils import Matrix

from . import groom, process
from .helper_client import HelperError

ENGINE = "VRAY_RENDER_RT"   # V-Ray for Blender's RenderEngine
ROLE = "VRAY"
SCENE_ASSET = "2"           # V-Ray for Blender's VRayAsset.assetType for a V-Ray Scene

_job = {"written": set(), "hidden": []}  # this render job: grooms written, objects whose visibility was changed


def available():
    """V-Ray for Blender is enabled (its properties are on objects)."""
    return hasattr(bpy.types.Object, "vray")


def renders(scene):
    return scene is not None and scene.render.engine == ENGINE and available()


def directory():
    path = os.path.join(tempfile.gettempdir(), "groomlab_vray")
    os.makedirs(path, exist_ok=True)
    return path


def scene_path(scalp):
    """One file per groom, written again for each render (not one per session: they are large)."""
    uid = scalp.groomlab.uid or scalp.name
    return os.path.join(directory(), "%s.vrscene" % uid)


def scalp_scale(scalp):
    """The scalp object's world scale (one number: the mean of its axes'), which the file carries
    in its points and widths: V-Ray does not scale hair widths with a transform."""
    sx, sy, sz = scalp.matrix_world.to_scale()
    s = (abs(sx) + abs(sy) + abs(sz)) / 3.0
    return s if s > 1e-12 else 1.0


def file_to_scalp(scale=1.0):
    """The file's space (groom space's axes, Y up, in scene units times the scalp's scale: the
    helper's units "scene" and scale) to the scalp's local space (Z up): (x, y, z) -> (x, -z, y),
    the helper's toGroom inverted, over the scalp's scale (its world matrix brings it back, so
    the V-Ray Scene object's world transform has no scale)."""
    k = 1.0 / scale
    return Matrix(((k, 0.0, 0.0, 0.0), (0.0, 0.0, -k, 0.0), (0.0, k, 0.0, 0.0), (0.0, 0.0, 0.0, 1.0)))


def find_object(scalp):
    for obj in scalp.children:
        if obj.groomlab.role == ROLE and obj.type == "MESH":
            return obj
    return None


def ensure_object(scalp, scene):
    """The groom's V-Ray Scene object (made once)."""
    obj = find_object(scalp)
    if obj is None:
        mesh = bpy.data.meshes.new("%s_vray" % scalp.name)
        obj = bpy.data.objects.new(mesh.name, mesh)
        for c in list(scalp.users_collection) or [scene.collection]:
            if obj.name not in c.objects:
                c.objects.link(obj)
        obj.parent = scalp
        obj.matrix_parent_inverse = Matrix.Identity(4)
        obj.groomlab.role = ROLE
        obj.groomlab.scalp = scalp
        obj.hide_select = True
        obj.hide_viewport = True   # nothing to draw (the hair Curves are the viewport's)
        obj.hide_render = True     # only V-Ray's final renders (render_pre) show it
    m = file_to_scalp(scalp_scale(scalp))
    if obj.matrix_basis != m:
        obj.matrix_basis = m
    if available():
        obj.vray.VRayAsset.assetType = SCENE_ASSET
        # Its path without the preview loader (VRayScene.filepath's update reads the file for a mesh).
        obj.data.vray.VRayScene["filepath"] = scene_path(scalp)
    return obj


def remove_object(scalp):
    obj = find_object(scalp)
    if obj is not None:
        mesh = obj.data
        bpy.data.objects.remove(obj)
        if mesh is not None and mesh.users == 0:
            bpy.data.meshes.remove(mesh)


def motion_blur(scene):
    """V-Ray will blur motion: its camera (physical camera's own setting) or the global one."""
    try:
        mb = scene.vray.SettingsMotionBlur
        cam = scene.camera
        if cam is not None and cam.type == "CAMERA":
            phys = cam.data.vray.CameraPhysical
            return bool(phys.use_moblur if phys.use else mb.on)
        return bool(mb.on)
    except AttributeError:
        return False


def frames(scene):
    """(first, last, step) the render needs: V-Ray's animation mode renders the scene's range."""
    first = last = float(scene.frame_current)
    step = 1.0
    try:
        animation = scene.vray.Exporter.animation_mode != "FRAME"
    except AttributeError:
        animation = False
    if animation:
        first, last, step = float(scene.frame_start), float(scene.frame_end), float(max(scene.frame_step, 1))
    if motion_blur(scene):
        first, last, step = first - 1.0, last + 1.0, 1.0
    return first, last, step


def write(scalp, scene, path=None, frame_range=None):
    """The groom's .vrscene for the frames, in groom space. A deforming scalp needs Blender to pose
    each frame: on the main thread its frames are played (groom.export); a render thread (a final
    render from the UI) writes the current pose alone."""
    path = path or scene_path(scalp)
    first, last, step = frame_range or frames(scene)
    main = threading.current_thread() is groom._main_thread
    if groom.deforms(scalp) and last > first:
        if main:
            return groom.export(scalp, scene, "vrscene", path, first, last, step, units="scene",
                                scale=scalp_scale(scalp))
        print("GroomLab: %s deforms: V-Ray gets its current pose for every frame (a render from Blender's "
              "window cannot play the scalp's frames)" % scalp.name)
        first = last = float(scene.frame_current)
    rt = groom.ensure(scalp, scene)
    groom.sync(scalp, scene=scene, evaluate_now=False, read_pose=main, topology=False)
    try:
        return process.call("export", session=rt.session, format="vrscene", path=path, density=1.0, units="scene",
                            scale=scalp_scale(scalp), frames={"first": first, "last": last, "step": step})
    finally:
        rt.frame = None  # the helper stepped its own time: the scene's frame again on the next sync


def render_init(scene):
    _job["written"] = set()


def render_pre(scene):
    """Before a frame of a V-Ray final render: each groom's file written (once per render job) and
    its V-Ray Scene object rendered instead of its hair Curves. False when a groom's file could not
    be written (its Curves render: they need every strand then)."""
    if not renders(scene):
        return False
    ok = True
    for scalp in groom.scalps():
        if scalp.name not in scene.objects:
            continue
        hair = scalp.groomlab.hair
        try:
            obj = ensure_object(scalp, scene)
            if scalp.name not in _job["written"]:
                write(scalp, scene, path=obj.data.vray.VRayScene["filepath"])
                _job["written"].add(scalp.name)
        except (HelperError, OSError) as e:
            print("GroomLab: %s renders as V-Ray's own hair curves: its V-Ray scene was not written: %s" % (scalp.name, e))
            ok = False
            continue
        _hide(obj, False)
        if hair is not None:
            _hide(hair, True)
    return ok


def _hide(obj, hidden):
    if obj.hide_render != hidden:
        _job["hidden"].append((obj.name, obj.hide_render))
        obj.hide_render = hidden


def render_post(scene):
    """After the render: the objects' render visibility as it was."""
    restore, _job["hidden"] = _job["hidden"], []
    for name, was in reversed(restore):
        obj = bpy.data.objects.get(name)
        if obj is not None and obj.hide_render != was:
            obj.hide_render = was
