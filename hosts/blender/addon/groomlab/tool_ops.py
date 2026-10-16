# SPDX-License-Identifier: GPL-3.0-or-later
"""The Groom Tools' operators: picking a tool (a tile, the pie menu, a key), the tool itself in
the 3D viewport (a modal operator: the brush ring under the cursor, strokes with the pen's
pressure, Ctrl the opposite action and Shift smoothing or picking a guide's level, lasso, box and
click selection, B / M held + drag for the radius / strength, Page Up / Down for the level, S for
Select, Y for the pie menu, Ctrl+Z / Ctrl+Shift+Z the helper's undo and redo, Esc to leave),
guide group and map edits, and the pie menu standing in for Maya's marking menu (the same radial
layout, the rest of the catalog below it).

The modal operator only translates Blender's events: ToolSession does the work, over plain
values, so the tests drive it with synthetic events and a synthetic view."""

import bpy
from bpy.props import BoolProperty, EnumProperty, FloatProperty, IntProperty, StringProperty

from . import display, groom, icons, process, props, tools
from .helper_client import HelperError

# hosts/maya/scripts/groomlab/ui/hotkeys.py's marking menu: the brushes reached for most around it.
RADIAL = [("N", "comb"), ("NE", "slide"), ("E", "smooth"), ("SE", "cut"),
          ("S", "place"), ("SW", "draw"), ("W", "length"), ("NW", "lift")]
# Blender lays a pie's items out in this order of directions.
PIE_ORDER = ("W", "E", "S", "N", "NW", "NE", "SW", "SE")
PIE_KEY = "Y"
# Maya's Help > Keyboard Shortcuts, as the Blender tool has them.
SHORTCUTS = (
    ("B + drag", "The brush radius"),
    ("M + drag", "The brush strength"),
    ("Ctrl (stroke)", "The opposite effect (Ctrl+Length shrinks, Ctrl+Lift lays down...)"),
    ("Shift (stroke)", "Smooth instead"),
    ("Shift + click", "Pick the guide's level"),
    ("S", "Select guides (Shift: toggle, Ctrl: deselect)"),
    ("Esc / right-click", "Cancel the stroke; Esc again leaves the tool"),
    ("Page Up / Page Down", "The brush level"),
    ("Y", "The pie menu (Maya's marking menu)"),
    ("Ctrl+Z / Ctrl+Shift+Z", "Undo / redo the groom's edits"),
    ("Tab (Graph Editor)", "Add a node at the cursor"),
    ("B / V (Graph Editor)", "Bypass; view up to the node"),
)
# (keymap, operator, (property, value) or None): the add-on's keys the preferences show.
KEYMAP_ITEMS = (("Object Mode", "wm.call_menu_pie", ("name", "GROOMLAB_MT_pie")),)
NAV_PASS = {"MIDDLEMOUSE", "WHEELUPMOUSE", "WHEELDOWNMOUSE", "WHEELINMOUSE", "WHEELOUTMOUSE", "TRACKPADPAN",
            "TRACKPADZOOM", "MOUSEROTATE", "MOUSESMARTZOOM", "NDOF_MOTION"}
SLIDER_KEYS = {"B": "radius", "M": "strength", "F": "radius"}


def _fail(op, e):
    op.report({"ERROR"}, e.message if isinstance(e, HelperError) else str(e))
    return {"CANCELLED"}


def active(context):
    return tools.active(context)


# ---- Where the mouse is: a 3D view's region (patched by the tests) ------------------------------

def _inside(region, x, y):
    return region.x <= x < region.x + region.width and region.y <= y < region.y + region.height


def locate(context, event):
    """(region, region_3d, x, y) of the 3D view under the mouse (region pixels, origin bottom left),
    or None over anything else (another editor, a header, the toolbar or sidebar)."""
    window = context.window
    if window is None:
        return None
    mx, my = event.mouse_x, event.mouse_y
    for area in window.screen.areas:
        if area.type != "VIEW_3D" or not (area.x <= mx < area.x + area.width and area.y <= my < area.y + area.height):
            continue
        for region in area.regions:
            if region.type != "WINDOW" and region.width > 1 and region.height > 1 and _inside(region, mx, my):
                return None  # a header, toolbar, sidebar or the HUD over the view
        space = area.spaces.active
        quads = list(getattr(space, "region_quadviews", None) or [])
        windows = [r for r in area.regions if r.type == "WINDOW"]
        for i, region in enumerate(windows):
            if _inside(region, mx, my):
                rv3d = quads[i] if len(windows) == 4 and len(quads) == 4 else space.region_3d
                return region, rv3d, float(mx - region.x), float(my - region.y)
    return None


# ---- The tool, event by event --------------------------------------------------------------------

class ToolSession(object):
    """The GroomLab tool in the viewport: Blender's events into the helper's hover, strokes,
    selection, undo and the tool's settings."""

    def __init__(self):
        self.stroking = None      # the scalp while a stroke is live
        self.points = []          # mouse samples not sent yet
        self.lasso = None         # (scalp, view, points, mode) while selecting
        self.slider = None        # (setting, x at the press, value at the press, (view, x, y) of the ring)
        self.held = set()         # B / M / F held
        self.hovering = None      # the scalp hovered last
        self.view = None
        self.cursor = False       # the crosshair set over a viewport

    def tool(self, context):
        return context.scene.groomlab_tools.tool

    def scalp(self, context):
        s = active(context)
        return s if s is not None and tools.is_hierarchy(s) else None

    # -- Events --

    def event(self, context, event, locate_fn=None):
        """One event: {'RUNNING_MODAL'} (ours), {'PASS_THROUGH'} (Blender's) or {'FINISHED'} (the tool left)."""
        locate_fn = locate_fn or locate
        if not self.tool(context):
            self.leave(context)
            return {"FINISHED"}
        scalp = self.scalp(context)
        if scalp is None or context.mode != "OBJECT":  # no groom, or another mode: Blender's tools again
            self.leave(context)
            return {"FINISHED"}
        kind, value = event.type, event.value
        if kind in NAV_PASS or kind.startswith("NDOF") or kind.startswith("NUMPAD"):
            return {"PASS_THROUGH"}
        if kind in ("MOUSEMOVE", "INBETWEEN_MOUSEMOVE"):
            return self.move(context, scalp, event, locate_fn, kind == "MOUSEMOVE")
        if kind in ("LEFTMOUSE", "PEN"):
            if value == "PRESS":
                return self.press(context, scalp, event, locate_fn)
            if value == "RELEASE":
                return self.release(context, scalp, event, locate_fn)
            return {"RUNNING_MODAL"} if (self.stroking or self.lasso or self.slider) else {"PASS_THROUGH"}
        busy = self.stroking is not None or self.lasso is not None or self.slider is not None
        if kind in ("ESC", "RIGHTMOUSE") and value == "PRESS":
            if busy:
                self.cancel(context)
                return {"RUNNING_MODAL"}
            if kind == "ESC":
                self.leave(context, clear_tool=True)
                return {"FINISHED"}
            return {"PASS_THROUGH"}
        over = locate_fn(context, event) is not None
        if kind in SLIDER_KEYS and not (event.ctrl or event.alt or event.oskey):
            if value == "PRESS" and over:
                self.held.add(kind if kind != "F" or not event.shift else "M")
                return {"RUNNING_MODAL"}
            if value == "RELEASE":
                was = bool(self.held)
                self.held.clear()
                return {"RUNNING_MODAL"} if was else {"PASS_THROUGH"}
            return {"RUNNING_MODAL"} if over else {"PASS_THROUGH"}
        if kind == "Z" and value == "PRESS" and (event.ctrl or event.oskey) and not busy and over:
            self.undo(context, scalp, redo=event.shift)  # over the viewport: the groom's own undo
            return {"RUNNING_MODAL"}
        if not over or value != "PRESS":
            return {"PASS_THROUGH"}
        if kind == pie_key() and not (event.ctrl or event.alt or event.shift):
            if not bpy.app.background:
                bpy.ops.wm.call_menu_pie("INVOKE_DEFAULT", name=GROOMLAB_MT_pie.bl_idname)
            return {"RUNNING_MODAL"}
        if kind == "S" and not (event.ctrl or event.alt or event.oskey or busy):
            self.pick(context, "select")
            return {"RUNNING_MODAL"}
        if kind in ("PAGE_UP", "PAGE_DOWN") and not busy:
            step_level(context, -1 if kind == "PAGE_UP" else 1)
            return {"RUNNING_MODAL"}
        return {"PASS_THROUGH"}

    def located(self, context, scalp, event, locate_fn):
        where = locate_fn(context, event)
        if where is None:
            return None
        region, rv3d, x, y = where
        return tools.view_of(region, rv3d, scalp), x, y

    def move(self, context, scalp, event, locate_fn, flush):
        if self.slider is not None:
            self.drag_slider(context, scalp, event)
            return {"RUNNING_MODAL"}
        where = self.located(context, scalp, event, locate_fn)
        pressure = float(getattr(event, "pressure", 1.0) or 1.0)
        if self.stroking is not None:
            if where is not None:
                self.points.append([where[1], where[2], pressure])
            if flush:
                self.flush(context)
            return {"RUNNING_MODAL"}
        if self.lasso is not None:
            if where is not None:
                self.lasso[2].append([where[1], where[2]])
            return {"RUNNING_MODAL"}
        self.set_cursor(context, where is not None)
        if where is None:
            if self.hovering is not None:
                tools.hover_clear(self.hovering)
                self.hovering = None
                display.redraw()
            return {"PASS_THROUGH"}
        if flush and self.tool(context) != "select":
            try:
                tools.hover(scalp, where[0], where[1], where[2])
                self.hovering = scalp
            except HelperError as e:
                tools.status(context.scene, e.message)
            display.redraw(viewport_only=True)
        return {"PASS_THROUGH"}

    def press(self, context, scalp, event, locate_fn):
        where = self.located(context, scalp, event, locate_fn)
        if where is None:
            return {"PASS_THROUGH"}
        view, x, y = where
        t = context.scene.groomlab_tools
        if self.held:
            setting = SLIDER_KEYS.get(sorted(self.held)[0], "radius") if "M" not in self.held else "strength"
            self.slider = (setting, float(event.mouse_x), float(t.radius if setting == "radius" else t.strength),
                           (view, x, y))
            return {"RUNNING_MODAL"}
        if self.tool(context) == "select":
            mode = "toggle" if event.shift else "deselect" if event.ctrl else "replace"
            self.lasso = (scalp, view, [[x, y]], mode)
            return {"RUNNING_MODAL"}
        pressure = float(getattr(event, "pressure", 1.0) or 1.0)
        try:
            tools.stroke_begin(scalp, view, x, y, pressure=pressure, ctrl=event.ctrl, shift=event.shift)
        except HelperError as e:
            tools.status(context.scene, e.message)
            return {"RUNNING_MODAL"}
        self.stroking = scalp
        self.points = []
        display.apply_strand_visibility(scalp, stroking=True)
        return {"RUNNING_MODAL"}

    def flush(self, context):
        if self.stroking is None or not self.points:
            return
        points, self.points = self.points, []
        try:
            tools.stroke_drag(self.stroking, points)
        except HelperError as e:
            tools.status(context.scene, e.message)
        display.redraw(viewport_only=True)

    def release(self, context, scalp, event, locate_fn):
        if self.slider is not None:
            self.slider = None
            return {"RUNNING_MODAL"}
        if self.stroking is not None:
            where = self.located(context, scalp, event, locate_fn)
            if where is not None:
                self.points.append([where[1], where[2], float(getattr(event, "pressure", 1.0) or 1.0)])
            self.flush(context)
            stroked, self.stroking = self.stroking, None
            try:
                tools.stroke_end(stroked)
            except HelperError as e:
                tools.status(context.scene, e.message)
            display.apply_strand_visibility(stroked)
            return {"RUNNING_MODAL"}
        if self.lasso is not None:
            lasso_scalp, view, points, mode = self.lasso
            self.lasso = None
            shape = context.scene.groomlab_tools.select_shape
            span = max(abs(p[0] - points[0][0]) + abs(p[1] - points[0][1]) for p in points)
            if span < 4.0:
                points = points[:1]                      # a click
            elif shape == "BOX":
                points = [points[0], points[-1]]         # a box's corners
            elif len(points) < 3:
                points = [points[0], points[-1]]
            try:
                r = tools.select(lasso_scalp, view, points, mode)
                tools.status(context.scene, "%d guides selected" % r.get("count", 0))
            except HelperError as e:
                tools.status(context.scene, e.message)
            return {"RUNNING_MODAL"}
        return {"PASS_THROUGH"}

    def drag_slider(self, context, scalp, event):
        """B / M held + drag: the radius (a pixel a pixel) or the strength (200 px for 1), the ring
        redrawn where the drag started, the value in the status line."""
        setting, x0, v0, (view, rx, ry) = self.slider
        t = context.scene.groomlab_tools
        dx = float(event.mouse_x) - x0
        if setting == "radius":
            t.radius = max(1, int(round(v0 + dx)))
            tools.status(context.scene, "Radius %d px" % t.radius)
        else:
            info = tools.tool_info(t.tool)
            lo = -1.0 if info and info.get("signed") else 0.0
            t.strength = min(1.0, max(lo, v0 + dx / 200.0))
            tools.status(context.scene, "Strength %.2f" % t.strength)
        if t.tool != "select":
            try:
                tools.hover(scalp, view, rx, ry)
            except HelperError:
                pass
        display.redraw()

    def cancel(self, context):
        if self.stroking is not None:
            stroked, self.stroking = self.stroking, None
            self.points = []
            try:
                tools.stroke_cancel(stroked)
            except HelperError as e:
                tools.status(context.scene, e.message)
            display.apply_strand_visibility(stroked)
        self.lasso = None
        if self.slider is not None:
            setting, _x0, v0, _ring = self.slider
            t = context.scene.groomlab_tools
            if setting == "radius":
                t.radius = int(v0)
            else:
                t.strength = v0
            self.slider = None
        display.redraw()

    def undo(self, context, scalp, redo=False):
        try:
            r = tools.undo(scalp, redo=redo)
        except HelperError as e:
            tools.status(context.scene, e.message)
            return None
        tools.status(context.scene, ("Redo" if redo else "Undo") + (" done" if r.get("done") else ": nothing to do"))
        return r

    def pick(self, context, tool):
        try:
            tools.pick(context, tool)
        except HelperError as e:
            tools.status(context.scene, e.message)

    def set_cursor(self, context, over):
        """A crosshair over the viewport (the ring is the brush), Blender's cursor elsewhere."""
        window = getattr(context, "window", None)
        if window is None or over == self.cursor:
            return
        try:
            if over:
                window.cursor_modal_set("CROSSHAIR")
            else:
                window.cursor_modal_restore()
        except Exception:
            pass
        self.cursor = over

    def leave(self, context, clear_tool=False):
        self.set_cursor(context, False)
        if self.stroking is not None:
            self.cancel(context)
        if self.hovering is not None:
            tools.hover_clear(self.hovering)
            self.hovering = None
        if clear_tool:
            with props.quiet():
                context.scene.groomlab_tools.tool = ""
        display.redraw()


def step_level(context, delta):
    """Page Up / Page Down: the Group choice one level up or down (Maya's step_level)."""
    t = context.scene.groomlab_tools
    level, _group = tools.group_target(context.scene)
    groups = list(t.groups)
    levels = sorted({g.level for g in groups})
    if not levels:
        return
    target = min(levels, key=lambda lv: abs(lv - (max(level, 0) + delta)))
    for g in groups:
        if g.level == target:
            t.group = g.name
            break


def pie_key():
    """The key the pie menu is bound to (the add-on keymap's, as the user set it)."""
    wm = bpy.context.window_manager
    for kc in (wm.keyconfigs.user, wm.keyconfigs.addon):
        if kc is None:
            continue
        km = kc.keymaps.get("Object Mode")
        if km is None:
            continue
        for kmi in km.keymap_items:
            if kmi.idname == "wm.call_menu_pie" and getattr(kmi.properties, "name", "") == "GROOMLAB_MT_pie" and kmi.active:
                return kmi.type
    return PIE_KEY


_session = [None]   # the running tool's ToolSession (one at a time)


def running():
    return _session[0] is not None


def stroking():
    return _session[0] is not None and _session[0].stroking is not None


class GROOMLAB_OT_tool_modal(bpy.types.Operator):
    """The active GroomLab tool in the 3D viewport (a tile or the pie menu starts it)"""
    bl_idname = "groomlab.tool_modal"
    bl_label = "GroomLab Tool"

    def invoke(self, context, event):
        if _session[0] is not None:
            return {"CANCELLED"}
        if context.window is None:
            return {"CANCELLED"}
        _session[0] = ToolSession()
        context.window_manager.modal_handler_add(self)
        self._status(context)
        return {"RUNNING_MODAL"}

    def _status(self, context):
        tool = context.scene.groomlab_tools.tool
        info = tools.tool_info(tool)
        label = info["label"] if info else tool
        text = ("GroomLab %s: drag over the groom · Ctrl the opposite · Shift smooth / Shift+click a guide solos it · "
                "B/M + drag radius/strength · %s pie · S select · Ctrl+Z undo · Esc leave" % (label, pie_key()))
        try:
            context.workspace.status_text_set(text)
        except Exception:
            pass
        self._shown = tool

    def modal(self, context, event):
        session = _session[0]
        if session is None:
            return self._end(context)
        try:
            result = session.event(context, event)
        except Exception as e:  # noqa: BLE001 - a modal operator must never leave Blender stuck
            session.leave(context)
            print("GroomLab tool: %r" % e)
            return self._end(context)
        if "FINISHED" in result:
            return self._end(context)
        if getattr(self, "_shown", None) != context.scene.groomlab_tools.tool:
            self._status(context)
        return result

    def _end(self, context):
        _session[0] = None
        try:
            context.workspace.status_text_set(None)
        except Exception:
            pass
        display.redraw()
        return {"FINISHED"}

    def cancel(self, context):
        if _session[0] is not None:
            _session[0].leave(context)
        _session[0] = None


def start_modal(context):
    """The tool in the viewport (from a tile, the pie or the menu), once."""
    if running() or bpy.app.background or context.window is None:
        return False
    try:
        return "RUNNING_MODAL" in bpy.ops.groomlab.tool_modal("INVOKE_DEFAULT")
    except RuntimeError:
        return False


class GroomToolOperator(bpy.types.Operator):
    @classmethod
    def poll(cls, context):
        s = active(context)
        return s is not None and tools.is_hierarchy(s)


class GROOMLAB_OT_tool(bpy.types.Operator):
    """Use this GroomLab tool: drag over the groom in the viewport"""
    bl_idname = "groomlab.tool"
    bl_label = "GroomLab Tool"
    bl_options = {"REGISTER"}

    tool: StringProperty(name="Tool")

    @classmethod
    def description(cls, context, properties):
        info = tools.tool_info(properties.tool)
        return "%s: %s" % (info["label"], info["hint"]) if info else "Use this GroomLab tool"

    def execute(self, context):
        try:
            tools.catalog()
        except HelperError as e:
            return _fail(self, e)
        s = active(context)
        if s is not None and not tools.is_hierarchy(s):
            self.report({"WARNING"}, "this groom's guides are Blender curves (sculpt them with Sculpt Guides); "
                                     "GroomLab's tools work on grooms made with Create Groom")
            return {"CANCELLED"}
        try:
            tools.pick(context, self.tool)
        except HelperError as e:
            return _fail(self, e)
        if s is not None:
            start_modal(context)
        return {"FINISHED"}


class GROOMLAB_OT_tool_exit(bpy.types.Operator):
    """Leave the GroomLab tool (Blender's own tools again)"""
    bl_idname = "groomlab.tool_exit"
    bl_label = "Leave Tool"

    def execute(self, context):
        with props.quiet():
            context.scene.groomlab_tools.tool = ""
        if _session[0] is not None:
            _session[0].leave(context)
        display.redraw()
        return {"FINISHED"}


class GROOMLAB_OT_tool_undo(GroomToolOperator):
    """Undo the groom's last edit (a stroke, a guide edit, a map): GroomLab's own undo"""
    bl_idname = "groomlab.tool_undo"
    bl_label = "Undo"

    redo: BoolProperty(name="Redo", default=False, options={"SKIP_SAVE"})

    def execute(self, context):
        try:
            r = tools.undo(active(context), redo=self.redo)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"} if r.get("done") else {"CANCELLED"}


class GROOMLAB_OT_step_level(GroomToolOperator):
    """Brush one guide level up (the parents) or down (the children)"""
    bl_idname = "groomlab.step_level"
    bl_label = "Step Level"

    delta: IntProperty(default=1)

    def execute(self, context):
        step_level(context, self.delta)
        return {"FINISHED"}


# ---- Guide groups and edits (Maya's "Guide groups & edits") --------------------------------------

class GROOMLAB_OT_group_new(GroomToolOperator):
    """New group (top-level, or a child group of another): Place and Draw add to it"""
    bl_idname = "groomlab.group_new"
    bl_label = "New Guide Group"
    bl_options = {"REGISTER", "UNDO"}

    name: StringProperty(name="Name", description="e.g. bangs, beard, flyaways")
    parent: StringProperty(name="Parent", description="Empty: a top-level group; a group's name: a child group of it")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        if not self.name.strip():
            self.report({"ERROR"}, "name the group")
            return {"CANCELLED"}
        args = {"name": self.name.strip()}
        if self.parent.strip():
            args["parent"] = self.parent.strip()
        try:
            r = tools.guides_edit(active(context), "add_group", **args)
        except HelperError as e:
            return _fail(self, e)
        made = r.get("group") or self.name.strip()
        context.scene.groomlab_tools.group = made
        tools.status(context.scene, "New guides go into %s." % made)
        return {"FINISHED"}


class GROOMLAB_OT_group_edit(GroomToolOperator):
    """Edit the picked guide group"""
    bl_idname = "groomlab.group_edit"
    bl_label = "Edit Group"
    bl_options = {"REGISTER", "UNDO"}

    action: EnumProperty(items=[("RENAME", "Rename", "Rename the group; graph params naming it follow"),
                                ("SELECT", "Select", "Select the group's guides"),
                                ("DELETE", "Delete", "Delete the group, its child groups and their guides"),
                                ("HIDE", "Show / Hide", "Show or hide the group in the viewport"),
                                ("PROMOTE", "Promote", "Move the group one level up"),
                                ("DEMOTE", "Demote", "Move the group one level down, under a sibling group")])
    group: StringProperty(name="Group", options={"SKIP_SAVE"})
    name: StringProperty(name="New Name", options={"SKIP_SAVE"})
    parent: StringProperty(name="New Parent", options={"SKIP_SAVE"})

    @classmethod
    def description(cls, context, properties):
        return {"RENAME": "Rename the group; graph params naming it follow", "SELECT": "Select the group's guides",
                "DELETE": "Delete the group, its child groups and their guides",
                "HIDE": "Show or hide the group in the viewport", "PROMOTE": "Move the group one level up",
                "DEMOTE": "Move the group one level down, under a sibling group"}.get(properties.action, "")

    def invoke(self, context, event):
        if not self.group:
            self.group = context.scene.groomlab_tools.group
        if self.action == "RENAME":
            self.name = self.group
            return context.window_manager.invoke_props_dialog(self)
        if self.action == "DEMOTE":
            return context.window_manager.invoke_props_dialog(self)
        if self.action == "DELETE":
            return context.window_manager.invoke_confirm(self, event)
        return self.execute(context)

    def execute(self, context):
        scalp = active(context)
        group = self.group or context.scene.groomlab_tools.group
        if not group or group == tools.ALL_GROUPS:
            self.report({"ERROR"}, "pick a group")
            return {"CANCELLED"}
        try:
            if self.action == "RENAME":
                tools.guides_edit(scalp, "rename_group", group=group, name=self.name.strip())
                context.scene.groomlab_tools.group = self.name.strip()
            elif self.action == "SELECT":
                tools.selection_set(scalp, group=group, mode="replace")
            elif self.action == "DELETE":
                tools.guides_edit(scalp, "remove_group", group=group)
            elif self.action == "HIDE":
                hidden = set(g.name for g in context.scene.groomlab_tools.groups if g.hidden)
                hidden ^= {group}
                tools.set_hidden_groups(scalp, sorted(hidden))
            elif self.action == "PROMOTE":
                tools.guides_edit(scalp, "promote_group", group=group)
            elif self.action == "DEMOTE":
                tools.guides_edit(scalp, "demote_group", group=group, parent=self.parent.strip())
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


GUIDE_EDITS = [
    ("CHILDREN", "Children", "Generate children for the selected guides (or the picked group): the count beside"),
    ("SYNC_POS", "Sync +X → -X", "Copy every guide on the +X side, children and edits included, over the -X side"),
    ("SYNC_NEG", "Sync -X → +X", "Copy every guide on the -X side over the +X side"),
    ("MIRROR_GROUP", "Mirror Group", "Copy the picked root group from one side to the other"),
    ("LOCK", "Lock", "Lock the selected guides: their children stop following them"),
    ("UNLOCK", "Unlock", "Unlock the selected guides"),
    ("FREEZE", "Freeze", "Freeze the selected guides: brushes leave them alone (they draw blue)"),
    ("UNFREEZE", "Unfreeze", "Unfreeze the selected guides"),
    ("UNFREEZE_ALL", "Unfreeze All", "Clear the freeze mask: every guide can be brushed again"),
    ("DELETE", "Delete Selected", "Delete the selected guides (and their children)"),
    ("SELECT_ALL", "Select All", "Select every guide of the picked group"),
    ("CLEAR", "Clear Selection", "Deselect every guide"),
]


class GROOMLAB_OT_guides_edit(GroomToolOperator):
    """Edits on many guides (Maya's Edit Guides menu): one undo step each"""
    bl_idname = "groomlab.guides_edit"
    bl_label = "Edit Guides"
    bl_options = {"REGISTER", "UNDO"}

    action: EnumProperty(items=GUIDE_EDITS)

    @classmethod
    def description(cls, context, properties):
        return next((tip for k, _l, tip in GUIDE_EDITS if k == properties.action), "")

    def execute(self, context):
        scalp = active(context)
        t = context.scene.groomlab_tools
        level, group = tools.group_target(context.scene)
        a = self.action
        try:
            if a == "CHILDREN":
                args = {"count": int(t.children_count)}
                if t.selected_count:
                    args["selected"] = True
                elif group:
                    args["group"] = group
                tools.guides_edit(scalp, "generate_children", **args)
            elif a in ("SYNC_POS", "SYNC_NEG"):
                tools.guides_edit(scalp, "mirror_all", axis="x", center=0.0, fromPositive=a == "SYNC_POS")
            elif a == "MIRROR_GROUP":
                if not group:
                    self.report({"ERROR"}, "pick a root group")
                    return {"CANCELLED"}
                tools.guides_edit(scalp, "mirror_group", group=group, axis="x", center=0.0, fromPositive=True)
            elif a in ("LOCK", "UNLOCK", "FREEZE", "UNFREEZE", "DELETE"):
                if not t.selected_count:
                    self.report({"ERROR"}, "select some guides first (the Select tool, or a group's Select)")
                    return {"CANCELLED"}
                op = {"LOCK": "lock", "UNLOCK": "unlock", "FREEZE": "freeze", "UNFREEZE": "unfreeze",
                      "DELETE": "delete_guides"}[a]
                tools.guides_edit(scalp, op)
            elif a == "UNFREEZE_ALL":
                tools.guides_edit(scalp, "unfreeze_all")
            elif a == "SELECT_ALL":
                if group:
                    tools.selection_set(scalp, group=group, mode="replace")
                else:
                    tools.selection_set(scalp, all=True, mode="replace")
            elif a == "CLEAR":
                tools.selection_set(scalp, ids=[], mode="replace")
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


# ---- Maps (Maya's Paint tab) ---------------------------------------------------------------------

_TARGET_ITEMS = []  # Blender keeps no reference to a dynamic enum's items: these are held here
_OTHER_ITEMS = []
FOR_TIP = ("A map for a setting is made white (the setting as it is), linked to it and painted at once, one undo "
           "step: strokes paint toward the value, Ctrl toward 0 (on Density: no hair). When the setting reads a map "
           "already, that map is picked instead")


def _target_items(self, context):
    """New Map's For: nothing (a blank map), the common settings the graph has a node for, Other."""
    _TARGET_ITEMS[:] = [("NONE", "Nothing yet (a blank map)", "A blank map, named as you like")]
    scalp = active(context) if context else None
    if scalp is not None:
        try:
            settings, others = tools.setting_targets(scalp)
        except Exception:  # noqa: BLE001 - no helper yet: a blank map only
            settings, others = [], []
        for s in settings:
            if s["node"]:
                _TARGET_ITEMS.append(("%s.%s" % (s["node"], s["param"]), "Setting: %s" % s["label"], s["hint"]))
        if others:
            _TARGET_ITEMS.append(("OTHER", "Other setting...", "Any mappable param of the graph"))
    return _TARGET_ITEMS


def _other_items(self, context):
    _OTHER_ITEMS[:] = []
    scalp = active(context) if context else None
    if scalp is not None:
        try:
            _OTHER_ITEMS.extend((o["id"], o["label"], o["hint"][:200]) for o in tools.setting_targets(scalp)[1])
        except Exception:  # noqa: BLE001
            pass
    return _OTHER_ITEMS or [("NONE", "(no mappable params)", "")]


class GROOMLAB_OT_map_new(GroomToolOperator):
    """A new map: on the guides, on the scalp's vertices, or a texture; blank, or for a setting (Density, Width,
    Length...: made, linked and painted at once; Ctrl on Density removes hair)"""
    bl_idname = "groomlab.map_new"
    bl_label = "New Map"
    bl_options = {"REGISTER", "UNDO"}

    target: EnumProperty(name="For", items=_target_items, description=FOR_TIP)
    other: EnumProperty(name="Setting", items=_other_items, description="Every mappable param of the graph")
    name: StringProperty(name="Name", default="mask", description="Letters, digits and _ (not starting with a digit)")
    kind: EnumProperty(name="Kind", items=[("guides", "On the guides", "A layer on the guides (children follow their parents)"),
                                           ("vertices", "On the scalp's vertices", "A value per scalp vertex"),
                                           ("texture", "A texture", "An image at the scalp's UVs")], default="vertices")
    color: BoolProperty(name="Color (r g b a)", default=False, description="Four channels instead of one gray value")
    value: FloatProperty(name="Fill", default=0.0, min=0.0, max=1.0, subtype="FACTOR")
    resolution: EnumProperty(name="Resolution", items=[(str(r), "%d x %d" % (r, r), "") for r in (256, 512, 1024, 2048, 4096)],
                             default="1024")

    def invoke(self, context, event):
        return context.window_manager.invoke_props_dialog(self)

    def chosen(self):
        """(node, param) the map is for, or None (a blank map)."""
        key = self.other if self.target == "OTHER" else self.target
        if not key or key == "NONE" or "." not in key:
            return None
        node, param = key.split(".", 1)
        return node, param

    def draw(self, context):
        layout = self.layout
        layout.prop(self, "target")
        if self.target == "OTHER":
            layout.prop(self, "other")
        setting = self.chosen() is not None
        if not setting:
            layout.prop(self, "name")
        layout.prop(self, "kind")
        if not setting:
            layout.prop(self, "color")
            layout.prop(self, "value")
        if self.kind == "texture":
            layout.prop(self, "resolution")
        if setting:
            node, param = self.chosen()
            layout.label(text="Named %s_%s, white; Ctrl paints toward 0" % (node, param))

    def execute(self, context):
        scalp = active(context)
        target = self.chosen()
        try:
            if target:
                # A setting's map lives on the scalp (vertices or a texture).
                kind = self.kind if self.kind in ("vertices", "texture") else "vertices"
                r = tools.map_for_param(scalp, target[0], target[1], kind=kind, resolution=int(self.resolution))
                tools.pick(context, "paint")
                tools.status(context.scene, "%s %s, which %s.%s reads: Ctrl paints it toward 0%s, strokes back toward the "
                                            "value" % ("Made" if r.get("made") else "Painting", r["name"], target[0],
                                                       target[1], " (no hair there)" if target[1] == "density" else ""))
                return {"FINISHED"}
            tools.map_create(scalp, self.name.strip(), kind=self.kind, color=self.color and self.kind != "guides",
                             value=self.value, resolution=int(self.resolution))
        except HelperError as e:
            return _fail(self, e)
        tools.push_safely(context)
        return {"FINISHED"}


class GROOMLAB_OT_paint_param(GroomToolOperator):
    """Paint the map this param reads (one made white and linked, on the scalp's vertices, when it reads none):
    the Paint tool on it, Ctrl toward 0"""
    bl_idname = "groomlab.paint_param"
    bl_label = "Paint Param"
    bl_options = {"REGISTER", "UNDO"}

    node: StringProperty(name="Node")
    param: StringProperty(name="Param")

    def execute(self, context):
        try:
            tools.map_for_param(active(context), self.node, self.param)
            tools.pick(context, "paint")
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_map_remove(GroomToolOperator):
    """Delete the map (a texture map's file stays on disk)"""
    bl_idname = "groomlab.map_remove"
    bl_label = "Delete Map"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        name = context.scene.groomlab_tools.paint_map
        if not name:
            return {"CANCELLED"}
        try:
            tools.map_remove(active(context), name)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_map_fill(GroomToolOperator):
    """Set the guide map to the value: on the selected guides when some are, else every guide"""
    bl_idname = "groomlab.map_fill"
    bl_label = "Fill"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        scene = context.scene
        m = tools.current_map(scene)
        if m is None or m.kind != "guides":
            self.report({"ERROR"}, "Fill sets a map on the guides; paint scalp and texture maps")
            return {"CANCELLED"}
        scalp = active(context)
        try:
            args = {"name": m.name, "value": float(scene.groomlab_tools.value)}
            if not scene.groomlab_tools.selected_count:
                args["ids"] = _all_guide_ids(scalp)
            tools.guides_edit(scalp, "set_attribute", **args)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


def _all_guide_ids(scalp):
    g = tools.call(scalp, "get_guides", source="hierarchy")
    return [int(i) for i in process.client().array(g["arrays"]["ids"]).tolist()]


# ---- Strand colors ---------------------------------------------------------------------------------

class GROOMLAB_OT_show_strand_colors(GroomToolOperator):
    """Show the strand colors in the 3D views (Solid shading: Attribute color)"""
    bl_idname = "groomlab.show_strand_colors"
    bl_label = "Show in Viewport"

    def execute(self, context):
        display.show_attribute_colors(context)
        return {"FINISHED"}


# ---- The pie menu (Maya's marking menu) ----------------------------------------------------------

def _tool_button(layout, tool, label=None):
    info = tools.tool_info(tool)
    text = label or (info["label"] if info else tool.capitalize())
    op = layout.operator("groomlab.tool", text=text, icon_value=icons.icon(tool))
    op.tool = tool
    return op


def overflow():
    """The catalog's tools not on the radial (Select and Freeze last, after a divider)."""
    radial = set(mode for _, mode in RADIAL)
    return [t["id"] for t in tools.cached_catalog().get("tools", []) if t["id"] not in radial
            and t["id"] not in ("select", "freeze")]


class GROOMLAB_MT_pie(bpy.types.Menu):
    bl_idname = "GROOMLAB_MT_pie"
    bl_label = "GroomLab"

    @classmethod
    def poll(cls, context):
        return active(context) is not None

    def draw(self, context):
        try:
            tools.catalog()
        except HelperError:
            pass
        pie = self.layout.menu_pie()
        by_dir = dict(RADIAL)
        for direction in PIE_ORDER:
            if direction == "S":
                # South: Place, and the rest of the catalog in a list below the pie (Maya's overflow).
                col = pie.column()
                _tool_button(col, by_dir["S"])
                col.separator()
                box = col.box().column(align=True)
                for tool in overflow():
                    _tool_button(box, tool)
                box.separator()
                _tool_button(box, "select")
                _tool_button(box, "freeze")
                box.separator()
                box.operator("groomlab.groom_tools", text="Groom Tools", icon_value=icons.icon("palette"))
            else:
                _tool_button(pie, by_dir[direction])


# ---- Keymaps ---------------------------------------------------------------------------------------

_keymaps = []


def register_keymaps():
    kc = bpy.context.window_manager.keyconfigs.addon if bpy.context.window_manager else None
    if kc is None:
        return
    km = kc.keymaps.new(name="Object Mode", space_type="EMPTY")
    kmi = km.keymap_items.new("wm.call_menu_pie", PIE_KEY, "PRESS")
    kmi.properties.name = GROOMLAB_MT_pie.bl_idname
    _keymaps.append((km, kmi))


def unregister_keymaps():
    for km, kmi in _keymaps:
        try:
            km.keymap_items.remove(kmi)
        except (ReferenceError, RuntimeError):
            pass
    _keymaps.clear()


classes = (GROOMLAB_OT_tool_modal, GROOMLAB_OT_tool, GROOMLAB_OT_tool_exit, GROOMLAB_OT_tool_undo,
           GROOMLAB_OT_step_level, GROOMLAB_OT_group_new, GROOMLAB_OT_group_edit, GROOMLAB_OT_guides_edit,
           GROOMLAB_OT_map_new, GROOMLAB_OT_paint_param, GROOMLAB_OT_map_remove, GROOMLAB_OT_map_fill,
           GROOMLAB_OT_show_strand_colors, GROOMLAB_MT_pie)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    register_keymaps()


def unregister():
    unregister_keymaps()
    _session[0] = None
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
