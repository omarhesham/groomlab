# SPDX-License-Identifier: Apache-2.0
"""The plugin, one-step undo, and the shape lookups every module builds on."""

import contextlib
import functools

from maya import cmds


PLUGIN_NAME = "GroomLab"
GROOM_TYPE = "groomLabShape"


def ensure_plugin():
    """Load the plugin if it is not loaded yet."""
    if not cmds.pluginInfo(PLUGIN_NAME, query=True, loaded=True):
        cmds.loadPlugin(PLUGIN_NAME)


@contextlib.contextmanager
def undo_chunk(name):
    """Everything done to the scene inside `with undo_chunk(name):` is one undo step, named
    "groomLab <name>" (closed however the block ends)."""
    cmds.undoInfo(openChunk=True, chunkName="groomLab " + name)
    try:
        yield
    finally:
        cmds.undoInfo(closeChunk=True)


def undoable(name):
    """Decorator: everything the function does to the scene is one undo step, named `name`."""
    def wrap(fn):
        @functools.wraps(fn)
        def wrapper(*args, **kwargs):
            with undo_chunk(name):
                return fn(*args, **kwargs)
        return wrapper
    return wrap


def _shape_of(node, shape_type):
    if cmds.nodeType(node) == shape_type:
        return node
    shapes = cmds.listRelatives(node, shapes=True, type=shape_type, noIntermediate=True, fullPath=True)
    if not shapes:
        raise ValueError("{} has no {} shape".format(node, shape_type))
    return shapes[0]


def _scalp_shape(groom):
    sources = cmds.listConnections(groom + ".inMesh", source=True, destination=False, shapes=True) or []
    return _shape_of(sources[0], "mesh") if sources else None


def _vertex_count(mesh):
    from maya.api import OpenMaya as om
    selection = om.MSelectionList()
    selection.add(mesh)
    return om.MFnMesh(selection.getDagPath(0)).numVertices


def _original_shape(mesh):
    """The undeformed "Orig" shape a deformer inserts under a mesh, if any.

    Only a mesh with a deformer (skin, blend shape, cluster...) in its history has one, and it is
    the same mesh: an intermediate mesh with the same vertex count. Others in the history are not:
    a Combine or Mirror kept with history leaves its pieces behind as intermediate meshes, and a
    Smooth after the deformers changes the count. listHistory lists upstream last, so the last
    match is the original (construction history may still feed it, e.g. polySphere).
    """
    history = cmds.listHistory(mesh) or []
    if not cmds.ls(history, type="geometryFilter"):
        return None
    count = _vertex_count(mesh)
    origs = [m for m in cmds.ls(history, type="mesh") or []
             if m != mesh and cmds.getAttr(m + ".intermediateObject") and _vertex_count(m) == count]
    return origs[-1] if origs else None
