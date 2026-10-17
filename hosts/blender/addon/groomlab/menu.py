# SPDX-License-Identifier: GPL-3.0-or-later
"""The GroomLab menu in Blender's top bar, Maya's GroomLab menu (hosts/maya/scripts/groomlab/ui/
menu.py) in its order with what Blender has: Groom Tools, Graph Editor, Create Groom from
Selection, Groom Files, Paint, Presets, Export, Keyboard Shortcuts..., Open Download Page (Maya's
Check for Updates...: the add-on has no update check of its own), Help, Support GroomLab."""

import bpy
from bpy.props import EnumProperty

from . import icons, process, tool_ops
from .ui import _wrap

DOCS_URL = "https://getgroomlab.com/docs/blender"
UPDATES_URL = "https://getgroomlab.com/download"  # the Download page (Blender's own updates: the remote repository)
DONATE_URL = "https://ko-fi.com/getgroomlab"  # Support GroomLab: free for any use, funded by donations
REPO_URL = "https://github.com/thekesslereffect/groomlab"  # the source: open source (Apache-2.0; this add-on GPL)
ABOUT = ("Hair, fur and feather grooming in Blender: the Groom Tools for the guides, the Graph Editor for the "
         "look, Cycles and EEVEE renders, and a .groom file Maya and the GroomLab app open as it is.")


def version():
    h = process.hello() if process.running() else {}
    if h.get("version"):
        return h["version"]
    try:
        import addon_utils
        for mod in addon_utils.modules():
            if mod.__name__ == __package__:
                info = addon_utils.module_bl_info(mod)
                return ".".join(str(v) for v in info.get("version", ()))
    except Exception:
        pass
    return ""


class GROOMLAB_OT_groom_tools(bpy.types.Operator):
    """Brushes, guide groups and painted maps: the 3D view sidebar's GroomLab tab"""
    bl_idname = "groomlab.groom_tools"
    bl_label = "Groom Tools"

    tab: EnumProperty(items=[("TOOLS", "Tools", ""), ("PAINT", "Paint", "")], default="TOOLS", options={"SKIP_SAVE"})

    def execute(self, context):
        from . import props
        with props.quiet():
            context.scene.groomlab_tools.tab = self.tab
        wm = context.window_manager
        shown = 0
        for window in wm.windows:
            for area in window.screen.areas:
                if area.type == "VIEW_3D":
                    area.spaces.active.show_region_ui = True
                    area.tag_redraw()
                    shown += 1
        if not shown and not bpy.app.background:
            self.report({"INFO"}, "Open a 3D Viewport: Groom Tools are in its sidebar (N), GroomLab tab")
        return {"FINISHED"}


class _TextPopup(bpy.types.Operator):
    lines = ()
    width = 460

    def invoke(self, context, event):
        return context.window_manager.invoke_popup(self, width=self.width)

    def execute(self, context):
        return {"FINISHED"}


class GROOMLAB_OT_shortcuts(_TextPopup):
    """The tool's keys and the Graph Editor's: change the pie menu's key in Preferences > Add-ons > GroomLab"""
    bl_idname = "groomlab.shortcuts"
    bl_label = "GroomLab: Keyboard Shortcuts"

    def draw(self, context):
        layout = self.layout
        layout.label(text="GroomLab: Keyboard Shortcuts")
        col = layout.column(align=True)
        for keys, what in tool_ops.shortcuts():
            row = col.row()
            split = row.split(factor=0.38)
            split.label(text=tool_ops.pie_key() if keys == "Y" else keys)
            split.label(text=what)
        layout.separator()
        layout.label(text="The pie menu's key: Preferences > Add-ons > GroomLab", icon="PREFERENCES")


class GROOMLAB_OT_about(_TextPopup):
    """About GroomLab"""
    bl_idname = "groomlab.about"
    bl_label = "About GroomLab"

    def draw(self, context):
        layout = self.layout
        row = layout.row()
        row.label(text="GroomLab %s" % (version() or ""), icon_value=icons.icon("logo"))
        _wrap(layout, ABOUT, width=64)
        _wrap(layout, "Open source (Apache-2.0); this add-on is GPL-3.0-or-later. Free for any use, commercial "
                      "included.", width=64)
        layout.operator("wm.url_open", text="Source on GitHub", icon="URL").url = REPO_URL
        layout.operator("wm.url_open", text="Support GroomLab", icon="FUND").url = DONATE_URL


class GROOMLAB_MT_files(bpy.types.Menu):
    """Grooms to and from .groom files: the round trip with Maya and the GroomLab app"""
    bl_label = "Groom Files"

    def draw(self, context):
        layout = self.layout
        layout.operator("groomlab.load_groom", text="Open Groom File...", icon="FILE_FOLDER")
        layout.operator("groomlab.save_groom", text="Save Groom File...", icon="FILE_TICK")


class GROOMLAB_MT_export(bpy.types.Menu):
    bl_label = "Export"

    def draw(self, context):
        layout = self.layout
        layout.operator("groomlab.export_alembic", text="Alembic...")
        layout.operator("groomlab.export_usd", text="USD...")
        layout.operator("groomlab.save_groom", text="Groom File (.groom)...")


class GROOMLAB_MT_help(bpy.types.Menu):
    bl_label = "Help"

    def draw(self, context):
        layout = self.layout
        layout.operator("wm.url_open", text="Docs", icon="HELP").url = DOCS_URL
        layout.operator("groomlab.shortcuts", text="Keyboard Shortcuts...")
        layout.operator("groomlab.about", text="About GroomLab")


class GROOMLAB_MT_main(bpy.types.Menu):
    """GroomLab: grooming tools, the node graph, grooms and exports"""
    bl_label = "GroomLab"

    def draw(self, context):
        layout = self.layout
        layout.operator("groomlab.groom_tools", text="Groom Tools", icon_value=icons.icon("palette"))
        layout.operator("groomlab.graph_open", text="Graph Editor", icon_value=icons.icon("graph"))
        layout.separator()
        layout.operator("groomlab.add_groom", text="Create Groom from Selection")
        layout.menu("GROOMLAB_MT_files")
        op = layout.operator("groomlab.groom_tools", text="Paint", icon_value=icons.icon("paint"))
        op.tab = "PAINT"
        layout.menu("GROOMLAB_MT_graph_presets", text="Presets")
        layout.menu("GROOMLAB_MT_export")
        layout.separator()
        layout.operator("groomlab.shortcuts", text="Keyboard Shortcuts...")
        layout.operator("wm.url_open", text="Open Download Page", icon="URL").url = UPDATES_URL
        layout.menu("GROOMLAB_MT_help")
        layout.operator("wm.url_open", text="Support GroomLab", icon="FUND").url = DONATE_URL


def draw_topbar(self, context):
    self.layout.menu("GROOMLAB_MT_main")


classes = (GROOMLAB_OT_groom_tools, GROOMLAB_OT_shortcuts, GROOMLAB_OT_about,
           GROOMLAB_MT_files, GROOMLAB_MT_export, GROOMLAB_MT_help, GROOMLAB_MT_main)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.TOPBAR_MT_editor_menus.append(draw_topbar)


def unregister():
    bpy.types.TOPBAR_MT_editor_menus.remove(draw_topbar)
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
