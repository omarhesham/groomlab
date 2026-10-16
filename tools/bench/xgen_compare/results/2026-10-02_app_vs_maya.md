# The standalone app against GroomLab in Maya: 2026-10-02

Machine: AMD Ryzen Threadripper PRO 5945WX (12 cores / 24 threads), 128 GB, NVIDIA GeForce RTX
3090, quiet (nothing else running). The Maya columns are the committed runs (not re-run):
`2026-10-02_groomlab_5e8bc26` (OpenGL, medians of 3), `2026-10-02_gpu_phase2_dx11` (DirectX 11,
medians of 2) and the brush rows of `2026-10-02_brush_cpu_after_run1_{dx11,gl}` (in brackets
where they differ). The app columns are main `f9384e4` (partial uploads, the GPU look preview:
`app/src/gpu_preview.cpp`), Direct3D 11 and OpenGL through QRhi, at 1920x1080 device pixels:
medians of 3 runs, min-max in brackets (each run itself the median over the steps of a drag or
the dabs of a stroke). The live "before" column is main `520cf09` (the live benchmark before
this work) built in its own tree.

## The same scene

`tools/bench/xgen_compare/make_app_scene.py --count 100000 --out bench_100k.groom` (and 500000)
writes the Maya bench's scene as a `.groom` without Maya: the head (`xgc_common.make_head`: a
radius-10 sphere of 160 x 100 subdivisions cut below 15% of the radius under the equator, scaled
1, 1.1, 1.2: 8801 vertices, 17440 triangles, 878 cm^2), 508 primary guides straight along the
normals (one per 1.6 cm cell, 15 cm, tube radius 0.8; the vertex a cell's guide sits on may
differ from Maya's by a neighbour) and the graph (Scatter density = count / area -> Interpolate
-> AutoClump 400 + Clump 0.8 -> Noise 0.5 / 1 -> Curl 0.3 / 0.5 -> Width 0.01 -> Resample 15
even), at 100k and 500k strands of 16 points. The camera is Maya's persp (0, 22, 62) looking
down 18 degrees (`--look-at 0,1.7,-0.6 --distance 65.8 --orbit -30 -2`); the app's vertical field
of view is 40 degrees where Maya's 35 mm lens gives about 32 at 16:9. The stroke is the bench's:
a comb of radius 120 px and strength 1, nine points 40 px apart across the top of the head
(`--stroke-world` around (0, 10.5, 5)), drawn after the press and each dab; two strokes a run,
the second's rows reported (its press finds the stroke document of the first; the first
stroke's press also makes the GPU engine: about 40 ms more, 125 at 500k).

## The commands

With `QT_ENABLE_HIGHDPI_SCALING=0` (device pixels), `GROOMLAB_APP_RHI=opengl` for the OpenGL
columns, `--no-lookdev` for the line path, `--gpu-preview off|on` for the two columns
(`build/wt/bench_runner.py` in a build tree runs the whole matrix and `bench_report.py` prints
the medians; both are scratch files, not committed):

```
GroomLab bench_100k.groom --size 1920x1080 --look-at 0,1.7,-0.6 --distance 65.8 --orbit -30 -2 [--no-lookdev]
    --benchmark 20                                   creation (open + evaluate), the first frame, ms/frame
    --benchmark-orbit 60                             orbit fps
    --gpu-preview on|off --benchmark-drag noise.amplitude=0.35,0.65,7     (clump.strength=0.56,0.88,7,
        curl.radius=0.21,0.39,7, width.width=0.007,0.013,7, interpolate.radius=3,5,7)
    --gpu-preview on|off --brush mode=comb --brush radius=120 --brush strength=1
        --stroke-world "-1.8,10.5,5 -1.35,10.5,5 -0.9,10.5,5 -0.45,10.5,5 0,10.5,5 0.45,10.5,5 0.9,10.5,5 1.35,10.5,5 1.8,10.5,5"
        --stroke-world "..." --draw-dabs             the press, the dabs and the release to their drawn frames
```

The headless measures draw into an offscreen target and wait for the GPU after every frame (an
event query on Direct3D 11, glFinish on OpenGL), as Maya's `groomLabGpuPreview -sync` does: a
frame is the whole frame, as an artist sees it, without a window's vsync. The live measures
(`--live-benchmark 60 --live-mode param|param-commit|stroke|orbit|hover`, the real window, on
`tests/data/app/head_scalp.obj --graph tests/data/app/head_graph.json`, with and without
`--bypass collide`) are between frames on screen, quantized by the display's 16.7 ms, and print
the input's handling, `render()`'s CPU and the five slowest frames with what is left over
(presentation and the event loop) once those two are taken out.

## What each viewport draws

- Maya Viewport 2.0 draws the strands as lit lines (Kajiya-Kay through Maya's hair surface
  shader, per-point normals, tangents and bitangents, colors where they vary), no shadows.
- The app's line path draws lit lines (positions, normals, tangents, uvs: 44 bytes a point,
  no bitangent stream), the look the Maya rows have.
- The app's lookdev path draws camera-facing ribbons from float textures (32 bytes a point),
  Marschner shading, deep opacity maps for hair-on-hair shadows and the camera's transparency,
  a linear HDR multisampled target, tone mapping: a different, far heavier picture, whose frame
  alone is 46 ms at 100k strands and 186 ms at 500k at 1920x1080 on this card (the "a frame,
  nothing changed" row); every lookdev row is bound by that frame, not by the preview (a drag's
  kernels take about a millisecond there as on the line path). The lookdev frame's cost is the
  renderer's, outside this work.
- In the head test's graph Collide sits between Interpolate and Clump with two Combs: a
  stroke's dabs cannot run on the GPU there (it runs the graph from Interpolate to the output
  only through its own nodes), so the live stroke rows are the CPU's with either setting, and
  their cost is Collide's walk of the moved strands; the bench scene's graph has no such node.

## Headless: the bench scene, 100k strands

| Measure | Maya OpenGL | Maya DX11 | App D3D11 lines | App D3D11 lookdev | App OpenGL lines |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame | 0.20 s | 0.28 s | 0.19 s [0.19-0.41] opened + 0.07 s [0.07-0.07] first frame | 0.19 s [0.19-0.20] + 2.01 s [1.99-2.02] (pipelines, maps) | 0.19 s [0.19-0.20] + 0.12 s [0.12-0.12] |
| A frame, nothing changed | 4.05 ms (redraw) | 2.13 ms | 1.10 ms [1.09-1.13] | 46.0 ms [45.8-46.1] | 1.02 ms [0.92-1.07] |
| Orbit | 280 fps | 948 fps | 1221 fps [1219-1242] | 23.0 fps [23.0-23.1] | 1199 fps [1181-1227] |
| Noise tweak / drag to drawn frame, CPU | 26.3 ms | 32.2 ms | 37.3 ms [37.2-38.0] (evaluate 16.2, draw 20.8) | 96.1 ms [96.0-96.5] (16.3 + 79.6) | 82.0 ms [81.7-82.7] (16.4 + 65.0) |
| Clump tweak / drag, CPU | 32.0 ms | 37.4 ms | 42.7 ms [42.7-43.6] (21.6 + 20.8) | 101 ms [101-102] | 89.7 ms [87.0-90.6] |
| Curl drag, CPU | | | 30.0 ms [29.8-30.7] (8.8 + 20.8) | 89.9 ms [89.4-91.3] | 74.8 ms [72.6-75.2] |
| Width drag, CPU | | | 24.5 ms [24.4-24.9] (3.7 + 20.4) | 83.3 ms [83.2-84.7] | 70.0 ms [69.6-70.7] |
| Interpolate drag, CPU | | | 54.0 ms [53.9-54.7] (33.2 + 20.6) | 112 ms [112-112] | 106 ms [100-107] |
| Noise drag, GPU preview | 3.50 ms | 2.89 ms | 2.53 ms [2.50-2.61] | 52.5 ms [52.3-53.0] | 2.93 ms [2.76-3.05] |
| Clump drag, GPU preview | 4.08 ms | 3.30 ms | 2.93 ms [2.88-3.07] | 53.0 ms [53.0-53.3] | 3.08 ms [3.00-3.22] |
| Curl drag, GPU preview | 2.94 ms | 2.64 ms | 2.29 ms [2.24-2.41] | 54.0 ms [53.6-54.5] | 2.58 ms [2.40-2.63] |
| Width drag, GPU preview | 2.72 ms | 2.09 ms | 1.86 ms [1.85-2.02] | 52.3 ms [52.2-52.4] | 2.42 ms [2.41-2.50] |
| Interpolate drag, GPU preview | 6.06 ms | 5.24 ms | 4.64 ms [4.54-4.77] | 51.3 ms [50.9-52.6] | 5.54 ms [5.45-5.60] |
| Release of a Noise drag: CPU commit to drawn frame | 27.1 ms | 33.3 ms | 45.9 ms [44.8-46.5] | 99.2 ms [98.8-99.4] | 74.7 ms [69.2-79.6] |
| Brush press (first dab) to drawn frame | 4.95 ms | 157 ms (4.49 after run1) | 16.9 ms [15.8-16.9] | 43.9 ms [42.7-44.5] | 17.3 ms [17.1-17.4] |
| Brush dab while dragging, CPU | 13.6 ms (12.9) | 42.2 ms (12.9) | 9.19 ms [9.00-9.25] | 25.2 ms [25.1-25.4] | 8.74 ms [8.66-8.91] |
| Brush release + full view, CPU | 45.6 ms | 467 ms (42.9) | 41.6 ms [41.3-44.5] | 107 ms [104-107] | 87.8 ms [86.0-96.4] |
| Brush press, GPU preview on | 5.88 ms (19.5 phase 2) | 139 ms | 22.0 ms [22.0-22.3] | 50.2 ms [49.5-50.9] | 24.6 ms [24.2-24.8] |
| Brush dab while dragging, GPU preview | 3.44 ms (3.71) | 4.82 ms | 2.98 ms [2.91-3.01] | 7.03 ms [6.97-7.14] | 3.62 ms [3.51-3.89] |
| Brush release + full view, GPU preview on | 43.9 ms | 464 ms | 44.9 ms [43.7-44.9] | 108 ms [107-109] | 89.2 ms [86.2-91.5] |

## Headless: the bench scene, 500k strands

| Measure | Maya OpenGL | Maya DX11 | App D3D11 lines | App D3D11 lookdev | App OpenGL lines |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame | 0.49 s | 0.57 s | 0.36 s [0.35-0.37] + 0.23 s [0.23-0.35] | 0.36 s [0.35-0.36] + 2.39 s [2.35-2.51] | 0.36 s [0.35-0.37] + 0.30 s [0.29-0.30] |
| A frame, nothing changed | 5.03 ms (redraw) | 1.99 ms | 4.34 ms [3.96-4.37] | 186 ms [185-186] | 4.34 ms [4.32-4.42] |
| Orbit | 241 fps | 370 fps | 273 fps [270-282] | 5.70 fps [5.70-5.70] | 296 fps [277-297] |
| Noise tweak / drag, CPU | 108 ms | 136 ms | 181 ms [181-187] (77.9 + 103) | 451 ms [450-458] (78.2 + 372) | 246 ms [242-260] (78.2 + 162) |
| Clump tweak / drag, CPU | 134 ms | 165 ms | 208 ms [205-218] (104 + 103) | 490 ms [486-491] | 258 ms [253-263] |
| Curl drag, CPU | | | 143 ms [141-144] (39.6 + 104) | 416 ms [415-422] | 192 ms [189-247] |
| Width drag, CPU | | | 117 ms [117-117] (14.6 + 102) | 387 ms [386-388] | 176 ms [169-181] |
| Interpolate drag, CPU | | | 258 ms [257-260] (152 + 104) | 523 ms [522-528] | 316 ms [305-318] |
| Noise drag, GPU preview | 12.0 ms | 9.95 ms | 10.2 ms [10.2-10.2] | 219 ms [219-219] | 10.9 ms [10.8-11.0] |
| Clump drag, GPU preview | 13.3 ms | 11.2 ms | 11.7 ms [11.1-12.1] | 233 ms [233-234] | 12.5 ms [12.4-12.6] |
| Curl drag, GPU preview | 9.99 ms | 7.86 ms | 8.23 ms [8.14-8.92] | 222 ms [222-222] | 9.29 ms [9.29-9.30] |
| Width drag, GPU preview | 9.09 ms | 6.06 ms | 6.33 ms [6.32-6.87] | 217 ms [217-218] | 9.10 ms [9.08-9.34] |
| Interpolate drag, GPU preview | 24.1 ms | 21.0 ms | 20.2 ms [20.1-20.5] | 163 ms [162-163] | 23.5 ms [23.1-23.6] |
| Release of a Noise drag: CPU commit | 132 ms | 178 ms | 227 ms [226-231] | 485 ms [483-486] | 293 ms [274-323] |
| Brush press (first dab) | 9.66 ms | 881 ms (8.73) | 63.0 ms [63.0-65.9] | 161 ms [160-162] | 66.5 ms [65.1-67.1] |
| Brush dab while dragging, CPU | 36.6 ms (35.5) | 304 ms (35.5) | 24.9 ms [24.7-25.8] | 79.5 ms [78.6-80.0] | 24.8 ms [24.8-25.0] |
| Brush release + full view, CPU | 174 ms | 2816 ms (160) | 175 ms [174-177] | 494 ms [491-501] | 178 ms [174-180] |
| Brush press, GPU preview on | 11.5 ms (64.7 phase 2) | 763 ms | 93.2 ms [93.0-95.5] | 186 ms [185-187] | 115 ms [113-116] |
| Brush dab while dragging, GPU preview | 5.14 ms (4.61) | 8.40 ms | 8.42 ms [8.35-8.52] | 23.6 ms [23.1-23.7] | 9.67 ms [9.64-9.79] |
| Brush release + full view, GPU preview on | 180 ms | 2826 ms | 191 ms [189-193] | 515 ms [508-515] | 193 ms [192-202] |

## Live: the head test, 100k strands (real window, 1920x1080)

Before (main `520cf09`: a commit a frame in its param mode, no GPU preview, no dab without
guides) and after (main `f9384e4`: a slider's drag in its param mode, `param-commit` the old one,
guides placed for the stroke): the median time between frames on screen (handling / `render()`
CPU), medians of 3 runs with min-max.

| Mode | Before | After, CPU path | After, GPU preview |
| --- | --- | --- | --- |
| Param drag (noise.amplitude a frame), lines | 75.5 ms [73.7-76.6] (41.1 / 14.1) | 49.5 ms [49.1-49.6] (16.5 / 16.9) | 16.5 ms [16.4-16.6] (2.0 / 0.06) |
| Param commit a frame, lines | 75.5 ms [73.7-76.6] (41.1 / 14.1) | 50.2 ms [49.5-50.6] (16.3 / 17.1) | |
| Param drag, lines, Collide bypassed | | 49.4 ms [48.5-50.2] (16.3 / 16.9) | 16.4 ms [16.2-16.6] (1.9 / 0.06) |
| Stroke dab, lines, Collide on | no dab | 130 ms [128-133] (114 / 1.2) | 130 ms [128-131] (114 / 1.2), the CPU's |
| Stroke dab, lines, Collide bypassed | | 22.5 ms [22.3-22.7] (11.4 / 1.2) | 22.9 ms [22.3-23.2] (11.5 / 1.2), the CPU's |
| Orbit, lines | 16.5 ms [16.4-16.5] | 16.4 ms [16.0-16.5] | |
| Hover, lines | 16.5 ms [16.2-16.6] | 16.6 ms [16.5-16.7] | |
| Param drag, lookdev | 88.9 ms [88.2-90.2] (41.0 / 33.5) | 55.0 ms [54.2-55.9] (16.0 / 26.2) | 33.2 ms [33.2-33.3] (2.1 / 0.11) |
| Stroke dab, lookdev, Collide on | | 141 ms [140-142] (113 / 11.7) | |
| Orbit, lookdev | 33.0 ms [32.8-33.0] | 32.4 ms [32.1-32.8] | |

Spikes: in every live mode, before and after, orbit and hover included, two or three frames of
sixty take 220-290 ms (the p95 of most runs) with the handler and `render()` at 0 ms in them
(the five slowest frames: "the rest" holds it all), mostly in the first second after the window
appears; a 120-frame orbit run shows the same two. They are the window's presentation on this
desktop, not the evaluation or the uploads, and leave the medians alone; a rerun reproduces
them, so they are noted rather than filtered.

## Findings

- **GPU preview drags: the app matches Maya.** Noise 2.5 ms against Maya's 2.9 (DX11) / 3.5
  (OpenGL) at 100k, 10.2 against 10.0 / 12.0 at 500k; Clump 2.9 / 11.7 against 3.3 / 11.2;
  Curl, Width and Interpolate likewise within a few tenths. The same kernels run on the same
  device; the app writes them into vertex buffers it made for that (or the lookdev point
  texture), as Maya's preview writes its own, so nothing is copied. OpenGL costs 0.3-3 ms more
  a step (the driver-compiled kernels, the image barrier).
- **GPU preview dabs: the app matches Maya at 100k (3.0 ms against 3.4-4.8) and Maya's DX11 at
  500k (8.4 against 8.4; its OpenGL 4.6-5.1).** A dab's evaluation stops at Interpolate and the
  GPU blends the guides; what remains is the stroke document's guide evaluation and the
  frame.
- **CPU dabs: the app beats Maya** (9.2 ms against 12.9-13.6 at 100k, 24.9 against 35.5 at
  500k): the partial writes send only the changed strands' ranges (1-2 ms of `render()`) where
  Maya's DirectX 11 path sends whole buffers again.
- **CPU tweaks: the app trails Maya by 1.2-1.4x on the line path** (Noise 37 ms against 26-32
  at 100k, 181 against 108-136 at 500k). The evaluation is the same core (16 ms at 100k, 78 at
  500k); the difference is the whole write of the points and frames: 21 ms at 100k and 103 at
  500k for 44 bytes a point through QRhi, whose upload batch copies the data once more than
  Maya's acquired buffers, then the driver copies it again. A dab avoids all of it; a tweak of
  a node the GPU runs avoids it through the preview (one vsync period live, 2-5 ms headless).
  The release of a GPU drag pays the same whole write (46 ms against 27-33 at 100k).
- **The first dab of a stroke (the press) trails Maya** (17 ms against 5-6 at 100k, 63 against
  9-12 at 500k): the stroke document is evaluated at the stroke's density and uploaded whole;
  Maya's is warmed in idle time between strokes, which the app does in brush mode between
  strokes but not in a headless run.
- **Live, in the window:** a param drag went from 75.5 ms between frames (a commit a frame) to
  49.5 on the CPU path (three vsync periods: 16 ms of handling, 17 of `render()`, the driver's
  copies and the rounding to vsync) and to one vsync period (16.5 ms, 2 ms of handling) with
  the GPU preview, against Maya's 26-37 ms tweak and 3-5 ms drag (Maya's bench measures to the
  redrawn frame without a display's vsync). A stroke dab on the head graph is Collide's (114 ms
  of the 130; 11 ms with Collide bypassed, 22.5 ms between frames) on either setting: the GPU
  cannot start at Interpolate past Comb and Collide, as in Maya.
- **The lookdev path is bound by its own frame** (46 ms at 100k, 186 at 500k at 1080p: two
  ribbon passes over 1.6 M or 8 M points at 4x MSAA, the camera's deep opacity map each
  frame): its drags and dabs cost that frame plus the maps' redraw, whatever the preview does
  (its kernels take 1 ms there too). Its creation also pays about 2 s of first frame (the
  pipelines and the surfaces' tables). Making that frame cheaper (fewer passes, a lower
  `resolutionScale`, no multisampling while interacting) is the renderer's next work.
- **Orbit and a still frame:** the line path draws a 100k frame in 1.1 ms (1200 fps headless;
  vsync-locked live), a 500k one in 4.3 ms (273 fps), against Maya's 280-948 and 241-370 fps.
- **Creation** (open and evaluate, 0.19 s at 100k, 0.36 at 500k, plus a 0.07-0.23 s first
  frame on the line path) is at Maya's 0.20-0.28 / 0.49-0.57 s.

## Interactivity pass

The same machine and scenes, the same commands, after four changes (the branch's commits
`6345808`, `d4c9873`, `827f589`, `5953ccf`): the lookdev's interactive frame shaded once per
point instead of once per overlapping fragment (`hair_lookdev.frag` under `HAIR_POINT_SHADE`,
the ribbons Gouraud-lit from the shaded points); an adaptive interactive quality (the Render
tab's "Interactive quality", `--interactive-quality adaptive|full`: a stream of changes draws
reduced into the targets' corner, the full picture once nothing moves); the whole strand
writes straight through the device (`UpdateSubresource` on Direct3D 11, the positions from the
strands' own array, the lookdev textures from the padded buffers) instead of through QRhi's
upload batch and its copy, and the stroke document warmed before a scripted stroke as the
window warms it between strokes; and the scalp's closest-point query without the branches
that cost it (an SSE box distance, each stack entry carrying its distance). The "before"
columns are this file's tables above (main `f9384e4`); "after" is medians of 3 serial runs on
a quiet machine, min-max in brackets, Direct3D 11. The Maya columns are the committed ones.

### Headless, 100k strands

| Measure | Maya OpenGL | Maya DX11 | App before | App after |
| --- | --- | --- | --- | --- |
| Lookdev: a frame, nothing changed (interactive) | | | 46.0 ms | 17.1 ms [13.3-23.3] (the refine frames 55.7 [46.8-59.6], their path untouched) |
| Lookdev: orbit, full size | | | 23.0 fps | 85.2 fps [84.7-93.6] |
| Lookdev: orbit, adaptive | | | 23.0 fps | 159 fps [151-164] |
| Lookdev: Noise drag, CPU | | | 96.1 ms | 69.9 ms [63.2-73.5] full, 86.3 [57.5-87.5] adaptive |
| Lookdev: Noise drag, GPU preview | | | 52.5 ms | 18.4 ms [18.4-18.8] full, 16.3 [14.4-19.2] adaptive |
| Lookdev: brush press, CPU | | | 43.9 ms | 28.5 ms [28.3-30.5] |
| Lookdev: brush dab, CPU | | | 25.2 ms | 25.0 ms [24.4-26.7] |
| Lookdev: brush release + full view, CPU | | | 107 ms | 71.2 ms [70.4-71.6] |
| Lookdev: brush dab, GPU preview | | | 7.03 ms | 6.96 ms [6.68-7.12] |
| Lines: a frame, nothing changed | 4.05 ms | 2.13 ms | 1.10 ms | 1.07 ms [1.02-1.08] |
| Lines: Noise tweak / drag, CPU | 26.3 ms | 32.2 ms | 37.3 ms (16.2 + 20.8) | 27.7 ms [27.6-29.0] (the upload's CPU side 7.5) |
| Lines: Clump tweak / drag, CPU | 32.0 ms | 37.4 ms | 42.7 ms | 34.2 ms [33.1-35.5] |
| Lines: Noise drag, GPU preview | 3.50 ms | 2.89 ms | 2.53 ms | 2.61 ms [2.53-2.68] |
| Lines: release of a Noise drag, CPU commit | 27.1 ms | 33.3 ms | 45.9 ms | 34.0 ms [33.9-34.3] |
| Lines: brush press (first dab), CPU | 4.95 ms | 4.49 ms | 16.9 ms | 2.59 ms [2.53-3.10] |
| Lines: brush dab, CPU | 13.6 ms (12.9) | 12.9 ms | 9.19 ms | 8.93 ms [8.66-9.07] |
| Lines: brush release + full view, CPU | 45.6 ms | 42.9 ms | 41.6 ms | 33.2 ms [33.1-34.1] |
| Lines: brush press, GPU preview on | 5.88 ms | 139 ms | 22.0 ms | 4.80 ms [4.75-5.16] |

### Headless, 500k strands

| Measure | Maya OpenGL | Maya DX11 | App before | App after |
| --- | --- | --- | --- | --- |
| Lookdev: a frame, nothing changed (interactive) | | | 186 ms | 39.1 ms [39.1-39.4] (the refine frames 184) |
| Lookdev: orbit, full size | | | 5.70 fps | 27.0 fps [26.9-27.0] |
| Lookdev: orbit, adaptive | | | 5.70 fps | 44.9 fps [44.9-45.4] |
| Lookdev: Noise drag, CPU | | | 451 ms | 295 ms [294-295] |
| Lookdev: Noise drag, GPU preview | | | 219 ms | 71.3 ms [71.1-71.6] full, 55.5 [55.4-55.9] adaptive |
| Lookdev: brush press, CPU | | | 161 ms | 103 ms [102-108] |
| Lookdev: brush dab, CPU | | | 79.5 ms | 80.4 ms [79.2-81.1] |
| Lookdev: brush release + full view, CPU | | | 494 ms | 342 ms [342-345] |
| Lookdev: brush dab, GPU preview | | | 23.6 ms | 24.0 ms [23.9-24.1] |
| Lines: a frame, nothing changed | 5.03 ms | 1.99 ms | 4.34 ms | 4.09 ms [3.87-4.25] |
| Lines: Noise tweak / drag, CPU | 108 ms | 136 ms | 181 ms (77.9 + 103) | 139 ms [136-140] (the upload's CPU side 41.9) |
| Lines: Clump tweak / drag, CPU | 134 ms | 165 ms | 208 ms | 165 ms [164-166] |
| Lines: Noise drag, GPU preview | 12.0 ms | 9.95 ms | 10.2 ms | 10.1 ms [9.59-10.3] |
| Lines: release of a Noise drag, CPU commit | 132 ms | 178 ms | 227 ms | 176 ms [172-178] |
| Lines: brush press (first dab), CPU | 9.66 ms | 8.73 ms | 63.0 ms | 11.8 ms [10.0-13.6] |
| Lines: brush dab, CPU | 36.6 ms (35.5) | 35.5 ms | 24.9 ms | 25.4 ms [25.1-26.2] |
| Lines: brush release + full view, CPU | 174 ms | 160 ms | 175 ms | 178 ms [176-181] |
| Lines: brush press, GPU preview on | 11.5 ms | 763 ms | 93.2 ms | 18.3 ms [18.2-18.7] |

### The head test's comb dab (headless, 100k, Collide on)

A Place stroke of 13 guides across the top of `head_scalp.obj` with `head_graph.json`, then
the bench's comb (radius 120, strength 1) twice, the second's rows (`--draw-dabs`, the line
path, the GPU preview off: Comb and Collide sit between Interpolate and the look chain, so a
dab is the CPU's either way):

| Measure | Before | After |
| --- | --- | --- |
| Comb dab to the drawn frame | 65.7 ms [63.8-70.0] | 43.9 ms [43.2-45.6] (9.87 [9.63-10.2] with Collide bypassed) |
| Comb release + full view | 292 ms [291-310] | 201 ms [187-207] |

Collide walks about 29k of the stroke document's 35k strands a dab, three passes each (the
graph's `iterations`, `maxDepth` 30 cm: every point is asked, nothing is skipped by distance),
1.7 M closest-point queries and 0.8 M pushes; the query went from 0.6 us to 0.5 (the box
tests' branches mispredicted; the SSE max does not branch), Collide's share of the dab from
about 55 ms to 35. The rest of the way to a few milliseconds a dab is the query count, not
its cost: a seeded search bound, skipping the points that did not move since their last
query and a branchless triangle test were tried and gave nothing measurable (every point
moves between passes; the tree's descent is the cost, not the leaf), so they are not kept.

### Live: the head test, 100k strands (real window, 1920x1080)

The median time between frames on screen (handling / `render()` CPU), medians of 3 runs:

| Mode | Before | After |
| --- | --- | --- |
| Orbit, lookdev | 33.0 ms | 16.6 ms [16.6-16.6] (one refresh; adaptive and full alike) |
| Param drag, lookdev, GPU preview | 33.2 ms (2.1 / 0.11) | 16.6 ms [16.6-16.7] (1.8 / 0.10) |
| Param drag, lookdev, CPU path | 55.0 ms (16.0 / 26.2) | 51.4 ms [51.2-51.7] (15.9 / 26.1) |
| Param drag, lines, CPU path | 49.5 ms (16.5 / 16.9) | 35.5 ms [35.4-35.6] (16.0 / 9.8) |
| Stroke dab, lines, Collide on | 130 ms (114 / 1.2) | 83.2 ms [82.7-86.5] (74.2 / 0.4) |
| Stroke dab, lookdev, Collide on | 141 ms (113 / 11.7) | 92.6 ms [92.2-92.8] (69.3 / 10.9) |

The 220-290 ms presentation spikes noted above are still there (two or three frames of sixty,
"the rest" holding them), and still leave the medians alone.

### The look

`--screenshot` of the bench scene at 1920x1080 (`--display guides=0`), the per-fragment frame
of main against the per-point one, over the drawn pixels (0-255): the interactive frame
differs by a mean of 0.96 (p99 9, max 54: the shading interpolated along a centimeter of
strand instead of evaluated at the fragment, mostly along the tips' highlights); 64 refined
frames differ by a mean of 0.014 (max 2), the refine path being untouched (the interactive
frame is the first of the accumulated frames, at 1/64). `GROOMLAB_APP_HAIR_SHADING=fragment`
draws the interactive frame the old way.

### Findings

- **The lookdev path is no longer bound by its frame at 100k**: 17 ms a frame (46 before), one
  refresh live for an orbit or a GPU-preview drag (33 before). At 500k it is 39 ms (186), 27
  fps orbiting at full size and 45 adaptive (5.7). The thin pass was all of the cost: the
  rasterization and the camera's deep opacity map are 3 and 1.3 ms at 100k, the Marschner
  shading of the overlapping fragments the other 40. Shading per point bounds it by the
  points, 1.6 M / 8 M, and leaves the rasterization (3 / 17 ms a pass) as the floor the
  adaptive resolution cannot lower; at 500k that floor, the camera's map and the point pass
  are the 39 ms.
- **The deep opacity maps** were already drawn only when the hair, the lights or the material
  change; the camera's own map must follow the camera (1.3 / 6.5 ms). A lookdev CPU tweak
  still pays the maps' redraw (four passes over every point) with the frame and the upload.
- **The whole write is at Maya's on the line path**: a CPU Noise tweak 27.7 ms against Maya's
  26-32 at 100k and 139 against 108-136 at 500k (181 before), the release of a GPU drag 34
  against 27-33 (46 before). The batch's copy is gone; what is left of the upload (7.5 / 42 ms
  of CPU) is the frames' build and the driver's own copy.
- **The press is at Maya's**: 2.6 ms against 4.5-5 at 100k (17 before), 12 against 9-10 at
  500k (63 before), with the GPU preview 4.8 / 18 (22 / 93).
- **Collide** is 35 ms of a 44 ms comb dab on the head graph (55 of 66 before), 1.7 M exact
  closest-point queries at 0.5 us. A few milliseconds a dab needs fewer queries: a cached
  grid of candidate triangles for points near the surface (skipping the tree's descent) or
  an incremental walk that keeps the pass before, both exact only with care about ties; or
  a change of the operator's terms (fewer `iterations`, a `maxDepth` that lets the bound skip
  far points, which this graph's 30 cm does not).
