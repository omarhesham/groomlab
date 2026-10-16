# SPDX-License-Identifier: GPL-3.0-or-later
"""The GroomLab tab of the 3D view's sidebar: the groom, presets, the graph's nodes and params."""

import json
import re

import bpy

from . import graph, groom, process, shading, units
from .ops import active_groom


def _words(name):
    return re.sub(r"(?<=[a-z0-9])(?=[A-Z])", " ", name).replace("_", " ").capitalize()


def _wrap(layout, text, icon="NONE", width=48):
    words, line, first = text.split(), "", True
    for w in words:
        if line and len(line) + len(w) + 1 > width:
            layout.label(text=line, icon=icon if first else "BLANK1")
            line, first = w, False
        else:
            line = (line + " " + w).strip()
    if line:
        layout.label(text=line, icon=icon if first else "BLANK1")


class GroomPanel(object):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "GroomLab"

    @classmethod
    def poll(cls, context):
        return active_groom(context) is not None


class VIEW3D_PT_groomlab(bpy.types.Panel):
    bl_space_type = "VIEW_3D"
    bl_region_type = "UI"
    bl_category = "GroomLab"
    bl_label = "GroomLab"
    bl_order = 1  # under Groom Tools (palette.py)

    def draw(self, context):
        layout = self.layout
        scene = context.scene
        layout.prop(scene.groomlab, "groom")
        obj = context.active_object
        if obj is not None and obj.type == "MESH" and obj.groomlab.role == "NONE":
            layout.operator("groomlab.add_groom", icon="CURVES")
        scalp = active_groom(context)
        if scalp is None:
            layout.label(text="Select a mesh (the scalp) and add a groom")
            layout.operator("groomlab.load_groom", icon="FILE_FOLDER")
            return
        g = scalp.groomlab
        rt = groom.runtime_if_any(scalp)
        row = layout.row(align=True)
        if g.guides is not None:  # B2's guides as Blender curves (GroomLab's tools edit the others: Groom Tools)
            row.operator("groomlab.sculpt_guides", icon="SCULPTMODE_HLT")
        row.operator("groomlab.update", text="" if g.guides is not None else "Update", icon="FILE_REFRESH")
        row = layout.row(align=True)
        row.prop(g, "preview_density", slider=True)
        row.prop(g, "preview_point_budget", text="Budget (k)")
        layout.prop(g, "follow_scalp")
        if rt is not None and rt.last:
            mode = "render" if rt.last.get("mode") == "render" else "preview"
            layout.label(text="%s strands, %s points (%s, %.0f ms)" % (
                "{:,}".format(rt.last.get("strandCount") or 0), "{:,}".format(rt.last.get("pointCount") or 0), mode,
                rt.last.get("milliseconds") or 0.0), icon="INFO")
        elif not process.running():
            layout.label(text="The helper starts on the first change", icon="INFO")
        if rt is not None:
            box = None
            for text in [rt.error, rt.scalp_note] + rt.errors + rt.messages:
                if text:
                    box = box or layout.box()
                    _wrap(box, text, icon="ERROR")
        for text in shading.render_notes(context.scene, scalp):
            _wrap(layout.box(), text, icon="INFO")
        unsupported = sorted({n.unsupported for n in g.nodes if n.unsupported})
        for text in unsupported:
            _wrap(layout.box(), text, icon="INFO")
        col = layout.column(align=True)
        row = col.row(align=True)
        row.operator("groomlab.save_groom", icon="FILE_TICK")
        row.operator("groomlab.load_groom", icon="FILE_FOLDER")
        row = col.row(align=True)
        row.operator("groomlab.export_alembic", text="Alembic", icon="EXPORT")
        row.operator("groomlab.export_usd", text="USD", icon="EXPORT")
        layout.operator("groomlab.remove_groom", icon="TRASH")


class VIEW3D_PT_groomlab_presets(GroomPanel, bpy.types.Panel):
    bl_label = "Presets"
    bl_parent_id = "VIEW3D_PT_groomlab"
    bl_options = {"DEFAULT_CLOSED"}

    def draw(self, context):
        layout = self.layout
        sg = context.scene.groomlab
        if not graph.cached_presets():
            layout.operator("groomlab.refresh_presets", icon="FILE_REFRESH", text="List Presets")
            return
        row = layout.row(align=True)
        row.prop(sg, "preset_category", text="")
        row.operator("groomlab.refresh_presets", text="", icon="FILE_REFRESH")
        layout.prop(sg, "preset", text="")
        for p in graph.cached_presets():
            if p[0] == sg.preset and p[2]:
                _wrap(layout.column(), p[2])
        row = layout.row(align=True)
        row.operator("groomlab.apply_preset", text="Apply")
        row.operator("groomlab.insert_preset", text="Insert After Node")


class GROOMLAB_UL_nodes(bpy.types.UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index=0):
        row = layout.row(align=True)
        icon = "ERROR" if item.error else "CANCEL" if item.unsupported else "INFO" if item.note else \
            "NODE" if item.in_chain else "DOT"
        row.label(text=item.name, icon=icon)
        sub = row.row()
        sub.alignment = "RIGHT"
        sub.label(text=item.type + (" (out)" if item.is_output else ""))


class VIEW3D_PT_groomlab_graph(GroomPanel, bpy.types.Panel):
    bl_label = "Graph"
    bl_parent_id = "VIEW3D_PT_groomlab"

    def draw(self, context):
        layout = self.layout
        scalp = active_groom(context)
        g = scalp.groomlab
        layout.operator("groomlab.graph_open", icon="NODETREE")  # the full editor: a node editor
        row = layout.row()
        row.template_list("GROOMLAB_UL_nodes", "", g, "nodes", g, "node_index", rows=6)
        col = row.column(align=True)
        col.operator("groomlab.node_add", text="", icon="ADD")
        col.operator("groomlab.node_remove", text="", icon="REMOVE")
        col.separator()
        col.operator("groomlab.node_move", text="", icon="TRIA_UP").direction = -1
        col.operator("groomlab.node_move", text="", icon="TRIA_DOWN").direction = 1
        layout.prop(context.scene.groomlab, "add_type", text="New Node")
        node = graph.active_node(scalp)
        if node is None or g.params_node != node.name:
            return
        box = layout.box()
        box.label(text="%s (%s)" % (node.name, node.type), icon="NODE")
        if g.node_type_description:
            _wrap(box, g.node_type_description)
        for text, icon in ((node.error, "ERROR"), (node.unsupported, "CANCEL"), (node.note, "INFO")):
            if text:
                _wrap(box, text, icon=icon)
        draw_params(box, context, g)


def draw_params(layout, context, g):
    scene = context.scene
    by_name = {p.name: p for p in g.params}
    shown = [p for p in g.params if graph.visible(p, by_name)]
    main = [p for p in shown if not p.group and not p.advanced]
    for p in main:
        draw_param(layout, p, scene)
    groups = []
    for p in shown:
        if p.group and not p.advanced and p.group not in groups:
            groups.append(p.group)
    open_groups = set(x for x in g.open_groups.split("|") if x)
    for name in groups:
        expanded = name in open_groups
        row = layout.row()
        op = row.operator("groomlab.toggle_group", text=name, icon="TRIA_DOWN" if expanded else "TRIA_RIGHT", emboss=False)
        op.group = name
        if expanded:
            col = layout.column()
            for p in shown:
                if p.group == name and not p.advanced:
                    draw_param(col, p, scene)
    advanced = [p for p in shown if p.advanced]
    if advanced:
        layout.prop(g, "show_advanced", icon="TRIA_DOWN" if g.show_advanced else "TRIA_RIGHT", emboss=False)
        if g.show_advanced:
            col = layout.column()
            for p in advanced:
                draw_param(col, p, scene)


def draw_param(layout, p, scene):
    label = _words(p.name) + units.suffix(p.unit, scene)
    field = {"bool": "value_bool", "int": "value_int", "float": "value_float", "length": "value_length",
             "factor": "value_factor", "color": "value_color", "vector": "value_vector", "enum": "value_enum",
             "string": "value_string"}.get(p.widget)
    if field is None:
        row = layout.row()
        row.label(text="%s: %s" % (_words(p.name), p.note))
        return
    if p.widget == "vector":
        layout.label(text=label)
        layout.row(align=True).prop(p, field, text="")
        return
    layout.prop(p, field, text=label)


classes = (VIEW3D_PT_groomlab, VIEW3D_PT_groomlab_presets, GROOMLAB_UL_nodes, VIEW3D_PT_groomlab_graph)


def register():
    for c in classes:
        bpy.utils.register_class(c)


def unregister():
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
