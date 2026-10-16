# Screen-space LOD with analytic clump coverage

Research note, 2026-10-09. Prototype: `tools/research/clump_lod/bench_clump_lod.cpp` (opt-in target
`research_clump_lod`). Results: `tools/research/clump_lod/results/` (`clump_lod.jsonl`,
`2026-10-09_clump_lod.txt`).

## Verdict: go

With per-clump level of detail and widened, **unfloored** coverage, the viewport draws 4 to 50
times fewer strands at mid and far framings. It also lands **closer to a physical reference** than
today's full-density drawing, which is biased by the coverage floor. Beer-Lambert impostors take a
distant head (32 px) to 0.29% of its strands at the same accuracy. On the 2026-10-09 audit's
measured cost slope, a 1M-strand frame goes from 7.5 ms to 3.4 ms (mid), 2.3 ms (far) and
2.0 ms (very far).

Two side findings about today's lines:

1. The 0.05 coverage floor adds 3-7% ink far away and roughly doubles the color error, because thin
   tips are weighted like mid-strand.
2. In the blended top of the range, depth writes lose 2.6% coverage at mid distance. This shows as
   light streaks in 1M-strand pictures.

## 1. Models

### 1.1 What the viewport draws

Each strand point projects to a pixel-wide line with coverage c = clamp(w_px, f, 1), where
w_px = width · pixelScale / depth and the floor f = `kLineCoverageFloor` = 0.05. A fragment is kept,
opaque, when `lineKeepShare(c, own)` reaches a threshold hashed from the strand's number and the
stretch of strand it lies on (`lineHashThreshold`). Up to 0.1 it is pure hashing. From 0.1 to 0.4
the top of the range blends, with alpha c / share and depth written. With independent thresholds,
the expected coverage of a pixel crossed by fragments i is

  E[α] = 1 - Π_i (1 - c_i).

The prototype checks this against the drawn pictures: drawn alpha equals expected alpha to 3
decimals wherever no blending happens.

### 1.2 The physical picture

The physical picture is opaque fibers of the true projected width, box filtered. Model the fibers
crossing a pixel as a Boolean field with line density λ(x) across the screen (fibers per pixel of
width at offset x). A point is uncovered with probability exp(-w λ(x)), which is Beer-Lambert with
optical depth τ = w λ. So

  α_phys = 1 - (1/A) ∫_A exp(-w λ(x)) dA.

* **Fibers spread uniformly over the pixel** (unclumped, many): α_phys = 1 - Π(1 - c_i), the same as
  the hashed lines. Hashed coverage is unbiased for unclumped hair.
* **Coverage floor.** Where w_px < f, the viewport draws f. In the optically thin limit the pixel's
  ink is inflated by f / w_px; a tip at 0.008 px is drawn 6 times too dark. Measured on the whole
  head: +3.4% ink far, +7.1% very far. The color error is larger because tips and roots differ in
  color.
* **Clumped fibers** (n members in a band of width D < 1 px). Physical ink is
  ∫ (1 - e^{-w λ(x)}) dx ≤ D. The hashed lines give 1 - (1 - w)^n per pixel row whatever D is.
  For D = 0.5 px, n = 40, w = 0.05 px that is 0.49 against 0.87: tight distant clumps are drawn up
  to 1.8 times too dark.

### 1.3 Representative subsets: widening, its bias and the selection rule

Replace a clump's n members (coverage c each) with m of them at

  c' = 1 - (1 - c)^{n/m}

(the formula `lineCoverageAtDensity` already uses for a thinned preview). A pixel that sees all n,
or a proportional share, keeps E[α] exactly.

A pixel crossed by k members sees K of the kept ones, with K hypergeometric: mean km/n, variance
≈ k (m/n)(1 - m/n). With γ = -ln(1 - c'), Jensen gives E[e^{-γK}] ≥ e^{-γ E K}: the subset
under-covers, by approximately

  δα ≈ ½ γ² Var(K) e^{-γ E K}.

In the thin regime (k γ₀ small, γ₀ = -ln(1 - c)) the relative deficit is
**δα/α ≈ ½ (n/m) c ≈ ½ c'**. In the thick regime it is exponentially small. So **for a relative
error δ, keep the widened coverage c' ≤ 2δ.** This gives the first rule:

  m ≥ n · ln(1 - c̄) / ln(1 - c*),   c* = 2δ   (the prototype: c* = 0.35)

Lines are one pixel wide, so a clump spread over D_px pixels across needs about D_px lines to fill
its band. This gives the second rule:

  m_c = clamp( max(⌈n ln(1 - c̄)/ln(1 - c*)⌉, ⌈D_px⌉), 1, n )

c̄ is the members' mean *true* coverage and D_px is 4 σ_px, the members' projected RMS spread about
the clump's axis. Strands are ranked within each clump by a hash of their ID, and the first m are
kept, so the subset is stable as m changes (fade the m-th in for no popping, §5).

### 1.4 Impostors

Where a clump's members spread less than σ_max = 0.75 px about its axis over the whole strand, draw
one line along the axis (the members' mean, point by point). Its per-pixel coverage is the physical
ink of a Gaussian band, λ(x) = n N(x; 0, σ_px):

  c_imp(pixel) = ∫_{pixel's span} (1 - exp(-w n N(x; σ))) dx   (16-point rule, Beer-Lambert)

It is hashed like any strand, and its color is the members' mean, root to tip.

### 1.5 Cost

Selection is O(clumps × points) per view change: 546 clumps × 17 points, microseconds. The draw is
O(kept strands × points). On the audit's lit_coverage orbits the frame is about 2 ms plus 5.5 µs per
thousand strands of 17 points (7-8 ms at 1M strands on both APIs); the predicted frame times below
use that slope.

## 2. Prototype

* **Groom.** The hair_basics preset (Interpolate, Clump with size 1.2 and strength 0.8, Noise, Width
  0.007 tapering to 8%) on the app head with 400 hairstyle guides, at 1M strands × 17 points:
  546 clumps of about 1830.
* **Camera.** 30° fov, from the side and a little above. Image 512², metrics over the hair's box.
  The head is 1600 px (near, a crop), 384 px (mid), 96 px (far) and 32 px (very far).
* **Reference.** Every segment is a screen ribbon of its true width, 64 stratified jittered samples
  a pixel, opaque, depth tested, box filtered.
* **Viewport lines.** The core's own `lineKeepShare`, `lineHashThreshold` and `lineIdOf`: one
  fragment a pixel along the major axis, depth tested, blended where the share says. The tiles keep
  strand order.
* **Methods.** current (floored), unfloored, clumpLod, preview (today's thinned preview at the same
  strand budget, floored then widened), impostor (+ clumpLod elsewhere).
* **Metrics** against the reference over the hair's box: MAE of gamma luminance after Gaussian blur
  σ = 1 and 2 px (×255); SSIM at blur 1; ink ratio; color MAE (blur 2, ×255).

## 3. Results (1M strands)

| framing (head px, mean width px) | method | strands | segments | fragments | MAE σ1 / σ2 | SSIM | ink | color MAE | predicted frame |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| near (1600, 1.42) | current | 1,000k | 16.0M | 24.8M | 8.31 / 7.16 | 0.703 | 0.987 | 7.11 | 7.5 ms |
| | clumpLod | 821k | 13.1M | 16.1M | 7.14 / 6.04 | 0.751 | 0.993 | 5.91 | 6.5 ms |
| | preview (same budget) | 820k | 13.1M | 20.3M | 7.74 / 6.57 | 0.727 | 0.992 | 6.45 | 6.5 ms |
| mid (384, 0.105) | current | 1,000k | 16.0M | 132.7M | 7.97 / 7.37 | 0.804 | 0.975 | 8.56 | 7.5 ms |
| | unfloored | 1,000k | 16.0M | 132.7M | 7.74 / 7.02 | 0.802 | 0.971 | 8.25 | 7.5 ms |
| | **clumpLod** | **258k (25.8%)** | 4.1M | 35.1M | **4.17 / 3.28** | 0.897 | 0.990 | **3.81** | **3.4 ms** |
| | preview | 257k | 4.1M | 34.1M | 4.21 / 3.42 | 0.901 | 0.994 | 3.87 | 3.4 ms |
| far (96, 0.025) | current | 1,000k | 16.0M | 33.5M | 4.67 / 4.17 | 0.927 | 1.034 | 5.84 | 7.5 ms |
| | unfloored | 1,000k | 16.0M | 33.5M | 2.58 / 1.68 | 0.957 | 1.009 | 2.50 | 7.5 ms |
| | **clumpLod** | **59.5k (6.0%)** | 0.95M | 2.0M | **2.85 / 1.93** | 0.950 | **1.004** | **2.92** | **2.3 ms** |
| | preview | 59.2k | 0.95M | 2.0M | 4.42 / 3.85 | 0.932 | 1.028 | 5.42 | 2.3 ms |
| very far (32, 0.008) | current | 1,000k | 16.0M | 11.1M | 7.11 / 6.28 | 0.899 | 1.071 | 9.84 | 7.5 ms |
| | unfloored | 1,000k | 16.0M | 11.1M | 3.53 / 2.69 | 0.958 | 1.026 | 4.45 | 7.5 ms |
| | clumpLod | 19.7k (2.0%) | 0.32M | 0.22M | 4.23 / 3.43 | 0.944 | 1.039 | 5.70 | 2.1 ms |
| | preview | 19.7k | 0.32M | 0.22M | 6.03 / 5.27 | 0.921 | 1.057 | 8.43 | 2.1 ms |
| | **impostor** | **2.9k (0.29%)** | 46k | 52k | **4.11 / 3.28** | 0.919 | **1.013** | 5.76 | **2.0 ms** |

Reading the table:

* **Mid and far.** The clump LOD beats the full-density drawing on every accuracy column except
  near-equal SSIM, with 4 to 17 times fewer strands.
* **Same strand budget.** Today's preview equals clumpLod at mid. Far away it carries the floor's
  bias (ink 1.028, color 5.42 against 1.004 and 2.92). Widening from the *true* coverage and choosing
  per clump is what pays.
* **Near.** The rule keeps 82%. LOD hardly matters there, and that is correct.
* **Impostors** apply only to clumps that stay under 0.75 px across. They take 487 of 546 clumps at
  32 px and none at 96 px. Per-segment impostors (only where the clump is tight, toward the tips)
  would reach the far framing too. Not prototyped.
* **Blended range.** At mid, `current` draws alpha 0.868 where its own fragments' expectation is
  0.891. Blended fragments (own coverage 0.1 to 0.4, near the roots) write depth and hide the hair
  behind them. clumpLod's widened coverage pushes those fragments toward opaque (0.878 against
  0.886).
* CPU raster time of the harness (a proxy only): mid 1107 → 304 ms, far 568 → 36 ms, very far
  387 → 4.5 ms.

## 4. Go / no-go

Go. The accuracy gain is measured against a physical reference, not against the old picture. The
cost model is the audit's measured slope. The GPU frame times still need confirming in the real
viewport (batch B2's first step).

## 5. Implementation plan

* **B1, core (2-3 days).** `ClumpLod` in display.cpp, beside `lineCoverage`:
  * Per evaluation, cache each clump's axis (mean of members by point index), member count and
    projected-spread inputs, and a per-strand rank (hash of the ID within its clump). Sort the strands
    by clump, then rank, so each clump's first m strands are contiguous.
  * Per view, `selectLod(camera) → per clump m_c, impostor flag`: the two rules of §1.3 and the σ
    test of §1.4, all from the cached axis and spread.
  * Output per strand a "stands for" factor s = n/m (0 = not drawn) and the impostor streams (axis
    points, σ world units, n, w).
  * Fractional m (m + φ) fades the (m+1)-th strand's coverage by φ, so nothing pops.
  * Groups without clumps (no Clump node): grid cells of the scalp (about 1 cm) as pseudo-clumps.
* **B2, viewport (2-3 days).**
  * Maya draw override and the app's line path: an index buffer of the kept strands, rebuilt when the
    view changes by more than about 10% in scale (throttled), or a GPU compaction pass.
  * The vertex stage takes s and computes c' = 1 - (1 - w_px)^s, applying the floor only when
    s = 1: exactly `lineCoverageAtDensity` with density 1/s, so the shaders change little.
  * Multiply s with the preview density.
  * First step: confirm the frame times at 1M strands with `--benchmark-orbit` (app) and the GUI
    viewport bench (`maya.gui_dx11`).
* **B3, impostors (2 days).** The axis lines with a Beer-Lambert coverage per pixel in the fragment
  stage (σ_px from σ / depth) on the hashed path. Per-segment impostors where only the tips are tight.
* **B4, tests.** The harness as an opt-in picture test: ink within 2% of the reference and MAE no
  worse than current at all four framings. The GUI viewport suite: a still view draws the same pixels
  twice, and an orbit changes under 1% of them (the hashing's existing checks) with LOD on.
* **Risks.**
  * Popping when m steps: fractional fade, hysteresis on the scale.
  * Clumps changing per evaluation: ranks are by ID, so they are stable.
  * Lit lines in Maya's shader graphs cannot write a sample mask (already blended there).
  * The 1024 line IDs: hash collisions inside a clump grow as m shrinks. Use the strand's rank within
    its clump as part of the line number.
  * Cards and ribbons are unaffected (lines only).

## 6. As built (2026-10-09)

Core `strand_lod.h` (`StrandLod`), the Maya override (`updateLod`), the app's line path
(`GroomRenderer::prepareLod`); docs/DEVELOPING.md, "Screen-space level of detail", says how. Where
it differs from the plan above:

* **Classes, not a per-strand factor.** n/m is rounded down to a class a quarter octave apart (32
  of them) and m taken as round(n / 2^(k/4)): each class is one draw with one exponent in a
  uniform (`lineLod`, the app's `lines.z`), so no stream changes per view and the GPU preview,
  Follow Scalp and partial writes keep writing the strands' own buffers. The strands stand for the
  group's n to a strand. A class change replaces the fractional fade: the group's hysteresis (a
  quarter class at the same view) keeps a dab or a tweak from changing anything.
* **The view orbits for free.** Each axis point is taken where it comes closest as the camera
  orbits the strands' bounding sphere at its distance (the level, eighth octaves with hysteresis,
  of the scale at the sphere's center): an orbit changes nothing, a dolly in steps. (Since
  2026-10-10 the orbit is about the host camera's own pivot, Maya's tumble tool's center of interest
  or tumble pivot, the app's target: the sphere's center comes as near as the pivot's depth less its
  distance to the pivot. Measured from the sphere's center alone, the wolf at near framing tumbling
  about its box center wrote the indices on about every other frame, below.) This keeps more
  strands than the research's per-clump depth at mid (37% against 26% on the head) and is still
  more accurate (below).
* **Thinned strands are hashed alone** (no blended top): they stand for several, so the blended
  range's light streaks cannot happen there.
* **The streaks elsewhere.** Class 0's segments that can blend at any orbit at this distance draw
  twice: their opaque fragments with everything (depth written), then their blended fragments
  alone after everything (depth tested, not written); the rest once. Measured on the harness
  (`unflooredTwoPass`, `coreSplit`): the drawn alpha equals the fragments' expectation (mid 0.886
  against 0.886, was 0.868 against 0.891; near 1.000, was 0.994).
* **Impostors are lines** of the band's ink (`lodBandInk`: sigma x F(total width / sigma), the
  spread widened by a fiber's own), drawn by the same shaders as any strand (hashed alone), not
  ribbons: one line holds the ink of a band up to a pixel.
* **No floor**: `kLineCoverageFloor` is 0 everywhere.
* **Unclumped strands** group by cells of their roots (about 256 strands a cell).

Picture error (the harness, `--methods current,coreSplit`, 1M strands, 512^2; `ctest -L pictures`
checks the last column's ink within 2% and its MAE no worse than the first's):

| framing | current: MAE / ink / color | as built (LOD, impostors, two passes): strands, MAE / ink / color |
| --- | --- | --- |
| near | 8.31 / 0.987 / 7.11 | 1,000k, 7.18 / 0.992 / 5.97 |
| mid | 7.97 / 0.975 / 8.56 | 371k, 2.66 / 1.000 / 1.68 |
| far | 4.67 / 1.034 / 5.84 | 74k, 2.72 / 1.007 / 2.71 |
| very far | 7.11 / 1.071 / 9.85 | 5k and 435 impostors, 3.79 / 1.000 / 4.83 |

At 300k strands the near framing's MAE is 7.49 against 7.37 (the second pass alone there).

GPU frame times in Maya (`maya.viewport_bench` part `lod`: hair_basics on a radius-10 sphere, lit
lines with width as coverage, the view 1100 x 573, each frame's refresh with the GPU waited for,
medians of 3; Maya's own floor is about 2 ms on DirectX 11 and 2.5 to 3 on OpenGL). The prediction
was 7.5 ms to 3.4 (mid), 2.3 (far) and 2.0 (very far) at 1M strands:

| strands | framing | DirectX 11 off -> on (ms) | OpenGL off -> on (ms) | drawn (impostors) |
| --- | --- | --- | --- | --- |
| 1M | near | 10.10 -> 7.56 | 12.99 -> 9.39 | 707k |
| 1M | mid | 9.61 -> 2.00 | 11.65 -> 2.53 | 88k |
| 1M | far | 15.13 -> 2.00 | 16.10 -> 2.53 | 211 (879) |
| 1M | very far | 9.71 -> 2.00 | 10.22 -> 2.68 | 0 (884) |
| 500k | near | 6.12 -> 4.50 | 6.57 -> 5.62 | 353k |
| 500k | mid | 5.50 -> 2.00 | 6.22 -> 2.51 | 44k |
| 500k | far | 7.51 -> 2.00 | 8.70 -> 3.00 | 143 (868) |
| 500k | very far | 5.16 -> 2.00 | 6.55 -> 6.53 * | 0 (875) |
| 100k | near / mid / far / very far | 2.73 / 2.06 / 2.01 / 2.01 -> 2.17 / 2.00 / 2.00 / 2.00 | 3.72 / 3.01 / 3.01 / 3.01 -> 3.01 / 3.00 / 2.57 / 3.11 | 74k / 10k / 36 (834) / 0 (842) |

(* runs of 3.0, 6.5 and 7.1 ms; run again, with other builds loading the machine, 6.54 -> 3.00.) Before it was built, the
same bench drawing the strand counts the level of detail would (1M, 258k, 60k, 20k) at one framing
gave 12.8, 5.7, 3.5 and 4.0 ms (DirectX 11): the cost follows the strands drawn down to Maya's
floor, as the audit's slope said. The selection's update costs 2 to 10 ms when the view's level
changes, 30 ms at 1M near (the indices written: 90 MB there, 11 MB at mid).

Very far (the head ten pixels across) every strand drawn by Maya reads about a fifth lighter than
the thinned strands and the impostors, which agree with each other; on the harness, the same groom at that
scale (`--sphere --tiny`), the level of detail is within 4% of the reference and every strand 11% over it.

## 7. Orbits about the camera's pivot (2026-10-10)

The wolf (615k strands at Scatter density 50) at near framing wrote the classes' indices on about
every other orbit frame: the view was the depth of the strands' bounding sphere's center, but Maya
turns the camera about its center of interest (the tumble tool's default) or its tumble pivot, here
the groom's box center, well off the strands' center; turning over the top about it moved that depth
by most of an octave each way (levels 24 to 38: 28 rebuilds in 60 frames of `orbit`'s second
angle, 130 ms frames against 12 with the level of detail off, the rebuild 60 to 100 ms: the
segments counted serially and the per-point scales taking an exponential each). Turning around the
vertical (a tumble's azimuth) moved it little, so the bench's other orbits showed nothing; about a
point of the surface or a recent selection the hair_basics groom wrote 4 times a turn at near.

Now the view is taken for an orbit about the host camera's pivot (strand_lod.h): an orbit keeps the
pivot's depth d and the center's distance e to it, so the center comes as near as d - e (the level)
and goes as far as d + e (`farLevel`, the blendable segments' farthest). Maya asks the tumble tool
which pivot (center of interest, or tumble pivot for "tumble on recent selection"), the app gives its
orbit target. A class's indices are written again only when its own segments change, counted and
written in parallel chunks, the per-point scales' exponentials taken once.

`maya.viewport_bench` part `lodorbit` (60 frames a full turn with 12 degrees up and down, near and
mid; rows in tools/bench/gpu_audit/results/2026-10-10_lod_orbit_*.jsonl; the GPU 3-20% busy with
another program in most rows), LOD on, rebuilds a turn, before -> after, both devices alike:

| groom, framing | center | over the top | surface point | Maya's framing | recent selection | `orbit -pivotPoint` (not told) |
| --- | --- | --- | --- | --- | --- | --- |
| wolf near | 0 -> 0 | 28 -> 0 | 5 -> 0 | 0 -> 0 | 6 -> 0 | 6 -> 5 |
| wolf mid | 0 -> 0 | 4 -> 0 | 2 -> 0 | 0 -> 0 | 2 -> 0 | 2 -> 2 |
| hair_basics 1M near | 0 -> 0 | 0 -> 0 | 4 -> 0 | 0 -> 0 | 4 -> 0 | 4 -> 4 |
| hair_basics 1M mid | 0 -> 0 | 0 -> 0 | 1 -> 0 | 0 -> 0 | 1 -> 0 | 3 -> 2 |

("Over the top" before: the first run's `orbit` about the tumble pivot at the box center.) The wolf
near over the top: 130 ms frames -> 13.6 (OpenGL), 13.5 (DirectX 11); with the level of detail off
5.5. What remains at near is the blendable segments' second pass, not the selection. A rebuild there
(a dolly's step, or the pivot the viewport is not told of): the indices 32-63 ms -> 5-6 ms, the
whole update 60-100 ms -> about 20 (the selection 25 -> 13 ms). A pivot the center can swing to the
eye's plane about (one close in front of the camera at near) draws every strand: nothing to rebuild.
