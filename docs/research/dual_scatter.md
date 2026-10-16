# A closed form for the viewport's dual scattering

Research note, 2026-10-09. Prototypes: `tools/research/dual_scatter/fit_dual_scatter.py` (the
shader's shading ported to numpy, the fit and the error tables), `count_real_shader.py` (the real
`hosts/app/shaders/hair_lookdev.frag` through qsb → HLSL 5.0 → fxc /O3), `count_instructions.py` and
`shade_variants.hlsl` (a synthetic port). Results: `results_2026-10-09*.{txt,json}`.

## Verdict: no-go for a fitted closed form; a small exact factorization is worth landing

* **Upper bound.** The global multiple-scattering spread lobes are 7.3% of the per-point shading
  pass, and the whole dual-scattering branch is 12.1% (instruction slots of the real shader, D3D11
  path). No closed form can save more than that.
* **Exact factorization.** Evaluating the lobes' azimuthal, Fresnel and attenuation terms once for
  both spreads already saves 2.2% with zero error. fxc's CSE recovers the rest of the duplicate call
  on its own.
* **Azimuth-averaged closed form.** It keeps the energy (mean bias -0.5%, worst -4%) but changes the
  angular look by 18% per light direction on average (p95 70%, worst cells over 100%) to save at most
  the remaining 5%. Not worth a look change that only an Arnold comparison could justify.
* **Where the shading time goes.** Each point runs shadeLight 11 times (3 lights + 8 environment
  lights): 73% of its light work is the environment's. That, not the multiple-scattering term, is the
  lever (§5).

## 1. The model as shipped

For one light with n hairs in the way (from the light's deep opacity map), per shadeLight:

  direct = e^{-n ρ},   T_f = 0.7 a_f^n · ms,   σ_f² = n (B/2)²
  single = Σ_p G(b_p, sin θ_h - α_p) X_p(θ_d, φ, c)
  spread = Σ_p G(√(b_p² + σ_f²), sin θ_h - α_p) X_p(θ_d, φ, c)
  result = [direct (single + 0.7 back G_b) + T_f (spread + 0.7 π back G_b')] √(1 - sin² θ_L)

* p runs over R, TT, TRT, with widths b = (B, B/2, 2B) and centers α = (-shift, shift/2, 3 shift/2).
* X_p = N_p F_p T_p · tint: the azimuthal lobe, Fresnel and attenuation.
* a_f = 0.04 + 0.86 c^1.2 and a_b = 0.02 + 0.3 c^0.85.
* back = 2 a_b ⟨a_f⟩² / π · ms.

The shader evaluates `lobes()` twice: with spread 0, then with σ_f².

## 2. The exact factorization

X_p depends only on the angles and the color, not on the longitudinal width. Hence

  spread = Σ_p G(√(b_p² + σ_f²), sin θ_h - α_p) X_p

reuses the single scattering's X_p. The spread costs three Gaussians instead of a second `lobes()`.
This is exact; the port reproduces the shader to 0 relative difference over the whole table.

## 3. The azimuth-averaged closed form (V2)

Zinke's global multiple scattering is azimuthally broad: after one or more forward scatterings the
azimuth is mixed. Replace the spread's Σ_p X_p(φ) G_p by its azimuthal mean and its longitudinal
moments:

  spread ≈ A(c) G(√(w² B² + σ_f²), sin θ_h - μ · shift)

A is the azimuthal mean of Σ_p X_p over directions, per channel value c, and it does not depend on
roughness. μ and w² are the lobes' luminance-weighted center and second moment. Least-squares fit
over 60 values of c × 3 roughnesses × 20,000 directions:

  A(c) = 0.01026 + 0.13101 c^2.10 + 0.01608 c^4.50   (max abs error 4e-4),   μ = 0.151,   w² = 0.576

## 4. Measurements

### 4.1 Error of V2 against the shader

20,000 random light and view directions around a strand. Melanin through the eumelanin and
pheomelanin spectra at redness 0.15, as the shader computes it; roughness through d'Eon's fits;
n ∈ {0.5, 1, 2, 4, 8, 16}. Each cell gives the mean relative error of a light's radiance (luminance,
normalized by the cell's mean), the worst p95 over counts, and the mean energy bias:

| melanin \ roughness | 0.1 | 0.2 | 0.3 | 0.45 | 0.6 |
| --- | --- | --- | --- | --- | --- |
| 0.05 | 0.32 / 1.87 / +0.007 | 0.33 / 1.43 / +0.004 | 0.35 / 1.43 / -0.000 | 0.35 / 1.25 / -0.011 | 0.31 / 0.97 / -0.025 |
| 0.2 | 0.37 / 2.18 / +0.007 | 0.37 / 1.91 / +0.004 | 0.38 / 1.73 / -0.001 | 0.37 / 1.48 / -0.015 | 0.32 / 1.15 / -0.032 |
| 0.35 | 0.28 / 1.68 / -0.002 | 0.26 / 1.56 / -0.001 | 0.26 / 1.36 / -0.002 | 0.24 / 1.07 / -0.010 | 0.20 / 0.76 / -0.022 |
| 0.5 | 0.11 / 1.34 / -0.010 | 0.09 / 0.75 / -0.006 | 0.09 / 0.56 / -0.003 | 0.08 / 0.57 / -0.002 | 0.07 / 0.48 / -0.006 |
| 0.7 | 0.07 / 1.15 / -0.012 | 0.04 / 0.52 / -0.008 | 0.04 / 0.37 / -0.003 | 0.03 / 0.30 / +0.001 | 0.03 / 0.26 / +0.002 |
| 0.9 | 0.05 / 1.00 / -0.012 | 0.03 / 0.41 / -0.008 | 0.02 / 0.28 / -0.003 | 0.02 / 0.21 / +0.002 | 0.02 / 0.19 / +0.005 |

The multiple-scattering share of a light's radiance is 85% for blonde (melanin 0.05), 45% for
melanin 0.35 and 7% for black hair. Errors follow that share: light hair, whose look *is* the
multiple scattering, changes most. Chromaticity of the mean stays within 0.003.

### 4.2 Cost

The real shader, compiled as the D3D11 backend compiles it (instruction slots; loop bodies counted
once):

| variant | per point (HAIR_POINT_SHADE) | per fragment |
| --- | --- | --- |
| shipped | 1599 | 1683 |
| exact factorization (lobes once, three Gaussians for the spread) | 1564 (-2.2%) | 1647 (-2.1%) |
| spread lobes removed (any closed form's best case) | 1483 (-7.3%) | 1563 (-7.1%) |
| dual-scattering branch removed | 1405 (-12.1%) | 1484 (-11.8%) |

In the synthetic all-inlined port, fxc's common-subexpression pass already merges the duplicate
`lobes()` call completely (V0 = V1 = 210 slots a light; 1864 for 11 lights; V2 1856). In the real
shader, qsb's SPIR-V → HLSL translation leaves part of it, which is the 2.2% above.

## 5. Go / no-go and what to do instead

* **No-go: a fitted closed form for the global multiple scattering.** It saves at most 5% beyond the
  exact refactor and changes the look of light hair by tens of percent per direction.
* **Go (trivial, when the shader is next touched): the exact factorization.** Add
  `lobesPair()` → (single, spread), as in `count_real_shader.py`'s PAIR. 2.2% fewer instructions, no
  change to any pixel, and no reliance on a driver's CSE (GLSL drivers vary).
* **Follow-up worth its own study: the environment.** 8 of the 11 shadeLight calls a point are the
  environment's directional lights. Projecting the hair's lobes (in θ_h and φ, per material) onto
  the environment's spherical harmonics once per material change would turn those 8 calls into a
  short SH dot product per point. The ceiling is 8/11 of the light work minus the SH evaluation's
  own cost; that is arithmetic, not measured here. Validate it with `tests/data/app/arnold_compare.py`
  before adopting.
