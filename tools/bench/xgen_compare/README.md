# GroomLab vs XGen Interactive Grooming: a reproducible benchmark

A side-by-side timing of GroomLab and Maya 2022's XGen Interactive Groom Splines (IGS) on the same
scalp, strand count and session settings. They are different systems (IGS sculpts every spline
directly; GroomLab grows strands from guides through a node graph), so the harness times the steps
a user goes through in both, with the closest settings each allows, and records everything that
is not matched. Results: [results/](results/).

## What it measures

Every measurement runs in a fresh Maya process: `mayapy` for batch work (`xgc_batch.py`),
`maya.exe` for the viewport (`xgc_gui.py`). Systems alternate which goes first on each run.

| # | Measure | How |
| --- | --- | --- |
| 1 | Creation | From nothing to a fully evaluated groom at the target count: XGen `xgmCreateSplineDescription` + clump and noise modifiers + a forced evaluation; GroomLab `create_groom` + guides from curves + the graph + a forced evaluation. Cold = the first in the process (the plugin already loaded; its load time is reported apart), warm = deleted and made again. Process working set and private bytes before and after. In the GUI: to the first drawn frame. |
| 2 | Parameter tweak | Clump strength and noise amplitude changed, then evaluated (batch: forced evaluation) or drawn (GUI: `refresh -force`). 7 of each per run, median. |
| 3 | Brush dab | A 9-point stroke through the system's own interactive brush code, timed to the redrawn frame. GroomLab: `groomLabBrushCtx -debugStroke` (comb, on its guides); then per dab, as an artist feels a drag: the same strokes with the viewport redrawn after the press and every point (`-debugStroke "draw ..."`, at the stroke's `sculptDensity`), each step timed to its redrawn frame (`-debugStrokeTimes`: the press, the dabs' median, the release; the results JSON keeps the last stroke's per-step breakdown, `brush.steps_draw` from `-debugStrokeDraws`: the strands changed, the evaluation in compute, the node times and the viewport's upload). XGen: its brushes are Maya contexts that only take mouse events; `xgmBrushManip -stroke` is tried and reported (it does not move the splines), so only GroomLab has a number. |
| 4 | Viewport | Orbit: 60 frames of `orbit` + `refresh -cv -f` (draw only). Playback: Maya's `play -wait` over frames 1-50 of the deforming scalp, every frame, free-running; then stepping the frames with a forced redraw. Batch: per-frame evaluation without drawing. |
| 5 | Arnold | `arnoldExportAss` time and size; `kick` render (960x540, AA 3, diffuse/specular/transmission 2, the same camera, sky dome + key light, the same `aiStandardHair` on both) static and with 2-key motion blur at frame 25. kick runs with `-sl` (no license check: watermarked, timings count). |
| 6 | Alembic | 24 frames of the deforming scalp: XGen `xgmSplineCache -export`, GroomLab `groomlab.export_alembic`. Time and size. |
| 7 | Scene | Saved `.ma` and `.mb` size and save time, the files next to them, and opening the `.mb` again to an evaluated groom. |

GPU memory: the whole device's change around creation (`nvidia-smi`), for both systems.
Machine info (CPU, cores, RAM, GPU, power plan) and the machine's load before and after every job
(CPU %, free RAM, busiest processes) are in the results JSON.

## The setup

- **Scalp**: `xgc_common.make_head`: a radius-10 polySphere (160 x 100), cut below 15% of the
  radius under the equator, scaled (1, 1.1, 1.2): 17,440 triangles, 878 cm2, polySphere UVs.
- **Strands**: 100k and 500k, 16 points each (XGen `cvCount 16`; GroomLab Resample 15 segments,
  `even`), 15 cm long, straight along the normal before the modifiers (XGen's default hair;
  GroomLab's guides are straight along the normals too), 0.01 cm wide.
- **XGen chain**: base -> scale -> sculpt (the defaults) -> Clump (400 clumps: `pointDensity` =
  400 / area, `clump 0.8`, `curl 0.5`, clump ramp 0 at the root to 1 at the tip) -> Noise
  (`magnitude 0.5`, `frequency 1`, ramp 0 to 1) -> description. Modifiers are inserted as the
  Interactive Groom Editor does (`xgIgSplineEditorTreeModel.addModifier`), including the clump's
  `inMeshData` from the base's `outMeshData`: without it the clump modifier has no effect.
- **GroomLab graph**: Scatter (density = count / area) -> Interpolate (~500 guides, straight along
  the normals, about one per 1.6 cm cell) -> AutoClump (400) + Clump (0.8) -> Noise (0.5,
  frequency 1) -> Curl (radius 0.3, 0.5 turns/cm) -> Width -> Resample. `previewDensity 1` (the
  viewport shows every strand, as IGS does), guides not drawn. The static measurements also run
  `groomlab_nocurl`: the same graph without Curl, i.e. only the operators XGen runs here.
- **Deforming scalp**: a bend deformer keyed 0 -> 40 -> -40 degrees over frames 1-50-100 (made
  before the grooms, so GroomLab takes the rest mesh). XGen follows the mesh; GroomLab is timed
  as is and with `followScalp` on (`groomlab_follow`).
- **Session**: a `MAYA_APP_DIR` of the harness' own (the GUI one seeded like
  `tests/hosts/maya/trust_plugin_folder.cmake`: version stamp, no first-run screens, the build's plugin
  folder trusted, so no dialog appears; nothing changes SafeMode at runtime), the window pinned to
  1600 x 1000 with the perspective view alone (as `tests/hosts/maya/gui_support.pin_view`), smooth
  shaded, default lighting, no AO or multisampling, Evaluation Manager parallel, cached playback
  off. The build's plugin is loaded by path; the installed module is never used.

## Running it

Build first (`cmake --preset win-maya2022 -B build/wt`, then
`cmake --build build/wt --config RelWithDebInfo`), close other heavy programs, then:

```bash
"C:/Program Files/Autodesk/Maya2022/bin/mayapy.exe" tools/bench/xgen_compare/run_bench.py --runs 5
```

Options: `--counts 100000,500000`, `--parts batch,render,gui` (any subset), `--build <build dir>`
(default `build/wt`), `--maya <Maya folder>`, `--work <scratch folder>` (default
`%TEMP%/xgen_compare_work`: every job's log, args, result, scenes, exports and viewport shots),
`--tag <name>` (default today's date), `--keep` (keep every run's exports; by default only the
first run's are kept, 500k strands make GBs of Alembic), `--systems groomlab,groomlab_follow` (any
subset of `xgen,groomlab,groomlab_nocurl,groomlab_follow,groomlab_gpu`: a GroomLab-only run;
`groomlab_gpu`, GUI only, times the GPU look preview against the CPU in one session, every
tweak to its drawn frame with the GPU waited for: drags of Noise, Curl, Width, Clump and
Interpolate (each run from its node to the output), the first drag, a drag after a clump change (the
base uploaded again) and after a brush stroke (only the stroke's strands), the releases of a Noise
and a Clump drag, VRAM, the read-back error from Noise, Clump and Interpolate, frame comparisons,
and the brush strokes of the CPU systems with the preview on (each dab after the press drawn by the
GPU: `brush.gpu_stats`); the CPU systems run with the preview off, `GROOMLAB_GPU_PREVIEW=0`), `--draw-api DirectX11` (Viewport 2.0's engine
for the GUI jobs, set in the harness' prefs as an artist's prefs set it; fresh prefs give OpenGL
Core Profile), `--phases static` (`static`, `anim` or both). GroomLab's GUI jobs also record, per tweak and for ten stepped frames,
`groomLabGraphStats -drawTimes` (the evaluation in compute, the viewport's build and upload) in
the results JSON (`tweak.noise_draw`, `tweak.clump_draw`, `playback.stepped_draw`). A full run (5 runs, both counts) takes
about two to three hours and needs ~30 GB of scratch space.

It writes `results/<tag>.json` (every raw number, per job, with the machine and its load) and
`results/<tag>.md` (medians with min-max). `run_bench.py --summarize results/<tag>.json` rebuilds
the summary and the table from the JSON. The caveats below go with any table it makes.

Files: `run_bench.py` (driver: jobs, kick, summary), `xgc_common.py` (the scalp, the deformer, the
look, the two adapters with the same verbs), `xgc_batch.py` (mayapy worker), `xgc_gui.py` (Maya
GUI worker). The scripts avoid standard-library module names (a script named like one breaks
mayapy's startup).

## Caveats (read these with the numbers)

- **Different work.** IGS evaluates every spline through its modifier chain; GroomLab scatters
  roots, interpolates ~500 guides into every strand and runs its graph. GroomLab runs one more
  operator than XGen: IGS 2022 has no per-strand curl or coil modifier, and the clump modifier's
  `curl` had no effect on the output in our setup (checked by comparing spline positions and
  by eye), so XGen does no curl. The clump algorithms differ (IGS's clump map vs GroomLab's
  AutoClump centers), and so does noise (frequency units differ). The looks are close, not equal:
  at the same nominal strength GroomLab's clumps come out much tighter than IGS's (see the renders
  in `results/` and the playblasts in the work folder). `groomlab_nocurl` is the closer match in
  operators.
- **Renders differ in geometry, not only in speed.** MtoA's IGS curves are b-splines with 18
  points (16 CVs, ends doubled) and a constant radius; GroomLab's are linear with 16 points and
  taper to a fifth at the tip. Both are ribbons. Tighter clumps and curls change how much hair a
  ray crosses, so render time follows the look as much as the system.
- **Counts.** At 500k XGen's density makes 489k splines (2% fewer than asked); at 100k both make
  ~100k.
- **What "evaluated" means.** XGen: `dgeval` of the description's `outSplineData` (IGS computes
  on the pull; a second pull is free and the time grows with the count and the modifiers, so it is
  the real work). GroomLab: `getAttr outStrandCount` (the shape's compute at previewDensity 1).
  Neither includes drawing; the GUI numbers do.
- **XGen creation is dominated by the mesh.** `xgmCreateSplineDescription` took ~10 s on this
  17k-triangle scalp at any count (2.6 s on a 4k-face sphere, 40 s on a 32k-triangle sphere in a
  probe), so XGen's creation number mostly measures that setup, which happens once per groom.
- **GPU.** IGS uses OpenCL in the GUI (`xgmSplineQuery -isCLReady` is true there, false in
  mayapy) and reports its VRAM use; GroomLab evaluates on the CPU and uploads to Viewport 2.0.
  The working-set deltas do not count GPU memory.
- **Arnold.** MtoA writes IGS as an `xgenProcedural` holding the spline data, and the modifiers
  run at render time inside kick; GroomLab writes the evaluated curves into the `.ass`. So XGen's
  export is lighter and its render heavier: compare export + render. kick needs Maya's and XGen's
  `bin` folders on PATH for the XGen procedural (the driver sets it). Renders are watermarked
  (`-sl`, unlicensed): the timings still count.
- **Brushes.** Only GroomLab's brush can be driven by script; its dab acts on ~500 guides and
  re-evaluates the strands, while an IGS brush acts on the splines themselves. Not comparable as
  a pair; GroomLab's number is reported alone.
- **Machine variance.** This machine varies by up to +-30% between days and hours. Runs alternate
  the order of the systems, and min-max are kept; compare medians from one results file, not
  across files.
- **Defaults kept.** Each system's viewport draws with its own default look (XGen's tube shading,
  GroomLab's lit hair), and Alembic files are each system's own layout (XGen writes its spline
  cache schema, GroomLab curves with widths).
