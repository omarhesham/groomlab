# SPDX-License-Identifier: Apache-2.0
"""Sculpting brushes: a scripted stroke, and the interactive brush tool."""

import json

from maya import cmds

from groomlab.base import ensure_plugin


BRUSH_MODES = ("comb", "slide", "smooth", "straighten", "lift", "twist", "curl", "attract", "direction", "noise", "length",
               "cut", "tube", "erase", "paint", "clump", "place", "draw", "part", "flow")
BRUSH_CONTEXT = "groomLabBrushCtx1"
# The Width and Density brushes became Paint on a setting's map (paint_param; Paint tab > + New Map >
# for a setting): scripts naming them hear where they went.
REMOVED_MODES = {"width": ("width", "width"), "density": ("scatter", "density")}


def _check_mode(mode):
    if mode in REMOVED_MODES:
        node, param = REMOVED_MODES[mode]
        raise ValueError("the %s brush is gone: paint the map %s.%s reads with the Paint brush instead, "
                         "groomlab.paint_param(groom, %r, %r) (Groom Tools: Paint tab > + New Map > for a setting > %s; "
                         "Ctrl paints toward 0%s)" % (mode.capitalize(), node, param, node, param, mode.capitalize(),
                                                      ", which removes hair" if mode == "density" else ""))
    if mode not in BRUSH_MODES:
        raise ValueError("mode must be one of %s" % (BRUSH_MODES,))


def brush_stroke(groom, mode, points, radius=0.25, strength=1.0, level=0, group=None, propagate=False,
                 length=0.5, segments=12, tube_radius=0.1, selected_only=False, symmetry=None, symmetry_center=0.0,
                 attribute=None, value=1.0, match_neighbors=True, collide=True, part_name="part", stretch=False,
                 targets="guides", paint_kind="guides", paint_texture="", paint_resolution=1024, hardness=0.0,
                 smooth_strength=1.0, paint_color=None, paint_channels="rgba", strength_profile=None,
                 falloff_profile=None, solo_new_guides=False):
    """Apply one scripted brush stroke (one undo step) to a groom's guides, parts or flow arrows.

    `points` is the stroke path in world space; each point stamps a brush
    sphere of `radius`.
    - comb drags along the path; with `stretch` guides get longer or shorter
      with the drag instead of keeping their length.
    - smooth, straighten, lift, twist, curl, attract, length, cut, tube and erase
      act where the path passes. direction turns guides about their roots toward the
      path's direction (negative strength: away); noise adds smooth random jitter along
      them, seeded per guide (negative strength smooths it out). Strength is signed: negative length cuts,
      negative lift lays guides down, negative attract pushes apart, and twist
      and curl turn either way.
    - place adds a guide at the scalp point nearest each point; with
      `match_neighbors` it copies the shape of the nearest guides, else it
      stands `length` tall. draw makes one guide shaped like the points,
      rooted at the first. With `solo_new_guides` the new guides stay unfrozen and every
      other guide is frozen (the freeze map), in the same undo step.
    - part adds a part through the points (projected onto the scalp),
      named `part_name` (made unique); symmetry adds its mirror image.
    - paint moves the per-guide `attribute` toward `value` (a painted map);
      clump is paint on the "clump" map (see clump_paint). A color map (vertices or texture,
      `paint_kind`) is painted toward `paint_color` (r, g, b[, a]) in the channels of
      `paint_channels` ("rgba" or a subset: the others keep their values, for packed masks).
    - a setting's map (density, width...) is painted with paint on the map paint_param makes and
      links (the Width and Density brushes are gone: paint_param(groom, "scatter", "density")).

    `strength_profile` (keys [(position, value), ...] or a ramp's JSON) scales the shaping
    brushes along each guide, root (0) to tip (1): tips more than roots, or the reverse (None:
    the same all along). `falloff_profile` draws the falloff as a curve instead, the weight
    from the brush's center (0) to its edge (1) (None: `hardness`'s smooth falloff).

    `selected_only` limits the stroke to the selected guides, scaled by the
    soft selection; `symmetry` ("x", "y" or "z") repeats it mirrored across
    the plane at `symmetry_center`, or ("topology") across the scalp's own
    surface: its left and right vertices matched once on its rest pose, so a
    head turned, tilted or not quite symmetric mirrors as it looks. `collide` keeps guides out of the scalp.
    `targets` ("guides,parts,flow", any subset, or "none") says what the shaping
    brushes act on: comb slides part points and flow arrows along the
    scalp, smooth and straighten relax them, twist turns arrows, length sizes
    whorls, attract gathers them, cut splits parts and erase removes them.
    """
    _check_mode(mode)
    kwargs = {"mode": mode, "radius": radius, "strength": strength, "level": level, "propagate": propagate,
              "point": [tuple(p) for p in points], "length": length, "segments": segments, "tubeRadius": tube_radius,
              "selectedOnly": selected_only, "symmetry": symmetry or "none", "symmetryCenter": symmetry_center,
              "value": value, "matchNeighbors": match_neighbors, "collide": collide, "partName": part_name,
              "stretch": stretch, "targets": targets, "hardness": float(hardness), "smoothStrength": float(smooth_strength),
              "strengthProfile": curve_text(strength_profile), "falloffProfile": curve_text(falloff_profile),
              "soloNewGuides": bool(solo_new_guides)}
    if group:
        kwargs["group"] = group
    if attribute:
        kwargs["attribute"] = attribute
    if paint_kind != "guides":
        kwargs.update(paintKind=paint_kind, paintTexture=paint_texture or "", paintResolution=int(paint_resolution))
    kwargs.update(_paint_color_flags(paint_color, paint_channels))
    cmds.groomLabBrushStroke(groom, **kwargs)


def curve_text(curve):
    """A brush curve as the brush's flags take it: a ramp's JSON ("" for None)."""
    if curve is None:
        return ""
    if isinstance(curve, str):
        return curve
    return json.dumps([[float(t), float(v)] for t, v in curve])


def _rgba(color):
    """(r, g, b, a) floats 0-1 from (r, g, b) or (r, g, b, a) (alpha 1 when left out)."""
    values = [float(c) for c in color]
    if len(values) == 3:
        values.append(1.0)
    if len(values) != 4:
        raise ValueError("a color is (r, g, b) or (r, g, b, a), not %r" % (color,))
    return tuple(min(max(c, 0.0), 1.0) for c in values)


def _paint_color_flags(color, channels):
    """The brush's -paintColor and -paintChannels flags (a color None leaves the tool's color)."""
    flags = {"paintChannels": channels or "rgba"}
    if color is not None:
        flags["paintColor"] = _rgba(color)
    return flags


def brush_tool(mode="comb", radius=60, strength=1.0, level=0, group="", propagate=False, length=0.5,
               paint_kind="guides", paint_texture="", paint_resolution=1024, hardness=0.0, smooth_strength=1.0,
               segments=12, tube_radius=0.1, selected_only=False, symmetry=None, symmetry_center=0.0,
               attribute=None, value=1.0, match_neighbors=True, spacing=0.1, collide=True, pressure=True,
               part_name="part", erase_guides=True, erase_parts=True, erase_flow=True, stretch=False, flow_sink="",
               targets="guides", paint_color=None, paint_channels="rgba", mask_by_scalp=True, strength_profile=None,
               falloff_profile=None, solo_new_guides=False, activate=True):
    """Configure the interactive GroomLab brush (radius in pixels) and make it the current tool.

    Select the groom first; then drag over it in the viewport. Each drag is one undo step.
    Options as in brush_stroke, plus:
    - place: click for one guide; drag to place many, no closer than `spacing`
      (scene units) to any root of the group.
    - draw: press on the scalp and draw the guide's shape.
    - part: draw a part freehand on the scalp; it is added on release.
    - flow: drag on the scalp to lay flow arrows along the stroke (every half a
      radius); arrows under the brush turn toward the stroke. Ctrl erases them.
      With `flow_sink` (whorl, whorlcw, converge, part) a click places a sink
      of that kind the size of the brush instead; Ctrl-click removes sinks.
    - erase: removes what the brush passes over: guides, parts and flow arrows
      (`erase_guides`, `erase_parts`, `erase_flow`). The part tool with Ctrl erases parts.
    - `pressure`: tablet pen pressure scales strength.
    - `mask_by_scalp`: what the scalp hides from the camera is left alone (guides, parts, flow
      arrows and painted vertices on the far side of the head); off, strokes reach through it.
    - `targets`: what the shaping brushes act on, "guides,parts,flow" or a subset
      (Acts on, in the Groom Tools' brush settings): comb slides part points and flow
      arrows along the scalp, smooth and straighten relax them, twist turns arrows.
    - `paint_color` (r, g, b[, a]) and `paint_channels` ("rgba" or a subset): what paint does to a
      color map (Ctrl: toward 1 - the color in those channels; Shift+drag smooths them).
    - `strength_profile` and `falloff_profile`: the brush's curves, as in brush_stroke.
    - `solo_new_guides` (place, draw): each stroke's new guides stay unfrozen and every other
      guide is frozen, so the brushes after work on them alone (the Groom Tools turn it on).
    - Shift+click a guide with a brush: that guide is soloed (unfrozen, every other frozen, one
      undo step) and its group brushed; Shift+click off a guide (bare scalp or empty space) unfreezes every guide.
    """
    _check_mode(mode)
    ensure_plugin()
    if not cmds.contextInfo(BRUSH_CONTEXT, exists=True):
        cmds.groomLabBrushCtx(BRUSH_CONTEXT)
    kwargs = {"paintKind": paint_kind, "paintTexture": paint_texture or "", "paintResolution": int(paint_resolution),
              "hardness": float(hardness), "smoothStrength": float(smooth_strength),
              "strengthProfile": curve_text(strength_profile), "falloffProfile": curve_text(falloff_profile),
              "soloNewGuides": bool(solo_new_guides)}
    kwargs.update(_paint_color_flags(paint_color, paint_channels))
    if attribute:
        kwargs["attribute"] = attribute
    cmds.groomLabBrushCtx(BRUSH_CONTEXT, edit=True, mode=mode, radius=radius, strength=strength, level=level,
                          group=group, propagate=propagate, length=length, segments=segments, tubeRadius=tube_radius,
                          selectedOnly=selected_only, symmetry=symmetry or "none", symmetryCenter=symmetry_center,
                          value=value, matchNeighbors=match_neighbors, spacing=spacing, collide=collide,
                          pressure=pressure, partName=part_name, eraseGuides=erase_guides, eraseParts=erase_parts,
                          eraseFlow=erase_flow, stretch=stretch, flowSink=flow_sink or "", targets=targets,
                          maskByScalp=mask_by_scalp, **kwargs)
    if activate:
        cmds.setToolTo(BRUSH_CONTEXT)
    return BRUSH_CONTEXT
