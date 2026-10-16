# GroomLab vs XGen Interactive Grooming: audit_dx11

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab f006a020. Medians of 1 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.35 s [0.35-0.35] |  |  | 0.34 s [0.34-0.34] |
| Creation working set delta (GUI, incl. draw buffers) |  | 289 MB [289-289] |  |  | 286 MB [286-286] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 79.0 MB [79.0-79.0] |  |  | 79.0 MB [79.0-79.0] |
| Redraw, nothing changed (GUI) |  | 3.19 ms [3.19-3.19] |  |  | 1.91 ms [1.91-1.91] |
| Clump tweak to redrawn frame (GUI) |  | 36.4 ms [36.4-36.4] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 28.7 ms [28.7-28.7] |  |  |  |
| Orbit (GUI) |  | 681 fps [681-681] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 61.3 ms [61.3-61.3] |  |  | 69.2 ms [69.2-69.2] |
| Brush dab while dragging, to redrawn frame (GUI) |  | 17.8 ms [17.8-17.8] |  |  | 4.33 ms [4.33-4.33] |
| Brush press (first dab), to redrawn frame (GUI) |  | 5.36 ms [5.36-5.36] |  |  | 8.83 ms [8.83-8.83] |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 47.0 ms [47.0-47.0] |  |  | 49.4 ms [49.4-49.4] |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 54.1 ms [54.1-54.1] |  |  | 69.7 ms [69.7-69.7] |
| Brush release (commit), to redrawn frame (GUI) |  | 52.0 ms [52.0-52.0] |  |  | 54.3 ms [54.3-54.3] |
| Playback, deforming scalp, 1st pass (GUI, play) |  | 14.9 fps [14.9-14.9] |  |  |  |
| Playback, deforming scalp, 2nd pass (GUI, play) |  | 15.2 fps [15.2-15.2] |  |  |  |
| Stepping frames + redraw, deforming (GUI) |  | 14.8 fps [14.8-14.8] |  |  |  |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 49.7 ms [49.7-49.7] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 53.8 ms [53.8-53.8] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 3.17 ms [3.17-3.17] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 6.00 ms [6.00-6.00] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 3.36 ms [3.36-3.36] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 2.81 ms [2.81-2.81] |
| Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width) |  |  |  |  | 4.04 ms [4.04-4.04] |
| Interpolate drag to drawn frame: GPU preview (from Interpolate on) |  |  |  |  | 2.08 ms [2.08-2.08] |
| GPU kernels of a Clump drag |  |  |  |  | 1.24 ms [1.24-1.24] |
| GPU kernels of an Interpolate drag |  |  |  |  | 1.22 ms [1.22-1.22] |
| GPU Interpolate kernel (an Interpolate drag) |  |  |  |  | 0 ms [0-0] |
| GPU Clump kernel and its centers (an Interpolate drag) |  |  |  |  | 0.25 ms [0.25-0.25] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 1.02 ms [1.02-1.02] |
| GPU Noise kernel |  |  |  |  | 0.29 ms [0.29-0.29] |
| GPU Curl kernel |  |  |  |  | 0.38 ms [0.38-0.38] |
| GPU lit frames kernel |  |  |  |  | 0.10 ms [0.10-0.10] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.03 ms [0.03-0.03] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.09 ms [0.09-0.09] |
| First drag (engine made, strands uploaded) |  |  |  |  | 28.2 ms [28.2-28.2] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 12.9 ms [12.9-12.9] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 18.6 ms [18.6-18.6] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 2.63 ms [2.63-2.63] |
| Upload size |  |  |  |  | 19.6 MB [19.6-19.6] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 13.3 ms [13.3-13.3] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 2.33 ms [2.33-2.33] |
| Upload size after the stroke |  |  |  |  | 2.35 MB [2.35-2.35] |
| Strands uploaded after the stroke |  |  |  |  | 11838  [11838-11838] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 99.0 MB [99.0-99.0] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 31.7 ms [31.7-31.7] |
| Release of a Clump drag: CPU commit to drawn frame |  |  |  |  | 38.1 ms [38.1-38.1] |
| Brush dabs drawn by the GPU (of the strokes timed) |  |  |  |  | 48.0  [48.0-48.0] |
| GPU vs CPU positions, max error |  |  |  |  | 3.9e-05 cm [3.9e-05-3.9e-05] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.5e-07 cm [9.5e-07-9.5e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 4.5e-05  [4.5e-05-4.5e-05] |
| GPU vs CPU positions from Clump, max error |  |  |  |  | 0.00015 cm [0.00015-0.00015] |
| GPU vs CPU positions from Interpolate, max error |  |  |  |  | 0.00028 cm [0.00028-0.00028] |
| GPU vs CPU positions from Interpolate, mean error |  |  |  |  | 2.4e-06 cm [2.4e-06-2.4e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 1.20 % [1.20-1.20] |
| Frame pixels differing: GPU Clump drag vs CPU commit, same value |  |  |  |  | 1.56 % [1.56-1.56] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 1.30 % [1.30-1.30] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 11.7 % [11.7-11.7] |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.63 s [0.63-0.63] |  |  | 0.62 s [0.62-0.62] |
| Creation working set delta (GUI, incl. draw buffers) |  | 1141 MB [1141-1141] |  |  | 1134 MB [1134-1134] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 401 MB [401-401] |  |  | 393 MB [393-393] |
| Redraw, nothing changed (GUI) |  | 1.93 ms [1.93-1.93] |  |  | 1.89 ms [1.89-1.89] |
| Clump tweak to redrawn frame (GUI) |  | 161 ms [161-161] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 127 ms [127-127] |  |  |  |
| Orbit (GUI) |  | 191 fps [191-191] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 211 ms [211-211] |  |  | 207 ms [207-207] |
| Brush dab while dragging, to redrawn frame (GUI) |  | 49.6 ms [49.6-49.6] |  |  | 5.62 ms [5.62-5.62] |
| Brush press (first dab), to redrawn frame (GUI) |  | 11.1 ms [11.1-11.1] |  |  | 19.0 ms [19.0-19.0] |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 61.9 ms [61.9-61.9] |  |  | 72.2 ms [72.2-72.2] |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 184 ms [184-184] |  |  | 263 ms [263-263] |
| Brush release (commit), to redrawn frame (GUI) |  | 196 ms [196-196] |  |  | 204 ms [204-204] |
| Playback, deforming scalp, 1st pass (GUI, play) |  | 4.10 fps [4.10-4.10] |  |  |  |
| Playback, deforming scalp, 2nd pass (GUI, play) |  | 4.13 fps [4.13-4.13] |  |  |  |
| Stepping frames + redraw, deforming (GUI) |  | 3.86 fps [3.86-3.86] |  |  |  |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 155 ms [155-155] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 182 ms [182-182] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 12.5 ms [12.5-12.5] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 17.3 ms [17.3-17.3] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 10.6 ms [10.6-10.6] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 8.64 ms [8.64-8.64] |
| Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width) |  |  |  |  | 14.5 ms [14.5-14.5] |
| Interpolate drag to drawn frame: GPU preview (from Interpolate on) |  |  |  |  | 6.42 ms [6.42-6.42] |
| GPU kernels of a Clump drag |  |  |  |  | 6.15 ms [6.15-6.15] |
| GPU kernels of an Interpolate drag |  |  |  |  | 7.18 ms [7.18-7.18] |
| GPU Interpolate kernel (an Interpolate drag) |  |  |  |  | 0 ms [0-0] |
| GPU Clump kernel and its centers (an Interpolate drag) |  |  |  |  | 1.12 ms [1.12-1.12] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 5.07 ms [5.07-5.07] |
| GPU Noise kernel |  |  |  |  | 1.47 ms [1.47-1.47] |
| GPU Curl kernel |  |  |  |  | 2.05 ms [2.05-2.05] |
| GPU lit frames kernel |  |  |  |  | 0.49 ms [0.49-0.49] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.13 ms [0.13-0.13] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.25 ms [0.25-0.25] |
| First drag (engine made, strands uploaded) |  |  |  |  | 97.1 ms [97.1-97.1] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 43.2 ms [43.2-43.2] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 113 ms [113-113] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 14.3 ms [14.3-14.3] |
| Upload size |  |  |  |  | 98.0 MB [98.0-98.0] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 70.9 ms [70.9-70.9] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 6.88 ms [6.88-6.88] |
| Upload size after the stroke |  |  |  |  | 11.8 MB [11.8-11.8] |
| Strands uploaded after the stroke |  |  |  |  | 59589  [59589-59589] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 493 MB [493-493] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 173 ms [173-173] |
| Release of a Clump drag: CPU commit to drawn frame |  |  |  |  | 194 ms [194-194] |
| Brush dabs drawn by the GPU (of the strokes timed) |  |  |  |  | 48.0  [48.0-48.0] |
| GPU vs CPU positions, max error |  |  |  |  | 0.00022 cm [0.00022-0.00022] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.5e-07 cm [9.5e-07-9.5e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 0.00029  [0.00029-0.00029] |
| GPU vs CPU positions from Clump, max error |  |  |  |  | 0.0004 cm [0.0004-0.0004] |
| GPU vs CPU positions from Interpolate, max error |  |  |  |  | 0.00082 cm [0.00082-0.00082] |
| GPU vs CPU positions from Interpolate, mean error |  |  |  |  | 2.4e-06 cm [2.4e-06-2.4e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 1.54 % [1.54-1.54] |
| Frame pixels differing: GPU Clump drag vs CPU commit, same value |  |  |  |  | 2.05 % [2.05-2.05] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 1.61 % [1.61-1.61] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 13.0 % [13.0-13.0] |

