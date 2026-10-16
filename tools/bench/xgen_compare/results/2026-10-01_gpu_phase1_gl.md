# GroomLab vs XGen Interactive Grooming: 2026-10-01_gpu_phase1_gl

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab 75e7f91. Medians of 3 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  |  |  |  | 0.23 s [0.23-0.26] |
| Creation working set delta (GUI, incl. draw buffers) |  |  |  |  | 352 MB [348-354] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  |  |  | 99.0 MB [98.0-99.0] |
| Redraw, nothing changed (GUI) |  |  |  |  | 2.65 ms [2.02-2.83] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 37.3 ms [27.6-38.5] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 41.6 ms [33.1-42.5] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 3.31 ms [3.20-3.32] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 19.0 ms [3.4-19.3] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 3.28 ms [3.28-3.48] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 3.05 ms [2.99-3.26] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 1.22 ms [1.22-1.22] |
| GPU Noise kernel |  |  |  |  | 0.30 ms [0.30-0.31] |
| GPU Curl kernel |  |  |  |  | 0.54 ms [0.54-0.55] |
| GPU lit frames kernel |  |  |  |  | 0.10 ms [0.10-0.10] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.001 ms [0.001-0.001] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.09 ms [0.09-0.09] |
| First drag (engine made, strands uploaded) |  |  |  |  | 51.9 ms [36.4-53.0] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 30.6 ms [29.8-31.9] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 14.1 ms [14.0-18.0] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 3.73 ms [3.63-3.77] |
| Upload size |  |  |  |  | 24.4 MB [24.4-24.4] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 8.91 ms [7.50-9.26] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 2.80 ms [2.70-3.16] |
| Upload size after the stroke |  |  |  |  | 2.28 MB [2.28-2.28] |
| Strands uploaded after the stroke |  |  |  |  | 11227  [11227-11227] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 50.0 MB [50.0-50.0] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 27.4 ms [27.0-27.8] |
| GPU vs CPU positions, max error |  |  |  |  | 9.4e-06 cm [9.4e-06-9.4e-06] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.1e-07 cm [9.1e-07-9.1e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 8.3e-06  [8.3e-06-8.3e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 0.05 % [0.05-0.05] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 0.04 % [0.04-0.04] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 5.92 % [5.92-5.92] |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  |  |  |  | 0.58 s [0.56-0.61] |
| Creation working set delta (GUI, incl. draw buffers) |  |  |  |  | 1595 MB [1591-1599] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  |  |  | 486 MB [481-486] |
| Redraw, nothing changed (GUI) |  |  |  |  | 4.88 ms [3.17-5.67] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 124 ms [123-126] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 154 ms [148-155] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 11.9 ms [11.6-12.3] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 12.3 ms [12.3-16.9] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 11.6 ms [11.4-11.7] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 11.7 ms [11.5-11.8] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 6.30 ms [6.28-6.31] |
| GPU Noise kernel |  |  |  |  | 1.44 ms [1.43-1.46] |
| GPU Curl kernel |  |  |  |  | 2.98 ms [2.98-2.98] |
| GPU lit frames kernel |  |  |  |  | 0.49 ms [0.48-0.49] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.001 ms [0.001-0.001] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.18 ms [0.18-0.18] |
| First drag (engine made, strands uploaded) |  |  |  |  | 174 ms [167-230] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 133 ms [129-135] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 70.7 ms [70.2-71.4] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 24.2 ms [23.7-24.2] |
| Upload size |  |  |  |  | 122 MB [122-122] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 33.6 ms [31.2-34.8] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 13.9 ms [13.4-14.4] |
| Upload size after the stroke |  |  |  |  | 10.6 MB [10.6-10.6] |
| Strands uploaded after the stroke |  |  |  |  | 52856  [52856-52856] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 250 MB [250-250] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 133 ms [132-133] |
| GPU vs CPU positions, max error |  |  |  |  | 1.1e-05 cm [1.1e-05-1.1e-05] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.1e-07 cm [9.1e-07-9.1e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 9.4e-06  [9.4e-06-9.4e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 0.09 % [0.09-0.09] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 0.09 % [0.09-0.09] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 7.27 % [7.27-7.27] |

