# Adding a node to GroomLab

A node is an operator in the core (`engine/core`): a pure function of its params, its inputs and the
evaluation context. Every host (Maya, the standalone app, Blender, the CLI, the C API) builds its
add-node menu, search, param panel and saved files from the core's operator specs, so a node added
to the core shows up everywhere with no host code. This page walks through adding one, testing it,
and porting it to the GPU preview later.

Read [DEVELOPING.md](DEVELOPING.md) section 6 ("Rules") first: no format migrations, scale
invariance, defaults written once, randomness from strand IDs, the CPU first. For the math helpers
and the expression language's builtins, see [MATH.md](MATH.md).

## 1. Make it with `new_node.py`

```bash
python tools/dev/new_node.py Frizz --category look            # --dry-run first to see the plan
```

`Frizz` is the node's type: what graph JSON stores, so it is never renamed once grooms use it.
`--category` is its add-node menu category, one of `nodeCategories()` in
`engine/core/src/node_catalog.cpp`: source, grow, shape, deform, collide, look, motion, feathers,
color, convert, field, utility, games. The script writes or edits:

| File | What it gets |
| --- | --- |
| `engine/core/src/operators/frizz_op.cpp` | The operator, copied from the template `tools/dev/new_node/example_op.cpp`: a working node that moves strands out from the scalp. You replace its params and `evaluate()`. |
| `engine/core/include/groomlab/builtin_operators.inc` | `GROOMLAB_OPERATOR(Frizz)` at the end of the `// [look]` section. That one line makes `op::kFrizz = "Frizz"`, declares `registerFrizzOperator` and has `registerBuiltinOperators` call it. Its place in the section is its place in the menu. |
| `engine/core/CMakeLists.txt` | The source file. |
| `tests/engine/core/test_golden.cpp` | A golden case, `{"Frizz", "op", ..., link("interp", "op", "strands")}`, fed the shared scene's interpolated strands. |
| `web/content/docs/node-graph.mdx` | `Frizz` in the Node categories table's Look row. |

It prints the next steps. The template is also built into the core tests as `TestExample`
(`tests/engine/core/test_example_node.cpp`), so it always compiles and works, and
`tools.new_node_dry_run` checks the script still finds every place it edits.

## 2. The operator

The generated file, trimmed to what you change:

```cpp
class FrizzOp : public Operator {
public:
    std::string typeName() const override { return op::kFrizz; }
    std::string category() const override { return "look"; }  // its role in a chain (below)
    std::vector<PortSpec> inputs() const override { return {{"strands", PortType::Curves}}; }
    PortSpec output() const override { return {"out", PortType::Curves}; }
    std::vector<std::string> externalInputs() const override { return {"scalp"}; }
    std::vector<ParamSpec> paramSpecs() const override { ... }  // section 3
    Value evaluate(const EvalContext& ctx, const std::vector<Value>& inputs) const override { ... }
};

void registerFrizzOperator(OperatorRegistry& registry) {
    registry.registerType(op::kFrizz,
                          {"look", "Frizz: short kinks near the surface of the hair.", "frizzy flyaway kinks"},
                          [] { return std::make_unique<FrizzOp>(); });
}
```

- **Ports.** `PortType::Curves` takes strands or guides and hands back the kind it took;
  `Strands`, `Guides`, `Mesh`, `Field`, `Curve` and `Value` are the others (`graph.h`). Optional
  inputs: `{"centers", PortType::Curves, true}`. Several outputs: override `outputs()` and
  `evaluateOutputs()` (the Map node does).
- **The `NodeInfo`** in `registerType` is what every host shows: the menu category, a line on what
  the node does (the top of its param panel and its search tooltip), search words its name does
  not say, and optionally a label (`"Linked Groom"` for `Reference`) and `hidden` (made by
  grouping, not from the menu). `Graph.NodeCatalogIsEveryHostsAddNodeMenu` fails for a built-in
  type with no description or an unknown category.
- **`category()`** is the node's role in a chain, not its menu category: presets look for where a
  look starts (`source`, `grow`), so most nodes return what they do (`shape`, `look`, `color`).
- **`externalInputs()`** names what the node reads from the host (`scalp`, `guides`, `parts`,
  `flow`, `maps`); the cache key hashes them. A node that reads the time salts its key
  (`contextSalt`), as Wind does.
- **`evaluate()`** returns new data and never changes its inputs: `std::make_shared<StrandSet>(*in)`
  shares the input's arrays until written, and `mutablePositions()` clones the points once before
  parallel writes. `requireStrands(inputs, op::kFrizz)` throws a `GraphError` the hosts show on
  the node when the input is not connected; `ctx.note("...")` shows a grey note for a setting that
  has no effect.
- **Speed.** `parallelFor` over strands; `RampAlong` evaluates a profile once per point index.
  For brushing, override `evaluateIncremental` to make again only the strands a dab changed
  (`deform_ops.cpp`'s `DisplaceOp` is a compact example); it must return exactly what
  `evaluate` does, bit for bit (`Incremental.*` checks).

## 3. Params

```cpp
std::vector<ParamSpec> paramSpecs() const override {
    return {
        ParamSpec{"amplitude", 0.05, "How far points move, scene units (mappable)", "length"}.soft(0, 0.5),
        ParamSpec{"frequency", 20.0, "Kinks along each strand"}.range(0, 200),
        {"profile", Ramp{{0.0f, 0.0f}, {1.0f, 1.0f}}, "Amplitude along the length, root to tip"},
        {"mode", std::string("soft"), "soft: smooth bends; sharp: kinks", "", {"soft", "sharp"}},
        ParamSpec{"amplitudeRandom", 0.0, "Each strand up to this share less, at random", "fraction"}.range(0, 1).in("Random"),
        notAnimatable({"seed", std::int64_t{0}, "Random seed"}).in("Random").adv(),
    };
}
```

- **The default is written once, in the spec.** Read with `params().getDouble("amplitude")`,
  `getInt`, `getBool`, `getString`, `getVec3`, `getRamp`, never with a fallback of your own
  (`ParamSpecs.*` checks every operator source).
- **Kinds** come from the default's type: `double` (float), `std::int64_t` (int), `bool`,
  `std::string` (with `choices`: a dropdown), `Vec3f`, `Ramp`.
- **Units.** A param that measures something says so: `"length"` and `"width"` (scene units),
  `"density"` (per area), `"perLength"` (a rate along a length). Presets and new nodes are fitted
  to the scalp's size by these (scale with the scalp), and the hosts show real units. Defaults
  are authored at a 20-unit scalp (`kDefaultsScalpSize`, a head in cm). `"angle"` (degrees),
  `"fraction"` (0-1) and `"color"` are for display. A param whose unit holds only in one mode
  says when (`unitsWhen`: Noise's frequency is per scene unit only in world space).
- **No scene-unit constants.** A threshold, epsilon or step is a share of a strand's length, a
  segment, a spacing or the scalp's size, never a number of scene units (`ScaleInvariance.*`).
- **Limits and the slider:** `range(lo, hi)` clamps (getters never return past it), `soft(lo, hi)`
  is only the slider's range. `in("Group")` and `adv()` place it in the panel; `when("mode",
  {"sharp"})` shows it only in that mode.
- **Mappable numbers.** Read a number through `ScalarField(params(), "amplitude", scalpOf(ctx))`
  and it takes a painted map, a texture, an expression or a plugged field node per strand (or per
  point: `perPoint()`, `atPoint`). Say `(mappable` in the description; `paramMappable` reads it.
  `VectorField` and `ColorField` do the same for vectors (`vectorExpr()`) and colors (`colorMap()`).
- **Keys.** A param that sets how many strands, points or cards there are, or where they go (a
  count, a density, a seed), is `notAnimatable`, so motion blur and caches stay valid.
- **World directions** say so (`.world()`): a groom turned to another up axis turns them with its
  points. `GroomFile.TurnsWorldDirectionsWithItsPoints` keeps a list of every such param: add
  yours there.
- **Naming** follows DEVELOPING.md's param naming: `strength`, `profile`, `xRandom`, `xProfile`,
  `direction`, `along`.
- **Saved grooms** store every param of every node. Renaming or removing a param breaks saved
  grooms and presets (`PresetsTest` runs `presets.check` on every shipped preset), so choose names
  with care. To change what an existing node makes, bump its `version()` and keep the old result
  behind `nodeVersion() < N` (DEVELOPING.md, "Node versions").
- **`affect`.** Nodes that reshape or recolor the same strands they take get the common `affect`
  param automatically (`takesAffect`, `node_common.cpp`): the graph mixes the result back toward
  the input by it. Do not add a blend param of your own.

## 4. Reading and writing strand channels

Channels are per-strand or per-point float arrays on the `StrandSet` (`strand_set.h`). Write one
under a name a param holds, and say so, so the graph can warn about overwrites and hosts can list
the channels a groom makes (`attributes.h`):

```cpp
// in paramSpecs():
ParamSpec{"channel", std::string("frizz"), "The strand channel it writes: each strand's amount"}.writesChannel(),
ParamSpec{"maskChannel", std::string(), "A strand channel to read as a 0-1 mask (empty: none)"}.readsChannel(true),

// in evaluate():
const std::string name = params().getString("channel");
std::vector<float>& amounts = out->addStrandChannel(name, 0.0f);      // or addPointChannel
const std::vector<float>* mask = in->strandChannel(params().getString("maskChannel"));  // null: absent
```

A channel always written under a fixed name is declared by overriding `channelWrites()`:
`return {{"frizz", ""}};` (RenderData writes `kOpacityChannel` this way). Channels flow to the
renderers as user data, to the viewport's color modes and to exports.

## 5. Testing

Every change ships with a test. A new node needs:

1. **A golden case** (generated). Give it params that show what the node does, then record it:

   ```powershell
   $env:GROOMLAB_UPDATE_GOLDEN = 1
   build/ninja-maya2022/tests/RelWithDebInfo/groomlab_tests --gtest_filter=Operators/Golden.MatchesStoredOutput/Frizz
   Remove-Item Env:GROOMLAB_UPDATE_GOLDEN
   ```

   Review `tests/data/golden/Frizz.json` and commit it. The test compares counts and IDs exactly
   and values within 1e-4, and checks one thread and many give the same strands to the bit.
   `Golden.EveryOperatorIsCovered` fails for a registered type with no case. Re-record a golden
   only for an intended change of output, never to make a test pass.
2. **Unit tests** of its behaviour in `tests/engine/core/` (an existing `test_*.cpp` or a new one
   added to `tests/CMakeLists.txt`): what each param does, edge cases, scale invariance where it
   measures lengths. `tests/engine/core/test_example_node.cpp` is a small example.
3. **The checks every operator gets for free**, which run once it is registered:
   - `ParamUse.*` (`test_param_use.cpp`): every param is read in some state where the UI shows
     it. A param read only by a host or the renderer goes in its `allowlist()` with who reads it.
   - `ParamSpecs.*`: defaults, ranges and units are sound.
   - `Robustness.*`: empty, degenerate and missing inputs give an error or empty strands, never a
     crash.
   - `Graph.NodeCatalogIsEveryHostsAddNodeMenu` and `Docs.NodeGraphListsEveryNode`: its menu info,
     its list line, its row in the website's table.
4. **Determinism and preset looks.** `PresetLooks.*` and `PresetDigests` hash every shipped
   preset's strands. A new node changes them only when a preset uses it. If you change an existing
   node's output on purpose, re-record with `python tools/dev/record_preset_looks.py` and review
   the diff; otherwise a change there is a bug.

Run the core tests with `ctest -C RelWithDebInfo -L core` in the build folder (or
`python tools/dev/test_changed.py` for the areas you touched).

## 6. Host UI: what is free and what is not

Free, in every host, from the specs: the add-node menu (category, order, label, search words), the
param panel (kinds, units, ranges, groups, `when`, advanced folding, dropdowns, ramps, mappable
fields, exposing a param as a port), ports and wiring, saving, undo, the C API and Python.

Per-type code exists only where a param needs a special widget, for example a file path's dialog
filter (`FILE_PATH_FILTERS` in `hosts/maya/scripts/groomlab/ui/graph_editor.py`), and a few hand
lists that are judgment calls rather than metadata: the first-groom card's "look" nodes
(`LOOK_TYPES`, `hosts/maya/scripts/groomlab/ui/palette.py`) and the params a preset inserted as a
Compound shows first (`exposePriority`, `engine/core/src/presets.cpp`). Add your node there only if
it belongs.

## 7. Making it GPU

The GPU preview (`engine/gpu`) runs the end of a graph as compute kernels while a param is dragged
or guides are brushed. It hands any node it does not know to the CPU, so a new node works as soon
as it lands on the CPU. Porting it is its own later change, with tests comparing the two
(`python tools/dev/new_node.py Frizz --gpu --dry-run` prints this list):

1. **The kernel**: a `[shader("compute")] FrizzCS` in `engine/gpu/shaders/look.slang` matching the
   CPU's result (its math helpers: MATH.md).
2. **`engine/gpu/include/groomlab/gpu/look_kernels.inc`**: `GROOMLAB_KERNEL(Frizz)`. That line makes
   `LookEngine::Kernel::Frizz`, its name, both backends' tables (DirectX 11 and OpenGL) and the
   CMake build that compiles and embeds it.
3. **`engine/gpu/include/groomlab/gpu/look_stages.inc`**: a row
   `GROOMLAB_STAGE(Frizz, "frizz", "amplitude", "", "profile", "", "affect amplitude frequency seed")`:
   `StageKind::Frizz`, its name, the params its two mapped fields read, and the ramp, vector and
   number params a drag may set. List only params the kernel matches the CPU on: a param left out
   sends the drag to the CPU, so the GPU never draws a value it ignores.
   `GpuPreview.StageTableNamesRealTypesAndParams` checks every name there is a real param of that
   kind.
4. **The stage's code**: `readLookParams` in `engine/gpu/src/look_chain.cpp` (its constants and
   fields) and the inputs it uploads; `kernelsOf` and its dispatch in
   `engine/gpu/src/look_engine.cpp`.
5. **Tests**: a case in `tests/engine/gpu/test_look_engine.cpp` comparing the GPU with the CPU, run
   on both backends (`ctest -C RelWithDebInfo -L gpu`).

## 8. Docs

- `web/content/docs/node-graph.mdx`: the Node categories table row (generated; checked by
  `Docs.NodeGraphListsEveryNode`), and a line in "Other main nodes" or a section of its own for a
  node with params worth explaining.
- `docs/reference/nodes.md` (its category line, and a row in the operators table) or the
  reference file of its area (`docs/reference/README.md` lists them), and `docs/USER_GUIDE.md`
  where the node fits a workflow.
- The release notes.
