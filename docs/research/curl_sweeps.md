# Curls as analytic sweeps, and compact strand storage

Research note, 2026-10-09. Prototype: `tools/research/curl_sweeps/bench_curl_sweeps.cpp` (opt-in
target `research_curl_sweeps`). Results: `tools/research/curl_sweeps/results/2026-10-09_curl_sweeps.*`.

## Verdict

| Idea | Verdict | Measured |
| --- | --- | --- |
| Resolution-independent Curl (the curl defined on a smooth base, not at the input's points) | **Go: quality fix** | The shipped curly preset at its 40 segments differs from the same groom at 256 segments by 0.40 mm on average (6 strand widths), 0.82 mm p99, 1.0 mm max. 128 segments is still 0.17 mm off. Convergence is O(1/n), not the O(1/n²) of chord error, and Curl adds a kink at every root. |
| Analytic sweep (base spline + f, φ, r) as the stored and drawn representation | **Go: draw-time tessellation** | 132 B a strand against 1.0-3.1 KB (curly) and 1.4-4.5 KB (coily) for polylines cut to the same error: 8-34x smaller. Screen-adaptive points: curly 50 / 25 / 15 a strand at hero / mid / far against a fixed 41. Coily is a 72-point helix at hero and a 9-point ribbon beyond. 120-150 Mpts/s reconstruction on the CPU. |
| Ringlet clumps share one offset curve | Math fact, measured | Members of a clump-coherent curl carry their clump's offset to 9e-7: a ringlet clump is one curve plus member bases. |
| Generic strand compression (cubic B-spline + 16-bit control points) | **No-go for the viewport** | Only 2-5.5x on final curly strands (C⁰ kinks from 32-sample clump axes defeat smooth fits). Maybe for export caches. |

## 1. Math

### 1.1 The sweep

A curl is a helix (or a wave or zigzag) about a base curve c(s):

  p(s) = c(s) + r(s) [cos θ(s) N(s) + sin θ(s) B(s)],   θ(s) = 2π f s + φ₀

N and B are the base's rotation-minimizing frames (double reflection), s is the base's arc length,
and r(s) = r · vary · profile(s / L). Storage: the base (a cubic B-spline of k control points,
16-bit in its box: 6k + 24 B) and three numbers (f, φ₀, r: 12 B). It does not grow with the
number of turns.

### 1.2 How many points a turn needs (chord error)

Drawn as a polyline with m points per turn, the helix's projection is a circle sampled every 2π/m.
The sagitta is

  ε(m) = r (1 - cos(π/m)) ≈ π² r / (2 m²)   ⇔   m(ε) = π / arccos(1 - ε/r) ≈ π √(r / 2ε).

For a general curve of curvature κ with segments of length ℓ, ε ≈ κ ℓ² / 8. The helix has
κ = r / (r² + (2π f)^-2) ≤ 1/r, and a curved base adds its own κ. The measured polylines track the
formula within a factor of 1.2-2 (coily) and 1.2-5 (curly, whose bases bend tightly). Pixel
version: for a tolerance of half a pixel, **m_px = π / arccos(1 - 0.5 / r_px)**.

### 1.3 Sub-pixel coils: the envelope

When r_px < 0.5 the helix cannot be resolved. Draw its axis as a ribbon of width 2r + w with
coverage equal to the fiber's share of the band. The ink per unit of axis length is w ℓ_h, where
ℓ_h = √(1 + (2π r f)²) is the helix's length per unit of axis, so

  c_env = w ℓ_h / (2r + w)   (≤ 1).

This preserves the first moment of coverage exactly. It loses only the sub-pixel texture, which
could not be seen anyway. It plugs straight into the width-as-coverage lines (the
clump_lod.md machinery).

### 1.4 Why today's Curl depends on the point count

`CurlOp` offsets each existing point. It draws the angle from the polyline's own arc length and the
frames from the polyline's own RMF, seeded by `frameSeed`: the scalp normal minus its component
along the *first segment*. Three resolution-dependent terms follow:

* **Frame seed.** It is ill-conditioned when the strand leaves along the normal, and the first
  segment's direction changes O(h) with the resolution. For coherence by clump, the seed comes from
  the clump axis (fixed 32 samples) instead.
* **Root kink.** The root never moves while point 1 moves by r · profile(1/(n-1)) ≈ r · profile(0).
  On the curly preset profile(0) = 0.1, so the first segment kinks sideways by 0.04 units
  (5.6 strand widths) at any resolution.
* **Clump axis.** By clump, the axis is 32 samples with frames interpolated linearly
  (`frameBetween`). The offsets are C⁰ with kinks every 1/31 of the length.

The measured convergence (each doubling of segments halves the difference) is consistent with O(h)
terms.

### 1.5 Ringlets share their clump's curve

With coherence "clump", every member's point i takes the clump's frame, angle and radius at its
share of the length (`curlByClump`). The offset o_s(i) = curled - base is therefore the same for
every member at equal i (radius per strand aside: a constant param here). Measured: at most 9e-7
apart. A ringlet clump is stored as **one offset curve plus member bases**. With 8.5 members a
clump at 10% density (about 85 at full density), the offset storage nearly vanishes per strand.

### 1.6 Complexity

* Reconstruction is O(points drawn) per strand, plus an O(K) frame table (K = 256 RMF samples, about
  2 µs a strand on one CPU core).
* On the GPU this is a thread a strand for the table and a thread a point for the offsets, the same
  shape as the look chain's CurlCS (4 ms for 1M strands × 16 points on DirectX 11).
* Storage is O(k) a strand instead of O(turns × m).

## 2. Prototype

* **Data.** The shipped curly preset (Clump, Curl coherent by clump, frizz Noise, Width), fitted to
  the app head (17.5 cm: curl radius 0.393, 0.458 turns a unit, width 0.0070), 400 hairstyle guides,
  10% preview density (12,744 strands; preview invariance makes these the render's strands).
  Coily is synthesized on the same bases: coil radius 3 widths (0.021), 3 turns a unit,
  23 turns a strand.
* **Sampling test.** The same graph evaluated with Resample (and so Scatter, Interpolate and Curl) at
  16, 40, 64 and 128 segments, against 256 segments, the most Resample allows. Measured as the
  farthest any dense point is from the coarse polyline.
* **Sweep test.** The truth is the analytic sweep at 4096 samples, with frames and arc length from a
  fixed 256-sample table along the base spline, so the curve is the same at any point count, and the
  offset eased in over the first 2% (continuous at the root). Representations measured against it:
  polylines at m points per turn, Douglas-Peucker cuts, and the sweep rebuilt at the chord formula's
  m.
* **Generic test.** Final strands (frizz included) as least-squares clamped cubic B-splines with
  16-bit control points: the fewest control points within ε.
* 136 of 2000 sweep bases whose 16-point fit folds back (a kink of the incoming strand) are left out
  of the sweep statistics and counted.

## 3. Results

Resolution dependence of the shipped preset (mm; strand width 0.07 mm):

| segments | points a turn | mean | p99 | max |
| --- | --- | --- | --- | --- |
| 16 | 4.5 | 1.02 | 1.86 | 2.43 |
| **40 (shipped)** | 11.1 | **0.40** | **0.82** | **1.03** |
| 64 | 17.8 | 0.32 | 0.65 | 0.80 |
| 128 | 35.7 | 0.17 | 0.30 | 0.50 |

Polylines against the analytic sweep (curly: radius 0.39 cm, 3.5 turns; coily: 0.021 cm, 23 turns):

| m a turn | curly p99 (widths) | chord formula | bytes | coily p99 (widths) | chord formula | bytes |
| --- | --- | --- | --- | --- | --- | --- |
| 11 | 4.73 | 2.27 | 478 | 0.20 | 0.12 | 3063 |
| 16 | 2.78 | 1.08 | 690 | 0.10 | 0.06 | 4450 |
| 32 | 1.33 | 0.27 | 1368 | 0.03 | 0.014 | 8888 |
| 64 | 0.78 | 0.07 | 2724 | 0.01 | 0.004 | 17764 |

Storage at a tolerance (bytes a strand):

| tolerance | curly: polyline cut / sweep | coily: polyline cut / sweep |
| --- | --- | --- |
| 0.1 widths | 3105 / 132 (**24x**) | 4458 / 132 (**34x**) |
| 0.25 widths | 2062 / 132 (16x) | 2895 / 132 (22x) |
| 1 width | 1037 / 132 (8x) | 1394 / 132 (11x) |

The sweep rebuilt with the chord formula's m stays within 0.9-2.7 widths p99 on curly. The base's
own curvature needs m about 1.6 times the formula's (§1.2).

Points to draw within half a pixel:

| framing (px a unit) | curly: radius px → points | coily: radius px → points |
| --- | --- | --- |
| 50 (head filling 1080p) | 19.6 → 50 | 1.05 → 72 |
| 12 | 4.7 → 25 | 0.25 → ribbon, 9 |
| 4 | 1.6 → 15 | 0.08 → ribbon, 9 |

The shipped curly strand has 41 points at every distance: too few at hero (11 points a turn, a chord
of 0.8 px at 50 px a unit), too many far away. **On coily hair, 41 points over 23 turns is
1.8 points a turn: pure aliasing.** A correct polyline needs 93-301 points a strand; a 1M-strand
coily groom would carry 0.1-0.3 G points. As sweeps it is 132 MB at rest and draws 9 points a
strand beyond hero distance.

Generic compression of the final strands: 2.0x (0.01 widths), 2.7x (0.1), 3.2x (0.25), 5.5x (1 width)
smaller than dense. Against Douglas-Peucker cuts it is 1.8-2.6x.

Reconstruction on the CPU (24 threads): 121-147 Mpts/s for sweeps (base spline, table lookups,
offsets); frame tables 2.2 µs a strand. A 24-control-point spline evaluates at 320 Mpts/s.

## 4. Go / no-go

* **Go: resolution-independent Curl.** It fixes a measured 0.4 mm drift and a root kink, and is the
  prerequisite for the rest.
* **Go: sweeps for display.** Coily hair becomes affordable and curly hair is drawn right at every
  distance.
* **No-go: generic spline compression for the viewport.**

## 5. Implementation plan

* **B1, Curl v2 math (core, 2 days; a look change, goldens re-recorded).**
  * Compute each strand's (or clump's) frames and arc length on a fixed-resolution table along its
    base (64 samples, the base resampled smoothly with centripetal Catmull-Rom), not on the input's
    points.
  * Seed the frames well conditioned (the tangent at the root, across its least-aligned axis,
    turned to the scalp normal's projection when that is well defined).
  * Ease the offset from 0 at the root over the profile's first key instead of jumping to
    profile(0).
  * Smooth the clump axis (a 32-sample spline instead of linear frame interpolation).
  * The point count then only samples one curve. Check with a test that 40 and 256 segments agree
    within the chord formula. GPU look chain parity (CurlCS reads the same tables).
* **B1 done (2026-10-09, engine/core/src/operators/curl_op.cpp; measured by
  `tools/research/curl_sweeps/measure_curl_b1.cpp`, before = `legacy_curl.h`, a copy of the old
  operator).**
  * Every strand curls about an *axis*: its clump's middle (32 samples, as before) or its own
    points (at most 32; more points are sampled down to 32 on their smooth curve, so a kink finer
    than that, such as a Clump center's, does not lengthen the arc). The axis is a cubic Hermite
    curve through its samples (slopes of the parabola through each sample and its neighbours,
    spaced by their distances), its arc length by two-point Gauss per segment, its frames by
    double reflection with the curve's own tangents; between samples, one reflection by the
    Hermite step from the sample before (not `p - x[k]`: no cancellation for small steps).
  * Each point takes the offset at its share of the strand: `r · vary · profile(t) · ease ·
    [cos θ N + sin θ B]` with `θ = 2π f s + φ`, `s` the axis's arc. A wave or zigzag uses
    `N cos(plane) + B sin(plane)` times `sin θ` or the triangle wave. The ease is a smoothstep
    over the first quarter turn or the first quarter of the strand, whichever comes first. The
    root stays.
  * The seed is the scalp normal across the way the strand leaves (the chord to its point a 31st
    of the way along, on the smooth curve), squared to the root tangent. It is still
    ill-conditioned for strands leaving along the normal. Blending toward a fixed perpendicular
    only moves the singular direction (hairy ball), and the shipped presets on the strokes groom
    found it (scale invariance and GPU parity both failed). The GPU therefore makes the seed's
    inputs with exact rounding (`divExact`, `sqrtExact`, precise sums).
  * **Points per turn:** Curl adds points (`tolerance` ε as a share of the radius gives
    `m = ⌈π / acos(1 − ε)⌉`; the default 0.5 gives 3 a turn, enough that no curl aliases). Each
    segment of a strand is cut into as many pieces as its most-turning segment needs: one count a
    strand, rarely changed by a small edit. A zigzag gets points at the kinks the segments cut by
    more than ε. `maxSegments` caps it (default 256). Points added change the layout, so the GPU
    preview starts after such a Curl (look_chain.cpp `curlAddsPoints`). B2's draw-time
    tessellation is the GPU's way to points a turn.
  * Results (app head, 10% density, against the same groom at 256 segments; mm; strand width
    0.07 mm):

    | groom, segments | points a strand before → after | groom's curl off the limit, mean / p99 / max | Curl alone, each point off the limit at its share, mean / max |
    | --- | --- | --- | --- |
    | curly, 40 (shipped) | 41 → 41 | 0.402 / 0.822 / 1.032 → 0.291 / 0.640 / 0.876 | 0.021 / 0.198 → 0.025 / 0.167 |
    | curly, 64 | 65 → 65 | 0.320 / 0.645 / 0.804 → 0.225 / 0.557 / 0.751 | 0.005 / 0.063 → 0.006 / 0.067 |
    | curly, 128 | 129 → 129 | 0.170 / 0.298 / 0.497 → 0.131 / 0.291 / 0.509 | 0.002 / 0.041 → 0.004 / 0.080 |
    | coily, 40 | 41 → 94 (max 161) | 0.337 / 0.634 / 0.882 → 0.236 / 0.602 / 0.873 | 0.095 / 0.563 → 0.014 / 0.134 |
    | coily, 64 | 65 → 117 | 0.246 / 0.557 / 0.750 → 0.197 / 0.527 / 0.677 | 0.085 / 0.563 → 0.002 / 0.082 |

    The floor under the groom's numbers is the input's own resolution dependence: the nodes
    before Curl, with the limit's input against its polyline at 0.24 / 0.63 / 0.88 at 40 segments.
    The new curly sits on it. The old Curl added its root kink and C⁰ frames, and the old coil
    never converged: its phase drifted with the input's fine detail, up to 0.56 mm, about 2.7
    radii. On analytic bends (test_shapes `Curl.ConvergesToOneCurveAtAnyPointCount`), each point
    lies on the 257-point curl to 1e-3 at 17 points, 3e-4 at 33, 9e-5 at 65 and 1.5e-5 at 129
    (radius 0.1). That is 25 to 40 times below the chord of a turn at that count.
  * Time at 500k strands of 16 points (CPU, 24 threads): each strand on its own, 23 → 57 ms (2.5x,
    the Hermite axis and Gauss arcs); by clump, 67 → 77 ms; adding points (34 a strand out)
    210-280 ms. On the GPU (gpu.bench, 500k × 16, each strand on its own) CurlCS takes 1.8 ms on
    DirectX 11 and 2.1 ms on OpenGL, against 2.2-3.7 and 3.5-4.6 ms before (that run was on a
    busier machine). By clump the axes take a thread per sample, two clumps a group: about 0.2
    ms for 8k strands in 60 clumps, where one thread per clump took 6 ms.
* **Curl v2 spacing (2026-10-09, engine/core/include/groomlab/curl_phase.h).** Two params on top of
  B1: `frequencyProfile` (a ramp, root to tip, a share of `frequency`) and `spacingRandom` (0-1).
  The curl turns at `f_local = f · profile(t) · (1 + spacingRandom · n(τ))` and
  `θ = 2π ∫ f_local ds + φ = 2π (f w + sign(f) · spacingRandom · S(|f| w)) + φ`:
  * `w = ∫ profile ds` is the axis's *profiled arc*, tabled with the arc at its 32 samples (two-point
    Gauss per segment, split at the ramp's keys so a straight ramp's kinks integrate exactly);
    between samples, the same quadrature on [0, u].
  * `n` is a sum of two layers (cells of 2 and 1.25 turns, weights 0.6 and 0.4), each
    `8 (v1 - v0) u² (1 - u)²` per cell from hashed lattice values in [-1, 1). That is C¹, 0 at the
    cell ends and |n| < 1, so the phase never turns back. Its integral telescopes:
    `N(x) = (4/15)(v_i + (v_{i+1} - v_i) S5(u) - v_0)`. The noise runs in turns `τ = |f| w`, which
    is scale-invariant. Its key comes from the bits of the strand's (or clump's) four draws, so the
    GPU needs no new per-strand data.
  * The tolerance's refinement takes the most of a segment's turns and its length times the
    local frequency at either end. A zigzag's kinks are found by bisection on the share. With
    the defaults, `w` is the arc and every expression is the B1 one, so goldens are unchanged.
  * The GPU mirrors this in look.slang (`curlAngle`, `profiledSegmentArc`). A clump's axis table
    grows from 16 to 17 floats a sample (the profiled arcs).
* **B2, sweep channel (core + GPU, 3-4 days).**
  * When Curl is the last shape operator before the output (or only Width, color and RenderData
    follow), the display path receives per strand its base points (the Curl's input), f, φ₀, r and
    the clump table, instead of dense points.
  * A compute pass per view tessellates m_px points a turn (§1.2), with the envelope ribbon below
    half a pixel (§1.3).
  * Arnold, exports and cards tessellate at render resolution from the same numbers, so render and
    viewport draw one curve.
* **B3, coil envelope coverage (1 day).** c_env in the line shaders, shared with clump LOD's
  impostor path.
* **Risks.**
  * Operators after Curl (Noise, Collide, Gravity, Flyaway) need real points: the sweep path applies
    only when none follows, or they are applied on the GPU to the tessellated points.
  * Bit-exact CPU/GPU parity for the tables (precise arithmetic as in look.slang).
  * Users who relied on the faceted look of low segment counts see a slightly rounder curl.
