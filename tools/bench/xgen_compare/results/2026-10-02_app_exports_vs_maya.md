# GroomLab standalone app vs the Maya plugin: exports, evaluation and playback

Maya numbers: 2026-10-02_groomlab_5e8bc26 (GroomLab 5e8bc26, medians of 3 runs, min-max in brackets), not re-run. App numbers: GroomLab 747fb28, one run of `groomlab_app_tests --gtest_filter=AppBench.*` (app/tests/bench_app.cpp) on the same machine (AMD Ryzen Threadripper PRO 5945WX 12-Cores, NVIDIA GeForce RTX 3090).

The app rebuilds the bench scene without Maya: the same head (17440 triangles, 878 cm2, polySphere UVs) bending over frames 1-100 as an Alembic scalp cache, 808 guides straight along the normals, the same graph (Scatter -> Interpolate -> AutoClump -> Clump -> Noise -> Curl -> Width -> Resample, 16 points a strand); the bend is an approximation of Maya's nonLinear bend (the same handle and keys, the crown swinging about 5 cm), so the scalp moves as much but not identically. The app evaluates through GroomDocument (the GroomScene over the session), as its timeline and export panel do; drawing is offscreen on D3D11 (the lookdev path), each frame waited for, where Maya's playback is its viewport's free-running `play`.

## 100000 strands, 16 points each

| Measure | Maya plugin | Standalone app |
| --- | --- | --- |
| Strands made | 99994 [99994-99994] | 100058 |
| Creation, cold (file read + first evaluation) | 0.14 s [0.14-0.14] | 0.16 s |
| Clump tweak to evaluated | 21.9 ms [21.8-22.0] | 25.1 ms |
| Noise tweak to evaluated | 16.6 ms [16.6-16.8] | 18.1 ms |
| Per-frame evaluation, deforming scalp (no draw) | 40.7 ms [39.8-40.8] | 44.8 ms |
| Per-frame evaluation, deforming scalp, followScalp | 8.1 ms [8.1-8.3] | 8.2 ms |
| Alembic export, 24 frames | 1.32 s [1.31-1.36] | 1.69 s |
| Alembic size, 24 frames | 617 MB [617-617] | 617 MB |
| Alembic export, 24 frames, followScalp | 0.59 s [0.59-0.60] | 0.61 s |
| USD export, 24 frames (text layer) | — | 5.30 s |
| USD size, 24 frames | — | 1969 MB |
| Playback, deforming scalp (Maya: GUI play; app: evaluate + build + draw offscreen, 1280x720) | 20.5 fps [18.7-20.7] | 6.6 fps |
| Playback, deforming scalp, followScalp | 56.3 fps [53.2-56.8] | 8.7 fps |

## 500000 strands, 16 points each

| Measure | Maya plugin | Standalone app |
| --- | --- | --- |
| Strands made | 500008 [500008-500008] | 500048 |
| Creation, cold (file read + first evaluation) | 0.29 s [0.29-0.30] | 0.30 s |
| Clump tweak to evaluated | 100.2 ms [99.8-103.6] | 111.9 ms |
| Noise tweak to evaluated | 75.7 ms [74.2-75.8] | 82.9 ms |
| Per-frame evaluation, deforming scalp (no draw) | 166.4 ms [165.1-170.8] | 175.5 ms |
| Per-frame evaluation, deforming scalp, followScalp | 28.0 ms [27.9-28.6] | 27.4 ms |
| Alembic export, 24 frames | 6.96 s [6.96-7.09] | 7.45 s |
| Alembic size, 24 frames | 3084 MB [3084-3084] | 3084 MB |
| Alembic export, 24 frames, followScalp | 3.53 s [3.53-3.63] | 3.62 s |
| USD export, 24 frames (text layer) | — | 25.16 s |
| USD size, 24 frames | — | 9842 MB |
| Playback, deforming scalp (Maya: GUI play; app: evaluate + build + draw offscreen, 1280x720) | 4.9 fps [4.6-5.0] | 1.5 fps |
| Playback, deforming scalp, followScalp | 16.4 fps [15.0-16.7] | 1.8 fps |

The playback rows are not like for like: the app's headless loop evaluates the frame, builds its display buffers and renders it through the offscreen helper, which makes its render target and the lookdev pass's own targets again on every call and waits for the GPU, so it is a floor for the window's playback (the window keeps its targets; its timeline drops frames to hold the clock), where Maya's number is its viewport playing free. The per-frame evaluation rows compare the same work on both sides.

Not compared: the Arnold .ass export (the app has no Arnold translator; the procedural reads the .groom file instead), the scene save and open (the app's .groom file is the Maya export), the GUI brush and orbit numbers (results/2026-10-02_app_vs_maya.md has the interactive ones).
