# Conventions

The rules the code keeps.

User guide: [Developer overview](https://www.getgroomlab.com/docs/developers).

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
