# SPDX-License-Identifier: GPL-3.0-or-later
"""The groom's node graph in the panel: its nodes as a list (the main chain from the roots to the
output first), the selected node's params as widgets (node_spec: groups, Advanced folded,
showWhen, units), presets, and simple edits (add, remove, move along the chain) through the
helper's edit_graph. A full node editor is the Maya plug-in's and the app's."""

import json

from . import groom, process, props, units
from .helper_client import HelperError

STRAND_TYPES = {"Strands", "Curves"}
_specs = {}
_presets = []


def _call(cmd, **args):
    return process.call(cmd, **args)


def operator_specs():
    if not _specs:
        for spec in _call("operator_specs")["operators"]:
            _specs[spec["type"]] = spec
    return _specs


def presets(refresh=False):
    """[(name, title, description, category)] of the shipped and user presets (cached)."""
    global _presets
    if refresh or not _presets:
        _presets = [(p["name"], p.get("title") or p["name"], p.get("description", ""), p.get("category", ""))
                    for p in _call("list_presets")["presets"]]
    return _presets


_library = {"at": 0.0, "nodes": []}


def library_nodes(refresh=False):
    """The node library's compound nodes (the helper's library_nodes: GROOMLAB_NODE_PATH, the user's
    Documents/GroomLab/nodes that Maya and the app share, the shipped ones) as Add menu entries:
    dicts with type ("node:<id>"), id, version, label, categoryTitle, description. Read again at
    most every two seconds (the Add menu asks as it draws)."""
    import time
    now = time.time()
    if refresh or now - _library["at"] > 2.0:
        try:
            _library["nodes"] = _call("library_nodes")["nodes"]
        except Exception:  # a library that cannot be read never stops the menu
            _library["nodes"] = []
        _library["at"] = now
    return _library["nodes"]


def cached_presets():
    return _presets


# ---- The graph's shape --------------------------------------------------------------------------

def main_chain(graph):
    """The nodes from the strands' source to the output, following each node's strands input; and
    each node's inputs {input: from}."""
    into = {}
    for c in graph.get("connections", []):
        into.setdefault(c["to"], {})[c["input"]] = c["from"]
    chain, seen = [], set()
    node = graph.get("output")
    while node and node not in seen:
        seen.add(node)
        chain.append(node)
        ins = into.get(node, {})
        node = ins.get("strands") or ins.get("a") or ins.get("shafts")
    chain.reverse()
    return chain, into


def _input_from(into, node, source):
    for name, frm in into.get(node, {}).items():
        if frm == source:
            return name
    return "strands"


def _node_error(name, errors):
    for e in errors:
        if e.startswith(name + ":") or ("'%s'" % name) in e:
            return e
    return ""


def refresh(scalp, rt=None):
    """The node list again from the helper's graph (after any change of the graph)."""
    rt = rt or groom.ensure(scalp)
    graph = _call("get_graph", session=rt.session)["graph"]
    g = scalp.groomlab
    g.graph_json = json.dumps(graph)
    types = {n["name"]: n["type"] for n in graph.get("nodes", [])}
    chain, _ = main_chain(graph)
    order = [n for n in chain if n in types] + [n for n in types if n not in chain]
    active = g.nodes[g.node_index].name if 0 <= g.node_index < len(g.nodes) else None
    with props.quiet():
        g.nodes.clear()
        for name in order:
            item = g.nodes.add()
            item.name = name
            item.type = types[name]
            item.in_chain = name in chain
            item.is_output = name == graph.get("output")
            item.unsupported = groom.UNSUPPORTED.get(types[name], "")
        if active in order:
            g.node_index = order.index(active)
        else:
            g.node_index = max(0, len([n for n in chain if n in types]) - 2)
    update_notes(scalp, rt)
    refresh_params(scalp)
    from . import graph_tree
    graph_tree.graph_changed(scalp)  # the Graph Editor's tree follows


def update_notes(scalp, rt=None):
    rt = rt or groom.runtime_if_any(scalp)
    if rt is None:
        return
    for item in scalp.groomlab.nodes:
        note = rt.notes.get(item.name, "")
        error = _node_error(item.name, rt.errors)
        if item.note != note:
            item.note = note
        if item.error != error:
            item.error = error


def active_node(scalp):
    g = scalp.groomlab
    return g.nodes[g.node_index] if 0 <= g.node_index < len(g.nodes) else None


# ---- Params -----------------------------------------------------------------------------------

def _set_widget(p, value, scene):
    kind, unit = p.kind, p.unit
    p.raw = json.dumps(value)
    p.widget = "fixed"
    p.note = ""
    if isinstance(value, dict):
        p.note = "mapped (%s): edit the map in Maya or the GroomLab app" % (value.get("kind") or value.get("type") or "map")
    elif kind == "bool" and isinstance(value, bool):
        p.widget, p.value_bool = "bool", value
    elif kind == "int" and isinstance(value, int) and not isinstance(value, bool):
        p.widget, p.value_int = "int", value
    elif kind == "float" and isinstance(value, (int, float)) and not isinstance(value, bool):
        if unit in units.SCENE_LENGTH:
            p.widget, p.value_length = "length", units.to_ui(unit, float(value), scene)
        elif unit == "fraction":
            p.widget, p.value_factor = "factor", float(value)
        else:
            p.widget, p.value_float = "float", units.to_ui(unit, float(value), scene)
    elif kind == "vec3" and isinstance(value, list) and len(value) == 3:
        if unit == "color":
            p.widget, p.value_color = "color", [float(v) for v in value]
        else:  # a position or a move (unit length): in scene units
            p.widget, p.value_vector = "vector", [units.to_ui(unit, float(v), scene) for v in value]
    elif kind == "string" and isinstance(value, str):
        choices = json.loads(p.choices or "[]")
        if choices and value in choices:
            p.widget, p.value_enum = "enum", value
        else:
            p.widget, p.value_string = "string", value
    elif kind == "ramp":
        p.note = "a ramp: edit it in Maya or the GroomLab app"
    else:
        p.note = "edit it in Maya or the GroomLab app"


def refresh_params(scalp):
    g = scalp.groomlab
    node = active_node(scalp)
    with props.quiet():
        g.params.clear()
        g.params_node = node.name if node else ""
    if node is None:
        return
    rt = groom.ensure(scalp)
    spec = _call("node_spec", session=rt.session, node=node.name)["spec"]
    graph = json.loads(g.graph_json or "{}")
    values = {}
    for n in graph.get("nodes", []):
        if n.get("name") == node.name:
            values = n.get("params") or {}
    scene = groom.scene_of(scalp)
    defaults = {ps["name"]: ps.get("default") for ps in spec.get("params", [])}
    with props.quiet():
        for ps in spec.get("params", []):
            p = g.params.add()
            p.name = ps["name"]
            p.kind = ps.get("kind", "")
            p.unit = units.effective(ps, lambda q: values.get(q, defaults.get(q)))  # (unitWhen: by the node's values)
            p.group = ps.get("group", "")
            p.advanced = bool(ps.get("advanced"))
            p.description = ps.get("description", "")
            p.choices = json.dumps(ps.get("choices") or [])
            p.show_when = json.dumps(ps.get("showWhen") or {})
            _set_widget(p, values.get(ps["name"], ps.get("default")), scene)
    g.node_type_description = spec.get("description", "")


def widget_value(p, scene):
    w = p.widget
    if w == "bool":
        return bool(p.value_bool)
    if w == "int":
        return int(p.value_int)
    if w == "length":
        return units.from_ui(p.unit, float(p.value_length), scene)
    if w == "factor":
        return float(p.value_factor)
    if w == "float":
        return units.from_ui(p.unit, float(p.value_float), scene)
    if w == "color":
        return [float(v) for v in p.value_color]
    if w == "vector":
        return [units.from_ui(p.unit, float(v), scene) for v in p.value_vector]
    if w == "enum":
        return str(p.value_enum)
    if w == "string":
        return str(p.value_string)
    return json.loads(p.raw or "null")


def push_param(p, context):
    """A widget changed: set the param in the helper (centimeters), evaluate soon."""
    scalp = p.id_data
    g = scalp.groomlab
    if not g.params_node:
        return
    scene = context.scene if context is not None and context.scene is not None else groom.scene_of(scalp)
    value = widget_value(p, scene)
    try:
        rt = groom.ensure(scalp, scene)
        _call("set_param", session=rt.session, node=g.params_node, param=p.name, value=value)
        p.raw = json.dumps(value)
        groom.remember_graph(scalp, rt)
        groom.mark(scalp, "eval")
        from . import graph_tree
        graph_tree.values_changed(scalp, g.params_node)
    except HelperError as e:
        groom.runtime(scalp).error = e.message


def set_param(scalp, node, param, value):
    """Scripts and tests: a param by name, in the groom's own units (centimeters)."""
    rt = groom.ensure(scalp)
    _call("set_param", session=rt.session, node=node, param=param, value=value)
    groom.remember_graph(scalp, rt)
    if scalp.groomlab.params_node == node:
        refresh_params(scalp)
    from . import graph_tree
    graph_tree.values_changed(scalp, node)
    groom.mark(scalp, "eval")


def visible(p, by_name):
    """Whether a param shows: its showWhen's param holds one of its values (that param's widget)
    and that param shows too, and so on up (graph.h ParamCondition: Clump's count hides with
    clumpBy)."""
    seen = set()
    while p is not None and p.name not in seen:
        seen.add(p.name)
        when = json.loads(p.show_when or "{}")
        other = by_name.get(when.get("param", "")) if when else None
        if other is None:
            return True
        value = json.loads(other.raw or "null")
        text = ("true" if value else "false") if isinstance(value, bool) else value if isinstance(value, str) else json.dumps(value)
        if text not in when.get("in", []):
            return False
        p = other
    return True


# ---- Edits ------------------------------------------------------------------------------------

def _edit(rt, **args):
    return _call("edit_graph", session=rt.session, **args)


def _ensure(scalp):
    """The groom's runtime, the Graph Editor's node moves sent first (they go with the next edit)."""
    rt = groom.ensure(scalp)
    from . import graph_tree
    graph_tree.push_positions(scalp)
    return rt


def _changed(scalp, rt):
    refresh(scalp, rt)
    groom.mark(scalp, "eval")


def add_node(scalp, type_name, after=None):
    """A node of `type_name`, on the main chain after `after` (else before the output)."""
    rt = _ensure(scalp)
    graph = _call("get_graph", session=rt.session)["graph"]
    chain, into = main_chain(graph)
    made = _edit(rt, op="add_node", type=type_name)["name"]
    spec = operator_specs().get(type_name, {})
    strand_in = next((i["name"] for i in spec.get("inputs", []) if i.get("type") in STRAND_TYPES), None)
    out_type = (spec.get("output") or {}).get("type")
    error = ""
    if strand_in and out_type in STRAND_TYPES and len(chain) >= 2:
        i = chain.index(after) if after in chain and after != chain[-1] else len(chain) - 2
        prev, nxt = chain[i], chain[i + 1]
        nxt_input = _input_from(into, nxt, prev)
        _edit(rt, op="connect", **{"from": prev, "to": made, "input": strand_in})
        error = _edit(rt, op="connect", **{"from": made, "to": nxt, "input": nxt_input})["graphError"]
    _changed(scalp, rt)
    g = scalp.groomlab
    for k, item in enumerate(g.nodes):
        if item.name == made:
            g.node_index = k
    return made, error


def remove_node(scalp, name):
    rt = _ensure(scalp)
    graph = _call("get_graph", session=rt.session)["graph"]
    chain, into = main_chain(graph)
    if name in chain and 0 < chain.index(name) < len(chain) - 1:
        i = chain.index(name)
        _edit(rt, op="connect", **{"from": chain[i - 1], "to": chain[i + 1], "input": _input_from(into, chain[i + 1], name)})
    error = _edit(rt, op="remove_node", name=name)["graphError"]
    _changed(scalp, rt)
    return error


def move_node(scalp, name, direction):
    """Moves a node one place along the main chain (-1: toward the roots, +1: toward the output).
    The source (Scatter...) and the output stay where they are. False when it cannot move."""
    rt = _ensure(scalp)
    graph = _call("get_graph", session=rt.session)["graph"]
    chain, into = main_chain(graph)
    if name not in chain:
        return False
    i = chain.index(name)
    j = i - 1 if direction < 0 else i  # swap chain[j] and chain[j + 1]
    if j < 1 or j + 2 > len(chain) - 1:
        return False
    a, b, c, d = chain[j - 1], chain[j], chain[j + 1], chain[j + 2]
    in_b, in_c, in_d = _input_from(into, b, a), _input_from(into, c, b), _input_from(into, d, c)
    _edit(rt, op="connect", **{"from": a, "to": c, "input": in_c})
    _edit(rt, op="connect", **{"from": c, "to": b, "input": in_b})
    _edit(rt, op="connect", **{"from": b, "to": d, "input": in_d})
    _changed(scalp, rt)
    g = scalp.groomlab
    for k, item in enumerate(g.nodes):
        if item.name == name:
            g.node_index = k
    return True


def apply_preset(scalp, name):
    rt = groom.ensure(scalp)
    r = _call("apply_preset", session=rt.session, name=name)
    from . import graph_tree
    graph_tree.mark_replaced(scalp)  # the preset's layout, not the tree's
    _changed(scalp, rt)
    return r


def insert_preset(scalp, name, after=None):
    rt = groom.ensure(scalp)
    from . import graph_tree
    graph_tree.push_positions(scalp)  # the look goes after `after`, the nodes right of it move over
    args = {"session": rt.session, "name": name}
    if after:
        args["after"] = after
    r = _call("insert_preset", **args)
    _changed(scalp, rt)
    return r
