# Collide on a signed distance field: 2026-10-03

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA
GeForce RTX 3090. Medians of 3 runs (min-max in brackets), each run started with the machine
quiet (total CPU under 11% over the 3 s before it, no compiler, Maya, kick or ctest running);
a first set taken while another build ran is not recorded.

Collide's `method` `field` (the default) reads each mesh's sparse signed distance field
(`engine/core/include/groomlab/distance_field.h`), made where the hair goes on the first
evaluation and kept across evaluations by the mesh's surface hash; `exact` asks the mesh's
closest-point tree at every point, as Collide did before (its results equal the old goldens
bit for bit, so its times are the "before").

## Brush dabs and first evaluations (core, `Budget.BrushStrokeLatencyWithCollide`)

`groomlab_tests.exe --gtest_filter=Budget.BrushStrokeLatencyWithCollide`: a session at
sculptDensity 1, two comb strokes of 10 dabs (the Maya bench's camera and stroke), each dab
evaluated and drawn into host buffers; Collide with the head graph's settings (offset 0.08,
maxDepth 30, 3 iterations). The head graph is `tests/data/app/head_graph.json` on
`head_scalp.obj`; the bench graph has Collide after Interpolate on a radius-10 sphere.

| Scene | Collide per dab, field | Collide per dab, exact | Dab evaluation, field | Dab evaluation, exact | Collide bypassed | First evaluation, field | First evaluation, exact |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Head, 100k strands | 1.6 ms [1.4-1.7] | 31.3 ms [30.4-31.7] | 17.5 ms [16.4-18.3] | 47.9 ms [46.5-48.7] | 16.6 ms [15.8-16.7] | 88 ms [83-98] | 158 ms [156-173] |
| Head, 500k strands | 5.6 ms [5.4-5.7] | 143.5 ms [141.7-149.3] | 67.8 ms [63.5-70.2] | 210.9 ms [206.3-218.5] | 64.4 ms [63.3-65.0] | 358 ms [337-382] | 719 ms [719-738] |
| Bench sphere, 100k | 5.7 ms [5.5-5.8] | 52.0 ms [50.4-56.0] | 16.1 ms [15.5-16.5] | 63.3 ms [60.6-67.5] | 10.8 ms [10.7-11.5] | 120 ms [116-130] | 344 ms [334-364] |
| Bench sphere, 500k | 25.5 ms [25.4-26.1] | 244.1 ms [242.8-244.4] | 62.9 ms [61.3-63.4] | 279.4 ms [278.9-279.5] | 38.0 ms [36.7-41.0] | 435 ms [420-457] | 1693 ms [1621-1746] |

A dab with Collide now costs what it costs with the node bypassed plus 1-2 ms on the head (the
walk of the moved strands is lookups: the head graph's input has no point under the offset, so
every answer is "kept", read off a far brick's bound or a cell's blend). The first evaluation,
which makes the field, is faster than the exact one too: regions of 4 x 4 x 4 bricks far from
the surface settle their points with one query each.

## Other evaluations

| Case | Field | Exact (before) |
| --- | --- | --- |
| One cold Collide, 100k strands combed down into a unit sphere, offset 0.01 (`Performance.CombAndCollideAt100kStrands`) | 212 ms [195-252] | 183-186 ms (the same process, `method` exact) |
| The same Collide again, the field made | 68 ms | 183 ms |
| A deforming scalp's frame, 50k strands (`Budget.CollidePlaybackFrame`) | 7.0 ms [6.9-7.2] | 6.9 ms |

A scalp deformed away from its rest pose is a new mesh every frame, whose field would serve one
evaluation: Collide asks it directly (the exact path), so playback is as before. A cold field
on dense hair near the surface costs about what the exact walk does (each cell the hair touches
samples its 8 corners once); every later evaluation is lookups.

## Accuracy against the exact method

`groomlab_tests.exe --gtest_filter=DistanceField.*:Collide.FieldMatchesExactOnTheHead`:

- **Pushes** (20,000 points around the head scalp, cell 0.0625 for offset 0.08): the field's
  push differs from the mesh's by a mean of 1.0e-4 and at most 2.0e-3 (2.5% of the offset); a
  point the field keeps is never one the mesh pushes by more than 1e-3.
- **The head graph** (25k strands, its combs: nothing to push): identical to exact, bit for bit.
- **Every strand combed straight down through the head** (most points deep under the scalp,
  pushed out and folded along it): field vs exact differ by a mean of 2.1e-4 (0.26% of the
  offset); the worst point 1.34 cm, which is the walk's own sensitivity (exact against itself
  with its input moved by 1e-4: mean 2.7e-4, worst 1.36 cm); 1 point of 403k under the offset by
  more than 3% of it (exact: none).
- **Goldens** (offset 0.02 on the golden scene's coarse sphere and its 16 x 32 ball collider):
  Collide 1.2e-4, ColliderMeshCollide 5.6e-4 from the exact results (kept as CollideExact and
  ColliderMeshCollideExact).

The field's error is the trilinear blend rounding off the kinks the height has where the
closest triangle changes (about an eighth of the cell times the angle between neighbouring
triangles), so it scales with the cell: about the offset here. Half the offset gave a worst
push of 0.85e-3 but cost 4x the memory (98 against 31 MB on the combed-through head) and made
the first evaluation 2.5x slower.

## Memory

The head graph's field after an evaluation: 7.8k regions, 28k far bricks, 12 fine bricks, 2.1
MB. Every strand combed through the head (the worst case: hair everywhere near and under the
surface): 26.6k fine bricks, 1.0 M nodes sampled, 30.6 MB. The last four fields are kept.

## The app: a comb dab on the head with placed guides

The app's headless stroke on `head_scalp.obj` with `head_graph.json` at 100k strands, the line
path, the GPU preview off (Comb and Collide sit between Interpolate and the look chain, so a dab
is the CPU's either way): a Place stroke of 13 guides across the top, then the bench's comb
(radius 120, strength 1) twice; the second stroke's rows. Here the placed guides comb the hair
into the scalp, so Collide pushes most points of the moved strands on every pass (the core
bench's head graph pushes none):

```
GroomLab head_scalp.obj --graph head_graph.json --size 1600x1000 --gpu-preview off --no-lookdev
  --set collide.method=field|exact --brush mode=place --stroke-world "-6,7.2,1 ... 6,7.2,1"
  --brush mode=comb --brush radius=120 --brush strength=1
  --stroke-world "-1.8,7.4,3.6 ... 1.8,7.4,3.6" --stroke-world "(the same)" --draw-dabs
```

| Measure | Field | Exact (before) | Collide bypassed |
| --- | --- | --- | --- |
| Comb dab to the drawn frame | 30.4 ms [29.7-30.5] | 73.6 ms [67.5-74.8] | 11.6 ms |
| Release + full view | 100.4 ms [94.3-104.7] | 208.9 ms [200.7-240.6] | 44.7 ms |

PENDING a quiet re-run: these three runs were taken while another session's Maya test ran
(total CPU 9-20% before each run); the field-vs-exact ratio held in every run, also under a
heavier load earlier (30-40 against 76-83 ms). Collide's share of the dab: about 19 ms against
62. Where every point is pushed the walk asks the field 2-3 times a point a pass (a push, then
again after the length is kept), about 60 ns a lookup in cache and 200 ns from memory, against
0.7 us for the mesh's closest point.
