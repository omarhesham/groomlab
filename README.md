# GroomLab

A hair and fur system for Maya that pairs Tonic-style hierarchical guides
(guides, then child guides generated inside their parents' tubes) with a
Yeti-style procedural graph (scatter, interpolate, multi-level clump, noise, curl).

The grooming logic lives in a Maya-independent C++ core. The Maya plugin, the
Python module and the headless CLI are thin layers over that core.

Spec and roadmap: [docs/SPEC.md](docs/SPEC.md).

**Status:** M1 to M5 are built and tested; 1.0 now waits on the pilot (see
[docs/PILOT.md](docs/PILOT.md)). The user guide is [docs/USER_GUIDE.md](docs/USER_GUIDE.md);
the developer handbook (how the code fits together, build, test, install, crash and hang
diagnosis, the rules) is [docs/DEVELOPING.md](docs/DEVELOPING.md).
- **Guides:** hierarchies of any depth live on the groom, are edited with undoable commands,
  follow a moving or deforming scalp, and draw per level with tube rings.
- **Brushes:** comb, smooth, lift, twist, curl, direction, noise, length/cut, tube, place,
  and painting maps (any setting's: density, width, length... from + New Map > for a setting), with
  each stroke one undo step; strength root to tip and the
  falloff as curves; symmetry across world X or the scalp's own topology.
- **Look:** clumping, noise, curl and frizz, with texture, painted or per-strand maps on any
  mappable parameter.
- **Authoring:** a node graph editor, graph presets, whole looks copied and pasted as text to
  share, a GroomLab menu and a Python graph API.
  Any number, switch, vector or color param keys in place (Set Key, or S over it), as Maya
  animation.
- **Pipeline:** groom files, a headless CLI, USD and Alembic export, Arnold and RenderMan
  procedurals for farm renders, deformation motion blur, rest meshes and a simulation round-trip.
- **Grooming tools:** guide selection (lasso, soft, mirrored), brush symmetry, clump and
  attribute paint, and reparent, promote, demote and mirror.
- **Painted maps:** one map concept painted with GroomLab's own brush on the guides, the
  scalp's vertices or a texture, convertible between the three, read by a Map node; gray, or
  color (r g b a) for painted hair color and masks packed in channels.
- **Game cards:** cards from the groom with baked Unreal maps and an atlas that survives
  brushing, LOD presets, a scalp bake and an Unreal export; feathers carded one strip per
  feather with shared, mirrored feather textures.
- **Feathers:** shafts (scattered or hand-placed guides) grow barbs through a Propagate node,
  shaped by the ordinary look nodes; designs as presets, Split for assortments.
- **Scene files:** everything a scene needs lives in its own `groomlab/<scene name>` folder beside
  it, named per groom (no two grooms or scenes share a file by accident), and follows a Save As.
- **1.0 hardening:**
  - portable paths for referencing;
  - errors on the node;
  - golden tests for every operator;
  - the spec's performance budgets, measured.
- **Graph extras:** parameter expressions, compound nodes, strand color modes and ribbons in the
  viewport.
- **Next:** the pilot. After 1.0: EXR/TIFF maps, and the standalone app (`hosts/app`) and the
  Blender add-on (`hosts/blender`) on the same core.
- **Grooming:** a groom scatters strands on a scalp mesh, shapes them from guides (Maya curves
  or an Alembic file), and draws them in Viewport 2.0.
- **Output:** Arnold (MtoA) renders the groom as curves at full density. Grooms export to
  Alembic and USD as animated curves, and to groom files that `groomlab-cli` evaluates without
  Maya.

Operators so far:
- **Sources:** `Scalp` (the groom's scalp, or another of its scalps), `Guides`, `ColliderMesh` (any other mesh: a
  collider), `ImportCurves`, `Reference` (another groom file's strands or guides: a library look).
- **Grow:** `Scatter`, `Interpolate`, `Cull`.
- **Shape:** `Length`, `Width`, `Resample`, `Direction`, `Bend`, `Transform`, `Flow` (short
  hair along painted flow arrows), `Part`, `Tie` (ponytails and hair ties), `Smooth`, `BlendNeighbors`, `Gravity`,
  `Displace`, `Mirror`, `Collide`, `SelfCollide` (hair no longer through itself), `MeshCut`,
  `RestoreLength`, `Sculpt` (brush the guides the graph made: see **Sculpting generated guides**).
- **Look:** `Clump`, `Twist`, `Braid`, `Noise`, `Curl`, `Flyaway`, `Static` (charged hair puffing out), `Tangle`, `Wet` (wet hair from one slider), `Instance`, `Scales`.
- **Convert:** `ToGuides`, `ToStrands`. **Games:** `Cards`.
- **Color:** `HairColor`, `Bands`, `FeatherColor`, `RenderData`.
- **Motion:** `Wind`, `Jiggle` (no simulation cache: see **Motion without a cache**), `Simulate`
  (the guides simulated, every host: see **Simulating the guides**).
- **Feathers:** `Propagate` (barbs grown off shafts), `Plume` (feathers laid over one another and off the body), `Split` (strands routed to four outputs).
- **Utility:** `Map`, `Attribute`, `Group`, `Merge`, `Switch`, `Blend`, `Output`; `Compound` and `Input` for compound nodes.
- **Fields:** `Control` (a keyable slider on the groom), `Expression`, `FieldNoise`, `FieldRemap`, `FieldCombine`, `Curve`.

## License

GroomLab is open source under the [Apache License 2.0](LICENSE) ([NOTICE](NOTICE)), at
https://github.com/thekesslereffect/groomlab. The Blender
add-on (`hosts/blender/addon/`) is under the GPL-3.0-or-later, as Blender add-ons must be.
Third-party components and their licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
Contributing: [CONTRIBUTING.md](CONTRIBUTING.md). Security reports: [SECURITY.md](SECURITY.md).

GroomLab is free for any use, commercial included: every feature and every export (Unreal,
Unity, glTF, Alembic, USD, cards), no watermark, no key, no activation. It is funded by
donations: support it at https://ko-fi.com/getgroomlab (GroomLab > Support GroomLab in Maya,
Help > Support GroomLab in the app, the GroomLab menu in Blender).

## Building from source

The engine (core, Python module, CLI, C API, Blender helper) builds with no proprietary SDK:
`cmake --preset win-core` (or `linux-core`). The Maya plugin, the Arnold translator and
procedural, and the RenderMan procedural build against your own Maya install or devkit, MtoA
and RenderMan Pro Server (`MAYA_LOCATION`, `MTOA_LOCATION`, `GROOMLAB_RMANTREE`). Their SDKs are
not redistributable and are never in this repository. Step by step: [BUILDING.md](BUILDING.md).
The full developer handbook: [docs/DEVELOPING.md](docs/DEVELOPING.md).

## Layout

`engine/` is the system, Maya-free; `hosts/` are the products built on it; `tests/` mirrors the two.

| Path | What |
| --- | --- |
| `engine/core/` | `groomlab_core` static library: strands, scalp mesh, guides, graph evaluator, graph JSON, operators, brushes, painted maps on the scalp (`scalp_paint`), cards, groom files, USD writer. No Maya headers. |
| `engine/gpu/` | `groomlab_gpu`: the GPU look preview's engine (DirectX 11 and OpenGL compute, kernels in Slang), host-free. |
| `engine/io/` | `groomlab_alembic`: Alembic curves writer and readers, animated mesh reader, `ImportCurves` operator, `GroomScene` (a groom file with its caches). Maya-independent. |
| `engine/python/` | `groomlab_core` pybind11 module, built for the Python each Maya uses (3.7 in Maya 2022, 3.9 in 2023, 3.10 in 2024, 3.11 in 2025 and 2026, 3.13 in 2027). |
| `engine/capi/` | The C API: the shared library `groomlab` exporting the `gl_*` functions of `groomlab_c.h` (core linked in, no TBB), its C++ wrapper `groomlab.hpp`, samples. |
| `engine/cli/` | Headless CLI (OR-4): `info` and `eval` for groom files, `abc-info`, `--list-operators`. |
| `hosts/maya/` | `GroomLab` plugin: `groomLabShape` shape node with VP2 drawing, the brush tool, the commands (`groomLabGuides`, `groomLabScalpMaps`, `groomLabBrushStroke`, exports...), crash logging, Maya module file, `groomlab` Python package (API and UI). |
| `hosts/maya/arnold/` | `groomLabTranslator` MtoA extension: renders `groomLabShape` as Arnold `curves` (with motion blur) or as a `groomlab_procedural`. |
| `hosts/arnold/` | `groomlab_procedural` Arnold plugin: renders groom files at render time, with no Maya needed. |
| `hosts/renderman/` | `groomlab_renderman` RenderMan procedural (a DSO for prman, Procedural2 and the older Procedural API that RenderMan for Maya's RenderManProcedural node uses): renders groom files at render time with primvars for a PxrHairColor + PxrMarschnerHair network (`examples/groom.rib`). Built when RenderMan Pro Server is installed (`GROOMLAB_RMANTREE`); docs/DEVELOPING.md, "RenderMan procedural". |
| `hosts/app/` | The standalone app (`GroomLab.exe`, Qt 6, off by default: `GROOMLAB_BUILD_APP`). |
| `hosts/blender/` | The Blender add-on (`addon/`, GPL-3.0-or-later) and its helper process (`helper/`, Apache-2.0). |
| `tests/` | GoogleTest core, GPU and Alembic tests (`tests/engine`), Python binding tests, mayapy plugin and Arnold tests, GUI-session tests (`tests/hosts/maya/gui_*.py`), the app's tests (`tests/hosts/app`), shared data (`tests/data`). |
| `tools/` | Packaging (`package_release.py`, `package/`), benchmarks (`bench/`), icon and fixture scripts. |
| `web/` | The website (Next.js). |
| `docs/` | The user guide, the developer handbook (`DEVELOPING.md`), the spec summary and the pilot plan. |
| `third_party/stb/` | `stb_image.h` (public domain or MIT), vendored at the commit in `COMMIT`, for texture maps. |
| `cmake/` | `FindMaya`, `FindMtoA`, TBB resolution, fetched dependencies (pybind11 2.13.6, GoogleTest 1.15.2, nlohmann/json 3.11.3, Imath 3.1.12, Alembic 1.8.8). |

## Supported Maya versions

One package holds a build for each Maya version below (Windows), in one module: Maya picks its own
build from the module file's per-version sections.

| Maya | Status | Python | Qt (UI) | Arnold translator |
| --- | --- | --- | --- | --- |
| 2022 | tested | 3.7 | Qt 5, PySide2 | MtoA 5.0 and later |
| 2023 | beta: build-verified | 3.9 | Qt 5, PySide2 | MtoA 5.2.1 and later |
| 2024 | beta: build-verified | 3.10 | Qt 5, PySide2 | none yet (renders groom files through the procedural) |
| 2025 | beta: build-verified | 3.11 | Qt 6, PySide6 | MtoA 5.4.0 and later |
| 2026 | beta: build-verified | 3.11 | Qt 6, PySide6 | MtoA 5.6.1 and later |
| 2027 | beta: build-verified | 3.13 | Qt 6, PySide6 | MtoA 5.6.1 and later |

**Tested** means the whole test suite (the mayapy suites, the GUI session, Arnold) runs in that
Maya. **Beta: build-verified** means the plugin, its Python module and translator are built from
that Maya's devkit and checked without Maya: they compile and link against that version's API,
import only that Maya's own DLLs, the Python module passes its tests under that Maya's Python
version, and the UI builds and runs offscreen under that Maya's PySide
(`tests/hosts/maya/qt_compat_check.py`); nobody has run them inside that Maya yet. The Arnold
procedural is one build for every version (it needs Arnold 7 alone). How each is built:
[DEVELOPING.md](docs/DEVELOPING.md), "Other Maya versions".

## Requirements

- Maya 2022 (headers and libs ship inside the install, e.g. `C:/Program Files/Autodesk/Maya2022`);
  the other versions build from their public devkits, no Maya install ([DEVELOPING.md](docs/DEVELOPING.md),
  "Other Maya versions")
- Visual Studio 2022 (the v143 toolset is binary compatible with Maya 2022's v142) or GCC 9+ on Linux
- CMake 3.25+
- Optional: MtoA 5.0 for Maya 2022 (Arnold 7.0) for the Arnold translator. Its SDK headers ship with MtoA.
- Network access on first configure. FetchContent pulls pybind11, GoogleTest, nlohmann/json,
  Imath and Alembic; Alembic and Imath build static and add a couple of minutes to the first build.

TBB comes from the Maya's own: TBB 2020 in Maya 2022 to 2025, oneTBB (`tbb12`) in 2026 and 2027.
Without Maya, a system TBB package is used if found. Otherwise loops run serially.

## Build and test

The short form; [docs/DEVELOPING.md](docs/DEVELOPING.md) has the whole loop, the test suites
one by one, and what to do when Maya crashes or hangs.

```bash
cmake --preset win-maya2022
```

```bash
cmake --build --preset win-maya2022
```

```bash
ctest --preset win-maya2022
```

The test run covers:
- the core unit tests,
- the `groomlab_core` Python tests under `mayapy`,
- `mayapy` standalone tests that load `GroomLab.mll` and groom a sphere,
- an Arnold test that exports a 100k-strand groom through MtoA to `.ass` and checks the curves,
  deformation blur keys, and a groom-file `.ass` rendered by `kick` through the procedural,
- Alembic round trips, both through the core reader and through Maya's `AbcImport`,
- groom files evaluated by `groomlab-cli` against Maya's own export, and USD checked with MayaUSD's `pxr`,
- the Python graph API and presets,
- 1.0 hardening:
  - golden output for every operator;
  - the same output with one thread and with many;
  - v1 compatibility fixtures, and bad-input sweeps;
  - errors on the node, referenced grooms in a moved project, and the spec's performance
    budgets.

Tests that open the Maya UI (the brush tool, guide selection, reference picking, the graph
editor and viewport speed: one Maya window, `maya.gui`, about a minute) are off by default. Turn
them on with `-DGROOMLAB_GUI_TESTS=ON`. The test tiers (`ctest -L quick|gui|arnold`) and when to
run each: [DEVELOPING.md](docs/DEVELOPING.md), section 2.

**Golden files.** `tests/data/golden` holds every operator's expected output:
- **What must match:** strand counts and IDs must match exactly. Sampled points, widths and
  channels must match within 1e-4, so the files hold on every compiler.
- **After an intended output change:** run the core tests with `GROOMLAB_UPDATE_GOLDEN=1`, then
  review the diff.
- **Stored formats change in place:** there are no migrations (see Conventions). The
  fixtures under `tests/data/compat/v1` (written by `tools/make_compat_fixtures.py`) are
  loaded by the compatibility tests to catch a format change nobody meant to make; update them
  with an intended one.

**Coverage.** The `linux-coverage` preset builds the core with gcov. CI's coverage job reports
line coverage of `engine/core/` in its summary; the target is 80%.

**Performance budgets.** These are measured by the `Budget.*` tests, `maya.hardening` and
the GUI session's `viewport` suite (`maya.gui`) on a 24-core machine. CI reports them without failing.

| Budget | Target | Measured |
| --- | --- | --- |
| Full graph, 100k strands x 16 points | < 100 ms | ~80 ms |
| One parameter tweak at preview density | < 33 ms | ~5 ms |
| Brush dab on 2k guides + interpolation preview | < 16 ms | ~14 ms (~20 ms with a full look graph) |
| Viewport orbit, 200k preview strands | >= 24 fps | ~120 fps |
| Render expansion, 1M strands x 24 points | < 5 s, < 1 GB | ~1 s, ~800 MB |
| Groom file, graph + 5k guides | < 10 MB | ~6 MB |
| Scene open with 3 grooms | < 3 s added | ~20 ms |

The Arnold translator builds when `MTOA_LOCATION` points at an MtoA install; the
Windows preset sets it. Turn it off with `-DGROOMLAB_BUILD_ARNOLD=OFF`.

For work without Maya, use the `win-core` or `linux-core` presets. On Linux with Maya, use
`linux-maya2022` and set `MAYA_LOCATION` if Maya is not in `/usr/autodesk/maya2022`. The Linux
build for farms and pipelines (CLI, C API, Python module, procedurals) is one command from Windows,
in WSL: `powershell -File tools/dev/linux_build.ps1` (docs/DEVELOPING.md, "Linux builds (WSL)").

## Use in Maya

Install the module into a folder on `MAYA_MODULE_PATH`:

```bash
cmake --install build/win-maya2022 --config RelWithDebInfo --component GroomLab --prefix "C:/path/to/maya/modules"
```

That writes `GroomLab.mod` and, under `GroomLab/`: `plug-ins/GroomLab.mll`, `scripts/groomlab`,
`python/groomlab_core.cp37-win_amd64.pyd`, `presets`, `expressions`, `themes`, `icons`,
`extensions/groomLabTranslator.dll`, `procedurals/groomlab_procedural.dll` and
`bin/groomlab-cli.exe`. Another Maya version's build (`build/ninja-maya2025`...) installs its
binaries under `GroomLab/maya<year>/` (`plug-ins`, `python`, `extensions`) and its own section of
`GroomLab.mod`; the packaged release holds them all. Always
pass `--component GroomLab`; without it the install also copies Alembic and Imath headers and
libraries. Put the long form of the path on `MAYA_MODULE_PATH`.
Maya's plugin lookup fails on 8.3 short paths like `C:/Users/ABCDEF~1/...`. Then, in Maya:

```python
from maya import cmds
import groomlab

scalp = cmds.polySphere()[0]
guide = cmds.curve(degree=2, point=[(0, 1, 0), (0.6, 1.5, 0.2), (1.4, 1.2, 0.3)])
groom = groomlab.create_groom(scalp, guides=[guide])
cmds.setAttr(groom + ".previewDensity", 1.0)
```

- **The preview is the render, thinned:** `previewDensity` changes only how many strands show.
  A preview at density p shows the strands whose hashed ID falls under p, and each one is the
  render's strand of that ID, bit for bit (points, widths, every channel): clumps, neighbours,
  partners and the rest come out the same at every density. Nodes that read other strands (a
  Clump picking its centers, its axes, Twist's and Curl's clump middles, BlendNeighbors, Tangle's
  partners, a Braid's own paths, Merge's seam, Bands' and Resample's lengths, an expression's
  `$longest`, ToGuides, Plume's feathers lying over others) read only the **core**: the strands
  every preview holds. A Scatter's core is the strands whose hashed ID falls under its core share:
  3% of a big scatter, a power of two more for a smaller one so that it holds at least 5,000
  strands (all of a scatter under 5,000; all of one with `previewAll`, and of guides as strands).
  Below the core share a preview evaluates the whole core and shows only its own share: the core
  strands it does not show are dropped before anything draws, exports or counts them (the GPU
  preview runs them too and writes the shown ones alone). The one difference left is an
  expression's `$index` (a strand's place in the set, which a thinned set changes).
- **Viewport memory:** every node keeps a copy of the strands while previewing, and the draw
  buffers more, so the preview is held to `previewPointBudget` million strand points (16 by
  default; 0: no limit). Past it Scatter thins the preview on its own (the same strands, fewer
  of them), so raising density or segments can't exhaust memory. Renders and exports ignore it.
- **Even roots:** `Scatter.randomness` (1 by default: random) below 1 spreads the roots evenly
  first (blue noise: roots relaxed apart on the scalp's rest pose until the spacing settles; open
  edges push back, so no row piles up along a hairline), then puts each root that fraction of the
  way back toward its random spot: 0 is fully even, and lowering it evens the groom out smoothly.
  Strands keep their IDs. A preview relaxes every root as the render does (once: the relaxed
  roots are kept, for every preview density and the render alike), so it shows the render's
  positions. Both are local: a root moves by its neighbors alone and stops once it and they have
  settled, so an edit of the scalp moves roots only near it. Viewing a Scatter node draws its roots as dots on the scalp.
- **Root layout:** `Scatter.layout` `random` (the default: `randomness` as above; 0 evenly
  spread) or `uvGrid`: one root per cell of a `gridRows` x `gridColumns`
  grid over the scalp's UVs (0-1), at the cell's center or moved in it by up to `gridRandom` of a
  cell, on the first face (in face order) whose UVs hold it: rows of fur, scales' or feathers'
  tracts. Each root's ID comes from its cell, so the grid keeps its strands while the scalp
  deforms; a density map thins it (each root kept by map / its maximum), and the preview density
  picks among the same IDs. It needs the scalp's UVs (an error without them); overlapping UV
  shells give a cell to the first face only.
- **Fur along the guides only:** `Scatter.guideRadius` (0: off) keeps only the roots within that
  distance of a guide's root, on the rest scalp (the `guides` input: a Guides node, one group
  say), so drawn guides place the fur, a patch, a stripe or a mane, and nothing grows elsewhere.
- **Every root in the preview:** `Scatter.previewAll` (off by default) keeps all of the Scatter's
  roots in the viewport whatever the groom's `previewDensity`: for a few big pieces each strand
  carries (feather shafts, scales, leaves) that a 10% preview would leave bald. The shipped
  feather, scale and instance presets set it.
- **One seed per Scatter:** a strand's ID and root come from the Scatter's `seed`, the scalp
  face and its number on the face, so two Scatters with the same seed grow the same strands.
  The Graph Editor (and `GraphModel.add_node`) gives a new Scatter, Instance, Flyaway or any
  other seeded node a seed no node of its type has, and warns (without stopping the save) when
  two Scatters, Instances or Flyaways share one (`GraphModel.warnings()`). A `Merge` still gives
  the second look's colliding IDs new ones, but their roots coincide.
- **Interpolate blends without seams:** every guide reaches `reach` (1.5) times the distance to
  its `neighbors`-th (3) nearest guide, and a strand inside that reach weighs the guide by
  `((R - d) / (R d))^power`, which reaches zero exactly at the edge of the reach. Guides in
  dense areas reach less far than sparse ones, and a guide's reach does not depend on the
  strand, so the blend is smooth across the scalp: no pops as guides move, no seams where a
  strand's nearest guides change (plain nearest-k inverse distance has both). `neighbors` 1
  gives every strand its nearest guide alone, with hard edges. Guides are sampled at least as
  finely as the strands, so curly guides keep their detail. With `smoothGuides` (on) each guide
  is followed as the smooth curve the viewport draws through its points (a centripetal
  Catmull-Rom spline), so a bend drawn with few points stays round in the hair instead of
  turning at each point. `preserveLength` (on) scales
  each blended strand to its guides' blended length, up to 2x, since blending guides that point
  different ways shortens the result.
- **Interpolate follows the skin:** with a scalp connected, each guide's shape is carried from
  its root to the strand's along the scalp: turned from one surface normal to the other where the
  straight line between the roots hugs the skin (almost everywhere: a guide combed back stays
  combed back over a curved head), and through each bend of the path where it goes round a fold,
  a thin flap or the rim of a shell (the two sides' normals face apart, and the turn comes the way
  the skin went). `followSurface` (Follow surface, on) also measures each guide's reach along the
  scalp, so guides do not reach across an ear, a fold or a thin shell to strands that are close in
  space but far along the skin; on a smooth scalp it changes nothing (on the bench heads, points
  move by less than 1e-5 cm). The scalp is welded first, so vertices split along UV seams make no
  seam in the hair. The paths are made once per guide root and reach (about 1-2 ms for 500 guides,
  once a groom: a dab that does not move roots makes none, one that moves a few makes theirs).
- **Interpolate keeps clearance:** a guide's shape is carried rigidly, so where the skin under a
  strand bulges more than under its guide (ears, cheeks, the neck, the back of the skull) the
  strand would go into the scalp. With `keepClearance` (Keep clearance, on; shown with a scalp
  connected, take shape) each guide's height above the scalp is measured at each of its samples
  (along the closest point's normal, through the scalp's signed distance field, the one Collide
  asks; 0 where the guide is inside, at most 1/32 of the scalp's size, about 1 cm on a head), and
  each strand's, blended with the shape's weights and at least 1/2048 of the scalp's size, is the
  least it keeps: from the root out, a point lower than that is lifted along the field's normal
  and the points after it follow, each segment's length kept. Points already that high are not
  moved (a strand clear of the skin is the same to the bit); points beside the scalp (past an
  open edge) or deeper than 1/8 of its size (the far side of an open cap) are left alone. One
  guide on a head gives strands that never go into the scalp, without a Collide (on a head with
  an ear, 4265 points inside it carried as they are, none keeping clearance). It costs about
  30-40 ms at 500k strands x 16 points (Interpolate alone about 35-45 ms) and 2-3 ms a brush dab at
  preview density. The walk takes the field's answers alone (where the field does not blend, the
  closest point among the triangles around the closest points it holds there: the mesh's own for
  99.7% of such points on the app's head), so the GPU preview walks it too, from the field as
  tables (made and uploaded once per scalp: about 56 MB and half a second for a head-sized test
  scalp) and the guides' clearances: strokes and Interpolate's drags run on the GPU from
  Interpolate (the walk about 8-13 ms at 500k strands), its strands the CPU's within the GPU's
  rounding. The field's grid is the world's (half a cell off its whole multiples) and the
  distances go by the scalp's size rounded to an eighth of an octave, so editing one side of a
  scalp leaves the strands on the other side as they were, to the bit. Over a scalp deformed away
  from its rest pose the walk is the CPU's, the nodes after it the GPU's.
- **Interpolate keeps to painted regions:** `regionMap` (Regions; empty, off, by default) names
  regions painted on the scalp in colors (XGen's region maps): a color map from Groom Tools >
  Paint, else a color set of that name on the scalp, else an image file at the scalp's UVs. A
  strand blends only the guides rooted in its own region, weighed as if they were the only guides
  (each guide's reach among its region's), so a region's blend is as smooth as a whole scalp's and
  the edge between two regions is hard, like a part, along an area instead of a line. The regions
  are the distinct colors at the guides' roots (colors within 0.1 of each other in RGB are one;
  a guide in soft paint between two regions, its color a blend of theirs, joins the nearer), so
  no exact colors need painting and every region has guides. A strand joins the region whose
  color is nearest its root's when that is within 0.1, or when its color lies within 0.1 of the
  line between two regions' colors (soft paint between them: the nearer); a strand in a color no
  guide is rooted on (a region painted without guides), or not rooted on the scalp, blends the
  nearest guides of any region, as without a region map. Paint regions in distinct colors (red,
  green, blue, yellow...): a color that is a mix of two others reads as the soft edge between
  them. Each strand's region number goes to `regionChannel` (`region`; -1 for none), which Clump,
  BlendNeighbors and Tangle read through their own `regionChannel` (`region` by default) to stay
  within regions too (the viewport's `strandColorMode` `clump` with `strandColorSource` `region`
  shows a color per region). Parts, `followSurface`, `limitDistance` and `keepClearance` work within each
  region as on the whole scalp. A brush dab reshaping guides binds nothing again; a guide moved
  within its region binds again only the strands near it; moved into another region, every strand
  is bound again, and a strand changing region (a guide rooted in a color no guide was) makes
  Interpolate evaluate in full. The GPU preview blends from the same binding.
- **Segments live on the Resample node:** Scatter only places roots. The **Resample** node's
  `segments` (16 by default; 0 keeps each strand's) is the strands' resolution: strands (or
  guides) through it are resampled evenly to it, and every Scatter grows its strands with it in
  the first place (the most any Resample asks for; 8 when there is none), whichever node the
  viewport shows, so nodes before it, like Interpolate, already work at that resolution. Put it
  after Interpolate (the default graph has it before the Output). Its `mode` is `even` by
  default: every strand gets `segments` (what the GPU look preview hands on); `curvature` spends
  fewer points for render and export: `segments` is the most a strand gets, straighter strands
  get fewer (down to `minSegments`), placed where they bend; `length` gives shorter strands fewer
  (Segments by length or curvature below).

The groom's graph lives in its `graph` attribute as JSON. `{}` means the default
graph (hair out of the box, `hair_basics` without its clumps: Scalp → Scatter → Interpolate ←
Guides → Length → Noise → HairColor → Width → Resample → Output). Use
`groomlab.get_graph` and `groomlab.set_graph` to edit it; the schema is described in
[graph_io.h](engine/core/include/groomlab/graph_io.h).

### Guide hierarchy

The Tonic-style part: primary guides, then children generated inside each parent's tube,
then grandchildren inside the children's tubes, to any depth. Children store their shape
relative to their parent, so editing a parent carries every descendant.

```python
curves = cmds.ls("guideCurve*", type="transform")
groomlab.guides_from_curves(groom, curves, group="primary", tube_radius=0.15)   # copies the curves
groomlab.generate_children(groom, "primary", name="children", count=8, twist=0.5)
groomlab.generate_children(groom, "children", name="grandchildren", count=4)
groomlab.list_guide_groups(groom)   # [{'name': 'primary', 'depth': 0, 'count': 6, ...}, ...]
cmds.setAttr(groom + ".displayTubes", True)
```

**Groups.** Guides live in named groups: `primary` (placed, drawn or from curves), child groups
from `generate_children`, `braids` for braid paths. The **Guides** tab of the Groom Tools shows
them as a tree with guide counts, levels and the graph nodes naming them; it selects, renames, deletes
and adds a Guides node for a group, and **Color guides by group** (`guideColorMode` "group")
gives each its own hue. A `Guides` node reads every group at a `level` (-1: the deepest), or the
groups named in `group` (one or several, their guides together), and leaves the groups in `exclude`
out; in the Graph Editor both have a **+** menu of the groom's groups. Other text params offer what
exists too: a dropdown for fixed values (Braid `style`, HairColor `mode`, Instance
`orient`, ...) and suggestions for names (color sets, parts, meshes, controls). A param naming an
attribute (a strand channel) says **Writes** or **Reads**: a writing one (Clump's `clumpChannel`,
Attribute's `name`) is a plain field with a warning when the name overwrites an upstream node's; a
reading one (Curl's `clumpChannel`, Clump's `regionChannel`) lists only the attributes reaching the
node, each with the node that writes it, and notes a name that does not reach
(`groomlab_core.graph_channels(graph_json)`, the same in the app and Blender).

```python
groomlab.group_users(groom, "braids")               # [('guides', 'exclude'), ('braidPaths', 'group')]
groomlab.rename_guide_group(groom, "braids", "rows")  # graph params follow; groomLabGuides -renameGroup
groomlab.color_guides_by_group(groom)
```

**Choosing who has children, and placing them by hand.** Parents don't have to be whole groups:

```python
groomlab.generate_children(groom, count=6, selected=True)          # only the selected guides get children
groomlab.generate_children(groom, count=4, guides=[child_id])      # a child picked to have children
groomlab.brush_stroke(groom, "place", [point], level=1)            # a child placed by hand
```

- **Growing a child group:** the child group (`<parent group>_children` unless named) grows
  with each call. A parent that already has children there gets a fresh set, so you decide
  parent by parent who has children, at any level.
- **Placing by hand:** Place and Draw at a child level add a child to the nearest guide one
  level up. The first ones copy their parents' shape, and later ones copy nearby children. In
  the Groom Tools, pick a child group in **Group** (New Group... under Guide groups & edits
  starts a deeper level) and use Place or Draw.
- **Individual edits:** brush at a child level to shape children one by one. They keep that
  shape relative to their parent, so moving the parent still carries them.

- **Storage:** the hierarchy is stored as JSON in the groom's hidden `guideData` attribute and
  saves with `.ma` and `.mb` scenes. Edit it only through `groomLabGuides` (or the Python
  wrappers: `guides_from_curves`, `generate_children`, `lock_guides`, `unlock_guides`,
  `remove_guide_group`, `clear_guides`). Every edit is undoable.
- **Scalp binding:** primary and locked guides are bound to the scalp by face and barycentric
  coordinates, with their shape stored in the root's surface frame, so they move and turn with a
  moving or deforming scalp.
- **Tip-first curves:** curves drawn tip-first are flipped when rooted.
- **Which level is used:** the default graph's `Guides` node uses the deepest level (`level: -1`).
  Set `level` or `group` to use another one, and connected `guideCurves` are merged in as extra
  flat guides.
- **Viewport:** primaries draw in `guideColor` (a vibrant blue by default), each level down
  lighter and further round the hue wheel, `guideWidth` pixels wide (4 by default; selected
  guides one more), drawn as a smooth curve through the guide's points (`guideSmoothing`
  pieces per segment, 4 by default; 1 draws straight segments).
  Guides toggle with `displayGuides`; tubes with `displayTubes`: a solid, shaded tube per guide
  (Lambert-like, in the theme's tube color) following its radius profile.
- **Over the hair:** guides, their dots, parts and flow arrows draw over the strands, tubes and
  cards (`displayOverlaysOnTop`, on by default): the viewport pulls them toward the camera by
  the length of the longest strand before the depth test, so anything within the hair shows
  through it. The pull stops short of every visible mesh in the scene (the scalp, a body, a
  collar; a ray per point, redone as the camera moves), so those still hide what is behind
  them: no X-ray through the head. Off, they are depth-tested like everything else.

**Hierarchy tools.** Every tool keeps each guide's current shape; what changes is what it
follows. Each is one undo step:

```python
groomlab.mirror_group(groom, "primary", "x")               # copy +X guides to -X, replacing what is there
groomlab.symmetry_sync(groom, "x")                         # the whole groom +X to -X, children and edits too
groomlab.reparent_guides(groom, parent_id, guides=[...])   # follow another parent (default: the selection)
groomlab.promote_group(groom, "children")                  # one level up; at the top they become primaries
groomlab.demote_group(groom, "extra", under="primary")     # one level down; each follows the nearest guide
groomlab.set_guide_attribute(groom, "clump", 0.5, group="primary")
```

- **Mirroring:** `mirror_group` mirrors one root group; generate children after it.
  `symmetry_sync` (**Sync +X → -X** / **Sync -X → +X** under Guide groups & edits in the Groom Tools)
  mirrors every root group and copies each guide's children, grandchildren and so on with it,
  hand edits, locks and painted attributes included. Guides on the plane are left alone.
- **Per-guide attributes:** children inherit them from their ancestors unless they set their
  own. `Interpolate` blends them into strand channels of the same name, which attribute maps
  read, for example `attribute_map("clump")` on a Clump's strength.

### Parting

**Drawing parts:** pick **Part** in the Groom Tools (Tools tab) and draw freehand on the scalp.
- **While drawing:** the stroke follows the surface under the cursor and shows in red.
- **On release:** the stroke becomes a part, in one undo step. With symmetry on, its mirror
  image is added too.
- **Part node:** the groom gets a Part node if it has none, so the part opens.
- **Parts box:** with Part picked, the Groom Tools' Parts box names new parts and lists,
  removes (**Remove**) or clears (**Clear All**) them.
- **From curves:** parts can still be made from existing curves (the Parts box's **From
  Curves**, or `add_part`).
- **Reshaping:** with **Parts** ticked under Acts on in the brush settings, Comb slides a part's points
  along the scalp, Smooth and Straighten relax or straighten the line, Attract pulls it toward
  the cursor and Cut splits it in two (see Brushes). The parts preview live as you drag; each
  stroke is one undo step.

Parts are lines on the scalp that hair does not mix across, like the parting in an animal's
fur. Draw them with Maya's curve tool on the live scalp:

```python
groomlab.draw_part_tool(groom)                           # scalp made live, EP curve tool on
names = groomlab.add_part(groom, curves, name="crown")    # stored on the groom, bound to the scalp
groomlab.add_part_node(groom, width=0.25)                 # a Part node: opens the gap
groomlab.list_parts(groom); groomlab.remove_part(groom, "crown"); groomlab.clear_parts(groom)
```

- **No mixing:** `Interpolate` and `Clump` never take a guide or a clump center from the other
  side of a part. Their `respectParts` parameter is on by default. To keep whole areas apart
  rather than the two sides of a line, paint regions and name them in Interpolate's `regionMap`
  (see Interpolate keeps to painted regions above).
- **Opening the gap:** the `Part` node makes strands within `width` of a part lean away from it,
  on their own side, keeping their lengths. Its `strength` parameter is mappable, `profile` sets
  the lean root to tip, and `part` limits it to one part.
- **Auto parts:** set the Part node's `partsFrom` to `auto` and it finds the parts
  itself, where the hair it takes flows away to both sides of a line (a parting combed into the
  guides), and opens them as it opens drawn ones (`width`, `strength`, `profile`). `splitAngle`
  (degrees, 90) is how sharply the hair must split, between its flow on the two sides; lower finds
  softer partings. `minLength` (a share of the scalp's size, 0.15) drops shorter splits, such as a
  whorl's center. The flow is the strands' own: each one's first segment along the scalp,
  smoothed over 5% of the scalp's size, so the parts move smoothly as you groom. Found parts are
  the node's alone: Interpolate's and Clump's `respectParts` keep to drawn parts (they run before
  Part). The GPU preview opens the parts an auto Part found at its last evaluation: while you
  drag or brush upstream of it they stay where they were, and the release finds them again
  (dragging `splitAngle` or `minLength` previews on the CPU). Scripted,
  `graph.part_lines(node_id, ctx)` gives the lines a Part node opens and
  `groomlab_core.auto_parts(strands, scalp, split_angle, min_length)` finds them.
- **Storage:** parts are stored bound to the scalp (face and barycentric coordinates). They
  follow a deforming scalp, draw red with the guides, and travel in groom files.
- **Sides:** sides are measured on the rest pose. Hair past either end of a part has no side, so
  it flows around the end.
- **Command:** `groomLabParts -fromCurves/-remove/-clear/-list` edits parts, and every edit is
  undoable.

### Guide selection

The selection is stored on the groom (`guideSelection`) and drawn on top of the other guides:
white for selected, orange for soft-selected. Every change is one undo step:

```python
groomlab.select_tool(level=0)                        # lasso or click in the viewport
groomlab.select_guides(groom, level=0)               # or by guides=[...], group="..."; mode="add"/"deselect"/"toggle"
groomlab.mirror_guide_selection(groom, "x")          # the mirror images of the selection
groomlab.set_soft_selection(groom, 0.5)              # weights fall off within 0.5 of the selection
groomlab.selected_guides(groom)
```

- **In the tool:** Shift toggles, Ctrl deselects and Ctrl+Shift adds.
- **Selection as a mask:** brushes take it with `selected_only=True`, with soft-selected guides
  weighted.
- **Hierarchy tools:** they act on the selection when no guides are given.
- **In the Groom Tools:** **Select** (Tools tab) lassoes, its Selection box sets the **Soft
  radius**, and Guide groups & edits has **Mirror Selection**, **Clear Selection** and the hierarchy
  tools.

### Brushes

Select the groom and pick a brush. Radius is in pixels, strokes are screen-space (hair at any
depth under the circle is affected), and each drag is one undo step:

```python
groomlab.brush_tool("comb", radius=80, strength=1.0, level=0)   # level 0 = primaries
groomlab.brush_tool("length", radius=60, strength=-1.0)         # negative = cut
groomlab.brush_tool("place", length=0.6)                        # click the scalp to add a guide
```

| Mode | Effect |
| --- | --- |
| `comb` | Drags guide points along the stroke. Roots stay pinned and segment lengths are kept, unless **Stretch** is on (`stretch=True`): then combing out past the tips makes guides longer (and pushing in, shorter), points respaced evenly. |
| `smooth` | Relaxes kinks, keeping lengths. |
| `straighten` | Relaxes guides toward the straight line along their root direction. |
| `lift` | Turns guides toward the scalp normal; negative strength lays them down. |
| `twist` | Swings guides around their root normal; the sign sets the direction. |
| `curl` | Bends guides progressively toward the tip, keeping length; the sign sets the direction. |
| `attract` | Pulls guide tips toward the brush center, for hand-made clumps; negative pushes them apart. |
| `direction` | Turns guides about their root normal toward the way you stroke, keeping their lean and length (Yeti's Direction brush); Ctrl turns them away. Half the turn per stamp at strength 1. |
| `noise` | Adds smooth random jitter along guides, across them, calm at the root, lengths kept. Each guide has its own pattern (seeded by its ID) and every stroke builds the same one up; negative strength (or Ctrl) smooths it out. |
| `length` | Grows guides (positive strength) or shortens them evenly (negative), 5% per stamp at strength 1. |
| `cut` | Trims guides where the brush touches them: the part past the brush is removed and the rest respaced. |
| `tube` | Grows or shrinks a guide's tube ring by ring where the brush touches it (the profile gets at least 8 rings), so a tube can bulge or taper along its length. The tube is where the guide's children are spread (Children..., and children placed by hand follow it). |
| `erase` | Removes what the brush touches, all in one undo step: guides (with their children), parts and flow arrows and sinks, each switchable (`erase_guides`, `erase_parts`, `erase_flow`; the Erases box in the Groom Tools). The Part tool with Ctrl erases parts too. Scripted, `-targets` picks what an erase stroke removes. |
| `paint` | Paints a per-guide `attribute` toward `value`, for any downstream attribute map (Ctrl: a gray map toward 0, or 1 when `value` is 0). A setting's map (density, width...) is painted with it: `groomlab.paint_param(groom, "scatter", "density")` (Groom Tools: Paint tab > + New Map > for a setting) makes, links and picks it, and Ctrl on the density map removes hair. The `width` and `density` brushes are gone: naming them raises an error that says so. |
| `clump` | Paint on the `clump` map (kept for scripts). Painted clumping is just a painted map: make one (say `clump`) and let the Clump's strength read it, or `groomlab.clump_paint(groom)` does both and starts the Paint brush. |
| `place` | Click to add a guide; drag to place many, no closer than `spacing` to any root of the group. New guides interpolate the 3 nearest guides of the group (`match_neighbors`, on by default): shape turned to the new root, length, point count, tube and painted maps (each map blended over the neighbors that have it). A group with no guides yet learns from the other groups at its level, so a new group starts from the hair around it. Without neighbors, or with `match_neighbors` off, guides stand `length` tall with `segments` segments. Draw keeps the drawn shape and takes the painted maps. |
| `slide` | Moves roots along the scalp with the drag; each guide keeps its shape, turned with the surface. Fixes a root in the wrong spot without redrawing. |
| `draw` | Press on the scalp and draw the guide's shape; it is resampled to `segments` segments. |

- **Brushes on parts and flow arrows:** the shaping brushes also work on part lines and on flow
  arrows and sinks. The **Brushes** box in the Groom Tools (Guides / Parts / Flow) picks what a
  brush acts on; scripted, `groomLabBrushStroke -targets guides,parts,flow` (default `guides`).
  Parts and arrows stay bound to the scalp: a moved point is re-bound to the closest scalp
  point, an arrow's direction carried onto the new tangent plane. What each brush does there:
  `comb` slides pins, sinks and part points with the stroke; a lone arrow is laid along the
  stroke, while the arrows of a drawn flow stroke point along the stroke as it now runs, so
  combing a flow curve reshapes it and its arrows follow the curve, not the brush; `smooth` averages an arrow with its nearest arrows and evens out the arrows of
  a stroke along it, and relaxes part lines; `twist` turns arrows around the normal and turns a
  sink's angle by the same; `length` grows or shrinks a sink's radius; `attract` pulls arrows,
  sinks and part points toward the brush center (negative pushes apart); `straighten` pulls a
  part toward the chord between its ends and turns arrows along their stroke's overall
  direction; `cut` splits a part where the brush touches it (the pieces keep the name, then
  `name1`, `name2`...); `erase` removes them. Lift, Curl, Tube, Paint and Slide leave them
  alone. Symmetry mirrors the stamps as for guides, and a stroke is one undo step with its
  guide edit. In C++: `applyBrushStrokeToFlow` / `applyBrushStrokeToParts` (`brush.h`); in
  Python, `apply_brush_stroke_to_flow` / `apply_brush_stroke_to_parts`.
- **Root to tip:** the shaping brushes (comb, smooth, straighten, lift, twist, curl, attract,
  direction, noise) take a curve of their strength along each guide, root to tip
  (`strength_profile=[[0, 0], [1, 1]]`: tips only; the **Root to tip** box in the brush
  settings). Flat (or none) is the brush as always, bit for bit.
- **Falloff curve:** the falloff drawn as a curve, the weight from the brush's center to its edge
  (`falloff_profile=[[0, 1], [1, 0]]`, the **Falloff curve** box), in place of the Falloff
  slider's smooth falloff; every brush, map paint included.
- **Symmetry:** `symmetry="x"` (or y, z) mirrors every stroke across a world plane at
  `symmetry_center`; `symmetry="topology"` mirrors it across the scalp's own surface instead:
  each vertex's partner is found once on the scalp's rest pose mirrored in x (its twin, or the
  nearest point of the surface on a mesh not quite symmetric), so a head turned, tilted or
  animated mirrors as it looks. Brushes, Place, Draw, Part, Flow and map paint all follow it.
- **Keeping off the scalp:** every brush keeps guides out of the scalp (`collide=True`): points
  pushed under the surface slide along it instead, with their lengths kept.
- **Pen pressure:** with a tablet, pen pressure scales strength (`pressure=True`). Maya's API has
  no pressure, so a Qt event filter reads it; with a mouse it is 1.
- **Resampling:** `groomlab.resample_guides(groom, segments, guides=None, group=None, level=None)`
  respaces guides (default: the selected ones) to a new point count, keeping their shape.
**Working comfortably:**
- **The groom choice:** the top of the Groom Tools picks the groom the tools work on. Pick one
  and every tool works on it whatever you select (`groomlab.pin_groom(groom)`); **Selected**
  goes back to the selected groom. The eye beside it shows or hides that groom.
- **Ctrl while stroking:** reverses the brush. Signed brushes flip, paint goes toward
  1 − value (unfreeze or erase a map), and Cut grows.
- **Hover:** the guides the brush would touch light up before you press.
- **While sculpting:** strands drop to `sculptDensity` × the preview density and are dimmed
  (or hidden: `sculptDisplay`), so the guides read.
- **Isolate** (beside the Group choice): the brushed level stays at full color and the rest dim
  (`groomlab.isolate_level`).
- **Guides by map:** guides can be colored by a painted map, black 0 to white 1
  (`color_guides_by_map`, or **Show on guides** in Groom Tools > Paint).
- **Freeze mask:** **Freeze** (Groom Tools > Tools) paints the `freeze` map, and brushes leave
  frozen guides (and their children) alone; they draw blue. Ctrl unfreezes, **Unfreeze All**
  (the tile beside Freeze, the GroomLab Unfreeze All runtime command) frees every guide, and
  `freeze_guides` and `unfreeze_all` do it by script.
- **Solo:** Place and Draw's **Solo new guides** (`solo_new_guides=True`; off by default) keep
  the guides a stroke makes unfrozen and freeze every other, in the same undo step, so the next
  brush works on the new guide at once. **Shift+click** a guide with any brush solos
  it the same way and switches to its group; Shift+click off a guide unfreezes them all. The **Select**
  tool solos its selection (the guides not selected freeze, a pick of nothing unfreezes all;
  `select_tool(solo=False)` turns it off), one undo step with the selection. With symmetry on
  (world X or topology) the mirror guides stay live too: a guide placed with its mirror image, a
  Shift+clicked guide's mirror guide, the mirror guides of a selection (`select_tool(symmetry=...)`,
  picked as well). In C++: `soloGuides`, `unfreezeAll`, `freezeToSelection` and
  `withMirrorGuides` (`brush.h`).
- **Hotkeys:** GroomLab > Install GroomLab Hotkeys makes a GroomLab hotkey set.
  - **Y + left click:** the marking menu, with the main brushes around the ring and the rest
    below.
  - **Page Up / Page Down:** step the brushed level.
  - **B / M (hold) + drag:** the brush radius / strength, as in Maya's sculpt tools. Outside
    GroomLab brushes B and M do what they do in Maya. Middle-drag (Shift: strength) works
    without the hotkey set.
- **Shift in a brush:** Shift+click a guide to solo it (it unfreezes, every other guide
  freezes) and switch to its group (the Groom Tools follow); Shift+click off a guide to unfreeze
  them all. Shift+drag smooths, whatever the brush.
  - **Your own keys:** every tool also has a runtime command (category GroomLab) you can bind.

- **Which guides:** `level` (or `group`) picks the guides a brush edits. Children still follow
  edited parents, and `propagate=True` also brushes locked descendants, which otherwise ignore
  their parents.
- **Selected guides only:** with `selected_only=True`, strokes only touch the selected guides,
  scaled by the soft selection.
- **Symmetry:** `symmetry="x"` (or `"y"` or `"z"`, with `symmetry_center`) repeats every stamp
  mirrored across the plane, so a groom mirrored with `mirror_group` stays symmetric. A guide on
  the seam changes once. Place puts a guide on each side. The Groom Tools' brush settings have
  the Symmetry choice (off, world X, topology) and every other option; Maya's Tool Settings
  window only points there.
- **Scripted strokes:** strokes can be scripted in world space, which is how the tests drive
  them. Each point stamps a brush sphere of `radius` scene units:

```python
groomlab.brush_stroke(groom, "comb", [(-1, 1.6, 0), (1.2, 1.6, 0)], radius=1.2)
```

Interactive strokes are journaled as `groomLabBrushStroke -mode comb -interactive true` for undo.
They cannot be replayed, because their weights came from the view.

### Grooming from reference

Photos of the real hair, as cameras you groom to: **Groom Tools > Reference**.

```python
import groomlab.reference as ref
cam = ref.import_reference("C:/photos/front.jpg", groom=groom)   # a camera + image plane
ref.look_through(cam)                    # the active viewport shows the photo's view
ref.align_tool(cam)                      # pick point pairs (below)
ref.solve(cam)                           # {"rms": 0.8, "focal": 52.3, "translate": ..., ...}
ref.lock_reference(cam)                  # tumbling through it does nothing now
ref.set_reference_opacity(cam, 0.4); ref.set_reference_depth(cam, "behind")
ref.look_back()                          # back to the camera the viewport showed before (persp)
```

- **Import Photo...** makes a camera under the `groomlab_references` group with the photo on an
  image plane: its film back takes the photo's aspect (36 mm wide) and the image plane is sized to
  the film gate, so the photo fills the camera's view exactly (the whole gate shows in the
  viewport, with a little margin). It starts in front of the groom's scalp (or the scene's
  meshes) at 35 mm, aimed at it. The photo is **copied** into the scene's
  `groomlab/<scene>/references` folder and stored relative to the scene, so it travels with the
  scene and Save Scene As takes it along (`copy=False` keeps the original file). The image plane
  shows only while looking through its camera.
- **Look Through** shows the photo's camera in the active viewport; again (**Back to
  Perspective**) returns to the camera it showed before. The list follows the viewport: looking
  through a reference any other way picks it in the tab.
- **Opacity** (the image plane's alpha gain) and **In front / Behind**: in front, the photo draws
  over the mesh and you see the mesh through it; behind, the mesh draws over the photo. The image
  plane's depth is set between the camera and the subject or past it, and follows the camera.
  The eye hides the photo.
- **Lock** fixes the camera where it is: Maya's Lock Camera (`camera -lockTransform`: tumbling,
  tracking and dollying through it do nothing), plus its scale, focal length, film back and the
  photo's placement on the image plane, all locked attributes. Opacity and side still change. A
  locked camera is not re-aligned (unlock it first).

**Align by points.** Line the camera up with the mesh from point pairs, a feature on the photo
(the corner of an eye, the tip of the nose, an ear lobe) and the same feature on the mesh:
- **Pick Points** turns on the pick tool (`groomLabReferenceCtx`). In a view that looks through
  the reference, a click marks a point on the photo; in any other view (the perspective view,
  where the mesh is easy to see whatever the camera's state), a click marks the point on the
  nearest visible mesh under the cursor. Picks fill the pairs in order: the next photo point goes
  into the first pair missing one, and the same for mesh points, so you can click all the photo
  points first or alternate. While the tool is on, each pair's points draw numbered in the
  viewports (photo points on the photo's plane in amber, mesh points in cyan); nothing is added
  to the scene for them. Right-click a pair to pick one of its points again, or delete it.
- **Solve** needs 6 complete pairs, spread in depth (points all on one plane can't fix the focal
  length: the solve says so), or 4 with **Keep focal length**. It sets the camera's position,
  rotation and focal length (the film back stays) as one undo step, and reports the average
  error in pixels; each pair shows its own error, so a mismatched pair stands out.
- The solve is a pinhole camera (principal point at the photo's center, square pixels, no lens
  distortion) from a Direct Linear Transform estimate refined by Levenberg-Marquardt, with a
  spread of starting views so a camera far from right still converges (`engine/core/include/groomlab/camera_solve.h`).
  `groomLabCameraSolve` runs it on any points: `-imagePoint u v` and `-worldPoint x y z` (one of
  each per pair; pixels from the image's top-left), `-imageSize w h`, `-filmBack`, `-focal`,
  `-fixFocal`, `-camera` (start from that camera); it returns the camera's translate, rotate
  (xyz order, degrees), focal length, RMS error and each pair's error, changing nothing.

A reference keeps its settings on its camera (saved with the scene): `glRefImage`, `glRefWidth`,
`glRefHeight`, `glRefPairs` (JSON), `glRefDepth`, `glRefRms`. `ref.list_references()`,
`ref.reference_info(cam)`, `ref.pairs(cam)`, `ref.add_pick`, `ref.remove_pair`,
`ref.clear_pairs`, `ref.rename_reference` and `ref.remove_reference(cam, delete_file=False)` do
the rest.

### Procedural look

**Strands or guides.** The shaping nodes (Noise, Curl, Smooth, Twist, Bend,
Length, Collide, Gravity, Displace, Mirror, Flow, Cull) take either: fed strands they give strands, fed guides they give
guides, each keeping its ID, root and painted maps. So guides can be shaped in the graph with no
conversion, e.g. `ToGuides -> Noise -> Clump.centers` for wavy clumps. Their ports show the
kind they carry (strands, guides, or grey while unconnected), links are checked against it, and a
link that stops fitting after a change upstream is reported. Nodes that are about strands
(Scatter, Interpolate, Width, HairColor, Braid, Instance, Cards) keep fixed types; ToGuides and
ToStrands convert explicitly.

Look operators are added to the graph between Interpolate and Length. "Along the length"
parameters are ramps, written in JSON as `[[position, value], ...]` from root (0) to tip (1); a
plain number means a constant ramp. Ramps are smooth: a monotone cubic through the keys, flat at
peaks and valleys and never overshooting, so profiles don't put kinks in the hair (two keys give a
straight line). `{"keys": [...], "linear": true}` keeps straight segments (the curve editor's
right-click menu: Linear).

| Operator | What it does | Main parameters |
| --- | --- | --- |
| `Clump` | Pulls each strand toward its nearest center; writes a `clumpId` channel (`clumpChannel`): each clump's number, from its center's ID (not its place in a list), so an edit in one place renumbers no clump elsewhere. `centersFrom` says where its centers come from: `pick` (the default) picks its own among the strands (a connected `centers` is ignored, and the node says so); `inputPoints` and `inputCurves` take the `centers` input, guides or strands (a low-density Scatter straight in, a guide level, ToGuides; of strands, their core: all of a scatter of under 5,000; nothing connected: the strands pass through, and the node says so). Connecting `centers` in an editor sets `inputPoints`; disconnecting leaves it. `inputPoints` takes only the centers' roots, to place the clumps (a single point will do), and each axis is the core strand the clump gathers nearest its center's root (else the core strand rooted nearest it), the hair as grown there (XGen's clump guides), so scattered points clump long, bent hair without flattening it onto the points; `inputCurves` follows the center curves themselves (groomed clump curves, a Noise waving the centers, whiskers to part around). Each strand's point goes toward the axis's point as far along it as the point is along the strand; `pastTip` says what a strand longer than its axis does past the axis's tip: `gather` (the default) at the tip, `extend` on along the axis's last direction, so long hair keeps its length. With `pick` (Focus on it shows the strands with its centers as dots on the scalp; these params show only with `pick`). `clumpBy` picks how the clumps are sized, and the UIs show only the param it picks: `count` (the default: about that many, whatever the scalp's size: the spacing is the rest scalp's area over the count, in 1% steps, so it re-fits to the scalp and an edit that changes the area by more than a step moves every clump) or `size` (a length: clumps about that far apart wherever strands grow; the count follows the area they cover, so a whole body gets more clumps than a head, each the same size; an edit of the scalp changes the clumps only near it; on feathers, as many along each vane as fit); a preset's size is fitted like any length. `centerWeight` (mappable) is each strand's chance to be a center, e.g. an expression for long strands only. The centers are picked locally: darts are thrown on the rest scalp (each face's from its own area and `seed`) and each lands on the nearest root, then each dart in a stable random order becomes a center unless one already taken is near it, so changing the Scatter's density or seed moves the clumps a little instead of rerolling them, and an edit of the scalp moves only the clumps around it; `randomness` is Scatter's: 0 keeps the centers 0.7 of the spacing apart (evenly spread), 1 lets them land anywhere, about as many either way; the centers come from the core strands (the ones every preview holds), so every preview shows the render's clumps. With a `lockRadius`, strands gather into a round lock of that thickness instead (dreads, wet strings); a `Twist` node after it turns the locks into ropes. A negative `strength` (to -1) pushes strands away from their centers instead: points within `repelRadius` of the center's point as far along it move out toward that distance (all the way at -1 for points on the center, easing off to none at the radius; strands beyond it untouched), across the center and never into the skin, lengths kept, the `profile` applying as ever: fur parting around whiskers, horns or eyes, their guides as the `centers` with `centersFrom` `inputCurves` (with `inputPoints` the push is from the strands nearest them, and the node says so). `flatness` squashes each clump across into a ribbon as much as it clumps (wet hair), lying along the scalp (`flatten` `surface`), turned at random per clump (`random`) or with its width along `flattenDirection` (`direction`). Per clump (by its center's ID and the `seed`, as XGen and Yeti vary clumps; all 0 by default): `volumize` (mappable) pushes each clump's strands out from its axis by that share of their distance from it, most near the root along `volumizeProfile` (1 to 0), filling the clump's volume; `strengthRandom` scales each clump's strength down by up to that share and `lockRadiusRandom` its lock radius up or down; `copyAmount` blends each clump's center curve toward the mean shape of its `copyNeighbors` nearest clumps (their curves from their roots), so neighbouring clumps share one flow. The GPU preview leaves a Clump with any of these to the CPU. What simpler nodes do is theirs: a **Noise** on its centers with `centersFrom` `inputCurves` (`ToGuides -> Noise -> Clump.centers`), or a Noise before the Clump (its own centers are strands as they come in), waves each clump as one (clump noise); a **Twist** after it spirals the clumps (ropes, locs); a **RestoreLength** after it, its `reference` the Clump's input, keeps every strand's length (Yeti's maintain length); a `strength` varying per strand (an expression such as `rand(7) < 0.03 ? 0.2 : 1`) lets a few strands escape their clump, and a **Flyaway** after it makes strays. After an Interpolate with a `regionMap`, strands clump only toward centers of their own region (`regionChannel`, `region` by default: a center's region is its strand's for centers of its own, its own value of the channel for centers that carry it, else the region of the strand rooted nearest it); a region without a center stays unclumped | `strength` (-1 to 1), `profile` (ramp), `centersFrom`, `clumpBy`, `count`, `size`, `randomness`, `centerWeight` (mappable), `pastTip`, `clumpChannel`, `regionChannel`, `seed`, `lockRadius`, `repelRadius`, `flatness`, `flatten`, `flattenDirection`; Shape: `volumize` (mappable), `volumizeProfile`, `copyAmount`, `copyNeighbors`; Random: `strengthRandom`, `lockRadiusRandom` |
| `Noise` | Smooth displacement; `strand` space (default) is independent per strand, `world` space is coherent across the groom. On a `fraction` of the strands (mappable) at a high `frequency` with one octave, it is frizz. `mode` `kinky` makes it jagged instead: random offsets at knots `frequency` to the strand's length (strand space) or to a unit of length (world space, the knots' offsets from the noise at the root, so neighbours kink alike), straight between them: sharp kinks, scraggly or frizzy hair | `amplitude`, `frequency`, `octaves`, `profile`, `space`, `fraction`, `mode` |
| `Curl` | Curls around each strand: a `helix` (the default), a planar `wave`, a `zigzag` of sharp kinks (kinky hair) or a `coil`, a tight helix `coilWidths` strand widths across (coily 4A-4C hair: it reads the widths the strands carry, so put a Width before it; a note says when they have none). Every curl starts at `phase` (turns) plus a random share of a turn (`phaseRandom`: 1, the default, any phase; 0, all alike), eased in from the root over its first quarter turn. `frequencyProfile` scales the turns per unit length along the strand (a ramp, root to tip, a share of `frequency`: curls loosening or tightening toward the tip) and `spacingRandom` (0-1) varies the spacing smoothly along each strand (a noise of its own a turn or two long; each clump's with `coherence` `clump`): the curl turns at the local frequency `frequency · frequencyProfile · (1 + spacingRandom · n)`, its angle that integrated along the strand's arc length in closed form, so it never turns back and stays the same curl at any resolution (`curl_phase.h`). `weight` (0, the default: none) stretches the turns where hair hangs below them, as a real ringlet carries the hair beneath it: the load at a point is how much the strand hangs along `weightDirection` (world space, down by default: max(0, tangent · direction), smoothed a little) times the curl's turns from there to the tip, and the local pitch grows by `1 + weight · load / 10` (at weight 1, ten turns hanging straight below double it), so curls open near the root, keep their spacing at the tip, stretch more on long ringlets than short curls and barely on hair lying flat or pointing up; each turn keeps its wire length, so the radius narrows (`r' = sqrt(r² - (q² - 1) / (2π f)²)` for a pitch stretched q-fold at f turns a unit, of the curl's own radius before `profile`, eased smoothly to 0 in its last stretch) and an overstretched curl goes straight. `coherence` `clump` curls each clump as one around its middle (the mean of its core strands, the ones every preview holds; a clump without one curls strand by strand), with one phase, frequency and plane (ringlets; after a Clump, by its `clumpChannel`). The curl is a curve of the strand's shape, not of its points (around a smooth axis through them, by its arc length, in its rotation-minimizing frames): any resolution makes the same curl. Where a strand's points are too few a turn for the `tolerance` (how far a segment may stray from the round curl, a share of the radius: 0.5, the default, 3 a turn, so no curl aliases; 0.1, 7), Curl adds points between them, at most `maxSegments` a strand (a zigzag gets the points of its kinks; 0: the points coming in; a note says how many strands got points, and a Curl that added some is the CPU's in a GPU preview) | `shape`, `radius` (mappable), `frequency` (turns per unit length), `frequencyProfile`, `randomness`, `spacingRandom`, `profile`, `coilWidths`, `phase`, `phaseRandom`, `weight`, `weightDirection`, `coherence`, `clumpChannel`, `tolerance`, `maxSegments`, `seed` |
| `Flyaway` | Stray hairs: a fraction of strands leave the flow from `start` on in slow arcs, a little longer; writes a `flyaway` channel (1 for strays; an earlier Flyaway's strays keep their 1). Put it after Clump. Strays are picked by strand ID and `seed`, so two Flyaways need different seeds (a new node gets one) | `fraction`, `stray` (both mappable), `frequency`, `start`, `standOut`, `lengthen` |
| `Static` | Hair charged with static repels itself, like dry winter hair: it lifts off the head, flares out toward the tips, and clumped locks fray into their strands. A closed form plus one solve, not a simulation. Every strand carries charge along its length by `profile` (rising toward the tips by default: dry ends carry most), some strands more than others by `randomness` (by ID). The core strands' charge (those every preview holds, at most about 8k of them; a strand's share of the scalp's area, not its width) is splatted into a sparse grid shared with SelfCollide. The electric potential is solved there (red-black SOR, `-lap phi = rho`) over the nodes within 4 cells of hair. The padding takes the potential of all the charge seen from far away (a softened point charge at its middle: hair inside a hollow of hair, such as the hanging part under the head, is pushed out, not balanced). The scalp lets no field into it, so hair is pushed off the head, never toward it. Below the grid's scale, each strand is also pushed from its neighbours' charge-weighted middle and by its own offset by ID, across itself and across the field, so a lock fans out into strands. Each strand bends from its root as a cantilever under those pushes times `strength` times its charge: each segment turns by the moment of the pushes beyond it, and the turns carry down the strand. Lengths are kept and the strand never kinks or goes into the scalp. About 45 ms for 100k strands of 16 points (`Budget.StaticEvaluation`). Evaluates in full (every strand reads the others). The GPU preview runs it while you drag or brush (its grid splatted and solved in compute: about 3 ms for 100k strands) | `strength` (mappable), `randomness`, `profile` |
| `Tangle` | Mats and wet hair: strands stick to a neighbor (roots within `radius`) over a span: onto it, or, when the two picked each other, both to the middle (at `strength` 1 they meet). Chain a few, after a tight Clump, for wet hair. Partners come from the core strands (those every preview holds), so the preview shows the render's tangles; with `sameClump` (on) from the strand's own clump, and after an Interpolate with a `regionMap` from its own region (`regionChannel`, `region` by default). A strand farther than `reach` from its partner there does not stick (0: no limit) | `fraction`, `strength` (both mappable), `radius`, `start`, `span`, `sameClump`, `regionChannel`, `reach` |
| `Wet` | Wet hair from one `wetness` slider (Bico et al., *Elastocapillary coalescence in wet hair*, Nature 2004): surface tension pulls wet strands into bundles against their stiffness, and a bundle forms only where the free length past where its strands join is long enough, longer for bigger bundles. So small clumps form near the root and merge into bigger and bigger ones toward the tip, each nested in one of the level above. `levels` sizes of clump, the biggest `clumpSize` apart, each level half the one above; level k (0 the smallest) engages from `start_k = sqrt(r_k / clumpSize) * lerp(0.9, 0.3, wetness)` of the strand on, blended in over 0.6 of that, at a tightness of `wetness (2 - wetness)` times 0.6 at the root to 0.92 at the tip. Inside, a chain of Clumps (largest first, each smaller one within the clumps of the one above), so each level's centers pull toward the next level's. Wetness 0 hands the strands on as they came: animate it from 1 to 0 to dry the hair. Writes the biggest clumps' numbers to `clumpChannel` (`clumpId`; the smaller levels to `clumpId1`, `clumpId2`...) and a `wetness` point channel for a shader. CPU only for now (the GPU preview runs up to it) | `wetness`, `clumpSize`, `levels`, `randomness`, `clumpChannel`, `seed` |
| `HairColor` | The hair's color per strand as one linear RGB (`colorR`, `colorG`, `colorB`; per point too with a tip color), a hair color in `aiStandardHair`'s base-color sense, which every host renders and exports as it is: Arnold's base color over no melanin (`hair_color` user data), the viewport's and the app's absorption by Chiang et al. 2016's mapping (`hairColorSigma`, the one Arnold maps a base color with), RenderMan's `rman_color`, Blender, Unreal's `groom_color`, USD `displayColor`, Alembic `Cd`, the cards bake. Modeled on RenderMan's PxrHairColor. `mode` `melanin` (natural, the default): the color whose absorption is the pigment's (`melanin` 0 white to 1 black, `redness` 0 brown to 1 red, both mappable; `hair_color.h` `melaninColor`, so it renders as `aiStandardHair`'s melanin would) dyed by `dye` (their absorptions added, as `aiStandardHair` combines melanin and base color); `mode` `color`: the `color` picked. Its `map` (a painted map by name, a color set on the scalp, or an image at the scalp's UVs) is read at the root: the color itself in mode color, a dye over the pigment in mode melanin. Per strand: `melaninRandom` (mode melanin, +/- melanin) or `valueRandom` (mode color, +/- share of brightness), then `hueRandom` (up to half a turn either way at 1) and `saturationRandom` (+/- share) in HSL with the lightness kept (grey and white stay neutral; `hair_color.h` `varyHairColor`); each draw mixes the strand's random number with its clump's by `clumpShare` (0 each strand its own, 1 the clump alike; the clump attribute `clumpChannel`, `clumpId` by default; a note when it reads nothing and is set off its default), keeping the spread the same. `greys` (mode melanin, mappable) is the share of strands with almost no pigment. It blends toward `tipColor` (varied with its strand) along the length by `tipStrength` x `tipProfile`, the start shifted per strand by up to `tipOffsetRandom` of the length (`tipStrength` mappable per point; above 0 the RGB is written per point too; at 0 an earlier color node's per-point colors are removed). It also writes each strand's `melanin` and `melaninRedness` as data (0 in mode color). It sets a look afresh: an earlier FeatherColor's iridescence is removed. The highlight (the Highlight section): `specular` (0-1; 0.3 is Arnold's full specular tint) and `roughness` (0-1), both mappable, each with a per-strand random spread (`specularRandom`, `roughnessRandom`), and `tilt` (cuticle tilt, degrees). Off their defaults they are written per strand (`specular`, `roughness`, `tilt`). The first HairColor making the output sets the highlight the viewport, the app, the cards and the Arnold, Maya and Blender shaders use: `create_hair_shader` sets the `aiStandardHair`'s specular tint to `min(1, specular / 0.3)`, its roughness to `0.1 + 0.4 x roughness`, its shift to the tilt, and reads the `specular` and `roughness` user data where they vary per strand. Bands and FeatherColor after it recolor its colors (see Color patterns). With several HairColor nodes, the look comes from the one making the output (walking up from the output, first inputs first; `groomlab.hair_color_node(groom)`, the graph stats' `hairColor`) | `mode`, `melanin`, `redness` (both mappable), `dye`, `color`, `map`, `greys` (mappable), `melaninRandom`, `valueRandom`, `hueRandom`, `saturationRandom`, `clumpShare`, `clumpChannel`, `tipColor`, `tipStrength` (mappable, per point), `tipProfile`, `tipOffsetRandom`, `specular`, `roughness` (both mappable), `specularRandom`, `roughnessRandom`, `tilt`, `seed` |
| `Bands` | Bands of another pigment along each hair, over the colors coming in (HairColor's, a tip too; else the pigment's): fur banded hair to hair (agouti, ticked, shaded). After a HairColor. `strength` (mappable, per point) mixes the band pigment (`melanin`, `redness`, `dye`, offset per strand by up to `randomness`) in where `profile` says, repeated `count` times root to tip; per strand the bands shift by up to `offsetRandom` of a period and stretch by up to `widthRandom`, so they do not line up across the coat. `guards` below 1 keeps that share of the strands as guard hairs (`guardsBy` `random` or the `longest`), which take the bands; the rest is undercoat, the incoming strand color times `undercoatDye` at every point (no bands, no tip color). Writes the same `colorR/G/B` channels (per strand and per point) every exporter reads; See Color patterns | `strength` (mappable, per point), `melanin`, `redness`, `dye`, `profile`, `count`, `guards`, `guardsBy`, `undercoatDye`, `randomness`, `offsetRandom`, `widthRandom`, `seed` |
| `FeatherColor` | Colors on feathers, over the colors coming in, after a HairColor (and a Propagate): a `pattern` in each feather's own flat space (vane space: `bars`, `chevrons`, `spots`, `fringe`, `eye`, a painted image of the feather with `texture`, or `none`) in its own pigment (`melanin`, `redness`, `dye`; an image its RGB) by `strength` on the `vanes` named, shaped by `count`, `width`, `angle`, `sharpness`, `offset` and `profile` and varied per feather (`offsetRandom`, `scaleRandom`, `angleRandom`); the down zone's barbs toward `downColor` by `downStrength`; and structural color, `iridescence` (mappable, per point; where: `iridescenceOn`) written as the per-point `iridescence` channel, its look `iridescenceColor` and `iridescenceShift`. Writes the `colorR/G/B` channels per point; the hosts' iridescence look is the first iridescent FeatherColor's making the output (`GroomSession::hairLook`). See Color patterns | `pattern`, `strength`, `melanin`, `redness`, `dye`, `vanes`, `count`, `width`, `angle`, `sharpness`, `offset`, `profile`, `texture`, `textureRight`, `offsetRandom`, `scaleRandom`, `angleRandom`, `downColor`, `downStrength`, `iridescence` (mappable, per point), `iridescenceColor`, `iridescenceShift`, `iridescenceOn`, `seed` |
| `RenderData` | Render controls per strand for the hair shader, beside the look (HairColor's color and highlight, which it leaves to it): `opacity` (mappable) times `opacityProfile` (a ramp root to tip), each strand up to `opacityRandom` less opaque, written per point (the `opacity` channel). The first RenderData making the output (walking up from it as for HairColor; the graph stats' `renderData`, `groomlab.render_data_node(groom)`): `create_hair_shader` wires opacity set off opaque into the `aiStandardHair`'s `opacity` (Arnold gets it varying, the curves made not opaque when any point is below 1); Blender's material mixes toward transparent by it. A later RenderData's values replace an earlier one's | `opacity` (mappable), `opacityProfile`, `opacityRandom`, `seed` |
| `Attribute` | Writes a per-strand channel `name` for later nodes or the shader: `value` is a number, a map read once, or an expression; a random value per strand is an expression of `rand` (`{"expression": "rand", "min": 0.2, "max": 0.8}`, or `mix(0.2, 0.8, rand)`) | `name`, `value` (mappable) |
| `Expression` (Fields) | A per-strand expression as a field for any exposed param: a block of lines with locals, reading the strand's shape (`$length`, `$longest`, `$reach`, `$straightness`, `$lift`, `$tipx/y/z`), root position, UV and normal, `$rand`, any strand channel (`$clumpId`, `$guideInfluence`, ...) and the node's own `variables` (numbers and vectors), remapped from 0-1 to `min`-`max`. E.g. `$length / $longest` to clump long strands harder. Per point with `$t` (arc length, root 0 to tip 1), `$pt`, `$px/py/pz`, `$pointIndex` or a point channel: e.g. `1 - $t` on Width. See **Expressions** below | `expression`, `min`, `max`, `variables` |
| `ToStrands` (Convert) | Each guide becomes a strand (the reverse of `ToGuides`): render accent guides as hair, run look nodes on guides, or `Merge` them with scattered strands. Painted guide maps become strand channels and layers painted along the guides point channels; IDs stay apart from scattered strands' | `segments` (0: the guide's points), `width`, `channels` |
| `Twist` | Turns strands around an axis: the nearest curve of its `axes` input (a lock's guide, braid paths, ToGuides), or with nothing connected the middle of each clump (after Clump: the mean of its core strands, the ones every preview holds; a clump without one does not turn), for ropes and twisted locks | `turns` (mappable, per unit length; negative turns the other way), `profile`, `clumpChannel` |
| `Braid` | Braids strands along its `braids` paths (drawn curves, guides), or with nothing on `braids` along paths it picks among the strands, about `region` apart (box braids): `three`, `fishtail`, `rope` or `cornrow`, gathering the strands rooted within `region` of a path's root (large: a ponytail), or with `feed` picked up within `feedDistance` of the path along its first `feedUntil`; `alongScalp` sweeps far-off hair over the scalp into the braid. Writes `braidId` and `braidFlat` channels | `style`, `sections`, `dutch`, `depth`, `phase`, `crossingsProfile` (ramp), `sectionTwist`, `width`, `widthProfile` (ramp), `crossings`, `lobeFill`, `region`, `feed`, `feedDistance`, `feedUntil`, `endAtPath`, `alongScalp`, `gather`, `tail`, `strays`, `fuzz`, `seed` |
| `Smooth` | Takes out each strand's bumps up to `scale` of its length (after noise, interpolation or sculpting), keeping lengths and roots: its direction along its length is averaged over a window that wide by arc length (three box filters, as a Gaussian), so a strand smooths the same at any point count and any scene size; `scale` 1 straightens it along the line from its root to its tip. `strength` blends each point toward that shape (with `profile` along the length), then the segment lengths are put back from the root. The default (0.09) takes out about what 4 passes of the old neighbour average did on 16-point strands. Strands or guides | `strength` (mappable), `scale`, `profile` |
| `Tie` | Ponytails, hair ties and clips in one node. The strands it affects (all by default; the common `affect` mapped to a Group, a painted map or an expression picks the ponytail's hair, so two Ties make a double ponytail) run from their roots to a ring at `center` (a world point: Maya's Graph Editor has a Pick beside it, from a selected vertex, locator or transform) of `radius`, facing `direction` (world; (0, 0, 0), the default: from the scalp's center through the center). Before the ring each strand is the taut string from its root over the scalp: in the plane through the scalp's center, its root and its ring point, the outer convex hull of the root, the scalp's closest points under that arc (lifted a 400th of the scalp's size off the skin) and the ring point, so it lies on the skin where the skin is in the way and leaves it along the tangent toward the tie; over its first `ease` it keeps its own shape (roots do not kink). Through the ring strands keep their arrangement: each passes at the azimuth about the tie's axis its root has (seen from the tie), out by its root's Lambert equal-area distance from the pole (divided by the most any core strand at that azimuth has, so the bundle fills the ring), so no two cross; `tightness` 1 packs them within the radius, lower up to twice as wide. Past the ring the tail runs along `direction`, fanning out to `spread` degrees at its edge and turning `twist` turns per unit length about its axis, straight otherwise (style it with Curl, Noise or Clump after, their `affect` mapped to the tail). The whole path is kept off the scalp (its collision field) and each strand is laid along it keeping every segment's length exactly; strands too short to reach the ring end on the way to it, lying along the scalp (loose wisps, never stretched). Writes the per-point `tailChannel` (`tail`): 0 before the ring, 1 past it, a smooth step over a radius either side. A second Tie writing the same channel starts each strand where the first one's tail does, so a Tie further down a tail gathers it again (a bubble ponytail; its tail then marks the part past the second). A Curl on the tail wants its `tolerance` at 0 (or enough points from a Resample before the Tie): a Curl that adds points cannot take a per-point affect. Strands with few points per ring radius pass the ring coarsely: give them points. Reads every tied strand's place (the core's), so it evaluates in full; CPU only (the GPU preview runs up to it) | `center`, `radius`, `direction`, `tightness`, `spread`, `twist`, `ease`, `tailChannel` |
| `Direction` | Points each strand (turned about its root, shape kept) by its `inclination` from the scalp's normal (0 straight out, 90 flat along the scalp) and its `roll` around the normal from where roll 0 points, `along` the scalp's U direction (`tangent`, from its UVs; without them `direction`) or a world `direction` laid onto the scalp, with random spreads per strand (by ID); `strength` turns part of the way. Yeti's Direction. Strands not rooted on the scalp stay | `inclination`, `roll`, `strength` (all mappable), `along`, `direction`, `inclinationRandom`, `rollRandom`, `seed` |
| `Bend` | Bends strands about their roots toward a direction (`along` the scalp's `normal` at the root, its U `tangent`, or a world `direction` laid onto the scalp at the root, as a comb lays hair; a strand off the scalp takes it as it is): the strand's direction at t turns by `angle` x the share of [`start`, `end`] behind t, so the bend is spread evenly between the two and the strand holds its shape past the end; lengths kept. A negative angle bends away (with the normal: down onto the scalp); a strand standing straight up bends over along the scalp's U. A strand never turns (root to tip) past the direction it bends toward, nor past its opposite bending away, and along the normal never below the scalp's tangent plane: `angle` 180 with `start` and `end` 0 points every strand along the direction (combing scales down a body), along the normal it turns each strand whole about its root (volume at the roots; negative: down onto the scalp). `direction` takes a vector expression per strand. `randomness` scales each strand's angle down by up to that share. Yeti's Bend | `angle` (mappable), `start`, `end`, `along`, `direction`, `randomness`, `seed` |
| `Transform` | Moves, turns and scales strands: each about its own root in its root's frame (`space` `strand`: x along the scalp's U, y its normal, z across), or the whole set about `pivot` in world axes (`world`); `translate`, `rotate` (degrees, x then y then z), `scale` (mappable), each with a random spread per strand (by ID); the common `affect` blends each strand toward its transformed self. Identity at its defaults. Lengthens strands too (`scale`; Length only trims). Yeti's Transform | `space`, `translate`, `rotate`, `scale`, `pivot`, `translateRandom`, `rotateRandom`, `scaleRandom`, `seed` |
| `Cull` | Thins strands out: keeps each with probability `keep`, by ID (stable; paint thin or bald patches) | `keep` (mappable), `seed` |
| `Merge` / `Switch` | Combine two strand sets, with both sides' channels / pick one (variants). A channel only one side has takes its default on the other side's strands (Clump's `clumpId` -1: no clump), except a per-point channel whose other side has a per-strand channel of the same name: each strand's value runs along its points (`StrandSet::append`), so a tipped HairColor merged with an untipped one leaves the untipped strands their own color per point; where only one side has a HairColor, the other side's strands get HairColor's default look (melanin 0.35, redness 0.2 and their RGB, per point too when the colored side has per-point colors; from their own `melanin` when they have one), never black. b's clumps, braids and feathers (`clumpId`, `braidId`, `featherId`) are numbered after a's (from the next multiple of 2^22 past a's highest, a start that which groups a has never moves), so two clumped looks stay apart for Cards, Twist and HairColor (a clump split between a and b becomes two). b's strand IDs that a already has get new ones (rehashed, deterministic), so instances, cards and exports tell them apart; other IDs are kept. Cards and instances from a node upstream of both sides are kept once. With `blend`, where two sub-grooms meet (a fringe and the body, say) strands within that distance of the other side take on its shape, halfway at the seam, so both sides meet in the same shape; roots never move | `blend`, `match` (Merge); `index` (Switch) |
| `Blend` | Mixes two looks of the same strands (branch both from one node, e.g. Interpolate): points and widths move from `a` to `b` by `mix` (0 a, 1 b; mappable, per point; a Control node makes it a keyable slider) times `profile` along the length; writes the mix to a channel for the shader (`mixChannel`, `blend` by default). With `include` `shapeAndLook` (the default) the look mixes too: HairColor's RGB per point by the same weight (written per point when a side has per-point colors or the weight varies along the strands), and per strand the RGB, `melanin`, `melaninRedness` and `roughness` by the strand's mean weight along it; a side without HairColor counts as its melanin's color or HairColor's default look, as in Merge. IDs (`clumpId`, `braidId`, `featherId`, `groupId`...) and every other channel stay a's. `shape` moves only points and widths | `mix`, `profile`, `include`, `mixChannel` |
| `Length` | Trims strands: each keeps `keep` of its length from the root (mappable: paint or write where hair is short). A `fraction` of the strands (mappable; picked by ID, so raising it only adds strands) are trimmed at random, each losing a share of its kept length drawn between `trimMin` and `trimMax` (its own draw, so widening the range scales each strand's trim smoothly): uneven ends. Lengthening is `Transform`'s `scale` | `keep`, `fraction`, `trimMin`, `trimMax`, `seed` |
| `Width` | Strand widths (diameters, scene units): `width` (mappable, per strand or per point) times `profile` along the length by arc length; `randomness` makes each strand up to that share thinner or thicker (a draw from its ID and `seed`), as real hair varies | `width`, `profile`, `randomness`, `seed` |
| `Gravity` | Bends strands from the root under their weight toward `direction` (world -Y, or painted per root in `directionColorSet`), keeping segment lengths: each segment turns further toward gravity the more hair hangs beyond it and the softer the strand is there (`stiffness` x `stiffnessProfile`, 1 at the root by default so strands leave the scalp in their own direction), the turns carried down the strand so curls and waves hang whole; the same at any point count. A `Collide` after it keeps strands bent under the scalp out of it | `strength` (mappable), `direction`, `directionColorSet`, `stiffness`, `stiffnessProfile` |
| `Displace` | Offsets strands along the scalp's normal at their root (`along` `normal`) or along a world `direction` (`along` `direction`), by `distance` (mappable, per point; negative the other way) times `profile`; the root stays. Volume off the scalp, or a region pushed in | `distance`, `profile`, `along`, `direction` |
| `Mirror` | Mirrors guides or strands across the plane where `axis` (x, y, z) equals `offset`, procedurally: `replace` keeps the strands rooted on `side` and mirrors them onto the other side, which loses its own (a symmetric groom from one half); `add` keeps every strand and adds its mirror. A strand rooted on the plane is kept once. Each mirror is rooted again on the scalp where its mirrored root lands (`reattach`: the closest point's face and barycentrics, so maps and UVs read there; it is moved onto it), keeps the strand's widths and channels and gets a new ID of its own. On guides (Guides -> Mirror -> Interpolate's guides) the groom grows symmetric and each side gets its own random look; on strands after the look, the look itself is mirrored exactly | `axis`, `offset`, `mode`, `side`, `reattach` |
| `Collide` | Pushes strands out of the scalp (connect a `Scalp` node to its `scalp` input) and out of any other mesh on its `collider` input (a `ColliderMesh` node), keeping segment lengths where it can. `method` `field` (the default) reads each mesh's signed distance field alone, made once and kept across evaluations (what the GPU preview runs too); `exact` asks the mesh for its closest point at every point (the reference; the CPU's alone). On the `collider` input's mesh (a floor, a shoulder: the scalp, which every root sits on, keeps its hard push, and a strand the collider never moves is left as without them), `softness` eases the push in over a band that far above the offset (a point within it rises smoothly, to a smooth maximum of its height and the offset), so strands curve away before they touch instead of folding flat where they meet the surface (a strand lying along it rests within the softness above the offset); `smoothing` spreads each push back toward the root over that length (a smoothstep-weighted mean of the pushes after each point), the strand turned segment by segment (lengths kept) onto the surface, then walked again so no point stays inside. With either, once a point has moved the walk carries each later segment along its own heading, so a strand meeting a surface slides along it rather than bunching up; both 0 by default (the hard push, as before) | `offset`, `softness`, `smoothing`, `maxDepth` (deeper points are on the far side of an open scalp), `iterations`, `method` |
| `SelfCollide` | Hair that no longer passes through itself: where strands are packed tighter than `packing` (the share of space hair may fill; real hair about 0.2 to 0.5), they are pushed apart, so layers rest on one another and hair piles up with volume (behind the ears, over a collar, at the crown); hair with room does not move (a set with no hair overpacked comes out as it came). Static, not a simulation: hair taken as an incompressible material, as a fluid's pressure projection takes water, in 3 passes. Each splats the strands' segments into a sparse grid (cells about two root spacings of the strands it reads, between 1/128 and 1/16 of the scalp's size): each sample its strand's cross-section (pi w^2 / 4 for strands with widths, a Width before it; else a nominal 2% of the scalp's area per strand), so rho at a node is the share of space hair fills. Where rho passes `packing`, a pressure solve (red-black SOR over the overpacked nodes, p 0 elsewhere, no flow into the scalp) spreads the excess. A layer much thinner than a cell (hair combed flat, two sheets crossing) is measured from its samples (their spread across it and their widths) and spread across itself to the thickness its hair needs, its strands' order kept (a strand's own small offset, by ID, stacks strands lying exactly together); one lying on the scalp spreads away from it from the bottom up. Points move by that times `strength` and `profile`, at most a cell a pass, each strand walked from its root keeping its segments' lengths, never into the scalp (the scalp external, kept a hair's width off). Reads only the core strands (those every preview holds, at most about 8k of them), so every preview shows the render's strands; 75 to 100 ms for 100k strands of 16 points nearly all overpacked (`Budget.SelfCollideEvaluation`). Evaluates in full (every strand reads the others). The GPU preview runs it while you drag or brush (its grid splatted and solved in compute: about 5 ms for 100k strands) | `strength` (mappable), `packing`, `profile` |
| `Sculpt` | Brush strokes on guides the graph made (a `ToGuides`, a guide level, a Noise on guides), kept in the node: see **Sculpting generated guides**. Each edited guide's stroke is stored by its ID as a relative form (per segment, the turn in the segment's own rotation-minimizing frame and the length factor, by normalized arc length), so when what feeds the node changes the same bends and length changes go onto the new shapes. No edits, or `strength` 0: the guides as fed, to the bit. An optional `scalp` input gives each guide's root frame (the surface's; without one, world axes). CPU only (it feeds guides: the GPU preview reads them from the CPU) | `strength` (0: as fed; past 1 more) |
| `MeshCut` | Cuts strands where they cross the mesh on its `mesh` input (a `ColliderMesh`, the `Scalp`): `keep` `outside` (an obstacle: cut where a strand first goes into it) or `inside` (a hull, a haircut's shape: where it first comes out); `offset` cuts that much shorter. Inside is behind the mesh's normals. A strand rooted on the side cut away is cut to nothing (every point at its root). The kept part is spread over the strand's points again (evenly by length), so IDs, point counts and widths along it stay. XGen's Mesh Cut | `keep`, `offset` |
| `BlendNeighbors` | Pulls each strand's shape toward the mean shape of its nearest neighbours (of the core strands, the ones every preview holds; by root, on the rest scalp; never across a part, nor, after an Interpolate with a `regionMap`, outside its region: `regionChannel`, `region` by default), for a coherent flow after noisy interpolation, sculpting or simulation. Neighbours' shapes are turned to this strand's root normal; roots stay and each strand keeps its length | `neighbors`, `strength` (mappable), `profile`, `preserveLength`, `respectParts`, `regionChannel` |
| `RestoreLength` | Brings each strand's length back from the `reference` input (the same strands from before the deformers, matched by ID): every segment's length along the deformed directions (`segments`), or the whole strand scaled about its root (`scale`). Without a reference, `even` respaces each strand's own points evenly by arc length; the common `affect` takes part of the way | `mode` |

For multi-level clumping, chain Clumps from small to large: the small clumps form first and the
large ones then carry them whole, so each lock reads as clumps of clumps (as `cinematic_hair` and
`everyday_sim` do). Large first, the small clumps' centers fall without regard to the locks and
pull strands across their borders. Each Clump writes its own `clumpChannel` (say `clumpId` for
the small, `lockId` for the large), which HairColor's `clumpChannel` can share randomness by.

Put `Collide` last when strands are combed or clumped into the scalp. Each point of its walk asks
how far above a surface it is, along the closest point's normal: with `method` `field` (the
default) that is a lookup in the mesh's sparse signed distance field (`distance_field.h`: cells of
about the offset, sampled from the mesh's own closest points where the hair goes and kept across
evaluations, so a brush dab or a param tweak upstream pays only lookups), and points land within a
few percent of the offset of where the exact closest point puts them (micrometres on a head in
centimetres). Where the field cannot tell as the mesh would (deep under the surface, across a
crease, the edge of the search) it asks the mesh. On the app's head at 100k strands a brush dab's
Collide went from 31 ms to 1.6, the first evaluation from 158 ms to 88
(`tools/bench/xgen_compare/results/2026-10-03_collide_field.md`). A scalp deformed away from its
rest pose (each frame of an animation is a mesh of its own) is asked directly, as `exact` does;
followScalp evaluates on the rest pose, which keeps its field.

**Collide with other meshes.** A `ColliderMesh` node brings any mesh the host registers into the
graph, by name (`collider` by default): connect it to Collide's `collider` input and strands are
pushed out of that mesh as well as the scalp, at every point of the same root-to-tip walk (a
shoulder, a collar, a hat). In code the meshes are `GroomData::meshes` (`GroomSession::setMesh(name,
mesh)`, `set_mesh` in Python), each registered as the external `mesh:<name>`; a ColliderMesh with
no mesh of its name yields nothing, and Collide then hits the scalp alone. They are not stored in
groom files. Deformers such as `Noise` and `Collide` stretch or shorten strands as they
push points about; put a `RestoreLength` after them, with the strands from before the deformers on
its `reference` input, to get the lengths back.

### Motion without a cache

Two nodes move the groom with no simulation and no cache: each frame is worked out on its own,
from the time and the scalp's last few frames. So a shot scrubs (any frame, in any order, looks
the same as when played to), renders on a farm frame by frame and caches the same every time.
Both are non-destructive: bypass or delete them and the groom is exactly what it was, and at
rest (no wind, a still scalp) they give their strands back to the bit. They keep roots and
segment lengths (segments turn, never stretch), and work on strands or guides.

| Operator | What it does | Main parameters |
| --- | --- | --- |
| `Wind` | Bends strands along a wind blowing toward `direction`. Its strength varies over space and time through a noise field that travels with the wind: `frequency` sets the gusts' size (features per scene unit), `speed` how fast they pass (scene units per second), `gust` how much the wind varies (and sways its direction). `turbulence` adds a fast flutter that grows toward the tips. Each segment turns toward the wind by strength x gust x `profile` (root to tip; stiff root, free tip by default). With `loop` the wind repeats every `loopEnd` - `loopStart` frames, seamlessly (see **Looping animation** below). A function of the strand, the time and the params: strength 0 is no wind at all | `direction`, `strength` (mappable), `gust`, `frequency`, `speed`, `turbulence`, `profile`, `loop`, `loopStart`, `loopEnd`, `seed` |
| `Jiggle` | Secondary motion from the scalp's motion: each strand hangs from its root's frame on the scalp by a damped spring, and where that frame was over the last `history` frames gives the spring's lag behind it now. Strands trail when the head moves, overshoot when it stops and settle; a turning head drags them too. The lag turns each strand about its root, by `profile` along it (tips lag more). With `loop`, the frames before `loopStart` are read from the end of the range, so a scalp animation that loops over it jiggles seamlessly. A scalp that did not move over the history changes nothing | `strength` (mappable), `frequency` (the swing's frequency, cycles per second), `damping` (0 swings on, 1 settles without overshoot), `history` (frames), `profile`, `loop`, `loopStart`, `loopEnd` |

- **Looping animation:** turn on `loop` and set `loopStart` and `loopEnd` (frames) to make a
  cycle (a game idle, a looping clip): the groom at `loopEnd` is the groom at `loopStart`, and
  the motion runs on across the seam without a jump or a kink. Wind repeats its time over the
  range (before and after it too): its gusts walk a closed path through a four-dimensional noise
  (the field travels `speed` x the loop's length along the wind each cycle, so the gusts repeat
  that far apart along it) and the flutter turns a whole number of cycles a loop. Jiggle reads a
  frame before `loopStart` that many frames before `loopEnd` (the host samples those frames of
  the scalp too), so give it a scalp animation that loops over the same range (its pose at
  `loopEnd` its pose at `loopStart`). Render `loopStart` to `loopEnd` - 1 and play them on
  repeat. While Maya plays the first time through, frames near `loopEnd` not seen yet read as the
  scalp now (still), as for any history frame; stopped on a frame, or from the second time
  round, they are read. `Simulate` has no loop (a simulation steps from its start frame).
- **Adding them:** Add Node > Motion in the Graph Editor, or `groomlab.add_wind(groom,
  direction=(1, 0, 0), strength=0.3, ...)` and `groomlab.add_jiggle(groom, strength=1.0, ...)`.
  The helpers put the node just before the output, before a Collide or Cards node that ends the
  chain (`groomlab.motion_anchor`): the look is finished when it moves, strands collide where they
  moved to, and cards, exports and renders all take the motion.
- **Previewing:** play the timeline. Only the motion nodes and what follows them re-evaluate from
  frame to frame (Wind's cache key holds the time, Jiggle's the scalp's history); a still groom
  with a bypassed or zero-strength Wind stays cached. Wind costs about 2 ms per 50k preview
  strands.
- **Where Jiggle's history comes from:** the graph reads the scalp's earlier frames as the
  external `scalpHistory` (its points at t - 1 frame ... t - `history` frames). In Maya the
  groom reads them only while a Jiggle node makes the output, by evaluating its `inMesh` at those
  times, and keeps them by time: playing or scrubbing forward reads no new frame (each earlier
  frame was the current one before), and a frame visited again is not read twice; stopped on a
  frame of a moving scalp (a script setting the time, an edit to the animation), every frame of
  the history is read again. Exports and the Arnold translator read the same frames, so renders
  and caches match the viewport. The CLI and the procedural sample a groom file's animated scalp
  cache (`scalpCache`); without one nothing moves, and Jiggle passes the strands through with a
  note (`groomlab-cli eval` prints it; the Graph Editor shows a node's note as its warning badge).
  `groomLabGraphStats` reports `scalpHistory` (`frames`, and `reads`: inMesh evaluations at other
  times so far) and each node's `notes`.
- **Longer memory:** the spring starts settled at the oldest frame of the history, so a swing
  older than `history` frames is forgotten. Give slow, loosely damped hair (a low `frequency`, a
  low `damping`) more history, about 1 / (damping x stiffness) seconds' worth.
- **Not yet:** Unreal card exports carry no wind weights; cards take the motion as geometry.

### Simulating the guides

`Simulate` (Guides, an optional collider Mesh -> Guides) is GroomLab's own hair simulation, in the
core, so Maya, the standalone app, groom files, the CLI and the Arnold procedural all run it the
same way. Between the guides and Interpolate, it moves the guides as a position-based rod solver
does over the frames since `startFrame` (from rest in the groomed shape): segments keep their
length (`stretchStiffness`), the groomed curves and twist pull back (`bend`, `twist`), points are held to
the groomed shape moving with the root by `rootStiffness` (mappable) times `rootStiffnessProfile` along the length, roots ride the scalp;
gravity, air `drag` toward a gusting wind (`windSpeed`, `windDirection`, `windGust`,
`windFrequency`), `damping` of the motion relative to the root, `inertia` (the share of the head's
motion the hair feels), collisions with the scalp and the collider (`collisionOffset`, `friction`)
and an optional guide-guide `repulsion`; `substeps` and `iterations` trade speed for accuracy.
The same inputs give the same frames, bit for bit, on any thread count.

- **Frames over time:** the host's session keeps every frame. Playing (or scrubbing past the last
  frame) steps on from the last frame (`cacheMode` `live`); scrubbing back reads the frames kept; a
  changed setting, or other guides or animation at a frame shown, simulates again from the start
  frame. `cached` steps only in Simulate Range.
- **Simulate, bake, render:** Simulate Range fills a range; Bake writes each Simulate node's
  frames as an Alembic guide cache (`strandId` the guide's ID) that the groom (Maya's `simCaches`,
  a groom file's `"simCaches"`) names with the stamp of the settings it was made with. Renders,
  exports and farms read the bake; a node whose settings changed since ignores it and simulates.
- **Maya:** GroomLab > Simulation > Add Simulate Node, Simulate Range, Bake Simulation, Clear
  Simulation; `groomlab.sim.add_simulate`, `simulate`, `bake_simulation`, `invalidate_simulation`;
  `groomLabSimulate`. The time is the scene's; the scalp is read at each frame on the way. nHair
  (Simulate Guides) is still there beside it.
- **The standalone app:** the timeline's Simulate, Bake and Clear, and a strip of the frames
  simulated and baked; `--simulate N`, `--bake-simulation N` from the command line.
- **Bypass** gives the groom exactly as it was.
- **Cost:** about 11 ms a frame for 1k guides of 20 points, 31 for 5k and 42-46 for 10k (4
  substeps, 6 iterations, scalp collisions; 24 threads): at 5k each substep takes about 8 ms,
  a fifth of a frame's time at 24 fps.
- **Memory:** every frame is kept (about 2 MB a frame at 5k guides of 20 points;
  `simulation_status` reports it): bake long shots and clear the frames.
- **Not yet:** guides inside a compound are not simulated (they pass through); colliders and guide
  curves at the frames on the way to a jump are read as they are now (the scalp is read at each
  frame).

### Expressions

An **Expression** node (Add Node > Fields & Curves) turns a formula into a value per strand for
any exposed param. In the Graph Editor its panel is a small code editor:
- **Completion:** type `$` (or Ctrl+Space) for every variable, channel and function, each with
  what it is; hover a name for the same. Your own variables complete first.
- **A block, not a line:** `$name = ...;` lines make locals for the lines below, `#` or `//`
  start a comment, and the last line is the value:
  ```
  # 1 near a point, fading out by the radius
  $d = sqrt(($x - $center.x)^2 + ($y - $center.y)^2 + ($z - $center.z)^2);
  1 - smoothstep($radius * 0.5, $radius, $d)
  ```
  Names take a `$` as in XGen, or none (`length` still works).
- **Checked as you type:** after a pause the text is compiled; a mistake shows under the code
  with its line tinted (a missing `;` says so), and only a text that checks out is applied. Leaving
  the field or Ctrl+Enter applies it anyway. Mistakes found when the groom evaluates (a channel
  the strands don't carry) show there too.
- **Your own variables:** **+ Number** and **+ Vector** add variables the expression reads as
  `$amount` or `$center.x` (`.y`, `.z`; `.r .g .b` too), each with a slider or three fields in the
  panel, so a look is dialed without touching the code. Dragging a slider shows the groom change
  as you drag (outside undo; letting go is one undo step). They are stored on the node
  (`variables`, JSON: `{"amount": 0.5, "center": [0, 1, 0]}`).
- **Keyable variables:** the diamond beside a number makes it an attribute on the groom,
  `ctl_<node>_<name>` (Channel Box, and the Attribute Editor's **Controls**): key it, animate
  it, drive it from a rig; the slider then sets the attribute. Stored as
  `{"value": 0.5, "control": "expression1_amount"}` (the value where no attribute sets it: groom
  files, other hosts); the diamond again makes it a plain number at its current value.
- **The library** (the **Library** menu): examples ship with GroomLab (long strands, top of
  the head, stripes, tip fade, a band along each strand, random per clump, a few random strands,
  noise patches, near a point, straight strands, standing strands), and **Save to Library...**
  keeps yours. Picking one loads its text, variables and range.
- **Where they are kept:** a node's expression and variables are part of the groom's graph, so
  they are saved in the scene file (`.ma`/`.mb`) and travel with it; nothing goes in the
  scene's `groomlab` folder (that is for images: masks, bakes, exports). The library is for
  reuse across scenes: it is read from every folder on `GROOMLAB_EXPRESSION_PATH` (a studio's
  shared set), then the user's `<Maya user app dir>/groomlab/expressions` (where Save writes, so
  a saved expression is there in every new scene), then the shipped examples; a name found
  earlier hides a later one. Graph presets work the same way (`GROOMLAB_PRESET_PATH`, the user's
  `groomlab/presets`, the shipped ones).
- **From scripts:** `groomlab.expressions` (`list_expressions`, `save_expression`,
  `apply_expression(groom, node, name)`, `check(text, variables)`) and
  `cmds.groomLabExpression(check=text, variables=json)`, which returns what is wrong or "".

Mappable params set to an expression (the **Map** button > Expression) get the same editor,
three lines tall.

### Fields: driving params from other nodes

Any mappable param can be exposed as an input and driven by a **field**: a value per strand
coming from another node (per point, where the field varies along the strand: an `Expression`
of `t` or of a point channel, and anything `FieldRemap` or `FieldCombine` build on them;
see Per-point maps under Maps).
- **Exposing:** right-click the node and choose **Expose Param as Input**, or right-click the
  param's name in the side panel. The node gets a diamond port for it.
- **Driving it:** connect a field node there. The param's own number multiplies the field: a 0-1
  mask on a density of 100 gives 0-100, so masks and noise stay 0-1 and the amount stays on the
  param (a param whose own value is a map takes the field alone). A driven param shows what
  drives it; its own value stands alone again once the link is removed.

| Node | Outputs |
| --- | --- |
| `Map` | A map painted in Groom Tools > Paint as a Field for any mappable param, found by `name` (the groom tells the graph where it lives: on the guides, the scalp's vertices or in a texture, and its file), or an image file's path, read at the scalp's UVs. Five outputs: `luminance` (the first: what a link naming no output reads) and `r`, `g`, `b`, `a`, each one channel of a color map (masks packed in one map: one Map node feeds clump strength from R and frizz from G); a gray map gives its value on luminance, r, g and b and 1 on a, a guide layer its value on all five. Feed Param connects it (from luminance), Paint... opens it; `min`, `max` (min above max inverts it). A strand channel as a node is an `Expression` (`$name`) |
| `FieldNoise` | Smooth 3D noise over the scalp, from `min` to `max` (`frequency`, `octaves`, `offset`, `seed`) |
| `FieldRemap` | A field reshaped: `[inMin, inMax]` through a `curve` to `[outMin, outMax]` (inMin above inMax flips the input) |
| `FieldCombine` | Two fields, or a field and a number `b`: multiply, add, subtract, min, max or mix |
| `Control` | A value set on the groom: the keyable attribute `ctl_<name>` (channel box, animatable), `default` until set. Right-click a mappable param and choose **Drive with Slider**, or `groomlab.add_control(groom, node, param)` |

One texture or noise can drive many params, each reshaped by its own `FieldRemap`. On Scatter
density, the field's maximum sets the scatter density.

```python
model = graph.GraphModel.from_groom(groom)
model.expose_param("length", "keep")
noise = model.add_node("FieldNoise", params={"frequency": 3.0, "min": 0.4, "max": 1.0})
model.connect(noise, "length", "keep")
model.save(groom)
```

In graph JSON, a node lists its exposed params in `"paramInputs"` and the links name the param
as the input. A keyed param is listed in `"paramControls"` with the control that sets it:
`{"keep": "length_keep"}`, or three names for a vector (`["wind_directionX", ...]`); its
`"params"` value is used where no control is set.

**Several outputs.** A node can have more than one output (the Map node: `luminance`, `r`, `g`,
`b`, `a`, and `rgb`, its color, for a color param that takes one: ParamSpec `colorField`, the
spec JSON's `"colorField": true`, read with `ColorField` in field.h; HairColor's `color`, `dye`
and `tipColor`, Bands' `dye` and `undercoatDye`, FeatherColor's `dye` and `downColor`; the param's
own color times the map's at each root; a number param reads its luminance). The Graph Editor draws one labelled port per output down the node's right edge; drag
a link from the one to read. It runs once for all of them. A link names the output it reads in
`"output"`; without it, it reads the first, so graphs from before read what they always did (a
Map node's old `channel` param is ignored now: such a node reads luminance until its links are
dragged from a channel's port):

```json
{"from": "packed", "to": "clump", "input": "strength", "output": "r"}
```

```python
model.connect("packed", "clump", "strength", output="r")   # GraphModel; None or omitted: the first
model.output_of("clump", "strength")                        # "r"
model.output_names("packed")                                # ["luminance", "r", "g", "b", "a", "rgb"]
```

### Braids

Draw curves from the scalp along where each braid should run. Add a **Braid** node in the Graph
Editor, select the curves and click the node's **Paths from Selected Curves** (they become guides
in its paths group; a Guides node for them is made and connected). By script:

```python
groomlab.add_braid(groom, ["braidCurve1"], style="three", width=0.15, crossings=2.5, region=0.3)
```

- **Paths:** the curves become the guide group `braids`, and a Guides node reads it into a
  Braid node placed after Interpolate. Noise and frizz later in the graph add flyaways.
- **Styles:** `three` (classic), `fishtail`, `rope` (two-strand twist) and `cornrow` (flat on
  the scalp). `dutch` raises the braid by crossing the lobes under.
- **Which strands:** those rooted within `region` of a braid's root. They leave the scalp over
  the first `gather` of their length, and over the last `tail` the lobes let go and the ends
  fan out.
- **Feed** (`feed`, on for cornrows by default in `add_braid`): strands within `feedDistance`
  of anywhere along a path join it where they reach it (their own shape, e.g. guides combed
  toward the braid, or straight from the root) and run along it from there, as hair is picked
  up into cornrows and French braids; the root region (`region`) joins as well. `feedUntil`
  (0-1) picks up strands along that first share of the path only: past it the braid hangs free,
  a French braid becoming a plain braid off the head (0: only the root region joins). With
  `endAtPath` (on by default) each strand is fitted to the path: however long it is and
  wherever it joined, it ends within the braid's last stretch (staggered over `tail`). Off,
  longer strands hang straight on past the path's end.
- **Ponytail braids:** draw the path from the ponytail base and give it a large `region`
  (0.45, say). With `alongScalp` (on by default) hair drawn in from afar sweeps over the scalp
  into the base (pressed a quarter of the braid's width above it) instead of cutting straight
  through the head; off, it goes in straight lines.
- **Box braids:** no drawn paths: leave the Braid's `braids` input open and it picks its own
  paths among the strands, about `region` apart, and every strand joins the path nearest it (the
  `braids` preset).
- **Channels:** the node writes `braidId` per strand (the path's index in the paths group, or
  for paths of its own a number from the picked strand's ID, so an edit elsewhere renumbers no
  braid; -1 for strands not braided, strays included) and `braidFlat` (1 for a cornrow, lying on the
  scalp); the Cards node uses them to build braid cards.
- **Braid paths are not hair:** the other Guides nodes list the paths group in `exclude`, so
  Interpolate never uses a path as a hair guide.
- **Shape:** `width` along a `widthProfile` curve, `crossings` per unit of length, `lobeFill`,
  `strays` (loose strands) and `fuzz`.
- **Look:** `sections` weaves more strands (4-8 for a four- or five-strand braid or cornrow, 3+
  plies for a rope; 0 keeps the style's own). `depth` sets the braid's thickness out of the scalp
  against its width (below 1 flatter, above rounder). `phase` slides the weave along the braid
  (in repeats), `crossingsProfile` varies the crossings from start to end (a curve times `crossings`:
  loose at the top, tight at the tail, say), and `sectionTwist` twists each section's hair around
  itself per repeat, as real sections do.

### Instancing geometry on strands

The `Instance` node puts a mesh on the strands, for snow, dandruff, dew, burrs or beads:

```python
flake = cmds.polyPlatonicSolid(name="snowflake")[0]
cmds.setAttr(flake + ".visibility", False)            # only the instances render
groomlab.add_instances(groom, flake, probability=0.08, scale=0.03, orient="random", t_range=(0.7, 1.0))
```

- **Placement:** each strand gets up to `count` instances. Each is kept with `probability`, a
  number or any map, so you can paint where the snow falls. It sits between `tMin` and `tMax`
  along the strand.
- **Look:** `orient` is `strand` (along it), `up`, `root` or `random`: what the prototype's
  **+Y** follows, so model a prototype with its long axis up +Y. `scale` is also mappable, and
  `scaleRandom`, `spinRandom` and `offset` add variation.
- **Size along the strand:** with `scaleByWidth` (on by default) each instance's size is
  multiplied by the strand's width where it sits over the strand's root width, so it follows the
  Width node's profile: smaller toward thin tips, and following width changes over time. A
  strand with no width at its root keeps the size as it is. Off, every instance has its `scale`.
- **Sized to the strand (`sizeMode`):** `fixed` (the default) makes `scale` a size in scene
  units, the same on every axis. `fitStrand` makes each instance span its strand, for leaves,
  feathers or scales on strands: along +Y it is `fitScale` (mappable, 1 = the whole strand) times
  the strand's **rest** length, and across (X and Z) `aspect` (mappable) times that, times the
  width factor when `scaleByWidth` is on; `scaleRandom` varies both alike. Model the prototype 1
  unit long up +Y from its base at the origin (and 1 unit wide for `aspect` to be its width over
  its length); put it at the root with `tMin` = `tMax` = 0. The length comes from the rest pose
  (below), never the length now, so the size holds while strands stretch and from frame to frame
  instead of pulsing with a simulation. `fitScale` is its own param rather than `scale` read as
  a fraction, since `scale` is a length: its unit shows in the editor, and presets fitted to
  another scalp scale it, which a fraction must not be.
  ```python
  groomlab.add_instances(groom, "leaf", probability=1.0, t_range=(0, 0), size_mode="fitStrand",
                         fit_scale=1.0, aspect=0.25, orient="strand")
  ```
- **Following the strands:** instances remember their strand and position along it, so nodes
  after the Instance node (curl, collide, simulation) carry them along, and preview density
  thins them with the strands. `add_instances` puts the node just before the output.
- **Stretching strands (`pinToRest`):** an instance sits at a fraction of its strand's length
  (between `tMin` and `tMax`), so a strand that stretches (simulated guides, a guide cache,
  correctives) slides it outward. With `pinToRest` on, it keeps the distance from the root that
  fraction gives on the strand at rest instead, and sits at the tip when the strand is shorter
  than that now. The rest pose is the groom without anything that moves: the scalp's rest pose
  (Rest mesh; its points without one), the guides as authored (no simulation, cache or
  corrective), no colliders, still Jiggle, and Wind and ImportCurves at time 0. It is evaluated
  only while an Instance node pins (`pinToRest`) or fits (`fitStrand`), once per edit, not per
  frame, and the viewport, exports, Arnold and the procedural all use it.
  Flat guides (connected curves) are taken as they are, so their animation is part of the rest.
- **Viewport:** each instance draws its prototype mesh, shaded in the theme's instance color
  (`displayInstances`), and follows edits to that mesh. A prototype that is not in the scene, or
  instances past a two-million-triangle budget, draw as small wire markers instead.
- **Arnold:** each prototype mesh becomes a hidden polymesh with its own shading group, and an
  `instancer` places the copies. The procedural (groom-file renders) finds prototypes by name in
  the scene. With deformation blur the instances blur with the hair: `instance_matrix` gets a key
  per motion step (see Motion blur). A fitted instance's matrix carries its per-axis scale in
  every key.
- **USD:** a `PointInstancer` beside the curves, with placeholder prototype prims to reference
  geometry into; `scales` is per axis (a fitted instance's X, Y, Z sizes).
- **Variants:** several prototype names separated by spaces (`"rock pebble shell"`) give each
  instance one of them, by its ID and the `seed` (`variantBy` `random`), or by the `variant` param
  (`map`: 0-1 over the list, mappable: a painted map, a channel, an expression).
- **Bent along the strand (`deform` `alongStrand`, Yeti's instance deform):** each copy bends
  along its strand instead of sitting rigid: every vertex's place along the prototype's long axis
  (`deformAxis`, +Y by default) is laid onto the strand's arc length from where the instance sits,
  across it in the strand's frame carried along from there (no twist of its own), +Z facing the
  feather's vane (Propagate's `vaneNormalX/Y/Z`) or else the skin at the root; `spin`, `offset`,
  `scale`/`fitStrand` and `scaleByWidth` apply as before. Leaves on vines (`vine_leaves`),
  modelled feathers following their shafts, ribbons. A bent copy is real geometry, made the way
  scales are (scales.h): the viewport draws it, Arnold renders it in the scales' polymesh (its
  prototype's shading group, the scales' user data, deformation blur per motion key) instead of
  the instancer, and USD and Alembic exports (Maya, the CLI, Blender) write it in the scales'
  mesh. `deform` `none` (the default) is exactly as before.
  ```python
  groomlab.add_instances(groom, "leaf", probability=1.0, count=6, t_range=(0.1, 0.95), size_mode="fitStrand",
                         fit_scale=0.16, aspect=0.45, spin_random=0.1, deform="alongStrand")
  ```

### Scales

Scales (dragons, reptiles, pangolins) are rigid plates, not hair, and the strands lay them out:
Scatter, Flow, Bend and maps decide where each scale sits and which way it points, the strand's
length is the scale's length, its root width the scale's breadth, its HairColor its color. Every
grooming tool works on them unchanged. The `Scales` node (Look) puts one scale at each strand's
root; the preset `dragon_scales` is the whole chain, `scales_from_guides` and
`scales_from_flow` point them by guides and by flow arrows, `swept_spikes` bends them along the strands.

- **Placement:** at the root, its **+Y** along the strand's direction laid onto the skin, **+Z**
  the skin's normal, X across. `size` (mappable) scales the length (the strand's rest length,
  steady while strands stretch) and the breadth (its root width) together; without widths the
  breadth is `aspect` of the length. `scaleRandom` varies the size, `spinRandom` turns each about
  the normal (1: half a turn either way). `offset` is the height above the skin. One scale per
  strand.
- **Which way:** `aim` `tip` (the default) points each scale along its strand's chord, root to
  tip: where a strand as long as its plate puts the plate's tip, and steady under the noise and
  curl strands pick up toward their tips. `aim` `root` takes the strand's direction near its root
  (to the point a tenth of its length along, at least its first segment), for strands that curl
  back over their root.
- **Follow the strand:** `follow` `skin` (the default) lays each plate flat on the skin;
  `follow` `strand` bends it along its strand's curve: the base on the skin, then (blended in over
  the first quarter of the plate) every vertex about the strand's path at its distance along the
  plate, the plate's width axis carried along the strand without twist, its thickness kept, past
  the strand's tip straight on. Swept scales, spikes and quills bend with the guides, Flow's lift
  or a Curl; `conform` still keeps every vertex above the skin, and `follow` `strand` always aims
  by the root. A scale following its strand is kept per scale until its strand moves.
- **Prototypes:** the built-in plates need no scene mesh: `groomlab:scale` (keeled and pointed,
  slightly domed, bevelled), `groomlab:scaleRound` (a rounded tip, no keel) and
  `groomlab:scaleShield` (broad and blunt, the belly's). Or a mesh of your own (Prototype from
  Selection): model it 1 unit long up **+Y** from its base at the origin, 1 unit wide along X,
  lying on the XY plane with its top up **+Z**. Several names are variants (above): the preset
  takes the keeled plate on the back and the shield on the belly by a map of the normal.
- **Overlap like roof tiles:** density and the strands' length make the overlap; `tile` (1 by
  default) tilts each scale about its base so its tip rises by its own plate height, over the
  next scale's base; `lift` adds degrees. `stack` raises each by a random fraction of its plate's
  height, so overlapping scales never share a depth (no z-fighting).
- **Conform:** `conform` (on) bends each scale onto the skin: every vertex laid onto the surface
  under it along the normal, at its height above the skin plus the offset, and pushed out where a
  fold would leave it inside, through the skin's signed distance field (the one Collide uses).
  The bent scales are kept per scale and bent again only when their strand, the skin or the
  prototype changed (about 0.75 s for 50,000 scales of ~100 vertices on 24 threads; a rebuild
  that keeps them all, 0.14 s).
- **Shader data:** each scale carries its strand's color along it (base to tip: a darker tip
  from HairColor's `tipStrength`), a random ID, the scalp's uv at its root (a place along the body)
  and its variant. Arnold gets them as user data on the scales' polymesh: `scaleColor` (per
  vertex), `scaleRootColor`, `scaleId` (0-1), `scaleUV` and `scaleVariant` (per face), for any
  surface shader (an `aiUserDataColor` reading `scaleColor` into an `aiStandardSurface`'s base
  color). `material` names the shading group they render with; empty, a mesh prototype's own,
  and the built-in plates a `standard_surface` reading `scaleColor`.
- **Strands:** they only lay the scales out, so neither the viewport nor Arnold shows them
  (`showStrands` on: both do).
- **Viewport:** the scales draw as lit meshes in their own colors (DirectX 11 and OpenGL).
  `preview` `auto` draws flat quads instead past the instances' two-million-triangle budget,
  `quads` always (fast while grooming); renders always take the meshes.
- **Motion blur:** on a deforming body (with a rest mesh) the scales' mesh gets a key of points
  and normals per motion step, in the MtoA translator and the procedural.
- **Exports:** the core's `ScaleMesh` (`engine/core/include/groomlab/scales.h`, Python
  `groomlab_core.build_scale_mesh`) holds every scale's frame, its bent mesh and its data, and
  every export reads it:
  - **Alembic** (`export_alembic`, `scales="mesh"` by default): `<groom>/scales/scalesShape`, an
    animated polymesh, normals and uvs per corner, arbitrary params `scaleColor` (per vertex),
    `scaleRootColor`, `scaleId`, `scaleUV`, `scaleVariant` (per face).
  - **USD** (`export_usd`, `scales="mesh" | "instancer" | "both" | "none"`): a `Mesh` with the
    same primvars, and/or a `PointInstancer` (`scaleInstances`) of the plates (written as meshes
    inside it) with each scale's tilted frame as position, orientation and scale and the data as
    per-instance primvars. Unreal 5's USD Importer turns it into instanced static meshes. An
    instancer loses the bend onto the skin: the mesh is the exact export.
  - **FBX** (`export_scales`, Export > Scales Mesh): a static mesh for Unreal (uv sets map1,
    scaleUV, scaleData; color sets scaleColor, scaleRootColor), from `scale_mesh(groom)`, which
    also leaves the mesh in the scene to skin; `export_unreal_groom` writes it beside the groom.
  - **Baked skin maps** (`bake_scale_maps`, Export > Bake Scale Maps, `groomlab-cli
    bake-scales`): tangent-space normal, 16-bit height and color (with coverage) on the body's
    uvs, 16 to 8192 texels, a tile each for UDIM uvs (`engine/core/include/groomlab/scale_maps.h`).
  - **Groom files** keep the Scales and Instance nodes' own meshes (`"prototypes"`), so the procedural and
    `groomlab-cli` build custom plates without the scene.

### Procedural guides

Strands grown and shaped in the graph can become guide curves, Yeti-style, for anything that
takes guides:
- **`ToGuides`:** turns strands into guides: their core (the strands every preview holds, so the
  guides are the same at every preview density: all of a scatter of under 5,000 strands, or one
  never thinned). `fraction` keeps a stable share of those, chosen by ID, and their channels come
  along as guide attributes (point channels as per-point layers).
- **Uses:** feed them either into Clump's `centers` for procedural clumping, or into Interpolate
  as procedural guides.

The pattern, for secondary clumps:
1. A sparse Scatter is interpolated from the groom's guides, waved with Curl, and turned into
   guides by `ToGuides`. Those are the big clump centers.
2. A second Clump, its `centers` open, picks its own centers from the clumped result (`clumpBy`
   `size`: about how far apart, in scene units) for secondary clumps. (`cinematic_hair` gets its
   two levels from two such Clumps instead.)

### Sculpting generated guides

The guides a graph makes can be brushed in place, as Houdini's guide levels are: sparse hand
guides -> an even `Scatter` + `Interpolate` -> `ToGuides` (dense, evenly spaced guides) ->
**`Sculpt`** -> the next `Interpolate`, a Clump's `centers` and so on.

- **Pick the node:** select the Sculpt node in the Graph Editor, its row **Node: sculpt1** in
  Groom Tools > Sculpt > Correctives, or its menu's **Brush This Node's Guides**. The viewport then
  draws the node's output as the guides, and the shaping brushes (Comb, Smooth, Lift, Length,
  Twist, Curl, Straighten, Attract, Cut, Direction, Noise; symmetry as for the guides) reshape
  them. Place, Draw, Erase, Paint and Part are for the guides themselves: pick the
  Base groom.
- **Kept in the node:** each stroke is one undo step of the groom's `sculptData` (by node name,
  then guide ID); only guides a stroke moved hold an edit. The scene saves it; groom files keep it
  (`"sculpts"`), so renders, the procedural, the CLI and the app see it.
- **Follows the graph:** an edit is the turn of each segment in its own frame and its length
  factor, by normalized arc length, not positions. Comb the sparse guides, change the density,
  add a Noise upstream: the dense guides are made again (with the same IDs, from their roots) and
  the strokes add the same bends on top (a 30 degree hook near the tip stays a 30 degree hook),
  nothing snapping back to where it was. Guides the graph no longer makes keep their edits until
  their ID comes back.
- **Renames, copies, deletion:** the Graph Editor carries the edits with a renamed node, gives a
  duplicate a copy and drops a deleted node's, in the edit's own undo step. **Clear Edits** (the
  node's panel, its menu, or the Correctives row's right-click) drops them.
- **While brushing** only the Sculpt node and what follows it evaluate (its input stays cached);
  the dabs are the CPU's (the GPU preview does not draw Sculpt strokes).

```python
from groomlab import sculpts
sculpts.set_target(groom, "sculpt1")    # brushes shape sculpt1's guides
sculpts.edited_guides(groom, "sculpt1") # how many hold an edit
sculpts.clear_edits(groom, "sculpt1")   # its guides as fed again (one undo step)
sculpts.set_target(groom, None)         # back to the base groom
```

Limits: guides only (not strands); a Sculpt node inside a compound is not brushed from the UI;
sculpting while the groom follows a moving scalp away from its rest pose shows the guides at rest.

### Feathers

Feathers are grown, not modelled (as in Yeti): every strand fed to a **Propagate** node is a
**shaft**, and barbs grow off it. Barbs are ordinary strands, so the nodes after Propagate (Noise,
Curl, Width, HairColor, Clump, Cards...) shape and color them like hair.

- **Shafts from a scatter or from guides.** Any strands are shafts: a Scatter shaped by guides
  through Interpolate (feathers lie the way the guides are combed), or guides placed by hand (a
  Guides node, one feather per guide; Propagate takes guides as they are). Brush, comb and
  shape the shafts like any hair before Propagate.
- **The design.** Barbs grow from `start` to `end` along each shaft, `count` a side
  (`sides`: both, left, right, or `around`: twice the count spiralling round the shaft, for down
  and plumes), `segments` each. Each is `length` times `lengthProfile` long (the ramp along the
  shaft, root to tip, is the vane's outline; `asymmetry` makes the left vane shorter and the
  right longer, as in flight feathers), leans `angle` degrees from the shaft (times
  `angleProfile`), bends `curl` degrees within the vane over its length (times `curlProfile`,
  positive toward the tip), rises `lift` degrees out of the vane at the shaft and bends
  `droop` degrees down over its length (a cupped vane). `splits` gathers neighbouring barbs into
  that many groups per vane, their tips `splitAmount` of the way to the group's middle barb (the
  gaps of a ruffled feather). `lengthRandom` and `angleRandom` vary each barb (seeded by its
  ID); `width` sets the barbs' width (0: the shaft's where the barb grows); `keepShaft` keeps
  the shafts in the output.
- **A down zone.** `down` (0: none) turns the shaft's base, from `start` up to `down` along it,
  into down (an afterfeather: plumulaceous barbs): `downCount` barbs a side (around: twice),
  `downLength` times the vane's length there, `downSegments` segments, curling `downCurl` degrees
  over their length (each between 0.6 and 1.4 times it) and kinked `downFrizz` degrees at every
  segment in and out of the vane, more varied in length and angle than the vane's; the vane's
  barbs then run from `down` to `end`. Down barbs gather into no split (`clumpId`: one of their
  own per vane) and carry `isDown` 1 (the channel exists only with a down zone), which
  FeatherColor's `downColor` reads. Without one the feathers are as before, to the bit.
- **The vane plane.** Barbs spread in the plane of the shaft and the scalp's tangent across it
  at the shaft's root, so feathers lie flat on the body; the plane follows a curving shaft
  (parallel transport). A shaft standing straight along the normal faces world up (+Y, else +Z);
  a curve rooted nowhere faces away from the way it bends.
- **What a barb carries.** An ID hashed from its shaft's ID, side and place (unique, the same
  every run), its shaft's root (so textures, color sets and painted maps read at the root sample
  the feather's base: one color per feather), its shaft's channels (HairColor's colors,
  `guideInfluence`, painted guide maps...; point channels at the point it grows from), and
  `featherId` (a number from the shaft's ID: an edit elsewhere renumbers no feather), `shaftU` (0 root .. 1 tip along the
  shaft), `side` (-1 left, +1 right, 0 around), `barbIndex` (its place along its side),
  `isShaft` (1 on the shafts, 0 on barbs: mask them with an attribute map) and `clumpId` (its
  vane, or its split, numbered from the shaft's ID too: HairColor's `clumpShare` varies color per vane; Cards makes a card per feather, see
  Feather cards), and its place in the feather laid flat (**vane space**, `color_patterns.h`):
  `vaneU` where it leaves the shaft and `vaneTipU`, `vaneTipV` where its tip reaches, u along the
  shaft (0 its root .. 1 the feather's farthest reach toward its tip), v out from it (0 on it ..
  1 the vane's widest reach, either side), as designed (curl and droop included, before splits,
  Noise and the rest move it); a point between sits at its place along the barb. A shaft runs
  up the middle (v 0). FeatherColor's patterns are drawn there. And the vane's normal where
  it grows (`vaneNormalX/Y/Z`), which the hosts take as a feather's strands' normal (their
  shading frames, the iridescence's angle) rather than the scalp's under the root.
  With a `groupId` above 0, every strand the node makes gets that `groupId` (the groom group
  engines split by; `groupName` names it), as `add_feathers` sets per design.
- **The rachis.** `shaftWidth` (0: none) makes each shaft a tube of that width at the root, times
  `shaftWidthProfile` along it (tapering to a quarter by default, never under 5%), with
  `shaftSides` sides (6; 2: a flat ribbon in the vane plane): geometry to skin into the
  character for game engines (Feathers in Unreal, below). With a rachis the barbs start on its
  surface instead of its center line (`rootOnShaft`, on): each barb moves, as designed, to where
  its first segment leaves the tube, so the closest triangle to its root is its own rachis's.
  The feather presets give their designs a rachis width (exposed as `shaftWidth`).
- **Nodes after Propagate.** Shaping nodes (Noise, Curl, Bend, Smooth, Length, Width,
  Resample, Collide, Flyaway, Cull, HairColor, Bands, FeatherColor, Merge, Cards) work on barbs as on any strand; a
  barb's root is where it grows from the shaft (Length cuts, Bend turns and Collide walks from
  there). **Clump** clumps barbs within their own vane: picking its own centers it shares its
  `count` over the vanes, or by `size` puts as many on each vane as fit that far apart along it
  (at least one center each, spread along the vane by barb index), and
  pulls each barb toward the nearest center of its vane (the centers carry `featherId` and
  `side`), never across the shaft; shafts do not clump. Merge numbers b's feathers
  (`featherId`) after a's (from the next block of 2^22), as it does clumps. Nodes that find neighbours by root position
  (BlendNeighbors, Tangle, Braid's region, Interpolate) see every barb of a feather at its
  shaft's root: use them on the shafts, before Propagate.
- **Preview.** The viewport's point budget thins barbs by ID when needed (the render has them
  all); Scatter's preview density thins whole feathers.

| Operator | What it does | Main parameters |
| --- | --- | --- |
| `Propagate` | Grows barbs off every strand or guide it is fed (the shafts): feathers, ferns, plumes. Outputs strands: the shafts (`keepShaft`) and their barbs, with vane space; a down zone at the base | `sides`, `count`, `start`, `end`, `segments`, `length` (mappable, per shaft), `lengthProfile` (ramp), `asymmetry`, `angle` (mappable), `angleProfile`, `curl`, `curlProfile`, `lift`, `droop`, `splits`, `splitAmount`, `lengthRandom`, `angleRandom`, `down`, `downCount`, `downLength`, `downCurl`, `downFrizz`, `downSegments`, `width`, `keepShaft`, `shaftWidth`, `shaftWidthProfile` (ramp), `shaftSides`, `rootOnShaft`, `groupId`, `groupName`, `seed` |
| `Plume` | Lays feathers (Propagate's output, by `featherId`) over one another and off the body (Yeti's Plume): ranks them (`rankBy` `rootDistance` along `direction`, or with none each neighbouring pair against its own mean direction, `length`, `channel` or `random`; `invert`) and turns each up about its root, lowest first, until it lies `offset` above every lower feather under it (each a height map of its vane) and `bodyOffset` above the scalp and the `collider` input. `stiffness` 1 rigid, lower bends; roots stay on the skin; deterministic on any thread count; full evaluation | `offset`, `bodyOffset`, `rankBy`, `direction`, `rankChannel`, `invert`, `iterations`, `stiffness`, `maxAngle`, `depth`, `strength` (mappable, per feather), `seed` |
| `Split` | Routes each strand (or guide: guides in, guides out) to one of four outputs `a`, `b`, `c`, `d`, by their weights' shares: in `random` mode by strand ID (stable: a thinner preview keeps each strand's output), in `map` mode by `select`, a 0-1 value per strand (a painted map, a channel) that picks the share it falls in (weights 1 1 0 0: under 0.5 to a, the rest to b). Every strand goes to exactly one output. Several feather designs on one set of shafts, or any look split by a map | `mode`, `weightA`-`weightD`, `select` (mappable), `seed` |

**Designs and presets.** The presets `feathers_contour` (a rounded vane lying on the body, a few
splits), `feathers_down` (short shafts, fluffy curly barbs all around), `feathers_flight` (a
long, stiff, asymmetric vane) and `feathers_afterfeather` (a contour vane whose base turns to
fluff: a second Propagate grows loose, curly barbs at the base and a third grows barbules on
them, each branch with its own HairColor, joined by Merge) and `feathers_barred` (a contour
feather with a down zone, white bars across its left vane and an iridescent green right one: see
Color patterns) are whole grooms of feathers; each design in them is a Compound
(Propagate and the nodes shaping its barbs) with its main params exposed (`count`, `length`,
`angle`, `splits`, `width`...). Fitted to a scalp, the lengths inside the compound scale too.
`groomlab.add_feathers` puts a design into an existing groom:

```python
groomlab.add_feathers(groom, "contour")                                  # shafts: a Scatter shaped by the guides
groomlab.add_feathers(groom, "flight", source="guides", curves=curves)   # hand-placed: guide group "feathers"
made = groomlab.add_feathers(groom, ["contour", "down"], weights=[3, 1])  # a Split: 3/4 contour, 1/4 down
groomlab.add_feathers(groom, "flight", source=made["split"] + ".c")      # a third design on the Split's c
groomlab.feather_designs()                                               # ["afterfeather", "barred", "contour", ...]
```

It returns the nodes it made (`shafts`, `split`, `designs`, `merge`) and is one undo step. Each
design is a groom group of its own (its Propagate's `groupId` 1, 2, ... and `groupName` the
design's name; the same design added again joins its group; `groomlab.feather_groups(groom)`
lists them), so the Unreal export gives hair and each design their own group. The
feathers are merged with the hair before the output (`keep_hair=False`: they replace it);
`density` sets the scattered shafts' density (default: the design's). Hand-placed shafts' guide
group (`group`, default `feathers`) is left out of the Guides nodes growing the hair. Save your
own design as a preset named `feathers_<name>` (a graph whose first Compound is the design) and
`add_feathers(groom, "<name>")` uses it.

### Color patterns

Two color nodes after a HairColor color more than a tone a strand
(`engine/core/include/groomlab/color_patterns.h`): **Bands** (fur) and **FeatherColor** (feathers).
Each recolors what comes in (HairColor's colors, else the pigment's) and writes the RGB per point,
so the viewport, the app, Arnold (`hair_color`, varying), the cards' Color bake and Unreal's
`groom_color` all show it.

- **Bands along the strand** (Bands: agouti, ticked, shaded fur). `strength` (0-1; mappable,
  per point) mixes the band pigment (`melanin`, `redness`, `dye`, offset per strand by up to
  `randomness`, drawn as HairColor draws its own) in where `profile` says: one period of the
  pattern, root to tip, 0 the incoming color and 1 the band's, repeated `count` times along the
  strand; before the first period and past the last its ends hold, so a single band moves along
  rather than wrapping. Per strand (by ID) the pattern shifts by up to `offsetRandom` of a period
  and stretches by up to `widthRandom`, so bands do not line up across the coat. The bands go over
  the incoming per-point colors, a tip too: keep the profile at 0 near the tip for a dark tip
  past an agouti band. By point index, as `tipProfile` (an even Resample makes that the length).
- **Guard hairs and undercoat.** `guards` (1: all) is the share of strands that are guard hairs:
  they take the bands and keep the tip color; the rest is the undercoat, the incoming strand color
  times `undercoatDye` at every point (no tip color). `guardsBy` `random` picks them by ID; `longest` takes the longest `guards` of
  the strands (by arc length, the length that parts them measured on the core strands, the same at
  every preview density: a pass over every strand, so the node then evaluates in full on a
  stroke, and the GPU look preview leaves drags of the nodes before it to the CPU,
  `look_chain.cpp`).
- **Feather patterns** (FeatherColor's `pattern`), drawn in vane space (Propagate's `vaneU`,
  `vaneTipU`, `vaneTipV`: the feather laid flat, see Feathers), so a bar crosses many barbs, each
  at its own place along the barb. Across the vane, `side x v` is divided by 3 (`kVaneAspect`, a
  typical feather's length over its half-width) before a shape is drawn, so shapes keep their
  proportions on such a feather. `bars` (straight across, tilted by `angle` degrees) and
  `chevrons` (a V mirrored across the shaft, its arms at `angle`): `count` per feather length,
  `width` the share of each period; `spots`: one in every cell of a grid `count` cells a feather
  length, moved within its cell, `width` across; `fringe`: the vane's edge, the last `width` of
  every barb; `eye`: centered on the shaft at `offset`, `width` of the feather's length across
  (`angle` tilts an oval one). `offset` shifts bars, chevrons and spots too; `profile` (a ramp
  from a shape's center to its edge) shapes them inside (rings of an eye spot, a hollow bar),
  `sharpness` their edges. Per feather (from its shaft's root on the scalp, stable)
  `offsetRandom` shifts the pattern by up to that share of its spacing (of its size for an eye),
  `scaleRandom` scales it, `angleRandom` turns it (degrees). `texture`: a painted image of the
  feather, the `texture` param (the whole feather laid flat: the shaft up the middle, the left
  vane on the left, the root at the bottom; read at `s = 0.5 + 0.5 side v`, `t = u`, its alpha
  the cover), or with `textureRight` one image per vane (`s = v`, the shaft along each image's
  left edge). A pattern takes its `melanin`, `redness` and `dye` (an image its own RGB) by
  `strength`, on the `vanes` named (left and right seen from the vane's top, the tip up; the
  shaft and barbs all around count as both). Strands without vane space (hair merged in) take
  none. One image per feather design: the FeatherColor sits inside the design's compound; the
  file's stamp joins the cache key, and Find Missing Files relinks it.
- **The down zone's color.** FeatherColor's `downStrength` mixes `downColor` into the barbs of a
  Propagate's down zone (`isDown`).
- **Iridescence** (structural color, angle-dependent). FeatherColor's `iridescence` (0-1; mappable, per point;
  where: `iridescenceOn` `all`, `leftVane`, `rightVane`, or where the feather pattern is,
  `pattern`, or is not, `notPattern`) writes the per-point `iridescence` channel; the shading
  mixes the fiber's color and its highlight's tint toward `iridescenceColor` with its hue turned
  by `iridescenceShift` turns times `1 - |V . N|` (N: the strand's normal, its vane's on a
  feather, Propagate's `vaneNormalX/Y/Z`, else the scalp's at the root, leaned along the strand as
  the viewport's strand frames are), `iridescentTint`. The viewport's strand
  and card fragments (`groomLabHairColor`, `groomLabCardColor`: the amount in the colors' alpha
  past 1, the view and normal routed through pass-throughs, both APIs), the app's hair shader
  (both shading paths: the per-point colors texture's alpha) and Arnold all shade it; the node's
  color and shift are the first iridescent FeatherColor's making the output
  (`GroomSession::hairLook`). Arnold: `create_hair_shader` builds the approximation the
  translator can drive, a ramp of five keys of the tint on `|Rd . root_normal|` (`aiStateVector`
  Rd, the `root_normal` user data, `aiDot`, `aiAbs`, `aiRampRgb`), mixed into `baseColor` and
  `specularTint` by the `iridescence` user data (`blendColors`); it takes the normal unleaned
  and a linear ramp between the keys (`Rd` is each ray's own direction, reflections' too), so it
  lands within a few hue degrees of the viewport face-on and edge-on and drifts a little between.
  Colors given as RGB (bands, patterns, tips) render lighter and less saturated in Arnold than
  in the app and the viewport: `aiStandardHair` turns its base color into an absorption and adds
  multiple scattering, where the app's hair shader takes the color as the fiber's transmittance.
  The patterns land in the same places; their contrast is lower.

**What each target gets.** Unreal strands: `groom_color` per vertex, `groom_iridescence` (and
`groom_ao` with `iridescence_as_ao`; [docs/UNREAL_HAIR_MATERIAL.md](docs/UNREAL_HAIR_MATERIAL.md)
is the material). Cards (Unreal, Unity, glTF): the patterns baked into the Color map, a feather's
card being its vane laid flat; the iridescence in Unreal's Color alpha and Unity's Mask red,
not in glTF (its `_BaseColor` alpha is the coverage). Unity strands and generic Alembic: `Cd` per
strand, so bands and patterns along a strand do not travel there, nor the iridescence. USD:
`displayColor` per strand, and every point channel (`colorR/G/B`, `iridescence`) as a vertex
primvar.

The presets `agouti_fur` (HairColor -> Bands: banded guard hairs, a black tip, a
plain undercoat) and `feathers_barred` (HairColor -> FeatherColor inside the design:
white bars across the left vane, an iridescent green right vane, a white downy base) put them
together; `feathers_spotted`, `feathers_eye_spot` and
`feathers_iridescent` show the other patterns.

### Maps

Any parameter marked *mappable* can take a map instead of a number. The map's 0–1 value (one
component: `r`, `g`, `b`, `a` or `luminance`) is remapped to `[min, max]`:

```python
groomlab.set_param(groom, "scatter", "density", groomlab.texture_map("C:/maps/density.png", max=300))
groomlab.set_param(groom, "clump", "strength", groomlab.color_set_map("clumpMask"))   # painted on the scalp
groomlab.set_param(groom, "noise", "amplitude", groomlab.attribute_map("frizzy", max=0.05))
```

| Map | Reads | Needs |
| --- | --- | --- |
| `texture_map` | An image at each root's UV. Formats: PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PNM (not EXR/TIFF yet). 8-bit values are used as stored; maps are data, not colors. | Scalp UVs (current UV set) |
| `color_set_map` | A vertex map (Groom Tools > Paint) or a Maya color set on the scalp, interpolated at the root | The map or color set |
| `attribute_map` | A per-strand channel written upstream, e.g. by `Attribute` or guide attributes; a **point channel** of the name (a per-point painted map, HairColor's colors) is read per point instead. Before strands exist (Scatter density) it reads the painted map of that name. | The channel, or a painted map with the guides connected (`set_param` and the editor connect them) |
| `expression_map` | A per-strand expression (see Expressions: a block with locals, names with or without `$`), e.g. `smoothstep(-1, 1, $y)` or `$clumpId > 10 ? 1 : 0.3`; per point with `t`, `pt`, `px py pz`, `pointIndex` or a point channel, e.g. `1 - t` | The channels it names |
| `guide_map` | A guide layer (a map of kind guides, see Painted maps below; "painted map" in the Graph Editor): the strand channel after Interpolate, or the nearest guides blended at the root before it | Guides with the layer; `Scatter` needs its `guides` input |

**Painted maps (Groom Tools > Paint).** A map is a named 0-1 value (gray) or color (r g b a)
painted on the scalp, of one of three kinds, all painted with GroomLab's own Paint brush (no
Maya paint tool, no other package) while the groom follows each stroke:

| Kind | Lives | Paint over | Best for |
| --- | --- | --- | --- |
| **guides** | A layer on the guides (children inherit it, strands blend it from their guides) | the guides | regions, density, clumping; anything that should follow the guides |
| **vertices** | A value per vertex of the scalp mesh, kept with the groom in the scene (`scalpMaps`) | the scalp | detail finer than the guides, with nothing to keep on disk |
| **texture** | A gray (or, for a color map, RGBA) PNG at the scalp's UVs in the scene's masks folder, `groomlab/<scene>/masks/<groom>/` (stored relative to the scene) | the scalp | the finest detail; editing in an image editor; exporting to an engine |

A map **converts** between kinds at any time (**Convert to** in the Paint tab,
`groomlab.convert_map`): it is resampled onto the guides' roots, the vertices or the texels, and
its name stays, so whatever reads it keeps reading it. Rough a mask in on the guides, then turn
it into a 4k texture for Unreal.

All three kinds **survive a new scalp** (Transfer to a new scalp): guide layers ride with the
guides, and vertex and texture maps are projected onto the new mesh by position (a new image
for a texture map; the old file is left as it is).

**Color maps.** A vertex or texture map can hold a color: red, green, blue and alpha (a guide
layer is gray). Make one with **New Map > Values: Color** (`add_map(..., color=(r, g, b))`) or
turn a gray map color (**Make Color** on the map's menu, `set_map_channels(groom, name, 4)`:
the value in r g b, alpha 1; back to gray keeps the luminance). The Paint tab then shows a color
swatch (click to pick) and **R G B A** toggles: the brush paints the color into the ticked
channels only and leaves the others as they are, Ctrl paints toward the inverse color (1 - r,
1 - g, ...) in those channels, as a gray map goes to 1 - value, Shift+drag smooths them, and
**Fill** fills the color there. A color map converts between vertices and texture with its
colors; onto the guides it keeps its luminance (the command warns). Two uses:

- **Packed masks.** Paint up to four masks in one map, one per channel, and read each from its
  output of the map's Map node (`r`, `g`, `b`, `a`): R the clump mask feeding Clump's strength,
  G the frizz mask feeding Noise's fraction, B the length mask feeding Length, all from one node. As a texture, that is one RGBA PNG to
  ship to an engine instead of three gray ones (`groomlab.export_map(groom, "packed", path)`
  writes any map as a PNG). `use_map(groom, "clump", "strength", "packed", channel="r")` and
  `paint_param(..., kind="texture", name="packed", channel="g")` set it up.
- **Painted hair color.** A HairColor node's `map` names a color map: its color at each root
  replaces the pigment's color (or multiplies it, `mode`), so painted color reaches the
  viewport, the cards' Color bake, Unreal's `groom_color`, USD `displayColor`, Alembic `Cd` and
  Arnold's `hair_color`. A name that is no painted map reads the scalp's color set of that name,
  else an image file at the scalp's UVs. The Graph Editor offers the groom's color maps and the
  scalp's color sets there.
- **Painted regions.** An Interpolate node's `regionMap` names a map painted in a color per
  region (red, green, blue...): each strand takes only the guides rooted in its own region, with
  a hard edge between regions, and Clump, BlendNeighbors and Tangle after it stay within regions
  too (see Interpolate keeps to painted regions). Soft paint needs no care: the regions are the
  colors at the guides' roots. The Graph Editor offers the groom's color maps and the scalp's
  color sets for it, as for HairColor's map.

The scalp shows a color map in color; **Show on Scalp**'s channel list (`scalpMapChannel`,
`show_map_on_scalp(groom, name, channel="g")`) shows one channel black to white. Scenes and
files from before keep working as they are: a map without `"channels"` in `scalpMaps` is gray,
and a color PNG read as a gray map is its luminance.

The graph reads a map through a **Map** node (Graph Editor > Add Node > Fields & Curves, or
right-click a mappable param > **Paint** > On Guides / On Scalp Vertices / As Texture): `name`
names the map and nothing else is set on the node: the groom tells the graph where each map lives
and, for a texture, its file, so converting a map changes nothing in the graph; a `name` that is
an image file's path reads that image at the scalp's UVs. `min` and `max` reshape it (min above
max inverts it). **Feed Param** on
the node lists every node's mappable params; pick one and the map feeds it (the param is exposed
as an input and connected; the panel warns while the node feeds nothing). The map stays 0-1
and the param's own value multiplies it: white on a density of 100 is 100, black is 0, and one
map can feed a density, a length and a width at once with each amount on its own param.
**Paint...** on the node opens Groom Tools > Paint on the map.

```python
groomlab.add_map(groom, "fur")                                   # a guide layer, 0 everywhere
groomlab.add_map(groom, "crown", kind="vertices")                # per scalp vertex
groomlab.add_map(groom, "detail", kind="texture", resolution=2048)  # groomlab/<scene>/masks/<groom>/detail.png
groomlab.paint_map(groom, "fur", value=1.0)                      # the Paint brush, the map shown
groomlab.use_map(groom, "scatter", "density", "fur")             # a Map node feeding density (x its 200)
groomlab.use_map(groom, "length", "keep", "fur")                 # the same node feeds Length's keep too
groomlab.paint_param(groom, "noise", "amplitude", kind="vertices")  # map noise_amplitude, white, feeding it; Ctrl lowers
groomlab.fill_map(groom, "fur", 0.5, selected_only=True)
groomlab.convert_map(groom, "fur", "texture", resolution=4096)   # the same map, now a PNG
groomlab.rename_map(groom, "fur", "coat")                        # Map nodes, params and views follow
groomlab.remove_map(groom, "coat")
groomlab.maps(groom)                                             # [{"name", "kind", "texture", "channels"}]
groomlab.add_map(groom, "packed", kind="texture", color=(0, 0, 0))  # a color map (RGBA PNG), black
groomlab.paint_map(groom, "packed", color=(1, 1, 1), channels="r")  # paint the r mask only
groomlab.use_map(groom, "clump", "strength", "packed", channel="r")   # from the Map node's r output
groomlab.use_map(groom, "noise", "fraction", "packed", channel="g")   # the same node's g
groomlab.fill_map(groom, "packed", color=(0, 0, 0, 1), channels="b")  # clear one channel
groomlab.map_channels(groom, "packed")                           # 4 (1 for a gray map)
groomlab.set_map_channels(groom, "fur", 4)                       # gray -> color (not on the guides)
groomlab.set_param(groom, "color", "map", "dye")                 # painted hair color (the HairColor node)
groomlab.export_map(groom, "packed", "C:/engine/packed.png")     # any map as a PNG
groomlab.set_map_texture(groom, "fur", "C:/art/fur.psd")         # read an image: a PNG copy of it
groomlab.reload_map(groom, "fur")                                # after editing it outside Maya
groomlab.missing_files(groom)                                    # {"maps", "params", "attributes"}
groomlab.find_missing_files(groomlab.list_grooms(), "D:/recovered")  # relink by file name
```

**A map's image.** `set_map_texture` (right-click a map > **Relink Image...**) points a map at an
image. A PNG is linked in place, stored relative to the scene, and painting edits it. Any other
format (JPEG, TGA, PSD, BMP, GIF, HDR...) is copied into the masks folder as a PNG
(`masks/<groom>/<map>.png`, a free name when taken) and the map reads the copy: painting writes
PNG, so the source is never painted over (`copy=True` copies a PNG too; a map that names another
format refuses to be painted or filled). A guide layer or vertex map becomes a texture map. The
map keeps its channels: a gray map reads the image's luminance, a color map its r g b a. The
relink is one undo step, and undo removes the copy. `reload_map` (**Reload Image**) makes the
groom read its images again after an edit in another program; files that did not change stay
cached. `missing_files` lists what a groom names that is not there: texture maps, graph params
naming files (texture maps, a HairColor `map` or Map `name` naming an image, FeatherColor's
images, ImportCurves' Alembic file) and the guide cache
and groom file. `find_missing_files` (GroomLab > Utilities > **Find Missing Files...**) relinks
them to files of the same name in a folder and its subfolders, in one undo step: names compare
as the file system does (without case on Windows), the file whose folders match the stored
path's wins when several share a name, and a name that still fits several is reported as
ambiguous, not guessed. The Paint tab marks a map whose image is missing, and the Map node's
panel offers **Relink Image...**.

- **Groom Tools > Paint** has it all: the Paint brush, the list of maps with their kinds (a
  swatch marks color maps), New... (name, kind, gray or color, a texture's resolution), rename,
  **Convert to**, **Make Color** / **Make Gray** and delete (the small buttons over the list, or
  right-click a map), Value and Fill (a gray map) or the color swatch, R G B A and Fill (a color
  map), Fill Selected (guide layers), and Show on guides / strands / scalp, with the channel a
  color map shows ("Read by" names the params the map feeds). **Unfreeze All** is in
  the Freeze tool's settings. Painting a scalp map shows it on the scalp, black to white, and
  the groom updates with every stroke (a texture map's file is rewritten when the stroke ends);
  Ctrl paints toward 1 - value, Shift+drag smooths the map (blends each vertex or texel with
  its neighbors, by the **Smooth** strength under the tabs, in passes past 1;
  `brush_stroke(..., "smooth", attribute=name, paint_kind=..., smooth_strength=2)` by script);
  **Falloff** (every brush) fades the brush toward its edge (1) or paints full strength to it (0;
  `hardness` in scripts); each stroke is one undo step, a texture's file included.
- **No seams in painted maps:** every kind of map is painted on the surface, not in UV space. A
  dab reaches the scalp within its sphere that is connected to where it lands: both sides of a
  UV seam, never across a gap to another part of the scalp (the back of an ear). Smoothing a
  texture map blends each texel with its neighbors on the surface, so at a UV island's edge it
  blends with the texels across the seam in the other island; a vertex map's points split along
  a seam smooth as one. The viewport's live image of a texture stroke pads each island's edge 8
  texels out with its nearest texel (as the saved PNG pads), so filtering shows no dark lines.
- **Density from a map:** a Map node feeding `Scatter` runs before strands exist; a guide layer
  there blends the nearest guides at each root (the node gets the guides connected). Strokes
  then re-scatter, which costs some viewport speed on dense grooms.
- **Errors:** a map no guide carries, a vertex map painted on a scalp with another vertex count,
  or a missing texture file is an error on the Map node, not a silent 0.
- **Other map sources** stay: `guide_map(name)` reads a guide layer straight into a param,
  `color_set_map(name)` a vertex map or a Maya color set of the scalp, `texture_map(path)` any
  image, `attribute_map(name)` a strand channel (`paint_scalp_map` makes a vertex map and an
  Attribute node writing it into a channel of the same name).
- **Commands:** `groomLabScalpMaps -list | -kindOf | -channelsOf | -make <name> -kind <kind>
  [-color [-fillColor r g b a]] | -remove | -rename | -fill (-value v | -fillColor r g b a
  [-fillChannels rgba]) | -convert | -setChannels <name> 1|4 | -values`; the brush's
  `-paintColor r g b a` and `-paintChannels rgba` (stroke command and tool); and `groomLabGuides -listAttributes`, `-removeAttribute <name>`,
  `-renameAttribute <old> <new>` for guide layers.

**Per-point painted maps (along the guides).** A painted map can also hold one value per guide
point, painted along the guide: the Paint brush with `pointwise` set moves each point under it
toward the value by the brush's falloff there (a band along the guide), starting a guide's layer
from its per-guide value where it has none. Children take their parent's layer, resampled along
them, unless they carry their own; mirroring, transfer, rename and delete cover both kinds, and
`-listAttributes` lists both. `Interpolate` blends per-point layers into **point channels** of
the same name on the strands (each guide's layer by arc length, with the same guide weights),
and any `attribute_map` of that name then reads the strands per point (see below). In Python:
`HierarchyGuide.point_attributes`, `GuideHierarchy.set_point_attribute(id, name, values)`,
`Guide.point_attributes`, `BrushSettings.pointwise`.

- **Mappable parameters** include `Scatter.density`, `Clump.strength`, `Noise.amplitude`,
  `Curl.radius`, `Noise.fraction`, `Length.keep`, `Width.width`, `Blend.mix`,
  `HairColor.tipStrength`, `Flyaway.fraction` and `stray`, `Tangle.fraction` and `strength`,
  `HairColor.melanin`, `redness`, `greys`, `specular` and `roughness`, `Bands.strength`,
  `FeatherColor.iridescence`, `RenderData.opacity`, `Attribute.value`.
- **Per-point maps:** a map varies along each strand when it is an `attribute_map` naming a
  **point channel** of the strands (a per-point painted map after Interpolate, HairColor's
  `colorR/G/B`), or an expression using a point variable or point channel, or a field node built
  on one. `Width.width`, `Clump.strength`, `Noise.amplitude`, `Curl.radius`,
  `Blend.mix`, `HairColor.tipStrength`, `Bands.strength` and `FeatherColor.iridescence`
  read such maps per point inside their loops (per strand
  otherwise, at the same cost as before); other params read the root's value.
- **Width:** `width` (scene units, mappable per point) times `profile`, a ramp along the length by
  arc length (default 1 at the root to 0.3 at the tip). `{"expression": "1 - t", "max": 0.02}` on
  `width` with a flat profile tapers every strand from 0.02 to 0 along its arc length.
- **Density maps:** strands are scattered at the map's maximum density and each is kept with
  probability `map / max`, decided by its stable ID. Painting more density adds strands without
  moving existing ones.
- **Caching:** textures are cached and re-read when the file changes on disk; editing a map image
  updates the groom.
- **Expressions:** they compile once and run per strand, or per point when they use a point
  variable or point channel.
  - **Variables:** `u v` (root UV), `x y z` (root position), `nx ny nz` (root normal), `rand`
    (stable per strand), `index`, and any strand channel. The strand's shape: `length`,
    `longest` (the set's longest strand, of its core strands: the same at every preview density;
    `index` is the one variable a thinned preview changes), `reach` (root to tip), `straightness` (reach / length),
    `lift` (1 straight out of the scalp, 0 along it) and `tipx tipy tipz`; they read 0 where
    strands don't exist yet (Scatter density). Along the strand (per point): `t` (0 at the root to
    1 at the tip by arc length), `pt` (the same by point index), `px py pz` (the point's position),
    `pointIndex`, and any point channel by name (a strand channel of the same name wins). Where a
    param reads per strand, the point variables read the root's. A channel named like one of these
    is shadowed.
  - **Language:** SeExpr's, as Maya XGen uses it: `+ - * / % ^`, comparisons, `&& || !`,
    `c ? a : b`, the apply operator `x->f(a)`, `+=` and the like, `if (c) { ... } else { ... }`.
  - **Functions:** SeExpr's builtins (`noise snoise fbm turbulence voronoi cellnoise
    smoothstep(x, a, b) fit remap curve spline pick choose hsi rotate`...): the full list and what
    differs from XGen are on the website's Expressions page (web/content/docs/expressions.mdx).
  - **Errors:** a typo shows on the node with its position (`groomlab.errors`).
  - **The editor:** its map editor has an expression source.

On 24 cores, 100k strands × 16 points through Scatter, Interpolate (300 guides),
Clump (picking its own centers), Noise, Curl and Width evaluates in about 90 ms.

### Groom states

- **Other groom states** use Maya's own tools: hide or show a groom in the Outliner, lock it
  with its `locked` attribute in the Attribute Editor (`groomlab.lock_groom`; a locked groom
  ignores brushes and part strokes), and pick it in the Groom Tools' groom list to keep working on it.

### Graph editor, presets and menu

Loading the plugin adds a **GroomLab** menu:
- **Groom Tools** and **Graph Editor**: the two windows.
- **Create Groom from Selection**.
- **Groom Files** (Import Groom File..., Reload from Groom File, Save to Groom File): grooms to and
  from `.groom` files, the standalone app's too (see Importing and reloading groom files).
- **Simulation**, **Review** (Density Heatmap, Strand Count HUD, Turntable) and **Export**
  (Alembic..., USD..., Unreal Groom..., Groom File..., Render from Groom File..., Render as
  Curves, and RenderMan Procedural while RenderMan for Maya is loaded: see Rendering with RenderMan).
- **Utilities**: guides and strands to and from Maya curves and meshes, baking a node to guides,
  transferring to another scalp, copying the graph to other grooms (see Utilities).
- **Install GroomLab Hotkeys**.

The groom's **Attribute Editor** shows what the groom is made of. **Inputs** comes first:
- **Scalp:** the mesh the hair grows on. **Pick** grows the groom on the selected mesh (a groom
  with guides asks **Closest Point** or **Fit and Move**; see Transfer to a new scalp), and
  **Select** selects the mesh.
- **Rest pose:** only for a scalp that moves (deformers, a cache): the scalp before it deforms,
  where strands are placed so the same strands stay put on every frame. A groom on a deformed
  scalp takes it on its own ("auto"); **Set** and **Clear** are for other cases (see Rest mesh).
  A still scalp doesn't show the row.
- **Guides:** what the groom has, e.g. "119 guides in 1 group"; **Edit** opens the Groom Tools.
- **File:** the groom file the groom is linked to (imported from, saved to); **Reload** brings its
  edits back (one undo step), **Save** writes the groom to it.

**Controls** follows, with the keyable attributes that Control nodes and keyed params add (the
`ctl_` attributes),
then the display and render settings. The button row opens the **Groom Tools** and the **Graph
Editor**. By script:

```python
groomlab.set_scalp(groom, "newHead", fit=False)   # like Pick
groomlab.scalp_mesh(groom); groomlab.rest_mesh(groom)
groomlab.inputs_summary(groom)                    # scalp, rest, guides, groups, curves, sim, cache
```

The **Groom Tools** (first item in the menu; `groomlab.ui.palette.show(tab=...)` with `"tools"`,
`"paint"`, `"sculpt"` or `"guides"`: Tools with the guide groups open) is the grooming hub, docked beside the viewport. Its header
picks what the tools work on: the groom (the selected one, or one picked to stay on it), an eye
to show or hide it, a Graph Editor button and a **...** menu (Create Groom from Selection,
Theme); under it, the **Group** the brushes edit, with **Isolate**. With no groom to work on,
a **Start a groom** card takes its place: select the scalp mesh (and any guide curves) and
click **Create Groom from Selection**. Until the groom has guides and a look, a
**Your first groom** card lists those steps, ticks each off as the groom gets there, and takes
you to the next one on a click (its close button hides it for good). Then four tabs (Tools,
Paint, Sculpt, Reference):
- **Tools:** every tool as an icon, in groups (the core's brush catalog, `brush_catalog.h`):
  Create (Place, Draw, Part, Flow), Shape (Comb, Slide, Smooth, Straighten, Lift, Twist, Curl,
  Attract, Direction, Noise, Tube), Trim (Cut, Length, Erase), Paint (Paint, Width, Density) and
  Guides (Select, Freeze, with the Unfreeze All tile beside Freeze). The tiles reflow to the
  panel's width. Click one and drag in the viewport over the groom.
- **Guide groups & edits** (a section of the Tools tab; click its title to open it): the guide
  groups, each row with a visibility checkbox, its name (double-click to rename; graph params
  naming it follow), a color swatch (click to give the group its own viewport color,
  `groomlab.set_guide_group_color`; right-click to clear), its guide count and level; the name's
  tooltip lists the graph nodes naming it. **+ New Group** and the small buttons over the list
  make a new group, rename, select the group's guides and delete; right-click a row for the
  same and Color. **Hue per group** colors groups without their own color by hue. Clicking a
  group makes it the brush group. **Segments** (a slider under it) shows the selected guides' point count, or every guide's with none
  selected, resamples them live while dragged and applies on release (one undo step). **Edit Guides** holds Children..., Sync +X → -X, Sync -X → +X, Mirror
  Selection, Mirror Group..., Reparent..., Promote / Demote..., From Curves and Clear Selection.
- **Paint:** the Paint brush and the maps of every kind (see Painted maps above).
- **Sculpt:** Correctives (see Correctives below).
- **Reference:** photo cameras to groom to (see Grooming from reference).
- **Settings:** below the tabs, the active tool's name, what it does, and one card with only
  what that tool uses: radius, strength (signed for tools that work both ways), falloff, the
  Falloff curve and Root to tip curves, symmetry (world X or topology), selected only, pen pressure, keep off scalp, mask by scalp (on: a stroke never reaches what the head hides from the camera, such as guides, parts, flow arrows or paint on its far side), and the tool's own options: New guides
  (Place and Draw: length, segments, spacing and neighbor matching), Solo new guides (Place and
  Draw), Parts (name, the list,
  **From Curves**, Remove, Clear All), Flow (**Place** arrows or a sink, **Clear All**), Stretch
  (Comb), Erases (Guides, Parts, Flow), Acts on (Guides, Parts, Flow for the shaping brushes),
  Selection (**Soft radius**) and Freeze mask (**Unfreeze All**). **While sculpting** (folded
  away under the card) sets how the strands draw during a stroke.
- **Group:** the guide group brushes edit and Place and Draw add new guides to; children follow
  their parents. **All groups** brushes every guide. **New Group...** makes a group, top-level
  or a child of another (`groomlab.add_guide_group(groom, name, parent=None)`); a **Guides**
  node in the Graph Editor reads it by name.
- **Staying in sync:** radius, strength and group changed in the viewport (middle-drag,
  B/M-drag, Shift+click a guide to solo it and take its group) show up here.
- **Icons:** the SVG icons live in the module's `icons` folder, on Maya's icon path for shelves
  too.

The **Graph Editor** is a dockable node editor for the selected groom:
- **Nodes:** each is a card with a category color, its name and type, and ports colored by
  type. The categories follow the order a groom is built in (the Add Node menu): Sources (Scalp,
  Guides, ColliderMesh, ImportCurves, Reference), Grow (Scatter, Interpolate, Cull), Shape (Length, Width,
  Resample, Direction, Bend, Transform, Flow, Part, Tie, Smooth, BlendNeighbors, Gravity,
  Displace, Mirror, Collide, SelfCollide, MeshCut, RestoreLength, Sculpt), Look (Clump, Twist, Braid, Noise, Curl,
  Flyaway, Static, Tangle, Wet, Instance, Scales), Motion (Wind, Jiggle, Simulate), Feathers (Propagate, Plume,
  Split), Color (HairColor, Bands, FeatherColor, RenderData), Convert (ToGuides, ToStrands), Fields &
  Curves, Utility (Group, Blend, Attribute, Merge, Switch, Output) and Games (Cards). A port is hollow when free and filled
  when linked. A short summary shows the params that differ from their defaults.
- **Wiring:** drag from an output port to an input port to connect.
  - **Snapping:** compatible ports light up while you drag, and a drop near one snaps to it.
  - **Moving a link:** drag a linked input to move its link elsewhere, or drop it on empty space
    to remove it.
  - **Refused links:** mismatched types and cycles are refused, with the reason in the status
    bar.
- **Toolbar:** the groom, **Add Node**, **Presets** and **Live** (with **Apply** while Live is
  off); the **...** menu holds Undo and Redo, Frame, Tidy Layout, Group into Compound (Ctrl+G), Ungroup
  Compound (Ctrl+Shift+G; also on a Compound's right-click menu and in its panel), Delete, Copy
  Graph and Paste Graph (also in the Presets menu: see Sharing a look as text), Units, Theme and
  Reload from Groom.
- **Nodes:** Add Node inserts after the selected node. Delete (the key) removes the selection.
  Tidy Layout re-arranges the graph. The canvas is open: pan (middle-drag or Alt+drag) and zoom (wheel) anywhere, past the nodes. A graph opens at a readable size: all of it when it fits, else Interpolate to Output (where look nodes go), or the output end. **Frame** (F) fits the selected nodes, or the whole graph; A fits everything.
- **Parameters:** the side panel edits the selected node, under its name, its type (in its
  category's color) and a line on what it does. Params read as words ("Guide radius"); the
  tooltip gives the param's name for scripts.
  - **Numbers:** each has a slider and a field. The slider covers a soft range guessed from the
    default: 0–1 for fractions, or a round number above larger values.
  - **Stretching the range:** type a value outside the range and it stretches to reach it.
    Right-click the slider to fit, set or reset the range. Stretched ranges are remembered per
    operator parameter across sessions.
  - **Dragging:** the groom updates live while you drag; releasing makes one undo step.
  - **Keys:** right-click a param > **Set Key**, or **S** with the mouse over it, keys it at the
    current frame. The first key links it to its own keyable attribute on the groom,
    `ctl_<node>_<param>` (the graph's `paramControls`; three channels for a vector, a color
    attribute for a color, a long with stepped keys for a switch), in one undo step, with no node
    on the canvas. Its field takes the Channel Box's colors (red: a key on this frame, pink:
    animated, yellow: driven by a Control node or a connection) and shows the value on each
    frame. An edit sets a key with Auto Key on, else the value until the time changes. **Delete
    Key**, **Remove Animation** (the value now back into the param) and **Select Attribute** are
    in the same menu. Params that set counts, segments, card topology or seeds can't be keyed
    (the schema's `animatable`: false). `groomlab.set_param_key`, `delete_param_key`,
    `remove_param_animation`, `param_key_state` by script.
  - **Mappable parameters:** the **Map** button beside the value switches it to a texture, a
    color set, a strand attribute, an expression or a **painted map** (a guide layer), and back. Right-click the name for
    **Paint** > On Guides / On Scalp Vertices / As Texture: a map of that kind, white, feeds the
    param through a Map node (white = the value now, black = 0), and Groom Tools > Paint opens on
    it (the brush lowers the param, Ctrl raises it).
  - **Node buttons:** nodes that need something from the scene have it under their params:
    **Cards** (Show cards, Show textures, LOD, Bake Maps, Bake Scalp, Export for Unreal), **Braid** (Paths from Selected Curves),
    **HairColor** (Create Hair Shader) and **Instance** (Prototype from Selection).
  - **Inputs:** the **Scalp** node shows the mesh it reads, with **Pick from Selection** (grow
    the groom on the selected mesh) and **Select**, and, for a moving scalp, the rest pose with
    **Set** and **Clear**. The **Guides** node shows the guides the groom has, with **Groups...** (the
    Groom Tools' Guide groups & edits). Plumbing params (the Scalp input, the Guides input and hierarchy)
    stay hidden unless changed.
- **Adding nodes:** press Tab or right-click the canvas to search. Type part of a name
  or of what it does ("remap curve"), then Enter adds it there.
- **Header toggles:** every node header has two, as in Yeti. **Focus** (eye) shows the groom
  up to that node in the viewport, and **Disable** (power) passes the node's input straight
  through; the node dims. Focus on a **Clump** draws its clump centers as dots on the scalp over
  its result (in the theme's `clumpCenters` color).
- **Live dragging:** every slider in the side panel (params and an Expression node's variables)
  shows the groom change while you drag; letting go makes one undo step.
- **Real units:** lengths, widths and densities show in real units (**...** > Units):
  lengths in cm by default (mm, m, in), widths in µm (mm, cm), density per cm² (mm², in²). Groom
  data stays in scene units, which are centimeters (Maya's API works in centimeters whatever the
  UI unit), so nothing is rescaled; map min/max and the Groom Tools' new-guide length follow too.
  `groomlab.units.set_unit("width", "um")` by script.
- **Curves:** ramp params (Clump profile, FieldRemap's curve and others) get a curve editor.
  - **Editing:** drag keys, click to add one, right-click a key to remove it, or right-click
    elsewhere for presets (rising, bell, tip only, flip and more).
  - **Sharing a curve:** expose a ramp param as an input and plug a **Curve** node into it, so
    one curve drives several nodes. The Curve node adds `interpolation` (smooth, ease or linear), `flip`, `min` and
    `max` (output = min + (max - min) x curve).
  - **Reshaping values:** a Curve plugged into FieldRemap's curve reshapes any field (noise, a
    texture, a painted map).
- **Keys on the selection:** Ctrl+C / Ctrl+V / Ctrl+D copy, paste and duplicate nodes,
  keeping the links between them.
  - **V:** views a node; the viewport shows its strands, while renders and exports still
    use the output (`viewNode`).
  - **B:** bypasses a node; it passes its input through, for a with/without comparison.
- **Node feedback:** a node that fails shows a red badge with the error in its tooltip. Each
  node shows how long it took the last time it ran (`groomLabGraphStats`). A warning badge
  flags a Cards node whose baked maps are stale, and a node with a note (a Jiggle with no scalp
  motion to react to). `cmds.groomLabGraphStats(groom, points=True)` (`groomlab.viewport_points`)
  returns the points of the strands the viewport shows, to compare exports with.
- **Backdrops:** right-click a node and choose Add Backdrop Around Selection to frame nodes.
  Moving the backdrop moves them, and double-click renames it. Backdrops are layout only.
- **Sliders:** right-click one for Reset to Default.
- **Applying:** with **Live** on, each valid edit applies as one undo step. With it off, press
  Apply. An invalid graph (a missing input or no Output) is shown but never applied.
- **Layout storage:** node positions live in the groom's `graphUi` attribute, so moving nodes
  never re-evaluates the groom.

**Presets** are graph JSON files. The Graph Editor's **Presets** button lists them in a submenu
per category, each with its description as the tooltip; pick one to load it (with its layout
and backdrops), or save the graph (**Save as Preset...**); there is no preset item in the
GroomLab menu. They are found in `GROOMLAB_PRESET_PATH`, then the user folder (`<Maya app
dir>/groomlab/presets`, or `GROOMLAB_USER_PRESETS`), then the ones shipped with the module. Your
own presets (no category) sit at the menu's top level.

**Sharing a look as text.** **Presets** > **Copy Graph** (also in the **...** menu) puts the
groom's whole graph on the clipboard as JSON text, for a chat, a forum or a file:

- **The text is a preset:** what Save as Preset writes (`version`, `size`, `about` titled after
  the groom in the "Shared" category, `nodes`, `connections`, `output`, `backdrops`), plus
  `controls`: the values of the groom's `ctl_` attributes (`{"ctl_curls": 0.7, "ctl_color_dye":
  [0.5, 0.4, 0.3]}`). Keys do not travel. Saved as a `.json` file in a presets folder, it loads
  like any preset, its control values too.
- **Insert after selected node** (each preset's submenu, beside **Apply**) keeps the groom's graph
  and adds the preset's look to it: every node between the preset's sources and its Output becomes
  one Compound (named after the preset, fitted to this scalp) placed after the selected node, or
  before the Output when nothing is selected, fed by it and feeding what it fed; one undo step.
  The **Add-ons** presets (`wet`, `frizz`, `flyaways`, `cards_mid`, `gravity`, `two_tone_tips`) are small looks made
  for inserting; any preset inserts the same way (`groomlab_core.insert_preset_json`, the core's
  `insertPresetJson`). The Compound exposes the look's key params (the preset's `"exposed"`
  list, below), each with its inner param's range, unit and group, so it is tweaked, keyed and
  saved as one node.
- **Paste Graph** replaces the shown groom's graph with the clipboard's: lengths, widths,
  densities and keyed lengths fitted to this scalp, the layout and the control values set, one
  undo step. A ```` ```json ```` code block around the text is fine.
- **What did not travel** is listed in the status bar: painted maps, color sets, files (textures,
  Alembic paths) that do not exist here, Instance prototypes, colliders, guide groups and parts.
  Nodes reading them use their defaults until they exist.
- **Safety:** the text is parsed as JSON only and checked against the operators (unknown node
  types, params and links are refused, nothing changes, and the reason shows in the status bar).
  Text over 4 MB or graphs over 2,000 nodes are refused. File paths on other machines
  (`\\host\share`) are left out, so a pasted graph never makes Maya reach a stranger's server.

The shipped presets are a small core set of starting points and lessons: each is one good-looking
example of one technique, in a small graph whose node names say what they do. The hair presets are
made on a 20 cm head, the fur presets on a wolf-sized body (100 cm), the feather designs on a
2-unit bird; all fit any scalp (see Presets fit the scalp, under Flow arrows). Their clumps are
sized in scene units (Clump's `size`, with `clumpBy` size, fitted like any length), so a whole body
gets more clumps of the same size than a head, not the same number stretched over it.

| Category | Preset | What it shows |
| --- | --- | --- |
| Hair | `hair_basics` | The chain every groom builds on: Scatter, Interpolate, Clump (picking its own centers by `size`), a little Noise, HairColor, Width, Resample |
| Hair | `cinematic_hair` | The hero showcase: big locks and finer clumps inside them (two Clump levels picking their own centers by `size`, the strands waved by a Noise before each Clump: `lockWave`, `clumpWave`), restrained noise, frizz on a few strands near the tips (a Noise named `frizzNoise`), a few flyaways, uneven lengths and widths, color variation |
| Hair | `curly` | Ringlets: small clumps curled together by Curl with `coherence` `clump`, a little frizz, the segments curls need |
| Hair | `braids` | Box braids with no curves to draw: the Braid node picks its own paths, one per 2.6 cm of scalp (`region`, exposed as `braidSpacing`) |
| Hair | `parted_style` | A part drawn with the Part tool: Interpolate and Clump respect it, the Part node opens it |
| Fur | `creature_fur` | Fur for a whole animal: laid by flow arrows (Flow, no guides), soft tufts about a centimetre across, lighter tips |
| Fur | `agouti_fur` | The creature fur in wild-type banding: guard hairs dark, a pale band, a black tip, the bands wandering hair to hair over a plain undercoat (HairColor, then Bands with guard hairs) |
| Fur | `white_wool` | Dense, crimped white fleece in soft locks: a picked off-white (HairColor's `color` mode, so no pigment browns it), a little brightness variation, white tips |
| Fur | `flow_with_guides` | Flow everywhere, guides only for accents (Interpolate's `limitDistance`, the `guideInfluence` channel) |
| Feathers | `layered_feathers` | The contour design's feathers laid over one another like roof tiles and off the body (Plume) |
| Feathers | `feathers_contour`, `feathers_down`, `feathers_flight`, `feathers_afterfeather`, `feathers_barred` | The Feathers tool's designs, as Compounds (see Feathers; `feathers_barred`: Color patterns) |
| Feathers | `feathers_spotted`, `feathers_eye_spot`, `feathers_iridescent` | The barred design's other color patterns: pale spots on both vanes, a ringed eye spot that shimmers blue, a dark feather shimmering green to blue (stylized; see Color patterns) |
| Scales | `dragon_scales` | Scales laid out by strands: a Bend combs them down the body, Transform (`scale`, `scaleRandom`) and Width size them (bigger on the back), HairColor colors them (darker tips), a Scales node lays a built-in keeled plate on each root (shields on the belly), tiled and bent onto the skin |
| Scales | `scales_from_guides` | Scales pointed by guides (Interpolate): comb or brush the guides and the scales turn with them; a Bend lays them down the body until there are guides |
| Scales | `scales_from_flow` | Scales pointed by Flow, no guides: draw flow arrows and the scales follow |
| Scales | `swept_spikes` | Long, narrow plates in `follow` `strand`: spikes or quills based on the skin that bend along their guides' curves |
| Motion | `everyday_sim` | Shoulder-length hair whose guides go through Simulate (calm settings: some bend, damping, a soft root hold) before Interpolate |
| Motion | `wind_and_jiggle` | Wind and Jiggle after the look and before Collide: motion with no cache |
| Techniques | `expression_driven` | Expression nodes: longer hair at the crown with a keyable variable, harder clumps on long strands |
| Techniques | `painted_maps` | One color map packing three masks: R clumps, G frizz, B length (the map is made on apply) |
| Techniques | `blend_two_looks` | A Blend of two looks of the same strands, its `mix` driven by a Control (the keyable `ctl_curls`) |
| Techniques | `merge_layers` | Layers, compounds and switches: a sparse, longer layer grown from its own Scatter and waved by a Compound with exposed params (`wavyFrizz`), merged onto the base; a Switch (`pickLook`) picks the full look or a light one (a third of the base) |
| Techniques | `vine_leaves` | The Instance node: leaves bent along curling vines (`deform` `alongStrand`; stylized) |
| Games | `game_cards` | A Cards node at the mid level of detail, ready to bake and export |
| Add-ons | `wet` | For inserting: a Wet node (small clumps near the root merging into bigger ones toward the tip) and a darker color, as hair just out of the water |
| Add-ons | `frizz` | For inserting: kinky noise on a share of the strands, growing toward the tips |
| Add-ons | `flyaways` | For inserting: a few strands leave the flow near the tip and stand out |
| Add-ons | `cards_mid` | For inserting before the Output: a Cards node at the mid level of detail |
| Add-ons | `gravity` | For inserting: strands hang under their weight, stiff at the root (a Collide after it keeps them off the scalp) |
| Add-ons | `two_tone_tips` | For inserting: a picked root color (a skin tone) blending to white tips, each strand a little lighter or darker and its tip starting at its own place |

A preset file is a graph plus a few optional fields of its own, none of which reach the groom's
graph:

```json
{
  "version": 1,
  "size": 20.0,
  "about": {"title": "Painted maps", "category": "Techniques",
            "description": "One plain sentence: what the look is."},
  "requires": {"maps": [{"name": "groomMasks", "kind": "vertices", "color": [1.0, 0.1, 1.0]}]},
  "exposed": [{"node": "clump", "param": "strength", "name": "clumpStrength"}, {"node": "width", "param": "width"}],
  "nodes": [...], "connections": [...], "output": "output",
  "backdrops": [{"title": "a: sleek", "x": 944, "y": -40, "w": 240, "h": 168, "color": [70, 90, 120]}]
}
```

`size` is the scalp size it was made on (for fitting); `about` is what the menu shows (the
categories, in order: `Hair`, `Fur`, `Feathers`, `Scales`, `Motion`, `Techniques`, `Games`, `Add-ons`); `requires` lists
painted maps the graph reads, made as `add_map` makes them when the groom has none of that name;
`exposed` lists the params the Compound shows when the preset is inserted (Insert after selected
node): a look node and its param each, `name` optional (the param's name; node and param,
`clumpStrength`, when the Compound has that name already), entries naming a node outside the
look skipped. Without the list a default pick shows: one param per look node for up to six,
the nodes of the types Clump, Wet, Curl, Noise, Braid, Tangle, Flyaway, Gravity, Bend, Length,
HairColor and Width first, each node's first float or int param that is not advanced and holds
a plain number. Every shipped preset with a look lists its own (1 to 8);
`backdrops` frame parts of the graph in the editor. `presets.check(preset)` lists what does not
fit the operators (unknown node types or params, values of the wrong kind or not among a param's
choices, links to ports that do not exist): the plugin ignores unknown params, so a test runs it
on every shipped preset.

The same editing is available from Python:

```python
from groomlab import graph, presets

model = graph.GraphModel.from_groom(groom)
model.add_node("Curl", name="curl")
model.insert_between("length", "width", "curl")
model.set_param("curl", "radius", 0.08)
model.save(groom)                  # validates, then writes the graph (undoable)

presets.save_preset(groom, "my_look", about={"title": "My look", "category": "Hair",
                                             "description": "Wavy, for the lead."})
presets.apply_preset(other_groom, "my_look")   # the graph, its layout and the maps it reads: one undo step
presets.list_presets(details=True)            # [{"name", "title", "description", "category", "path"}]
presets.check(presets.read_preset("my_look")) # [] when every node and param fits the operators

text = groomlab.copy_graph_text(groom)        # Copy Graph: the look as preset JSON, with control values
report = groomlab.paste_graph_text(other_groom, text)  # Paste Graph: checked, fitted, one undo step
report["missing"]   # {"maps", "colorSets", "files", "meshes", "colliders", "groups", "parts"} this groom lacks
report["message"]   # the line the Graph Editor's status bar shows
```

`paste_graph_text` raises `ValueError` (nothing changed) for text that is not a GroomLab graph.
Its report also holds `"title"`, `"nodes"`, `"fit"` (the sizes fitted from and to, or None),
`"controls"` (the control attributes set), `"controlsSkipped"` (and why: keyed on this groom,
say) and `"removed"` (params naming files on other machines, left out).

`graph.OperatorCatalog.from_maya()` lists every operator with its inputs, outputs and parameter
specs (the `groomLabOperators` command returns the same as JSON: `"outputs"` lists every output
port, `"output"` is the first).

**Compound nodes.** A compound packages part of a graph as one node. Select nodes and press
Group in the editor, or in Python:

```python
name = model.collapse(["noise", "curl"])           # one Compound node in their place
model.expose(name, "amount", "noise.amplitude")    # an inner param on the compound
model.set_param(name, "amount", 0.08)
model.expand(name)                                 # back to the nodes, exposed values kept
```

- **Inputs and output:** sources outside the group become the compound's inputs (`Input` nodes
  inside; one per source node and output it is read from). The one node feeding the rest of the
  graph is its output: a compound has one output, that node's first (links inside it may read
  any output of an inner node).
- **Storage:** the definition is stored readably in the graph JSON, and compounds nest.
- **Caching:** the outer graph caches a compound as one node.
- **Reuse:** presets carry compounds, so a compound saved in a preset is reusable across grooms.
- **Repeat:** `repeat` (1-64) runs the subgraph that many passes, each pass taking the previous
  pass's output as the compound's first strands input (the other inputs stay): a `Noise ->
  Smooth` compound repeated 4 times builds up like an iterative relax. An inner `Input` node
  named `iteration` of type `Field` yields the pass index (0, 1, ...) as a field, so a param
  exposed as an input (through a `FieldRemap`, say) can vary from pass to pass. With `repeat` 1
  the compound is what it was.

### Viewport look

The viewport can color strands and draw them as ribbons. Neither re-evaluates the groom:

| `strandColorMode` | Colors strands by |
| --- | --- |
| `solid` | One hair color (the default) |
| `colorSet` | The scalp color set `strandColorSource` at each root |
| `attribute` | The channel `strandColorSource` over [`strandColorMin`, `strandColorMax`], blue to red |
| `random` | A random color per strand |
| `clump` | A random color per value of the channel `strandColorSource` (default `clumpId`), one per clump; with `region`, one per painted region (Interpolate's `regionMap`) |
| `density` | Root density against the groom's median: blue a quarter or less, green typical, red 4x or more (bald spots, bunched roots) |

`displayRibbons` draws strands as ribbons as wide as their `Width`, times `ribbonWidthScale`, for
width and density look checks.

`displayWidthCoverage` (on by default) draws the strand lines with their widths too: they stay a
pixel wide, and each point covers the share of the pixel its width (times `ribbonWidthScale`)
covers at its distance from the camera, so tapered tips fade, thin strands read lighter, and
strands a pixel wide or more stay solid: hair as dense as it renders, not as dense as a pixel-wide
line of every strand would make it. The coverage is drawn stochastically (hashed alpha testing):
each strand's pixel is drawn whole or not by a threshold of that strand's own, so the many strands
behind a pixel of a groom seen from afar add up as they do in the render instead of the front one
hiding the rest, and the pattern stays put on the strands (the same frame draws the same picture;
the camera moving carries it along). With the viewport's multisample anti-aliasing on, each strand
covers its share of a pixel's samples instead, which reads smooth; without it, strands thinner than
a tenth of a pixel stipple, and those covering much of a pixel (seen close) blend smooth. Strands
without widths (no Width node) draw opaque. Flat, colored and lit lines alike, on DirectX 11 and
OpenGL. `displayCoverageDensity` (on by default) takes a thinned preview to the render's density:
each strand a preview at `previewDensity` 0.1 shows stands for ten, and covers what ten would
together (never more than its pixel), so the preview reads as dense as the render (the whole groom
at the preview density's share: a source the preview never thins, imported curves or a Scatter with
`previewAll`, reads denser than it renders; a point budget's further thinning, lighter). Off,
every strand draws opaque, as before. In the Attribute Editor under Display > Strand Shading.

In `solid` mode strands and ribbons are lit (`displayLighting`, on by default) with GroomLab's own
hair shading (`displayHairShading` GroomLab, the default): Marschner's three lobes (the surface
highlight, the light through the fiber, the colored highlight off its back) from the strands'
pigment and the HairColor node's specular, roughness and tilt, as Arnold's `aiStandardHair` and the
standalone app show them, lit by the viewport's lights (directional, point, spot and area lights,
ambient lights; their color, intensity, decay and cone; light linking through the groom's shading
group or a light not lighting everything). The hair shadows itself (`displaySelfShadowing`, 1: as
opaque as rendered; 0 off): the GPU keeps a grid of its density, written again whenever the strands
move, and each strand is lit through the hair between it and each light, with the light scattered
on through that hair. Each strand shades a little apart from its neighbors
(`displayStrandVariation`: its highlight's tilt, width and strength and its pigment, from its ID;
0 none), on top of HairColor's specular and roughness randomness. The viewport's surfaces show a
light's intensity times their color, so the hair is drawn at pi times the render's radiance to sit
with them. `displayHairShading` Maya draws them as before with Maya's hair tube shading: Lambert
diffuse plus highlights along the strand, with screen-space ambient occlusion when it is on. With
`useMaterialColor` on (the default) and no material, or the `aiStandardHair` `create_hair_shader`
made from the groom, each strand shows its own color, as the render does: HairColor's map (a
painted map, a color set or an image), randomness and tip color, and Bands' and FeatherColor's
colors, per point along the strands where they vary (strands without color channels: `displayColor`). Another assigned material colors
every strand alike (an `aiStandardHair`'s `melanin` and `melaninRedness` approximated, or a
`baseColor` or `color`); with `useMaterialColor` off they all take `displayColor`.

| `displayShadows` | Strands |
| --- | --- |
| `Off` | Take no part in shadows |
| `Cast` | Cast shadows on the scalp, body and floor (the default) |
| `Cast and Receive` | Also shadow each other; dense strands speckle in a shadow map (Maya's hair shading; GroomLab's takes its own, `displaySelfShadowing`) |

The color modes stay flat: they are for reading data. At 200k preview strands the viewport orbits
at about 200 fps as lit lines and about 65 fps as lit ribbons with self-shadowing and ambient
occlusion, measured by the GUI viewport test on an RTX 3090.

**Guides.** With `themeGuideColors` (on by default) guides take the theme's colors, one per
hierarchy level (primaries blue, children emerald, then yellow, red, white in the Playful theme);
off, primaries use `guideColor` and each level down turns a little round the hue wheel.
`guideColorMode` colors them by `level`, by a painted map (`layer`) or by `group`. `guidePoints`
draws round dots on each guide, in the guide's own color: `all` (the default), `roots and
tips`, or `off`. With `all`, `guideDisplaySegments` (6 by default) spaces the dots evenly
along the curve as drawn, like beads, whatever point count the guide has; 0 puts a dot on
every point it has. Dots are `guideWidth` plus the theme's `pointSize` across, keep their size
on screen and hide behind the head; on a groom with many guides they thin to about 2,500
(roots and tips always). To rebuild the guides themselves to a point count, use
the **Segments** slider (Groom Tools > Tools, Guide groups & edits) or `groomlab.resample_guides` (points spaced along
the smooth curve through the old ones, so bends stay round).

### Themes

GroomLab's look comes from a theme: the Groom Tools and Graph Editor (colors, rounding, node
style, slider colors) and the viewport's guide, dot, selection, part and flow colors. Pick one from
the **Theme** menu (in the **...** menu of the Groom Tools or of the Graph Editor); the
choice is remembered.

- **Playful** (the default): charcoal with red, yellow, blue, emerald and white; flat, round,
  nodes with solid colored headers; tool icons emerald (create), blue (shape), red (trim) and
  yellow (paint).
- **Studio**: low-key greys with one quiet accent; small corners, tinted nodes, muted guides.
- **Polar**: near-black and monochrome after polar.sh (Night, Ash, Mist and Snow) with Ether
  indigo as the one accent; soft corners, tinted nodes, grey tool icons.
- **Your own:** **Theme > New Theme from Current...** copies the active theme into your themes
  folder (`<Maya user app dir>/groomlab/themes`) and opens it; edit the JSON, then **Theme >
  Reload Theme**. A theme may set only what it changes and `"extends"` another (default
  `playful`). Colors are hex (`"#2F7BF6"`), a name from its `palette` (`"blue"`), or `"accent"`.
  `GROOMLAB_THEME_PATH` adds folders (a studio's shared themes).

```json
{
  "name": "Night Owl",
  "extends": "playful",
  "palette": {"blue": "#7C5CFF"},
  "ui": {"background": "#141418", "radius": 16},
  "graph": {"headerStyle": "filled", "categories": {"Look": "red"}},
  "viewport": {"guideLevels": ["blue", "white"], "pointSize": 7}
}
```

The keys: `palette` (named colors); `ui` (`background`, `panel`, `raised`, `hover`, `border`,
`text`, `textDim`, `accent`, `accentText`, `warning`, `radius`, `fontSize`, `sliders`: the colors
sliders cycle through); `graph` (`canvas`, `grid`, `node`, `nodeBorder`, `text`, `textDim`,
`selected`, `cornerRadius`, `headerStyle` `filled` or `tinted`, `shadow`, `categories` by node
category, `ports` by port type); `icons` (a color per tool group: `create`, `shape`, `trim`, `paint`, and `app` for the window icons; `ink` for hair and neutral parts; a shade and a tint of each come with it, and a checked tool's icon turns to `accentText` on the accent); `viewport` (`guideLevels`, `pointSize`,
`selected`, `softSelected`, `frozen`, `parts`, `flow`, `flowField`, `tubes`, `instances`).

```python
from groomlab.ui import theme
theme.list_themes(); theme.set_theme("studio"); theme.create_theme("mine"); theme.reload()
```

### Flow arrows (guideless short hair)

For vellus, short fur, brows and other short hair, groom a direction field instead of guides.
With the **Flow** tool (Groom Tools > Tools, or `groomlab.flow_tool(groom)`), drag on the scalp:
arrows go down along the stroke every half a brush radius, and arrows the brush passes over turn
toward the stroke; Ctrl+drag erases. Between arrows the field blends smoothly (the nearest
arrows, inverse-distance weighted, carried onto the surface), so a few strokes make partings
and sweeps.

**Sinks** are points hair turns around: set the Flow box's **Place** to Whorl, Whorl
(clockwise), Gather or Part out and click on the scalp. Each sink is the size of the brush;
within it hair spirals out (a crown whorl), gathers in, or points away (a cowlick), fully near the
center and fading into the arrows at its edge. Ctrl-click removes sinks; symmetry places a
mirrored twin (a whorl's mirror turns the other way). Sinks draw as a ring with streamlines, and
alone (no arrows) they make a field too.

The **Flow** node (Shape) lays strands along the field: each leaves its root `lift` degrees off
the skin, heads the way the field points there and bends toward the skin along its length
(`bend`), turned at random up to `rollRandom` degrees around the normal; the common `affect` blends from the incoming shape. Without
arrows, strands lie along their own heading. `Scatter -> Length -> Flow -> Width` grooms fur with
no guides at all: the `creature_fur` preset is a starting point.

**Guides in a flow groom.** Put an `Interpolate` after Flow and give it a `limitDistance` (its
Limit group, with `falloff` and `influenceChannel`, which do nothing at 0; the
`flow_with_guides` preset does): each guide then only takes the strands within that distance of
its root, blending by the `falloff` curve (full at the guide, none at the edge; edit it in the
Graph Editor), and everything farther keeps the flow. Place or Draw guides where you want a
tuft, a cowlick or longer hair, in their own group if you like (e.g. `accents`). `take` "length"
keeps the flow's direction and only brings nearby strands to the guides' length. The influence
(0-1) is written to the `guideInfluence` channel (`influenceChannel`), so later nodes can use it through an attribute
map (clump only near the guides, say). With `limitDistance` 0, Interpolate shapes every strand, as usual.
The preset also clumps only the guided hair: the Clump's `strength` and `centerWeight`
both read the `guideInfluence` channel (an attribute map), so clumps form around the guides and
fade out with their reach, and the flow elsewhere stays smooth. Nothing in it depends on the
groom's size.

**Presets fit the scalp.** A preset records the scalp size it was made on (`"size"`: the
largest extent of the scalp's bounding box). `apply_preset` (and the Graph Editor's Presets menu)
fits it to the groom's scalp: params in length and width units (strand length, radii, widths,
noise amplitudes, the min and max of their maps) scale with the size, densities with its square,
and rates per unit of length (Curl's turns, Twist's turns, Braid's crossings, Wind's and
FieldNoise's frequency: their spec's unit `perLength`) with its inverse, so curls keep their count
per strand. Noise's `frequency` is per strand length by default and does not change.
A preset made on a head works on a 1 cm test sphere. `save_preset` records the size;
`apply_preset(groom, name, fit=False)` applies the numbers as they are.

- Each stroke draws as one curve along its arrows with an arrowhead at its end (it breaks where
  arrows were erased); the field draws as a pale preview (`displayFlow`). Arrows are bound to the
  scalp and follow it when it deforms, and travel with groom files (`"flow"`: each pin records its
  `line` and `seq`).
- With **Flow** ticked under Acts on in the brush settings, the shaping brushes work on the arrows and sinks
  too: Comb slides them with the stroke and lays arrows along it, Smooth
  averages arrows with their neighbours and evens out a stroke's arrows, Twist turns arrows
  and a whorl's angle, Length sizes a sink's radius, Attract gathers them, Straighten lines a
  stroke's arrows up (see Brushes). The field previews live as you drag.
- Symmetry mirrors strokes and sinks. Each stroke or click is one undo step. The Flow box's
  **Clear All** removes every arrow and sink; the Erase brush removes both (`erase_flow`). Sinks
  travel in `flowData` (`"sinks"`: face, u, v, `angle` in degrees, 0 out, 180 in, positive
  counter-clockwise, and `radius`).

```python
groomlab.flow_stroke(groom, points, radius=0.2)          # scripted; erase=True removes
groomlab.add_flow_node(groom, after="interpolate", lift=10, bend=0.6)
groomlab.flow_sink(groom, point, kind="whorl", radius=0.3)  # whorlcw, converge, part; erase=True
groomlab.flow_arrows(groom); groomlab.flow_sinks(groom); groomlab.clear_flow(groom)
```

### Correctives (sculpt layers and shot fixes)

The groom is a stack, as with Yeti's layers: the **Base groom** (its guides) at the bottom and
**correctives** on top. In Groom Tools > Sculpt, **Correctives**, click the row brushes should edit:
the Base groom edits the guides; a corrective records each stroke as a correction and leaves the
guides alone.

- **Weights are keyable.** Each corrective's weight is an attribute on the groom's transform
  named after it (in the Channel Box with the groom selected), driving the shape; its keys show in
  the timeline. Unkeyed, a corrective applies on every frame at its weight (**+**: a new one; use
  them to try looks side by side and blend them). For a shot fix, key it: **Fix Frame** (the ◆+
  button) makes a corrective that is fully on at the current frame and blends in over the 6
  frames before and out over the 6 after (keyed 0, 1, 0), so the fix doesn't pop. Drag or add
  keys in the timeline or Graph Editor like any animation (more keys, holds, eases). The **Weight** slider sets it on this frame; once it is
  keyed, changing it keys it there (◆ keys it by hand). Above 1 exaggerates.
- **The list:** each corrective's weight on the current frame and its keys (◆ 3, or "all
  frames"); tick to show or hide, double-click to rename, ↑ / ↓ to reorder, **S** to solo
  (again for all), ✕ to delete it and its keys. Right-click: Go to Its Frame (its strongest
  key), Key Weight Here, Clear Keys, Rename.
- Offsets live in each guide's own frames, so a fix made on frame 1012 follows the hair as it moves
  (a simulation or a guide cache). Children follow corrected parents, and only guides you actually
  sculpted get offsets.
- **Renders** include them: the MtoA translator, groom files (`"correctives"`, keyed weights
  sampled per frame into a `weightCurve`) and so the procedural and groomlab-cli.
- **Tools:** shaping brushes only (not Place, Draw, Erase, Paint or Part). Each stroke is one
  undo step.
- Older shot layers (with fade-in/out frames) turn into keys the first time the Correctives list
  shows them (`shots.upgrade(groom)`).

```python
from groomlab import shots
shots.add_layer(groom)                              # a corrective on every frame; brushes edit it
shots.add_layer(groom, frame=1012, fade=6)          # a shot fix keyed 0 -> 1 -> 0 around 1012
shots.set_layer(groom, "sculpt1", weight=0.5, enabled=False, rename="look")
shots.key_weight(groom, "shot1012", 0.7, frame=1015); shots.clear_keys(groom, "look")
shots.weight_plug(groom, "shot1012")                # "groom.correctiveWeight[i]": key it like anything
shots.set_target(groom, None)                       # back to editing the base groom
shots.layers(groom)                                 # name, enabled, weight, keys, frame, guides
shots.solo_layer(groom, "look"); shots.move_layer(groom, "look", 1); shots.remove_layer(groom, "look")
```

`groomLabGuides -guidePoints <id> -evaluated` returns a guide as shown (posed and corrected).

### Review

GroomLab > Review, or by script:

```python
groomlab.density_heatmap(groom)          # strandColorMode density; False goes back to solid
groomlab.show_hud()                      # active groom: strands, strands at render, guides, eval ms
groomlab.groom_stats(groom)              # the same numbers as a dict
rig = groomlab.turntable(groom, frames=120)  # orbiting camera (renderable) + key/fill/rim lights
```

The turntable camera frames the groom's bounds and orbits once over `frames`; the lights stay
put, so highlights travel over the hair. It sets the playback and render range.
- **Render Turntable...** (GroomLab > Review): the groom going round once, rendered in Arnold for
  dailies. Pick the frames, every Nth frame, the size and Preview or Final quality. It uses the
  scene's turntable rig (making one, and a hair shader if the groom has none), saves the scene and
  renders the saved file in a background `Render` process, so Maya stays free; a progress window
  can stop it. Frames land in the scene's `groomlab/<scene>/renders/turntable_<date>` folder (opened when
  done), with a `turntable.mp4` when `ffmpeg` is on the PATH, and a `render.log`.
  `groomlab.render_turntable(groom, frames=120, step=1, width=960, height=540, quality="preview")`
  returns the job (`poll()`, `wait()`, `cancel()`, `rendered()`).

### Rendering with Arnold

The module adds its `extensions` folder to `MTOA_EXTENSIONS_PATH`, so MtoA loads
`groomLabTranslator` automatically. At render time the translator re-evaluates the
graph at full density and ignores `previewDensity`. It exports linear curves with
one radius per point (half the strand width).

- **Shader:** the shading group's shader if one is assigned (use
  `groomlab.assign_shading_group(groom, sg)`, because `sets -forceElement`
  rejects plugin shapes), otherwise a default `standard_hair`.
- **Hair color from the groom:** `groomlab.create_hair_shader(groom)` makes an `aiStandardHair`
  whose `melanin` and `melaninRedness` read the strand channels `HairColor` writes (through
  `aiUserDataFloat`), assigns it, and turns off `melaninRandomize`, since HairColor varies the
  color per strand and per clump. When the color is more than the pigment (a map, hue,
  saturation or value randomness, a tip color, a Bands or FeatherColor node) or a Blend mixes two
  looks' colors, the shader reads the RGB (`hair_color`) instead. Its highlight is the HairColor
  node's (specular tint `min(1, specular / 0.3)`, roughness `0.1 + 0.4 x roughness`, shift the
  tilt; 0.3, 0.5 and 3 degrees without one). The lit viewport shows each strand's own color, as without a shader.
- **One color, everywhere:** the `HairColor` node writes the color twice, as melanin for the
  Arnold hair shader and as linear RGB per strand (`colorR/G/B`) for engines and formats that
  want a color: Unreal `groom_color`, USD `displayColor`, Alembic `Cd`, Arnold `hair_color`,
  the cards' `_Color` bake. A tip color (`tipStrength`), Bands and FeatherColor write the RGB
  per point as well; USD's per-point primvars and Arnold's `hair_color` carry it per point (Alembic's `Cd`
  is per curve). The RGB is the
  melanin's color tinted by the dye, or the `map` (a painted map, a scalp color set or an image),
  so the same groom renders in Arnold and runs in Unreal alike.
- **Presets:** `cinematic_hair` (two clump levels, restrained noise, frizz, flyaways, length,
  width and color variation) is the starting point for hero hair; insert the `wet` add-on for
  wet hair.
- **Level of detail:** for crowds and wide shots, the groom's Level of Detail attributes (in
  the Attribute Editor under Rendering) thin and cull strands at render time, in both render
  modes, against the render camera:
  - `lodDistance`: strands are kept with a probability falling from 1 at `lodNear` to
    `lodMinDensity` at `lodFar` (distance from the camera to the root, in the groom's space),
    by stable ID, so the same strands stay from frame to frame. Survivors are widened by
    density^-`lodWidthCompensation` (1 keeps the coverage; at most 10x).
  - `cameraCull`: strands wholly outside the camera's view, widened by `cullMargin` on each
    side, are dropped. Keep a margin for shadows, reflections and motion blur near the frame.
  - `backfaceCull` (off by default): strands whose root faces away from the camera (the scalp's
    normal under it, else the strand's first segment) by more than `backfaceAngle` degrees past
    side-on are dropped: the back of a head in a close-up. Keep a padding for long hair that
    hangs into view, silhouettes, shadows and reflections; decided by root, so by strand.
  - Every motion key keeps the same strands. The render log reports what LOD kept.
- **Render overrides** (Attribute Editor > Rendering > Render Overrides; renders alone, the
  viewport and the groom as groomed, both render modes): `renderDensity` the share of the strands
  rendered (0-1, by the preview's thinning: the groom's own strands, the same look),
  `renderWidthScale` times every width, `renderLengthScale` each strand scaled about its root as a
  Transform node's scale does (shape and all; 0.5 renders it at half size, not trimmed). The
  procedural takes them as `density`, `width_scale` and `length_scale`; exports with
  `-renderOverrides`. The defaults (1) change nothing.
- **Arnold attributes:** `aiMode` (ribbon or thick; Arnold's `oriented` mode needs orientations,
  which it does not support on linear curves, so that choice renders as ribbon with a warning),
  `aiMinPixelWidth` and the usual visibility flags appear on the groom node.
- **Shading data:** curves carry the root UV in `uvs` (when the scalp has UVs), a `strand_id`
  user attribute (uniform UINT), every strand channel, such as `clumpId`, as uniform FLOAT
  user data, and `hair_color` (linear RGB) when the groom has a color node (its RGB):
  uniform, one color per strand, or varying, one per point, when a HairColor tip color, Bands or
  FeatherColor write the RGB per point. With FeatherColor's iridescence: `iridescence` (varying
  FLOAT) and `root_normal` (uniform RGB holding a direction, for `aiUserDataColor`: a feather's
  vane normal, else the scalp's at each root), which `create_hair_shader`'s iridescence network
  reads (Color patterns). `aiUserDataColor` reads either; a varying color is interpolated along the
  curve. A RenderData node adds `opacity` (varying FLOAT; the curves not opaque when any point is
  below 1), a HairColor with its highlight set off the defaults `specular`, `roughness` and `tilt`
  (uniform FLOAT, as every strand channel). Read
  them with `aiUserDataInt`, `aiUserDataFloat` and `aiUserDataRGB`. Every exporter (Alembic,
  USD, Unreal, Arnold) describes the strands the same way (`engine/core/include/groomlab/export_view.h`).
- **Render LOD camera:** both render modes read the camera from Arnold (`fov`, the screen window
  from Maya's film fit and offsets, and the options' `xres`, `yres` and `pixel_aspect_ratio`), so
  they see the same view. An orthographic camera culls to its width and never thins by distance.
- **Motion blur:** with deformation blur on, the groom is evaluated at every motion step and
  the points get one key per step. Keys need the same strands, which a rest mesh guarantees on a
  deforming scalp. If the strands still differ, the groom renders without deformation blur and
  logs a warning. Instances blur the same way: the instancer's `instance_matrix` gets a key per
  step from that step's strands (and its `matrix` the groom's transform keys), in both render
  modes; if the instances differ between steps (other strands), they render without blur on
  those steps and a warning is logged. The prototype meshes themselves are exported once, in
  their own space.

#### Rendering from a groom file (farms)

```python
groomlab.render_from_groom_file(groom, "C:/shots/010/hair.groom", start=1, end=48)
```

This exports the groom file and scalp cache and sets the groom's `renderMode` to `groomFile`.
Exported `.ass` files then hold a small `groomlab_procedural` node instead of every strand, and
Arnold evaluates the groom at render time, motion keys included. Re-run it after editing the
groom. `groomlab.render_curves(groom)` switches back. Both are also under GroomLab > Export.

The procedural needs only Arnold. It works in kick and on farms without Maya:

```bash
kick -i shot.0010.ass -l C:/path/to/modules/GroomLab/procedurals
```

- **Install:** the module ships `GroomLab/procedurals/groomlab_procedural.dll`, with TBB beside
  it, and puts that folder on `ARNOLD_PLUGIN_PATH`. `GroomLab/bin` holds `groomlab-cli`.
- **Hand-written scenes:** `.ass` files can use the procedural directly:

```
groomlab_procedural
{
 name hair
 filename "C:/shots/010/hair.groom"
 frame 10
 motion_start -0.25
 motion_end 0.25
 motion_keys 3
 shader "hairShader"
}
```

- **Parameters:** `fps` (0 uses the file's), `density`, `mode`, `min_pixel_width`,
  `width_scale` and `length_scale`, plus the LOD parameters (`lod_distance`, `lod_near`, `lod_far`,
  `lod_min_density`, `lod_width_compensation`, `camera_cull`, `cull_margin`, `backface_cull`,
  `backface_angle`).
- **Shape parameters:** `shader` is the shape's usual `NODE[]` (one shader, as above; `kick
  -info groomlab_procedural` lists it). It and the other shape parameters the procedural
  carries (`visibility`, `sidedness`, `matte`, `opaque`, `receive_shadows`, `self_shadows`,
  `light_group`/`use_light_group`, `shadow_group`/`use_shadow_group`, `trace_sets`, `id`,
  `transform_type`) pass to the curves it makes.

### Rendering with RenderMan

The module ships `GroomLab/procedurals/renderman/groomlab_renderman.dll`, a RenderMan 27
procedural that grows a groom file at render time in prman, without Maya, with primvars for a
PxrHairColor + PxrMarschnerHair network (`groom.rib` beside it is a complete example scene). RIB
files call it in either of RenderMan's two procedural forms:

```
Procedural2 "DynamicLoad" "DSOBound" "string dsoname" ["groomlab_renderman"]
    "string filename" ["C:/shots/010/hair.groom"] "float frame" [12]

Procedural "DynamicLoad" ["groomlab_renderman" "-file 'C:/shots/010/hair.groom' -frame 12"]
    [-100 100 -100 100 -100 100]
```

The second, older form takes its parameters as one data string of `-key value` pairs: `-file`,
`-frame`, `-fps`, `-density`, `-widthScale`, `-lengthScale`, `-motionStart`, `-motionEnd`,
`-motionKeys`, `-searchPaths`, `-name`, `-debug`. A value with spaces goes in quotes. An unknown key
is skipped with a warning. docs/DEVELOPING.md, "RenderMan procedural", has the details.

#### RenderMan for Maya (experimental)

This needs RenderMan for Maya 27 on Maya 2024 to 2026. It isn't verified in Maya yet; the render
path is, in prman with the same call. RenderMan for Maya renders the groom through its own generic
**RenderManProcedural** node, with no GroomLab translator:

1. Load RenderMan for Maya (`RenderMan_for_Maya` in the Plug-in Manager). **GroomLab > Export**
   then shows **RenderMan Procedural (experimental)**.
2. Select the groom and choose it. It makes `<groom>_rman`, a RenderManProcedural node linked to
   the groom, which:
   - points `DSO Name` at the module's `groomlab_renderman.dll`;
   - sets `Data` to the groom's settings;
   - assigns a PxrMarschnerHair network that reads the procedural's primvars.
   In Python: `groomlab.renderman.add_renderman_procedural(groom, density=1.0)`.
3. Render. Before RenderMan for Maya translates the node at each frame, its **Pre Shape Python
   Script** writes the groom to a groom file at that frame:
   `<scene folder>/groomlab/<scene>/renders/renderman/<groom>.<frame>.groom`. If the groom renders
   from its own groom file (**Export > Render from Groom File**, which caches the scalp's
   animation), it uses that file instead. Then it puts `-file` and `-frame` into `Data`, plus the
   shutter as motion keys while 3D motion blur is on in the render settings.
4. Edit `Data` for the rest, for example `-density 0.5 -widthScale 1.2 -debug`. The script only
   replaces `-file`, `-frame` and the motion keys, and keeps your keys. Set the node's
   `groomLabShutterFromGlobals` off to set the motion keys yourself.

The groom's own shape renders nothing in RenderMan for Maya. Without a scalp cache, a moving scalp
doesn't blur: each frame's file holds the scalp at that frame. Use **Render from Groom File** for
deformation blur. On Maya 2022, or with RenderMan for Maya not loaded, the item doesn't show and
nothing changes.

### Exporting Alembic

```python
groomlab.export_alembic(groom, "C:/cache/groom.abc", start=1, end=48)
```

In Maya it is GroomLab > Export > Alembic... (the playback range). This wraps the
`groomLabExportAlembic` command (`-file`, `-startFrame`, `-endFrame`, `-step`, `-density`; `-scale`,
`-pointsPerStrand` and `-vertexUVs` fit the file to an engine's importer, as Exporting strands for
Unity does):
- **Evaluation:** each frame is evaluated in a DG context, so the scene's current time never
  changes.
- **Density:** 1.0 by default (render density). `previewDensity` is ignored.
- **Render overrides:** `-renderOverrides true` (`render_overrides=True`; USD too) exports as a render
  has the groom: its `renderDensity` times `-density`, `renderWidthScale`, `renderLengthScale`.
- **Layout:** the file holds `/<groom transform>/<groom transform>Shape` as linear curves with a
  per-point `width` (diameter) and, per curve: `strandId` (uint64), every strand channel such
  as `clumpId` (float), `Cd` (linear RGB per curve: the color nodes' `colorR/G/B` channels, else
  from melanin channels, when present) and
  the native `uvs` (the root UV on the scalp, when the export is given the scalp). The first
  frame decides which channels every frame carries, as the USD export does.
- **Atomic:** every export (Alembic, Unreal, USD, groom files) is written to `<file>.tmp` and
  moved into place when it closes, so a failed export never leaves a half-written file.
- **Topology:** on a deforming scalp, give the groom a rest mesh (below) to keep the same
  strands on every frame; without one the strand count may change from frame to frame.
- **Frame rate:** frame times use the scene's time unit, so a reading scene needs the same frame
  rate to line up.

To inspect a file, run `groomlab-cli abc-info file.abc`.

### Game cards (live)

Groom the hair as usual, then turn it into cards for a game engine that keep its look: the
cards follow its clumps and shape, and their textures are baked from its own strands (color,
coverage, direction), so the carded groom reads like the groom.

Add a **Cards** node (Graph Editor > Add Node > Games) where the look should be carded, or run
`groomlab.create_cards(groom, lod="mid")`, which adds one before the output and shows the cards.
The node's panel shows what its own cards cost (cards, triangles, textures) and has **Show cards**, **Show textures**,
**LOD** (Hero, Mid, Low), **Bake Maps**, **Bake Scalp** (both into the scene's `groomlab/<scene>/cards/<groom>`
folder), **Export for Unreal...**, **Export for Unity...** and **Export glTF...** (Game cards for Unity,
Game cards as glTF, below).
- **The groom draws the cards; no mesh needed:** **Show cards** (the groom's `displayCards`;
  the A/B, `groomlab.compare_cards(groom)`) shows the cards *instead of* the strands; off, the
  strands. Guides stay either way. Both ways they draw with the strands' own hair shading
  (Maya's hair tube shading: diffuse, highlights along the card, translucence, the scene's
  lights and shadows, the viewport's ambient occlusion and other post effects, two-sided).
  Before a bake they are solid in the theme's card color; once baked, with the maps: the baked
  color x AO, cut out by
  the coverage map (an alpha test at 0.5, as masked cards are in engines: hard, depth-correct
  edges, nothing to sort), so the hair's silhouette shows instead of solid quads. **Show
  textures** (`displayCardTextures`, on by default; `groomlab.card_display(groom, textures=False)`)
  turns the maps off to look at the flat card geometry. A rebake shows at once (`cardMaps` on the
  groom records the maps, and maps rewritten on disk reload by themselves).
- **Out of date:** once a Cards node's cards change after a bake (the strands coming into it,
  or its params), the maps no longer fit them: that node gets an orange badge in the Graph
  Editor and its panel says "Maps out of date: rebake" (`groomlab.card_stats(groom)["stale"]`
  for any, `card_stats(groom)["nodes"][name]["stale"]` or `card_stats(groom, node=name)` per
  node). It is told per node: the bake records, in `cardMaps`, what each node's cards were made
  from (`Strands=<node>:<hash>,...`), so a look with no Cards node after it, or a change under
  one node only, never flags the others; a Cards node the bake never had is out of date too.
  Bake Maps again clears it.
- **The bake locks the atlas:** Bake Maps also writes `<name>_Atlas.json` next to the maps
  (named in `cardMaps`): where each card's texture sits, by the Cards node it came from and the
  strands it was made of. Cards rebuilt after that (guides brushed, the groom re-evaluated, the
  segments changed) take the cell they were baked with, matched among their own node's baked
  cards by the strands they share (layered Cards nodes card the same strands, so one node's
  cards never take another's cells; the cards of a cross each keep their own), so the maps keep
  fitting instead of every rebuild shuffling the atlas; a card of strands the bake never saw
  borrows a baked card's cell (a node added or renamed since, any node's). The next bake lays
  the atlas out afresh.
- **Cards follow the brush:** while a stroke is live the cards rebuild with it when a rebuild
  (the groom at `cardDensity` with its cards) takes a quarter second or less; heavier grooms
  catch up when the stroke ends. **Full density while sculpting** (Groom Tools > Tools; the
  groom's `sculptDensity` 1.0) keeps every strand of the preview up during a stroke instead of
  a lighter share, so nothing pops at the end of it.
- **A mesh only to export:** Export for Unreal makes a card mesh for the FBX and removes it.
  `groomlab.create_card_mesh(groom)` makes a live one on `outCards` for exporting by hand; while
  it exists it shows the cards in the cards view instead of the groom (its visibility follows
  the view), so they never show twice.
- **From the groom before it:** the node makes cards from the strands coming into it and passes
  them through unchanged, so place it where the look should be carded (e.g. before frizz or
  flyaways that cards can't show). Several Cards nodes make layers of cards in one atlas and
  one bake: the bake's resolution is the largest `atlasSize` any of them asks for (unless Bake
  Maps is given one) and the padding between textures the smallest. Only Cards nodes that make
  the output build cards: one bypassed, not connected to the output, or on a Switch's other
  input makes none; one inside a Compound on the way does (named `compound/inner` in the stats
  and the atlas). `groomlab.create_cards` and `cards_preset` change every Cards node of the
  graph (not those inside compounds), or only the one named (`node=`); the panel's LOD menu sets
  its own node.
- **Cards per clump:** clumps come from the `clumpChannel` (`clumpId` by default, what Clump
  writes); without clumps, strands group by root into as many groups as `maxCards` allows
  (`maxCards` / (`cardsPerClump` x `layers`); 0: 60). Each clump then gets `cardsPerClump` x `layers` cards: its strands are banded by depth (how far
  out of the scalp they run) into `layers` (1 to 3) inner and outer shells, and each band is split
  (k-means over where the strands run in the clump's cross-section, at several heights) into
  groups of strands lying together, each group a card along its own median curve showing only its
  own strands. `1 x 1` is one flat card per clump (the low LOD); a hero groom wants 4 to 8 x 2.
  `maxCards` is the one budget, on the cards themselves (sub-cards, crossed and flyaway cards
  count), what the LOD presets set: over it, the clumps with the fewest strands per card give up
  sub-cards first, then neighbouring clumps merge to share wider cards. Merging many clumps of hair that
  turns (short fur combed by a flow) makes wide cards the hair runs across: keep one per clump
  there, with **Bake Scalp** for the base.
- **Cards lie the way the hair lies:** a card turns to its strands' principal direction across
  (a PCA of their offsets at each station, then one steady turn fitted through them up the card,
  at most 100 degrees; `fitTwist` 1, 0 keeps it facing out of the scalp), so a sheet of hair that
  rolls over gets a card that rolls with it instead of a flat blade its strands leave. Crossed
  cards (round tufts) keep their facings. Cards clear the scalp by their centre line, smoothly
  along their length, and each card's base (its root row) sits on the scalp under it, a strand's
  width above the skin across the whole row, however the card turns, bows (`curvature`) or
  ruffles above it (the mesh bends to it over the first tenth of the card, the UVs as they were;
  a card standing on edge puts its lowest vertex there). A ribbon lies on its strands' outer side (where a quarter of
  them run farther out of the scalp), so the groom's silhouette, its outermost strands, keeps its
  size. `ruffle` pushes alternate cards of a clump in and out along their normal (a share of their
  width, never farther than their strands lie off it) and turns them a little the other way, so
  layered cards never share a plane (no z-fighting, more volume); a lone card is never ruffled.
- **The hair fits on its card, the card on the groom:** each card is as wide as nine in ten of
  its strands spread across it (in its plane), plus a `margin` (a tenth of the width each side)
  kept clear of the texture's edges, and as long as nine in ten of its strands reach (it carries on
  past the median tip); the farthest few are drawn along its edge or at its end rather than cut
  off, or widen it: a card as wide as its farthest strand balloons past the groom, and a lower
  level of detail's cards, holding more strands, would balloon more. A strand a narrower card
  (`widthScale` under 1) can't hold is drawn along its edge too. Cards sit a hair above the scalp wherever they lie on it, so the
  skin doesn't cut through them. Cards taper in the mesh (`widthProfile`, the strands' spread),
  but the texture keeps one scale along the card (its widest station's): narrower stations show
  the middle band of it, so a straight strand bakes straight and nothing stretches sideways.
- **Strays leave their card:** a strand that gets far from its group (a Flyaway, a strand the
  Clump let escape: 2.5 times the group's typical reach from its centre line) would make the
  card as wide as its wander, a wing off the clump. It is left out and the card is built from
  the rest; strays go to the flyaway cards first. Card widths are smoothed up the card, never
  narrower than the hair there, so outlines have no notches.
- **Nothing is dropped silently:** strands in groups too small for a card (`minStrands`) join the
  card of their clump they lie closest to along their length, which widens to hold them. Strands
  without a clump, or in clumps too small for one, and the strays above, get thin **flyaway
  cards** of their own, the farthest from any card first, up to `flyawayCards` (the hero preset
  gives 200), and the rest join the nearest card within reach; only strands far from every card
  with no flyaway budget left, and strays past the budget, are left out.
- **How close it is:** `cardFitDistances` (C++) measures how far the strands lie from the cards
  that draw them. On the test head (a sphere, two dozen loose clumps) the 95th percentile was
  1.17 clump radii with one flat card per clump and is 0.36 with the hero settings (the fit
  alone takes it to 0.98, sub-cards without the fit to 0.52).
- **Segments are only edge loops:** a card is made at a fixed resolution; `segments` picks how
  many of its rings become the mesh. Its texture's place in the atlas and the bake don't change,
  so changing the segments keeps the UVs and the baked maps valid.
- **Levels of detail:** `groomlab.cards_preset(groom, "hero" | "mid" | "low", triangles=None,
  node=None)` sets (on every Cards node, or on `node`) a triangle budget (about 50k, 15k and 4k) and the card count, cards per clump and layers
  (hero 6 x 2 with flyaway cards, mid 2 x 1, low one flat card per clump), segments, width,
  curvature, ruffle, texture variations and atlas size that go with it. Every level keeps the
  groom's silhouette (`widthScale` 1 at all three): fewer cards cover the same envelope, the
  coverage in their maps (`tools/bench/card_fidelity` measures it).
- **Texture space:** alike cards (length, width, how their strands lie, color) share one of
  `variations` textures, baked from the most typical of them; every other card shows it mirrored,
  so the repeat doesn't show. Textures are packed by their size in the world, so texel density is
  even and the atlas is used well; `variations` 0 gives every card its own texture. Each card is
  **tinted** to its own strands' color: it keeps their mean color root to tip (a tip color and
  bands too) and that of the strands its texture was baked from, and its vertices carry the ratio
  (`CardMesh::tints`), which the viewport multiplies the map by and the exports carry (the card
  mesh's color set `groomLabTint`, half the tint; glTF's `_TINT`), so shared textures show every
  card's own color.
- **Shading:** `volumeNormals` (on) gives the cards their strands' own normals, as the strands'
  lit drawing has them: out of the scalp at the card's roots, half their part along the card taken
  out, so cards light like the strands they stand for rather than as flat planes. The viewport
  shades them the same from either side (strands have no back to turn round), with the strands'
  ambient and highlight.
- **Crossed cards:** `crossStyle` sets cards per clump: `single` (one ribbon), `cross`
  (`crossCount` cards crossed around the clump's axis: 2 an X, 3 a star, up to 6) or `auto` (the
  default: a cross for round tufts standing off the scalp, spikes and fur tufts, which one ribbon
  would show as a flat blade; one ribbon for hair lying along the head). `maxCards` counts them
  all: over budget, the smallest clumps give up their extra cards first.
- **Braids:** strands a Braid node wove carry a `braidId` channel; with `braids` on (the default)
  each braid makes its own card set instead of being chopped across the clumps its strands came
  from: a cross of `crossCount` (at least 2) cards along the braid's axis for a hanging braid, or
  one ribbon lying along the scalp, facing out of it, for a cornrow (`braidFlat` 1). Braid cards
  get no sub-cards, layers, ruffle or twist fit, take their cards from `maxCards` first, and carry
  the braid's id on the card; strands the Braid left loose are carded as usual. Turn `braids` off
  (`groomlab.create_cards(groom, braids=False)`) to card braided strands by their clumps.
- **Segments by length:** `segmentsByLength` gives each card segments in proportion to its length
  (`segments` for the longest, at least `minSegments`), so short cards don't carry the edge loops
  long ones need. The UVs and baked maps don't change.
- **Controls:** `cardsPerClump`, `layers`, `fitTwist`, `ruffle`, `margin`, `flyawayCards`, `braids`,
  `segments` along, `widthSegments` across, `widthScale`, `widthProfile` (taper), `twist` (on top
  of the fit), `curvature` (bowed, needs 2+ width segments), `minStrands`, `maxCards`,
  `minWidth`, `atlasSize` (the bake's resolution); `groomlab.create_cards(groom, cards_per_clump=6,
  layers=2, flyaway_cards=200, ...)` sets them from Python. `cardDensity` is the strand density
  cards are made and baked at: 1.0, the whole groom, so every strand of a clump goes into its
  card and the bake sees all of them. The viewport's `previewDensity` and `previewPointBudget`
  never touch the cards (the viewport can show 5% of the hair while the cards, the card mesh,
  the bake and the export are all made from 100%); lower `cardDensity` only to rough cards in
  faster. Cards are built only while something uses them (**Show cards**, a bake, an export or
  a live mesh).

Bake Maps (`groomlab.bake_cards(groom, directory=None, resolution=None, occlusion=1.0)`; the folder
defaults to the groom's own in the scene's cards folder, `groomlab/<scene>/cards/<groom>/`, the files
named `<groom>_<map>`) renders the strands into the atlas as TGA files in Unreal's hair card layout:

| Map | Channels |
|---|---|
| `_Depth` | distance off the card plane (0.5 on it, higher in front) |
| `_Coverage` | opacity; fades in over the first 6% of each card so card roots blend |
| `_Tangent` | each strand's own direction in card space: x across (U), y root to tip (V), z out; 0.5 + 0.5 t (a feather's barbs run diagonal to the card) |
| `_Attribute` | R, G: root UV on the scalp; B: seed per strand; A: coordU, 0 root to 1 tip along each strand (a feather's barbs: from the shaft out) |
| `_Color` | base color, sRGB: the strands' color as the viewport shows it (the color nodes' RGB channels, per point along the strands where a tip color, Bands or FeatherColor vary it, else melanin); with FeatherColor's iridescence an RGBA map, its alpha the iridescence (255 full) |
| `_AO` | ambient occlusion from the hair around (1 open) |

Strands are drawn at their own width, at least a texel wide and opaque (`minStrandTexels`: a
viewport draws every strand at least a pixel wide, and a hair's width is a fraction of a texel, so
to scale they baked faint and the cards' cutoff left them out), where they actually lie
on the card (a short strand ends partway, it isn't stretched), front ones over back ones, and
colors spread a few pixels past them so mip maps don't pull in black. Every texture keeps its
edge rows and columns clear (the card's margin, and strands past an edge drawn along it), so
nothing bleeds into the neighbouring texture and the coverage's border is always zero. No card mesh is needed to
bake. **Show cards** (`groomlab.compare_cards(groom)`, which also turns on two-sided lighting:
cards are seen from both sides) then shows them against the strands, shaded as the strands are
(Maya's hair shading with the strands' normals, ambient and highlight, the same from either side,
each card's tint over the map, the coverage cut out crisply up close and kept, hashed per pixel,
as the texture shrinks), on DirectX 11 and the OpenGL Core Profile alike; a live card mesh, if
there is one, gets a material from the maps (color x half the AO, coverage as transparency).

**Bake the groom onto its scalp** (the node's **Bake Scalp**, or
`groomlab.bake_scalp(groom, directory=None, resolution=2048, reach=None, density=1.0)`): low LODs leave
skin showing between cards, so this bakes the hair near the skin, as seen from outside, into the
scalp mesh's UVs (it needs UVs): `<name>_ScalpColor` (the hair's color, spread over the whole map)
and `<name>_ScalpCoverage` (how much hair covers the skin: hair lying on it by its footprint, hair
standing up by the side it shows, softened over about the spacing between roots). It also makes
a **hair cap**: a copy of the scalp just above the skin, following it, textured with the maps
(`groomlab.scalp_cap(groom)`); it shows with the cards. Hair within `reach` of the skin counts
(default 3% of the scalp's size); the groom is read at `density`.

**For Unreal:** Export for Unreal (`groomlab.export_cards_unreal(groom, folder, name, lod=None)`)
bakes and writes `<name>_cards.fbx` (UV0 is the atlas, V root to tip, with a material pointing at
the maps; the mesh it makes for this is removed after), the six maps, the scalp maps with the hair
cap `<name>_scalpCap.fbx` (when the scalp has UVs; `scalp=False` leaves them out), and
`<name>_cards.json` listing what each channel holds. `name` defaults to the groom's asset name,
with `_<lod>` when `lod` says which level of detail it is (`"hero"`, `"mid"`, `"low"`), so LODs
exported into one folder keep apart; the folder defaults to the scene's exports folder. The
export bakes a copy: the groom keeps showing its own bake (its `cardMaps`, card mesh and hair cap
materials are left as they were). Import the FBX as the groom's cards LOD (or a static mesh with a
hair card material) and assign the textures. The FBX carries what the viewport shades the cards
with: the strands' normals (locked) and each card's tint as the color set `groomLabTint` (half the
tint): in the material, Base Color = Color x 2 x Vertex Color (RGB, read as linear), and an Opacity
Mask of the Coverage map (dithered, or with alpha-to-coverage, keeps thin strands as the card
shrinks on screen). Unreal can also import the strands themselves
(Exporting for Unreal Engine).

### Feather cards

Games draw a feather as one alpha-tested card textured with a baked feather, many feathers
sharing a few textures. A Cards node does that for the feathers a Propagate node grows (strands
carrying `featherId`, `shaftU`, `side` and `isShaft`):

- **A card per feather:** a strip along the shaft of `featherSegments` segments (default 6), bending
  with it, lying in the vane plane (the way the barbs spread, toward the right vane) and as wide
  as both vanes reach at each station, plus the `margin`; it runs from the quill past the
  farthest barb tip. Asymmetric vanes keep the shaft off the texture's middle, at one place all
  along, so the rachis bakes straight. The texture is the feather's barbs and its rachis (a kept
  shaft, `keepShaft`, drawn at least 2.5 times a barb's width). Feathers are told apart by
  `featherId` and their shaft's root, so several Propagate nodes merged together stay apart.
- **The spine** (`featherSpine`, on by default): each card gets a column of vertices exactly on
  the shaft, so every segment is two quads, left edge to spine and spine to right edge, and the
  card can bend along the rachis. Each half follows its own vane as the groom has it: at each
  station, the way that vane's barbs head across the shaft (their samples there, the outer parts
  counting most) sets the half's fold out of the vane plane, so lifted vanes make a V opening up,
  drooping or cupped ones a V opening down, and one vane up with the other down shows as such.
  The fold is smoothed along the shaft, kept within 60 degrees, and flat where a vane has no
  barbs (the bare quill). Each edge vertex has its half's normal (the spine's lies between), so
  a cupped feather lights as one. The spine's U is where the rachis bakes (the off-center shaft
  line of asymmetric vanes), for shared and mirrored variations too. *The bake:* games bake a
  feather flat and fold the geometry, and so does this: each fold lifts the flat card's edge
  straight out of its plane (along the card's normal), so every vertex keeps the flat card's UV
  and a barb lying in its half shows exactly where it is (the flat bake's projection is exact
  for it); only Depth and Tangent are measured in each half's own frame, matching the normals
  the half shades with. Off: the flat strip of one quad per segment, exactly as before. Down
  feathers' crossed cards stay flat strips either way (a puff has no vanes to fold to, and each
  crossed card already bends along the shaft down its middle).
- **Down feathers** (barbs `around` the shaft) get `crossCount` crossed cards (two, an X) as wide
  as the puff, each showing the same baked puff, alternate ones mirrored.
- **Shared textures:** with `variations`, alike feathers (length, the vanes' outline, their
  asymmetry, the barbs' angle and curl, color) share one baked feather, the most typical of
  them; every other one shows it scaled to its own length (so the barbs keep their angle) with
  the shaft where the owner's is, mirrored left to right when its longer vane is on the other
  side (`mirror`; near-symmetric feathers alternate so the repeat doesn't show). Down feathers
  cluster apart from vaned ones; the two share `variations` by their numbers.
- **The maps:** Coverage, Color, Depth and AO come from the barbs and the rachis. The Tangent map
  holds each barb's own direction in card space (barbs at 45 degrees read (0.71, 0.71, 0) on the
  right vane, (-0.71, 0.71, 0) on the left), which is what Unreal's and Unity's hair shaders
  light along, so highlights run along the barbs, not up the card. The Attribute map's coordU
  (A) runs root to tip along each barb, out from the shaft, for the shaders' root-to-tip shading;
  a gradient along the feather is the card's own V (UV0 root to tip; glTF's TEXCOORD_1 y), and
  the feather's colors along its shaft (a color node reading `shaftU`) are baked into Color.
- **featherMode:** `auto` (default) cards feathers per feather and any hair reaching the node as
  hair; `perFeather` only the feathers; `off` only the hair (a hair Cards node after feathers that
  have a node of their own); `perVane` everything as hair (a card per `clumpId`: per vane or
  split, the old way). Plain hair is carded exactly as before.
- **Budget:** feathers take their cards from `maxCards` first (a down feather's cross counts
  every card); past it, the shortest feathers make no card. `featherMinLength` leaves out
  feathers shorter than that share of the longest (small down feathers at low LODs).

```python
made = groomlab.add_feathers(groom, "contour", cards=True, cards_lod="mid")  # feathers with cards of their own
node = groomlab.create_feather_cards(groom)          # or later: a Cards node on the feather branch
groomlab.create_cards(groom, lod="mid")               # the hair's cards
groomlab.card_node_kind(groom, node)                  # "feathers" ("hair", "both")
groomlab.card_stats(groom)["nodes"][node]["feathers"] # feathers carded by that node
groomlab.cards_preset(groom, "low")                   # hair nodes the hair preset, feather nodes the feather one
```

`create_feather_cards(groom, feathers=None, lod=None, segments=None, min_length=None,
variations=None, spine=None)` puts a Cards node (featherMode `perFeather`) right after the node giving the
feathers (default: the end of the branch `add_feathers` made, where the feathers meet the hair or
the output; an existing one there is reused) and sets Cards nodes after it that card feathers
too (`auto`) to `off`, so none is carded twice. The hair's and the feathers' Cards nodes share the
groom's one atlas and one bake; each node keeps its own cells, stats (`cards`, `triangles`,
`textures`, `feathers`) and stale flag (a node carding only feathers, or only hair, makes its
cards from those strands alone, so a change to the other never makes it stale). The viewport
draws them with the card effect like hair cards.

**Levels of detail:** `cards_preset` gives a Cards node only feathers reach (or set to
`perFeather`) the `FEATHER_CARD_PRESETS`, all with the spine: hero 6 segments along each feather
(about 30k triangles), mid 3 (8k, feathers under 15% of the longest left out), low 1, a simple V
of two quads per feather (2k, under 35% left out), with 24, 12 and 6 shared textures and a 4096,
2048 and 1024 atlas. A vaned feather's card costs 2 x `featherSegments` triangles, 4 x with the
spine (`groomlab.feather_card_triangles(segments, spine)`: hero 24, mid 12, low 4), and
`maxCards` is the triangle budget over that (hero 1250 cards, mid 666, low 500; a down feather's
cross counts each of its flat cards, which cost half, so the budget holds). A node both hair and
feathers reach gets the hair preset plus the feather one's segments, spine and minimum length,
and both budgets.

**Exports:** Unreal (FBX), Unity (FBX) and glTF card exports take the feather cards with the
hair's, in one mesh and one atlas: the same maps, alpha-masked at 0.5 and double-sided; the
manifests count them (`feathers`) and describe the spine (`featherCards`), and glTF's `lods`
bakes each level with the feather presets. The spine goes out as it is: three vertices a
station (left edge, spine, right edge) with the halves' normals, and glTF's TEXCOORD_1 x is 0 on
the left edge, 0.5 on the spine and 1 on the right edge wherever the rachis sits across the card,
so `abs(2x - 1)` is 0 on the rachis and 1 at the vanes' edges (stiff to free, for wind), as it is
from a hair card's middle to its edges.
(Feathers as strands instead: Feathers in Unreal, Feathers in Unity.)

### Segments by length or curvature (Resample node)

The **Resample** node's `mode` spends fewer points where they aren't needed, so the viewport,
renders and every export (Unreal, Alembic, USD, groom files) get lighter:

- **even** (default): every strand gets `segments`, evenly spaced.
- **length**: segments in proportion to each strand's length: `segments` for the longest (0: as
  many as they have), at least `minSegments` for the shortest. Short hair stops carrying the
  points long hair needs.
- **curvature**: as many segments as each strand needs to hold its shape, about one per
  `maxAngle` degrees it turns (15 by default; smaller keeps curls rounder), from `minSegments` for
  a straight strand up to `segments`, and placed closer together where it bends, farther apart
  on straight runs. `segments` stays the working resolution Scatter grows with, so curls and
  frizz are shaped in full detail first. With 40 segments, straighter presets drop from 30.5k to
  3-4k points with no visible change; a fully curly groom keeps nearly all of its points.

Put it where the shapes and lengths are final (after Interpolate takes the guides' shape and
lengths, and after Length). Widths and per-point channels follow the resampling; IDs, roots and
per-strand channels stay.

When the viewport shows a node other than the output (the Graph Editor's view/focus toggle), it
says so at the top: nodes after it, Width, HairColor, Cards, don't show until the output is
viewed again.

### Exporting for Unreal Engine

GroomLab > Export > Unreal Groom..., or `groomlab.export_unreal_groom(groom, path)`, writes
an Alembic file in Unreal's groom format:
- **Strands:** a `strands` curves object with `groom_version_major/minor` (1.5),
  `groom_group_id`, `groom_group_name`, `groom_id`, `groom_width` and `groom_root_uv`.
  `groom_id` is a dense index (0..N-1, in export order; Unreal wants a non-negative int), with
  the stable 64-bit strand ID beside it as `groomlab_strand_id_low/high` (two uint32).
  `groom_group_id` comes from a `groupId` strand channel when the groom has one (an Attribute
  node can write it), else 0. `groom_color` (linear RGB per vertex, the scope Epic's schema
  gives it) is written when the groom has a color node (the `colorR/G/B` point channels
  when they were written: a tip, Bands, FeatherColor; else the strand channels repeated along
  each strand); `groom_iridescence` (per vertex, 0-1) with FeatherColor's iridescence, and with
  `iridescence_as_ao=True` (`-iridescenceAo`, the app's **Iridescence as AO**) the same in
  `groom_ao`, which Unreal's Hair Attributes node hands a material
  ([docs/UNREAL_HAIR_MATERIAL.md](docs/UNREAL_HAIR_MATERIAL.md): the material recipe);
  `groom_roughness` (per curve, 0-1) from the `roughness` strand channel when there is one (a
  HairColor with its roughness set writes it); with a Clump node, `groom_clump_id`. Unreal reads color and
  roughness per curve or per vertex; these are per curve.
- **Guides:** the primary guides as a `guides` object with `groom_guide` = 1. These are what
  Unreal simulates.
- **Groups** (`groups="auto"`, the default): each groom group (the `groupId` strand channel:
  hair 0, each feather design its own, see Feathers) is a curves object of its own,
  `strands_<name>` (and `guides_<name>`), with `groom_group_id` = its id on every curve and
  `groom_group_name` = its name (the groom's name for group 0, the design's for feathers), so
  Unreal gives each group its own material, LODs and physics. `groups="<channel>"` groups by
  another channel; `groups=None` writes one `strands` object, `groom_group_id` per curve from
  `groupId`. A groom of one group (hair alone) writes the same file either way.
- **groom_id** is unique over the whole file: a dense 0..N-1 in export order, group after group
  (guides numbered among themselves). Strand IDs are 64-bit hashes that an int32 could not hold
  without collisions, so they are not truncated into it; they travel as
  `groomlab_strand_id_low/high` beside it. The numbering is per export: a groom cache keeps it
  from frame to frame only while the strands do (a rest mesh).
- **Feathers** (Feathers in Unreal, below): the shafts are left out when the feathers have a rachis
  (`shafts=None`, the default; `shafts=True` keeps them as strands); feather groups get no guides
  (`feather_guides=True`: their shafts); `lod="mid"`/`"low"` thins the barbs.
- **Frames:** the current frame makes a static groom; a frame range makes a groom cache.
- **Importing:** import the .abc in Unreal as a Groom.

#### Feathers in Unreal (the Meerkat eagle workflow)

Weta Digital's Unreal short *Meerkat* (Unreal Engine 4.26, 2020) has an eagle of about 2.5
million strands, groomed in Yeti and brought in as Alembic grooms. Feathers had to be rigged
for real time, so each feather's shaft (the rachis) was modelled as geometry in the eagle's
skeletal mesh, and the barbs were groom strands bound to it: the rachis carries the feather,
the barbs ride the rachis. Heavy shots swapped to a lower-resolution groom. GroomLab builds the
same setup from a feather groom:

1. **Groom the feathers** (Feathers, above): `groomlab.add_feathers(groom, ["contour", "flight"])`.
   Each design is its own groom group, and the presets give it a rachis width
   (`groomlab.set_rachis_width(groom, width, profile=None, sides=None)` changes every design's;
   `rachis_width(groom)` reads it). With a rachis the barbs grow from its surface.
2. **Make the rachis mesh**: `groomlab.create_rachis_mesh(groom)` makes a live Maya mesh on the
   groom's `outRachis` (a tube round every shaft, every feather at full density, following the
   groom as it changes). UV set `map1`: u around the tube, v root to tip; UV set `featherId`: u
   the feather's number (the barbs' `featherId`), v its groom group, so each
   feather's faces can be selected and weighted.
3. **Skin it with the body**: bind it to the character's skeleton with the body (feathers
   following the bones they sit on; a feather's vertices can share one influence so it moves
   rigidly), or combine it into the body mesh before skinning: in Unreal it must be part of the
   skeletal mesh the groom binds to. `groomlab.export_rachis(groom, "rachis.fbx")` (or `.abc`)
   writes it on its own.
4. **Export the groom**: `groomlab.export_unreal_groom(groom, "bird.abc")`: the hair and each
   feather design as groups, the barbs without their shafts, no guides for the feathers.
5. **In Unreal**: import the skeletal mesh (body and rachis) and the groom. Create a Groom
   Binding from the groom to that skeletal mesh (binding type **Skinning**): Unreal binds each
   strand root to the closest triangle of the mesh, and because every barb's root lies on its
   own rachis (never inside it, where the body's skin could be closer) the barbs follow their
   feathers. In the groom asset, turn simulation off for the feather groups (they ride the
   skinned rachis; the hair group keeps its physics and the primary guides) and give each group
   its material and LOD settings. Barbs carry `groom_root_uv` of their feather's base on the
   body (a body texture colors a feather through its root UV), `groom_color` from the color nodes and
   `groom_clump_id` per vane.
6. **Heavy shots**: `groomlab.export_unreal_groom(groom, "bird_low.abc", lod="low")` (or `"mid"`)
   writes a lighter groom to swap in: 25% (50%) of the barbs, picked by ID so the same ones
   every frame, each 1/sqrt(share) wider so the vanes keep their look (`barb_density` and
   `barb_width_scale` set them directly); the hair keeps `density`.

What only Unreal shows: how the binding, the per-group materials and LODs look on the skinned
character, and how the rachis's skin weights move the feathers. Sources:
[befores & afters, "Eagle vs. meerkat"](https://beforesandafters.com/2020/11/18/eagle-vs-meerkat-behind-weta-digitals-stunning-unreal-engine-short/),
[Unreal Engine spotlight on *Meerkat*](https://www.unrealengine.com/en-US/spotlights/behind-the-scenes-on-weta-digital-s-real-time-hair-and-fur-short-meerkat),
Epic's [Using Alembic for Grooms](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-alembic-for-grooms-in-unreal-engine)
and [Setting Up Bindings for Grooms](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-bindings-for-grooms-in-unreal-engine).

### Exporting strands for Unity

GroomLab > Export > Unity Strands (.abc)..., or
`groomlab.export_unity_strands(groom, path, scale=1.0, resample_to=None, start=None, end=None,
groups="auto", shafts=None, barb_points=None, lod=None, max_strands=64000)`, writes
Alembic curves for Unity's strand hair package,
[com.unity.demoteam.hair](https://github.com/Unity-Technologies/com.unity.demoteam.hair), which reads
them through [com.unity.formats.alembic](https://github.com/Unity-Technologies/com.unity.formats.alembic).
What that importer reads (`Editor/HairAssetBuilder.cs`, `Source/abci/Importer/aiCurves.cpp`) and how
the export fits it:
- **Points and widths:** positions, `nVertices` and the native `width` param per point (a diameter).
  A curve set with one root width of 0 or less loses all its widths (Unity falls back to its
  default diameter), so a strand whose root width is not above 0 gets its widest point's width
  at the root (else the groom's first root width); the rest of it is written as it is.
- **Root UVs per point:** Unity reads the curves' `uvs` expanded, one value per point, and drops
  the whole curve set when there are fewer UVs than points. `export_alembic` writes one UV per
  curve (Alembic's usual scope); this export repeats each strand's root UV (its place on the
  scalp's UVs) at every point, which is what the Hair Asset's **Root UV: Resolve From Curves**
  reads (at the root).
- **Points per strand:** the Hair Asset resamples to **Resample Resolution** points (16 by
  default, 3 to 128) and must when strands have different point counts. `resample_to` (3 to 128)
  gives every strand that many points evenly by arc length (widths follow), so **Resample
  Curves** can be off and the curves go in exactly as groomed.
- **Units:** `scale` multiplies points and widths. The default 1.0 keeps Maya's centimeters,
  which Unity's defaults expect: the Alembic importer's **Scale Factor** is 0.01 (centimeters
  to meters; it scales the points, not the widths) and the Hair Asset reads **Alembic Scale
  Positions** as **Data In Meters** (after that factor) and **Alembic Scale Diameters** as **Data
  In Centimeters**. With `scale=0.01` (meters in the file), set the Scale Factor to 1 and the
  diameters to Data In Meters. The importer's **Swap Handedness** (on) turns Maya's right-handed
  Y-up space into Unity's.
- **Other attributes** (`strandId`, `Cd`, strand channels) ride along; Unity ignores them.
  (Unreal's `groom_*` attributes are not written here.)
- **Groups** (`groups="auto"`, the default): each groom group (hair 0, each feather design its
  own, as for Unreal) is a curves object of its own, `/<groom>/<group>/<group>Shape`, named after
  the group (the groom's transform for the hair). The Hair Asset makes a strand group of each
  (`GetComponentsInChildren<AlembicCurves>`, in file order) when its **Alembic Asset Groups** is
  **Preserve**; the default **Combine** merges neighbouring curve sets of the same point count
  into one. A group of more than `max_strands` strands (64,000, `HairSim.MAX_STRAND_COUNT`; 0 or
  None: no limit) goes in even parts, `<group>_0`, `<group>_1`, ..., each a run of the group's
  strands in order, so the same groom gives the same parts. A groom of one group within the
  limit writes exactly what `groups=None` (one object, `/<groom>/<groom>Shape`) writes.
- **Barbs:** `barb_points` (3 to 128) resamples the feather groups' strands to their own point
  count (barbs are short: 4 to 8 points hold them), the hair keeping `resample_to`. The Hair
  Asset's resampling is one **Resample Resolution** for every group, so leave **Resample
  Curves** off to keep each group's own count (each group's strands must then share one count,
  which `resample_to` and `barb_points` give). `shafts`, `lod`, `barb_density` and
  `barb_width_scale` are export_unreal_groom's: the shafts are left out when the feathers have a
  rachis, and `lod="mid"`/`"low"` thins the barbs.
- **Frames:** the current frame by default; the Hair Asset takes its rest pose from the first
  sample.

In Unity: install com.unity.formats.alembic and com.unity.demoteam.hair (Package Manager > Add
package from git URL, `https://github.com/Unity-Technologies/com.unity.demoteam.hair.git`), drop
the .abc into the project (Import Curves on), then Create > Hair > Hair Asset, set its **Type** to
Alembic, assign the imported .abc prefab, and **Build strand groups**. Put a Hair Instance in the
scene with that asset (and a hair material) to simulate and render it.

#### Feathers in Unity

The same setup as Feathers in Unreal: the rachis is skinned geometry in the character, the
barbs are strands riding it. GroomLab > Export > Unity Feathers..., or
`groomlab.export_unity_feathers(groom, folder=None, lod=None, name=None, points=16, barb_points=6,
density=1.0, shafts=None, rachis=True)`, writes into `folder` (default: the scene's exports folder):

- `<name>_strands.abc`: export_unity_strands with groups: the hair and each feather design a
  strand group (Unity's per-group material, LOD and physics), the barbs without their shafts,
  the hair at `points` and the barbs at `barb_points` points a strand, root UVs per point
  (each barb's is its feather's base on the body), root widths above 0, centimeters.
- `<name>_rachis.fbx`: `export_rachis(groom, path, unity=True)`, the rachis mesh in the Unity card
  export's conventions: **Y up, converted to meters** (UnitScaleFactor 100), so it imports at
  scale 1 and lands where the strands land after the Alembic importer's Scale Factor 0.01.
- `<name>_unity_feathers.json`: the files, the packages, every strand group in Unity's order
  (index, object, group, strands, points per strand) with the Hair Instance settings to give it,
  and the Hair Asset's settings. `lod="mid"`/`"low"` writes the lighter groom (`<name>` gets
  `_<lod>`).

How Unity attaches strands to a skinned mesh, from the packages' source: a Hair Instance's
**Skinning** settings (per strand group; they need
[com.unity.demoteam.digital-human](https://github.com/Unity-Technologies/com.unity.demoteam.digital-human),
0.2.1-preview or later for the root frame's twist) put a `SkinAttachment` of type **Mesh** on the
group's roots mesh (a vertex per strand root, its normal along the strand's first segment:
`HairInstanceBuilder.BuildMeshRoots`) and point it at a `SkinAttachmentTarget` on a
SkinnedMeshRenderer. `SkinAttachmentDataBuilder.BuildDataAttachToClosestVertex` finds each root's
**nearest vertex** of the target mesh (`meshVertexBSP.FindNearest`), then projects the root onto
the triangles around that vertex (barycentric coordinates plus the distance along the triangle's
normal, preferring triangles the projection falls inside) and carries it, and its frame, with
those triangles as the mesh skins. Nothing is projected along a UV or a direction: the root
must be nearest to vertices of the surface it should ride. With `rootOnShaft` every barb's root
lies on its own rachis's surface, so attach the feather groups to the rachis itself (a
SkinnedMeshRenderer of its own, skinned to the same skeleton), not to the body: a body vertex
nearer than any rachis vertex (a feather lying on the skin, a sparse tube) would take the barbs.

In Unity (2020.2 or later, the render pipeline's hair shaders for HDRP or URP):

1. **Packages**: com.unity.formats.alembic (2.2.2 or later), then from git
   `https://github.com/Unity-Technologies/com.unity.demoteam.hair.git` and
   `https://github.com/Unity-Technologies/com.unity.demoteam.digital-human.git` (Package Manager >
   Add package from git URL).
2. **The rachis**: skin `<name>_rachis.fbx` to the character's skeleton, in Maya before export
   (bind it with the body; a feather's vertices can share one influence so it moves rigidly; the
   `featherId` UV set tells feathers apart) or in your DCC of choice, and import it with the
   character (Convert Units on). Give its SkinnedMeshRenderer a **SkinAttachmentTarget**.
   Combining it into the body mesh works too when no body vertex lies nearer a barb root than the
   rachis does; a mesh of its own is the safe choice.
3. **The strands**: drop `<name>_strands.abc` into the project (Import Curves on, Scale Factor
   0.01, Swap Handedness on). Create > Hair > Hair Asset: **Type** Alembic, the .abc as its
   asset, **Alembic Asset Groups: Preserve**, **Alembic Scale Positions: Data In Meters**,
   **Alembic Scale Diameters: Data In Centimeters**, **Resample Curves** off (each group has its
   own point count), **Root UV: Resolve From Curves**, **Strand Diameter** and **Tip Scale:
   Resolve From Curves**; **Build strand groups**. The manifest lists the groups in the order
   the asset shows them.
4. **The Hair Instance**: add one with the asset, parented under the character. Per strand
   group (Strand Group Settings): the hair's **Skinning** attaches its roots to the body's
   SkinAttachmentTarget; each feather group's to the rachis's. Give each group its material.
   The barbs ride the rachis: hold them to their groomed shape with the group's **Physics**
   overrides (**Global Position** and **Global Rotation** at 1, **Gravity** 0), or put the
   feathers in a Hair Instance of their own with **Simulation** off (Executive settings, per
   instance) when nothing there should move on its own.
5. **Heavy shots**: `export_unity_feathers(groom, folder, lod="low")` writes the lighter strands
   (a quarter of the barbs, each twice as wide) to build a second Hair Asset from and swap in.

What only Unity shows: how the attachment holds up as the rachis deforms (it follows the
triangles around each root's nearest rachis vertex), the look of the per-group materials, and
whether the physics overrides hold the barbs as stiffly as wanted. Sources:
[`Editor/HairAssetBuilder.cs`](https://github.com/Unity-Technologies/com.unity.demoteam.hair/blob/master/Editor/HairAssetBuilder.cs)
(a strand group per `AlembicCurves`; Combine vs Preserve; resampling; widths dropped for a root at 0),
[`Runtime/HairAsset.Settings.cs`](https://github.com/Unity-Technologies/com.unity.demoteam.hair/blob/master/Runtime/HairAsset.Settings.cs)
(the Alembic and resolve settings and their defaults),
[`Runtime/HairSim.cs`](https://github.com/Unity-Technologies/com.unity.demoteam.hair/blob/master/Runtime/HairSim.cs)
(`MAX_STRAND_COUNT` 64000, 3 to 128 particles a strand),
[`Runtime/HairInstance.cs`](https://github.com/Unity-Technologies/com.unity.demoteam.hair/blob/master/Runtime/HairInstance.cs),
[`Runtime/HairInstanceBuilder.cs`](https://github.com/Unity-Technologies/com.unity.demoteam.hair/blob/master/Runtime/HairInstanceBuilder.cs)
(per-group skinning, the roots mesh) and digital-human's
[`Runtime/SkinAttachmentDataBuilder.cs`](https://github.com/Unity-Technologies/com.unity.demoteam.digital-human/blob/master/Runtime/SkinAttachmentDataBuilder.cs)
(nearest vertex, then its triangles).

### Game cards for Unity

The Cards node's **Export for Unity...**, GroomLab > Export > Unity Cards..., or
`groomlab.export_cards_unity(groom, directory=None, name=None, resolution=None, scalp=True, lod=None)`
writes into `directory` (default: the scene's exports folder), named like Export for Unreal
(`<asset>_<lod>`):
- `<name>_cards.fbx`: the card mesh, **Y up, in meters** (the FBX is converted from centimeters,
  so Unity imports it at scale 1 whatever Convert Units says), UV0 the atlas (V root to tip), a
  material `<name>_cards_mat` with the base color map (its alpha as transparency).
  `<name>_scalpCap.fbx` (`<name>_scalpCap_mat`) is the hair cap when the scalp has UVs.
- PNG maps (Unity's preferred format), baked from the same cards:

| Map | Channels |
|---|---|
| `_BaseColor` | RGB: base color (sRGB); A: coverage (alpha clipping at 0.5) |
| `_Mask` | HDRP's mask map layout. R: metallic (0; with FeatherColor's iridescence, the iridescence instead: a material reads it from there); G: ambient occlusion; B: root (0) to tip (1); A: smoothness (1 - the hair look's roughness) |
| `_AO` | ambient occlusion (1 open), gray: URP's occlusion map |
| `_Tangent` | strand direction in tangent space (R along +U, G along +V root to tip, B out), 0.5 + 0.5 t: HDRP Hair's strand direction |
| `_Depth` | distance off the card plane (0.5 on it), for pixel depth offset |
| `_Attribute` | R, G: root UV on the scalp; B: seed per strand; A: root to tip |
| `_ScalpBaseColor` | the hair cap: RGB the hair's color on the scalp, A how much hair covers the skin |

- `<name>_unity.json`: the files, what each channel holds, import settings (BaseColor sRGB with
  Alpha Is Transparency; the others linear) and suggested materials: **HDRP** (a Hair Shader Graph
  or HDRP/Lit: Base Color Map, Alpha Clipping 0.5, Double-Sided, Mask Map, the Tangent map into
  Hair Strand Direction) and **URP** (Universal Render Pipeline/Lit: Base Map, Alpha Clipping 0.5,
  Render Face Both, Occlusion Map, Smoothness).

**Feathers** (Feather cards) go in the same FBX and atlas as the hair's cards: a strip (or, for
down, crossed cards) per feather with its baked barbs and rachis, folded along its spine
(`featherSpine`), the manifest's `feathers` counting them and `featherCards` describing the spine. The suggested materials suit them as they are: alpha clipped at 0.5 (a feather's
outline and the gaps between its barbs are in the coverage), double-sided (HDRP **Double-Sided**,
URP **Render Face: Both**: a strip is seen from both sides as the bird turns), and the Tangent map
runs along each barb (not up the card), so HDRP Hair's highlights run across the vane as on a real
feather. Cards sharing a texture may be mirrored in U (the other side's asymmetry).

As with Unreal, the export bakes its own copy: the groom's `cardMaps`, card mesh and hair cap are
left as they were (a live card mesh keeps its material). The FBX's UVs are the cards' current
atlas: after changing the cards since the groom's last Bake Maps, bake again first (the export
warns).

### Game cards as glTF

The Cards node's **Export glTF...**, GroomLab > Export > glTF Cards..., or
`groomlab.export_cards_gltf(groom, path=None, lod=None, lods=None, resolution=None, binary=True,
cap=True, scale=0.01, alpha_mode="mask", alpha_cutoff=0.5)` (the `groomLabExportGltf` command)
writes the cards as glTF 2.0, for any engine or viewer that reads it (Godot, three.js, Babylon,
Blender, Unity with glTFast): a `.glb` with the maps inside, or with `binary=False` a `.gltf` with
`<stem>.bin` and `<stem>_<texture>.png` beside it. The writer is GroomLab's own
(`engine/core/src/gltf_writer.cpp`: JSON and binary buffers, compressed PNGs, no other library), so the
CLI and pipelines can use it too.
- **Space:** Y up, meters (`scale` 0.01 from Maya's centimeters; a Z-up scene is turned Y up).
- **Nodes:** one root node named after the groom, a mesh node under it per level of detail:
  `<name>_cards` (the cards as they are), or with `lods=["hero", "mid", "low"]` `<name>_hero` ...
  each baked from a copy of the graph with that preset (cards_preset); the groom is untouched.
  With `cap`, `<name>_scalpCap` (the scalp lifted just above the skin, with the scalp bake).
- **Vertices:** POSITION (with its bounds), NORMAL (the cards' strand normals, `volumeNormals`), TANGENT (+U across
  the card, w so that the bitangent runs root to tip), TEXCOORD_0 (the atlas, V flipped to
  glTF's top-left origin), TEXCOORD_1 (x across the card 0..1 by vertex column, 0.5 on a
  feather card's spine; y root 0 to tip 1) and `_TINT` (each card's tint, linear RGB 0..2, 1
  none: multiply the base color's RGB by it; not COLOR_0, which viewers would multiply in as it
  is). Indices
  are 16-bit, 32-bit past 65,535 vertices; triangles wind to face their normals.
- **Material** (PBR metallic-roughness), one per mesh:

| Texture | Channels |
|---|---|
| `<mesh>_BaseColor` | baseColorTexture. RGB: the baked color (sRGB); A: coverage |
| `<mesh>_ORM` | occlusionTexture (R: ambient occlusion) and metallicRoughnessTexture (G, B at 1: roughnessFactor is the hair look's roughness, metallicFactor 0) |

  `alphaMode` MASK with `alphaCutoff` 0.5 (hard, depth-correct edges, nothing to sort), or BLEND /
  OPAQUE with `alpha_mode`; double-sided. The hair cap blends. Each material's `extras.groomlab`
  names the channels and the hair look (roughness, specular).
- **Checked on write:** attribute counts, indices in range, no NaN or infinite values, unit
  normals and tangents, 4-byte aligned buffer views, textures and materials that exist.

### Exporting USD

```python
groomlab.export_usd(groom, "C:/cache/groom.usda", start=1, end=48)
```

In Maya it is GroomLab > Export > USD... (the playback range). This wraps `groomLabExportUsd`,
with the same flags and rules as the Alembic export. It writes a text layer with no USD library
needed; convert it with `usdcat` when size matters:
- **Prims:** `/<groom transform>` is an Xform and the default prim. Under it, `strands` is a
  linear, nonperiodic `BasisCurves`.
- **Samples:** points, `curveVertexCounts`, widths (per point, diameter) and extent, one time
  sample per frame.
- **Primvars:** `strandId` (int64, per curve); `st`, the root UV on the scalp, when the scalp
  has UVs; `displayColor` (linear RGB per curve): the color nodes' `colorR/G/B` channels, else
  from melanin channels, when present; and every strand channel, such as `clumpId`. Channel names become USD identifiers;
  two that collide (`a-b` and `a_b`) or one that is reserved (`st`, `strandId`) get a numbered
  suffix (`a_b_2`).
- **Size:** `.usda` is text and large (about 60 bytes per point per frame); convert with
  `usdcat` to `.usdc` for anything beyond a test.
- **Stage:** `timeCodesPerSecond` follows the scene; `metersPerUnit` is 0.01 (centimeters);
  `upAxis` follows Maya's.

### Where a scene's files live

Everything GroomLab writes for a scene goes into **its own folder beside the scene file**,
`groomlab/<scene name>` (so `shot_v001.ma` and `shot_v002.ma` in one folder never share files),
one subfolder per kind: `masks` (texture maps), `cards` (card and scalp bakes: the maps and
the atlas record), `exports` (Unreal cards, strand exports, groom files), `caches` (guide
caches, baked simulations), `renders` and `references` (reference photos). Inside, a groom's files are named by its **asset
name** (`groomlab.asset_name(groom)`): its full DAG path made file safe, so `|charA|hair` and
`|charB|hair` never meet in a file name, and namespaces never put a `:` in one
(`|charA|hair` and `charA:hair` both give `charA_hair`). Texture maps are
`masks/<asset>/<map>.png`, bakes `cards/<asset>/<asset>_<map>.tga`, and the export dialogs
start from `<asset>.<ext>`. The groom stores those paths relative to the scene
(`groomlab/hero/masks/hair/crown.png`), so **send the scene with its `groomlab` folder and
everything works on the other machine**: masks read, baked cards show, exports sit where they
were. Scenes from before this layout (`groomlab/masks/hair_crown.png`) keep reading their files
where they are. Bake Maps, Bake Scalp and Paint... go there without asking; the export and cache
dialogs open there. `groomlab.asset_folder(kind)` gives the folder (made if missing).

Files are never shared by accident:
- A **new** file never replaces one that is there: a new texture map, a conversion to a texture
  or a default bake whose file is taken (a deleted groom's, another groom's) gets a free name
  beside it (`density_2.png`, `hair_2_Color.tga`).
- A file **two grooms name** (a duplicated groom copies its maps' paths; two references of one
  rig resolve to the same files) is copied for the groom about to write it, which is repointed
  at its copy in the same undo step: a paint stroke, a Fill, a rename (it copies instead of
  moving) or a bake never changes the other groom's hair.
- The Paint brush always paints the file of the painted groom's own map, whichever groom was
  active when the tool was set up.

**Save Scene As** (and the first save of an untitled scene) copies the files the grooms name
into the new scene's own folder (`groomlab/<old name>/...` to `groomlab/<new name>/...`; other
relative paths keep their path under the new folder) and repoints the grooms (and their card
mesh and hair cap materials) there before the file is written, so the saved scene is whole and
independent of the one it came from. A file already at a destination with other content is
never adopted: the copy takes a free name and a Script Editor warning says so. While a scene is
untitled there is no "beside the scene", so the folder is `<project>/groomlab/untitled`: Create
Groom, a new texture map and Paint > As Texture ask you to save the scene first (Save Scene
As..., or go on untitled for now), and a warning says where the files go until you do.

### Paths, referencing and moving projects

Grooms keep no absolute paths, so they work in referenced and namespaced scenes and after a
project moves:
- **Relative paths:** stored paths resolve against the scene's folder, then the Maya project
  root; a groom from a **referenced** file looks in that file's folder first (where its own
  scene kept its files), so a rig referenced into shots in other folders still finds its masks
  and bakes. This covers texture maps, mask images, card maps, `ImportCurves` files, guide
  caches and `groomFile`.
- **Environment variables:** `$VAR`, `${VAR}` and `%VAR%` are expanded when a path is read.
- **Helpers store portable paths:** `texture_map`, masks, card bakes, `import_guides_from_alembic`,
  `set_guide_cache` and `render_from_groom_file` store a path relative to the scene's folder, else
  to the project, when it is inside one (`groomlab.portable_path`).
- **Finding stragglers:** `groomlab.absolute_paths(groom)` lists the absolute paths still
  stored on a groom.
- **Checking a path:** `groomlab.resolve_path(path, node=None)` (or `groomLabPaths -resolve`,
  with `-node` for a node's own resolution) shows where a path resolves.
- **Groom files:** they resolve relative paths against their own folder, then their
  `searchPaths`, which `export_groom` fills with the project and scene folders. Point the search
  paths at the farm's copy of the project with a variable, for example `$SHOW_ROOT`.
- **Node errors:** a groom reports what went wrong in its `lastError` attribute
  (`groomlab.errors(groom)`), such as an unknown operator, a map that can't be read, bad guide
  data or a file from a newer GroomLab. The graph editor's status bar shows it after Apply.
  Evaluation never throws into Maya.

### Rest mesh

On a scalp with deformers (skin, blend shapes), strands are scattered from triangle areas, so
area changes would add and remove strands frame to frame. A rest mesh fixes that: scattering
and the guide and clump matching use the rest pose, and the strands still ride the deformed
scalp.

- **Automatic:** `create_groom` connects the scalp's `Orig` shape when the scalp has deformers.
- **By hand:** `groomlab.set_rest_mesh(groom, mesh=None)`, or **Set** on the Rest row of the
  Attribute Editor's Inputs (or the Graph Editor's Scalp node). It uses the `Orig` shape, or
  else a hidden copy of the scalp at the current frame. The mesh must have the scalp's vertices.
- **Removing:** `groomlab.clear_rest_mesh(groom)`, or **Clear** on the Rest row.

### Follow the scalp

`followScalp` (Attribute Editor > Inputs > Follow Scalp, `groomlab.set_follow_scalp(groom, True)`,
off by default): the groom is evaluated on the rest mesh and each strand is carried to the
scalp as it is by its root's rigid frame (the root face's longest edge at rest made
perpendicular to the smooth normal at the root, the normal, their cross product; a rotation and
a translation, no scale). The evaluation at rest stays cached while only the scalp moves, so a
deforming groom costs the carrying per frame and per motion key (core: 100k strands, 51 ms a
frame normally, 8 ms followed).

- **Needs a rest mesh;** without one, or with the scalp at its rest pose, the groom is exactly
  what it is unfollowed (the Script Editor says so once when there is no rest mesh).
- **Taken back to rest through their roots,** so they keep their world place: guide curves,
  simulated guides (`simGuides`), guide caches, a brush stroke's working guides. The stored
  guides, parts, flow and painted maps are laid on the rest mesh.
- **Read at rest as they are** (they then turn with the head): colliders (Collide against a
  ColliderMesh), ImportCurves, Wind's direction and gusts, world-space noise and anything else
  that reads world positions. Jiggle's scalp history is turned into rest space (each vertex's
  motion by its own frame), so it lags as the head moves.
- **Also carried:** Instance nodes' fixed directions, the Cards nodes' cards (each by its first
  strand's root) and Propagate's rachis rings (each by the scalp point nearest its base).
- Exports, the Arnold translator (one evaluation at rest for all motion keys), groom files
  (`"followScalp": true`), the procedural and the CLI follow too. Core:
  `GroomSession::setFollowScalp`, Python `GroomSession.follow_scalp` (`follow_scalp.h`).

### Colliders

Any mesh can keep the hair off it: a body, a collar, a hat, another character.

```python
groomlab.add_collider(groom, "collar")            # ColliderMesh "collar" + Collide before the output
groomlab.add_collider(groom, "body", name="torso", wire=False)  # the mesh input alone: wire it yourself
groomlab.colliders(groom)                          # [{"name", "mesh", "index"}]
groomlab.remove_collider(groom, "collar")
```

- **How it is wired:** the mesh's world mesh goes onto the groom's `colliderMeshes` (an array of
  `{colliderMesh, colliderName}`), and the graph reads it as the external mesh `mesh:<name>`
  through a **ColliderMesh** node of that name (Graph Editor > Add Node > Sources). `add_collider`
  adds one and a **Collide** node (`collide_<name>`) just before the output, with the scalp on
  its `scalp` input and the ColliderMesh on its `collider` input; move the Collide node earlier in
  the Graph Editor when the collision should happen before clumping or noise. The name defaults
  to the mesh's transform name (rename the mesh and the collider keeps its name).
- **Everywhere the groom is evaluated:** the viewport, the cards, renders (the Arnold
  translator) and exports (Alembic, USD, Unreal) all read the same colliders at the current
  frame, so an animated body keeps the hair out as it moves. Groom files store no meshes: a
  `.groom` rendered on the farm collides only with its scalp (cache the collider with the scalp,
  or bake the groom to Alembic instead).
- **Attribute Editor:** the **Inputs** section lists the colliders with **Select** and
  **Remove**; **Pick** makes the selected mesh a collider. **Remove** (and
  `remove_collider`) drops the ColliderMesh node and the Collide nodes it alone fed (their
  strands pass straight through).

### Several scalps

A groom can grow on more meshes than its scalp: separate ears, a body and a head, eyebrow patches.

```python
groomlab.add_scalp(groom, "earL")             # a Scalp, Scatter and Interpolate for it, merged in
groomlab.add_scalp(groom, "brows", name="brows", wire=False)  # the mesh alone: a Scalp node reads it
groomlab.scalps(groom)                         # [{"name", "mesh", "index", "rest"}]
groomlab.remove_scalp(groom, "earL")
```

- **How it is wired:** the mesh's world mesh goes onto the groom's `extraScalps` (an array of
  `{extraScalpMesh, extraScalpName, extraScalpRestMesh}`, joined after `inMesh` in index order).
  A **Scalp** node whose `input` is the name reads that mesh alone (the Graph Editor's Scalp
  panel has a menu of the groom's scalps and a mesh field with the eyedropper), so a Scatter fed
  it grows hair there. `add_scalp` adds a Scalp node (`scalp_<name>`), a Scatter with the groom's
  own Scatter's settings, an Interpolate on the groom's guides, and a Merge right after the
  groom's own Interpolate, so the look that follows applies to both.
- **One groom:** the scalps are joined into one surface (the groom's own scalp first, so what is
  rooted on it stays put): guides, parts, flow, painted maps (`add_scalp` and `remove_scalp`
  refit vertex maps), the rest pose, Follow the scalp and the simulation work across them; every
  strand's root is on the joined surface and follows its own mesh as it deforms.
- **Subdivision:** each scalp is refined at its own level: the Scalp nodes reading it (match
  render reads that mesh's own level), or the groom's own scalp's level for a scalp no Scalp node
  reads. Refining one scalp leaves the others' strands, maps and guide roots where they were.
- **Jiggle:** each scalp's earlier frames are read (Maya reads every scalp's mesh at the frames
  before), so hair on a moving ear or body jiggles as hair on the groom's own scalp does.
- **Everywhere:** renders (the translator and the procedural), exports and groom files
  (`"scalps"`), the CLI, the app and Blender.
- **Attribute Editor:** the **Inputs** section lists the other scalps with **Select** and
  **Remove**; the **Scalps** row's **Pick** adds one (its branch wired). Core: `GroomData::scalps`,
  `GroomSession::setScalps`, `scalp_pieces.h`.

### Reference node (groom libraries)

A **Reference** node (Graph Editor > Add Node > Sources) brings in another groom file's result: a
library look ("eyebrows.groom", "fur_patch.groom") reused, or instanced several times, in a
groom. Merge it with the groom's strands (a Merge node), or feed its `guides` output to an
Interpolate.

- **`path`:** the `.groom` file (relative paths resolve like a map's file: the scene's folder,
  then the project; a groom file's folder, then its search paths). It is read again when the file
  changes. The referenced groom is evaluated at the groom's own preview density, time and
  viewport or render setting, with its own painted maps and paths.
- **Outputs:** `strands` (its output) and `guides` (its guide hierarchy, posed and corrected,
  at its deepest level, and its guide curves).
- **`bind`:** `world` keeps them where the file has them (not rooted on this scalp: they do not
  follow it); `closestPoint` roots each strand at the closest point of this groom's scalp (or the
  mesh on the node's `scalp` input: one of several scalps), turned with the surface; `uv` roots it
  at the point with its UV in the file (both scalps sharing a UV layout), turned with the surface
  and its UVs. Rebound strands follow the scalp like the groom's own.
- **Errors:** a missing file, or one that references itself, is an error naming the file; no
  `path` is a note. Instance prototypes and card bakes stay the library's (use bind `world`).

### Transfer to a new scalp

Move a groom onto another mesh: a new sculpt, a retopologized head, or another character. It is
one undo step:

```python
missing = groomlab.transfer_groom(groom, "newHead")            # closest point, painted maps projected
groomlab.transfer_groom(groom, "bigCat", fit=True)             # a scalp elsewhere or of another size
groomlab.transfer_groom(groom, "newHead", maps=False)          # maps left as they are
groomlab.transfer_groom(groom, "newHead", color_sets=True)     # also the color sets the graph reads
groomlab.transfer_groom(groom, "newHead", method="markersWarp", # by marker pairs: old point, new point
                        markers=[("head.vtx[12]", "newHead.vtx[40]"), ...])
groomlab.transfer_groom(groom, "newHead", method="uv")          # both heads laid out alike in UV
```

**Transfer methods** (`method=`, the dialog's **Method**): where each guide root, part point, flow
arrow and map texel of the old scalp lands on the new one.

| Method | For | How |
|---|---|---|
| `closest` (default) | the same head, resculpted or retopologized, in place | the closest point of the new scalp |
| `bounds` (`fit=True`) | the same head elsewhere or of another size | the old scalp's bounding box moved and scaled (uniformly) onto the new one's, then the closest point |
| `markersAlign` | a head elsewhere, **turned**, of another size | 3 or more marker pairs give the best move, turn and uniform scale (least squares: Horn's method with Umeyama's scale), then the closest point |
| `markersWarp` | a head of **other proportions** (wider, longer, another character) | 4 or more pairs spread over the head (not on one plane) bend space smoothly so each old marker lands on its new one (a polyharmonic spline, kernel r³, with an affine part and a small smoothing term so close markers do not blow up), then the closest point. Fewer pairs, or pairs on one plane, fall back to align with a warning |
| `uv` | two heads with the same UV layout | each root found at its UV on the new scalp; roots without a UV match go to the closest point (counted in a warning) |

- **Markers:** a pair is a point of the old scalp and the same point of the new one (a vertex, or
  the middle of an edge, a face or a few components: `("head.vtx[12]", "newHead.f[40]")`, world
  points `((x, y, z), (x, y, z))`, or stored sides). Spread them over the head: forehead, temples,
  crown, nape, behind the ears. They are snapped onto both meshes (a face and barycentrics, so they
  follow a moved or deforming mesh, and the world point) and stored on the groom
  (`transferMarkers`; `.groom` files keep them as `"transferMarkers"`), so a transfer again reuses
  them: `markers=None` takes the stored ones. `groomlab.get_transfer_markers(groom)`,
  `set_transfer_markers(groom, pairs)` and `snap_markers(groom, newHead, pairs)` read and write them.
- **Guide shapes:** with every method but closest a guide is carried by the mapping at its root: the
  similarity's turn and scale for align, the warp's local stretch made into a turn and a uniform
  scale (its polar decomposition: guides turn and grow, they never shear) for warp, the change of
  the two meshes' UV tangent frames for uv. Then it is set upright on the new surface.
  `keep_lengths=True` keeps guide lengths and tube radii (they turn, they do not scale).

- **Guides:** each primary and locked guide keeps its shape and is re-rooted at the closest point
  of the new scalp. The shape turns with the surface normal there. Children follow their parents,
  and IDs, selection and painted maps are kept.
- **Parts:** they are re-bound the same way.
- **Fit:** `fit` (the `bounds` method) first moves the old scalp's bounding box onto the new
  one's and scales it uniformly. Guide lengths and tube radii scale with it.
- **Connections:** the groom's `inMesh` is reconnected to the new mesh. The old rest mesh is
  disconnected, and the new scalp's `Orig` shape is connected when it has deformers.
- **Maps:** guide layers travel with the guides. Vertex and texture maps are **projected** onto
  the new scalp (`maps=True`, the default): each point of the new scalp is taken back through the
  method's inverse to the old scalp (the fit's or the markers' inverse then the closest point, the
  warp's inverse fitted as a second spline, or the same UV), and the map's value there becomes its
  value, so a
  map lands where the guides do whatever the two meshes' vertices and UVs are. A vertex map gets
  values for the new vertices; a texture map gets a new image of the old one's size at the new
  scalp's UVs (gray or RGBA as before), written under the groom's masks folder with a free name
  (`<map>_2.png` when `<map>.png` is taken), and the map points at it. The old file is never
  written: other grooms, or the scene before the transfer, may read it. Undo puts the maps back
  and removes the new files. A map that cannot be projected (values that do not fit the old
  scalp, a missing image, a scalp without UVs) stays as it was, with a warning. With
  `maps=False` vertex maps keep the old vertices' values (they fit only a scalp of the same
  vertex count) and texture maps their image, read at the new scalp's UVs.
- **Color sets:** Maya color sets of the old mesh that the graph reads by name (a
  `color_set_map`, or a Map node naming one) stay with that mesh unless `color_sets=True`: then
  each is copied onto the new mesh with Maya's **Transfer Attributes** (closest point, after the
  same fit, or the markers' best move, turn and scale (a warp's bend is not followed); by UV for
  `uv`), and the new mesh's history is baked so nothing stays live. A new scalp with deformers
  is left alone (baking would bake its rig too): its color sets are reported to copy by hand.
- **What comes back:** `transfer_groom` returns what could not be carried: maps left behind and
  color sets the graph reads that the new scalp lacks. `report={}` also fills in `projected`
  (the maps projected) and `colorSets` (the color sets copied).
- **When to run it:** at the bind pose, since both meshes are read at the current frame.
- **Not moved:** connected guide curves and simulation curves stay where they are. Re-create
  them afterwards (you get a warning).
- **In Maya:** select the new mesh and click **Pick** on the Scalp row of the groom's Attribute
  Editor (Inputs), or **Pick from Selection** on the Graph Editor's Scalp node. A groom with
  guides asks **Closest Point** or **Fit and Move**. By script, `groomlab.set_scalp(groom, mesh,
  fit=False)` does the same: it transfers a groom with guides and just connects an empty one.
- **Command:** `groomLabTransfer -scalp <mesh> [-method <m>] [-markers <json>] [-keepLengths]
  [-fitBounds] [-maps] <groom>`. `-method` is one of the five (`-fitBounds`: bounds); `-markers`
  takes the pairs as JSON (`[{"old": {"point": [x, y, z]}, "new": {...}}]`, a side may give a
  `face`, `u`, `v` instead), stores them snapped on the groom, and without it a marker method reads
  the stored ones. `-snapMarkers` with `-scalp` and `-markers` changes nothing and returns the
  pairs snapped. `-maps` projects the
  painted maps; it is off for the command (scripts say what they want), and the Python API
  passes it by default. `groomLabTransfer -mapsFrom <otherGroom> [-mapName <name> ...]
  [-fitBounds] <groom>` projects another groom's maps (the ones named, else all) from its scalp
  onto this groom's, adding them to its maps and changing nothing else (Copy Graph uses it).
- **From the menu:** GroomLab > Utilities > **Transfer Groom to Scalp...** opens a window, never
  modal, laid out like Wrap's: two views side by side, the groom's current scalp on the left and the
  **New scalp** (the dropdown: changing it reloads the right view) on the right. Each view is a
  Maya modelEditor with a hidden camera of its own (in a `groomLabTransferUI` group, kept out of
  the undo queue, deleted with the window), isolated to its mesh, with Alt navigation and a
  **Frame** button. Below: **Method**, **Symmetry X**, **Wireframe**, **Undo** / **Redo** /
  **Clear**, **Keep guide lengths**, **Project painted maps** (on), **Copy mesh color sets** (off),
  the status line (`6 pairs, 1 unmatched`, and why **Transfer** is disabled: align needs 3 pairs,
  warp 4) and **Transfer**, which says what moved, what was projected and what did not come along.
  The marker points (marker methods; with the others the views show the meshes only):
  - each side keeps its own ordered list: pair N is the Nth point on the left and the Nth on the
    right, placed in any order. Points draw as numbered locators (yellow left, cyan right) in a
    `groomLabTransferMarkers` group, each side's shown in its own view only; an unmatched point
    (its partner not placed yet) draws dim.
  - a click on a mesh adds a point at the end of that side's list (a ray from the view's
    `M3dView.viewToWorld` against the mesh: `MFnMesh.closestIntersection`, a face and
    barycentrics); a drag on a point slides it over the surface; **Ctrl+click** on a point deletes
    it and its partner at the same index, and both lists renumber; hovering a point lights it and
    its partner. The clicks reach the window through a Qt event filter on the two views only (the
    left button without Alt): Maya's current tool is never changed and the main viewport works
    as before.
  - **Symmetry X** also adds the mirror of each click (across x = the mesh's bounding-box center,
    snapped onto the mesh) as the next point on that side; the two are a group: deleting either
    deletes both, and their partners.
  - **Ctrl+Z** / **Ctrl+Y** (or Ctrl+Shift+Z) in the window undo and redo marker edits: the
    window's own stack, not Maya's.
  - the complete pairs are stored on the groom on every edit (`transferMarkers`, snapped onto both
    meshes); unmatched points are kept only while the window is open.

  The point lists are the core's `TransferMarkerEditor` (`transfer_markers_edit.h`, bound to
  Python as `groomlab_core.TransferMarkerEditor`), the same model the app's window uses. Scripts
  drive the window without the mouse: `w = transfer_window.show(groom, scalp="newHead")`, then
  `w.add_point("old", (x, y, z))` (snapped onto the mesh), `w.delete_point(side, i)`,
  `w.move_point(side, i, point)`, `w.undo()`, `w.redo()`, `w.set_new_scalp(mesh)`, `w.pairs()`,
  `w.pending()`, `w.set_method(...)`, `w.transfer()`. `menu.transfer_dialog(scalp=...,
  method=..., markers=...)` transfers at once without the window.
- **In the standalone app:** Tools > **Transfer Groom to Mesh...** opens the same window: the
  groom's scalp on the left, the mesh picked with **New Scalp...** (.obj, .abc) on the right, each
  view with its own orbit camera (Alt+left orbits, middle pans, right or the wheel dollies;
  **Frame**), the same point rules, **Symmetry X** and **Ctrl+Z** / **Ctrl+Y**, and **Import
  Markers...** for pairs in a JSON file (the format above). The views draw the meshes with the
  app's renderer and pick with the core's ray test (the brushes'). The complete pairs are stored
  with the groom as you edit and saved in its file.

### Utilities

GroomLab > **Utilities** moves hair in and out of plain Maya geometry and a look between grooms.
Every item works on the active groom (the pinned one, else the selected one); a dialog asks for
choices only when you click the item. Each is one undo step, and each is a function of
`groomlab.utils` that takes its choices as arguments:

```python
from groomlab import utils
made = utils.guides_to_curves(groom)                      # selection=True, group="primary", degree=3...
utils.curves_to_guides(groom, made["group"])              # edited curves back into their guides
utils.strands_to_curves(groom, node=None, max_curves=20000, attributes=True)
utils.strands_to_mesh(groom, style="tube", sides=4, width_scale=1.0, max_strands=20000)
utils.bake_guides(groom, "toGuides", group="baked")       # = groomlab.bake.bake_guides
utils.transfer_to_scalp(groom, "newHead", fit=True, maps=True, color_sets=False)  # transfer_groom, with a report
utils.copy_graph(groom, ["hair2", "hair3"], guides="keep", maps=False)
```

- **Guides to Curves:** a NURBS curve per guide, as authored (the shapes the brushes edit), under
  a group `<groom>_guideCurves` with a transform per guide group, nested as the groups are
  (`keep_hierarchy=False`: flat). The selected guides, else every guide (`group=`: one group's).
  Each curve's transform carries `glGuideId` and `glGuideGroup` (strings). Degree 1 by default:
  the CVs are the guide's points exactly; `degree=3` makes a cubic through them.
- **Curves to Guides...:** the selected curves (or their group) back into the groom. A curve
  tagged `glGuideId` whose guide still exists reshapes that guide in place: same ID, children
  following, its layers kept; the curve's edit points become the guide's points (a degree 1
  curve's CVs). Curves whose shape did not change leave their guide alone, so an untouched child
  keeps following its edited parent. Every other curve (untagged, an ID the groom lacks, a second
  curve with the same ID) becomes a new primary guide of the chosen root group. So: Guides to
  Curves, edit with any Maya tool (CVs, lattices, a sculpt deformer, a sim), Curves to Guides.
  By command: `groomLabGuides -reshape <id> <curve> ...`.
- **Strands to Curves...:** the evaluated strands (the groom's output, or any node's; a node
  giving guides works too) as a curve per strand under `<groom>_strandCurves`, for XGen, other
  packages or deformers. At most `max_curves` (20,000): beyond it strands are thinned by strand
  ID, the ones a lower preview density keeps, so the same curves come every time and a warning
  says how many were left out. With **Strand attributes** each curve's shape carries
  `glStrandId`, `glWidth` (a value per point) and the strands' channels (`glColorR`, `glClumpId`,
  `glMask`...: a number per strand channel, an array per point channel). The curves are made in
  one go through the API: 20,000 in about 4 s.
- **Strands to Mesh...:** the strands as one polygon mesh with the default shader: **tube**
  (`sides` around each strand, as wide as its widths times `width_scale`, the tip closed) or
  **ribbon** (a flat strip as wide as the strand, lying along the scalp: turned by the scalp's
  normal at the root and carried along the strand). UVs: u across, v root (0) to tip (1); **Per-
  strand UVs** adds a UV set `strandUVs` with every strand in its own cell of a grid. Capped and
  thinned like the curves (`max_strands`); 20,000 strands make a 320,000-face mesh in under a
  second.
- **Bake Node to Guides...:** a graph node's strands or guides (ToGuides, a Noise on guides, a Scatter...)
  become primary guides of a root group, to brush, mirror and give children (see Procedural
  guides). Also on a node's right-click menu in the Graph Editor: **Bake to Guides...**.
- **Transfer Groom to Scalp...:** see Transfer to a new scalp. The dialog's **Project painted
  maps** (on) and **Copy mesh color sets the graph reads** (off) are `maps` and `color_sets`.
  `transfer_to_scalp` returns the counts that moved (guides, parts, flow arrows), what was
  dropped, the maps projected (`projected`), the color sets copied (`colorSets`) and what stays
  with the old mesh (`missing`).
- **Copy Graph to Other Grooms...:** the active groom's graph (its look) and control values onto
  the other selected grooms, all in one undo step. **Guides:** `keep` leaves each groom's own;
  `replace` gives it the source's too: copied onto the same scalp or one of the same topology
  (bound to its faces, they sit on the same faces there), moved across with the transfer onto
  another (bounding boxes fitted; the target's parts and flow stay its own). **Maps** the graph
  reads by name that a groom lacks are reported; with **Copy painted maps** they come along:
  onto the same scalp or one of the same topology copied as they are (a vertex map's values, a
  texture map as a copy of its image), onto another scalp projected by position as the transfer
  projects them (bounding boxes fitted; `groomLabTransfer -mapsFrom`). New images go under the
  groom's own masks folder with a fresh name, so the grooms never paint into one file. The
  report lists `copied` and `projected` maps per groom. A guide layer comes only with replaced
  guides.
- **Find Missing Files...:** the files the selected grooms (else every groom) name that are not
  there (texture maps, images and Alembic files in the graph, the guide cache, the groom file)
  relinked to files of the same name in a folder you pick and its subfolders, in one undo step
  (`find_missing_files`; see A map's image). The Script Editor lists each file relinked, still
  missing or ambiguous; nothing is asked when nothing is missing.
- **Command:** `groomLabConvert` builds the curves and meshes (`-guideCurves`, `-strandCurves`,
  `-strandMesh`; see `hosts/maya/src/convert_cmd.h`).

### Groom files and the CLI

A groom file (`.groom`, JSON) holds everything a groom evaluates from: the graph, guide
hierarchy, correctives, parts, flow, painted maps, connected guides and the scalp, with its UVs,
color sets and rest pose, plus the last card bake. An animated scalp goes in an Alembic cache
next to it. It is the interchange between Maya and the standalone app (both open and save it),
and `groomlab-cli` evaluates groom files without Maya, for farms and other hosts:

```python
groomlab.export_groom(groom, "C:/shots/010/hair.groom", scalp_cache=True, start=1, end=48)
# -> hair.groom and hair.scalp.abc (AbcExport of the scalp, world space)
```

```bash
groomlab-cli info hair.groom
groomlab-cli eval hair.groom -o hair.abc -f 1-48 --density 1
groomlab-cli eval hair.groom -o hair.usda -f 1-48 --step 0.5
```

- **Matching:** the CLI matches Maya's own export frame for frame, subframes included, bit for
  bit on one machine: `groomlab-cli hash file.abc` prints an exact hash per frame of the strand
  IDs, points and widths (and one of the file), and the pipeline test compares the two exports
  by it. Across platforms or compilers the strands agree within a small tolerance (`pow`, `sin`
  and the like may differ in the last bits).
- **Units and up axis:** the file records its scene unit (`"units": "cm"` by default) and the
  world's up axis (`"upAxis": "Y"`, or `"Z"` from a Z-up scene) so another host knows what the
  scalp and guides are in. Maya and the app are Y-up; a Z-up file comes in turned a quarter
  about X (the scalp, its rest pose and the guides; world directions in the graph's params, a
  Gravity's or a Wind's, are not turned). Only centimeter files import into Maya.
- **Subframes:** between cache samples the scalp is blended linearly. Cache subframes (`step`)
  when the motion between frames matters.
- **Texture maps:** a painted texture map's image is named relative to the groom file (where
  every reader and the app look first); other files the graph names (texture params,
  `ImportCurves`) keep their paths, resolved through the file's search paths (the project and
  scene folders), so they must be reachable from where the file is evaluated.
- **Card bake:** a groom with a bake writes it beside the file as `<name>_cards/<name>_*` (the
  TGA maps, the atlas and a preview: the app's layout) and names it with its stamp
  (`"cardBake"`), so the app and an import keep the cards' atlas cells and the stale check.
- **Flat guides:** connected guide curves are stored at the exported frame. They don't animate
  in the file.
- **Cache paths:** paths are stored relative to the groom file when the caches sit beside it.
  The scalp cache is written with its UVs and color sets, so an import takes its mesh as the
  scalp.
- **Command:** `groomLabExportGroom -file f [-scalpCache abc -scalpObject name]` writes the file
  alone.
- **Older files:** a `"license"` field written by versions before GroomLab was free is ignored
  on load.

#### Importing and reloading groom files

GroomLab > Groom Files > **Import Groom File...** (`groomlab.import_groom`) makes a groom from a groom file,
one Maya exported or the standalone app saved; **Reload from Groom File**
(`groomlab.reload_groom`, also the Attribute Editor's **Reload** beside the linked file) brings a
file's edits back into the same node; **Save to Groom File** (`groomlab.save_groom`, the
Attribute Editor's **Save**) writes the groom back to the file it is linked to. Together they
make the round trip Maya -> app -> Maya:

```python
groom = groomlab.import_groom("C:/shots/010/hair.groom")   # a new groom (and its scalp)
groomlab.save_groom(groom)                                 # back to hair.groom, for the app
groomlab.reload_groom(groom)                               # the app's edits, one undo step
```

- **What comes in:** the graph (nodes, params, ramps, maps on params, bypass, param inputs,
  controls with their values), the guide hierarchy with its groups and layers, correctives
  (each layer's weight on the groom's transform, keyed as the file's weight curve), parts, flow,
  followScalp and the painted maps: vertex maps into the groom's `scalpMaps`, a texture map's
  image copied into the scene's masks folder for the groom (linked in place when it is that
  folder's already, as after a save and a reload). The card bake the file names is copied into
  the groom's cards folder, its stamp kept: the cards keep their atlas and are not stale.
- **The scalp:** a mesh passed (or selected) must be the file's scalp: as many vertices and the
  same faces, since the guides are rooted on its triangles. Another mesh is refused with the
  reason; import without one (the file's scalp is made, with its UVs and color sets, and a
  hidden rest mesh when it has a rest pose) and move the groom with Utilities > **Transfer
  Groom to Scalp**. Reload checks the groom's own scalp the same way and never touches it.
- **Animation:** the file's guide cache poses the groom (`guideCache`); its scalp cache is read
  with AbcImport (under `<name>ScalpCache`) and its mesh is the scalp when it is the file's,
  else it drives the scalp through a world-space blendShape. AbcImport does not undo: undoing
  an import leaves what it read, as File > Import does. Reload leaves caches alone.
- **The link:** the groom's `groomFile` names the file (as for Rendering from a groom file);
  `groomlab.groom_file(groom)` resolves it. `save_groom` keeps naming the scalp cache the file
  named before.
- **Exactness:** an exported groom imported into a new scene evaluates bit for bit as before
  (the same CLI hash), the cache's frames within float rounding; a groom the app saved
  evaluates in Maya exactly as the app evaluates it, and the other way round.
- **Command:** `groomLabImportGroom -file f` with `-info` (what the file holds, as JSON),
  `-checkScalp mesh`, `-makeScalp [-rest]`, or a groom to apply the file to (one undo step).

### Guides from Alembic

```python
groomlab.import_guides_from_alembic(groom, "C:/cache/guides.abc", object_name=None, samples=24)
```

This swaps the graph's `guides` node for an `ImportCurves` node and roots the curves on the
groom's scalp, the same way connected curves are rooted:
- **Which curves:** every curve in the file, or only those under `object_name`. Parent
  transforms in the file are applied.
- **Animation:** animated files are read at the scene time. Between samples positions blend
  when the curves match; otherwise the nearest sample is used. To pose a hierarchy with
  simulated guides instead, see Simulation below.
- **Curve types:** linear curves come in exactly. Cubic curves, such as those from `AbcExport` of
  degree-3 Maya curves, are read as their control points.
- **Caching:** rewriting the file invalidates the cache, because the file's timestamp is part of
  the cache key.

XGen guides work through Maya curves: convert them with XGen's guides-to-curves tool and
connect them, or `AbcExport` them.

Guide curves connect to `guideCurves`. Whichever curve end is nearer the scalp
becomes the root. The scalp and guides are read in world space, including their
transforms. Keep the groom's own transform at identity.

### Simulation

Simulated guides pose the groom's hierarchy guides by ID. Children re-follow their posed parents,
so simulating the primary guides moves the whole groom. The authored guides (`guideData`) never
change; remove the simulation and the groom is as groomed.

In Maya, with nHair or any curve deformer:

```python
from groomlab import sim

made = sim.create_sim_curves(groom, "primary")   # a curve per guide, made dynamic (nHair), connected
# ... play the scene from the start frame ...
sim.remove_sim(groom, made)                      # back to the authored groom, nodes deleted

made = sim.create_sim_curves(groom, "primary", preset="long", collide=True, preroll=20,
                             wind=((1, 0, 0), 3.0, 0.2))
sim.bake_sim(groom, "shot010.guides.abc", made)  # cache the playback range, drop the live sim
```

- **Presets:** `fur`, `short`, `long`, `bouncy` and `wet` set the hair system's stiffness,
  damping, drag, mass and start-curve attraction, with attraction falling along the length
  (stiff roots, free tips). `apply_sim_preset(hair_system, name, **overrides)` and
  `configure_sim(sim, ...)` change them later.
- **Collision:** `collide=True` makes the scalp a passive collider (nRigid); `self_collide`
  turns on hair-to-hair collision.
- **Pre-roll:** the nucleus starts `preroll` frames before the playback range, so the hair has
  settled by the first frame. **Wind:** direction, speed and noise on the nucleus.
- **Baking:** `bake_sim` plays the sim from its start (pre-roll included), writes the posed
  guides over the playback range to Alembic, reads the groom from that cache and deletes the
  live sim: playback becomes fast and scrubbable, and renders and groom files use the cache.
  Shot sculpts stay on top of it (they are not baked into the cache).

- **Curves:** `create_sim_curves` builds the curves from the guides at the current frame.
- **nHair:** `makeCurvesDynamic` attaches the follicles to the scalp. The simulated output
  curves connect to the groom's `simGuides`.
- **Other deformers:** `dynamic=False` connects the plain curves for any other deformer.
- **Custom curves:** `connect_sim_curves(groom, {guide_id: curve})` connects curves you already
  have.

With another package (Houdini, ...):

```python
sim.export_guides(groom, "guides.abc", "primary", start=1, end=100)  # strandId = guide ID
# ... simulate, keeping the strandId attribute ...
sim.set_guide_cache(groom, "sim.abc")
```

- **Guide caches:** a cache blends between samples, so motion-blur subframes are smooth.
- **Groom files:** `export_groom` writes the poses to `<name>.guides.abc` and references it, so
  `groomlab-cli` and the Arnold procedural render the simulated groom too. The pipeline test
  runs an nHair simulation and checks that the CLI matches Maya on the simulated frame.
- **Menu:** Simulate Guides (nHair, with a group and a preset), Bake Simulation to Cache, Remove
  Simulation, Export Guides for Simulation and Load Guide Cache are under GroomLab > Simulation.
- **Commands:** `groomLabExportAlembic` takes `-guideGroup <name>` (posed guides of a group) or
  `-guidePoses` (the poses alone). `groomLabGuides -guidePoints <id>` returns a guide's points.

Every saved groom writes all of its params and each node's version, so later changes of defaults
or of an operator's behaviour never change it (an outdated node offers an Upgrade); see
[DEVELOPING.md](docs/DEVELOPING.md), "Param specs, versions and saved grooms".

## Conventions

- The core targets C++17 and must not include Maya headers.
- Operators are pure. `evaluate()` depends only on params, context and inputs, so results can be cached.
- A node's cache key never includes the time: nothing reads it, and everything that changes with
  time (a deforming scalp, guide poses, correctives, controls, an Alembic file read at a time)
  arrives as a hashed external input or salts its own key (`Operator::contextSalt`). A static groom
  scrubbed on the timeline is a cache hit on every frame.
- `StrandSet` copies share their arrays until written (copy on write). An operator copies its
  input and writes only what it changes: shaping nodes share IDs, roots, widths and channels with
  their input, a node writing one channel shares the points. Read through the const accessors;
  write through `mutablePositions()` and friends, which clone the array once.
- One graph is evaluated by one thread at a time: evaluation plugs fields into operator params
  and records node times. Operators parallelise inside `evaluate()`.
- Every operator declares a `category()` (source, grow, shape, look, motion, color, feathers,
  field, utility) that
  `operatorSpecsJson` reports, so editors list new operators without a code change.
- An operator has one output (`output()`) unless it overrides `outputs()` (the primary first)
  and `evaluateOutputs()`, which makes every output from one run; the graph caches them together
  and a connection picks one by index (`Graph::connect(src, dst, slot, output)`, JSON
  `"output"`). `Graph::evaluate` gives the primary, `evaluateOutput`/`evaluateOutputs` the others.
- Randomness is seeded from stable strand IDs (`groomlab::Rng`, `groomlab::hash64`), never from thread order or time.
- Maya attribute short names use a `gl` prefix to avoid clashes with inherited shape attributes.
- The shape's draw classification must be `drawdb/subscene/...` (subscene override). A `drawdb/geometry/...` classification crashes VP2.
- Node type IDs sit in Autodesk's local range (`0x0007F100`). Register a real block before distributing.
- **No format migrations.** Stored formats (graph JSON, guide data, scalp maps, groom files)
  change in place and unknown fields are ignored on load; no schema upgrade steps, no Python
  upgrade shims. `schema.cpp` only refuses documents from a newer GroomLab. Update tests,
  goldens, fixtures and presets with a format change.
- A scene's files (texture maps, card bakes, caches, exports) live in its `groomlab/<scene>`
  folder and are stored relative to it, named by the groom's `asset_name` (never a short name);
  new features that write files use `groomlab.asset_folder(kind)`, `asset_name` and
  `portable_path`, read through the node's path resolver (`resolveNodePath`), never overwrite a
  file they did not make, and copy a file another groom names before writing it.
- Painted maps are one concept (guide layer, vertex map, texture) behind `groomLabScalpMaps`
  and the Paint tab; nothing else should reach for Maya's paint tools or color sets.
- Never assign a null texture to a Maya shader parameter, and never free a vertex buffer a
  render item still references: both crash inside Maya.
