# GroomLab vs XGen Interactive Grooming: audit_gl

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab f006a020. Medians of 1 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.39 s [0.39-0.39] |  |  | 0.40 s [0.40-0.40] |
| Creation working set delta (GUI, incl. draw buffers) |  | 386 MB [386-386] |  |  | 391 MB [391-391] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 81.0 MB [81.0-81.0] |  |  | 80.0 MB [80.0-80.0] |
| Redraw, nothing changed (GUI) |  | 4.53 ms [4.53-4.53] |  |  | 4.04 ms [4.04-4.04] |
| Clump tweak to redrawn frame (GUI) |  | 32.9 ms [32.9-32.9] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 24.9 ms [24.9-24.9] |  |  |  |
| Orbit (GUI) |  | 570 fps [570-570] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 66.6 ms [66.6-66.6] |  |  | 65.6 ms [65.6-65.6] |
| Brush dab while dragging, to redrawn frame (GUI) |  | 18.6 ms [18.6-18.6] |  |  | 5.03 ms [5.03-5.03] |
| Brush press (first dab), to redrawn frame (GUI) |  | 6.10 ms [6.10-6.10] |  |  | 7.90 ms [7.90-7.90] |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 13.8 ms [13.8-13.8] |  |  | 16.1 ms [16.1-16.1] |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 64.4 ms [64.4-64.4] |  |  | 68.6 ms [68.6-68.6] |
| Brush release (commit), to redrawn frame (GUI) |  | 53.1 ms [53.1-53.1] |  |  | 54.5 ms [54.5-54.5] |
| Playback, deforming scalp, 1st pass (GUI, play) |  | 16.8 fps [16.8-16.8] |  |  |  |
| Playback, deforming scalp, 2nd pass (GUI, play) |  | 16.7 fps [16.7-16.7] |  |  |  |
| Stepping frames + redraw, deforming (GUI) |  | 16.3 fps [16.3-16.3] |  |  |  |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 43.3 ms [43.3-43.3] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 36.7 ms [36.7-36.7] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 5.27 ms [5.27-5.27] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 6.02 ms [6.02-6.02] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 4.89 ms [4.89-4.89] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 5.84 ms [5.84-5.84] |
| Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width) |  |  |  |  | 5.33 ms [5.33-5.33] |
| Interpolate drag to drawn frame: GPU preview (from Interpolate on) |  |  |  |  | 3.09 ms [3.09-3.09] |
| GPU kernels of a Clump drag |  |  |  |  | 1.53 ms [1.53-1.53] |
| GPU kernels of an Interpolate drag |  |  |  |  | 1.53 ms [1.53-1.53] |
| GPU Interpolate kernel (an Interpolate drag) |  |  |  |  | 0 ms [0-0] |
| GPU Clump kernel and its centers (an Interpolate drag) |  |  |  |  | 0.36 ms [0.36-0.36] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 1.30 ms [1.30-1.30] |
| GPU Noise kernel |  |  |  |  | 0.29 ms [0.29-0.29] |
| GPU Curl kernel |  |  |  |  | 0.48 ms [0.48-0.48] |
| GPU lit frames kernel |  |  |  |  | 0.10 ms [0.10-0.10] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.03 ms [0.03-0.03] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.44 ms [0.44-0.44] |
| First drag (engine made, strands uploaded) |  |  |  |  | 65.4 ms [65.4-65.4] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 57.1 ms [57.1-57.1] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 25.5 ms [25.5-25.5] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 8.68 ms [8.68-8.68] |
| Upload size |  |  |  |  | 19.6 MB [19.6-19.6] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 13.5 ms [13.5-13.5] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 4.71 ms [4.71-4.71] |
| Upload size after the stroke |  |  |  |  | 2.35 MB [2.35-2.35] |
| Strands uploaded after the stroke |  |  |  |  | 11838  [11838-11838] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 99.0 MB [99.0-99.0] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 29.9 ms [29.9-29.9] |
| Release of a Clump drag: CPU commit to drawn frame |  |  |  |  | 36.6 ms [36.6-36.6] |
| Brush dabs drawn by the GPU (of the strokes timed) |  |  |  |  | 48.0  [48.0-48.0] |
| GPU vs CPU positions, max error |  |  |  |  | 3.9e-05 cm [3.9e-05-3.9e-05] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.3e-07 cm [9.3e-07-9.3e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 4.5e-05  [4.5e-05-4.5e-05] |
| GPU vs CPU positions from Clump, max error |  |  |  |  | 0.00028 cm [0.00028-0.00028] |
| GPU vs CPU positions from Interpolate, max error |  |  |  |  | 0.00028 cm [0.00028-0.00028] |
| GPU vs CPU positions from Interpolate, mean error |  |  |  |  | 2.4e-06 cm [2.4e-06-2.4e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 1.21 % [1.21-1.21] |
| Frame pixels differing: GPU Clump drag vs CPU commit, same value |  |  |  |  | 1.56 % [1.56-1.56] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 1.27 % [1.27-1.27] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 11.6 % [11.6-11.6] |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.58 s [0.58-0.58] |  |  | 0.61 s [0.61-0.61] |
| Creation working set delta (GUI, incl. draw buffers) |  | 1456 MB [1456-1456] |  |  | 1454 MB [1454-1454] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 395 MB [395-395] |  |  | 395 MB [395-395] |
| Redraw, nothing changed (GUI) |  | 4.53 ms [4.53-4.53] |  |  | 33.9 ms [33.9-33.9] |
| Clump tweak to redrawn frame (GUI) |  | 137 ms [137-137] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 105 ms [105-105] |  |  |  |
| Orbit (GUI) |  | 173 fps [173-173] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 211 ms [211-211] |  |  | 208 ms [208-208] |
| Brush dab while dragging, to redrawn frame (GUI) |  | 51.6 ms [51.6-51.6] |  |  | 6.70 ms [6.70-6.70] |
| Brush press (first dab), to redrawn frame (GUI) |  | 18.0 ms [18.0-18.0] |  |  | 14.3 ms [14.3-14.3] |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 50.8 ms [50.8-50.8] |  |  | 50.3 ms [50.3-50.3] |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 216 ms [216-216] |  |  | 213 ms [213-213] |
| Brush release (commit), to redrawn frame (GUI) |  | 203 ms [203-203] |  |  | 208 ms [208-208] |
| Playback, deforming scalp, 1st pass (GUI, play) |  | 4.72 fps [4.72-4.72] |  |  |  |
| Playback, deforming scalp, 2nd pass (GUI, play) |  | 4.71 fps [4.71-4.71] |  |  |  |
| Stepping frames + redraw, deforming (GUI) |  | 4.54 fps [4.54-4.54] |  |  |  |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 124 ms [124-124] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 152 ms [152-152] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 15.6 ms [15.6-15.6] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 23.2 ms [23.2-23.2] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 13.3 ms [13.3-13.3] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 12.7 ms [12.7-12.7] |
| Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width) |  |  |  |  | 16.8 ms [16.8-16.8] |
| Interpolate drag to drawn frame: GPU preview (from Interpolate on) |  |  |  |  | 8.07 ms [8.07-8.07] |
| GPU kernels of a Clump drag |  |  |  |  | 7.73 ms [7.73-7.73] |
| GPU kernels of an Interpolate drag |  |  |  |  | 7.73 ms [7.73-7.73] |
| GPU Interpolate kernel (an Interpolate drag) |  |  |  |  | 0 ms [0-0] |
| GPU Clump kernel and its centers (an Interpolate drag) |  |  |  |  | 1.71 ms [1.71-1.71] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 6.67 ms [6.67-6.67] |
| GPU Noise kernel |  |  |  |  | 1.39 ms [1.39-1.39] |
| GPU Curl kernel |  |  |  |  | 2.61 ms [2.61-2.61] |
| GPU lit frames kernel |  |  |  |  | 0.48 ms [0.48-0.48] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.13 ms [0.13-0.13] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.48 ms [0.48-0.48] |
| First drag (engine made, strands uploaded) |  |  |  |  | 186 ms [186-186] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 151 ms [151-151] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 127 ms [127-127] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 29.3 ms [29.3-29.3] |
| Upload size |  |  |  |  | 98.0 MB [98.0-98.0] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 36.7 ms [36.7-36.7] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 13.3 ms [13.3-13.3] |
| Upload size after the stroke |  |  |  |  | 11.8 MB [11.8-11.8] |
| Strands uploaded after the stroke |  |  |  |  | 59589  [59589-59589] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 493 MB [493-493] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 118 ms [118-118] |
| Release of a Clump drag: CPU commit to drawn frame |  |  |  |  | 143 ms [143-143] |
| Brush dabs drawn by the GPU (of the strokes timed) |  |  |  |  | 48.0  [48.0-48.0] |
| GPU vs CPU positions, max error |  |  |  |  | 0.00022 cm [0.00022-0.00022] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.3e-07 cm [9.3e-07-9.3e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 0.0003  [0.0003-0.0003] |
| GPU vs CPU positions from Clump, max error |  |  |  |  | 0.00023 cm [0.00023-0.00023] |
| GPU vs CPU positions from Interpolate, max error |  |  |  |  | 0.00082 cm [0.00082-0.00082] |
| GPU vs CPU positions from Interpolate, mean error |  |  |  |  | 2.4e-06 cm [2.4e-06-2.4e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 1.54 % [1.54-1.54] |
| Frame pixels differing: GPU Clump drag vs CPU commit, same value |  |  |  |  | 1.99 % [1.99-1.99] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 1.57 % [1.57-1.57] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 13.0 % [13.0-13.0] |

