# SPDX-License-Identifier: GPL-3.0-or-later
"""Blender's handlers: sculpted guides and edited scalps (depsgraph updates, debounced), frames
(the scalp's pose and the time), final renders (every strand), .blend saves and loads, undo, quit."""

import bpy
from bpy.app.handlers import persistent

from . import groom, process


@persistent
def on_depsgraph(scene, depsgraph):
    if groom._rendering or groom._exporting or getattr(depsgraph, "mode", "VIEWPORT") == "RENDER":
        return
    found = groom.scalps()
    if not found:
        return
    for update in depsgraph.updates:
        idd = update.id.original if update.id is not None else None
        if idd is None:
            continue
        if isinstance(idd, bpy.types.Object):
            role = idd.groomlab.role
            if role == "GUIDES" and (update.is_updated_geometry or update.is_updated_transform) and idd.groomlab.scalp:
                groom.mark(idd.groomlab.scalp, "guides")
            elif role == "SCALP" and update.is_updated_geometry:
                groom.mark(idd, "pose")  # its evaluated mesh (an armature, shape keys): the points
                                        # again; the mesh data itself comes as a Mesh update below
        elif isinstance(idd, bpy.types.Curves):
            for s in found:
                if s.groomlab.guides is not None and s.groomlab.guides.data == idd:
                    groom.mark(s, "guides")
        elif isinstance(idd, bpy.types.Mesh):
            for s in found:
                if s.data == idd:
                    groom.mark(s, "scalp")


@persistent
def on_frame(scene, depsgraph=None):
    if groom.scalps():
        groom.on_frame(scene, depsgraph)


@persistent
def on_render_pre(scene, *args):
    if groom.scalps():
        groom.set_rendering(scene, True)


@persistent
def on_render_post(scene, *args):
    if groom._rendering:
        groom.set_rendering(scene, False)


@persistent
def on_save_pre(*args):
    groom.snapshot_all()


@persistent
def on_load_post(*args):
    groom.forget_all()


@persistent
def on_undo(*args):
    groom.forget_cache()
    groom.invalidate_all()


@persistent
def on_exit(*args):
    process.shutdown()


_HANDLERS = (
    ("depsgraph_update_post", on_depsgraph),
    ("frame_change_post", on_frame),
    ("render_pre", on_render_pre),
    ("render_complete", on_render_post),
    ("render_cancel", on_render_post),
    ("save_pre", on_save_pre),
    ("load_post", on_load_post),
    ("undo_post", on_undo),
    ("redo_post", on_undo),
    ("exit_pre", on_exit),
)


def register():
    for name, fn in _HANDLERS:
        lst = getattr(bpy.app.handlers, name)
        if fn not in lst:
            lst.append(fn)
    if not bpy.app.background and not bpy.app.timers.is_registered(groom.poll):
        bpy.app.timers.register(groom.poll, first_interval=1.0, persistent=True)


def unregister():
    for name, fn in _HANDLERS:
        lst = getattr(bpy.app.handlers, name)
        if fn in lst:
            lst.remove(fn)
    if bpy.app.timers.is_registered(groom.poll):
        bpy.app.timers.unregister(groom.poll)
