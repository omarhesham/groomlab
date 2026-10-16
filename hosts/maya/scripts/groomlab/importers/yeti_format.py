# SPDX-License-Identifier: Apache-2.0
"""Yeti's scene data read without Yeti: pure Python, no Maya, no plugin, no license.

Yeti keeps everything in two node types of the Maya scene, and a Maya ASCII file (.ma) holds both
as text, so a scene opened without the plugin (its nodes "unknown" to Maya, their data kept as
written) or the file itself can be read:

    pgYetiMaya    a Yeti graph. `.pds` (data type pgYetiMayaData) is one base64 string decoding to
                  XML, <yeti-graph version="2">: <items> holds <node id name type> elements (also
                  inside <group> elements, which only lay the graph out) and <note>s; <globals>
                  has <metadata name="root" value="<node id>"/>, the node whose output is the
                  groom (a shader node, usually). `.isp` is the image search path (folders, with
                  ${VARIABLES}, separated by ";"). `.ing` takes the meshes, `.icd` the grooms.
    pgYetiGroom   a groom: guide strands with painted attributes. `.pds` (pgYetiMayaStrandData) is
                  plain tokens: PGYS10 <groom name> <attribute count>, then each attribute as
                  <name> <type> <flag> <count> and count values (type 0 int, 1 float, 2 a vector:
                  3 floats a value). P holds every strand point (flag 1: a position), numVertices
                  the points per strand, s and t each strand's root UV, N its root normal; a
                  painted attribute (scraggle, clumping, length...) has a value per strand.
                  `.pat` names the attribute being painted; `.cgt` names the groom a corrective
                  groom corrects.

A graph node, as <node id name type>:
    <inputs><ref id="<input id>" value="<output id of the node feeding it, or ->"/>...</inputs>
    <outputs><ref id="<output id>"/></outputs>
    <param name value [attachment="<output id>"]/>   attachment: a node's output drives the param
                                                      (a texture, an expression, a curve), its value
                                                      unused then
    <config>22 serialization::archive 17 p v i p v i ...</config>   a curve node's keys
    <metadata name="gui:pos" .../>

A param's value is text: a number, a vector "[x, y, z]", an expression (SeExpr, Yeti's variables:
$strandu along the fibre, $s $t the root's UV, $N, $id; attribute names bare or with $; $f0-$f3
and $v0-$v3 an Expression node's float and vector params), or an attribute's name.

    nodes, connections = read_ma(text)                 # the pgYeti* nodes of a .ma's text
    graph = parse_graph(decode_graph(nodes["yetiShape"].attr_text("pds")))
    groom = parse_strands(nodes["groomShape"].attr_tokens("pds"))
"""

import base64
import re
import xml.etree.ElementTree as ET

YETI_TYPES = ("pgYetiMaya", "pgYetiGroom")
# Strand attributes Yeti keeps for itself: not painted, never a guide layer.
GROOM_INTERNAL = ("P", "N", "s", "t", "numVertices", "strandSelectionStateInternal", "radiusOfInfluence",
                  "automaticPartingAngleThreshold", "automaticPartingReferencePosition", "automaticParting",
                  "system_type", "maxNumberOfGuideInfluences", "partRandomness", "hasExplicitGuideReference",
                  "id", "width")
_WIDTHS = {0: 1, 1: 1, 2: 3}


class FormatError(ValueError):
    pass


# ------------------------------------------------------------------------------------------------
# Maya ASCII


class MaNode(object):
    """A node of a .ma file: its type, name, parent, and setAttr values ({".attr": (type, tokens)},
    tokens as (text, quoted))."""

    def __init__(self, type_, name, parent=""):
        self.type = type_
        self.name = name
        self.parent = parent
        self.attrs = {}

    def attr(self, name):
        return self.attrs.get(name if name.startswith(".") else "." + name)

    def attr_tokens(self, name):
        a = self.attr(name)
        return [t for t, _ in a[1]] if a else None

    def attr_text(self, name):
        """A string attribute's text: its quoted parts joined (Maya splits long strings with +)."""
        a = self.attr(name)
        if not a:
            return None
        return "".join(t for t, q in a[1] if q)

    def __repr__(self):
        return "MaNode(%s %s)" % (self.type, self.name)


def ma_statements(text):
    """The statements of a .ma's text (split at ; outside strings), each a list of tokens (text,
    quoted): strings unescaped, the rest split at white space."""
    out, tokens, i, n = [], [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j, buf = i + 1, []
            while j < n and text[j] != '"':
                if text[j] == "\\" and j + 1 < n:
                    nxt = text[j + 1]
                    buf.append({"n": "\n", "t": "\t", "r": "\r"}.get(nxt, nxt))
                    j += 2
                    continue
                buf.append(text[j])
                j += 1
            tokens.append(("".join(buf), True))
            i = j + 1
        elif c == ";":
            if tokens:
                out.append(tokens)
            tokens = []
            i += 1
        elif c.isspace():
            i += 1
        elif c == "/" and text.startswith("//", i) and not tokens:
            j = text.find("\n", i)
            i = n if j < 0 else j + 1
        else:
            j = i
            while j < n and not text[j].isspace() and text[j] not in ';"':
                j += 1
            tokens.append((text[i:j], False))
            i = j
    if tokens:
        out.append(tokens)
    return out


def _flag(tokens, flag):
    for k, (t, q) in enumerate(tokens):
        if not q and t == flag and k + 1 < len(tokens):
            return tokens[k + 1][0]
    return None


def read_ma(text, types=YETI_TYPES):
    """({name: MaNode} of the nodes of `types` in a .ma's text, [(source plug, destination plug)]
    of every connectAttr touching them). A name used twice keys by parent|name too."""
    nodes, connections, current = {}, [], None
    for st in ma_statements(text):
        head = st[0][0]
        if head == "createNode":
            type_ = st[1][0] if len(st) > 1 else ""
            name = _flag(st, "-n") or ""
            if type_ in types:
                current = MaNode(type_, name, _flag(st, "-p") or "")
                key = name if name not in nodes else "%s|%s" % (current.parent, name)
                nodes[key] = current
            else:
                current = None
        elif head == "setAttr" and current is not None:
            k = 1
            while k < len(st) and not (st[k][1] and st[k][0].startswith(".")):
                k += 1
            if k >= len(st):
                continue
            attr = st[k][0]
            rest = st[k + 1:]
            type_ = None
            if len(rest) >= 2 and rest[0] == ("-type", False):
                type_, rest = rest[1][0], rest[2:]
            rest = [t for t in rest if t != ("+", False)]
            current.attrs[attr] = (type_, rest)
        elif head == "connectAttr":
            plugs = [t for t, q in st[1:] if q]
            if len(plugs) >= 2:
                connections.append((plugs[0], plugs[1]))
        elif head in ("select", "relationship", "fileInfo", "requires", "currentUnit", "file", "workspace"):
            current = None
        elif head not in ("setAttr", "rename", "addAttr", "lockNode", "parent", "dataStructure"):
            current = None
    names = set(n.name for n in nodes.values())

    def node_of(plug):
        return plug.split(".")[0].lstrip(":").split("|")[-1]

    touching = [(a, b) for a, b in connections if node_of(a) in names or node_of(b) in names]
    return nodes, touching


# ------------------------------------------------------------------------------------------------
# The graph


class GraphParam(object):
    def __init__(self, name, value, attachment=None):
        self.name = name
        self.value = value
        self.attachment = attachment

    def __repr__(self):
        return "GraphParam(%s=%r%s)" % (self.name, self.value, " <- %s" % self.attachment if self.attachment else "")


class GraphNode(object):
    """A Yeti graph node: `inputs` (the output id feeding each input, None where unconnected),
    `outputs` (its output ids), `params` ({name: GraphParam}), `config` (a curve's keys' text)."""

    def __init__(self, id_, name, type_):
        self.id = id_
        self.name = name
        self.type = type_
        self.inputs = []
        self.outputs = []
        self.params = {}
        self.config = ""

    def value(self, name, default=None):
        p = self.params.get(name)
        return default if p is None else p.value

    def number(self, name, default=0.0):
        return to_number(self.value(name), default)

    def flag(self, name, default=False):
        v = self.value(name)
        if v is None:
            return default
        return str(v).strip().lower() in ("true", "1", "yes", "on")

    def __repr__(self):
        return "GraphNode(%s %r)" % (self.type, self.name)


class Graph(object):
    """A parsed <yeti-graph>: `nodes` ({id: GraphNode}, in file order), `root` (the root node's id
    or None), `version`."""

    def __init__(self):
        self.nodes = {}
        self.root = None
        self.version = ""
        self._by_output = {}

    def node_of_output(self, output_id):
        return self._by_output.get(output_id)

    def input_node(self, node, index):
        """The node feeding input `index` of `node`, or None."""
        if index >= len(node.inputs) or not node.inputs[index]:
            return None
        return self._by_output.get(node.inputs[index])

    def consumers(self, node):
        """The nodes reading `node`'s outputs through an input or a param attachment."""
        outs = set(node.outputs)
        found = []
        for n in self.nodes.values():
            if any(i in outs for i in n.inputs if i) or any(p.attachment in outs for p in n.params.values()):
                found.append(n)
        return found

    def root_node(self):
        """The root (globals' root), else the one node nothing reads that has inputs."""
        if self.root in self.nodes:
            return self.nodes[self.root]
        ends = [n for n in self.nodes.values() if n.inputs and not self.consumers(n)]
        return ends[0] if len(ends) == 1 else None


def decode_graph(value):
    """A pgYetiMaya `.pds` value (base64) as the graph's XML text."""
    text = re.sub(r"\s+", "", value or "")
    if not text:
        raise FormatError("the graph data is empty")
    try:
        data = base64.b64decode(text + "=" * (-len(text) % 4))
    except Exception as e:
        raise FormatError("the graph data is not base64: %s" % e)
    for encoding in ("utf-8", "latin-1"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    raise FormatError("the graph data is not text")


def encode_graph(xml_text):
    """The `.pds` value for a graph's XML text (tests, writing scenes)."""
    return base64.b64encode(xml_text.encode("utf-8")).decode("ascii")


def parse_graph(xml_text):
    """A <yeti-graph> XML text as a Graph."""
    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError as e:
        raise FormatError("the graph's XML does not parse: %s" % e)
    if root.tag != "yeti-graph":
        raise FormatError("not a Yeti graph (<%s>)" % root.tag)
    g = Graph()
    g.version = root.get("version", "")
    for el in root.iter("node"):
        n = GraphNode(el.get("id"), el.get("name") or el.get("id"), el.get("type") or "")
        ins = el.find("inputs")
        for r in (list(ins) if ins is not None else []):
            v = r.get("value")
            n.inputs.append(None if not v or v == "-" else v)
        outs = el.find("outputs")
        for r in (list(outs) if outs is not None else []):
            n.outputs.append(r.get("id"))
            g._by_output[r.get("id")] = n
        for p in el.findall("param"):
            n.params[p.get("name")] = GraphParam(p.get("name"), p.get("value", ""), p.get("attachment") or None)
        cfg = el.find("config")
        n.config = (cfg.text or "") if cfg is not None else ""
        g.nodes[n.id] = n
    glob = root.find("globals")
    if glob is not None:
        for m in glob.findall("metadata"):
            if m.get("name") == "root":
                g.root = m.get("value")
    return g


def curve_keys(config):
    """A curve node's <config> as [(position, value, interpolation)], by position: boost's text
    archive (22 serialization::archive <version>) of position, value, interpolation triples (with
    or without a count first)."""
    tokens = (config or "").split()
    if "serialization::archive" in tokens:
        tokens = tokens[tokens.index("serialization::archive") + 2:]
    nums = []
    for t in tokens:
        try:
            nums.append(float(t))
        except ValueError:
            return []
    if len(nums) % 3 == 1 and nums and int(nums[0]) == (len(nums) - 1) // 3:
        nums = nums[1:]
    if len(nums) % 3:
        return []
    keys = [(nums[i], nums[i + 1], int(nums[i + 2])) for i in range(0, len(nums), 3)]
    return sorted(keys, key=lambda k: k[0])


def to_number(text, default=0.0):
    try:
        return float(str(text).strip())
    except (TypeError, ValueError):
        return default


def to_vector(text):
    """"[x, y, z]" (or "s * [x, y, z]") as a tuple, else None."""
    t = str(text or "").strip()
    scale = 1.0
    m = re.match(r"^([-+]?[\d.eE+-]+)\s*\*\s*(\[.*\])$", t) or re.match(r"^(\[.*\])\s*\*\s*([-+]?[\d.eE+-]+)$", t)
    if m:
        a, b = m.group(1), m.group(2)
        num, vec = (a, b) if not a.startswith("[") else (b, a)
        scale, t = to_number(num, 1.0), vec
    m = re.match(r"^\[\s*([^,\]]+),\s*([^,\]]+),\s*([^,\]]+)\]$", t)
    if not m:
        return None
    try:
        return tuple(float(m.group(i)) * scale for i in (1, 2, 3))
    except ValueError:
        return None


# ------------------------------------------------------------------------------------------------
# Grooms


class GroomStrands(object):
    """A pgYetiGroom's strands: `strands` (point lists, root first), `attributes` ({name: values}:
    per strand, len == strand count), `point_attributes` (per point), `constants` ({name: value}),
    `types` ({name: Yeti type})."""

    def __init__(self, name):
        self.name = name
        self.strands = []
        self.attributes = {}
        self.point_attributes = {}
        self.constants = {}
        self.types = {}

    @property
    def count(self):
        return len(self.strands)

    def painted(self):
        """The per-strand float attributes a groomer painted (not Yeti's own)."""
        return [k for k, t in self.types.items() if k in self.attributes and t == 1 and k not in GROOM_INTERNAL]


def parse_strands(tokens):
    """A pgYetiGroom `.pds` value (its tokens) as GroomStrands."""
    if isinstance(tokens, str):
        tokens = tokens.split()
    if not tokens or not re.match(r"^PGYS\d+$", tokens[0]):
        raise FormatError("not Yeti strand data (starts %r)" % (tokens[0] if tokens else ""))
    if len(tokens) < 3:
        raise FormatError("the strand data is cut short")
    out = GroomStrands(tokens[1])
    try:
        count = int(tokens[2])
    except ValueError:
        raise FormatError("the strand data's attribute count is %r" % tokens[2])
    raw = {}
    i = 3
    for _ in range(count):
        if i + 4 > len(tokens):
            raise FormatError("the strand data is cut short")
        name = tokens[i]
        try:
            type_, n = int(tokens[i + 1]), int(tokens[i + 3])
        except ValueError:
            raise FormatError("attribute %r: type and count %r %r" % (name, tokens[i + 1], tokens[i + 3]))
        i += 4
        width = _WIDTHS.get(type_)
        if width is None:
            raise FormatError("attribute %r has a type Yeti's strand data was not known to use (%d)" % (name, type_))
        vals = tokens[i:i + n * width]
        if len(vals) < n * width:
            raise FormatError("attribute %r is cut short" % name)
        i += n * width
        nums = [float(v) for v in vals]
        if width == 3:
            nums = [tuple(nums[k:k + 3]) for k in range(0, len(nums), 3)]
        raw[name] = (type_, nums)
        out.types[name] = type_
    if "P" not in raw or "numVertices" not in raw:
        raise FormatError("the strand data has no P or numVertices")
    points = raw["P"][1]
    counts = [int(c) for c in raw["numVertices"][1]]
    if sum(counts) != len(points):
        raise FormatError("numVertices adds up to %d points, P has %d" % (sum(counts), len(points)))
    k = 0
    for c in counts:
        out.strands.append(points[k:k + c])
        k += c
    strands, total = len(counts), len(points)
    for name, (type_, vals) in raw.items():
        if name in ("P", "numVertices"):
            continue
        if len(vals) == strands and strands != 1:
            out.attributes[name] = vals
        elif len(vals) == total:
            out.point_attributes[name] = vals
        elif len(vals) == 1:
            out.constants[name] = vals[0]
            if strands == 1:
                out.attributes[name] = vals
    return out


def format_strands(name, strands, attributes=None, constants=None):
    """The tokens of a pgYetiGroom `.pds` value (tests, writing scenes): `strands` point lists,
    `attributes` {name: value per strand} (floats), `constants` {name: int}."""
    attributes = attributes or {}
    constants = constants or {}
    points = [p for s in strands for p in s]
    parts = [("P", 2, 1, points), ("numVertices", 0, 0, [len(s) for s in strands])]
    parts.append(("N", 2, 1, [(0.0, 1.0, 0.0)] * len(strands)))
    for k, v in constants.items():
        parts.append((k, 0, 0, [v]))
    for k, v in attributes.items():
        parts.append((k, 1, 0, list(v)))
    tokens = ["PGYS10", name, str(len(parts))]
    for pname, type_, flag, values in parts:
        tokens += [pname, str(type_), str(flag), str(len(values))]
        for v in values:
            if isinstance(v, tuple):
                tokens += ["%g" % c for c in v]
            else:
                tokens.append("%g" % v if type_ else str(int(v)))
    return tokens
