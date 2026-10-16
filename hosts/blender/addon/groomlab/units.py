# SPDX-License-Identifier: GPL-3.0-or-later
"""Param units in Blender's UI. The groom keeps every length in centimeters (as Maya and .groom
files do, docs/BLENDER.md); the panels show lengths and widths in scene units (Blender's own
length display: meters, millimeters... per the scene's unit settings), per-length rates per scene
unit, densities per square centimeter (hairs per cm², what hair density is quoted in), and the
spec's other units with a fixed suffix."""

import json

SCENE_LENGTH = {"length", "width"}
SUFFIX = {"density": "/cm²", "angle": "°", "turns": " turns", "seconds": " s", "pixels": " px"}


def meters_per_unit(scene):
    scale = float(scene.unit_settings.scale_length) if scene is not None else 1.0
    return scale if scale > 0.0 else 1.0


def cm_per_unit(scene):
    return 100.0 * meters_per_unit(scene)


def effective(spec, value_of):
    """What a param measures for its node now: its spec's unit while its unitWhen holds (graph.h
    ParamSpec::unitWhen: Noise's frequency is per scene unit in world space, per strand length in
    strand space), else "" (unitless). `value_of(param)`: the node's value of another param."""
    unit = spec.get("unit") or ""
    when = spec.get("unitWhen")
    if unit and when:
        v = value_of(when.get("param"))
        text = ("true" if v else "false") if isinstance(v, bool) else v if isinstance(v, str) else json.dumps(v)
        if text not in (when.get("in") or []):
            return ""
    return unit


def to_ui(unit, value, scene):
    """A param's stored value (centimeters) as the panel shows it."""
    if unit in SCENE_LENGTH:
        return value / cm_per_unit(scene)
    if unit == "perLength":
        return value * cm_per_unit(scene)
    return value


def from_ui(unit, value, scene):
    if unit in SCENE_LENGTH:
        return value * cm_per_unit(scene)
    if unit == "perLength":
        return value / cm_per_unit(scene)
    return value


def suffix(unit, scene):
    if unit == "perLength":
        metric = scene is not None and scene.unit_settings.system != "NONE" and abs(meters_per_unit(scene) - 1.0) < 1e-9
        return " /m" if metric else " /unit"
    return SUFFIX.get(unit, "")
