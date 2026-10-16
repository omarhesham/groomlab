# SPDX-License-Identifier: GPL-3.0-or-later
"""Groom Tools: Maya's GroomLab palette (hosts/maya/scripts/groomlab/ui/palette.py) as the 3D
view sidebar's GroomLab tab. The groom picker with its eye, the Graph Editor and a More menu; how
to start a groom, then the first-groom steps; the Group the brushes edit
and Isolate; the Tools and Paint tabs (tiles by the catalog's categories, with Maya's icons); the
active tool's card with exactly the settings the catalog lists for it, by Maya's labels and hints;
GroomLab's undo; and, folded, While sculpting, Guide groups & edits, and the groom's Display.

Drawing never writes Blender data: the lists (groups, maps) are refreshed by the tools after each
edit, or by a timer when the panel shows another groom than they were filled for."""

import bpy

from . import groom, icons, process, tools
from .ops import active_groom
from .ui import _wrap

NO_TOOL = "Pick a tool, then drag over the groom in the viewport."
GESTURES = ("B + drag: radius · M + drag: strength · Shift+click a guide: solo it (the rest frozen) and brush its "
            "group · Shift+click off a guide: unfreeze all")
FLOW_HINTS = {"": "Drag to lay arrows; drag over arrows to comb them; Ctrl erases.",
              "*": "Click to place, the size of the brush; Ctrl-click removes sinks."}
STARTER = (("guides", "Place some guides", "Place: click the scalp, or drag to lay down many."),
           ("look", "Give it a look", "Graph Editor: add Clump, Noise or Curl after Interpolate."))
_lists_for = [None]


def _chars(context):
    region = getattr(context, "region", None)
    width = region.width if region is not None and region.width > 50 else 300
    scale = (context.preferences.system.ui_scale if context.preferences else 1.0) or 1.0
    return max(20, int(width / (7.0 * scale)))


def _lists_current(context, scalp):
    """Fills the palette's lists (by a timer: drawing cannot) when they are another groom's."""
    rt = groom.runtime_if_any(scalp)
    if rt is None or rt.session is None or not process.running():
        return
    if _lists_for[0] != rt.uid and not bpy.app.timers.is_registered(_refresh_lists):
        bpy.app.timers.register(_refresh_lists, first_interval=0.0)


def _refresh_lists():
    context = bpy.context
    scalp = active_groom(context)
    if scalp is not None and tools.is_hierarchy(scalp):
        try:
            tools.session(scalp)
            tools.refresh_info(scalp)
            _lists_for[0] = groom.runtime(scalp).uid
        except Exception as e:  # noqa: BLE001
            print("GroomLab: %r" % e)
        groom._redraw()
    return None


def tile(layout, tool, active):
    info = tools.tool_info(tool)
    op = layout.operator("groomlab.tool", text=info["label"] if info else tool, icon_value=icons.icon(tool),
                         depress=active == tool)
    op.tool = tool
    return op


def tool_grid(layout, tool_ids, active, columns=4):
    grid = layout.grid_flow(row_major=True, columns=columns, even_columns=True, even_rows=True, align=True)
    grid.scale_y = 1.4
    for t in tool_ids:
        tile(grid, t, active)
    return grid


def heading(layout, text):
    row = layout.row()
    row.label(text=text.upper())
    return row


class VIEW3D_PT_groomlab_tools(bpy.types.Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "GroomLab"
    bl_label = "Groom Tools"
    bl_order = 0

    def draw_header(self, context):
        self.layout.label(text="", icon_value=icons.icon("palette"))

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        t = scene.groomlab_tools
        scalp = active_groom(context)
        # What the tools work on: the groom, its eye, the Graph Editor, More.
        row = layout.row(align=True)
        row.prop(scene.groomlab, "groom", text="")
        if scalp is not None:
            d = scalp.groomlab_tools
            row.prop(d, "show_strands", text="", icon="HIDE_OFF" if d.show_strands else "HIDE_ON")
        row.operator("groomlab.graph_open", text="", icon_value=icons.icon("graph"))
        row.menu("GROOMLAB_MT_tools_more", text="", icon="DOWNARROW_HLT")
        if scalp is None:
            box = layout.box()
            box.label(text="Start a groom")
            _wrap(box, "Select the scalp mesh, then:", width=_chars(context))
            box.operator("groomlab.add_groom", text="Create Groom from Selection", icon="ADD")
            return
        if not tools.is_hierarchy(scalp):
            box = layout.box()
            _wrap(box, "This groom's guides are Blender curves: sculpt them with Blender's brushes. A groom made with "
                       "Create Groom uses GroomLab's own tools.", icon="INFO", width=_chars(context))
            box.operator("groomlab.sculpt_guides", icon="SCULPTMODE_HLT")
            return
        if not tools.cached_catalog():
            if process.running():
                try:
                    tools.catalog()
                except Exception:
                    pass
            if not tools.cached_catalog():
                layout.operator("groomlab.update", text="Start GroomLab", icon="FILE_REFRESH")
                return
        _lists_current(context, scalp)
        if t.guide_count == 0 and not t.starter_dismissed:
            self.draw_starter(layout, context)
        row = layout.row(align=True)
        row.label(text="Group")
        row.prop(t, "group", text="")
        row.prop(t, "isolate", toggle=True)
        if t.status:
            _wrap(layout, t.status, icon="INFO", width=_chars(context))
        layout.row().prop(t, "tab", expand=True)
        if t.tab == "TOOLS":
            for category, tlist in tools.sections():
                heading(layout, category)
                ids = [x["id"] for x in tlist]
                grid = tool_grid(layout, ids, t.tool)
                if "freeze" in ids:  # beside Freeze: every guide brushable again
                    grid.operator("groomlab.guides_edit", text="Unfreeze All",
                                  icon_value=icons.icon("unfreeze")).action = "UNFREEZE_ALL"
        else:
            self.draw_paint(layout, context, t)
        self.draw_card(layout, context, t, scalp)
        row = layout.row(align=True)
        undo, redo = tools.undo_label(scalp), tools.undo_label(scalp, redo=True)
        sub = row.row(align=True)
        sub.enabled = bool(undo)
        sub.operator("groomlab.tool_undo", text="Undo %s" % undo if undo else "Undo", icon="LOOP_BACK").redo = False
        sub = row.row(align=True)
        sub.enabled = bool(redo)
        sub.operator("groomlab.tool_undo", text="Redo %s" % redo if redo else "Redo", icon="LOOP_FORWARDS").redo = True

    def draw_starter(self, layout, context):
        box = layout.box()
        row = box.row()
        row.label(text="Your first groom")
        row.prop(context.scene.groomlab_tools, "starter_dismissed", text="", icon="X", emboss=False)
        first = True
        for key, text, tip in STARTER:
            op = box.operator("groomlab.tool" if key == "guides" else "groomlab.graph_open",
                              text=("▶  " if first else "○  ") + text, emboss=False)
            if key == "guides":
                op.tool = "place"
            if first:
                _wrap(box, tip, width=_chars(context))
            first = False

    def draw_paint(self, layout, context, t):
        tool_grid(layout, ["paint"], t.tool)
        row = layout.row(align=True)
        row.operator("groomlab.map_new", text="New Map", icon="ADD")
        row.operator("groomlab.map_remove", text="", icon="TRASH")
        layout.template_list("GROOMLAB_UL_maps", "", t, "maps", t, "map_index", rows=4)
        m = tools.current_map(context.scene)
        col = layout.column(align=True)
        if m is None or m.channels == 1:
            row = col.row(align=True)
            row.prop(t, "value", slider=True)
            fill = row.row(align=True)
            fill.enabled = m is not None and m.kind == "guides"
            fill.operator("groomlab.map_fill", text="Fill")
        if m is not None and m.channels > 1:
            row = col.row(align=True)
            row.prop(t, "paint_color", text="Color")
            for ch in ("channel_r", "channel_g", "channel_b", "channel_a"):
                row.prop(t, ch, toggle=True)
        row = layout.row(align=True)
        row.label(text="Show on")
        row.prop(t, "show_on_guides", toggle=True)
        row.prop(t, "show_on_scalp", toggle=True)
        if m is not None and m.channels > 1:
            row.prop(t, "scalp_channel", text="")
        _wrap(layout, "A map is a 0-1 value painted on the scalp that any param can read (the Graph Editor's Map node).",
              width=_chars(context))

    def draw_card(self, layout, context, t, scalp):
        tool = t.tool
        info = tools.tool_info(tool) if tool else None
        heading(layout, info["label"] if info else "Tool")
        if info is None:
            _wrap(layout, NO_TOOL, width=_chars(context))
            return
        _wrap(layout, info["hint"], width=_chars(context))
        uses = set(info["settings"])
        card = layout.box()
        label = tools.setting_label
        if "radius" in uses:
            col = card.column(align=True)
            col.prop(t, "radius", text=label("radius", "Radius"))
            col.prop(t, "strength", text="Strength (±)" if info.get("signed") else label("strength", "Strength"),
                     slider=True)
            col.prop(t, "falloff", text=label("falloff", "Falloff"), slider=True)
            if "falloffProfile" in uses:
                col.prop(t, "falloff_curve", text=label("falloffProfile", "Falloff curve"))
            if "strengthProfile" in uses:
                col.prop(t, "strength_profile", text=label("strengthProfile", "Root to tip"))
            if "smoothStrength" in uses:
                col.prop(t, "smooth_strength", text=label("smoothStrength", "Smooth"))
        checks = [(k, p) for k, p in (("symmetry", "symmetry"), ("selectedOnly", "selected_only"),
                                      ("pressure", "pressure"), ("collide", "collide"),
                                      ("frontFacingOnly", "front_facing")) if k in uses]
        if checks:
            grid = card.grid_flow(row_major=True, columns=2, even_columns=True, align=False)
            for k, p in checks:
                grid.prop(t, p, text=label(k, k))
        if "symmetry" in uses and t.symmetry:
            card.row(align=True).prop(t, "symmetry_mode", expand=True)
        if "newGuides" in uses:
            box = card.column()
            box.label(text=label("newGuides", "New guides").upper())
            box.prop(t, "match_neighbors")
            if "soloNewGuides" in uses:
                box.prop(t, "solo_new_guides", text=label("soloNewGuides", "Solo new guides"))
            box.prop(t, "length")
            box.prop(t, "segments")
            if "spacing" in uses:
                box.prop(t, "spacing")
        if "parts" in uses:
            box = card.column()
            box.label(text=label("parts", "Parts").upper())
            box.prop(t, "part_name")
        if "flow" in uses:
            box = card.column()
            box.label(text=label("flow", "Flow").upper())
            box.prop(t, "flow_sink_item")
            _wrap(box, FLOW_HINTS["" if t.flow_sink_item == "ARROWS" else "*"], width=_chars(context))
        if "stretch" in uses:
            card.prop(t, "stretch", text=label("stretch", "Stretch"))
        if "erases" in uses:
            row = card.row(align=True)
            row.label(text=label("erases", "Erases"))
            for p in ("erase_guides", "erase_parts", "erase_flow"):
                row.prop(t, p, toggle=True)
        if "targets" in uses:
            row = card.row(align=True)
            row.label(text=label("targets", "Acts on"))
            for p in ("target_guides", "target_parts", "target_flow"):
                row.prop(t, p, toggle=True)
        if "map" in uses:
            m = tools.current_map(context.scene)
            card.label(text="%s: %s" % (label("map", "Map"), m.name if m else "make one in the Paint tab"),
                       icon="BRUSH_DATA")
        if "selection" in uses:
            box = card.column()
            box.label(text=label("selection", "Selection").upper())
            box.prop(t, "select_shape", expand=True)
            box.prop(t, "soft_radius")
            row = box.row(align=True)
            row.label(text="%d selected" % t.selected_count)
            row.operator("groomlab.guides_edit", text="All").action = "SELECT_ALL"
            row.operator("groomlab.guides_edit", text="None").action = "CLEAR"
            _wrap(box, "Drag: lasso (or box) · click: a guide · Shift: toggle · Ctrl: deselect · the rest freeze, so "
                       "brushes work on the selection (nothing picked: all unfrozen)", width=_chars(context))
        if "freezeMask" in uses:
            box = card.column()
            box.label(text=label("freezeMask", "Freeze mask").upper())
            row = box.row(align=True)
            row.label(text="%d frozen" % t.frozen_count)
            row.operator("groomlab.guides_edit", text="Unfreeze All").action = "UNFREEZE_ALL"
        if "radius" in uses:
            _wrap(card, GESTURES, width=_chars(context))
        row = layout.row(align=True)
        row.operator("groomlab.tool_exit", text="Leave Tool (Esc)", icon="CANCEL")


class GroomToolsSubpanel(object):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "GroomLab"
    bl_parent_id = "VIEW3D_PT_groomlab_tools"

    @classmethod
    def poll(cls, context):
        s = active_groom(context)
        return s is not None and tools.is_hierarchy(s)


class VIEW3D_PT_groomlab_tools_sculpting(GroomToolsSubpanel, bpy.types.Panel):
    bl_label = "While sculpting"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        t = context.scene.groomlab_tools
        self.layout.prop(t, "sculpt_display")
        self.layout.prop(t, "full_density")


class GROOMLAB_UL_groups(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index=0):
        row = layout.row(align=True)
        row.label(text="    " * item.depth + item.name)
        sub = row.row(align=True)
        sub.alignment = "RIGHT"
        sub.label(text="%d" % item.guides)
        sub.label(text="L%d" % item.level)
        op = sub.operator("groomlab.group_edit", text="", icon="HIDE_ON" if item.hidden else "HIDE_OFF", emboss=False)
        op.action = "HIDE"
        op.group = item.name


class GROOMLAB_UL_maps(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index=0):
        row = layout.row(align=True)
        kind = {"guides": "On the guides", "vertices": "On the scalp's vertices", "texture": "A texture"}.get(item.kind, item.kind)
        row.label(text=item.name, icon="TEXTURE" if item.kind == "texture" else "VERTEXSEL" if item.kind == "vertices"
                  else "CURVES")
        sub = row.row()
        sub.alignment = "RIGHT"
        sub.label(text=kind + (" (color)" if item.channels > 1 else ""))


class GROOMLAB_MT_guide_edits(bpy.types.Menu):
    """Children, symmetry, mirroring, locking, freezing, deleting, selection"""
    bl_label = "Edit Guides"

    def draw(self, context):
        from .tool_ops import GUIDE_EDITS
        layout = self.layout
        for i, (key, label, _tip) in enumerate(GUIDE_EDITS):
            if key in ("SYNC_POS", "LOCK", "UNFREEZE_ALL", "SELECT_ALL"):
                layout.separator()
            layout.operator("groomlab.guides_edit", text=label).action = key
        layout.separator()
        layout.operator("groomlab.group_edit", text="Promote Group").action = "PROMOTE"
        layout.operator("groomlab.group_edit", text="Demote Group...").action = "DEMOTE"


class VIEW3D_PT_groomlab_tools_guides(GroomToolsSubpanel, bpy.types.Panel):
    bl_label = "Guide groups & edits"
    bl_options = {"DEFAULT_CLOSED"}

    @classmethod
    def poll(cls, context):
        return GroomToolsSubpanel.poll(context) and context.scene.groomlab_tools.tab == "TOOLS"

    def draw(self, context):
        layout = self.layout
        t = context.scene.groomlab_tools
        _wrap(layout, "Place and Draw add to the Group picked at the top; brushes edit it. Guides nodes in the graph "
                      "read groups by name.", width=_chars(context))
        row = layout.row(align=True)
        row.operator("groomlab.group_new", text="New Group", icon="ADD")
        row.operator("groomlab.group_edit", text="", icon="GREASEPENCIL").action = "RENAME"
        row.operator("groomlab.group_edit", text="", icon="RESTRICT_SELECT_OFF").action = "SELECT"
        row.operator("groomlab.group_edit", text="", icon="TRASH").action = "DELETE"
        layout.template_list("GROOMLAB_UL_groups", "", t, "groups", t, "group_index", rows=4)
        row = layout.row(align=True)
        row.menu("GROOMLAB_MT_guide_edits", text="Edit Guides")
        row.prop(t, "children_count", text="x")
        scalp = active_groom(context)
        if scalp is not None:
            layout.prop(scalp.groomlab_tools, "guide_color_mode")
        layout.label(text="%d guides, %d selected, %d frozen" % (t.guide_count, t.selected_count, t.frozen_count))


class VIEW3D_PT_groomlab_tools_display(GroomToolsSubpanel, bpy.types.Panel):
    bl_label = "Display"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        d = active_groom(context).groomlab_tools
        row = layout.row(align=True)
        for p in ("show_guides", "show_tubes", "show_flow", "show_strands"):
            row.prop(d, p, toggle=True)
        layout.prop(d, "guide_points")
        layout.prop(d, "guide_color_mode")
        if d.guide_color_mode == "layer":
            layout.prop(d, "guide_color_layer")
        layout.prop(d, "strand_color")
        if d.strand_color in ("colorSet", "attribute"):
            layout.prop(d, "strand_color_source")
            row = layout.row(align=True)
            row.prop(d, "strand_color_min")
            row.prop(d, "strand_color_max")
        if d.strand_color != "solid":
            layout.operator("groomlab.show_strand_colors", icon="SHADING_SOLID")


class GROOMLAB_MT_tools_more(bpy.types.Menu):
    """More: a new groom, leave the tool"""
    bl_label = "More"

    def draw(self, context):
        layout = self.layout
        layout.operator("groomlab.add_groom", text="Create Groom from Selection")
        layout.operator("groomlab.tool_exit")
        layout.separator()
        layout.operator("groomlab.shortcuts", text="Keyboard Shortcuts")


classes = (VIEW3D_PT_groomlab_tools, VIEW3D_PT_groomlab_tools_sculpting, GROOMLAB_UL_groups, GROOMLAB_UL_maps,
           GROOMLAB_MT_guide_edits, VIEW3D_PT_groomlab_tools_guides, VIEW3D_PT_groomlab_tools_display,
           GROOMLAB_MT_tools_more)


def register():
    for c in classes:
        bpy.utils.register_class(c)


def unregister():
    if bpy.app.timers.is_registered(_refresh_lists):
        bpy.app.timers.unregister(_refresh_lists)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
