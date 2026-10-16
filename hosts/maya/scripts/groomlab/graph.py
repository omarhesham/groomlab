# SPDX-License-Identifier: Apache-2.0
"""Editable model of a groom graph (the Python graph API, PG-7).

No Maya or Qt imports, so it runs anywhere and is what the graph editor edits. The edits every
host's node editor makes (a node added with its name and seed, removed, connected with the type
and cycle checks, a node's ports) are the core's (engine/core/include/groomlab/graph_edit.h,
through groomlab_core), so the standalone app edits a graph exactly as this does. Operator
ports and parameters come from the plugin's `groomLabOperators` command (see
OperatorCatalog.from_maya).

    catalog = OperatorCatalog.from_maya()
    model = GraphModel.from_groom(groom, catalog)
    model.add_node("Curl", name="curl")
    model.insert_between("clump", "length", "curl")
    model.set_param("curl", "radius", 0.02)
    model.save(groom)

The graph dict matches the groom's JSON (see engine/core/include/groomlab/graph_io.h), with the
layout in it as every host keeps it: an optional "ui": {"x": .., "y": ..} per node and the
graph's "backdrops", which evaluation ignores and groom files carry to the standalone app.

A node can have several outputs (a Map node: luminance, r, g, b, a); a connection
{"from", "to", "input"} reads the source's first output, or the one its "output" names
(stored only when it is not the first):

    model.connect("mask", "clump", "strength", output="r")

A keyed param names the control that sets it on every frame in the node's "paramControls" (a
name, or three for a vector; groomlab.set_param_key makes the link and its attribute):

    model.link_control("clump", "strength", "clump_strength")
"""

import contextlib
import copy
import json

import groomlab_core

LAYOUT_DX = 240  # the core's column for a new node (graph_edit.cpp)
LAYOUT_DY = 140


class GraphError(ValueError):
    """An edit the graph cannot take (unknown node or port, type mismatch, cycle)."""


@contextlib.contextmanager
def _core_errors():
    """The core's GraphError as this module's (a ValueError), with its message."""
    try:
        yield
    except groomlab_core.GraphError as e:
        raise GraphError(str(e))


class OperatorCatalog(object):
    """Operator specs: ports, parameters and their defaults, by type name."""

    def __init__(self, specs):
        self._specs = {s["type"]: s for s in specs}

    @classmethod
    def from_json(cls, text):
        return cls(json.loads(text))

    _cached = (None, None)  # (groomLabOperators text, catalog): parsed once per plugin build

    @classmethod
    def from_maya(cls):
        from maya import cmds  # imported here: the model itself needs no Maya
        text = cmds.groomLabOperators()
        if OperatorCatalog._cached[0] != text:
            OperatorCatalog._cached = (text, cls.from_json(text))
        return OperatorCatalog._cached[1]

    def types(self):
        return sorted(self._specs)

    def spec(self, op_type):
        try:
            return self._specs[op_type]
        except KeyError:
            raise GraphError("unknown operator type %r" % op_type)

    def input_names(self, op_type):
        return [p["name"] for p in self.spec(op_type)["inputs"]]

    def input_spec(self, op_type, input_name):
        for p in self.spec(op_type)["inputs"]:
            if p["name"] == input_name:
                return p
        raise GraphError("%s has no input %r" % (op_type, input_name))

    def outputs(self, op_type):
        """The operator's output ports [{"name", "type"}], the primary first."""
        spec = self.spec(op_type)
        return spec.get("outputs") or [spec["output"]]

    def output_names(self, op_type):
        return [p["name"] for p in self.outputs(op_type)]

    def output_type(self, op_type, output=None):
        """The type of the operator's output `output` (a name; None: the first)."""
        for port in self.outputs(op_type):
            if output is None or port["name"] == output:
                return port["type"]
        raise GraphError("%s has no output %r" % (op_type, output))

    def param_spec(self, op_type, param):
        for p in self.spec(op_type)["params"]:
            if p["name"] == param:
                return p
        return None


CURVE_KINDS = ("Strands", "Guides", "Curves")


def types_match(out_type, in_type):
    """Whether an output of `out_type` can feed an input of `in_type`: the same type, or Curves
    (strands or guides: the shaping nodes) on either side with Strands or Guides on the other
    (the core's portTypesMatch)."""
    if out_type == in_type:
        return True
    try:
        return groomlab_core.port_types_match(getattr(groomlab_core.PortType, out_type),
                                              getattr(groomlab_core.PortType, in_type))
    except (AttributeError, TypeError):  # a name no port type has
        return False


def kind_label(port_type):
    return {"Strands": "strands", "Guides": "guides", "Curves": "strands or guides"}.get(port_type, port_type)


def node_fields(node):
    """A node's fields as stored (type, params, paramInputs, bypass, ui and anything else the
    writer put there), without its name; empty paramInputs and a false bypass are dropped.
    Every field is kept so nothing the plugin writes is lost through a load and save."""
    fields = {}
    for key, value in node.items():
        if key == "name":
            continue
        if key in ("paramInputs", "paramControls") and not value:
            continue
        if key == "bypass" and not value:
            continue
        if key == "evaluation" and value == "always":
            continue
        fields[key] = copy.deepcopy(value)
    fields.setdefault("params", {})
    return fields


def without_layout(data):
    """A graph's dict without the editors' layout (every node's "ui", the backdrops)."""
    out = {k: v for k, v in data.items() if k != "backdrops"}
    out["nodes"] = [{k: v for k, v in n.items() if k != "ui"} for n in data.get("nodes", [])]
    return out


MAP_SOURCES = ("texture", "colorSet", "attribute", "expression", "guide")
# When a node runs (a node's "evaluation"; none: always): viewportOnly nodes are bypassed in
# renders, exports, the Arnold procedural and the CLI, renderOnly ones in the viewport.
EVALUATION_MODES = ("always", "viewportOnly", "renderOnly")
MAP_LABELS = {"texture": "texture", "colorSet": "color set", "attribute": "attribute", "expression": "expression",
              "guide": "painted map"}


def map_source(value):
    """Which kind of map a param value is ("texture", "colorSet", "attribute", "expression" or "guide"),
    or None for a plain value."""
    if not isinstance(value, dict):
        return None
    return next((s for s in MAP_SOURCES if s in value), None)


BACKDROPS_KEY = "__backdrops__"
CLIPBOARD_KEY = "groomlabNodes"



def linkable(spec):
    """Whether a param (its spec) can take an input port: a mappable number (a field drives it), a
    color that takes a color field (colorField: a Map's rgb output), a ramp (a Curve node)."""
    return bool(spec) and bool(spec.get("mappable") or spec.get("colorField") or spec.get("kind") == "ramp")

class GraphModel(object):
    """Nodes, connections and the output node of one groom graph."""

    def __init__(self, catalog, data=None):
        self.catalog = catalog
        self.nodes = {}          # name -> {"type": str, "params": dict, "ui": {"x", "y"}}
        self.order = []          # node names in insertion order (stable JSON and layout)
        self.connections = []    # [{"from": src, "to": dst, "input": port[, "output": src's output]}]
        self.output = None
        self.backdrops = []      # [{"title", "x", "y", "w", "h", "color"}]: layout only
        self.renamed = {}        # old name -> new name since the last save (the groom's viewNode follows)
        self.copied = {}         # pasted node -> the node it copies, since the last save (Sculpt edits follow)
        self.groom_size = None   # the groom's scalp size (presets.scalp_size): new nodes' defaults fit it
        if data:
            self.load_dict(data)

    # -- Construction ------------------------------------------------------

    @classmethod
    def from_groom(cls, groom, catalog=None):
        from maya import cmds
        catalog = catalog or OperatorCatalog.from_maya()
        text = cmds.getAttr(groom + ".graph") or "{}"
        data = json.loads(text)
        if not data:
            data = json.loads(cmds.groomLabDefaultGraph())
        model = cls(catalog, data)
        from groomlab import presets  # Maya's (scalp_size), as cmds above
        model.groom_size = presets.scalp_size(groom)
        if not any("ui" in model.nodes[n] for n in model.order):
            # A scene from before the layout lived in the graph kept it in graphUi: read it then.
            layout = json.loads(cmds.getAttr(groom + ".graphUi") or "{}")
            backdrops = layout.pop(BACKDROPS_KEY, [])
            if not model.backdrops:
                model.backdrops = [dict(b) for b in backdrops]
            for name, (x, y) in layout.items():
                if name in model.nodes:
                    model.set_position(name, x, y)
        model.ensure_layout()
        return model

    def ensure_layout(self):
        """Lay the graph out when any node has no position yet."""
        if any("ui" not in self.nodes[n] for n in self.order):
            self.auto_layout()

    def load_dict(self, data):
        self.nodes, self.order, self.connections, self.output = {}, [], [], None
        for n in data.get("nodes", []):
            self.catalog.spec(n["type"])  # validates the type
            self.nodes[n["name"]] = node_fields(n)
            self.order.append(n["name"])
        self.output = data.get("output")
        self.backdrops = [dict(b) for b in data.get("backdrops") or [] if isinstance(b, dict)]
        # Every connection made again as connect makes it (its checks), in one go in the core.
        self.connections = [dict(c) for c in data.get("connections", [])]
        edit = self._edit()
        with _core_errors():
            edit.check_connections()
        self.connections = json.loads(edit.to_json()).get("connections", [])

    # -- The core's edits (graph_edit.h): what every host's editor does to a graph ----------

    def _edit(self):
        """The graph as the core edits it."""
        return groomlab_core.GraphEdit(json.dumps(self.to_dict()))

    def _take(self, edit):
        """The core's edited graph back into the model (a node that did not change keeps its dict)."""
        data = json.loads(edit.to_json())
        nodes, order = {}, []
        for n in data.get("nodes", []):
            fields = node_fields(n)
            kept = self.nodes.get(n["name"])
            nodes[n["name"]] = kept if kept is not None and node_fields(kept) == fields else fields
            order.append(n["name"])
        self.nodes, self.order = nodes, order
        self.connections = data.get("connections", [])
        self.output = data.get("output")

    def to_dict(self):
        nodes = []
        for name in self.order:
            entry = {"name": name}
            entry.update(node_fields(self.nodes[name]))
            nodes.append(entry)
        data = {"version": 1, "nodes": nodes, "connections": copy.deepcopy(self.connections)}
        if self.output:
            data["output"] = self.output
        if self.backdrops:
            data["backdrops"] = copy.deepcopy(self.backdrops)
        return data

    def to_json(self, indent=None, layout=True):
        """The graph as the groom stores it, with its layout (every node's "ui", the backdrops);
        layout=False leaves the layout out."""
        data = self.to_dict()
        if not layout:
            data = without_layout(data)
        return json.dumps(data, indent=indent, separators=None if indent else (",", ":"))

    def layout_json(self):
        layout = {n: list(self.position(n)) for n in self.order if "ui" in self.nodes[n]}
        if self.backdrops:
            layout[BACKDROPS_KEY] = self.backdrops
        return json.dumps(layout)

    # -- Bypass --------------------------------------------------------------

    def bypassed(self, name):
        return bool(self._node(name).get("bypass"))

    def set_bypass(self, name, on):
        """Pass the node's first input straight through (compare with and without it)."""
        if on:
            self._node(name)["bypass"] = True
        else:
            self._node(name).pop("bypass", None)

    # -- Evaluation (when the node runs) ---------------------------------------

    def evaluation(self, name):
        """"always", "viewportOnly" (bypassed in renders and exports) or "renderOnly"
        (bypassed in the viewport)."""
        return self._node(name).get("evaluation", "always")

    def set_evaluation(self, name, mode):
        if mode not in EVALUATION_MODES:
            raise GraphError("evaluation is one of %s, not %r" % (", ".join(EVALUATION_MODES), mode))
        if mode == "always":
            self._node(name).pop("evaluation", None)
        else:
            self._node(name)["evaluation"] = mode

    # -- Copy and paste ---------------------------------------------------------

    def copy_nodes(self, names):
        """The nodes and the links between them, as JSON text for the clipboard."""
        names = [n for n in self.order if n in set(names)]
        data = {CLIPBOARD_KEY: 1, "nodes": [], "connections": []}
        for name in names:
            n = copy.deepcopy(self.nodes[name])
            n["name"] = name
            data["nodes"].append(n)
        data["connections"] = [c for c in self.connections if c["from"] in names and c["to"] in names]
        return json.dumps(data)

    def paste_nodes(self, text, offset=(40.0, 40.0)):
        """Add copied nodes (new names, moved by `offset`) with their links. Returns the new names."""
        data = json.loads(text)
        if not isinstance(data, dict) or CLIPBOARD_KEY not in data:
            raise GraphError("the clipboard holds no GroomLab nodes")
        renamed = {}
        for n in data["nodes"]:
            new = self.unique_name(n["name"])
            renamed[n["name"]] = new
            x, y = n.get("ui", {}).get("x", 0.0), n.get("ui", {}).get("y", 0.0)
            self.catalog.spec(n["type"])
            self.nodes[new] = {k: copy.deepcopy(v) for k, v in n.items() if k != "name"}
            self.nodes[new]["ui"] = {"x": x + offset[0], "y": y + offset[1]}
            self.order.append(new)
            self.copied[new] = n["name"]
        for c in data["connections"]:
            self.connect(renamed[c["from"]], renamed[c["to"]], c["input"], output=c.get("output"))
        return [renamed[n["name"]] for n in data["nodes"]]

    def save(self, groom):
        """Validate, then store the graph and its layout on the groom: one undo step.

        The groom's viewNode (the Focus node) follows a renamed node and is
        cleared when the node it names is gone."""
        problems = self.validate()
        if problems:
            raise GraphError("; ".join(problems))
        from maya import cmds
        import groomlab
        self.complete_params()
        text = self.to_json()  # with the layout: it travels with the groom (groom files, the app)
        with groomlab.undo_chunk("graph edit"):
            groomlab.sync_controls(groom, self.to_dict())
            if cmds.getAttr(groom + ".graph") != text:  # unchanged graph: keep the evaluation cache
                cmds.setAttr(groom + ".graph", text, type="string")
            # Sculpt nodes' edits follow renames, copies and deletions, in the same undo step.
            from groomlab import sculpts
            copied, self.copied = self.copied, {}
            sculpts.follow_graph_edit(groom, renamed=self.renamed, copied={k: v for k, v in copied.items() if k in self.nodes},
                                      kept=set(self.nodes))
            self._sync_view_node(groom)

    def complete_params(self):
        """Write every param each node leaves out (its spec default) and each node's version, as
        saved grooms hold them (graph_io.h): a later change of a default never alters this groom."""
        edit = self._edit()
        with _core_errors():
            edit.complete_params()
        self._take(edit)

    def node_version(self, name):
        """(the behaviour version the node keeps, its operator's newest): an Upgrade is on offer
        while the first is lower (graph_edit.h; no version stored: the newest)."""
        node = self._node(name)
        newest = self.catalog.spec(node["type"]).get("version", 1)
        saved = node.get("version")
        return (saved if isinstance(saved, int) and not isinstance(saved, bool) else newest), newest

    def upgrade_node(self, name):
        """Raise an outdated node (its spec's "outdated") to its operator's newest behaviour.
        False when it is the newest already."""
        edit = self._edit()
        with _core_errors():
            done = edit.upgrade_node(name)
        self._take(edit)
        return done

    def _sync_view_node(self, groom):
        from maya import cmds
        viewed = cmds.getAttr(groom + ".viewNode") or ""
        renamed, self.renamed = self.renamed, {}
        if not viewed:
            return
        wanted = renamed.get(viewed, viewed)
        if wanted not in self.nodes:
            wanted = ""
        if wanted != viewed:
            cmds.setAttr(groom + ".viewNode", wanted, type="string")

    def save_layout(self, groom):
        """Store only the layout (node positions, backdrops) in the groom's graph as it is stored:
        an edit of the model not saved yet stays out of it (the Graph Editor calls this with undo
        off, so moves make no undo steps). Nothing evaluates again: the graph is the same. Returns
        the graph's text now."""
        from maya import cmds
        text = cmds.getAttr(groom + ".graph") or "{}"
        stored = json.loads(text) if text.strip() else {}
        if not stored:
            stored = json.loads(cmds.groomLabDefaultGraph())  # the default graph, laid out
        bare = without_layout(stored)
        bare["nodes"] = [dict([("name", n.get("name"))] + list(node_fields(n).items())) for n in bare["nodes"]]
        bare.setdefault("version", 1)
        if bare == without_layout(self.to_dict()):
            new = self.to_json()  # nothing else differs: the model's own text (what the editor compares)
        else:
            for n in stored.get("nodes", []):
                if n.get("name") in self.nodes and "ui" in self.nodes[n["name"]]:
                    n["ui"] = copy.deepcopy(self.nodes[n["name"]]["ui"])
            stored.pop("backdrops", None)
            if self.backdrops:
                stored["backdrops"] = copy.deepcopy(self.backdrops)
            new = json.dumps(stored, separators=(",", ":"))
        if new != text:
            cmds.setAttr(groom + ".graph", new, type="string")
        return new

    # -- Nodes -------------------------------------------------------------

    def unique_name(self, base):
        """A new node's name (the core's, every host's): `base` with its first letter lowered,
        else base1, base2, ..."""
        names = groomlab_core.GraphEdit(json.dumps({"nodes": [{"name": n} for n in self.order]}))
        return names.unique_name(base)

    def add_node(self, op_type, name=None, params=None, pos=None):
        """Add a node with default params (overridden by `params`); returns its name.

        A node with a "seed" param that `params` leaves out gets a seed no other
        node of its type has (the default when it is free): two Scatters with one
        seed would make the same strands, two Flyaways pick the same strays. Without
        `pos` it goes in a free column right of the graph (the core's addNode). The defaults
        `params` leaves out that measure something fit `groom_size` (the groom's scalp, as
        presets fit it; the defaults are authored at 20)."""
        self.catalog.spec(op_type)
        if name and name in self.nodes:
            raise GraphError("a node named %r exists" % name)
        edit = self._edit()
        with _core_errors():
            made = edit.add_node(op_type, name or "", json.dumps(params or {}),
                                 None if pos is None else float(pos[0]), None if pos is None else float(pos[1]),
                                 self.groom_size)
        self._take(edit)
        return made

    def remove_node(self, name):
        self._node(name)
        edit = self._edit()
        with _core_errors():
            edit.remove_node(name)
        self._take(edit)
        self.renamed = {k: v for k, v in self.renamed.items() if v != name}

    def rename_node(self, old, new):
        self._node(old)
        if new in self.nodes:
            raise GraphError("a node named %r exists" % new)
        self.nodes[new] = self.nodes.pop(old)
        self.order[self.order.index(old)] = new
        for c in self.connections:
            if c["from"] == old:
                c["from"] = new
            if c["to"] == old:
                c["to"] = new
        if self.output == old:
            self.output = new
        for k, v in list(self.renamed.items()):
            if v == old:
                self.renamed[k] = new
        self.renamed.setdefault(old, new)

    def node_type(self, name):
        return self._node(name)["type"]

    _specs = {}  # node_spec's answers (the core's text) by what they depend on, kept per process

    def node_spec(self, name):
        """Ports and params of a node: its operator's, except that a Compound's
        come from its definition (inputs, output, exposed params) and an
        Input's output type from its params. "outputs" lists every output port
        (the primary, "output", first). The core decides it (graph_edit.h), as
        it does for the app's node graph and for connect's checks."""
        node = self._node(name)
        params = node.get("params") or {}
        exposed = tuple(node.get("paramInputs") or ())
        if node["type"] == "Compound":
            key = (node["type"], json.dumps(params.get("definition") or {}, sort_keys=True), exposed)
            bare = {"type": node["type"], "params": {"definition": params.get("definition") or {}}}
        elif node["type"] == "Input":
            key = (node["type"], params.get("type", "Strands"), exposed)
            bare = {"type": node["type"], "params": {"type": params.get("type", "Strands")}}
        else:
            self.catalog.spec(node["type"])  # the catalog's error for an unknown type
            key = (node["type"], None, exposed)
            bare = {"type": node["type"]}
        text = GraphModel._specs.get(key)
        if text is None:
            if exposed:
                bare["paramInputs"] = list(exposed)
            with _core_errors():
                text = groomlab_core.node_spec_json(json.dumps(bare))
            if len(GraphModel._specs) > 2000:
                GraphModel._specs.clear()
            GraphModel._specs[key] = text
        return json.loads(text)

    # -- Exposed params (PG-7): a param driven by another node's Field output

    def param_inputs(self, name):
        return list(self._node(name).get("paramInputs", []))

    def expose_param(self, name, param):
        """Give a param an input port: a mappable param a Field port (Field, FieldNoise, ... drive it),
        a color param that takes a color field (colorField: a Map's rgb output colors it) one too, a
        ramp param a Curve port (a Curve node drives it)."""
        spec = self.param_spec(name, param)
        if not linkable(spec):
            raise GraphError("%s.%s is neither a mappable, a color nor a ramp param" % (name, param))
        if any(p["name"] == param for p in self._base_inputs(name)):
            raise GraphError("%s already has an input named %r" % (name, param))
        exposed = self._node(name).setdefault("paramInputs", [])
        if param not in exposed:
            exposed.append(param)

    def unexpose_param(self, name, param):
        """Remove a param's input port (and what was connected to it); the param keeps its own value."""
        node = self._node(name)
        if param in node.get("paramInputs", []):
            node["paramInputs"].remove(param)
            if not node["paramInputs"]:
                del node["paramInputs"]
            self.disconnect(name, param)

    # -- Keyed params: a param linked to the groom's control attribute(s), set from them per frame

    def param_controls(self, name):
        """{param: control} of the node's keyed params: a control name, or [x, y, z] names for a vector."""
        return copy.deepcopy(self._node(name).get("paramControls", {}))

    def control_of(self, name, param):
        """The control(s) keying a param (a name, or three for a vector), or None."""
        return copy.deepcopy(self._node(name).get("paramControls", {}).get(param))

    def keyable(self, name, param):
        """None when the param can be keyed in place, else why not."""
        spec = self.param_spec(name, param)
        if spec is None:
            return "%s has no param %r" % (self.node_type(name), param)
        if self.node_type(name) == "Control":
            return "a Control node's params make its slider: key the slider (ctl_ attribute) itself"
        if spec.get("animatable") is False:
            return ("it sets how many strands, points or cards there are, or where they go: that must stay the same "
                    "on every frame (motion blur, Alembic and Unreal groom caches)")
        if spec.get("kind") not in ("float", "int", "bool", "vec3"):
            return "only numbers, switches and vectors can be keyed"
        if isinstance(self.param(name, param), dict):
            return "it reads a map: set it back to a value to key it"
        return None

    def link_control(self, name, param, control):
        """Key a param from the groom's control(s): `control` a name, or [x, y, z] names for a vector."""
        why = self.keyable(name, param)
        if why:
            raise GraphError("%s.%s cannot be keyed: %s" % (name, param, why))
        vector = self.param_spec(name, param)["kind"] == "vec3"
        if vector != isinstance(control, (list, tuple)) or (vector and len(control) != 3):
            raise GraphError("%s.%s is keyed by %s" % (name, param, "three controls" if vector else "one control"))
        self._node(name).setdefault("paramControls", {})[param] = list(control) if vector else control

    def unlink_control(self, name, param):
        """The param back to its own value (in "params")."""
        node = self._node(name)
        links = node.get("paramControls", {})
        links.pop(param, None)
        if not links:
            node.pop("paramControls", None)

    def driver_of(self, name, param):
        """The node driving an exposed param, or None."""
        return self.source_of(name, param) if param in self.param_inputs(name) else None

    def _base_inputs(self, name):
        spec = self.node_spec(name)
        return [p for p in spec["inputs"] if not p.get("param")]

    def input_spec(self, name, input_name):
        for port in self.node_spec(name)["inputs"]:
            if port["name"] == input_name:
                return port
        raise GraphError("%s has no input %r" % (name, input_name))

    def output_names(self, name):
        """The node's output ports by name, the primary first (one for most nodes)."""
        return [p["name"] for p in self.node_spec(name)["outputs"]]

    def output_spec(self, name, output=None):
        """The node's output port `output` (a name; None: the first). GraphError when it has none."""
        outputs = self.node_spec(name)["outputs"]
        if output is None:
            return outputs[0]
        for port in outputs:
            if port["name"] == output:
                return port
        raise GraphError("%s has no output %r (%s)" % (name, output, ", ".join(p["name"] for p in outputs)))

    def output_type(self, name, output=None, _edit=None):
        """A node's output type as connected (its output `output`, None: the first): a Curves node
        (strands or guides) gives what feeds it (the core's, as connect checks it)."""
        out = self.output_spec(name, output)["type"]
        if out != "Curves":
            return out
        with _core_errors():
            return (_edit or self._edit()).output_type(name, output or "")

    def param_spec(self, name, param):
        return next((p for p in self.node_spec(name)["params"] if p["name"] == param), None)

    def set_position(self, name, x, y):
        self._node(name)["ui"] = {"x": float(x), "y": float(y)}

    def position(self, name):
        ui = self._node(name).get("ui", {})
        return ui.get("x", 0.0), ui.get("y", 0.0)

    # -- Params ------------------------------------------------------------

    def param(self, name, param):
        """The node's value for `param`, or the operator default."""
        node = self._node(name)
        if param in node["params"]:
            return node["params"][param]
        spec = self.param_spec(name, param)
        if spec is None:
            raise GraphError("%s has no param %r" % (node["type"], param))
        return copy.deepcopy(spec["default"])

    def set_param(self, name, param, value):
        node = self._node(name)
        if self.param_spec(name, param) is None:
            raise GraphError("%s has no param %r" % (node["type"], param))
        node["params"][param] = copy.deepcopy(value)

    # -- Connections -------------------------------------------------------

    def source_of(self, dst, input_name):
        for c in self.connections:
            if c["to"] == dst and c["input"] == input_name:
                return c["from"]
        return None

    def output_of(self, dst, input_name):
        """Which output of its source feeds dst's input (a name; the first when the link names
        none), or None when the input is not connected."""
        for c in self.connections:
            if c["to"] == dst and c["input"] == input_name:
                return c.get("output") or self.output_names(c["from"])[0]
        return None

    def connect(self, src, dst, input_name, output=None, by_hand=False):
        """Connect src's output `output` (a name; None: its first) to dst's input, replacing what
        fed that input. The link records the output only when it is not the first. The core
        checks it (graph_edit.h, the app's node graph's too): the types must fit, no cycle.
        by_hand: a link the user drew (GraphEdit::connectByHand): the node is set to use it where
        a dropdown says whether it does (a Clump of centersFrom pick fed centers: inputPoints)."""
        edit = self._edit()
        with _core_errors():
            if by_hand:
                if edit.connect_by_hand(src, dst, input_name, output or ""):
                    self._take(edit)
                    return
            else:
                edit.connect(src, dst, input_name, output or "")
        self.connections = json.loads(edit.to_json()).get("connections", [])

    def disconnect(self, dst, input_name):
        """Remove what feeds dst's input (the core's disconnect)."""
        edit = self._edit()
        if edit.disconnect(dst, input_name):
            self.connections = json.loads(edit.to_json()).get("connections", [])

    def upstream(self, name):
        """All nodes feeding `name`, directly or not."""
        if name not in self.nodes:
            return set()
        with _core_errors():
            return set(self._edit().upstream(name))

    def channels(self, guide_maps=()):
        """The attributes (strand channels) per node, as the graph is set, without evaluating it
        (groomlab_core.graph_channels): {node: {"incoming": [{"name", "node"}] (reaching it, each
        with the node that wrote it last), "writes": [{"name", "param", "extends"}], "overwrites":
        [{"name", "param", "node"}], "reads": [{"param", "name", "reaches", "node", "optional"}]}}.
        guide_maps: the maps painted on the guides (Interpolate turns them into attributes)."""
        with _core_errors():
            return groomlab_core.graph_channels(json.dumps(self.to_dict()), list(guide_maps))

    def downstream_of(self, src):
        return [(c["to"], c["input"]) for c in self.connections if c["from"] == src]

    def insert_between(self, src, dst, node, node_input=None):
        """Route src -> node -> dst in place of every src -> dst connection (node reading the
        output of src the first of them read)."""
        node_input = node_input or self.node_spec(node)["inputs"][0]["name"]
        links = [c for c in self.connections if c["from"] == src and c["to"] == dst]
        if not links:
            raise GraphError("%s is not connected to %s" % (src, dst))
        self.connect(src, node, node_input, output=links[0].get("output"))
        for c in links:
            self.connect(node, dst, c["input"])

    def insert_after(self, anchor, node, input_name=None):
        """Put `node` right after `anchor` in its chain (anchor -> node -> anchor's first
        consumer), under the anchor; with no consumer, just after it."""
        downstream = self.downstream_of(anchor)
        if downstream:
            self.insert_between(anchor, downstream[0][0], node)
        else:
            self.connect(anchor, node, input_name or self.node_spec(node)["inputs"][0]["name"])
        x, y = self.position(anchor)
        self.set_position(node, x + LAYOUT_DX / 2, y + LAYOUT_DY)

    def insert_before(self, anchor, node, input_name="strands"):
        """Put `node` between `anchor`'s `input_name` source and `anchor` (e.g. before the output)."""
        upstream = self.source_of(anchor, input_name)
        if upstream is None:
            raise GraphError("%s has no %s input to insert before" % (anchor, input_name))
        self.insert_between(upstream, anchor, node)
        x, y = self.position(anchor)
        self.set_position(node, x, y + LAYOUT_DY)

    def set_output(self, name):
        self._node(name)
        self.output = name

    # -- Compounds (PG-6) ------------------------------------------------

    def collapse(self, names, compound=None):
        """Replace nodes with one Compound node holding them; returns its name.

        Connections from outside become the compound's inputs (one per
        source node and output); exactly one of the nodes may feed nodes
        outside, from its first output, and it becomes the compound's output
        (a compound has one output).
        """
        names = [n for n in self.order if n in set(names)]
        if not names:
            raise GraphError("nothing to collapse")
        inside = set(names)
        incoming = [c for c in self.connections if c["to"] in inside and c["from"] not in inside]
        outgoing = [c for c in self.connections if c["from"] in inside and c["to"] not in inside]
        feeders = sorted({c["from"] for c in outgoing})
        if len(feeders) > 1:
            raise GraphError("only one of the nodes may feed the rest of the graph (%s do)" % ", ".join(feeders))
        other = next((c for c in outgoing if c.get("output")), None)
        if other is not None:
            raise GraphError("%s feeds the rest of the graph from its output %r: a group gives its node's first "
                             "output only" % (other["from"], other["output"]))
        if feeders:
            inner_output = feeders[0]
        else:
            sinks = [n for n in names if not any(c["from"] == n and c["to"] in inside for c in self.connections)]
            if self.output in inside:
                sinks = [self.output]
            if len(sinks) != 1:
                raise GraphError("the nodes need one output node")
            inner_output = sinks[0]

        # One compound input per outside source. Inner nodes keep every field
        # (params, exposed params, bypass, position) so Ungroup gives them back.
        inputs, input_names = [], {}
        inner_nodes = []
        for n in names:
            entry = {"name": n}
            entry.update(node_fields(self.nodes[n]))
            inner_nodes.append(entry)
        inner_links = [dict(c) for c in self.connections if c["from"] in inside and c["to"] in inside]
        for c in incoming:
            src = (c["from"], c.get("output"))  # one input per source node and output
            if src not in input_names:
                taken = [i["name"] for i in inputs]
                port = c["input"] if c["input"] not in taken else "_".join(n for n in src if n)
                input_names[src] = port
                kind = self.output_type(src[0], src[1])
                inputs.append({"name": port, "type": kind})
                inner_nodes.append({"name": "in_" + port, "type": "Input", "params": {"name": port, "type": kind}})
            inner_links.append({"from": "in_" + input_names[src], "to": c["to"], "input": c["input"]})
        definition = {"graph": {"version": 1, "nodes": inner_nodes, "connections": inner_links,
                                "output": inner_output},
                      "inputs": inputs, "expose": {}}

        xs = [self.position(n)[0] for n in names]
        ys = [self.position(n)[1] for n in names]
        was_output = self.output in inside
        snapshot = self._snapshot()
        try:
            for n in names:
                self.remove_node(n)
            name = self.add_node("Compound", name=compound or self.unique_name("compound"),
                                 params={"definition": definition}, pos=(sum(xs) / len(xs), sum(ys) / len(ys)))
            for (src, output), port in input_names.items():
                self.connect(src, name, port, output=output)
            for c in outgoing:
                self.connect(name, c["to"], c["input"])
        except GraphError:
            self._restore(snapshot)
            raise
        if was_output:
            self.output = name
        return name

    def _snapshot(self):
        return copy.deepcopy((self.nodes, self.order, self.connections, self.output, self.renamed))

    def _restore(self, snapshot):
        self.nodes, self.order, self.connections, self.output, self.renamed = copy.deepcopy(snapshot)

    def expose(self, compound, name, target):
        """Show an inner param ("node.param") as the compound param `name`."""
        node = self._node(compound)
        if node["type"] != "Compound":
            raise GraphError("%s is not a Compound" % compound)
        inner_name, _, param = target.partition(".")
        definition = node["params"].setdefault("definition", {})
        inner = next((n for n in definition.get("graph", {}).get("nodes", []) if n["name"] == inner_name), None)
        if inner is None or self.catalog.param_spec(inner["type"], param) is None:
            raise GraphError("the compound has no param %r" % target)
        definition.setdefault("expose", {})[name] = target

    def expand(self, compound):
        """Replace a Compound node with the nodes it holds (exposed values applied, every field
        kept); returns their names. A definition that cannot be expanded (an unknown type, a
        link that fits nothing) raises and leaves the graph as it was."""
        node = self._node(compound)
        if node["type"] != "Compound":
            raise GraphError("%s is not a Compound" % compound)
        definition = copy.deepcopy(node["params"].get("definition") or {})
        inner = definition.get("graph", {})
        inner_nodes = inner.get("nodes", [])
        for n in inner_nodes:
            self.catalog.spec(n["type"])  # an unknown type: refused before anything changes
        for exposed, target in definition.get("expose", {}).items():
            if exposed in node["params"]:
                inner_name, _, param = target.partition(".")
                for n in inner_nodes:
                    if n["name"] == inner_name:
                        n.setdefault("params", {})[param] = copy.deepcopy(node["params"][exposed])
        real = [n for n in inner_nodes if n["type"] != "Input"]
        if inner.get("output") not in [n["name"] for n in real]:
            raise GraphError("%s has no output node inside" % compound)
        sources = {c["input"]: (c["from"], c.get("output")) for c in self.connections if c["to"] == compound}
        consumers = [c for c in self.connections if c["from"] == compound]
        x, y = self.position(compound)
        was_output = self.output == compound

        # Where they were when grouped, moved with the compound; nodes without a position fan out.
        placed = [n["ui"] for n in real if "ui" in n]
        if placed:
            cx = sum(u.get("x", 0.0) for u in placed) / len(placed)
            cy = sum(u.get("y", 0.0) for u in placed) / len(placed)
        else:
            cx, cy = x, y

        snapshot = self._snapshot()
        try:
            self.remove_node(compound)
            renamed, inputs = {}, {}
            for i, n in enumerate(inner_nodes):
                if n["type"] == "Input":
                    inputs[n["name"]] = sources.get(n.get("params", {}).get("name", "strands"), (None, None))
                    continue
                new = self.unique_name(n["name"])
                fields = node_fields(n)
                ui = fields.pop("ui", None)
                if ui is not None:
                    pos = (ui.get("x", 0.0) - cx + x, ui.get("y", 0.0) - cy + y)
                else:
                    pos = (x + 40 * i, y + 40 * i)
                fields["ui"] = {"x": float(pos[0]), "y": float(pos[1])}
                self.nodes[new] = fields
                self.order.append(new)
                renamed[n["name"]] = new
            for c in inner.get("connections", []):
                if c["from"] in inputs:
                    src, output = inputs[c["from"]]
                else:
                    src, output = renamed.get(c["from"]), c.get("output")
                if c["to"] not in renamed:
                    raise GraphError("%s links %s to a node it does not hold (%s)" % (compound, c["from"], c["to"]))
                if src:
                    self.connect(src, renamed[c["to"]], c["input"], output=output)
            out = renamed[inner["output"]]
            for c in consumers:
                self.connect(out, c["to"], c["input"])
        except GraphError:
            self._restore(snapshot)
            raise
        if was_output:
            self.output = out
        return list(renamed.values())

    # -- Checks and layout -------------------------------------------------

    def feeding_output(self):
        """The nodes the output is made from (the output and everything upstream of it, bypassed
        nodes included): the ones the groom evaluates."""
        feeding = {self.output} if self.output in self.nodes else set()
        frontier = list(feeding)
        while frontier:
            name = frontier.pop()
            for c in self.connections:
                if c["to"] == name and c["from"] in self.nodes and c["from"] not in feeding:
                    feeding.add(c["from"])
                    frontier.append(c["from"])
        return feeding

    def validate(self):
        """Problems that would stop the groom from evaluating (empty list: fine): only those of the
        nodes the output is made from. A node off to the side (its output unplugged, say) never
        evaluates, so its problems are warnings (unused_problems), never a reason to stop applying
        the graph."""
        return self._problems(self.feeding_output())

    def unused_problems(self):
        """The problems of nodes the output isn't made from (they evaluate only when viewed)."""
        feeding = self.feeding_output()
        return self._problems([n for n in self.order if n not in feeding], graph_level=False)

    def _problems(self, names, graph_level=True):
        problems = []
        edit = self._edit()  # one for every output type asked below
        if graph_level and not self.output:
            problems.append("no output node")
        elif graph_level and self.output not in self.nodes:
            problems.append("output node %r does not exist" % self.output)
        for name in [n for n in self.order if n in set(names)]:
            for port in self.node_spec(name)["inputs"]:
                source = self.source_of(name, port["name"])
                if not port["optional"] and source is None:
                    problems.append("%s.%s is not connected" % (name, port["name"]))
                    continue
                if source is None:
                    continue
                output = self.output_of(name, port["name"])
                if output not in self.output_names(source):  # e.g. a Compound's definition changed
                    problems.append("%s has no output %r (%s.%s reads it)" % (source, output, name, port["name"]))
                elif not types_match(self.output_type(source, output, edit), port["type"]):
                    # e.g. a Noise now fed guides, still linked to a strands-only input
                    problems.append("%s gives %s but %s.%s takes %s" % (
                        source, kind_label(self.output_type(source, output, edit)), name, port["name"],
                        kind_label(port["type"])))
            for param, value in self.nodes[name]["params"].items():
                source = map_source(value)
                if source is not None and not value[source]:
                    problems.append("%s.%s: the %s has no name" % (name, param, MAP_LABELS[source]))
        return problems

    def warnings(self):
        """Things that evaluate but are likely mistakes (the editor shows them; save does not stop).

        Two Scatters with one seed make the same strands (same IDs and roots; a
        Merge renumbers the second's IDs but the roots still coincide); two
        Instances or Flyaways with one seed pick the same strands (the core's)."""
        with _core_errors():
            return list(self._edit().seed_warnings())

    def auto_layout(self):
        """Columns by depth from the sources, rows in insertion order."""
        depth = {}

        def depth_of(name, trail=()):
            if name in depth:
                return depth[name]
            sources = [c["from"] for c in self.connections if c["to"] == name and c["from"] not in trail]
            d = 0 if not sources else 1 + max(depth_of(s, trail + (name,)) for s in sources)
            depth[name] = d
            return d

        rows = {}
        for name in self.order:
            col = depth_of(name)
            row = rows.get(col, 0)
            rows[col] = row + 1
            self.set_position(name, col * LAYOUT_DX, row * LAYOUT_DY)

    def _node(self, name):
        try:
            return self.nodes[name]
        except KeyError:
            raise GraphError("no node named %r" % name)
