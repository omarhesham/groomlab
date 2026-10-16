# GroomLab vs XGen Interactive Grooming: 2026-10-01_gpu_preview_dx11

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab 4fee040. Medians of 3 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  |  |  |  | 0.28 s [0.27-0.29] |
| Creation working set delta (GUI, incl. draw buffers) |  |  |  |  | 273 MB [272-277] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  |  |  | 98.0 MB [97.0-98.0] |
| Redraw, nothing changed (GUI) |  |  |  |  | 2.85 ms [1.78-2.88] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 46.8 ms [46.7-47.3] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 49.2 ms [38.1-49.4] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 2.58 ms [2.55-2.68] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 4.35 ms [4.05-4.97] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 2.62 ms [2.53-2.67] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 2.56 ms [2.54-2.61] |
| GPU kernels + copy into VP2 buffers (timestamps) |  |  |  |  | 1.16 ms [1.15-1.16] |
| GPU Noise kernel |  |  |  |  | 0.31 ms [0.30-0.31] |
| GPU Curl kernel |  |  |  |  | 0.39 ms [0.38-0.39] |
| GPU lit frames kernel |  |  |  |  | 0.10 ms [0.10-0.10] |
| GPU copy into VP2 buffers |  |  |  |  | 0.22 ms [0.22-0.22] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.03 ms [0.03-0.03] |
| First drag (kernels compiled, strands uploaded) |  |  |  |  | 979 ms [960-986] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 6.68 ms [6.64-7.16] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 2.91 ms [2.89-3.28] |
| Upload size |  |  |  |  | 24.4 MB [24.4-24.4] |
| GPU preview buffers (VRAM) |  |  |  |  | 127 MB [127-127] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 31.7 ms [31.0-31.7] |
| GPU vs CPU positions, max error |  |  |  |  | 9.3e-06 cm [9.3e-06-9.3e-06] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.3e-07 cm [9.3e-07-9.3e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 8.4e-06  [8.4e-06-8.4e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 0.08 % [0.08-0.08] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 0.07 % [0.07-0.07] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 5.94 % [5.94-5.94] |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  |  |  |  | 0.54 s [0.49-0.56] |
| Creation working set delta (GUI, incl. draw buffers) |  |  |  |  | 1205 MB [1146-1207] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  |  |  | 485 MB [485-485] |
| Redraw, nothing changed (GUI) |  |  |  |  | 2.08 ms [1.65-2.62] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 154 ms [150-166] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 175 ms [174-177] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 9.97 ms [9.76-9.99] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 10.3 ms [10.3-20.5] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 9.81 ms [9.71-10.00] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 9.87 ms [9.62-9.98] |
| GPU kernels + copy into VP2 buffers (timestamps) |  |  |  |  | 5.64 ms [5.63-5.66] |
| GPU Noise kernel |  |  |  |  | 1.42 ms [1.42-1.43] |
| GPU Curl kernel |  |  |  |  | 2.08 ms [2.07-2.08] |
| GPU lit frames kernel |  |  |  |  | 0.48 ms [0.48-0.49] |
| GPU copy into VP2 buffers |  |  |  |  | 0.95 ms [0.95-0.95] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.13 ms [0.11-0.15] |
| First drag (kernels compiled, strands uploaded) |  |  |  |  | 1116 ms [1039-1120] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 41.0 ms [40.6-41.4] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 15.3 ms [15.2-15.5] |
| Upload size |  |  |  |  | 122 MB [122-122] |
| GPU preview buffers (VRAM) |  |  |  |  | 634 MB [634-634] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 155 ms [152-159] |
| GPU vs CPU positions, max error |  |  |  |  | 1.1e-05 cm [1.1e-05-1.1e-05] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.3e-07 cm [9.3e-07-9.3e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 9.7e-06  [9.7e-06-9.7e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 0.15 % [0.15-0.15] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 0.13 % [0.13-0.13] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 7.36 % [7.36-7.36] |

