# SPDX-License-Identifier: Apache-2.0
"""Expressions: what they can read (for the editor's completion and help), and a library of
them to reuse across scenes.

An Expression node keeps its text and its own variables in the groom's graph, so they are
saved with the scene. The library is for reuse: a saved expression is a JSON file
({"name", "description", "expression", "variables", "min", "max"}) looked up, in order, in:
  1. every directory on GROOMLAB_EXPRESSION_PATH (os.pathsep-separated): a studio's shared set,
  2. the user's folder (<Maya user app dir>/groomlab/expressions), where save_expression writes,
     so what you save is there in every new scene,
  3. the examples shipped with GroomLab (<module>/expressions).
An expression in an earlier directory hides one of the same name in a later one.

    from groomlab import expressions
    expressions.list_expressions()                      # [{"name": ..., "expression": ...}, ...]
    expressions.apply_expression(groom, "expression1", "Stripes around the scalp")
    expressions.save_expression("My mask", "$y > 0.5", variables={"soft": 0.2})
"""

import json
import os
import re

SHIPPED_DIR = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", "..", "expressions"))

# (name, what it is). Written with a `$` in the editor; the language takes them with or without.
VARIABLES = [
    ("length", "The strand's length, along it"),
    ("longest", "The longest strand's length (the whole set)"),
    ("reach", "Root to tip, in a straight line"),
    ("straightness", "reach / length: 1 for a straight strand"),
    ("lift", "How far the strand points off the scalp: 1 straight out, 0 along it, below 0 into it"),
    ("tipx", "Tip position x"), ("tipy", "Tip position y"), ("tipz", "Tip position z"),
    ("u", "Root U on the scalp (0 without UVs)"), ("v", "Root V on the scalp (0 without UVs)"),
    ("x", "Root position x"), ("y", "Root position y"), ("z", "Root position z"),
    ("nx", "Root normal x"), ("ny", "Root normal y"), ("nz", "Root normal z"),
    ("rand", "A stable random number per strand, 0 to 1"),
    ("id", "The strand's ID (its low 24 bits): a whole number, stable as strands come and go"),
    ("faceId", "The scalp triangle the root sits on"),
    ("index", "The strand's place in the set, 0 to 1"),
    ("t", "Along the strand: 0 at the root to 1 at the tip, by length (per point)"),
    ("pt", "Along the strand: 0 at the root to 1 at the tip, by point (per point)"),
    ("px", "The point's position x (per point)"), ("py", "The point's position y (per point)"),
    ("pz", "The point's position z (per point)"),
    ("pointIndex", "The point's index on its strand (per point)"),
    ("frame", "The frame evaluated (also F); an expression reading it changes from frame to frame"),
    ("F", "The frame evaluated (as frame)"),
    ("time", "The time evaluated, in seconds"),
    ("P", "Vector: the root's position [x, y, z] (P.x, P.y, P.z or P[0]...)"),
    ("N", "Vector: the root's normal [nx, ny, nz]"),
    ("Pref", "Vector: the root on the scalp's rest pose (P without one)"),
    ("tip", "Vector: the tip's position [tipx, tipy, tipz]"),
    ("pointP", "Vector: the point's position [px, py, pz] (per point)"),
    ("pi", "3.14159..."),
]
# Channels most grooms carry (any strand channel reads by its name).
CHANNELS = [
    ("clumpId", "The strand's clump (Clump writes it)"),
    ("guideInfluence", "How close the strand is to its guides (Interpolate writes it)"),
    ("flyaway", "1 for stray hairs (Flyaway writes it)"),
    ("melanin", "The strand's pigment (HairColor writes it)"),
]
# (name, arguments, what it does). SeExpr's builtins, as XGen has them (web docs: Expressions).
FUNCTIONS = [
    ("sin", "x", "Sine (radians)"), ("cos", "x", "Cosine (radians)"), ("tan", "x", "Tangent (radians)"),
    ("asin", "x", "Arc sine"), ("acos", "x", "Arc cosine"), ("atan", "x", "Arc tangent"),
    ("atan2", "y, x", "Angle of (x, y)"), ("sind", "x", "Sine (degrees)"), ("cosd", "x", "Cosine (degrees)"),
    ("tand", "x", "Tangent (degrees)"), ("asind", "x", "Arc sine, in degrees"), ("acosd", "x", "Arc cosine, in degrees"),
    ("atand", "x", "Arc tangent, in degrees"), ("atan2d", "y, x", "Angle of (x, y), in degrees"),
    ("sinh", "x", "Hyperbolic sine"), ("cosh", "x", "Hyperbolic cosine"), ("tanh", "x", "Hyperbolic tangent"),
    ("asinh", "x", "Inverse hyperbolic sine"), ("acosh", "x", "Inverse hyperbolic cosine"),
    ("atanh", "x", "Inverse hyperbolic tangent"),
    ("abs", "x", "Absolute value"), ("sqrt", "x", "Square root"), ("cbrt", "x", "Cube root"),
    ("pow", "a, b", "a to the power b (also a ^ b)"), ("exp", "x", "e to the power x"),
    ("log", "x", "Natural logarithm"), ("log10", "x", "Base-10 logarithm"), ("hypot", "x, y", "sqrt(x*x + y*y)"),
    ("floor", "x", "Round down"), ("ceil", "x", "Round up"), ("round", "x", "The nearest whole number"),
    ("trunc", "x", "The whole part"), ("fract", "x", "The part after the point"), ("sign", "x", "-1, 0 or 1"),
    ("min", "a, b", "The smaller"), ("max", "a, b", "The larger"),
    ("mod", "a, b", "Remainder, always positive (GroomLab's)"), ("fmod", "a, b", "Remainder with a's sign (also a % b)"),
    ("radians", "deg", "Degrees to radians (also rad)"), ("degrees", "rad", "Radians to degrees (also deg)"),
    ("invert", "x", "1 - x (also ~x)"),
    # Ranges and blends (on vectors too, component by component).
    ("clamp", "x, lo, hi", "x kept between lo and hi"), ("mix", "a, b, t", "a to b as t goes 0 to 1"),
    ("lerp", "a, b, t", "a to b as t goes 0 to 1 (as mix)"),
    ("step", "edge, x", "0 below edge, 1 from it on"), ("boxstep", "x, a", "0 below a, 1 from it on"),
    ("linearstep", "x, a, b", "0 below a, 1 above b, a straight line in between"),
    ("smoothstep", "x, a, b", "0 below a, 1 above b, smooth in between"),
    ("gaussstep", "x, a, b", "0 below a, 1 above b, a bell-shaped rise in between"),
    ("fit", "x, a1, b1, a2, b2", "x from a1..b1 onto a2..b2 (not clamped)"),
    ("remap", "x, source, range, falloff, interp", "1 within range of source, falling to 0 over the falloff"),
    ("compress", "x, lo, hi", "0-1 onto lo..hi"), ("expand", "x, lo, hi", "lo..hi onto 0-1"),
    ("gamma", "x, g", "x to the power 1/g"), ("bias", "x, b", "Pushes 0-1 values up (b > 0.5) or down"),
    ("gain", "x, g", "Pushes 0-1 values toward the ends (g > 0.5) or the middle"),
    ("contrast", "x, c", "More contrast around 0.5 (c > 0.5) or less"),
    # Random and noise (a point p is a vector; a number x is [x, 0, 0]).
    ("rand", "[min, max[, seed]]", "A stable random number per strand: 0-1, or min to max (rand(seed): 0-1 per seed)"),
    ("hash", "a, b, ...", "A random 0-1 value from the numbers alone"),
    ("noise", "p", "Smooth noise, 0 to 1 (also noise(x, y), noise(x, y, z), noise(x, y, z, w))"),
    ("snoise", "p", "Smooth noise, -1 to 1"), ("vnoise", "p", "Vector noise, each -1 to 1"),
    ("cnoise", "p", "Vector noise, each 0 to 1"), ("snoise4", "p, t", "4D noise, -1 to 1"),
    ("vnoise4", "p, t", "4D vector noise, -1 to 1"), ("cnoise4", "p, t", "4D vector noise, 0 to 1"),
    ("pnoise", "p, period", "Noise repeating every period cells, -1 to 1"),
    ("fbm", "p, octaves, lacunarity, gain", "Fractal noise around 0.5 (6, 2, 0.5 when left out)"),
    ("vfbm", "p, octaves, lacunarity, gain", "Fractal vector noise around 0"),
    ("cfbm", "p, octaves, lacunarity, gain", "Fractal vector noise around 0.5"),
    ("turbulence", "p, octaves, lacunarity, gain", "Fractal |noise|, 0.5 and up: creased"),
    ("vturbulence", "p, octaves, lacunarity, gain", "Fractal |vector noise|"),
    ("cturbulence", "p, octaves, lacunarity, gain", "Fractal |vector noise|, 0.5 and up"),
    ("fbm4", "p, t, octaves, lacunarity, gain", "4D fractal noise around 0.5"),
    ("vfbm4", "p, t, octaves, lacunarity, gain", "4D fractal vector noise around 0"),
    ("cfbm4", "p, t, octaves, lacunarity, gain", "4D fractal vector noise around 0.5"),
    ("cellnoise", "p", "One random 0-1 value per unit cell"),
    ("ccellnoise", "p", "One random 0-1 vector per unit cell"),
    ("voronoi", "p, type, jitter, fbmScale, fbmOctaves, fbmLacunarity, fbmGain",
     "Cells: type 1 a value per cell, 2 nearest distance, 3 second nearest, 4 their gap, 5 edges"),
    ("cvoronoi", "p, type, jitter, fbmScale, fbmOctaves, fbmLacunarity, fbmGain", "voronoi with a random color per cell"),
    ("pvoronoi", "p, jitter, fbmScale, fbmOctaves, fbmLacunarity, fbmGain", "The nearest cell point"),
    # Vectors.
    ("dot", "a, b", "Dot product of two vectors"), ("cross", "a, b", "Cross product: a vector"),
    ("length", "v", "A vector's length"), ("normalize", "v", "The vector at length 1 (also norm)"),
    ("distance", "a, b", "Distance between two points (also dist)"),
    ("angle", "a, b", "The angle between two vectors (radians)"),
    ("ortho", "a, b", "The unit vector at right angles to both"),
    ("rotate", "v, axis, angle", "v turned around an axis (radians)"),
    ("up", "v, upvec", "v turned so that +Y points along upvec"),
    # Colors.
    ("rgbtohsl", "c", "RGB to hue (0-1), saturation, lightness"), ("hsltorgb", "c", "Hue, saturation, lightness to RGB"),
    ("hsi", "c, h, s, i, mask", "Hue shift (degrees), saturation and intensity scale"),
    ("midhsi", "c, h, s, i, mask, falloff, interp", "hsi weighted toward the mask's ends"),
    ("saturate", "c, amount", "Saturation scale"),
    # Curves, picking, maps.
    ("curve", "t, p0, v0, i0, p1, v1, i1, ...",
     "A curve through keys (position, value, interpolation 0 none, 1 linear, 2 smooth, 3 spline, 4 monotone)"),
    ("ccurve", "t, p0, c0, i0, ...", "curve with colors"),
    ("linearcurve", "t, p0, v0, p1, v1, ...", "Straight segments through keys (position, value)"),
    ("spline", "t, y1, y2, y3, y4, ...", "A smooth curve through y2..y(n-1), y1 and yn its end controls"),
    ("pick", "index, lo, hi, weights...", "A whole number lo to hi from a hash of index"),
    ("choose", "t, a, b, ...", "The argument t (0 to 1) falls on"),
    ("wchoose", "t, a, wa, b, wb, ...", "choose, by weights"),
    ("cycle", "index, lo, hi", "lo + index, wrapped into lo to hi"),
    ("map", "\"name\", \"channel\"", "The groom's map of that name at the root (luminance, or r g b a)"),
    ("cmap", "\"name\"", "The map's color as a vector (also vmap)"),
]
NAME = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")


def completions(variables=None, channels=()):
    """(text to insert, what it is) for the editor: the node's variables, the built-in ones,
    channels, then functions."""
    out = []
    for name, value in (variables or {}).items():
        if isinstance(value, (list, tuple)):
            out += [("$%s.%s" % (name, a), "Your vector's %s" % a) for a in "xyz"]
        elif isinstance(value, dict):
            out.append(("$" + name, "Your keyable variable (ctl_%s on the groom)" % value.get("control", "")))
        else:
            out.append(("$" + name, "Your variable (%g)" % value))
    out += [("$" + name, text) for name, text in VARIABLES]
    known = {n for n, _ in CHANNELS}
    out += [("$" + name, text) for name, text in CHANNELS]
    out += [("$" + name, "A channel of the strands") for name in channels if name not in known]
    out += [("%s(" % name, "%s(%s): %s" % (name, args, text)) for name, args, text in FUNCTIONS]
    return out


def check(text, variables=None):
    """What is wrong with an expression ("" when nothing), with the node's own variables; the
    plugin compiles it, so the editor can say so as you type, connected or not."""
    from maya import cmds
    try:
        return cmds.groomLabExpression(check=text, variables=json.dumps(check_variables(variables or {}))) or ""
    except ValueError as e:
        return str(e)


def error_place(message):
    """(line, column), 1-based, that an error message points at, or None."""
    m = re.search(r"line (\d+), column (\d+)", message)
    if m:
        return int(m.group(1)), int(m.group(2))
    m = re.search(r"at position (\d+)", message)
    return (1, int(m.group(1))) if m else None


# ---------------------------------------------------------------------------
# The library

def user_dir():
    import groomlab
    return groomlab.library_user_dir("GROOMLAB_USER_EXPRESSIONS", "expressions")


def search_dirs():
    import groomlab
    return groomlab.library_dirs("GROOMLAB_EXPRESSION_PATH", user_dir, SHIPPED_DIR)


def _file_name(name):
    return re.sub(r"[^A-Za-z0-9_ -]+", "", name).strip().replace(" ", "_") or "expression"


def list_expressions():
    """Every expression in the library, sorted by name; each a dict with "name",
    "description", "expression", "variables", "min", "max", "path" and "shipped"."""
    found = {}
    for d in search_dirs():
        if not os.path.isdir(d):
            continue
        for f in sorted(os.listdir(d)):
            if not f.endswith(".json"):
                continue
            path = os.path.join(d, f)
            try:
                with open(path, encoding="utf-8") as fh:
                    data = json.load(fh)
            except (IOError, OSError, ValueError):
                continue
            name = data.get("name") or f[:-5]
            if name in found:
                continue  # hidden by an earlier directory
            found[name] = dict(name=name, description=data.get("description", ""),
                               expression=data.get("expression", ""), variables=data.get("variables") or {},
                               min=float(data.get("min", 0.0)), max=float(data.get("max", 1.0)),
                               path=path.replace("\\", "/"), shipped=os.path.normpath(d) == SHIPPED_DIR)
    return sorted(found.values(), key=lambda e: (e["shipped"], e["name"].lower()))


def find_expression(name):
    for e in list_expressions():
        if e["name"] == name:
            return e
    raise ValueError("no expression named %r in the library (%s)" % (name, ", ".join(search_dirs())))


def control_name(node, variable):
    """The control (the groom attribute ctl_<name>) a keyable variable of `node` reads."""
    return re.sub(r"[^A-Za-z0-9_]", "_", "%s_%s" % (node, variable))


def check_variables(variables):
    """Variables as the node stores them: {name: number, [x, y, z], or a keyable number
    {"value": v, "control": name}}; raises ValueError."""
    out = {}
    for name, value in (variables or {}).items():
        if not NAME.match(name):
            raise ValueError("%r is not a variable name (letters, digits and _, not starting with a digit)" % name)
        if isinstance(value, (list, tuple)):
            if len(value) != 3:
                raise ValueError("$%s: a vector has three numbers" % name)
            out[name] = [float(c) for c in value]
        elif isinstance(value, dict):
            control = value.get("control") or ""
            if control and not NAME.match(control):
                raise ValueError("$%s: %r is not a control name" % (name, control))
            out[name] = {"value": float(value.get("value", 0.0)), "control": control}
        else:
            out[name] = float(value)
    return out


def save_expression(name, expression, variables=None, description="", minimum=0.0, maximum=1.0, overwrite=False):
    """Save an expression to the user's library (available in every scene); returns the path."""
    if not name.strip():
        raise ValueError("an expression in the library needs a name")
    folder = user_dir()
    if not os.path.isdir(folder):
        os.makedirs(folder)
    path = os.path.join(folder, _file_name(name) + ".json")
    if os.path.exists(path) and not overwrite:
        raise ValueError("the library has an expression named %r" % name)
    data = {"name": name.strip(), "description": description, "expression": expression,
            "variables": check_variables(variables), "min": float(minimum), "max": float(maximum)}
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    return path.replace("\\", "/")


def delete_expression(name):
    """Remove an expression from the user's library (shipped examples stay)."""
    entry = find_expression(name)
    if entry["shipped"]:
        raise ValueError("%s ships with GroomLab and stays" % name)
    os.remove(entry["path"])


def node_values(groom, node):
    """The Expression node's text, variables, min and max, from the groom's graph."""
    from groomlab import get_graph
    for n in get_graph(groom).get("nodes", []):
        if n.get("name") == node:
            params = n.get("params", {})
            try:
                variables = json.loads(params.get("variables") or "{}")
            except ValueError:
                variables = {}
            return {"expression": params.get("expression", "$length / $longest"), "variables": variables,
                    "min": float(params.get("min", 0.0)), "max": float(params.get("max", 1.0))}
    raise ValueError("graph has no node named %r" % node)


def params_for(entry, node=None):
    """An expression (a library entry or node_values) as the Expression node's params; a keyable
    variable reads `node`'s control of its name."""
    variables = check_variables(entry.get("variables"))
    for name, value in variables.items():
        if isinstance(value, dict) and node:
            value["control"] = control_name(node, name)
    return {"expression": entry["expression"], "variables": json.dumps(variables),
            "min": float(entry.get("min", 0.0)), "max": float(entry.get("max", 1.0))}


def apply_expression(groom, node, name):
    """Load library expression `name` into the groom's Expression node `node` (one undo step)."""
    import groomlab
    entry = find_expression(name)
    with groomlab.undo_chunk("expression"):
        for key, value in params_for(entry, node=node).items():
            groomlab.set_param(groom, node, key, value)
    return entry
