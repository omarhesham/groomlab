# Baked guide-weight field for Interpolate

Research note, 2026-10-09. Prototype: `tools/research/baked_binding/bench_baked_binding.cpp`
(opt-in target `research_baked_binding`, `-DGROOMLAB_BUILD_RESEARCH=ON`). Raw results:
`tools/research/baked_binding/results/2026-10-09_*.{txt,jsonl}`.

## Verdict

| Idea | Verdict | Why (measured) |
| --- | --- | --- |
| Baked binding (per-sample records, strands gather 3) | **No-go** | The current binding is 0 to 25% of a first Interpolate evaluation and is already memoized and incremental. A baked field as accurate as needed costs more samples than it saves. |
| Baked shape field (strands blend 3 sample curves) | **No-go as a speedup; possible as a look change** | It blends twice as fast (52-62 ms against 104-110 ms at 1M strands), but at about 100 samples a guide the shapes move by 0.25 mm on average and 1 mm at p99 on a 17.5 cm head. That is a different groom, not the same one faster. |
| Second-order reconstruction (Hermite midpoint) | Math result, no product use | It converges at cubic order, as derived: at about 90 samples a guide, error is 0.02 mm mean and 0.12 mm p99. |
| **Compact binding records for the GPU** (spun out of this study) | **Go (small)** | 208 B a strand becomes 36 B: 208 MB becomes 36 MB at 1M strands. The largest point error is 5 µm (2.8e-5 of the head's size). |

## 1. What Interpolate binds today

Guides g have roots x_g and reaches R_g. R_g is `reach` times the distance to the guide's
`neighbors`-th nearest guide. Each strand at root x gets a base for each candidate guide (the 14
nearest when neighbors = 3):

  b_g(x) = (R_g - d_g(x))_+ / (R_g d_g(x)),   w_g = (b_g / max_h b_h)^p,   W_g = w_g / sum_h w_h

Here d is the straight-line distance, or the distance along the skin with `followSurface`. Over the
outer quarter of the best-covering guide's reach, W is mixed with a Shepard-Franke "fringe" blend
(`kFringeBand`). Paths around folds add a twist. The shape at sample t is
x(t) = x_s + sum_g W_g R(n_g -> n_s) o_g(t). `R` is the minimal rotation between root normals, and
o_g is the guide's offset curve at the common resolution. `preserveLength` and an arc-length
resample follow.

The binding `(g, W_g, twist)` depends only on root positions. The blend depends on guide shapes.
`CandidateCells` (SSE over cell lists) does the k-nearest search, records are kept in 256-strand
chunks, and a guide-root move rebinds only the chunks it reaches.

## 2. The baked field: math

### 2.1 Smoothness of the weights

Fix a sample spacing h. Away from guide roots and reach boundaries, W is C^∞ in x for Euclidean d,
and also for geodesic d inside the injectivity radius of the skin. Three places need care:

* **At a guide root (d_g -> 0).** Use b_g ~ 1/d_g and write β_h = b_h. Then
  W_g = 1 / (1 + d_g^p S(x)) with S = Σ_{h≠g} β_h^p. For p = 2, d_g² is smooth, so W is smooth,
  but its Hessian there is H ≈ -2 S I. For blue-noise guides at spacing ℓ, β_h ≈ c/ℓ for the
  k ≈ 6-8 guides that weigh, so **‖H‖ ≈ 2 k c² / ℓ² ≈ 12/ℓ²**. This is the Shepard "flat spot":
  the weight drops from 1 to 1/2 within about 0.4 ℓ of every root. For p < 2 there is a cusp, and
  the local error is O(h^p).
* **At a reach boundary (d = R_g).** w_g ∝ (R - d)^p is C^{p-1}. For p = 2 it is C¹ with a jump in
  the second derivative, so linear interpolation is still O(h²) there, with the jump as its constant.
* **Hard edges.** Parts, region maps, the end of a `followSurface` reach ("Beyond") and top-k
  truncation are discontinuities. Any interpolation is O(1) wrong in the cells they cross, so those
  cells must fall back to the per-strand search.

### 2.2 Linear (barycentric) reconstruction

The scalp is covered by a lattice of level L per triangle (points (i/L, j/L), shared on edges). A
strand in a sub-triangle with corners v_k and barycentrics λ_k gets
W̃_g(x) = Σ_k λ_k W_g(v_k). This is a partition of unity because each corner's weights sum to 1.
Taylor expansion about x:

  W̃_g(x) - W_g(x) = ½ Σ_k λ_k (v_k - x)ᵀ H_g(ξ_k) (v_k - x),   so |e_W| ≤ ½ ρ² ‖H‖ ≤ (h²/6) ‖H‖

Here ρ² = Σ λ_k |v_k - x|² ≤ h²/3. With the flat-spot Hessian this gives **e_W ≈ 2 (h/ℓ)²**.

### 2.3 From weight error to shape error

Both weight vectors sum to 1, so Σ_g ΔW_g = 0. Therefore

  |Δx(t)| = |Σ_g ΔW_g (R_g o_g(t) - ō(t))| ≤ ½ ‖ΔW‖₁ · D(t),   D(t) = max_{g,g'} |R_g o_g(t) - R_g' o_g'(t)|

The error scales with how much neighbouring guides disagree, not with how long they are. A combed
groom where neighbours agree hides it; a messy groom shows it. `preserveLength` adds a perturbation
of the same order.

### 2.4 Second order: the Hermite-midpoint rule (new)

Both linear interpolation (+½ Σ λ (v-x)ᵀH(v-x)) and a first-order Taylor blend
Σ λ_k [W(v_k) + ∇W(v_k)·(x - v_k)] (which errs by -½ Σ λ (v-x)ᵀH(v-x)) are wrong by the same
quadratic term, with opposite signs. Their average

  W̃_g(x) = Σ_k λ_k [ W_g(v_k) + ½ ∇W_g(v_k) · (x - v_k) ]

is therefore **exact for quadratic W**, so its error is O(h³ ‖D³W‖). It stays a partition of unity
because Σ_g ∇W_g = 0. Weights can go slightly negative; the prototype clamps them at 0 and
renormalizes. The gradients along the skin come from central differences: the operator itself binds
4 probes a sample at ±h/4.

Measured convergence ratios when h is halved (100k strands, 1000 guides):

| samples a guide (β = ℓ/h) | linear mean err (mm) | ratio | 2nd-order mean err (mm) | ratio |
| --- | --- | --- | --- | --- |
| 10 (2) | 1.76 | | 0.49 | |
| 29 (4) | 0.71 | 2.5 | 0.114 | 4.3 |
| 92 (8) | 0.234 | 3.0 | 0.0178 | 6.4 |
| 367 (16) | 0.060 | 3.9 → 4 (h²) | 0.0020 | 8.9 → 8 (h³) |

Both rates approach the orders derived. Samples a guide ≈ 1.4 β² on this head.

### 2.5 Incremental update and memory

A guide root move changes W only inside the union of the reaches of the moved guides and of the
guides whose `neighbors`-th neighbour changed. Measured: 3 moved roots changed 3.9% of samples
(1122 of 28,676 at 1000 guides). A guide shape edit changes no binding at all, as today.

Memory: S samples × one record. At about 90 samples a guide, 1000 guides is 90k records
(about 8 MB). 5000 guides is 450k records, which is comparable to the strands themselves. A
50k-vertex scalp used directly as the samples is 50k records (about 4 MB), but vertices only help
where they are dense relative to the guides (§4).

Complexity: bake O(S · bind); gather O(N · 3k) merges of up to 3 × 16 entries; current binding
O(N · (cell scan + k log k)).

## 3. Prototype

* **Data.** The app head (`tests/data/app/head_scalp.obj`: 1225 vertices, 17.5 cm). Strands come
  from the core's Scatter and Resample (16 points). Guides follow a hairstyle: combed back and down,
  ±20° jitter between neighbours, gravity sag, 8 points. Counts: 100, 1000 or 5000 guides; 100k,
  500k or 1M strands. A midpoint-subdivided head (19.6k and 78k vertices) is also run.
* **Reference.** The current `Interpolate` operator (`keepClearance` off: the walk is orthogonal).
  Its records come from `interpolateState` and `interpolateRecords`, its guide rows from
  `interpolateGuideRows`. The prototype re-blends the operator's own records and reproduces its
  points bit for bit (harness check: max difference 0).
* **Baked.** Lattice samples are bound by the operator itself (strands rooted at the samples), so
  `followSurface` paths, fringe and twists are all included. Strands then merge their sub-triangle's
  three records. Variants: linear, linear with a 1% weight drop, second order, a baked shape field,
  and a guide-root move.
* Timings are the best of 3 to 5 runs on 24 threads (TBB from Maya 2022).

## 4. Results

Current Interpolate (blend = re-evaluation with the binding kept; bind = the first evaluation of a
new node minus that):

| strands | guides | first eval ms | blend alone ms | bind ≈ ms | guides a strand |
| --- | --- | --- | --- | --- | --- |
| 100k | 100 / 1000 / 5000 | 12.4 / 18.1 / 19.7 | 12.4 / 17.0 / 24.0 | ≈0 / 1 / ≈0 | 7.4-7.6 |
| 500k | 100 / 1000 / 5000 | 48.5 / 41.6 / 56.9 | 51.9 / 44.7 / 53.1 | ≈0-4 | 7.4-7.6 |
| 1M | 100 / 1000 / 5000 | 133.9 / 96.4 / 92.1 | 109.3 / 90.1 / 101.5 | 25 / 6 / ≈0 | 7.4-7.6 |

The binding is within timing noise except at 1M strands with 100 guides. The blend is the cost.

Baked field against the operator's points. The error is per strand, its farthest point; ×10 for mm.
It is the same at every strand count, as predicted by the dependence on h/ℓ alone.

| guides | samples a guide | linear mean / p99 / max (mm) | 2nd order mean / p99 / max (mm) | shape-field blend 1M ms (vs 104-110) |
| --- | --- | --- | --- | --- |
| 100 | 12 / 47 / 104 | 1.89 / 6.6 / 15.8 · 0.54 / 2.1 / 4.6 · 0.25 / 1.0 / 2.8 | 0.52 / 2.8 / 6.5 · 0.07 / 0.46 / 3.0 · 0.020 / 0.14 / 2.6 | 61.7 / 51.6 / 61.8 |
| 1000 | 10 / 29 / 92 | 1.75 / 6.3 / 12.3 · 0.71 / 2.8 / 7.1 · 0.23 / 0.96 / 3.1 | 0.49 / 2.6 / 9.2 · 0.11 / 0.71 / 3.4 · 0.018 / 0.12 / 0.71 | 54.2 / 59.9 / 59.9 |
| 5000 | 6 / 23 / 91 | 2.80 / 9.6 / 21.2 · 0.89 / 3.4 / 9.2 · 0.24 / 1.0 / 3.1 | 1.06 / 4.8 / 11.8 · 0.16 / 1.0 / 3.9 · 0.019 / 0.12 / 0.68 | 48.7 / 47.9 / 51.6 |

* The spread test (corners disagreeing by more than 0.25 in some guide's weight) would send 30-87%
  of strands back to the search. The weights are not smooth at affordable sample spacings.
* Gathering 1M strands takes 11-15 ms (linear). The prototype's second-order gather is
  unoptimized at 130-300 ms.
* Moving 3 guide roots rebinds 1.5-15 ms of samples plus 0.3-6 ms of gathers. The current rebind
  is within noise of 0.
* The 78k-vertex subdivided head with its vertices as samples (73 a guide) has 2nd-order max errors
  of 6.8 mm. Faceted vertex normals and kinks make vertices worse samples than a lattice.

Compact records (`--compact`). Each normalized weight is quantized to 16 bits, plus a 16-bit guide
index, an offset a strand, an f16 nearest², and twists in a side table (none on this head). The
blend is from the quantized weights:

| strands | guides | bytes a strand | GPU records today | max point error |
| --- | --- | --- | --- | --- |
| 100k / 500k / 1M | 100 | 35.7 | 208 B (BIND_WORDS = 52) | 4.9e-4 / 5.6e-4 / 5.6e-4 cm |
| 100k / 500k / 1M | 1000 | 36.2-36.3 | 208 B | 3.5e-4 / 3.8e-4 / 4.3e-4 cm |
| 100k / 500k / 1M | 5000 | 36.3 | 208 B | 2.4e-4 / 2.5e-4 / 2.7e-4 cm |

At 1M strands that is 36 MB against 208 MB. The audit's first GPU run at 1M strands is
98 ms CPU and 296 ms to done on DirectX 11; the records are its largest upload. That saving is
estimated from the upload bandwidth and is not yet measured (batch B2 below).

## 5. Go / no-go

* **Baked binding: no-go.** The binding is not the cost (§4). Accuracy equivalent to the current
  binding needs the second-order rule at about 90 samples a guide plus fallbacks at parts and
  regions. That only beats the search when strands outnumber samples about 10 to 1, which is rare
  with 1000 or more guides. It would also make PreviewInvariance depend on a lattice.
* **Baked shape field: no-go as a "faster, same groom".** It halves the blend, but its look error
  (0.25 mm mean, 1 mm p99 at about 100 samples a guide) breaks the GPU = CPU bit-exact contract and
  the goldens. It could be reconsidered only as an explicit new Interpolate mode.
* **Compact records: go.** This is the useful by-product.

## 6. Implementation plan (compact records)

* **B1 (core, about 1 day).** The binding stores each normalized weight as u16 and the guide index
  as u16, falling back to u32 above 65,535 guides. Twists go in a per-chunk side table. The blend
  reads the dequantized weights, so the CPU defines the quantized look: goldens re-recorded, changes
  at most 5 µm. Tests: `PreviewInvariance.*`, the Interpolate incremental and verify checks,
  `test_golden`.
* **B2 (GPU, about 1-2 days).** `look_chain`/`look_engine` upload variable-length records (offsets
  plus packed pairs) instead of BIND_WORDS = 52, and `InterpolateCS` reads them. Measure the first
  run at 1M strands with `groomlab_gpu_bench`. Check parity in `groomlab_gpu_tests` on both APIs.
* **Risks.** Bit-exact normalization on the GPU (sum the quantized weights in the CPU's order),
  guide sets above 65k, and region and twist side tables in the incremental path.

## 7. Done (B1 and B2, 2026-10-09)

* **Format.** Each weight is its share of the strand's weights in 65536ths, the shares summing to
  65536 exactly (Hamilton's largest remainders), kept as the share less one in 16 bits; a guide
  whose share rounds to 0 is dropped (7.56 guides a strand become 7.40 on the head). The blend reads
  (code + 1) / 65536, exact in a float, so no normalization is left to agree on. Guide indices are
  16 bits up to 65,535 guides, else 32. Twists go in a sparse per-chunk table (record, twist).
  nearest² stays a float: in f16 its rounding moves `limitDistance` strands near the radius by up to
  about 0.2 mm (the influence ramp's last twentieth). The GPU reads two words a strand (its records'
  start, twist flag and count; nearest²), then a word a record (index | code << 16; two words past
  65,535 guides), then the strand's twists if any. A stroke binding again writes only its chunks, in
  place when they fit, else into the room the buffer keeps after the records (1/16), else all again.
* **Error** against the unrounded weights (app head, 1000 guides, `--dump`/`--compare`, per strand
  its farthest point): without keepClearance mean 0.22 µm, max 4.7 µm (500k strands); with
  limitDistance max 2.3 µm. With keepClearance (the default) the same, except that the clearance
  walk's per-point decision flips on 15 of 500k strands (one point moves up to 56 µm, the rest of the
  strand about 2 µm): any rounding-sized change does that.
* **Memory.** CPU records at 1M strands, 1000 guides: 73.5 MB becomes 38.6 MB. GPU binding: 208 MB
  becomes 37.6 MB on the head (37.1 MB on the bench's sphere).
* **Upload** (`groomlab_gpu_bench`, sphere, 326 guides, the run from Interpolate, medians of 3
  interleaved runs, a second engine: kernels made): the first run's upload at 1M strands
  371.8 MB becomes 201.0 MB; its CPU ms 88.3 becomes 54.5 (DirectX 11) and 86.7 becomes 55.3
  (OpenGL), done 181.4 becomes 138.0 and 120.7 becomes 89.4. At 500k: 43.2 to 26.4 and 52.0 to 34.1
  CPU ms. Warm dabs and the InterpolateCS time are unchanged (a dab that keeps the roots uploads no
  binding).
