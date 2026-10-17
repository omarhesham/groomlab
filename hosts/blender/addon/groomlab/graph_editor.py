# SPDX-License-Identifier: GPL-3.0-or-later
"""The GroomLab Graph Editor in Blender's node editor, after Maya's GroomLab Graph Editor: the
groom's graph as nodes with typed, colored ports (graph_tree.py keeps it in step with the helper),
the selected node's params in the sidebar's Node tab (collapsible groups, Advanced folded,
showWhen, units, Upgrade, notes and errors), presets (apply, or insert as a Compound), View (a
node's strands in the viewport instead of the output's), and Maya's keys: Tab adds, F frames, V
views, B bypasses, Ctrl+D duplicates, Delete deletes."""

import json
import re

import bpy
from bpy.props import BoolProperty, EnumProperty, StringProperty

from . import graph, graph_tree, groom, process, units
from .graph_tree import TREE, ours
from .helper_client import HelperError, sentence

_folded = {}       # (operator type, section) -> folded, for the session (Maya: unset = Advanced folded)
_deferred = {}     # key -> (fn, args): run once from a timer (not while Blender draws)
_status = {}       # scalp uid -> {"serial", "times"}: the node timings the overlay shows
_channels = {}     # scalp uid -> {"graph", "channels"}: the attributes per node (graph_channels)
_catalog = {"presets": [], "categories": []}


# ---- Helpers ------------------------------------------------------------------------------------

def defer(key, fn, *args):
    """Runs fn(*args) once soon, outside drawing (a panel may not change data while it draws)."""
    if key in _deferred:
        return
    _deferred[key] = (fn, args)
    if bpy.app.background:
        return
    if not bpy.app.timers.is_registered(_run_deferred):
        bpy.app.timers.register(_run_deferred, first_interval=0.0)


def _run_deferred():
    jobs = list(_deferred.values())
    _deferred.clear()
    for fn, args in jobs:
        try:
            fn(*args)
        except HelperError as e:
            print("GroomLab: %s" % e)
        except Exception as e:  # noqa: BLE001 - a timer must not raise
            print("GroomLab: %r" % e)
    graph_tree._redraw()
    return None


def editor_tree(context):
    """The GroomLab graph the node editor shows (None elsewhere)."""
    space = getattr(context, "space_data", None)
    if space is None or space.type != "NODE_EDITOR" or space.tree_type != TREE:
        return None
    tree = space.edit_tree
    return tree if graph_tree.is_graph_tree(tree) else None


def context_groom(context):
    """The groom an operator works on: the node editor's, else the scene's (the panels')."""
    tree = editor_tree(context)
    if tree is not None and groom.is_groom(tree.gl_scalp):
        return tree.gl_scalp
    from .ops import active_groom
    return active_groom(context)


def context_tree(context):
    tree = editor_tree(context)
    if tree is not None:
        return tree
    return graph_tree.tree_of(context_groom(context))


def active_node(context):
    tree = context_tree(context)
    node = tree.nodes.active if tree is not None else None
    return node if node is not None and ours(node) and node.gl_name else None


def selected_nodes(context):
    tree = context_tree(context)
    return [n for n in tree.nodes if ours(n) and n.select and n.gl_name] if tree is not None else []


def wrap(layout, text, icon="NONE", width=44):
    words, line, first = text.split(), "", True
    for w in words:
        if line and len(line) + len(w) + 1 > width:
            layout.label(text=line, icon=icon if first else "BLANK1")
            line, first = w, False
        else:
            line = (line + " " + w).strip()
    if line:
        layout.label(text=line, icon=icon if first else "BLANK1")


def node_errors(scalp):
    """{node: error} from the last evaluation (Maya's: "node 'x': ...")."""
    rt = groom.runtime_if_any(scalp)
    out = {}
    for e in (rt.errors if rt is not None else []):
        m = re.match(r"node '([^']+)': (.*)", e, re.S)
        if m:
            out.setdefault(m.group(1), m.group(2))
    return out


def node_channels(scalp):
    """The attributes per node as the graph is set (the helper's graph_channels): {node: {"incoming",
    "writes", "overwrites", "reads"}}, fetched again (outside drawing) when the graph changed."""
    rt = groom.runtime_if_any(scalp)
    if rt is None or rt.session is None:
        return {}
    uid = scalp.groomlab.uid
    text = scalp.groomlab.graph_json
    cached = _channels.get(uid)
    if cached is None or cached["graph"] != text:
        def fetch():
            if rt.session is None or rt.generation != process.generation():
                return
            _channels[uid] = {"graph": text, "channels": process.call("graph_channels", session=rt.session).get("channels", {})}
        defer(("channels", uid), fetch)
        return cached["channels"] if cached else {}
    return cached["channels"]


def attribute_notes(scalp):
    """{node: note} for each node reading an attribute no node upstream writes (not an optional default)."""
    notes = {}
    for name, info in node_channels(scalp).items():
        missing = [r for r in info.get("reads", []) if not r.get("reaches") and not r.get("optional")]
        if missing:
            notes[name] = "; ".join("reads the attribute %s (%s), which no node upstream writes: it is ignored"
                                    % (r["name"], r["param"]) for r in missing)
    return notes


def node_notes(scalp):
    rt = groom.runtime_if_any(scalp)
    notes = dict(rt.notes) if rt is not None else {}
    for name, note in attribute_notes(scalp).items():
        notes[name] = (notes[name] + "; " + note) if notes.get(name) else note
    tree = graph_tree.tree_of(scalp)
    if tree is not None:
        for n in tree.nodes:
            if ours(n) and n.gl_type in groom.UNSUPPORTED:
                notes[n.gl_name] = (notes.get(n.gl_name, "") + " " + groom.UNSUPPORTED[n.gl_type]).strip()
    return notes


def show_groom(context, scalp):
    """The scene's groom changed: the node editors showing a GroomLab graph show this groom's."""
    if not groom.is_groom(scalp):
        return
    wm = getattr(context, "window_manager", None) or bpy.context.window_manager
    spaces = []
    for window in (wm.windows if wm is not None else []):
        for area in window.screen.areas:
            if area.type == "NODE_EDITOR":
                space = area.spaces.active
                if space.tree_type == TREE:
                    spaces.append((area, space))
    if not spaces:
        return
    tree = graph_tree.tree_of(scalp)
    if tree is None or not len(tree.nodes):
        try:
            tree = graph_tree.rebuild(scalp)
        except HelperError as e:
            print("GroomLab: %s" % e)
            return
    for area, space in spaces:
        if space.node_tree != tree:
            space.node_tree = tree
        area.tag_redraw()


def _follow_editor(tree):
    """The node editor shows another groom's graph (picked in its tree list): the scene follows."""
    scalp = tree.gl_scalp
    scene = bpy.context.scene
    if groom.is_groom(scalp) and scene.groomlab.groom != scalp:
        from . import props
        with props.quiet():
            scene.groomlab.groom = scalp


def _refresh_stale(tree):
    """A tree from a .blend just opened (or the helper restarted): made again from the groom."""
    scalp = tree.gl_scalp
    if groom.is_groom(scalp):
        graph_tree.rebuild(scalp)


def check_tree(tree):
    """While drawing: the tree is current, else it is rebuilt soon (a file just opened, nodes of
    types not registered yet, another helper)."""
    if tree is None or not groom.is_groom(tree.gl_scalp):
        return False
    stale = tree.get("gl_generation") != process.generation() or any(
        n.bl_idname == "NodeUndefined" for n in tree.nodes)
    if stale:
        defer(("stale", tree.name), _refresh_stale, tree)
    return not stale


# ---- Operators ----------------------------------------------------------------------------------

class _GraphOperator(bpy.types.Operator):
    @classmethod
    def poll(cls, context):
        return context_groom(context) is not None


class _EditorOperator(bpy.types.Operator):
    """Keys of the GroomLab Graph Editor: only there (elsewhere Blender's own keys act)."""

    @classmethod
    def poll(cls, context):
        return editor_tree(context) is not None


def _fail(op, e):
    op.report({"ERROR"}, sentence(e.message if isinstance(e, HelperError) else str(e)))
    return {"CANCELLED"}


class GROOMLAB_OT_graph_open(_GraphOperator):
    """Open the groom's graph in the GroomLab Graph Editor (a node editor)"""
    bl_idname = "groomlab.graph_open"
    bl_label = "Graph Editor"

    new_window: BoolProperty(name="New Window", default=True, options={"SKIP_SAVE"})

    def execute(self, context):
        scalp = context_groom(context)
        try:
            tree = graph_tree.rebuild(scalp)
        except HelperError as e:
            return _fail(self, e)
        screen = getattr(context, "screen", None)
        if screen is None or bpy.app.background:
            return {"FINISHED"}  # no interface: the tree is made
        area = next((a for a in screen.areas if a.type == "NODE_EDITOR" and a.spaces.active.tree_type == TREE), None)
        if area is None and self.new_window:
            before = set(context.window_manager.windows[:])
            try:
                bpy.ops.wm.window_new()
            except RuntimeError:
                pass
            window = next((w for w in context.window_manager.windows if w not in before), None)
            if window is not None:
                area = window.screen.areas[0]
        if area is None:
            area = context.area
        if area is None:
            return {"FINISHED"}
        area.type = "NODE_EDITOR"
        area.ui_type = TREE
        space = area.spaces.active
        space.node_tree = tree
        space.show_region_ui = True
        return {"FINISHED"}


class GROOMLAB_OT_graph_add_search(_EditorOperator):
    """Find a node type and add it (Tab, as in Maya's Graph Editor)"""
    bl_idname = "groomlab.graph_add_search"
    bl_label = "Add Node (Search)"

    def invoke(self, context, event):
        bpy.ops.wm.search_single_menu("INVOKE_DEFAULT", menu_idname="NODE_MT_add")
        return {"FINISHED"}


class GROOMLAB_OT_graph_frame(_EditorOperator):
    """Fit the selected nodes in the view, or the whole graph when none are selected (F)"""
    bl_idname = "groomlab.graph_frame"
    bl_label = "Frame"

    def execute(self, context):
        if any(n.select for n in editor_tree(context).nodes):
            return bpy.ops.node.view_selected()
        return bpy.ops.node.view_all()


class GROOMLAB_OT_graph_duplicate(_EditorOperator):
    """Duplicate the selected nodes (Ctrl+D): each a node of its own in the groom, with the same params"""
    bl_idname = "groomlab.graph_duplicate"
    bl_label = "Duplicate"

    def invoke(self, context, event):
        bpy.ops.node.duplicate_move("INVOKE_DEFAULT")
        return {"FINISHED"}


def _strands_output(node):
    return len(node.outputs) and node.outputs[0].bl_idname in ("GroomLabSocketStrands", "GroomLabSocketCurves")


def _viewable(node):
    """Focus shows a node giving strands (a Clump's centers as dots)."""
    return bool(_strands_output(node))


class GROOMLAB_OT_graph_view_node(_GraphOperator):
    """Focus: show the groom up to this node in the viewport (V); renders keep the output. Again: back to the output"""
    bl_idname = "groomlab.graph_view_node"
    bl_label = "Focus: View Up to Here"

    node: StringProperty(options={"SKIP_SAVE"}, description="The node (empty: the active one)")
    clear: BoolProperty(options={"SKIP_SAVE"})

    def execute(self, context):
        scalp = context_groom(context)
        if self.clear:
            graph_tree.view_node(scalp, "")
            return {"FINISHED"}
        name = self.node or (active_node(context).gl_name if active_node(context) else "")
        tree = graph_tree.tree_of(scalp)
        node = next((n for n in tree.nodes if ours(n) and n.gl_name == name), None) if tree else None
        if node is None:
            self.report({"INFO"}, "No node selected: select the node to view, then press V")
            return {"CANCELLED"}
        if not _viewable(node):
            self.report({"INFO"}, "Nothing to view: %s gives no strands. Pick a node that outputs strands or curves" % name)
            return {"CANCELLED"}
        graph_tree.view_node(scalp, "" if scalp.groomlab.view_node == name else name)
        if scalp.groomlab.view_node:
            self.report({"INFO"}, "Viewing %s (renders use the output)" % name)
        return {"FINISHED"}


class GROOMLAB_OT_graph_bypass(_GraphOperator):
    """Bypass: pass the selected nodes' input straight through (B; Blender's M mutes the same way)"""
    bl_idname = "groomlab.graph_bypass"
    bl_label = "Bypass"
    bl_options = {"REGISTER", "UNDO"}

    node: StringProperty(options={"SKIP_SAVE"}, description="The node (empty: the selected ones)")

    def execute(self, context):
        scalp = context_groom(context)
        names = [self.node] if self.node else [n.gl_name for n in selected_nodes(context)]
        if not names:
            return {"CANCELLED"}
        tree = graph_tree.tree_of(scalp)
        bypassed = {n.gl_name for n in tree.nodes if ours(n) and n.mute} if tree else set()
        try:
            graph_tree.set_bypass(scalp, names, not all(n in bypassed for n in names))
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_graph_evaluation(_GraphOperator):
    """When the node runs: always, only in the viewport (renders and exports skip it) or only in
    renders (the viewport skips it)"""
    bl_idname = "groomlab.graph_evaluation"
    bl_label = "Evaluate"
    bl_options = {"REGISTER", "UNDO"}

    node: StringProperty(options={"SKIP_SAVE"}, description="The node (empty: the selected ones)")
    mode: EnumProperty(items=(("always", "Always", "In the viewport and in renders"),
                              ("viewportOnly", "Viewport Only", "Renders, exports and the CLI skip it"),
                              ("renderOnly", "Render Only", "The viewport skips it")),
                       default="always")

    def execute(self, context):
        scalp = context_groom(context)
        names = [self.node] if self.node else [n.gl_name for n in selected_nodes(context)]
        if not names:
            return {"CANCELLED"}
        try:
            graph_tree.set_evaluation(scalp, names, self.mode)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_graph_set_output(_GraphOperator):
    """Make this node the groom's output: what it shows and renders"""
    bl_idname = "groomlab.graph_set_output"
    bl_label = "Set as Output"
    bl_options = {"REGISTER", "UNDO"}

    node: StringProperty(options={"SKIP_SAVE"})

    def execute(self, context):
        name = self.node or (active_node(context).gl_name if active_node(context) else "")
        if not name:
            return {"CANCELLED"}
        try:
            graph_tree.set_output(context_groom(context), name)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_graph_upgrade_node(_GraphOperator):
    """Upgrade: the node evaluates as new nodes of its type do (its result may change)"""
    bl_idname = "groomlab.graph_upgrade_node"
    bl_label = "Upgrade"
    bl_options = {"REGISTER", "UNDO"}

    node: StringProperty(options={"SKIP_SAVE"})

    @classmethod
    def description(cls, context, properties):
        tree = context_tree(context) if context is not None else None
        node = next((n for n in tree.nodes if ours(n) and n.gl_name == properties.node), None) if tree else None
        if node is None:
            return cls.__doc__
        newest = node.gl_spec.get("version", 1)
        return ("This %s keeps the behaviour of version %d, as it was saved; %s nodes are at version %d now. "
                "Upgrade: it evaluates as new %s nodes do (its result may change)"
                % (node.gl_type, node.gl_version, node.gl_type, newest, node.gl_type))

    def execute(self, context):
        name = self.node or (active_node(context).gl_name if active_node(context) else "")
        try:
            done = graph_tree.upgrade_node(context_groom(context), name)
        except HelperError as e:
            return _fail(self, e)
        if not done:
            self.report({"INFO"}, "Already the newest version: %s" % name)
        return {"FINISHED"}


class GROOMLAB_OT_graph_fold(bpy.types.Operator):
    """Fold or open this section of every node of this type (for the session)"""
    bl_idname = "groomlab.graph_fold"
    bl_label = "Fold Section"

    op_type: StringProperty()
    section: StringProperty()

    def execute(self, context):
        _folded[(self.op_type, self.section)] = not is_folded(self.op_type, self.section)
        return {"FINISHED"}


def is_folded(op_type, section):
    return _folded.get((op_type, section), section == graph_tree.ADVANCED)


class GROOMLAB_OT_graph_tidy(_GraphOperator):
    """Lay the graph out again: columns from the sources to the output"""
    bl_idname = "groomlab.graph_tidy"
    bl_label = "Tidy Layout"
    bl_options = {"REGISTER", "UNDO"}

    def execute(self, context):
        try:
            graph_tree.tidy(context_groom(context))
        except HelperError as e:
            return _fail(self, e)
        if editor_tree(context) is not None:
            bpy.ops.node.view_all()
        return {"FINISHED"}


class GROOMLAB_OT_graph_reload(_GraphOperator):
    """Make the editor's graph again from the groom (its layout too)"""
    bl_idname = "groomlab.graph_reload"
    bl_label = "Reload from Groom"

    def execute(self, context):
        scalp = context_groom(context)
        try:
            graph.refresh(scalp)
            graph_tree.rebuild(scalp, replaced=True)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


def preset_catalog(refresh=False):
    """{"presets": [{name, title, description, category}], "categories": [...]} (cached)."""
    if refresh or not _catalog["presets"]:
        r = process.call("list_presets")
        _catalog["presets"] = r.get("presets", [])
        _catalog["categories"] = r.get("categories", [])
        graph.presets(refresh=True)  # the 3D view's browser lists them too
    return _catalog


def preset_info(name):
    return next((p for p in _catalog["presets"] if p.get("name") == name), None)


def preset_categories():
    """The categories in the core's order, then the others; "" (your own presets) last."""
    found = []
    for p in _catalog["presets"]:
        c = p.get("category") or ""
        if c not in found:
            found.append(c)
    order = [c for c in _catalog["categories"] if c in found]
    order += sorted(c for c in found if c and c not in order)
    if "" in found:
        order.append("")
    return order


class GROOMLAB_OT_graph_preset(_GraphOperator):
    """Apply a preset, or insert its look after the selected node as one Compound"""
    bl_idname = "groomlab.graph_preset"
    bl_label = "Preset"
    bl_options = {"REGISTER", "UNDO"}

    name: StringProperty()
    mode: EnumProperty(items=[("APPLY", "Apply", "Replace this groom's graph with the preset, sizes fitted to this scalp"),
                              ("INSERT", "Insert after selected node",
                               "Keep this graph and add the preset's look to it as one Compound after the selected node "
                               "(before the Output when none is selected), fitted to this scalp")])

    @classmethod
    def description(cls, context, properties):
        info = preset_info(properties.name) or {}
        what = info.get("description") or info.get("title") or properties.name
        how = ("Insert after the selected node as one Compound (before the Output when none is selected), fitted to "
               "this scalp" if properties.mode == "INSERT" else
               "Apply: replace this groom's graph with the preset, sizes fitted to this scalp")
        return "%s.\n%s" % (what.rstrip("."), how)

    def execute(self, context):
        scalp = context_groom(context)
        try:
            if self.mode == "APPLY":
                r = graph.apply_preset(scalp, self.name)
                missing = [m["name"] for m in r.get("missingMaps", [])]
                if missing:
                    self.report({"WARNING"}, "The preset reads maps this groom has not painted: %s. Paint them in Groom Tools > Paint" % ", ".join(missing))
            else:
                node = active_node(context)
                after = node.gl_name if node is not None and node.select and not node.gl_is_output and _strands_output(node) else None
                r = graph.insert_preset(scalp, self.name, after=after)
                made = r.get("node")
                tree = graph_tree.tree_of(scalp)
                if made and tree is not None:
                    for n in tree.nodes:
                        n.select = ours(n) and n.gl_name == made
                        if n.select:
                            tree.nodes.active = n
                self.report({"INFO"}, "Inserted %s as %s after %s" % (self.name, made, after or "the last node"))
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


class GROOMLAB_OT_graph_presets_refresh(bpy.types.Operator):
    """List the shipped and user presets again"""
    bl_idname = "groomlab.graph_presets_refresh"
    bl_label = "Refresh Presets"

    def execute(self, context):
        try:
            preset_catalog(refresh=True)
        except HelperError as e:
            return _fail(self, e)
        return {"FINISHED"}


# ---- Menus --------------------------------------------------------------------------------------

class GROOMLAB_MT_graph_preset_actions(bpy.types.Menu):
    bl_label = "Preset"

    def draw(self, context):
        name = getattr(context, "groomlab_preset", "") or ""
        layout = self.layout
        for mode, text in (("APPLY", "Apply"), ("INSERT", "Insert after selected node")):
            op = layout.operator("groomlab.graph_preset", text=text)
            op.name, op.mode = name, mode


class GROOMLAB_MT_graph_preset_category(bpy.types.Menu):
    bl_label = "Presets"

    def draw(self, context):
        category = getattr(context, "groomlab_preset_category", None)
        draw_preset_items(self.layout, category or "")


def draw_preset_items(layout, category):
    presets = sorted((p for p in _catalog["presets"] if (p.get("category") or "") == category),
                     key=lambda p: (p.get("title") or p["name"]).lower())
    for p in presets:
        layout.context_string_set("groomlab_preset", p["name"])
        layout.menu("GROOMLAB_MT_graph_preset_actions", text=p.get("title") or p["name"])


class GROOMLAB_MT_graph_presets(bpy.types.Menu):
    """Start from a look (hair, fur, feathers), or a technique to learn from"""
    bl_label = "Presets"

    def draw(self, context):
        layout = self.layout
        if not _catalog["presets"]:
            layout.operator("groomlab.graph_presets_refresh", text="List Presets", icon="FILE_REFRESH")
            return
        for category in preset_categories():
            if not category:
                layout.separator()
                draw_preset_items(layout, "")
                continue
            layout.context_string_set("groomlab_preset_category", category)
            layout.menu("GROOMLAB_MT_graph_preset_category", text=category)
        layout.separator()
        layout.operator("groomlab.graph_presets_refresh", icon="FILE_REFRESH")


class GROOMLAB_MT_graph_more(bpy.types.Menu):
    """Layout, view, the selected node"""
    bl_label = "GroomLab"

    def draw(self, context):
        layout = self.layout
        layout.operator("groomlab.graph_frame", text="Frame (F)")
        layout.operator("groomlab.graph_tidy")
        layout.separator()
        draw_node_actions(layout, context)
        layout.separator()
        layout.operator("groomlab.graph_reload", icon="FILE_REFRESH")


def draw_node_actions(layout, context):
    node = active_node(context)
    if node is None:
        return
    scalp = context_groom(context)
    layout.operator("groomlab.graph_set_output", icon="NODE").node = node.gl_name
    if _viewable(node):
        on = scalp is not None and scalp.groomlab.view_node == node.gl_name
        layout.operator("groomlab.graph_view_node", text="Focus: View Up to Here (V)",
                        icon="HIDE_OFF" if on else "HIDE_ON", depress=on).node = node.gl_name
    layout.operator("groomlab.graph_bypass", text="Bypass (B)", icon="CHECKBOX_HLT" if node.mute else "CHECKBOX_DEHLT").node = node.gl_name
    # When it runs: always, viewport only, render only.
    row = layout.row(align=True)
    for mode, label in (("always", "Always"), ("viewportOnly", "Viewport Only"), ("renderOnly", "Render Only")):
        op = row.operator("groomlab.graph_evaluation", text=label, depress=node.gl_evaluation == mode)
        op.node = node.gl_name
        op.mode = mode
    if editor_tree(context) is not None:
        layout.operator("groomlab.graph_duplicate", text="Duplicate (Ctrl+D)")
        layout.operator("node.delete", text="Delete")


def draw_context_menu(self, context):
    if editor_tree(context) is None:
        return
    layout = self.layout
    layout.separator()
    draw_node_actions(layout, context)


def draw_header(self, context):
    tree = editor_tree(context)
    if tree is None:
        return
    layout = self.layout
    scene = context.scene
    if tree.gl_scalp is not None and scene.groomlab.groom != tree.gl_scalp:
        defer(("follow", tree.name), _follow_editor, tree)
    check_tree(tree)
    row = layout.row(align=True)
    row.prop(scene.groomlab, "groom", text="", icon="CURVES")
    layout.menu("GROOMLAB_MT_graph_presets")
    scalp = tree.gl_scalp
    if scalp is not None and scalp.groomlab.view_node:
        op = layout.operator("groomlab.graph_view_node", text="Viewing %s" % scalp.groomlab.view_node,
                             icon="HIDE_OFF", depress=True)
        op.clear = True
    layout.menu("GROOMLAB_MT_graph_more", text="", icon="THREE_DOTS")


# ---- The Add menu: Maya's categories ------------------------------------------------------------

def _add_categories():
    import nodeitems_utils

    class GroomLabCategory(nodeitems_utils.NodeCategory):
        @classmethod
        def poll(cls, context):
            space = getattr(context, "space_data", None)
            return space is not None and getattr(space, "tree_type", "") == TREE

    def items_of(category):
        def items(context):
            for cat, types in graph_tree.categories() if graph_tree._node_classes else ():
                if cat != category:
                    continue
                for t in types:
                    cls = graph_tree.node_class(t)
                    if cls is not None and t not in graph_tree.HIDDEN_TYPES:
                        yield nodeitems_utils.NodeItem(cls.bl_idname, label=graph_tree.label_of(t))
        return items

    names = list(graph_tree.CATEGORY_TITLES)
    return [GroomLabCategory("GROOMLAB_" + re.sub(r"\W", "_", c).upper(), c, items=items_of(c)) for c in names]


# ---- The sidebar: the selected node's params (Maya's param panel) -------------------------------

class GROOMLAB_PT_graph_node(bpy.types.Panel):
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_category = "Node"
    bl_label = "GroomLab Node"

    @classmethod
    def poll(cls, context):
        return editor_tree(context) is not None

    def draw(self, context):
        tree = editor_tree(context)
        layout = self.layout
        if not check_tree(tree):
            layout.label(text="Reading the groom's graph...", icon="TIME")
            return
        node = tree.nodes.active
        if node is None or not ours(node) or not node.select:
            wrap(layout, "Select a node to edit it. Shift+A or Tab adds one; drag from a port to wire it.")
            return
        draw_node_panel(layout, context, node)


def draw_node_panel(layout, context, node):
    tree = node.id_data
    scalp = tree.gl_scalp
    scene = context.scene
    # The header: the name (edit it to rename), the type, Upgrade when outdated, what it does.
    row = layout.row(align=True)
    row.prop(node, "name", text="")
    kind = "OUTPUT" if node.gl_is_output and node.gl_type == "Output" else node.gl_type
    row.label(text=kind, icon="NODE")
    if node.gl_outdated:
        row.operator("groomlab.graph_upgrade_node", text="Upgrade", icon="FILE_REFRESH").node = node.gl_name
    about = (json.loads(node.gl_spec_json).get("description") if node.gl_spec_json else None) or node.gl_spec.get("description", "")
    if about:
        wrap(layout.column(), about)
    error = node_errors(scalp).get(node.gl_name) if scalp is not None else None
    if error:
        wrap(layout.box(), error, icon="ERROR")
    note = node_notes(scalp).get(node.gl_name) if scalp is not None else None
    if note:
        wrap(layout.column(), note, icon="INFO")
    draw_params(layout, context, node, scene)
    col = layout.column(align=True)
    col.separator()
    draw_node_actions(col, context)


def draw_params(layout, context, node, scene):
    op_type = node.gl_type
    specs = graph_tree.node_param_specs(node)
    params = graph_tree.values(node)
    mapped = json.loads(node.gl_mapped or "{}")
    inputs = set(json.loads(node.gl_inputs or "[]"))
    drivers = json.loads(node.gl_drivers or "{}")
    collection = {p.name: p for p in node.gl_params} if node.gl_spec_json else None

    def value_of(param):
        spec = node.gl_param_specs.get(param) if collection is None else None
        if spec is not None and hasattr(node, graph_tree.prop_name(param)) and param not in mapped:
            return graph_tree.property_value(node, spec, scene)
        return params.get(param)

    spec_of = {s["name"]: s for s in specs}.get  # what a showWhen names
    shown = []
    for spec in specs:
        name = spec["name"]
        if op_type == "Compound" and name == "definition":
            continue
        if (op_type, name) in graph_tree.PLUMBING and params.get(name, spec.get("default")) == spec.get("default"):
            continue
        shown.append(spec)
    if not shown:
        layout.label(text="No parameters.")
        return
    for section, members in graph_tree.param_sections(shown):
        visible = [s for s in members if graph_tree.shown_when(s, value_of, spec_of)]
        if not visible:
            continue
        col = layout.column()
        if section:
            folded = is_folded(op_type, section)
            op = col.operator("groomlab.graph_fold", text=section.upper(), icon="TRIA_RIGHT" if folded else "TRIA_DOWN",
                              emboss=False)
            op.op_type, op.section = op_type, section
            if folded:
                continue
        for spec in visible:
            draw_param(col, node, spec, scene, mapped, inputs, drivers, collection)


def _map_label(value):
    source = next((k for k in ("texture", "colorSet", "attribute", "expression", "guide") if k in value), None)
    label = {"texture": "texture", "colorSet": "color set", "attribute": "attribute", "expression": "expression",
             "guide": "painted map"}.get(source, "map")
    name = value.get(source) if source else ""
    return "%s %s" % (label, name if isinstance(name, str) else "")


def draw_param(layout, node, spec, scene, mapped, inputs, drivers, collection):
    name = spec["name"]
    unit = graph_tree._unit_of(node, spec)  # (unitWhen: as the node's values have it)
    label = graph_tree.pretty(name) + (" ◆" if name in inputs else "")
    suffix = units.suffix(unit, scene) if unit not in units.SCENE_LENGTH else ""
    driver = drivers.get(name)
    if name in mapped:
        row = layout.row(align=True)
        row.label(text="%s: %s" % (label, _map_label(mapped[name])), icon="IMAGE_DATA")
        if hasattr(bpy.types, "GROOMLAB_OT_paint_param"):  # Groom Tools' painting (when installed)
            op = row.operator("groomlab.paint_param", text="Paint", icon="BRUSH_DATA")
            op.node, op.param = node.gl_name, name
        return
    if spec.get("kind") == "ramp" and collection is None:
        if driver:
            layout.label(text="%s: driven by %s" % (label, driver), icon="LINKED")
            return
        layout.label(text=label)
        curve = graph_tree.ramp_node(node, name)
        if curve is None:
            layout.label(text="(updating)", icon="TIME")
            defer(("rebuild", node.id_data.name), _refresh_stale, node.id_data)
            return
        if graph_tree.ramp_changed(curve):
            defer(("ramps", node.id_data.name), graph_tree.sync_ramps, node.id_data)
        layout.template_curve_mapping(curve, "mapping")
        return
    if collection is not None:
        p = collection.get(name)
        if p is not None:
            from . import ui
            ui.draw_param(layout, p, scene)
        return
    prop = graph_tree.prop_name(name)
    if not hasattr(node, prop):
        layout.label(text="%s: edit it in Maya or the GroomLab app" % label)
        return
    if spec.get("channel") in ("writes", "reads") and spec.get("kind") == "string":
        draw_attribute_param(layout, node, spec, prop, label)
        return
    row = layout.row(align=True)
    if spec.get("kind") == "vec3" and unit != "color":
        col = layout.column(align=True)
        col.label(text=label + suffix)
        col.row(align=True).prop(node, prop, text="")
    else:
        slider = spec.get("kind") in ("float", "int") and ("softMax" in spec or "max" in spec)
        row.prop(node, prop, text=label + suffix, slider=slider)
    if driver:
        row.label(text="x %s" % driver, icon="LINKED")


def draw_attribute_param(layout, node, spec, prop, label):
    """A param naming an attribute: "Writes" (a warning when the name overwrites an upstream node's) or
    "Reads" with a menu of the attributes reaching the node, each with the node that writes it (a note
    when the one named does not reach)."""
    name = spec["name"]
    writes = spec.get("channel") == "writes"
    info = node_channels(node.id_data.gl_scalp).get(node.gl_name, {}) if node.id_data.gl_scalp is not None else {}
    col = layout.column(align=True)
    row = col.row(align=True)
    row.label(text="Writes" if writes else "Reads", icon="EXPORT" if writes else "IMPORT")
    row.prop(node, prop, text=label)
    if not writes:
        op = row.operator("groomlab.graph_pick_attribute", text="", icon="DOWNARROW_HLT")
        op.node, op.param = node.gl_name, name
    if writes:
        over = [o for o in info.get("overwrites", []) if o.get("param") == name]
        if over:
            col.label(text="overwrites " + ", ".join("%s from %s" % (o["name"], o["node"]) for o in over), icon="ERROR")
    else:
        read = next((r for r in info.get("reads", []) if r.get("param") == name), None)
        if read is not None and not read.get("reaches"):
            col.label(text="no node upstream writes %s: %s" % (read["name"], "none applies" if read.get("optional")
                                                                else "this node ignores it"), icon="INFO")


class GROOMLAB_OT_graph_pick_attribute(bpy.types.Operator):
    """Pick an attribute reaching this node: each with the node that writes it last"""
    bl_idname = "groomlab.graph_pick_attribute"
    bl_label = "Attributes Reaching the Node"
    bl_options = {"INTERNAL"}

    node: StringProperty()
    param: StringProperty()
    value: StringProperty(options={"SKIP_SAVE"})
    pick: BoolProperty(options={"SKIP_SAVE"})

    def execute(self, context):
        tree = editor_tree(context)
        target = next((n for n in tree.nodes if ours(n) and n.gl_name == self.node), None) if tree else None
        if target is None:
            return {"CANCELLED"}
        if not self.pick:  # the menu of what reaches the node
            sources = node_channels(tree.gl_scalp).get(self.node, {}).get("incoming", [])
            node, param = self.node, self.param

            def draw(menu, _context):
                if not sources:
                    menu.layout.label(text="No attribute reaches this node")
                for source in sources:
                    text = "%s (from %s)" % (source["name"], source["node"]) if source.get("node") else source["name"]
                    op = menu.layout.operator(GROOMLAB_OT_graph_pick_attribute.bl_idname, text=text)
                    op.node, op.param, op.value, op.pick = node, param, source["name"], True
            context.window_manager.popup_menu(draw, title="Attributes reaching " + self.node)
            return {"FINISHED"}
        setattr(target, graph_tree.prop_name(self.param), self.value)
        return {"FINISHED"}


# ---- The sidebar's GroomLab tab: the groom, its status, the presets browser -----------------------

class GROOMLAB_PT_graph_groom(bpy.types.Panel):
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_category = "GroomLab"
    bl_label = "Groom"

    @classmethod
    def poll(cls, context):
        space = context.space_data
        return space is not None and space.tree_type == TREE

    def draw(self, context):
        layout = self.layout
        layout.prop(context.scene.groomlab, "groom")
        tree = editor_tree(context)
        scalp = tree.gl_scalp if tree is not None else None
        if not groom.is_groom(scalp):
            layout.label(text="Add a groom in the 3D view's GroomLab tab")
            return
        rt = groom.runtime_if_any(scalp)
        if rt is not None and rt.last:
            layout.label(text="%s strands, %.0f ms" % ("{:,}".format(rt.last.get("strandCount") or 0),
                                                       rt.last.get("milliseconds") or 0.0), icon="INFO")
        text = graph_tree.message(scalp)
        if text:
            wrap(layout.box(), text, icon="ERROR")
        if rt is not None:
            for e in rt.errors + ([rt.error] if rt.error else []):
                wrap(layout.box(), e, icon="ERROR")
        row = layout.row(align=True)
        row.operator("groomlab.update", text="Evaluate", icon="FILE_REFRESH")
        row.operator("groomlab.graph_tidy", text="Tidy")


class GROOMLAB_PT_graph_presets(bpy.types.Panel):
    bl_space_type = "NODE_EDITOR"
    bl_region_type = "UI"
    bl_category = "GroomLab"
    bl_label = "Presets"

    @classmethod
    def poll(cls, context):
        return editor_tree(context) is not None

    def draw(self, context):
        layout = self.layout
        if not _catalog["presets"]:
            layout.operator("groomlab.graph_presets_refresh", text="List Presets", icon="FILE_REFRESH")
            return
        sg = context.scene.groomlab
        row = layout.row(align=True)
        row.prop(sg, "preset_category", text="")
        row.operator("groomlab.graph_presets_refresh", text="", icon="FILE_REFRESH")
        category = sg.preset_category
        col = layout.column(align=True)
        for p in sorted(_catalog["presets"], key=lambda p: (p.get("title") or p["name"]).lower()):
            if category not in ("ALL", "", p.get("category") or ""):
                continue
            row = col.row(align=True)
            op = row.operator("groomlab.graph_preset", text=p.get("title") or p["name"])
            op.name, op.mode = p["name"], "APPLY"
            op = row.operator("groomlab.graph_preset", text="", icon="NODE_INSERT_ON")
            op.name, op.mode = p["name"], "INSERT"
        layout.label(text="Click: apply. Insert: as a Compound after the selected node", icon="INFO")


# ---- The overlay: Maya's accent bars, badges and timings on the nodes -----------------------------

_draw_handle = [None]


def _node_times(scalp):
    """{node: ms} of the last evaluation (node_status), fetched once per evaluation."""
    rt = groom.runtime_if_any(scalp)
    if rt is None or not rt.last:
        return {}
    uid = scalp.groomlab.uid
    cached = _status.get(uid)
    serial = (rt.last.get("serial"), rt.last.get("milliseconds"))
    if cached is None or cached["serial"] != serial:
        def fetch():
            if rt.session is None or rt.generation != process.generation():
                return
            _status[uid] = {"serial": serial, "times": process.call("node_status", session=rt.session).get("times", {})}
        defer(("times", uid), fetch)
        return cached["times"] if cached else {}
    return cached["times"]


def _draw_overlay():
    try:
        _draw_overlay_now()
    except Exception as e:  # noqa: BLE001 - never break the editor's drawing
        print("GroomLab overlay: %r" % e)


def _draw_overlay_now():
    import blf
    import gpu
    from gpu_extras.batch import batch_for_shader
    context = bpy.context
    tree = editor_tree(context)
    region = context.region
    if tree is None or region is None or not groom.is_groom(tree.gl_scalp):
        return
    scalp = tree.gl_scalp
    v2d = region.view2d
    sysp = context.preferences.system
    scale = float(sysp.ui_scale) * float(getattr(sysp, "pixel_size", 1.0))
    x0, y0 = v2d.view_to_region(0.0, 0.0, clip=False)
    x1, _ = v2d.view_to_region(100.0, 0.0, clip=False)
    zoom = (x1 - x0) / 100.0
    errors, notes, times = node_errors(scalp), node_notes(scalp), _node_times(scalp)
    shader = gpu.shader.from_builtin("UNIFORM_COLOR")
    gpu.state.blend_set("ALPHA")
    font = 0

    def rect(ax, ay, bx, by, color):
        batch = batch_for_shader(shader, "TRIS", {"pos": [(ax, ay), (bx, ay), (bx, by), (ax, by)]},
                                 indices=[(0, 1, 2), (0, 2, 3)])
        shader.uniform_float("color", color)
        batch.draw(shader)

    def disc(cx, cy, r, color):
        import math
        pts = [(cx, cy)] + [(cx + r * math.cos(a * math.pi / 8), cy + r * math.sin(a * math.pi / 8)) for a in range(17)]
        batch = batch_for_shader(shader, "TRIS", {"pos": pts}, indices=[(0, i, i + 1) for i in range(1, 17)])
        shader.uniform_float("color", color)
        batch.draw(shader)

    def text(x, y, s, color, size):
        blf.size(font, max(6.0, size))
        blf.color(font, *color)
        blf.position(font, x, y, 0)
        blf.draw(font, s)

    for node in tree.nodes:
        if not ours(node) or node.hide:
            continue
        loc = node.location_absolute if hasattr(node, "location_absolute") else node.location
        w, h = node.dimensions
        if w <= 0:
            continue
        ax, ay = v2d.view_to_region(loc[0] * scale, loc[1] * scale, clip=False)
        bx, by = ax + w * zoom, ay - h * zoom
        if bx < 0 or ax > region.width or ay < 0 or by > region.height:
            continue
        category = "Output" if node.gl_is_output else graph_tree.category_of(node.gl_type)
        accent = graph_tree._rgb(graph_tree.CATEGORY_COLORS.get(category, (142, 150, 158)))
        rect(ax + 2 * zoom, ay, bx - 2 * zoom, ay - 3 * zoom * scale, accent)  # the category's accent bar
        r = 8 * zoom * scale
        size = 11 * zoom * scale
        name = node.gl_name
        if name in errors:
            disc(bx, ay, r, (0.94, 0.31, 0.27, 1.0))
            text(bx - 0.18 * size, ay - 0.36 * size, "!", (1, 1, 1, 1), size)
        elif name in notes:
            disc(bx, ay, r, (0.5, 0.52, 0.55, 1.0))
            text(bx - 0.14 * size, ay - 0.36 * size, "i", (0.96, 0.96, 0.96, 1), size)
        if node.gl_viewed:
            disc(bx - 2.6 * r, ay, r * 0.8, (0.35, 0.82, 0.94, 1.0))
            disc(bx - 2.6 * r, ay, r * 0.3, (0.1, 0.1, 0.12, 1.0))
        ms = times.get(name)
        if ms is not None:
            heat = (0.47, 0.78, 0.47, 1) if ms < 5 else ((0.9, 0.75, 0.31, 1) if ms < 25 else (0.94, 0.39, 0.31, 1))
            label = "%.1f ms" % ms
            blf.size(font, max(6.0, 10 * zoom * scale))
            tw, _ = blf.dimensions(font, label)
            text(bx - tw - 4 * zoom, by - 13 * zoom * scale, label, heat, 10 * zoom * scale)
    gpu.state.blend_set("NONE")


# ---- Registration -------------------------------------------------------------------------------

_keymaps = []

classes = (GROOMLAB_OT_graph_open, GROOMLAB_OT_graph_add_search, GROOMLAB_OT_graph_frame, GROOMLAB_OT_graph_duplicate,
           GROOMLAB_OT_graph_view_node, GROOMLAB_OT_graph_bypass, GROOMLAB_OT_graph_evaluation, GROOMLAB_OT_graph_set_output,
           GROOMLAB_OT_graph_upgrade_node, GROOMLAB_OT_graph_fold, GROOMLAB_OT_graph_pick_attribute, GROOMLAB_OT_graph_tidy, GROOMLAB_OT_graph_reload,
           GROOMLAB_OT_graph_preset, GROOMLAB_OT_graph_presets_refresh, GROOMLAB_MT_graph_preset_actions,
           GROOMLAB_MT_graph_preset_category, GROOMLAB_MT_graph_presets, GROOMLAB_MT_graph_more,
           GROOMLAB_PT_graph_node, GROOMLAB_PT_graph_groom, GROOMLAB_PT_graph_presets)

KEYS = (("groomlab.graph_add_search", "TAB", {}), ("groomlab.graph_frame", "F", {}),
        ("groomlab.graph_view_node", "V", {}), ("groomlab.graph_bypass", "B", {}),
        ("groomlab.graph_duplicate", "D", {"ctrl": True}))


def register():
    import nodeitems_utils
    for c in classes:
        bpy.utils.register_class(c)
    nodeitems_utils.register_node_categories("GROOMLAB_NODES", _add_categories())
    bpy.types.NODE_HT_header.append(draw_header)
    bpy.types.NODE_MT_context_menu.append(draw_context_menu)
    _draw_handle[0] = bpy.types.SpaceNodeEditor.draw_handler_add(_draw_overlay, (), "WINDOW", "POST_PIXEL")
    kc = bpy.context.window_manager.keyconfigs.addon if bpy.context.window_manager else None
    if kc is not None:
        km = kc.keymaps.new(name="Node Editor", space_type="NODE_EDITOR")
        for idname, key, mods in KEYS:
            _keymaps.append((km, km.keymap_items.new(idname, key, "PRESS", **mods)))


def unregister():
    import nodeitems_utils
    for km, kmi in _keymaps:
        try:
            km.keymap_items.remove(kmi)
        except (ReferenceError, RuntimeError):
            pass
    _keymaps.clear()
    if _draw_handle[0] is not None:
        bpy.types.SpaceNodeEditor.draw_handler_remove(_draw_handle[0], "WINDOW")
        _draw_handle[0] = None
    bpy.types.NODE_MT_context_menu.remove(draw_context_menu)
    bpy.types.NODE_HT_header.remove(draw_header)
    try:
        nodeitems_utils.unregister_node_categories("GROOMLAB_NODES")
    except KeyError:
        pass
    if bpy.app.timers.is_registered(_run_deferred):
        bpy.app.timers.unregister(_run_deferred)
    _deferred.clear()
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
