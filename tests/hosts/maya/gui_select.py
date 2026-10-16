# SPDX-License-Identifier: Apache-2.0
"""GUI test for guide selection and interactive symmetric paint (needs a Maya UI session).

The viewport ignores synthetic mouse input, so the lasso and the stroke go
through the tools' -debugLasso / -debugStroke hooks, which run the same code
as the mouse from view coordinates.

A suite of the GUI session (gui_runner.py, ctest maya.gui; harness: gui_support.py).
Each step is a deferred call, as with a user.
"""

import math

import maya.OpenMayaUI as omui
from maya import cmds

from gui_support import check, finish, persp_panel, settle_undo, soon, to_view

state = {}


def start():
    state.clear()
    soon(setup)


def setup():
    import groomlab
    cmds.file(new=True, force=True)
    scalp = cmds.polySphere(radius=1, subdivisionsX=40, subdivisionsY=20)[0]
    groom = groomlab.create_groom(scalp)
    curves = []
    for angle in (0.35, 0.8):
        n = (math.sin(angle), math.cos(angle), 0.2)
        curves.append(cmds.curve(degree=1, point=[n, tuple(1.6 * c for c in n), tuple(2.1 * c for c in n)]))
    groomlab.guides_from_curves(groom, curves)
    groomlab.mirror_group(groom, "primary", "x")
    groomlab.generate_children(groom, "primary", name="children", count=3)
    cmds.select(cmds.listRelatives(groom, parent=True))
    cmds.setFocus(persp_panel())
    cmds.setAttr("persp.translate", 0, 2, 9)
    cmds.setAttr("persp.rotate", -8, 0, 0)
    cmds.refresh(force=True)
    state["groom"] = groom
    soon(lasso_all)


def lasso_all():
    import groomlab
    ctx = groomlab.select_tool(level=0)
    view = omui.M3dView.active3dView()
    w, h = view.portWidth(), view.portHeight()
    settle_undo()  # Maya's idle work done: the clicks below are the steps on top of the queue
    count = cmds.groomLabSelectCtx(ctx, edit=True, debugLasso="0 0 %d 0 %d %d 0 %d" % (w, w, h, h))
    primaries = groomlab.guide_ids(state["groom"], "primary")
    check("lasso over the view selects the %d primaries (got %s)" % (len(primaries), count), count == len(primaries))
    check("selection stored", sorted(groomlab.selected_guides(state["groom"])) == sorted(primaries))

    # A click at a guide's tip picks that guide alone.
    target = primaries[0]
    xyz = cmds.groomLabGuides(state["groom"], guidePoints=target)
    x, y = to_view(xyz[-3:])
    cmds.groomLabSelectCtx(ctx, edit=True, debugLasso="%d %d" % (x, y))
    check("click picks one guide", groomlab.selected_guides(state["groom"]) == [target])
    frozen = [str(g["id"]) for g in groomlab.get_hierarchy(state["groom"])["guides"]
              if g.get("attributes", {}).get("freeze", 0.0) > 0.5 and str(g["id"]) in set(map(str, primaries))]
    check("the Select tool freezes the guides not selected (%d frozen)" % len(frozen),
          str(target) not in frozen and len(frozen) == len(primaries) - 1)  # the primaries but the one
    cmds.groomLabSelectCtx(ctx, edit=True, debugLasso="%d %d" % (x, y), debugMode="toggle")
    check("shift-click toggles it off", groomlab.selected_guides(state["groom"]) == [])
    state["target"] = target
    soon(undo_selection)


def undo_selection():
    import groomlab
    cmds.undo()
    check("undo restores the clicked selection", groomlab.selected_guides(state["groom"]) == [state["target"]])
    soon(symmetric_paint)


def symmetric_paint():
    import groomlab
    groom = state["groom"]
    groomlab.clear_guide_selection(groom)
    groomlab.unfreeze_all(groom)  # the Select tool soloed its selection: every guide brushable again
    groomlab.set_guide_attribute(groom, "clump", 0.0, level=0)
    ctx = groomlab.brush_tool("clump", radius=150, strength=1.0, level=0, symmetry="x", selected_only=False)
    xyz = cmds.groomLabGuides(groom, guidePoints=state["target"])
    x, y = to_view(xyz[:3])  # press on the scalp, at the guide's root
    changed = cmds.groomLabBrushCtx(ctx, edit=True, debugStroke="%d %d %d %d" % (x, y, x + 2, y))
    stored = {str(g["id"]): g.get("attributes", {}).get("clump", 0.0) for g in groomlab.get_hierarchy(groom)["guides"]}
    groomlab.select_guides(groom, guides=[state["target"]])
    (mirror,) = groomlab.mirror_guide_selection(groom, "x")
    check("stroke painted (changed=%s)" % changed, changed and stored[state["target"]] > 0.2)
    # The mirror guide is the nearest guide to the target's mirror image, not an exact mirror, so its
    # paint depends on where that image falls in the brush circle (how many pixels the offset spans).
    check("symmetry painted the mirror guide too (%.3f vs %.3f)" % (stored[state["target"]], stored[mirror]),
          stored[mirror] > 0.6 * stored[state["target"]])

    # Selected only: with just the target selected, the mirror stays put.
    groomlab.set_guide_attribute(groom, "clump", 0.0, level=0)
    groomlab.select_guides(groom, guides=[state["target"]])
    cmds.groomLabBrushCtx(ctx, edit=True, symmetry="none", selectedOnly=True)
    cmds.groomLabBrushCtx(ctx, edit=True, debugStroke="%d %d %d %d" % (x, y, x + 2, y))
    stored = {str(g["id"]): g.get("attributes", {}).get("clump", 0.0) for g in groomlab.get_hierarchy(groom)["guides"]}
    others = [v for k, v in stored.items() if k != state["target"]]
    check("selected-only paints the selection alone", stored[state["target"]] > 0.2 and max(others) == 0.0)
    finish()
