# SPDX-License-Identifier: Apache-2.0
"""The mapping layer every importer shares: a foreign groom (XGen's, Yeti's) described in
GroomLab's terms, then built as a GroomLab groom.

A reader (importers/xgen.py, importers/yeti.py) turns a source's own stack (XGen's modifiers or FX modules, Yeti's
graph nodes) into a ForeignGroom: the scalp, guide curves, the strand density and an ordered list
of ForeignOps, each one GroomLab node's worth of settings (role "clump", "noise", "curl",
"length", "cut", "width", "color", "displace", "twist", "flyaway") with the maps that drive its
params (MapSource: an image file, a Ptex file, an expression, or a guide layer the import makes
from the source's per-guide values: ForeignGroom.guide_layers). It notes what it mapped, approximated and skipped in an
ImportReport as it goes. build_groom then makes the groom: the guides, the maps (files found once
by importers/paths.py and copied into the scene's masks folder by importers/textures.py), and the
graph in the usual order, laid out left to right:

    Scalp, Guides -> Scatter -> Interpolate -> [Transform: length] -> the look ops in the source's
    order (Length for cuts, Clump, Noise, Curl) -> [HairColor] -> Width -> Resample -> Output

Nothing in here knows XGen or Yeti: each reader fills the same classes (Yeti's: its scatter
density and texture, guide curves and painted attributes from its groom, clumping, scraggle, width
and length nodes as ops).
"""

import json

# The roles a ForeignOp can take: the GroomLab node type each becomes, and where it goes. The look
# roles keep the order the source gives them; length (a scale) comes first, color and width last.
ROLE_TYPES = {
    "length": "Transform",  # a length scale: strand-space scale about the root (mappable)
    "cut": "Length",        # trimmed tips: keep a share of each strand
    "clump": "Clump",
    "noise": "Noise",
    "curl": "Curl",
    "color": "HairColor",
    "width": "Width",
    "displace": "Displace",
    "twist": "Twist",
    "flyaway": "Flyaway",
}
_LEADING = ("length",)
_TRAILING = ("color", "width")


class MapSource(object):
    """Where a param's value comes from, per strand root: an image (`kind` "file"), a Ptex file
    ("ptex"), an expression ("expression": `expression` holds GroomLab's text, with map("name")
    reading the maps in `maps`), or a guide layer of the groom ("layer": `name`, made from
    ForeignGroom.guide_layers; no file). `path` is the path as the source wrote it (tokens and variables
    unexpanded), `resolver` the PathResolver that expands it (paths.py), `name` the GroomLab map
    name wanted, `channel` what of it the param reads ("luminance", "r", "g", "b", "a", or "rgb"
    for a color), `color` whether to keep it in color (None: as the image is). `is_dir`: the path
    names a folder holding the file (XGen's ${DESC}/paintmaps/<map>/), `file_names` the names
    looked for in it."""

    def __init__(self, kind, path="", name="", channel="luminance", expression="", maps=None, color=None,
                 resolver=None, is_dir=False, file_names=(), source="", value_range=None):
        self.kind = kind
        self.path = path
        self.name = name
        self.channel = channel
        self.expression = expression
        self.maps = list(maps or [])
        self.color = color
        self.resolver = resolver
        self.is_dir = is_dir
        self.file_names = list(file_names)
        self.source = source  # where the source named it, for the report ("Clumping1.clump")
        # An expression's 0-1 remapped to (min, max): the param is min + (max - min) x the expression,
        # and max its largest value (what Scatter's density samples up to).
        self.value_range = tuple(value_range) if value_range else (0.0, 1.0)

    def __repr__(self):
        return "MapSource(%s, %r -> %r)" % (self.kind, self.path or self.expression, self.name)


class ForeignOp(object):
    """One GroomLab node's worth of a foreign stack: `role` (ROLE_TYPES), `params` (the node's
    params in GroomLab's names and units), `maps` ({param: MapSource}: the param reads the map,
    times its own value), `source` (the foreign node, for the report), `inputs` ({input: "guides"
    or "scalp"}: more of the node's inputs fed by the graph's Guides or Scalp node, e.g. a Clump's
    centers at the guides, or a ForeignBranch: strands of their own, e.g. Yeti's clump curves)."""

    def __init__(self, role, params=None, maps=None, source="", inputs=None, per_strand=None):
        if role not in ROLE_TYPES:
            raise ValueError("unknown role %r" % role)
        self.role = role
        self.params = dict(params or {})
        self.maps = dict(maps or {})
        self.source = source
        self.inputs = dict(inputs or {})
        # {param: amount per strand}: a param counted per unit length that the source counts per
        # strand (XGen's coil turns): set to amount / the strands' median length once built.
        self.per_strand = dict(per_strand or {})


class ForeignBranch(object):
    """Strands of their own feeding an op's input (ForeignOp.inputs): a Scatter (`density`,
    `density_map`, more `scatter` params), an Interpolate on the groom's guides, then `ops`, e.g.
    Yeti's clump curves for a Clump's centers."""

    def __init__(self, density=1.0, density_map=None, ops=None, source=""):
        self.density = float(density)
        self.density_map = density_map
        self.scatter = {}
        self.ops = list(ops or [])
        self.source = source
        self.segments = 16


def all_ops(ops):
    """`ops` and the ops of the branches they read, each once."""
    out = []
    for op in ops:
        out.append(op)
        for feed in op.inputs.values():
            if isinstance(feed, ForeignBranch):
                out.extend(all_ops(feed.ops))
    return out


class ForeignGroom(object):
    """A foreign groom in GroomLab's terms. `guides`: curves as lists of (x, y, z), world space,
    root first. `density`: strands per unit area (Scatter's density), `density_map` a MapSource
    multiplying it, `strand_count` the source's own count (for the report). `ops`: ForeignOps.
    `scatter`: more Scatter params (e.g. guideRadius: hair only near the guides). `interpolate`:
    Interpolate's params. `guide_layers`: {name: (value per guide, default)}, made as guide layers
    (painted per-guide values: Yeti's groom attributes) that "layer" MapSources read."""

    def __init__(self, name, scalp, guides=None, density=80.0, density_map=None, ops=None, strand_count=0,
                 segments=16, source=""):
        self.scatter = {}
        self.interpolate = {}
        self.guide_layers = {}
        self.name = name
        self.scalp = scalp
        self.guides = list(guides or [])
        self.density = float(density)
        self.density_map = density_map
        self.ops = list(ops or [])
        self.strand_count = strand_count
        self.segments = segments
        self.source = source


class ImportReport(object):
    """What an import did, per groom: mapped (taken over as is), approximated (taken over in
    GroomLab's terms, not exactly), skipped (left out), files (each source file -> its copy) and
    missing (each file not found, with every path it was looked for at and the variables that
    were set nowhere)."""

    SECTIONS = ("mapped", "approximated", "skipped", "files", "missing")

    def __init__(self):
        self.grooms = []  # [{"name", "source", "groom", "mapped": [...], ...}]
        self._current = None

    def begin(self, name, source):
        entry = {"name": name, "source": source, "groom": None}
        for s in self.SECTIONS:
            entry[s] = []
        self.grooms.append(entry)
        self._current = entry
        return entry

    def note(self, section, what, detail=""):
        if section not in self.SECTIONS:
            raise ValueError(section)
        if self._current is None:
            self.begin("", "")
        item = {"what": what, "detail": detail}
        if item not in self._current[section]:
            self._current[section].append(item)

    def mapped(self, what, detail=""):
        self.note("mapped", what, detail)

    def approximated(self, what, detail=""):
        self.note("approximated", what, detail)

    def skipped(self, what, detail=""):
        self.note("skipped", what, detail)

    def file(self, source, copy, as_map):
        self._current["files"].append({"what": as_map, "detail": "%s -> %s" % (source, copy), "source": source,
                                       "copy": copy})

    def missing(self, what, path, tried=(), unset=()):
        self._current["missing"].append({"what": what, "detail": path, "path": path, "tried": list(tried),
                                         "unset": list(unset)})

    @property
    def unset_variables(self):
        """Every variable a missing file's path needed and nothing set: what the Roots field wants."""
        out = []
        for g in self.grooms:
            for m in g["missing"]:
                for v in m["unset"]:
                    if v not in out:
                        out.append(v)
        return out

    def as_dict(self):
        return {"grooms": self.grooms, "unset_variables": self.unset_variables}

    def text(self):
        lines = []
        for g in self.grooms:
            lines.append("%s (%s) -> %s" % (g["name"], g["source"], g["groom"] or "not imported"))
            for section in self.SECTIONS:
                if not g[section]:
                    continue
                lines.append("  %s:" % section)
                for item in g[section]:
                    lines.append("    %s%s" % (item["what"], (": " + item["detail"]) if item["detail"] else ""))
                    if section == "missing":
                        for t in item["tried"]:
                            lines.append("      looked at %s" % t)
                        if item["unset"]:
                            names = ", ".join("${%s}" % v for v in item["unset"])
                            lines.append("      set nowhere: %s (give it a root)" % names)
        return "\n".join(lines)

    def __str__(self):
        return self.text()


def ramp_keys(points, linear=False):
    """A ramp param's JSON from [(position, value), ...] (GroomLab smooth keys, or linear)."""
    keys = sorted([[float(p), float(v)] for p, v in points], key=lambda k: k[0]) or [[0.0, 1.0]]
    return {"keys": keys, "linear": True} if linear else keys


def ordered_ops(ops):
    """The ops in graph order: length scales, the look ops as the source has them, color, width."""
    lead = [o for o in ops if o.role in _LEADING]
    trail = sorted([o for o in ops if o.role in _TRAILING], key=lambda o: _TRAILING.index(o.role))
    look = [o for o in ops if o.role not in _LEADING and o.role not in _TRAILING]
    return lead + look + trail


def graph_for(foreign, map_names):
    """The graph (a dict) of a ForeignGroom, its ops' MapSources resolved to the GroomLab maps in
    `map_names` ({id(MapSource): map name}; a source missing there leaves the param at its value):
    a Map node per map, wired into the params reading it."""
    from groomlab.graph import LAYOUT_DX, LAYOUT_DY
    nodes, connections = [], []
    names = set()

    def unique(base):
        name, n = base, 2
        while name in names:
            name, n = "%s%d" % (base, n), n + 1
        names.add(name)
        return name

    column = [0]

    def add(type_, base, params=None, row=0, advance=True):
        name = unique(base)
        node = {"name": name, "type": type_, "ui": {"x": column[0] * LAYOUT_DX, "y": row * LAYOUT_DY}}
        if params:
            node["params"] = params
        nodes.append(node)
        if advance:
            column[0] += 1
        return node

    map_nodes = {}

    def link(node, param, source):
        """Wire map `source` into node.param: a Map node per map (reused), or an expression in place."""
        if source.kind == "expression":
            lo, hi = source.value_range
            node.setdefault("params", {})[param] = {"expression": _rename_maps(source, map_names), "min": float(lo),
                                                    "max": float(hi)}
            return
        map_name = map_names.get(id(source))
        if not map_name:
            return
        if map_name not in map_nodes:
            below = LAYOUT_DY * (1 + len(map_nodes) % 3)
            m = {"name": unique(map_name), "type": "Map", "params": {"name": map_name},
                 "ui": {"x": node["ui"]["x"] - LAYOUT_DX // 2, "y": node["ui"]["y"] + below}}
            nodes.append(m)
            map_nodes[map_name] = m["name"]
        node.setdefault("paramInputs", [])
        if param not in node["paramInputs"]:
            node["paramInputs"].append(param)
        c = {"from": map_nodes[map_name], "to": node["name"], "input": param}
        if source.channel and source.channel != "luminance":
            c["output"] = "rgb" if source.channel == "rgb" else source.channel
        connections.append(c)

    scalp = add("Scalp", "scalp", row=1, advance=False)
    guides = add("Guides", "guides", row=-1)
    scatter_params = dict(foreign.scatter)
    scatter_params["density"] = foreign.density
    scatter = add("Scatter", "scatter", scatter_params)
    if foreign.density_map is not None:
        link(scatter, "density", foreign.density_map)
    interpolate = add("Interpolate", "interpolate", dict(foreign.interpolate) or None)
    connections += [{"from": scalp["name"], "to": scatter["name"], "input": "scalp"},
                    {"from": scatter["name"], "to": interpolate["name"], "input": "strands"},
                    {"from": guides["name"], "to": interpolate["name"], "input": "guides"},
                    {"from": scalp["name"], "to": interpolate["name"], "input": "scalp"}]
    if scatter_params.get("guideRadius"):
        connections.append({"from": guides["name"], "to": scatter["name"], "input": "guides"})
    previous = interpolate
    op_nodes = {}  # id(op): its node's name
    branches = [0]

    def branch(feed, x):
        """A ForeignBranch's nodes (Scatter, Interpolate, its ops) on a row of their own below the
        main one, ending at column x: its last node."""
        branches[0] += 1
        row = 1 + branches[0]
        count = 2 + len(feed.ops)
        saved = column[0]
        column[0] = max(0, x - count)
        params = dict(feed.scatter)
        params["density"] = feed.density
        b_scatter = add("Scatter", "clumpScatter", params, row=row)
        if feed.density_map is not None:
            link(b_scatter, "density", feed.density_map)
        b_interp = add("Interpolate", "clumpInterpolate", row=row)
        connections.extend([{"from": scalp["name"], "to": b_scatter["name"], "input": "scalp"},
                            {"from": b_scatter["name"], "to": b_interp["name"], "input": "strands"},
                            {"from": guides["name"], "to": b_interp["name"], "input": "guides"},
                            {"from": scalp["name"], "to": b_interp["name"], "input": "scalp"}])
        last = b_interp
        for b_op in feed.ops:
            n = add(ROLE_TYPES[b_op.role], "clump" + b_op.role.capitalize(), dict(b_op.params), row=row)
            op_nodes[id(b_op)] = n["name"]
            for param, source in b_op.maps.items():
                link(n, param, source)
            connections.append({"from": last["name"], "to": n["name"], "input": "strands"})
            last = n
        column[0] = saved
        return last

    for op in ordered_ops(foreign.ops):
        node = add(ROLE_TYPES[op.role], op.role, dict(op.params))
        op_nodes[id(op)] = node["name"]
        for param, source in op.maps.items():
            link(node, param, source)
        for input_name, feed in op.inputs.items():
            if isinstance(feed, ForeignBranch):
                source_node = branch(feed, column[0] - 1)
            else:
                source_node = {"guides": guides, "scalp": scalp}[feed]
            connections.append({"from": source_node["name"], "to": node["name"], "input": input_name})
        connections.append({"from": previous["name"], "to": node["name"], "input": "strands"})
        previous = node
    foreign.op_nodes = op_nodes
    resample = add("Resample", "resample", {"segments": int(foreign.segments)})
    output = add("Output", "output")
    connections += [{"from": previous["name"], "to": resample["name"], "input": "strands"},
                    {"from": resample["name"], "to": output["name"], "input": "strands"}]
    return {"version": 1, "nodes": nodes, "connections": connections, "output": output["name"]}


def _rename_maps(source, map_names):
    """An expression's map("<as the source named it>") calls reading the GroomLab maps made for them;
    a call whose map is missing (its file not found) reads white: 1 (cmap: [1, 1, 1])."""
    import re
    text = source.expression
    for m in source.maps:
        name = map_names.get(id(m))
        if not m.path:
            continue
        if name:
            for quote in ("'", '"'):
                text = text.replace("map(%s%s%s" % (quote, m.path, quote), 'map("%s"' % name)
                text = text.replace("cmap(%s%s%s" % (quote, m.path, quote), 'cmap("%s"' % name)
        else:
            call = re.compile(r"""(?<![\w])(c|v)?map\(\s*(['"])%s\2\s*(?:,[^)]*)?\)""" % re.escape(m.path))
            text = call.sub(lambda c: "[1, 1, 1]" if c.group(1) else "1", text)
    return text


def map_sources(foreign):
    """Every MapSource a ForeignGroom reads (expressions' maps included), each once, in order."""
    out, seen = [], set()

    def add(s):
        if s is None or id(s) in seen:
            return
        seen.add(id(s))
        if s.kind == "expression":
            for m in s.maps:
                add(m)
        elif s.kind != "layer":
            out.append(s)

    add(foreign.density_map)
    for op in all_ops(foreign.ops):
        for s in op.maps.values():
            add(s)
        for feed in op.inputs.values():
            if isinstance(feed, ForeignBranch):
                add(feed.density_map)
    return out


def build_groom(foreign, report, map_resolution=1024):
    """Make the GroomLab groom of a ForeignGroom: the groom on its scalp, its guides, its maps
    (found and copied: textures.import_map_source) and its graph. Returns the groom shape."""
    from maya import cmds
    import groomlab
    from groomlab.importers import textures

    groom = groomlab.create_groom(foreign.scalp, name=_node_name(foreign.name))
    report._current["groom"] = groom
    ids = []
    if foreign.guides:
        curves, kept = [], []
        try:
            for i, points in enumerate(foreign.guides):
                if len(points) >= 2:
                    curves.append(cmds.curve(degree=1, point=[tuple(p) for p in points]))
                    kept.append(i)
            if curves:
                ids = list(zip(kept, groomlab.guides_from_curves(groom, curves) or []))
        finally:
            if curves:
                cmds.delete(curves)
        report.mapped("guides", "%d guide curves" % len(curves))
    map_names = {}
    layers = _guide_layers(groom, foreign, ids, report) if foreign.guide_layers else set()
    imported = {}  # each file once: the same file read by several params (other channels) shares a map
    for source in map_sources(foreign):
        key = _file_key(source)
        if key not in imported:
            imported[key] = textures.import_map_source(groom, source, report, resolution=map_resolution)
        if imported[key]:
            map_names[id(source)] = imported[key]
    for source in _layer_sources(foreign):
        if source.name in layers:
            map_names[id(source)] = source.name
    graph = graph_for(foreign, map_names)
    groomlab.set_graph(groom, graph)
    _per_strand(groom, foreign, graph, report)
    return groom


def _file_key(source):
    """What makes two MapSources the same file: kind, the path as expanded, folder lookups, color."""
    path = source.path
    if source.resolver is not None:
        path = source.resolver.expand(source.path)[0]
    return (source.kind, (path or "").replace("\\", "/").lower(), source.is_dir, tuple(source.file_names),
            source.color)


def _layer_sources(foreign):
    """Every "layer" MapSource the groom's params and expressions read."""
    out = []

    def add(s):
        if s is None:
            return
        if s.kind == "layer":
            out.append(s)
        elif s.kind == "expression":
            for m in s.maps:
                add(m)

    add(foreign.density_map)
    for op in all_ops(foreign.ops):
        for s in op.maps.values():
            add(s)
        for feed in op.inputs.values():
            if isinstance(feed, ForeignBranch):
                add(feed.density_map)
    return out


def _guide_layers(groom, foreign, ids, report):
    """foreign.guide_layers as guide layers of the groom: each guide's value written into the
    groom's guide data (one edit for every layer). Returns the names made."""
    import groomlab
    import groomlab_core
    from maya import cmds
    made = set()
    if not ids:
        return made
    data = json.loads(cmds.getAttr(groom + ".guideData") or "{}")
    by_id = dict((int(g["id"]), g) for g in data.get("guides", []))
    taken = set(groomlab.list_maps(groom))
    for name, (values, default) in sorted(foreign.guide_layers.items()):
        error = groomlab_core.map_name_error(name)
        if error or name in taken:
            report.skipped("groom attribute %s" % name, error or "the groom already has a map of that name")
            continue
        for index, gid in ids:
            g = by_id.get(int(gid))
            if g is not None:
                v = values[index] if index < len(values) else default
                g.setdefault("attributes", {})[name] = round(float(v), 5)
        made.add(name)
    if made:
        cmds.setAttr(groom + ".guideData", json.dumps(data, separators=(",", ":")), type="string")
    for name in sorted(made):
        report.mapped("groom attribute %s" % name, "a guide layer: each guide's value, blended into the strands "
                                                   "from their guides")
    return made


def _per_strand(groom, foreign, graph, report):
    """Params a source counts per strand (ForeignOp.per_strand) set per unit length from the
    strands' median length, measured with those nodes switched off (affect 0)."""
    import groomlab
    ops = [(i, op) for i, op in enumerate(all_ops(foreign.ops)) if op.per_strand]
    if not ops:
        return
    by_name = dict((n["name"], n) for n in graph["nodes"])
    names = getattr(foreign, "op_nodes", {})
    nodes = dict((id(op), by_name[names[id(op)]]) for _, op in ops if names.get(id(op)) in by_name)
    saved = {}
    for _, op in ops:
        node = nodes.get(id(op))
        if node:
            saved[node["name"]] = node.setdefault("params", {}).get("affect", 1.0)
            node["params"]["affect"] = 0.0
    groomlab.set_graph(groom, graph)
    length = median_strand_length(groom)
    for _, op in ops:
        node = nodes.get(id(op))
        if not node:
            continue
        node["params"]["affect"] = saved[node["name"]]
        for param, amount in op.per_strand.items():
            node["params"][param] = amount / max(length, 1e-6)
            report.approximated(op.source, "%s %g per strand: %.4g per unit at the strands' median length %.4g" %
                                (param, amount, node["params"][param], length))
    groomlab.set_graph(groom, graph)


def median_strand_length(groom, sample=2000):
    """The median length of a groom's strands as it renders (an Alembic export, read back)."""
    import os
    import shutil
    import tempfile
    import groomlab
    import groomlab_core
    folder = tempfile.mkdtemp(prefix="groomlab_length_")
    try:
        path = os.path.join(folder, "strands.abc")
        groomlab.export_alembic(groom, path, scales="none")
        strands = groomlab_core.read_alembic_curves_at(path)
        step = max(1, strands.strand_count // sample)
        lengths = []
        for i in range(0, strands.strand_count, step):
            pts = strands.strand_points(i)
            lengths.append(sum(((a.x - b.x) ** 2 + (a.y - b.y) ** 2 + (a.z - b.z) ** 2) ** 0.5
                               for a, b in zip(pts, pts[1:])))
        lengths.sort()
        return lengths[len(lengths) // 2] if lengths else 1.0
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def _node_name(name):
    import re
    clean = re.sub(r"[^A-Za-z0-9_]", "_", name or "groom").strip("_") or "groom"
    if clean[0].isdigit():
        clean = "g_" + clean
    return clean + "_groomLab"


def dumps(foreign):
    """A ForeignGroom as JSON text (debugging, tests)."""
    def src(s):
        return None if s is None else {"kind": s.kind, "path": s.path, "name": s.name, "channel": s.channel,
                                       "expression": s.expression}
    return json.dumps({"name": foreign.name, "scalp": foreign.scalp, "guides": len(foreign.guides),
                       "guide_layers": sorted(foreign.guide_layers), "interpolate": foreign.interpolate,
                       "scatter": foreign.scatter, "segments": foreign.segments,
                       "density": foreign.density, "density_map": src(foreign.density_map),
                       "ops": [{"role": o.role, "params": o.params, "maps": {k: src(v) for k, v in o.maps.items()},
                                "source": o.source} for o in all_ops(foreign.ops)]}, indent=2, default=str)
