# SPDX-License-Identifier: Apache-2.0
"""The cleaned Buddy scene in mayapy, shared by clean_rig.py, export_fbx.py and export_obj.py.

open_clean(source) opens the artist's scene and drops what a sample must not bring into a user's
scene: the unskinned reference mesh `ref`, the Arnold nodes (aiOptions and its drivers and filter:
with them go the mtoa requires), the script nodes (UI and scene configuration). geometry_GRP stops
inheriting, so the joints alone carry the skinned meshes (scaling or turning buddy_rig doesn't
move them twice). The scene is named buddy_rig.ma, as the files written from it record.
"""

import os

from maya import cmds

MESHES = ["buddy_body", "buddy_eye_l", "buddy_eye_r", "buddy_teeth", "buddy_tongue"]


def open_clean(source):
    cmds.file(source, open=True, force=True, ignoreVersion=True)
    cmds.delete("ref")
    for node in cmds.ls(type=("aiOptions", "aiAOVDriver", "aiAOVFilter", "script")):
        if cmds.objExists(node):  # deleting aiOptions takes its drivers with it
            cmds.lockNode(node, lock=False)
            cmds.delete(node)
    cmds.setAttr("geometry_GRP.inheritsTransform", False)
    cmds.file(rename=os.path.join(os.path.dirname(source), "buddy_rig.ma"))
    skinned = sorted(cmds.listRelatives(cmds.ls(type="mesh", noIntermediate=True), parent=True))
    if skinned != MESHES:
        raise RuntimeError("expected the meshes %s, the scene has %s" % (MESHES, skinned))
    check_bind_pose()


def check_bind_pose(tolerance=1e-4):
    """The controls at rest hold every joint at its bind pose (what the skins were bound at)."""
    worst = 0.0
    for joint in cmds.ls(type="joint"):
        if not cmds.listConnections(joint + ".bindPose", type="dagPose"):
            continue  # not an influence: no bind pose recorded
        bind = cmds.getAttr(joint + ".bindPose")
        world = cmds.xform(joint, query=True, worldSpace=True, matrix=True)
        worst = max(worst, max(abs(a - b) for a, b in zip(bind, world)))
    if worst > tolerance:
        raise RuntimeError("the rig isn't at its bind pose (a joint is %g off)" % worst)
    # And each skin there gives its mesh as bound (the intermediate Orig shape): no moved vertices.
    for mesh in MESHES:
        shapes = cmds.listRelatives(mesh, shapes=True, fullPath=True)
        live = [s for s in shapes if not cmds.getAttr(s + ".intermediateObject")]
        orig = [s for s in shapes if cmds.getAttr(s + ".intermediateObject")]
        a, b = _points(live[0]), _points(orig[0])
        if len(a) != len(b):
            raise RuntimeError("%s: the skin and its Orig shape differ in vertex count" % mesh)
        off = max(max(abs(a[i][k] - b[i][k]) for k in range(3)) for i in range(len(a)))
        if off > 1e-3:
            raise RuntimeError("%s at the bind pose is %g cm off its Orig shape" % (mesh, off))
        worst = max(worst, off)
    return worst


def _points(shape):
    from maya.api import OpenMaya as om

    sel = om.MSelectionList()
    sel.add(shape)
    return om.MFnMesh(sel.getDagPath(0)).getPoints(om.MSpace.kObject)


def finish():
    """Leave mayapy at once: opening the scene loads mtoa (its requires), and Arnold's shutdown
    crashes Maya at exit, which then saves the scene to the temp folder."""
    cmds.file(new=True, force=True)
    if cmds.pluginInfo("mtoa", query=True, loaded=True):
        cmds.unloadPlugin("mtoa", force=True)
    os._exit(0)
