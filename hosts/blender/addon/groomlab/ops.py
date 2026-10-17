# SPDX-License-Identifier: GPL-3.0-or-later
"""The add-on's operators: make a groom on a mesh, sculpt its guides, presets, graph edits, .groom
files and Alembic/USD exports."""

import bpy
from bpy.props import BoolProperty, EnumProperty, FloatProperty, IntProperty, StringProperty
from bpy_extras.io_utils import ExportHelper, ImportHelper

from . import graph, graph_tree, groom
from .helper_client import HelperError, sentence


def active_groom(context):
    """The groom the panel works on: the scene's choice, else the active object's groom."""
    chosen = context.scene.groomlab.groom
    if chosen is not None and groom.is_groom(chosen):
        return chosen
    return groom.scalp_of(context.active_object)


def _fail(op, e):
    op.report({"ERROR"}, sentence(e.message if isinstance(e, HelperError) else str(e)))
    return {"CANCELLED"}


class GroomOperator(bpy.types.Operator):
    @classmethod
    def poll(cls, context):
        return active_groom(context) is not None


def _preset_items(self, context):
    items = [(p[0], p[1], p[2]) for p in graph.cached_presets()]
    return items or [("hair_basics", "Hair Basics", "")]


class GROOMLAB_OT_add_groom(bpy.types.Operator):
    """Grow a GroomLab groom on the selected mesh: place and shape its guides with the Groom Tools, GroomLab makes the hair from them"""
    bl_idname = "groomlab.add_groom"
    bl_label = "Create Groom from Selection"
    bl_options = {"REGISTER", "UNDO"}

    preset: EnumProperty(name="Preset", items=_preset_items, description="The look to start from")
    guides: IntProperty(name="Starter Guides", default=0, min=0, max=5000,
                        description="Guides spread over the scalp to start from (0: place them with Place and Draw, as in Maya)")
    guide_points: IntProperty(name="Points per Guide", default=8, min=2, max=64)

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == "MESH" and obj.groomlab.role == "NONE"

    def invoke(self, context, event):
        try:
            graph.presets()
        except HelperError as e:
            return _fail(self, e)
        return context.window_manager.invoke_props_dialog(self)

    def execute(self, context):
        scalp = context.active_object
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        try:
            groom.create_groom(context, scalp, preset=self.preset, guide_count=self.guides, guide_points=self.guide_points)
            context.scene.groomlab.groom = scalp
            graph.refresh(scalp)
        except HelperError as e:
            return _fail(self, e)
        from . import tools
        try:
            tools.refresh_info(scalp, context.scene)
        except HelperError:
            pass
        self.report({"INFO"}, "New groom created: place its guides with Groom Tools > Place")
        return {"FINISHED"}


class GROOMLAB_OT_remove_groom(GroomOperator):
    """Remove the groom: its guides and hair objects are deleted, the mesh stays"""
    bl_idname = "groomlab.remove_groom"
    bl_label = "Remove Groom"
    bl_options = {"REGISTER", "UNDO"}

    def invoke(self, context, event):
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        scalp = active_groom(context)
        bpy.ops.groomlab.tool_exit()
        groom.close(scalp)
        graph_tree.remove_tree(scalp)
        scalp.groomlab_tools.state = ""
        g = scalp.groomlab
        g.view_node = ""
        for obj in (g.guides, g.hair):
            if obj is not None:
                data = obj.data
                bpy.data.objects.remove(obj)
                if data is not None and data.users == 0:
                    bpy.data.hair_curves.remove(data)
        g.role = "NONE"
        g.uid = ""
        g.snapshot = ""
        g.graph_json = ""
        g.nodes.clear()
        g.params.clear()
        groom.forget_cache()
        if context.scene.groomlab.groom == scalp:
            context.scene.groomlab.groom = None
        return {"FINISHED"}


class GROOMLAB_OT_sculpt_guides(GroomOperator):
    """Sculpt the guides with Blender's Curves Sculpt brushes (Comb, Add, Delete, Grow/Shrink, Snake Hook, Puff, Smooth...)"""
    bl_idname = "groomlab.sculpt_guides"
    bl_label = "Sculpt Guides"

    @classmethod
    def poll(cls, context):
        s = active_groom(context)
        return s is not None and s.groomlab.guides is not None  # B2's guides as Blender curves

    def execute(self, context):
        guides = active_groom(context).groomlab.guides
        if guides is None:
            self.report({"ERROR"}, "This groom has no Blender curve guides: edit its guides with the Groom Tools")
            return {"CANCELLED"}
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        guides.hide_set(False)
        for o in context.selected_objects:
            o.select_set(False)
        guides.select_set(True)
        context.view_layer.objects.active = guides
        bpy.ops.object.mode_set(mode="SCULPT_CURVES")
        return {"FINISHED"}


class GROOMLAB_OT_update(GroomOperator):
    """Send the scalp and guides again and evaluate (also restarts the helper if it stopped)"""
    bl_idname = "groomlab.update"
    bl_label = "Update"

    def execute(self, context):
        scalp = active_groom(context)
        try:
            groom.sync(scalp, scene=context.scene, force_eval=True, force_inputs=True)
            graph.refresh(scalp)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_refresh_presets(bpy.types.Operator):
    """List the shipped and user presets again"""
    bl_idname = "groomlab.refresh_presets"
    bl_label = "Refresh Presets"

    def execute(self, context):
        try:
            graph.presets(refresh=True)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_apply_preset(GroomOperator):
    """Replace the groom's graph with the preset's, fitted to this scalp (the guides stay)"""
    bl_idname = "groomlab.apply_preset"
    bl_label = "Apply Preset"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        scalp = active_groom(context)
        try:
            r = graph.apply_preset(scalp, context.scene.groomlab.preset)
        except HelperError as e:
            return _fail(self, e)
        missing = [m["name"] for m in r.get("missingMaps", [])]
        if missing:
            self.report({"WARNING"}, "The preset reads maps this groom has not painted: %s. Paint them in Groom Tools > Paint" % ", ".join(missing))
        return {"FINISHED"}


class GROOMLAB_OT_insert_preset(GroomOperator):
    """Add the preset's look after the selected node, as one compound node"""
    bl_idname = "groomlab.insert_preset"
    bl_label = "Insert Preset"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        scalp = active_groom(context)
        node = graph.active_node(scalp)
        try:
            graph.insert_preset(scalp, context.scene.groomlab.preset, after=node.name if node and not node.is_output else None)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_node_add(GroomOperator):
    """Add a node after the selected one on the strands' chain"""
    bl_idname = "groomlab.node_add"
    bl_label = "Add Node"
    bl_options = {"REGISTER", "UNDO"}

    type: StringProperty(name="Type")

    def invoke(self, context, event):
        try:
            graph.operator_specs()
        except HelperError as e:
            return _fail(self, e)
        return self.execute(context)

    def execute(self, context):
        scalp = active_groom(context)
        node = graph.active_node(scalp)
        kind = self.type or context.scene.groomlab.add_type
        try:
            made, error = graph.add_node(scalp, kind, after=node.name if node else None)
        except HelperError as e:
            return _fail(self, e)
        if error:
            self.report({"WARNING"}, sentence(error))
        return {"FINISHED"}


class GROOMLAB_OT_node_remove(GroomOperator):
    """Remove the selected node (the chain closes over it)"""
    bl_idname = "groomlab.node_remove"
    bl_label = "Remove Node"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        scalp = active_groom(context)
        node = graph.active_node(scalp)
        if node is None:
            return {"CANCELLED"}
        try:
            error = graph.remove_node(scalp, node.name)
        except HelperError as e:
            return _fail(self, e)
        if error:
            self.report({"WARNING"}, sentence(error))
        return {"FINISHED"}


class GROOMLAB_OT_node_move(GroomOperator):
    """Move the selected node along the strands' chain"""
    bl_idname = "groomlab.node_move"
    bl_label = "Move Node"
    bl_options = {"REGISTER", "UNDO"}

    direction: IntProperty(default=-1)

    def execute(self, context):
        scalp = active_groom(context)
        node = graph.active_node(scalp)
        try:
            if node is None or not graph.move_node(scalp, node.name, self.direction):
                self.report({"INFO"}, "This node does not move that way: the source and the output stay put")
                return {"CANCELLED"}
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_toggle_group(GroomOperator):
    """Show or fold this group of params"""
    bl_idname = "groomlab.toggle_group"
    bl_label = "Toggle Group"

    group: StringProperty()

    def execute(self, context):
        g = active_groom(context).groomlab
        groups = [x for x in g.open_groups.split("|") if x]
        if self.group in groups:
            groups.remove(self.group)
        else:
            groups.append(self.group)
        g.open_groups = "|".join(groups)
        return {"FINISHED"}


class GROOMLAB_OT_save_groom(GroomOperator, ExportHelper):
    """Save the groom to a groom file (.groom; Y up, centimeters: Maya and the GroomLab app open it as it is)"""
    bl_idname = "groomlab.save_groom"
    bl_label = "Save Groom File"
    filename_ext = ".groom"
    filter_glob: StringProperty(default="*.groom", options={"HIDDEN"})
    up_axis: EnumProperty(name="Up Axis", items=[("Y", "Y (Maya)", "GroomLab's own space"), ("Z", "Z", "Z up (Blender, Houdini pipelines)")],
                          default="Y")

    def execute(self, context):
        try:
            r = groom.save_groom(active_groom(context), self.filepath, up_axis=self.up_axis)
        except HelperError as e:
            return _fail(self, e)
        self.report({"INFO"}, "Saved %s" % r["path"])
        return {"FINISHED"}


class GROOMLAB_OT_load_groom(bpy.types.Operator, ImportHelper):
    """Open a groom file (.groom, from Maya, the GroomLab app or Blender) on the selected mesh, or with the file's own scalp"""
    bl_idname = "groomlab.load_groom"
    bl_label = "Open Groom File"
    bl_options = {"REGISTER", "UNDO"}
    filename_ext = ".groom"
    filter_glob: StringProperty(default="*.groom", options={"HIDDEN"})
    use_file_scalp: BoolProperty(name="Use the File's Scalp", default=False,
                                 description="Make a mesh of the scalp stored in the file instead of growing on the selected mesh")

    def invoke(self, context, event):
        obj = groom.scalp_of(context.active_object) or context.active_object
        self.use_file_scalp = obj is None or obj.type != "MESH"
        return ImportHelper.invoke(self, context, event)

    def execute(self, context):
        target = None
        if not self.use_file_scalp:
            target = groom.scalp_of(context.active_object) or context.active_object
            if target is None or target.type != "MESH":
                self.report({"ERROR"}, "No mesh selected: select the mesh to grow the groom on, or turn on Use the File's Scalp")
                return {"CANCELLED"}
        if context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")
        try:
            scalp, info, _ = groom.load_groom(context, self.filepath, target)
            context.scene.groomlab.groom = scalp
            graph_tree.mark_replaced(scalp)  # the file's layout in the Graph Editor
            graph.refresh(scalp)
        except HelperError as e:
            return _fail(self, e)
        notes = list(info.get("notes") or []) + list(info.get("inputErrors") or [])
        for n in notes:
            self.report({"WARNING"}, sentence(n))
        return {"FINISHED"}


class _Export(GroomOperator, ExportHelper):
    frames: EnumProperty(name="Frames", items=[("CURRENT", "Current Frame", ""), ("SCENE", "Scene Range", "")], default="CURRENT")
    density: FloatProperty(name="Density", default=1.0, min=0.0, max=1.0, subtype="FACTOR",
                           description="Share of the strands written (1: all, as rendered)")
    fmt = ""

    def execute(self, context):
        scene = context.scene
        first = last = scene.frame_current
        if self.frames == "SCENE":
            first, last = scene.frame_start, scene.frame_end
        try:
            r = groom.export(active_groom(context), scene, self.fmt, self.filepath, first, last, density=self.density)
        except HelperError as e:
            return _fail(self, e)
        self.report({"INFO"}, "Exported %s" % r.get("path", self.filepath))
        return {"FINISHED"}


class GROOMLAB_OT_export_alembic(_Export):
    """Export the groom's strands as Alembic curves (Y up, centimeters; Blender's importer converts)"""
    bl_idname = "groomlab.export_alembic"
    bl_label = "Export Alembic"
    filename_ext = ".abc"
    filter_glob: StringProperty(default="*.abc", options={"HIDDEN"})
    fmt = "alembic"


class GROOMLAB_OT_export_usd(_Export):
    """Export the groom's strands as USD basis curves (upAxis Y, metersPerUnit 0.01)"""
    bl_idname = "groomlab.export_usd"
    bl_label = "Export USD"
    filename_ext = ".usda"
    filter_glob: StringProperty(default="*.usd;*.usda", options={"HIDDEN"})
    fmt = "usd"


classes = (GROOMLAB_OT_add_groom, GROOMLAB_OT_remove_groom, GROOMLAB_OT_sculpt_guides, GROOMLAB_OT_update,
           GROOMLAB_OT_refresh_presets, GROOMLAB_OT_apply_preset, GROOMLAB_OT_insert_preset, GROOMLAB_OT_node_add,
           GROOMLAB_OT_node_remove, GROOMLAB_OT_node_move, GROOMLAB_OT_toggle_group, GROOMLAB_OT_save_groom,
           GROOMLAB_OT_load_groom, GROOMLAB_OT_export_alembic, GROOMLAB_OT_export_usd)


GROOM_FILE_TEXT = "GroomLab Groom File (.groom)"  # File > Import and File > Export, named as Blender names formats


def menu_add(self, context):
    self.layout.operator(GROOMLAB_OT_add_groom.bl_idname, icon="CURVES")


def menu_import(self, context):
    self.layout.operator(GROOMLAB_OT_load_groom.bl_idname, text=GROOM_FILE_TEXT)


def menu_export(self, context):
    self.layout.operator(GROOMLAB_OT_save_groom.bl_idname, text=GROOM_FILE_TEXT)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.VIEW3D_MT_object.append(menu_add)
    bpy.types.TOPBAR_MT_file_import.append(menu_import)
    bpy.types.TOPBAR_MT_file_export.append(menu_export)


def unregister():
    bpy.types.TOPBAR_MT_file_export.remove(menu_export)
    bpy.types.TOPBAR_MT_file_import.remove(menu_import)
    bpy.types.VIEW3D_MT_object.remove(menu_add)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
