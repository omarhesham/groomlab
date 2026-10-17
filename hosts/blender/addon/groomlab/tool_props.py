# SPDX-License-Identifier: GPL-3.0-or-later
"""The Groom Tools' properties: the palette's tool settings (Scene.groomlab_tools, saved with the
.blend, as Maya keeps the palette's in its preferences), each groom's display settings and its
kept state (Object.groomlab_tools, as Maya keeps them on the groom node), and the add-on's
preferences (the pie menu's key)."""

import zlib

import bpy
from bpy.props import (BoolProperty, CollectionProperty, EnumProperty, FloatProperty, FloatVectorProperty,
                       IntProperty, PointerProperty, StringProperty)

from . import props


def _push(*keys):
    """An update callback sending these settings to the active tool."""
    def update(self, context):
        if props.is_quiet():
            return
        from . import tools
        tools.push_safely(context)
    return update


def _strength(self, context):
    if props.is_quiet():
        return
    from . import tools
    tools.set_strength(context.scene, self.strength)
    tools.push_safely(context)


def _display(self, context):
    if props.is_quiet():
        return
    from . import display
    scalp = self.id_data if isinstance(self.id_data, bpy.types.Object) else None
    if scalp is not None:
        display.apply_strand_visibility(scalp)
        display.mark(scalp)
    else:
        display.mark_all()
    display.redraw()


def _number(name):
    """A group's enum value from its name: the choice stays on the group as others come and go
    (Blender keeps a dynamic enum's number, not its name)."""
    return zlib.crc32(name.encode("utf-8")) & 0x3FFFFFFF


def _group_items(self, context):
    from . import tools
    items = [(tools.ALL_GROUPS, "All groups", "Every guide: brushes edit every group", "NONE", _number(tools.ALL_GROUPS))]
    names = [g for g in self.groups]
    for g in names:
        items.append((g.name, "    " * g.depth + g.name,
                      "%s: level %d, %d guides%s" % (g.name, g.level, g.guides, " (hidden)" if g.hidden else ""), "NONE",
                      _number(g.name)))
    if not names:
        items.append((tools.FIRST_GROUP, tools.FIRST_GROUP, "Place starts this group on a groom without guides", "NONE",
                      _number(tools.FIRST_GROUP)))
    _group_cache[:] = items
    return _group_cache


_group_cache = []


def _group_changed(self, context):
    if props.is_quiet():
        return
    with props.quiet():
        self.group_index = next((i for i, g in enumerate(self.groups) if g.name == self.group), -1)
    from . import display, tools
    tools.push_safely(context)
    display.mark_all()
    display.redraw()


def _group_index_changed(self, context):
    if props.is_quiet():
        return
    if 0 <= self.group_index < len(self.groups):
        self.group = self.groups[self.group_index].name


def _map_index_changed(self, context):
    if props.is_quiet():
        return
    if 0 <= self.map_index < len(self.maps):
        with props.quiet():
            self.paint_map = self.maps[self.map_index].name
        from . import display, tools
        tools.push_safely(context)
        display.mark_all()
        display.redraw()


def _soft_radius(self, context):
    if props.is_quiet():
        return
    from . import tools
    scalp = tools.active(context)
    if scalp is not None and tools.is_hierarchy(scalp):
        try:
            tools.selection_set(scalp, softSelectRadius=float(self.soft_radius), mode="replace",
                                ids=_selected_ids(scalp))
        except Exception as e:  # noqa: BLE001 - shown in the palette
            tools.status(context.scene, str(e))


def _selected_ids(scalp):
    from . import tools
    info = tools.groom_tools(scalp).info
    return list((info.get("selection") or {}).get("selection") or [])


class GroomLabGroupItem(bpy.types.PropertyGroup):
    # name: the group's
    parent: StringProperty()
    level: IntProperty()
    guides: IntProperty()
    hidden: BoolProperty()
    depth: IntProperty()


class GroomLabMapItem(bpy.types.PropertyGroup):
    # name: the map's
    kind: StringProperty()      # guides, vertices, texture
    channels: IntProperty(default=1)
    texture: StringProperty()


FLOW_ITEMS = [(k or "ARROWS", label, tip) for k, label, tip in (
    ("", "Arrows", "Drag to lay arrows; drag over arrows to comb them"),
    ("whorl", "Whorl", "Hair spirals out counter-clockwise around the click (a crown)"),
    ("whorlcw", "Whorl (clockwise)", "Hair spirals out clockwise around the click"),
    ("converge", "Gather", "Hair points in toward the click"),
    ("part", "Part out", "Hair points away from the click (a cowlick)"))]


# The brush's curves (the catalog's falloffProfile and strengthProfile) as presets: each a ramp the
# helper takes ([[position, value], ...]), or None (the slider's falloff; the same all along).
FALLOFF_CURVES = [("SLIDER", "Slider", "The Falloff slider's smooth falloff", None),
                  ("LINEAR", "Linear", "Straight down from the center to the edge", [[0, 1], [1, 0]]),
                  ("SMOOTH", "Smooth", "An S from the center to the edge", {"keys": [[0, 1], [0.5, 0.5], [1, 0]]}),
                  ("SPHERE", "Sphere", "Full most of the way, falling at the edge", [[0, 1], [0.6, 0.9], [1, 0]]),
                  ("SHARP", "Sharp", "Falling fast from the center", [[0, 1], [0.25, 0.25], [1, 0]]),
                  ("CONSTANT", "Constant", "Full to the edge", [[0, 1], [1, 1]])]
STRENGTH_PROFILES = [("FLAT", "Flat", "The whole guide alike", None),
                     ("TIPS", "Tips", "Tips more than roots", [[0, 0], [1, 1]]),
                     ("TIPS_ONLY", "Tips only", "The outer part of each guide", [[0, 0], [0.5, 0], [1, 1]]),
                     ("ROOTS", "Roots", "Roots more than tips", [[0, 1], [1, 0]]),
                     ("MIDDLE", "Middle", "The middle of each guide most", [[0, 0], [0.5, 1], [1, 0]])]


def curve_of(items, key):
    """The ramp of preset `key` in `items` (None for none)."""
    return next((c for k, _, _, c in items if k == key), None)


class GroomLabTools(bpy.types.PropertyGroup):
    """Maya's Groom Tools palette, its settings by the catalog's labels and hints (brush_catalog.h)."""
    tool: StringProperty(description="The active GroomLab tool (a catalog id)")
    tab: EnumProperty(name="Tab", items=[("TOOLS", "Tools", "The brushes, guide groups and edits"),
                                         ("PAINT", "Paint", "Painted maps: 0-1 values on the scalp any param can read")],
                      default="TOOLS")
    status: StringProperty()
    strengths: StringProperty()  # {tool: strength}: each tool keeps its own, as in Maya
    radius: IntProperty(name="Radius", default=60, min=1, max=2000, soft_min=5, soft_max=400, subtype="PIXEL",
                        description="The brush circle (B held + drag sets it)", update=_push("radius"))
    strength: FloatProperty(name="Strength", default=1.0, min=-1.0, max=1.0, update=_strength,
                            description="How much each stamp does (Comb: the fraction of the drag); negative reverses "
                                        "the tools that work both ways (M held + drag sets it)")
    falloff: FloatProperty(name="Falloff", default=1.0, min=0.0, max=1.0, subtype="FACTOR", update=_push(),
                           description="How the brush fades toward its edge: 0 not at all (hard to the edge), 1 from "
                                       "the center out (soft)")
    falloff_curve: EnumProperty(name="Falloff curve", items=[(k, l, d) for k, l, d, _ in FALLOFF_CURVES], default="SLIDER",
                                update=_push(),
                                description="The falloff drawn as a curve instead: the weight from the brush's center to "
                                            "its edge; Slider: the Falloff slider's")
    strength_profile: EnumProperty(name="Root to tip", items=[(k, l, d) for k, l, d, _ in STRENGTH_PROFILES], default="FLAT",
                                   update=_push(),
                                   description="How much of the strength each part of a guide gets, root to tip: tips "
                                               "more than roots, or the reverse; Flat: the whole guide alike")
    smooth_strength: FloatProperty(name="Smooth", default=1.0, min=0.0, max=3.0, update=_push(),
                                   description="How much Shift+drag smooths a map per stamp (past 1: several passes)")
    symmetry: BoolProperty(name="Symmetry", default=False, update=_push(),
                           description="Mirror every stroke: across world X, or across the scalp's own topology")
    symmetry_mode: EnumProperty(name="Across", items=[("X", "World X", "Across the plane x = 0"),
                                                      ("TOPOLOGY", "Topology",
                                                       "Across the scalp's own surface: its left and right vertices "
                                                       "matched once, whatever its pose (a head turned, or not quite "
                                                       "symmetric)")],
                                default="X", update=_push(), description="What symmetry mirrors across")
    selected_only: BoolProperty(name="Selected only", default=False, update=_push(),
                                description="Only the selected guides, scaled by the soft selection")
    pressure: BoolProperty(name="Pen pressure", default=True, update=_push(),
                           description="A tablet's pressure scales the strength")
    collide: BoolProperty(name="Keep off scalp", default=True, update=_push(), description="Guides never dip into the scalp")
    front_facing: BoolProperty(name="Front facing only", default=True, update=_push(),
                               description="What the scalp hides from the camera gets no weight: a stroke never reaches "
                                           "guides, parts, flow arrows or paint on the far side of the head")
    stretch: BoolProperty(name="Stretch", default=False, update=_push(),
                          description="On: guides get longer (or shorter) as you pull them. Off: they keep their length")
    target_guides: BoolProperty(name="Guides", default=True, update=_push(), description="The brush shapes guides")
    target_parts: BoolProperty(name="Parts", default=False, update=_push(), description="The brush shapes part lines")
    target_flow: BoolProperty(name="Flow", default=False, update=_push(), description="The brush shapes flow arrows and sinks")
    erase_guides: BoolProperty(name="Guides", default=True, update=_push(), description="Erase removes guides (with their children)")
    erase_parts: BoolProperty(name="Parts", default=True, update=_push(), description="Erase removes parts")
    erase_flow: BoolProperty(name="Flow", default=True, update=_push(), description="Erase removes flow arrows")
    match_neighbors: BoolProperty(name="Match nearby guides", default=True, update=_push(),
                                  description="New guides take the shape, length and painted maps of the nearest guides: "
                                              "the group's own, or the groups around it while it has none. Draw takes the "
                                              "maps. Off: they stand straight up, with no maps")
    solo_new_guides: BoolProperty(name="Solo new guides", default=False, update=_push(),
                                  description="New guides stay unfrozen and every other guide is frozen, so the brushes "
                                              "work on the new ones at once (Shift+click a guide to solo it, Unfreeze "
                                              "All to free them all)")
    length: FloatProperty(name="Length", default=0.005, min=1e-6, soft_max=1.0, subtype="DISTANCE", unit="LENGTH",
                          precision=4, update=_push(), description="A standing new guide's length")
    segments: IntProperty(name="Segments", default=12, min=1, max=256, soft_max=32, update=_push(),
                          description="A new guide's segments")
    spacing: FloatProperty(name="Spacing", default=0.001, min=0.0, soft_max=0.1, subtype="DISTANCE", unit="LENGTH",
                           precision=4, update=_push(), description="The distance between the guides a drag places")
    part_name: StringProperty(name="Name", default="part", update=_push(), description="Name for the next part (made unique)")
    flow_sink_item: EnumProperty(name="Place", items=FLOW_ITEMS, default="ARROWS", update=_push(),
                                 description="What a click or drag lays: arrows, or a sink (brush-sized) hair turns around")
    # Paint
    paint_map: StringProperty(name="Map")
    map_index: IntProperty(update=_map_index_changed)
    maps: CollectionProperty(type=GroomLabMapItem)
    value: FloatProperty(name="Value", default=1.0, min=0.0, max=1.0, subtype="FACTOR", update=_push(),
                         description="What the Paint brush paints toward, and what Fill sets")
    paint_color: FloatVectorProperty(name="Color", size=4, default=(1.0, 1.0, 1.0, 1.0), min=0.0, max=1.0,
                                     subtype="COLOR_GAMMA", update=_push(),
                                     description="The color the Paint brush paints on a color map (Ctrl: its inverse)")
    channel_r: BoolProperty(name="R", default=True, update=_push(), description="Paint, smooth and fill change red")
    channel_g: BoolProperty(name="G", default=True, update=_push(), description="Paint, smooth and fill change green")
    channel_b: BoolProperty(name="B", default=True, update=_push(), description="Paint, smooth and fill change blue")
    channel_a: BoolProperty(name="A", default=True, update=_push(), description="Paint, smooth and fill change alpha")
    show_on_guides: BoolProperty(name="Guides", default=False, update=_display, description="Color the guides by the map")
    show_on_scalp: BoolProperty(name="Scalp", default=True, update=_display,
                                description="Show the map on the scalp (black to white), whatever its kind")
    scalp_channel: EnumProperty(name="Channel", items=[("rgb", "RGB", "Its color"), ("r", "R", ""), ("g", "G", ""),
                                                       ("b", "B", ""), ("a", "A", "")], default="rgb", update=_display,
                                description="What of a color map the scalp shows: its color, or one channel black to white")
    # Selection
    soft_radius: FloatProperty(name="Soft radius", default=0.0, min=0.0, soft_max=0.1, subtype="DISTANCE", unit="LENGTH",
                               update=_soft_radius,
                               description="Soft selection radius: guides near the selection are partly selected; brushes "
                                           "with Selected only fade out over it")
    select_shape: EnumProperty(name="Shape", items=[("LASSO", "Lasso", "Drag a lasso around guides"),
                                                    ("BOX", "Box", "Drag a box over guides")], default="LASSO")
    # The group brushes edit and Place and Draw add to
    group: EnumProperty(name="Group", items=_group_items, update=_group_changed,
                        description="The guide group brushes edit and new guides go into; All groups: every guide")
    group_index: IntProperty(update=_group_index_changed)
    groups: CollectionProperty(type=GroomLabGroupItem)
    isolate: BoolProperty(name="Isolate", default=False, update=_display,
                          description="Show the group's level at full color and dim the others")
    guide_count: IntProperty()
    selected_count: IntProperty()
    frozen_count: IntProperty()
    children_count: IntProperty(name="Children", default=5, min=1, max=500,
                                description="Children per guide for Edit Guides > Children (the selected guides, else the "
                                            "picked group)")
    # While sculpting
    sculpt_display: EnumProperty(name="Strands", items=[("NORMAL", "Strands as they are", ""),
                                                        ("GUIDES_ONLY", "Guides only", "The strands hide while a stroke is live")],
                                 default="NORMAL", description="How strands draw while a stroke is live")
    full_density: BoolProperty(name="Full density", default=False,
                               description="Keep every strand of the preview up while a stroke is live, instead of a "
                                           "lighter share of them (slower on heavy grooms, but nothing pops)")
    starter_dismissed: BoolProperty(default=False)

    @property
    def flow_sink(self):
        return "" if self.flow_sink_item == "ARROWS" else self.flow_sink_item


STRAND_COLORS = [("solid", "Solid", "The hair's own color"),
                 ("colorSet", "Color set", "A color set (map) of the scalp, at each root"),
                 ("attribute", "Attribute", "A strand channel of the graph, black to white over Min to Max"),
                 ("random", "Random", "A color per strand"), ("clump", "Clump", "A color per clump"),
                 ("density", "Density", "Strands per area: a heatmap")]


class GroomLabGroomDisplay(bpy.types.PropertyGroup):
    """A groom's viewport display (Maya's groom attributes) and its kept state."""
    show_guides: BoolProperty(name="Guides", default=True, update=_display, description="Draw the guides")
    show_tubes: BoolProperty(name="Tubes", default=False, update=_display,
                             description="Draw each guide's tube (where its children grow)")
    show_flow: BoolProperty(name="Flow", default=True, update=_display, description="Draw parts and flow arrows")
    show_strands: BoolProperty(name="Strands", default=True, update=_display, description="Show the hair in the viewport")
    guide_points: EnumProperty(name="Points", items=[("off", "Off", ""), ("ends", "Roots and tips", ""),
                                                     ("all", "All", "Thinned out on grooms with many guides")],
                               default="all", update=_display, description="The guides' points drawn as dots")
    guide_color_mode: EnumProperty(name="Guide Colors", items=[("level", "Level", "By level"),
                                                               ("layer", "Layer", "By a painted layer"),
                                                               ("group", "Group", "A hue per group")],
                                   default="level", update=_display)
    guide_color_layer: StringProperty(name="Layer", update=_display, description="The painted layer the guides show")
    strand_color: EnumProperty(name="Strand Colors", items=STRAND_COLORS, default="solid", update=_display,
                               description="What the strands show in the viewport (renders keep the hair's material)")
    strand_color_source: StringProperty(name="Source", default="clumpId", update=_display,
                                        description="The color set or channel the strands show")
    strand_color_min: FloatProperty(name="Min", default=0.0, update=_display)
    strand_color_max: FloatProperty(name="Max", default=1.0, update=_display)
    state: StringProperty()  # the groom's state (get_state) kept in the .blend: guides, maps, selection


def _keymap_draw(prefs, context):
    import rna_keymap_ui
    from . import tool_ops
    layout = prefs.layout
    layout.label(text="GroomLab's pie menu (Maya's marking menu) and Groom Tools keys:")
    kc = context.window_manager.keyconfigs.user
    for km_name, idname, prop in tool_ops.KEYMAP_ITEMS:
        km = kc.keymaps.get(km_name)
        if km is None:
            continue
        for kmi in km.keymap_items:
            if kmi.idname == idname and (prop is None or getattr(kmi.properties, prop[0], None) == prop[1]):
                col = layout.column()
                col.context_pointer_set("keymap", km)
                rna_keymap_ui.draw_kmi([], kc, km, kmi, col, 0)
    box = layout.box()
    box.label(text="While a GroomLab tool is active (Maya's keys):")
    for keys, what in tool_ops.shortcuts():
        row = box.row()
        row.label(text=keys)
        row.label(text=what)


class GroomLabPreferences(bpy.types.AddonPreferences):
    bl_idname = __package__

    def draw(self, context):
        _keymap_draw(self, context)


classes = (GroomLabGroupItem, GroomLabMapItem, GroomLabTools, GroomLabGroomDisplay, GroomLabPreferences)


def register():
    for c in classes:
        bpy.utils.register_class(c)
    bpy.types.Scene.groomlab_tools = PointerProperty(type=GroomLabTools)
    bpy.types.Object.groomlab_tools = PointerProperty(type=GroomLabGroomDisplay)


def unregister():
    del bpy.types.Object.groomlab_tools
    del bpy.types.Scene.groomlab_tools
    for c in reversed(classes):
        bpy.utils.unregister_class(c)
