<!-- SPDX-License-Identifier: Apache-2.0 -->
# Math in GroomLab

Where the engine's math lives, so that a function is written once and a fix lands in the nodes,
the expression language and the GPU preview together. Read [DEVELOPING.md](DEVELOPING.md) section
6 ("Rules") first: the CPU is the reference, and its results are the same bits on every platform.

## Where it lives

All under `engine/core/include/groomlab/`:

| Header | What is in it |
| --- | --- |
| `vec3.h` | `Vec3f`, the core's vector. |
| `math.h` | Includes the two above and the shared header; adds `smoothstep`, `gaussstep`, `remapFalloff`, `fract`, `truncToInt`, `normalizedRecip`, `rotateAxisAngle`, mirroring, `roundToDecadeStep`. Start here. |
| `shared/groom_math.h` | The math the CPU and the GPU kernels share, in C++ and Slang at once (below): `mix1`, `clamp01`, `fade`, `smoothstepStd`, `linearstep`, `biasOf`, `gainOf`, `fitOf`, the ramp cubic, the noise's lattice hash, gradients and `latticeNoise` (3D, 4D), the curl spacing noise and phase, and the expression functions' GPU codes. |
| `portable_math.h` | `pm::sin`, `pm::exp`, `pm::pow`... the core's only transcendental functions (below). |
| `random.h` | Hashes and random numbers: `hash64`, `hashCombine`, `unitFloat`, `Rng`, `floatBits` / `floatBitsZero`, `hashFloat`, `hashDouble`, `hashString`, `hashChars`. |
| `noise.h` | Gradient noise: `gradientNoise`, `fractalNoise`, `vectorNoise` (and the fast `VectorNoise` for a strand's run of points), 4D forms, `octaveWeights`, SeExpr's FBM (`fbmNoise`...), `cellHash`, `periodicNoise`. |
| `color.h` | `rgbToHsl`, `hslToRgb`. (Hair's own color model is `hair_color.h`.) |
| `ramp.h` | `Ramp`, the monotone cubic curve every profile is; `Ramp::tangent` is the slope the GPU takes with the keys. |
| `curl_phase.h` | What Curl's phase is (the shared header holds the functions). |
| `frames.h`, `curve.h` | Frames along a strand, resampling. |

Before writing a helper in an operator (a hash, a lerp, a smoothstep, a clamp), look here. If two
"copies" give different numbers, keep both as named variants (`floatBits` and `floatBitsZero`,
`normalized()` and `normalizedRecip`) rather than changing either one's output.

## The shared CPU/GPU header

`shared/groom_math.h` is compiled as C++ into the core (namespace `groomlab::shared`; `math.h`,
`noise.h` and `curl_phase.h` bring what they use into `groomlab`) and included by
`engine/gpu/shaders/look.slang`. The kernels depend on it in `engine/gpu/CMakeLists.txt`, so an edit
recompiles them; the DirectX kernel cache is keyed by each kernel's HLSL, which holds it.

It is written in what both languages read alike: functions of `float`, `int`, `uint`, `float3`,
`const` locals, fixed-size arrays, constant tables. Its shim maps the rest: `float3` is `Vec3f` in
C++; `u64` is a `uint64_t` in C++ and a `uint2` (low, high) in Slang, as DirectX 11 has no 64-bit
integers, with `mul64`, `shr64`, `lo32` and `hi32` for its arithmetic; `pmPow` and `pmLog` are the
portable math in C++; `floor`, `abs`, `clamp`, `sqrt`, `dot` and `asuint` are `std::`'s in C++.

To add a function to it:

1. Write it in the shared subset, `inline`, with a comment saying what it is. A constant table is
   `SHARED_CONST`, a small constant loop takes `SHARED_UNROLL`. Use `pmPow`/`pmLog` for powers and
   logarithms; add a shim pair (C++ and Slang) for another transcendental.
2. Use it from both sides: in C++ as `shared::name` (or a `using shared::name;` in the header that
   owns the topic), in `look.slang` by its name. Delete the copies it replaces.
3. Keep the C++ arithmetic in the order it was: the CPU's bits must not move.
4. Build (`tools/dev/vsenv.ps1 cmake --build --preset ninja-maya2022`; `gpu.kernel_targets` also
   compiles every kernel for SPIR-V and Metal) and run the tests below.

## Portable math, and why

The core's float math is IEEE 754 as written: no contraction, no fast math (`CMakeLists.txt`,
"Determinism"), so additions and multiplications give the same bits on every compiler. The C
runtimes' float `sinf`, `expf`, `powf`... do not: MSVC and glibc differ in the last bit now and
then, and that bit grows into other strands (a clump picked, a cluster, a collision). So the core
calls `pm::` (`portable_math.h`): with `GROOMLAB_PORTABLE_MATH` on (the default), each float
function is the runtime's double function rounded to float, the same bits on Windows, Linux and
macOS. Never call `std::sin` and the like on floats in the core; `std::floor`, `std::sqrt` and
`std::fabs` are exact and fine. `tests/data/golden/PresetDigests.json` holds every shipped preset's
bits, recorded with portable math on.

## Adding an expression builtin

In `engine/core/src/expression.cpp`:

1. A code in `enum Fn`, anywhere after `kNoise` (the codes up to it are inlined into the
   interpreter's loop and come first).
2. Its row in `kFns`: code, name, arguments and a one-line description (what the editors show:
   Maya's `expressions.py` reads the list from `groomlab_core.expression_functions()`), arity, kind
   (`Math`: numbers, or vectors component by component; `Scalar`: numbers only; `Vector`: by a
   signature; `Curve`; `Map`), and for a vector function its signature, result count and defaults.
   Rows go in any order; the editors list them in this one.
3. Its case in `scalarFn` (kinds Math and Scalar) or `vectorFn` (Vector), built from the shared
   headers above.
4. A line in `web/content/docs/expressions.mdx`'s Functions section, naming it in a code span
   (`Expression.EveryBuiltinIsDocumentedAndHasItsOwnGpuCode` fails until it is there).
5. A test in `tests/engine/core/test_expression.cpp`.

To run it on the GPU too (a number function of numbers only): a `kField*` code in
`shared/groom_math.h` (any unused number), a case of that name in `look.slang`'s `fieldCall`, and
the code in its `kFns` row. It is CPU-only until then, as a new feature should first be.

## Which tests guard what

| Test | Guards |
| --- | --- |
| `Determinism.EveryPresetMakesTheRecordedBitsOnEveryPlatform` (`test_determinism.cpp`) | Every shipped preset's output, bit for bit, against `PresetDigests.json`. Any change to CPU arithmetic order shows here. Never re-record it for a refactor. |
| `Determinism.*` | The same output in any order, on any thread count, through a groom file. |
| `Operators/Golden.*` (`test_golden.cpp`) | Each operator's output against stored files (counts and IDs exact, values within 1e-4). |
| `Expression.*` (`test_expression.cpp`) | The language: every builtin's values, errors, the docs page listing them, the GPU codes. |
| `Backends/LookEngineTest.*` (`tests/engine/gpu`, label `gpu`) | The GPU kernels against the CPU operators on DirectX 11 and OpenGL, within float rounding. |
| `gpu.kernel_targets` | Every kernel still compiles for SPIR-V and Metal. |

Run the narrow ones while working (`python tools/dev/test_changed.py`, or
`ctest -C RelWithDebInfo -R "Determinism|Golden|Expression"` in `build/ninja-maya2022`), and
`ctest -C RelWithDebInfo -L gpu` when the shared header or `look.slang` changed.
