# GroomLab in the browser (WebAssembly)

A proof of concept of the web runtime: the real core (`engine/core`), built to WebAssembly with
Emscripten, evaluating a `.groom` in the page and feeding the website's three.js viewer. The page
is `/view` (`web/app/(home)/view/page.tsx`): a sample groom, its sliders, a density slider,
**Download .groom**, and drag and drop of your own `.groom` (with its `.obj`, or a preset `.json`
with an `.obj`).

What runs is the desktop engine, not a port: the same sources, the same portable math. The
module's strands are **bit for bit** the desktop's (see "Checking it").

## Pieces

| Where | What |
| --- | --- |
| `engine/web/src/web_groom.{h,cpp}` | `WebGroom`: a .groom (or a preset + OBJ) loaded into a `GroomSession`, its sliders, edits, evaluation into flat buffers, saving. Plain C++, no Emscripten. |
| `engine/web/src/bind.cpp` | Its embind binding (the browser build). |
| `engine/web/src/web_probe.cpp` | `groomlab_web_probe`: the same `WebGroom` built natively, for comparisons. |
| `engine/web/tools/compare.mjs` | The module in Node, printing what the probe prints. `diff_positions.mjs` compares raw dumps. |
| `engine/web/tools/make_sample.mjs` | Writes `web/public/samples/manny_hair.groom` with the module. |
| `web/public/wasm/groomlab.{js,wasm}` | The build's output (copied there by the build). Committed, so the site builds without Emscripten. |
| `web/public/wasm/groomlab-worker.js` | A classic worker that loads the module and answers the page. |
| `web/lib/groomlab-wasm/index.ts` | The page's typed client of that worker. |
| `web/components/view/groom-view.tsx` | The `/view` page: viewer, sliders, open, download, drop. |

## Building it

Emscripten lives outside the repository, in `%USERPROFILE%\groomlab-deps\emsdk` (any folder works):

```
git clone https://github.com/emscripten-core/emsdk %USERPROFILE%\groomlab-deps\emsdk
cd %USERPROFILE%\groomlab-deps\emsdk
emsdk install latest
emsdk activate latest
```

Then, in a shell where `emsdk_env` has run (it sets `EMSDK` and puts `emcc` on `PATH`) and `ninja` is
on `PATH` (on Windows, Visual Studio's: `tools/dev/vsenv.ps1`, or
`Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja`):

```
cmake --preset wasm
cmake --build --preset wasm
```

The `wasm` preset builds the core and `engine/web` only (no CLI, tests, Python, C API, Alembic,
Maya, renderers, TBB) into `build/wasm` and copies `groomlab.js` and `groomlab.wasm` to
`web/public/wasm/` (`GROOMLAB_WEB_PUBLIC_DIR`; empty: no copy). A full build takes about a minute.
Then `node engine/web/tools/make_sample.mjs` writes the sample groom again, if wanted.

What it took to compile: nothing beyond the existing guards. The SSE2 paths fall back to their
scalar code (Emscripten does not define `__SSE2__` without `-msse2`), `engine/gpu` leaves itself
out off Windows, TBB is off (the serial backend, `parallel_serial.cpp`), and one cast in
`node_library.cpp` (Emscripten's libc++ counts file times in `__int128`). C++ exceptions are
WebAssembly exceptions (`-fwasm-exceptions`, top-level `CMakeLists.txt`, under `if(EMSCRIPTEN)`).

## The JavaScript API

The worker client (`web/lib/groomlab-wasm`), all calls async:

```ts
import { createGroomEngine } from '@/lib/groomlab-wasm';

const engine = createGroomEngine();            // starts public/wasm/groomlab-worker.js
const { controls, scalp, version } = await engine.loadGroom(groomText, objText?);
await engine.setParam('control:frizz', 2.5);   // or setParams([[id, value], ...])
const { strands, guides, evaluateMs, errors } = await engine.evaluate({ density: 0.25 });
viewer.setStrands(strands);                    // StrandData: count, offsets, positions, colors (sRGB)
const bytes = await engine.saveGroom();        // the edited .groom
```

- `loadGroom(groom, mesh?)`: a `.groom`'s text (its scalp is inside it), or a preset's graph JSON
  with an OBJ to grow on (fitted to the mesh as hosts fit presets). An OBJ given with a `.groom`
  replaces its scalp.
- `listControls()`: the sliders, `{id, kind, label, group, node, param, min, max, step, default,
  value, unit, description}`: first the **Control nodes** (`control:<name>`, their own min, max and
  default), then the **exposed params** (`param:<node>.<param>`, ranges and defaults from the
  engine's param specs): each Compound's exposed params; else the graph's `"exposed"` list (a
  preset's, kept when a preset is loaded); else a default pick of the look's key params (one plain
  number param per look node, Clump, Curl, Noise... first).
- `setParam(id, value)`: a Control value goes to `GroomSession::setControls`; a node param is written
  into the graph and set with `setGraphJson`, which keeps every node whose inputs did not change.
  Values sent while an evaluation runs are coalesced (the latest per slider).
- `evaluate({density})`: the preview density (default 0.25). Strands come back as typed arrays,
  transferred from the worker.
- `scalpMesh()`, `saveGroom()`, `dispose()`.

The embind class (`WebGroom`, in the worker or in Node: `createGroomLab()` from `groomlab.js`) has
the same calls synchronously (`load`, `controlsJson`, `setParam`, `evaluate`, `offsets`,
`positions`, `colors`, `guideOffsets`, `guidePositions`, `scalpPositions`, `scalpNormals`,
`scalpIndices`, `statsJson`, `saveGroom`, `graphJson`, `setGraphJson`, `bakeGuides`); its buffers are
views into the module's memory, valid until the next call.

## Size and speed (2026-10-21, Emscripten 6.0.13, -O3)

| | raw | gzip -9 | brotli 11 |
| --- | --- | --- | --- |
| `groomlab.wasm` | 3.95 MB | 1.48 MB | 1.03 MB |
| `groomlab.js` | 46 KB | 13 KB | 11 KB |
| `-Oz` instead (not used) | 2.35 MB | 0.95 MB | 0.72 MB |

`-Oz` is 1.4 to 1.7 times slower to evaluate. The sample groom (Hair basics on the Manny head,
25,843 strands at full density), single-threaded, in Node 24 on the development machine:

| density | strands | first evaluation (fresh page / warm) | melanin changed | clump strength changed | frizz changed |
| --- | --- | --- | --- | --- | --- |
| 0.25 | 6,442 | 250 ms / 117 ms | 3 ms | 13 ms | 12 ms |
| 1.0 | 25,843 | 560 ms / 282 ms | 10 ms | 54 ms | 47 ms |

Headless Chrome (SwiftShader) showed the same: 252 ms for the page's first evaluation, 19 ms after
four sliders moved. The native probe (MSVC, also single-threaded) takes 237 ms and 563 ms for the
fresh evaluations: WebAssembly runs here at about native single-thread speed. Edits after the first evaluation re-run only
the nodes after the one changed, so sliders stay live at full density. The home page's Try section
could run on it (its grooms are smaller); it stays procedural for now.

## Checking it

`groomlab_web_probe` is `WebGroom` built natively: configure any native preset with
`-DGROOMLAB_BUILD_WEB=ON` and build that target. Then compare:

```
groomlab_web_probe web/public/samples/manny_hair.groom --density 0.25 --dump native.f32
node engine/web/tools/compare.mjs web/public/samples/manny_hair.groom --density 0.25 --dump wasm.f32
node engine/web/tools/diff_positions.mjs native.f32 wasm.f32
```

On 2026-10-21 every case matched bit for bit (strand counts, point counts, a hash of every
position's bits): the sample at densities 0.25, 0.5 (with three sliders moved) and 1, the strokes
test groom, and Creature fur, Curly, Dragon scales and the head lookdev graph on their meshes. The
shipped `groomlab-cli` 0.24.0 (TBB on, multi-threaded) exporting the sample to USD
(`eval ... -o sample.usda -f 0 --density 0.25`, and 1) gives the same points to the bit: 328,542 and
1,317,993 floats.

## Limits

- **Single-threaded.** Threads need `-pthread` (Emscripten's pthreads on Web Workers, with the
  core's own pool backend, `parallel_pool.cpp`, instead of the serial one) and a page that is
  cross-origin isolated: served with `Cross-Origin-Opener-Policy: same-origin` and
  `Cross-Origin-Embedder-Policy: require-corp` (or `credentialless`), for `SharedArrayBuffer`. The
  static export would need those headers from its host (Vercel: `vercel.json` headers), and every
  cross-origin asset on the page then needs CORP or CORS.
- **No files.** Built with `-sFILESYSTEM=0`: texture maps, Alembic caches (scalp, guides,
  simulation bakes) and `ImportCurves` are not read; a node that needs one says so in the errors.
  Painted vertex maps, guides, parts, flow, sculpts and controls are inside the `.groom` and work.
- **No GPU look preview** (`engine/gpu` is Windows only); evaluation is the CPU engine.
- **Instances with prototypes of their own** (`"prototypes"` in a `.groom`) are not handed to the
  session yet; cards, feathers and scales draw as their strands only.
- **Memory**: up to 2 GB (`-sMAXIMUM_MEMORY`); grows as needed from 64 MB.
- The module is a build product committed to the repository (about 4 MB a build): rebuilding it
  rewrites the file, so commit it only when the engine changed.
