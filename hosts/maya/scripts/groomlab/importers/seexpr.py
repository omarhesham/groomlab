# SPDX-License-Identifier: Apache-2.0
"""XGen's SeExpr attribute values read as GroomLab values: a constant, a map read, or an expression.

XGen stores every expression attribute as text, with its slider range as a comment and newlines
written as the two characters \\n in .xgen files: "$a=1.5;#0.05,5.0\\n$a". GroomLab's expressions
take SeExpr as XGen writes it (groomlab_core's expression.h: XGen's variables, map() of a
${DESC}/paintmaps path), so an expression goes through as it is; what can be simpler is made so:

    simplify("$a=1.5;#0.05,5.0\\n$a")             -> ("const", 1.5)
    simplify("$a=map('${DESC}/paintmaps/len/');#3dpaint,10\\n$a")
                                                  -> ("map", "${DESC}/paintmaps/len/", 1.0)
    simplify("0.5*map('${DESC}/paintmaps/d/')")   -> ("map", "${DESC}/paintmaps/d/", 0.5)
    simplify("rand(0.5, 1.0)")                    -> ("expr", "rand(0.5, 1.0)")

ramp() reads XGen's ramp text, rampUI(position,value,interpolation:...).
"""

import re

_NUMBER = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"
_MAP = re.compile(r"""\b(c?map)\(\s*(['"])(.*?)\2\s*(?:,[^)]*)?\)""")
# XGen variables GroomLab reads with another meaning: noted when an expression uses them.
DIFFERENT_VARIABLES = {
    "u": "XGen's $u is the root's position across its face, GroomLab's the scalp's U",
    "v": "XGen's $v is the root's position across its face, GroomLab's the scalp's V",
}


def unescape(text):
    """An attribute's text with its escaped newlines (\\n as two characters) as newlines."""
    return (text or "").replace("\\n", "\n")


def strip_comments(text):
    """The text without # comments (outside quotes)."""
    out = []
    for line in unescape(text).split("\n"):
        quote = None
        for i, ch in enumerate(line):
            if quote:
                if ch == quote:
                    quote = None
            elif ch in "'\"":
                quote = ch
            elif ch == "#":
                line = line[:i]
                break
        out.append(line)
    return "\n".join(out)


def _statements(text):
    parts = []
    for chunk in re.split(r"[;\n]", text):
        chunk = chunk.strip()
        if chunk:
            parts.append(chunk)
    return parts


def split(text):
    """(assignments [(name, expression)], the last expression) of an expression's text, comments
    gone; (None, the text) when it has blocks (if/else) it can't take apart."""
    clean = strip_comments(text)
    if re.search(r"(?<!\$)\{", clean):  # a block (an XGen ${TOKEN} is not one)
        return None, clean.strip()
    assigns, last = [], ""
    for s in _statements(clean):
        m = re.match(r"^\$?([A-Za-z_]\w*)\s*=(?!=)\s*(.+)$", s)
        if m:
            assigns.append((m.group(1), m.group(2).strip()))
        else:
            last = s
    if not last and assigns:
        last = "$" + assigns[-1][0]
    return assigns, last


def _inline(assigns, expr):
    """`expr` with $variables assigned once before it written out (to a depth of 8)."""
    values = {}
    for name, value in assigns:
        values[name] = value
    for _ in range(8):
        m = re.match(r"^\$?([A-Za-z_]\w*)$", expr.strip())
        if not m or m.group(1) not in values:
            break
        expr = values[m.group(1)]
    return expr.strip()


def _number(text):
    text = text.strip()
    while text.startswith("(") and text.endswith(")"):
        text = text[1:-1].strip()
    return float(text) if re.match(r"^%s$" % _NUMBER, text) else None


def simplify(text):
    """("const", value), ("map", path, scale) or ("expr", GroomLab expression text)."""
    if text is None:
        return ("const", 0.0)
    if isinstance(text, (int, float)):
        return ("const", float(text))
    assigns, last = split(text)
    if assigns is None:
        return ("expr", strip_comments(text).strip())
    value = _inline(assigns, last)
    n = _number(value)
    if n is not None:
        return ("const", n)
    m = re.match(r"^(?:(%s)\s*\*\s*)?map\(\s*(['\"])(.*?)\2\s*\)(?:\s*\*\s*(%s))?$" % (_NUMBER, _NUMBER), value)
    if m:
        scale = float(m.group(1) or 1.0) * float(m.group(4) or 1.0)
        return ("map", m.group(3), scale)
    # Statements kept (GroomLab's expressions take them), comments gone, one per line.
    body = "\n".join(["$%s = %s;" % (k, v) for k, v in assigns] + [last])
    return ("expr", body)


def map_paths(text):
    """The paths an expression's map() and cmap() calls name, in order, each once."""
    out = []
    for m in _MAP.finditer(strip_comments(text or "")):
        if m.group(3) not in out:
            out.append(m.group(3))
    return out


def variables(text):
    """The $names an expression reads (not the ones it assigns)."""
    clean = strip_comments(text or "")
    assigned = set(re.findall(r"\$([A-Za-z_]\w*)\s*=(?!=)", clean))
    return sorted(set(re.findall(r"\$([A-Za-z_]\w*)", clean)) - assigned)


def compose(text, template):
    """An expression whose last value goes through `template` ("max(0, 1 - ({}) / $length)"): its
    assignments first, then the template of its last expression."""
    assigns, last = split(text)
    if assigns is None:
        raise ValueError("an expression with blocks can't be composed")
    lines = ["$%s = %s;" % (k, v) for k, v in assigns]
    return "\n".join(lines + [template.format(last)])


def ramp(text):
    """XGen's rampUI(p,v,i:p,v,i...) as [(position, value)] and whether every span is linear
    (interpolation 1); a bare number is a flat ramp; None when it is neither."""
    if text is None:
        return None
    t = str(text).strip()
    n = _number(t)
    if n is not None:
        return [(0.0, n), (1.0, n)], True
    m = re.match(r"^rampUI\((.*)\)$", t)
    if not m:
        return None
    keys, linear = [], True
    for part in m.group(1).split(":"):
        nums = [x for x in part.split(",") if x.strip()]
        if len(nums) < 2:
            continue
        keys.append((float(nums[0]), float(nums[1])))
        if len(nums) > 2 and int(float(nums[2])) != 1:
            linear = False
    return (sorted(keys) or None), linear


def ramp_at(keys, t):
    """A ramp's value at t, linear between keys (enough to combine ramps)."""
    if not keys:
        return 1.0
    if t <= keys[0][0]:
        return keys[0][1]
    for (p0, v0), (p1, v1) in zip(keys, keys[1:]):
        if t <= p1:
            return v0 if p1 <= p0 else v0 + (v1 - v0) * (t - p0) / (p1 - p0)
    return keys[-1][1]
