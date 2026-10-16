# SPDX-License-Identifier: Apache-2.0
"""Guides: made from curves or an Alembic file, their hierarchy and groups, selection, the hierarchy
tools, the sculpting display, resampling and per-guide attributes."""

import json

from maya import cmds

from groomlab.base import ensure_plugin, undo_chunk, _shape_of
from groomlab.params import get_graph, set_graph
from groomlab.paths import portable_path


def add_guides(groom, curves):
    """Connect curves (transforms or nurbsCurve shapes) as guides of a groom."""
    for curve in curves:
        cmds.connectAttr(_shape_of(curve, "nurbsCurve") + ".worldSpace[0]", groom + ".guideCurves",
                         nextAvailable=True)


# ---------------------------------------------------------------------------
# Guide hierarchy (stored on the groom, edited through groomLabGuides; every
# edit is undoable)


def guides_from_curves(groom, curves, group="primary", tube_radius=0.1, tube_tip_radius=None, disconnect=True):
    """Add curves as primary guides of a root group; returns the new guide IDs.

    The curves are copied, so they can be deleted afterwards. With
    `disconnect`, curves also connected to the groom's guideCurves are
    disconnected so they are not used twice.
    """
    shape_names = [_shape_of(c, "nurbsCurve") for c in curves]
    kwargs = {"fromCurves": shape_names, "group": group, "tubeRadius": tube_radius}
    if tube_tip_radius is not None:
        kwargs["tubeTipRadius"] = tube_tip_radius
    ids = cmds.groomLabGuides(groom, **kwargs)

    if disconnect:
        for shape in shape_names:
            for dst in cmds.listConnections(shape + ".worldSpace", plugs=True, source=False) or []:
                if dst.split(".")[0] == groom.split("|")[-1] and ".guideCurves" in dst:
                    cmds.disconnectAttr(shape + ".worldSpace[0]", dst)
    return ids


def generate_children(groom, parent_group=None, name=None, count=5, root_spread=0.8, tip_spread=1.0, twist=0.0,
                      jitter=0.1, tube_scale=0.35, seed=0, guides=None, selected=False):
    """Give guides `count` children each, in the child group `name` (default "<parent group>_children").

    Parents: `guides` (IDs), or the selected guides with `selected=True`, else
    every guide of `parent_group`. The child group is created if missing and
    added to otherwise; a parent that already has children there gets new
    ones. So you choose, parent by parent, which guides have children, at any
    level (children of children too). Spreads and jitter are in units of the
    parent tube radius; twist is in radians. Returns the size of the child group.
    """
    kwargs = {"generateChildren": parent_group or "", "count": count, "rootSpread": root_spread,
              "tipSpread": tip_spread, "twist": twist, "jitter": jitter, "tubeScale": tube_scale, "seed": seed}
    if name:
        kwargs["name"] = name
    if guides:
        kwargs["guide"] = [str(g) for g in guides]
    elif selected:
        kwargs["onlySelected"] = True
    elif not parent_group:
        raise ValueError("give a parent group, guides, or selected=True")
    return cmds.groomLabGuides(groom, **kwargs)


def list_guide_groups(groom):
    """Return the groom's guide groups as dicts: name, parent, depth, count."""
    groups = []
    for row in cmds.groomLabGuides(groom, listGroups=True) or []:
        name, parent, depth, count = row.split("|")
        groups.append({"name": name, "parent": parent or None, "depth": int(depth), "count": int(count)})
    return groups


def guide_ids(groom, group):
    """Return the IDs (as strings) of the guides in a group."""
    return cmds.groomLabGuides(groom, listGuides=group) or []


def lock_guides(groom, ids):
    """Detach child guides from their parents; they keep their current shape."""
    cmds.groomLabGuides(groom, lock=True, guide=list(ids))


def unlock_guides(groom, ids):
    """Re-attach locked child guides to their parents without moving them."""
    cmds.groomLabGuides(groom, unlock=True, guide=list(ids))


def remove_guide_group(groom, group):
    """Remove a guide group with its sub-groups and guides."""
    cmds.groomLabGuides(groom, removeGroup=group)


def _group_list(text):
    return [g for g in (text or "").replace(",", " ").split() if g]


def group_users(groom, group):
    """[(node, param)] of the graph params naming a guide group: Guides nodes
    reading it (group) or leaving it out (exclude)."""
    users = []
    for node in get_graph(groom, expand_default=True).get("nodes", []):
        params = node.get("params", {})
        if node.get("type") != "Guides":
            continue
        if params.get("group") == group:
            users.append((node["name"], "group"))
        if group in _group_list(params.get("exclude")):
            users.append((node["name"], "exclude"))
    return users


def add_guide_group(groom, name, parent=None):
    """A new, empty guide group: top-level, or with `parent` a child group of it
    (guides placed in it attach to the nearest guide of the parent group)."""
    kwargs = {"under": parent} if parent else {}
    cmds.groomLabGuides(groom, addGroup=name, **kwargs)
    return name


def rename_guide_group(groom, old, new):
    """Rename a guide group, and the graph params that name it (one undo step)."""
    with undo_chunk("rename guide group"):
        cmds.groomLabGuides(groom, renameGroup=(old, new))
        graph = get_graph(groom)
        touched = False
        for node in graph.get("nodes", []):
            params = node.get("params", {})
            if node.get("type") != "Guides":
                continue
            if params.get("group") == old:
                params["group"] = new
                touched = True
            excluded = _group_list(params.get("exclude"))
            if old in excluded:
                params["exclude"] = " ".join(new if g == old else g for g in excluded)
                touched = True
        if touched:
            set_graph(groom, graph)
        colors = guide_group_colors(groom)
        if old in colors:  # and keeps its color
            set_guide_group_color(groom, old, None)
            set_guide_group_color(groom, new, colors[old])
        hidden = hidden_guide_groups(groom)
        if old in hidden:  # a hidden group stays hidden
            cmds.setAttr(groom + ".hiddenGuideGroups", " ".join(new if g == old else g for g in hidden), type="string")
    return new


def guide_group_colors(groom):
    """{group: "#rrggbb"} of the guide groups given their own viewport color."""
    colors = {}
    for word in (cmds.getAttr(groom + ".guideGroupColors") or "").split():
        name, _, color = word.rpartition(":")
        if name and len(color) == 7 and color.startswith("#"):
            colors[name] = color
    return colors


def set_guide_group_color(groom, group, color):
    """Give a guide group its own viewport color: "#rrggbb", or (r, g, b) in 0-1; None goes back to
    the level (or group) colors."""
    if isinstance(color, (tuple, list)):
        color = "#%02x%02x%02x" % tuple(int(round(max(0.0, min(1.0, float(c))) * 255)) for c in color)
    colors = guide_group_colors(groom)
    colors.pop(group, None)
    if color:
        colors[group] = color.lower()
    cmds.setAttr(groom + ".guideGroupColors", " ".join("%s:%s" % kv for kv in sorted(colors.items())),
                 type="string")


def hidden_guide_groups(groom):
    """Names of the guide groups hidden in the viewport."""
    return _group_list(cmds.getAttr(groom + ".hiddenGuideGroups"))


def show_guide_group(groom, group, visible=True):
    """Show or hide a guide group in the viewport. Hidden groups aren't drawn,
    brushed or picked by the select tool; the strands they drive stay."""
    hidden = [g for g in hidden_guide_groups(groom) if g != group]
    if not visible:
        hidden.append(group)
    cmds.setAttr(groom + ".hiddenGuideGroups", " ".join(hidden), type="string")


def color_guides_by_group(groom, on=True):
    """Color each guide group its own hue in the viewport; off goes back to level colors."""
    cmds.setAttr(groom + ".guideColorMode", 2 if on else 0)


def clear_guides(groom):
    """Remove every guide group and guide."""
    cmds.groomLabGuides(groom, clear=True)


# ---------------------------------------------------------------------------
# Guide selection (GT-5; stored on the groom, every change undoable)

SELECT_CONTEXT = "groomLabSelectCtx1"


def _targets(guides=None, group=None, level=None):
    kwargs = {}
    if guides:
        kwargs["guide"] = [str(g) for g in guides]
    elif group:
        kwargs["group"] = group
    elif level is not None:
        kwargs["level"] = level
    return kwargs


def select_guides(groom, guides=None, group=None, level=None, mode="replace"):
    """Select guides by ID, group or level. mode: replace, add, deselect or toggle. Returns the selection."""
    if mode not in ("replace", "add", "deselect", "toggle"):
        raise ValueError("mode must be replace, add, deselect or toggle")
    kwargs = _targets(guides, group, level)
    if not kwargs:
        raise ValueError("give guides, a group or a level")
    if mode != "replace":
        kwargs["append" if mode == "add" else mode] = True
    return cmds.groomLabGuides(groom, select=True, **kwargs) or []


def selected_guides(groom):
    return cmds.groomLabGuides(groom, listSelected=True) or []


def clear_guide_selection(groom):
    cmds.groomLabGuides(groom, clearSelection=True)


def mirror_guide_selection(groom, axis="x", center=0.0, add=False):
    """Select the mirror images of the selected guides (nearest guide in the same group)."""
    kwargs = {"append": True} if add else {}
    return cmds.groomLabGuides(groom, mirrorSelection=axis, center=center, **kwargs) or []


def set_soft_selection(groom, radius):
    """Soft-select guides within `radius` (scene units) of the selection; 0 turns it off."""
    cmds.setAttr(groom + ".softSelectRadius", radius)


def select_tool(level=0, group="", roots_only=False, solo=True, symmetry=None, activate=True):
    """The lasso tool for guides of the selected groom (Shift: toggle, Ctrl: deselect, Ctrl+Shift: add).
    With `solo` every pick also freezes the guides not selected, in the same undo step, so the
    brushes work on the selection; a pick of nothing unfreezes every guide. `symmetry` ("x" or
    "topology", as the brushes take it) picks each guide's mirror guide too."""
    ensure_plugin()
    if not cmds.contextInfo(SELECT_CONTEXT, exists=True):
        cmds.groomLabSelectCtx(SELECT_CONTEXT)
    cmds.groomLabSelectCtx(SELECT_CONTEXT, edit=True, level=level, group=group, rootsOnly=roots_only,
                           soloSelection=bool(solo), symmetry=symmetry or "none")
    if activate:
        cmds.setToolTo(SELECT_CONTEXT)
    return SELECT_CONTEXT


# ---------------------------------------------------------------------------
# Hierarchy tools (shapes kept; every edit undoable)


def reparent_guides(groom, parent, guides=None):
    """Make guides (default: the selected ones) follow another parent guide in their parent group."""
    return cmds.groomLabGuides(groom, reparent=str(parent), **_targets(guides))


def promote_group(groom, group):
    """Move a group one level up the hierarchy (a group reaching the top becomes primaries)."""
    cmds.groomLabGuides(groom, promote=group)


def demote_group(groom, group, under):
    """Move a group one level down, under a sibling group; each guide follows the nearest guide there."""
    cmds.groomLabGuides(groom, demote=group, under=under)


def mirror_group(groom, group="primary", axis="x", center=0.0, from_negative=False):
    """Copy a root group's guides from one side of a plane to the other, replacing what is there."""
    kwargs = {"fromNegative": True} if from_negative else {}
    return cmds.groomLabGuides(groom, mirror=group, axis=axis, center=center, **kwargs) or []


def symmetry_sync(groom, axis="x", center=0.0, from_negative=False):
    """Make the whole groom symmetric: every guide on one side, children included, copied to the other.

    Hand-edited and locked children and painted attributes come across too. One undo step.
    Returns the new guide IDs.
    """
    return mirror_group(groom, "*", axis, center, from_negative)


# ---------------------------------------------------------------------------
# The sculpting display


def set_sculpt_display(groom, mode="dimStrands", density=0.35):
    """While a stroke is live: "normal", "dimStrands" or "guidesOnly", at `density` x the preview
    density (1.0: every strand of the preview stays up, nothing pops when the stroke ends)."""
    modes = ("normal", "dimStrands", "guidesOnly")
    cmds.setAttr(groom + ".sculptDisplay", modes.index(mode))
    cmds.setAttr(groom + ".sculptDensity", density)


def isolate_level(groom, level=None):
    """Show one guide level at full color and dim the rest; None shows all."""
    cmds.setAttr(groom + ".displayLevel", -1 if level is None else int(level))


def color_guides_by_map(groom, name=None):
    """Color guides by a painted map (black 0 to white 1); None goes back to level colors."""
    if name:
        cmds.setAttr(groom + ".guideColorLayer", name, type="string")
    cmds.setAttr(groom + ".guideColorMode", 1 if name else 0)


# ---------------------------------------------------------------------------
# Resampling, per-guide attributes, the stored hierarchy, guides from Alembic


def resample_guides(groom, segments, guides=None, group=None, level=None):
    """Give guides (default: the selected ones, else every guide) `segments` segments evenly spaced
    along the smooth curve through their points, keeping their shape. One undo step; returns the
    number of guides resampled."""
    return cmds.groomLabGuides(groom, resample=segments, **_targets(guides, group, level))


def guide_segments(groom, guides=None, group=None, level=None):
    """The segment counts (points - 1) of guides (default: the selected ones, else every guide)."""
    return list(cmds.groomLabGuides(groom, pointCounts=True, **_targets(guides, group, level)) or [])


def preview_resample_guides(groom, segments, guides=None, group=None, level=None):
    """Show guides (as resample_guides picks them) resampled to `segments`, live, without changing
    the groom or making an undo step: always from the stored guides, so previews never compound.
    resample_guides commits; end_guide_preview drops the preview. Returns the guides previewed."""
    return cmds.groomLabGuides(groom, previewResample=segments, **_targets(guides, group, level))


def end_guide_preview(groom):
    """Drop a preview_resample_guides preview: the groom shows its stored guides again."""
    cmds.groomLabGuides(groom, endPreview=True)


def set_guide_attribute(groom, name, value, guides=None, group=None, level=None):
    """Set a per-guide attribute (children inherit it; Interpolate blends it into a strand channel)."""
    return cmds.groomLabGuides(groom, setAttribute=name, value=value, **_targets(guides, group, level))


def get_hierarchy(groom):
    """Return the stored guide hierarchy as a dict (empty dict when none)."""
    text = cmds.getAttr(groom + ".guideData")
    return json.loads(text) if text else {}


def import_guides_from_alembic(groom, path, object_name=None, samples=24):
    """Take the groom's guides from an Alembic file instead of connected curves.

    Replaces the graph's "guides" node with an ImportCurves node reading
    `path` (all curves, or those under `object_name`) and roots them on the
    groom's scalp. Animated files are read at the scene time.
    """
    graph = get_graph(groom, expand_default=True)
    params = {"path": portable_path(path), "samples": samples}
    if object_name:
        params["object"] = object_name

    nodes = [n for n in graph["nodes"] if n["name"] != "guides"]
    nodes.append({"name": "guides", "type": "ImportCurves", "params": params})
    graph["nodes"] = nodes

    connections = [c for c in graph.get("connections", []) if c["to"] != "guides"]
    if any(n["name"] == "scalp" for n in nodes):
        connections.append({"from": "scalp", "to": "guides", "input": "scalp"})
    graph["connections"] = connections
    set_graph(groom, graph)
