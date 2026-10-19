<!-- SPDX-License-Identifier: Apache-2.0 -->
# Custom nodes: design (draft for review)

Status: proposal, 2026-10-18. Nothing here is built yet.

## Goal

Anyone can make a node of their own, share it as one file, and anyone else can drop that file into
their library and use it like a built-in node, **without compiling anything**.

A custom node must work wherever a groom is evaluated, not only in Maya:

- Maya, the standalone app, Blender (through its helper);
- the Arnold and RenderMan procedurals, on a render farm with no Maya or Python;
- groomlab-cli, the C API, Linux.

So a custom node cannot depend on Maya's Python, and because people will share nodes with
strangers, running one must be safe: no file or network access, nothing that can hang a render.
It must also be deterministic (the same bits on any thread count and platform), as every node is.

## Where we start from

A built-in node today is a C++ class compiled into the core: it declares ports, a list of
`ParamSpec`s and one `evaluate()`, and pulls its math from the shared library (docs/MATH.md).
Everything else comes from the specs automatically: the UI in every host, saving, the Python and
C APIs, Add Node search, value ports, paintable maps, unit scaling. `tools/dev/new_node.py` makes
a new one (docs/EXTENDING.md). That stays the way to add heavy, core features.

Two things we already have make custom nodes cheap:

- **Compounds** (Ctrl+G) with **value ports**: a subgraph with its params exposed, stored in the
  groom as a `definition` (graph JSON) on a Compound node.
- **The expression language** (docs: Expressions): a sandboxed, deterministic language with
  variables (`P`, `N`, `t`, `length`, channels...), vectors, `if`/`else`, assignments, ~60 functions
  (noise, fbm, voronoi, curves, `map()`), already evaluated in the core, partly on the GPU. Today
  it only produces a *field* (a value per strand or point that drives a param) or a single value.

## Three kinds of custom node

| Kind | Made of | Can do | Speed | Status |
| --- | --- | --- | --- | --- |
| **Compound node** | Built-in nodes (a saved compound) | Anything a graph of nodes can | As the nodes inside | Phase 1 |
| **Script node** | Built-in node ports + a script in the expression language | Move points, set widths, write channels, per strand and per point | Close to built-in for simple math; GPU where it can | Phase 2 |
| **Compiled plugin** | WebAssembly, compiled once by its author | Heavy algorithms | Near native | Later, if needed |

### 1. Compound nodes (no code)

**Save as Node...** on a compound (Graph Editor right-click, both hosts) writes it to the library:
its subgraph, the params you exposed (with your labels, ranges, defaults and descriptions), a
name, a category and an icon. It then appears in Add Node and Tab search under its category (or
**My Nodes**), and behaves as one node.

This is mostly plumbing on what exists: a compound's definition plus a header.

### 2. Script nodes (custom logic)

A script node has normal ports (strands in, strands out; optionally guides, scalp, fields), params
declared in its header, and a **script** that runs per strand and/or per point:

```
# Wave: bends each strand sideways along its length.
per point {
    $side = normalize(cross(tangent, N));
    P += $side * $amplitude * sin(t * $frequency * 2 * pi + rand * 6.28) * t;
}
```

The language is the expression language, extended for nodes:

- **Writable outputs:** `P` (the point), `width`, `color` (r, g, b), and any channel
  (`channel.myMask = ...` per point or per strand). A node declares which it writes in its header,
  so the graph knows what it changes (for incremental updates: a script that writes only `width`
  keeps every point, like a Width edit does today).
- **Params** are read as variables (`$amplitude`); a param marked mappable can be painted or driven
  by a field, exactly like a built-in node's.
- **Blocks:** `per strand { ... }` runs once per strand (it can set strand channels and locals the
  per-point block reads); `per point { ... }` runs for every point, root to tip, and can read the
  point before it (`prev.P`) for follow-the-leader style effects.
- **More inputs:** `tangent`, `prev`, `root`, `tip`, `pointCount`, the strand's guide
  (`guidePoint(t)`, `guideId`), the scalp (`scalpNormal(p)`, `closestOnScalp(p)`), maps (`map()`
  as today), tube coordinates (`tubeU`, `tubeV`) and the shared math library's functions.
- **Bounded loops only:** `for $i in 0 .. 8 { }` with a literal bound, so a script always
  finishes. There is no recursion, file, time or network access.
- **A cost cap per node** (steps per point), reported as a node error rather than hanging.

What a script node cannot do in phase 2: add or remove strands or points, or read other strands
(neighbours). Those need more machinery (see the open questions).

**How it runs:** the core parses the script once per edit into the same compact program the
expression language uses today, and runs it over strands in parallel (TBB), deterministically.
Where every function used has a GPU form, the GPU preview runs it too (as Expression fields do
now); otherwise the preview falls back to the CPU for that node, with a note.

### 3. Compiled plugins, later

For a heavy algorithm that would be slow in a script (a custom solver, a mesh-to-hair method),
a **WebAssembly** module rather than a DLL: authors compile once (C++, Rust, Zig...) to one `.wasm`
file that runs on every platform and host, sandboxed, through a small versioned API (read strands
and params, write strands). A DLL would break across compilers and GroomLab versions and could do
anything to a machine, a poor fit for sharing. Only worth building if phases 1 and 2 leave real
gaps.

## The node file

One file per node, `name.glnode` (JSON), readable and diffable:

```json
{
  "glnode": 1,
  "id": "nick.wave",
  "version": 3,
  "label": "Wave",
  "category": "Shape",
  "description": "Bends each strand sideways in a travelling wave.",
  "author": "Nick Rutlinh",
  "license": "CC0-1.0",
  "inputs": [{"name": "strands", "type": "Strands"}],
  "output": {"name": "strands", "type": "Strands"},
  "params": [
    {"name": "amplitude", "type": "length", "default": 0.5, "min": 0, "softMax": 5, "mappable": true,
     "description": "How far the wave swings"},
    {"name": "frequency", "type": "number", "default": 3, "min": 0, "softMax": 20,
     "description": "Waves along the strand"}
  ],
  "writes": ["P"],
  "script": "per point { ... }"
}
```

A compound node has `"graph": { ... }` (the compound's subgraph and its exposed params) in place of
`"script"`. `id` is a reverse-domain style name that never changes; `version` goes up when the
node's behaviour changes.

## The library

- **Where:** the same scheme as presets: `GROOMLAB_NODE_PATH` folders, then the user's folder
  (`~/Documents/GroomLab/nodes`, shared by Maya, the app and Blender), then nodes shipped with
  GroomLab (a few examples). An earlier folder hides a later one with the same `id`.
- **Install:** drop a `.glnode` into the folder, or **Import Node...** (Graph Editor), or drag the
  file onto the Graph Editor.
- **Share:** send the file. A **Node Library** page on the website, later, could list community
  nodes (moderation and trust are a later question).

## Grooms carry their nodes

A groom that uses a custom node **stores a copy of the node's file inside it** (as compounds store
their definition today). So a groom opens on another machine, on a render farm, in the app or in
Blender without that machine having the node installed, and keeps its look even if the library
copy changes.

When the library has a newer `version` of a node a groom uses, the Graph Editor shows a small
"update available" mark on the node; updating is a click (one undo step), never automatic.

## Editing

- The node's params show like any built-in node's (from its header).
- **Edit Node...** opens a script editor (Graph Editor side panel in Maya and the app): the script
  with syntax errors underlined, the header as a form (params, ports, label, category), live
  re-evaluation as you type (debounced), and **Save to Library** / **Save as New**.
- A compound node opens as its compound (as double-click does today), and Save to Library updates
  the file.

## Rules that keep it safe and stable

- **Sandboxed:** scripts reach only their inputs, params, maps and the math library.
- **Bounded:** literal loop bounds and a step budget; an over-budget node errors, never hangs.
- **Deterministic:** the portable math, no clocks, no unordered sums across strands.
- **Versioned:** `glnode` (the file format) and the language have versions; GroomLab reads every
  older one. New functions may be added; existing ones never change meaning.
- **Graceful:** a node file that fails to load becomes a pass-through node with an error saying
  why, so a groom never fails to open because of it.

## Phases

1. **Compound nodes:** the `.glnode` format, the library folders, Save as Node..., Add Node listing,
   grooms embedding their nodes, update marks. Maya and the app, then Blender. About a week.
2. **Script nodes:** the language extensions (writable outputs, blocks, the new inputs, bounded
   loops, cost cap), the node evaluator with incremental updates, the script editor, GPU where
   possible, docs and a handful of example nodes (Wave, Kink, Taper by channel, Random length
   by map). Two to three weeks.
3. **WebAssembly plugins:** only if phases 1 and 2 leave real gaps.

## Decisions (2026-10-18 review)

- Scripts modify existing strands and points only; adding or removing them is later.
- The license field on a shared node is free-form (optional).
- A community node page on the website: someday, not now.
- Names: **Compound node** (a saved compound) and **Script node**; the file extension `.glnode`.

## Open questions

1. **Neighbour queries** (a script reading nearby strands): in phase 2, or later? See below.

### What neighbour queries would allow

A script could ask "which strands are near this one, and where are they?" (read from the node's
input, so the answer never depends on the order strands are processed in), through functions such
as `neighbours(radius, count)`, `neighbourP(i, t)` and `density(radius)`. That opens:

- **Custom clumping:** gather strands toward the average of their neighbours, clump by any rule
  (by a channel, by age, by distance to a part line), or clump sizes that vary with local density.
- **Spacing and separation:** push strands apart where they bunch up, or pull strays back in.
- **Coherence:** noise, curl or direction averaged over neighbours, so nearby strands move together
  (locks that hold) while far ones differ.
- **Density-aware effects:** thin, frizz or lighten where hair is sparse; matte or darken where
  dense; find stray strands that sit far from all others.
- **Group behaviours:** twist or braid small groups around their shared center, flow that aligns
  each strand with its neighbours.

The cost: a spatial index built per evaluation (fast, but not free) and more work per point, so
such scripts run slower and are CPU-only at first. It is a contained addition on top of phase 2,
so it can come right after it.
