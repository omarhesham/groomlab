# B7: commits off the UI thread (2026-10-09)

`maya.viewport_bench` / `maya.viewport_bench_dx11` (gui_bench.py), `GROOMLAB_BENCH_PARTS=commit,stroke`,
`GROOMLAB_BENCH_COUNTS=100000,500000,1000000`, medians of 3 releases after one to warm up. 24 threads,
GPU 6-17% busy with desktop apps before and after each run (no game). Before: main at 57b5e158; after:
the async commit (docs/DEVELOPING.md, "Commits off the UI thread").

- **Perceived release latency**: from the release (a drag: `-endDrag`, `set_param`; a stroke: the
  `-debugStroke` release step) to the UI responsive with the final-looking frame drawn (a drag:
  `refresh` and the GPU waited for; a stroke: the release step's time to its redrawn frame).
- **Commit**: the CPU evaluation that used to run in that time, now on a worker (`-frameInfo`
  `commitMs`); it lands on Maya's idle, the update then (`landUpdateMs`) is the one a release made.

## A GPU drag's release (Noise amplitude, default graph, lit lines with width as coverage)

| API | strands | before ms | after ms | commit (worker) ms | landing update ms |
|---|---|---|---|---|---|
| OpenGL | 100k | 33.0 | 9.0 | 14 | 7 |
| OpenGL | 500k | 134.3 | 7.0 | 65 | 20 |
| OpenGL | 1M | 215.5 | 8.1 | 131 | 35 |
| DirectX 11 | 100k | 29.1 | 5.6 | 14 | 7 |
| DirectX 11 | 500k | 142.6 | 4.5 | 67 | 17 |
| DirectX 11 | 1M | 201.0 | 4.0 | 132 | 31 |

## A comb stroke's release (hair_basics, 9 dabs on the GPU, the stroke changing every strand)

`final`: sculptDisplay normal, sculptDensity 1 (the GPU's dabs draw the final look). `default`: the
shape's defaults (dimmed strands at sculptDensity 0.35: the lit full-density strands appear when the
commit lands).

| API | strands | sculpt | before ms | after ms | commit (worker) ms | landing update ms |
|---|---|---|---|---|---|---|
| OpenGL | 100k | final | 77.2 | 5.7 | 54 | 10 |
| OpenGL | 500k | final | 253.2 | 5.8 | 208 | 16 |
| OpenGL | 1M | final | 516.7 | 6.3 | 440 | 23 |
| OpenGL | 100k | default | 428.8 | 5.3 | 50 | 21 |
| OpenGL | 500k | default | 422.1 | 5.7 | 212 | 47 |
| OpenGL | 1M | default | 671.6 | 5.5 | 427 | 85 |
| DirectX 11 | 100k | final | 87.5 | 5.6 | 49 | 19 |
| DirectX 11 | 500k | final | 278.0 | 5.6 | 233 | 41 |
| DirectX 11 | 1M | final | 505.8 | 6.0 | 474 | 33 |
| DirectX 11 | 100k | default | 940.1 | 5.9 | 54 | 19 |
| DirectX 11 | 500k | default | 394.7 | 5.5 | 223 | 54 |
| DirectX 11 | 1M | default | 638.7 | 5.8 | 467 | 42 |

Notes:

- The worker's loops run on 18 of the 24 threads (`GROOMLAB_ASYNC_THREADS`): on all of them the
  release's redraw waited ~50 ms at 500k (Maya's drawing shares TBB with them); on 12 the perceived
  latency was ~5 ms and the commit ~10% slower.
- In the `default` sculpt display the redraw after the stroke (the lit strands back) takes 300-800 ms in
  Viewport 2.0 beyond the override's update (20-85 ms), at every count and before as after (the
  before releases include it; after, it comes with the landing's redraw): not the commit's cost, and
  worth a look of its own.
- The landing writes the CPU's strands as a synchronous release did (partial writes for a stroke);
  pictures held while pending and landed match (0 of the pixels differ, viewport GUI suite `async` case).
