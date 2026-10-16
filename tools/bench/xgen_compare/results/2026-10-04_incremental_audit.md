# Incremental evaluation and look-operator copies (algorithm audit, batch B): 2026-10-04

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA
GeForce RTX 3090. Medians of 3 runs (min-max in brackets). CPU load was 7-13% before each set.
The "before" binary is main at d31a6c6 with only the new benchmark cases added (the
`BrushStrokeLatencyWithFields` and `ClumpStrengthTweak` budgets). One "before" run overlapped
another process and is not counted; a fourth was taken in its place. Every result is bit for
bit the full evaluation's: the goldens are unchanged, and `Graph::setVerifyIncremental` is on in
the incremental tests.

`groomlab_tests.exe --gtest_filter=Budget.<name>`.

## Brush dabs with field nodes plugged into params (`Budget.BrushStrokeLatencyWithFields`)

This is the bench sphere graph (Scatter, Interpolate, AutoClump, Clump, Noise, Curl, Width,
Resample even) at 500k strands. A FieldNoise is plugged into Noise's amplitude, and a FieldRemap
of it into Clump's strength. The run is two comb strokes of 10 dabs, each evaluated and drawn
into host buffers.

| Case | Before | After |
| --- | --- | --- |
| Dab, sculptDensity 0.35 | 72.9 ms [53.6-76.3] | 19.5 ms [19.3-22.1] |
| Dab, sculptDensity 1.00 | 169.0 ms [131.8-190.1] | 44.7 ms [43.8-46.5] |
| Nodes incremental per dab, at least | 2 | 6 |

Before this change, a plugged field node made its node evaluate in full, along with everything
after it. Now each FieldValue says whether it is strand-local.

## Look operators at 500k strands x 16 points (`Budget.LookOperatorsHalfMillion`, best of 3 each)

| Node | Before | After |
| --- | --- | --- |
| Noise | 39.8 ms [38.8-43.9] | 35.1 ms [33.5-36.7] |
| Curl | 27.7 ms [27.0-34.7] | 21.2 ms [20.4-23.2] |
| Width (unchanged, for scale) | 10.6 ms [9.2-30.7] | 9.7 ms [8.7-10.0] |

Noise, Curl (except by clump) and Clump now write their points anew (`overwritePositions`).
Each strand is copied in just before it is shaped, instead of all of them being cloned first.

## A Clump strength tweak (`Budget.ClumpStrengthTweak`, median of 5 tweaks per run)

The setup is 400 centers, and the node evaluates in full on each tweak.

| Strands x 16 points | Before | After |
| --- | --- | --- |
| 100k | 4.2 ms [4.1-4.4] | 2.7 ms [2.3-2.7] |
| 500k | 18.2 ms [18.0-19.0] | 9.7 ms [9.1-9.7] |

Clump keeps each strand's query (its root on the rest scalp, else its first point) with its
assignment. A full run with the same queries, centers' roots and parts reuses the assignment
instead of searching again. The run also includes the overwrite above.

## A full result diffed against the last one (`strandChangesBetween`, in `LookOperatorsHalfMillion`)

| 500k strands x 16 points | Time |
| --- | --- |
| 1% of the strands moved, other arrays shared | 2.5 ms [2.5-2.6] |
| Widths and a channel written anew too | 3.5 ms [3.4-3.6] |

A node evaluated in full while its inputs changed in only some strands hands on the strands
that differ. Examples are Tangle, BlendNeighbors, a Compound, or an incremental operator that
fell back. A single Noise at 500k costs about 35 ms in full, so the diff pays as soon as one
node after it runs incrementally. For that reason it is always on: `Graph::setDiffFullResults`
can turn it off for timings.

## Unchanged paths (for scale)

| Budget | Before | After |
| --- | --- | --- |
| Cull, 500k x 8, 12 channels (full) | 19.4 ms [17.5-19.8] | 17.2 ms [16.9-17.6] |
| Split, 500k x 8, 12 channels (full) | 43.0 ms [42.4-44.8] | 36.7 ms [35.1-36.8] |
| Collide playback frame, 50k (Collide) | 14.0 ms (8.2) | 11.6 ms (7.2) |

These full paths did not change. Their differences are run-to-run noise.
