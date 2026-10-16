# SPDX-License-Identifier: Apache-2.0
"""Real-world units for GroomLab's UI.

Groom data is in scene units, which are centimeters: Maya's API (and so the
scalp, guides and strands GroomLab reads) always works in centimeters,
whatever the UI's linear unit. Params that measure something say so in their
spec ("unit": "length", "width" or "density"), and the UI shows them in the
units picked here, stored per user. The spec's other units show with a fixed
suffix: a fraction as a percentage (x100), angles in degrees, turns, seconds,
pixels, and a per-length rate per the length unit picked:

    from groomlab import units
    units.set_unit("width", "um")      # widths in micrometers (hair: ~50-120 um)
    units.to_display("width", 0.006)   # -> 60.0
"""

from maya import cmds

# kind -> [(key, label, display units per scene unit)]
UNITS = {
    "length": [("cm", "cm", 1.0), ("mm", "mm", 10.0), ("m", "m", 0.01), ("in", "in", 1.0 / 2.54),
               ("scene", "units", 1.0)],
    "width": [("um", u"µm", 1e4), ("mm", "mm", 10.0), ("cm", "cm", 1.0), ("scene", "units", 1.0)],
    "density": [("cm2", u"/cm²", 1.0), ("mm2", u"/mm²", 0.01), ("in2", u"/in²", 2.54 ** 2),
                ("scene", "/unit²", 1.0)],
}
DEFAULTS = {"length": "cm", "width": "um", "density": "cm2"}
# What real hair measures, in scene units: sliders of that kind center on it.
TYPICAL = {"density": 200.0}  # hairs per cm² on a human scalp (roughly 150-300)
# The spec's units no one picks (graph.h ParamSpec::unit): kind -> (key, label, factor).
FIXED = {"fraction": ("percent", "%", 100.0), "angle": ("deg", u"°", 1.0), "turns": ("turns", "turns", 1.0),
         "seconds": ("s", "s", 1.0), "pixels": ("px", "px", 1.0)}


def _var(kind):
    return "groomLabUnit_" + kind


def unit(kind):
    """(key, label, factor) for a kind: display value = scene value x factor. A per-length rate
    ("perLength") follows the length unit: per mm is a tenth of per cm."""
    if kind in FIXED:
        return FIXED[kind]
    if kind == "perLength":
        key, text, f = unit("length")
        return ("per_" + key, "/" + ("unit" if key == "scene" else text), 1.0 / f)
    choices = UNITS[kind]
    key = cmds.optionVar(query=_var(kind)) if cmds.optionVar(exists=_var(kind)) else DEFAULTS[kind]
    return next((c for c in choices if c[0] == key), next(c for c in choices if c[0] == DEFAULTS[kind]))


def set_unit(kind, key):
    if key not in [c[0] for c in UNITS[kind]]:
        raise ValueError("no %s unit %r (%s)" % (kind, key, ", ".join(c[0] for c in UNITS[kind])))
    cmds.optionVar(stringValue=(_var(kind), key))


def shown(kind):
    """Whether the UI shows a kind's values with a unit (else unitless, or a color)."""
    return kind in UNITS or kind in FIXED or kind == "perLength"


def factor(kind):
    return unit(kind)[2] if shown(kind) else 1.0


def label(kind):
    return unit(kind)[1] if shown(kind) else ""


def soft_range(kind):
    """A slider range centered on the typical value of a kind, in display units; None when the
    kind has no typical value (the range then comes from the param's default)."""
    if kind not in TYPICAL:
        return None
    return 0.0, 2.0 * TYPICAL[kind] * factor(kind)


def to_display(kind, value):
    return value * factor(kind)


def from_display(kind, value):
    return value / factor(kind)
