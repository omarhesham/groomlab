# GroomLab

A hair, fur and feather system for Maya, a standalone app and Blender. It pairs Tonic-style
hierarchical guides (guides, then child guides generated inside their parents' tubes) with a
Yeti-style procedural graph (scatter, interpolate, multi-level clump, noise, curl). The grooming
logic lives in a host-independent C++ core; the Maya plugin, the app, the Blender add-on, the
Python module, the C API and the headless CLI are thin layers over it, and one `.groom` file opens
in all of them.

- **Guides and brushes:** guide hierarchies of any depth, tube guides, comb, smooth, lift, twist,
  curl, cut and more, symmetry, guide selection, parting, correctives, grooming from reference photos.
- **Procedural look:** a node graph with clumping, noise, curl, frizz, braids, wet hair, color
  patterns and hair color, painted or texture maps on any mappable setting, expressions, fields,
  compound and script nodes, and presets.
- **Fur, feathers and scales:** flow arrows for short fur, feathers grown from shafts and layered,
  scales and meshes instanced on strands.
- **Motion:** Wind and Jiggle without a cache, guide simulation, follow the scalp, Alembic guide caches.
- **Rendering:** Arnold (in Maya and through a procedural for farms), RenderMan, V-Ray, and
  Alembic and USD exports for any other renderer.
- **Games:** hair and feather cards with LODs and baked maps, strands for Unreal, Unity and glTF.

Downloads: https://www.getgroomlab.com. GroomLab is free for any use (see License).

## Documentation

The user documentation is at **https://www.getgroomlab.com/docs**:
[installation](https://www.getgroomlab.com/docs/installation),
[quick start](https://www.getgroomlab.com/docs/quick-start),
[guides and brushes](https://www.getgroomlab.com/docs/guides-and-brushes),
[node graph](https://www.getgroomlab.com/docs/node-graph),
[painting maps](https://www.getgroomlab.com/docs/painting-maps),
[fur, feathers and scales](https://www.getgroomlab.com/docs/fur-and-feathers),
[rendering](https://www.getgroomlab.com/docs/rendering),
[game export](https://www.getgroomlab.com/docs/game-export),
[motion and simulation](https://www.getgroomlab.com/docs/motion-and-simulation),
[Python API](https://www.getgroomlab.com/docs/python-api),
[the app](https://www.getgroomlab.com/docs/app), [Blender](https://www.getgroomlab.com/docs/blender).

In this repository:
- [docs/reference/](docs/reference/README.md): the deep technical reference, every node,
  parameter and tool in detail, by topic.
- [docs/USER_GUIDE.md](docs/USER_GUIDE.md): the artist's walkthrough of the main workflows.
- [docs/DEVELOPING.md](docs/DEVELOPING.md): the developer handbook (how the code fits together,
  build, test, install, crash and hang diagnosis, the rules).
- [docs/EXTENDING.md](docs/EXTENDING.md): adding a node. [docs/SPEC.md](docs/SPEC.md): the spec
  and roadmap.

## Hosts and Maya versions

GroomLab runs in Maya (Windows), as the standalone GroomLab app (Windows, beta) and in Blender
5.2 (beta). One Maya package holds a build for each Maya version below, in one module: Maya picks
its own build from the module file's per-version sections.

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
that Maya's devkit and checked without Maya (they compile and link against that version's API,
import only that Maya's own DLLs, pass the Python tests under its Python, and the UI runs offscreen
under its PySide); nobody has run them inside that Maya yet. The Arnold procedural is one build
for every version (it needs Arnold 7 alone). How each is built: [docs/DEVELOPING.md](docs/DEVELOPING.md),
"Other Maya versions".

## Building from source

Requirements:
- Visual Studio 2022 on Windows (the v143 toolset is binary compatible with Maya 2022's v142), or
  GCC 9+ on Linux; CMake 3.25+.
- For the Maya plugin: Maya 2022 (headers and libs ship inside the install); the other versions
  build from their public devkits, no Maya install. Optional: MtoA for the Arnold translator,
  RenderMan Pro Server for the RenderMan procedural.
- Network access on first configure: FetchContent pulls pybind11, GoogleTest, nlohmann/json, Imath,
  Alembic and the Slang compiler.

The engine (core, Python module, CLI, C API, Blender helper) builds with no proprietary SDK. The
Maya plugin, the Arnold translator and procedural, and the RenderMan procedural build against your
own Maya install or devkit, MtoA and RenderMan Pro Server (`MAYA_LOCATION`, `MTOA_LOCATION`,
`GROOMLAB_RMANTREE`); their SDKs are not redistributable and are never in this repository. Step by
step: [BUILDING.md](BUILDING.md).

Quick start:

```bash
cmake --preset win-core              # the engine only (linux-core on Linux)
cmake --build --preset win-core
ctest --preset win-core

cmake --preset win-maya2022          # the Maya plugin and every test
cmake --build --preset win-maya2022
ctest --preset win-maya2022
```

Install the Maya module into a folder on `MAYA_MODULE_PATH`:

```bash
cmake --install build/win-maya2022 --config RelWithDebInfo --component GroomLab --prefix "C:/path/to/maya/modules"
```

The Ninja presets (several times faster), the test tiers (`ctest -L quick|gui|arnold`), golden
files, performance budgets, other Maya versions, Linux builds from WSL, the app and what to do when
Maya crashes or hangs: [docs/DEVELOPING.md](docs/DEVELOPING.md), section 2.

## Layout

`engine/` is the system, host-free; `hosts/` are the products built on it; `tests/` mirrors the two.

| Path | What |
| --- | --- |
| `engine/core/` | `groomlab_core`: strands, guides, the graph evaluator, operators, brushes, painted maps, cards, groom files, the USD writer. No host headers. |
| `engine/gpu/` | The GPU look preview (DirectX 11 and OpenGL compute, kernels in Slang). |
| `engine/io/` | Alembic readers and writers, `GroomScene` (a groom file with its caches). |
| `engine/python/`, `engine/capi/`, `engine/cli/` | The `groomlab_core` pybind11 module, the C API (`groomlab_c.h`, `groomlab.hpp`), the headless `groomlab-cli`. |
| `hosts/maya/` | The Maya plugin (`groomLabShape`, the brush tool, commands, the `groomlab` Python package); `arnold/`: the MtoA translator. |
| `hosts/arnold/`, `hosts/renderman/` | The Arnold and RenderMan procedurals: groom files rendered at render time. |
| `hosts/app/`, `hosts/blender/` | The standalone app (Qt 6) and the Blender add-on with its helper process. |
| `tests/`, `tools/`, `web/`, `docs/` | Tests and their data, packaging and dev tools, the website, the documentation. |
| `third_party/`, `cmake/` | Vendored `stb_image.h`; find modules and fetched dependencies. |

The full table: [docs/DEVELOPING.md](docs/DEVELOPING.md), section 1.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Adding a node: [docs/EXTENDING.md](docs/EXTENDING.md).
Security reports: [SECURITY.md](SECURITY.md).

## License

GroomLab is open source under the [Apache License 2.0](LICENSE) ([NOTICE](NOTICE)), at
https://github.com/thekesslereffect/groomlab. The Blender
add-on (`hosts/blender/addon/`) is under the GPL-3.0-or-later, as Blender add-ons must be.
Third-party components and their licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

GroomLab is free for any use, commercial included: every feature and every export (Unreal,
Unity, glTF, Alembic, USD, cards), no watermark, no key, no activation. It is funded by
donations: support it at https://ko-fi.com/getgroomlab (GroomLab > Help > Support GroomLab in Maya,
Help > Support GroomLab in the app, the GroomLab menu in Blender).
