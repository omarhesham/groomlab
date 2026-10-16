# Strokes chained on a pending commit (2026-10-10)

`maya.viewport_bench_dx11` (gui_bench.py), `GROOMLAB_BENCH_PARTS=chain`, `GROOMLAB_BENCH_CHAIN=<wolf copy>@20,<wolf
copy>@50,hair_basics:500000`, DirectX 11, medians of 3 after one stroke to warm up. The wolf: a copy of the user's
`wolf_groom.ma` (32 guides; Interpolate keeping clearance, neighbors 3; two Clumps, Noise, Flyaway, Length, Width,
Resample; previewDensity 1, sculptDensity 1), framed from the side, comb strokes of radius 150 px drawn with
`-debugStroke` (9 dabs, all on the GPU). hair_basics: the radius-10 sphere of `stroke_bench`, radius 150 px. Before: main
at ef02f580 (with `groomLabGraphStats` `runs` only); after: the detached commits and chained strokes
(docs/DEVELOPING.md, "Strokes chained on a pending commit"). Machine load at each scene's start (CPU / GPU %, from the
processors' load and nvidia-smi; desktop apps, no game): before 15/18, 11/25, 8/6; after 8/25, 9/14, 18/10.

- **Commit**: the release's CPU evaluation on the worker (`-frameInfo` `commitMs`).
- **Next stroke ready**: a stroke pressed right after a release (no idle in between: the worst case), its press's step
  time to its drawn frame (`-debugStrokeTimes`). Before, the press took the commit in first (waited for it).

| scene | strands | commit before ms | commit after ms | next stroke ready before ms | after ms |
|---|---|---|---|---|---|
| wolf, Scatter density 20 | 246k | 565 (543-570) | 544 (535-557) | 515 (320-640) | 9.4 (9.2-9.8) |
| wolf, Scatter density 50 | 615k | 1496 (1337-1513) | 1385 (1265-1414) | 750 (639-781) | 14.8 (13.5-93) |
| hair_basics | 500k | 195 (190-208) | 171 (164-172) | 222 (197-228) | 13.1 (13.1-76) |

Notes:

- The commit was incremental already: Interpolate blends again only the strands bound to a guide the stroke changed,
  and every node after it follows those (`runs`: `incremental 150578` of 246k at density 20, `415548` of 615k at 50;
  377k of 500k on hair_basics). A comb over ~9 of the wolf's 32 guides reaches most strands (each blends 3), and
  Interpolate's clearance walk costs 2-5 us a strand on 18 threads (60% of the commit; the walk's closest points from
  the mesh, `answerNear`, half of it). Nothing smaller to make again exactly.
- The GPU's points are not adopted: its Interpolate and clearance walk agree with the CPU's within a tolerance
  (`ADabWalksOnlyTheStrandsOfTheGuidesItChanged`: `kPointTolerance`), not to the bit, so a render or export after it
  would differ from a fresh evaluation.
- Chained: `nextPressChained` 1 and no commit landed between the two strokes in every pair; the second commit
  (copying what the first made, then the second stroke's strands) took 489, 1252 and 166 ms. The commit copies the
  arrays its incremental operators used to take from the last results (the session's document still holds them while a
  stroke may chain): no slower here (the after rows are 4-11% lower, within run-to-run noise).
- One press in three was slower on two scenes (76-93 ms), with no commit landed before it either.
