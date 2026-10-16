# GroomLab vs XGen Interactive Grooming: 2026-10-02_gpu_phase2_gl

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab 482e9ed. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.24 s [0.24-0.24] |  |  | 0.25 s [0.24-0.26] |
| Creation working set delta (GUI, incl. draw buffers) |  | 344 MB [338-349] |  |  | 348 MB [348-348] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 99.0 MB [99.0-99.0] |  |  | 99.0 MB [99.0-99.0] |
| Redraw, nothing changed (GUI) |  | 3.25 ms [2.54-3.96] |  |  | 3.10 ms [3.01-3.18] |
| Clump tweak to redrawn frame (GUI) |  | 31.9 ms [31.5-32.3] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 26.0 ms [26.0-26.1] |  |  |  |
| Orbit (GUI) |  | 783 fps [776-791] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 89.6 ms [88.7-90.5] |  |  | 92.3 ms [90.2-94.3] |
| Brush dab while dragging, to redrawn frame (GUI) |  | 14.3 ms [14.3-14.4] |  |  | 3.71 ms [3.62-3.80] |
| Brush press (first dab), to redrawn frame (GUI) |  | 13.9 ms [13.8-14.0] |  |  | 19.5 ms [19.2-19.8] |
| Brush release (commit), to redrawn frame (GUI) |  | 81.7 ms [81.4-82.0] |  |  | 83.3 ms [81.9-84.8] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 40.8 ms [39.3-42.3] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 44.8 ms [44.4-45.2] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 3.58 ms [3.49-3.68] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 12.5 ms [5.0-20.1] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 3.74 ms [3.10-4.38] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 3.15 ms [3.05-3.26] |
| Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width) |  |  |  |  | 4.29 ms [4.04-4.54] |
| Interpolate drag to drawn frame: GPU preview (from Interpolate on) |  |  |  |  | 6.18 ms [6.10-6.26] |
| GPU kernels of a Clump drag |  |  |  |  | 1.43 ms [1.43-1.43] |
| GPU kernels of an Interpolate drag |  |  |  |  | 3.37 ms [3.37-3.38] |
| GPU Interpolate kernel (an Interpolate drag) |  |  |  |  | 1.91 ms [1.90-1.91] |
| GPU Clump kernel and its centers (an Interpolate drag) |  |  |  |  | 0.31 ms [0.31-0.31] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 1.17 ms [1.15-1.19] |
| GPU Noise kernel |  |  |  |  | 0.28 ms [0.28-0.29] |
| GPU Curl kernel |  |  |  |  | 0.49 ms [0.49-0.49] |
| GPU lit frames kernel |  |  |  |  | 0.10 ms [0.10-0.10] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.001 ms [0.001-0.001] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.38 ms [0.31-0.44] |
| First drag (engine made, strands uploaded) |  |  |  |  | 49.2 ms [40.5-57.8] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 33.8 ms [33.0-34.7] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 17.6 ms [17.4-17.8] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 2.17 ms [2.09-2.24] |
| Upload size |  |  |  |  | 19.6 MB [19.6-19.6] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 8.23 ms [7.83-8.63] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 1.96 ms [1.88-2.04] |
| Upload size after the stroke |  |  |  |  | 2.37 MB [2.37-2.37] |
| Strands uploaded after the stroke |  |  |  |  | 11936  [11936-11936] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 88.0 MB [88.0-88.0] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 28.6 ms [28.5-28.7] |
| Release of a Clump drag: CPU commit to drawn frame |  |  |  |  | 34.3 ms [34.3-34.3] |
| Brush dabs drawn by the GPU (of the strokes timed) |  |  |  |  | 40.0  [40.0-40.0] |
| GPU vs CPU positions, max error |  |  |  |  | 9.4e-06 cm [9.4e-06-9.4e-06] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.1e-07 cm [9.1e-07-9.1e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 8.3e-06  [8.3e-06-8.3e-06] |
| GPU vs CPU positions from Clump, max error |  |  |  |  | 1e-05 cm [1e-05-1e-05] |
| GPU vs CPU positions from Interpolate, max error |  |  |  |  | 1.6e-05 cm [1.6e-05-1.6e-05] |
| GPU vs CPU positions from Interpolate, mean error |  |  |  |  | 1.7e-06 cm [1.7e-06-1.7e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 0.05 % [0.05-0.05] |
| Frame pixels differing: GPU Clump drag vs CPU commit, same value |  |  |  |  | 0.04 % [0.04-0.04] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 0.04 % [0.04-0.04] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 5.92 % [5.92-5.92] |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.61 s [0.56-0.65] |  |  | 0.55 s [0.53-0.57] |
| Creation working set delta (GUI, incl. draw buffers) |  | 1593 MB [1592-1594] |  |  | 1591 MB [1590-1592] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 566 MB [489-643] |  |  | 487 MB [487-487] |
| Redraw, nothing changed (GUI) |  | 3.93 ms [2.58-5.28] |  |  | 2.74 ms [2.71-2.76] |
| Clump tweak to redrawn frame (GUI) |  | 144 ms [141-148] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 116 ms [114-117] |  |  |  |
| Orbit (GUI) |  | 211 fps [203-218] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 248 ms [238-258] |  |  | 237 ms [223-252] |
| Brush dab while dragging, to redrawn frame (GUI) |  | 43.3 ms [43.0-43.7] |  |  | 4.61 ms [4.60-4.61] |
| Brush press (first dab), to redrawn frame (GUI) |  | 45.8 ms [45.5-46.1] |  |  | 64.7 ms [58.2-71.2] |
| Brush release (commit), to redrawn frame (GUI) |  | 227 ms [226-229] |  |  | 218 ms [215-222] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 125 ms [123-126] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 153 ms [147-160] |
| Noise drag to drawn frame: GPU preview |  |  |  |  | 12.2 ms [12.2-12.2] |
| Noise drag to drawn frame: GPU preview, GPU at idle clocks |  |  |  |  | 19.4 ms [18.4-20.5] |
| Curl drag to drawn frame: GPU preview |  |  |  |  | 9.94 ms [9.81-10.07] |
| Width drag to drawn frame: GPU preview |  |  |  |  | 9.05 ms [8.94-9.16] |
| Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width) |  |  |  |  | 13.4 ms [13.1-13.7] |
| Interpolate drag to drawn frame: GPU preview (from Interpolate on) |  |  |  |  | 24.0 ms [24.0-24.0] |
| GPU kernels of a Clump drag |  |  |  |  | 7.65 ms [7.44-7.85] |
| GPU kernels of an Interpolate drag |  |  |  |  | 16.7 ms [16.6-16.8] |
| GPU Interpolate kernel (an Interpolate drag) |  |  |  |  | 8.88 ms [8.66-9.11] |
| GPU Clump kernel and its centers (an Interpolate drag) |  |  |  |  | 1.50 ms [1.50-1.50] |
| GPU kernels (timestamps; with the copy into VP2's buffers where there is one) |  |  |  |  | 5.97 ms [5.97-5.97] |
| GPU Noise kernel |  |  |  |  | 1.39 ms [1.38-1.39] |
| GPU Curl kernel |  |  |  |  | 2.70 ms [2.70-2.70] |
| GPU lit frames kernel |  |  |  |  | 0.48 ms [0.48-0.48] |
| GPU copy into VP2 buffers (0: written in place) |  |  |  |  | 0.001 ms [0.001-0.001] |
| GPU preview apply on the CPU (params, dispatch) |  |  |  |  | 0.43 ms [0.40-0.46] |
| First drag (engine made, strands uploaded) |  |  |  |  | 191 ms [190-191] |
| First drag: GPU preview apply on the CPU |  |  |  |  | 148 ms [145-150] |
| Noise drag after a clump change (CPU to Clump + upload + GPU) |  |  |  |  | 107 ms [105-110] |
| Upload of Clump's strands (CPU side) |  |  |  |  | 16.8 ms [16.3-17.3] |
| Upload size |  |  |  |  | 98.0 MB [98.0-98.0] |
| Noise drag after a brush stroke (CPU stroke + partial upload + GPU) |  |  |  |  | 31.7 ms [31.4-31.9] |
| Upload after the stroke (CPU side): the stroke's strands |  |  |  |  | 7.21 ms [6.91-7.52] |
| Upload size after the stroke |  |  |  |  | 11.3 MB [11.3-11.3] |
| Strands uploaded after the stroke |  |  |  |  | 57079  [57079-57079] |
| GPU preview engine buffers (VRAM) |  |  |  |  | 437 MB [437-437] |
| GPU preview's own vertex buffers during a drag (VRAM, DirectX 11) |  |  |  |  | 0 MB [0-0] |
| Release: drag ends, CPU commit to drawn frame |  |  |  |  | 132 ms [125-138] |
| Release of a Clump drag: CPU commit to drawn frame |  |  |  |  | 151 ms [149-153] |
| Brush dabs drawn by the GPU (of the strokes timed) |  |  |  |  | 40.0  [40.0-40.0] |
| GPU vs CPU positions, max error |  |  |  |  | 1.1e-05 cm [1.1e-05-1.1e-05] |
| GPU vs CPU positions, mean error |  |  |  |  | 9.1e-07 cm [9.1e-07-9.1e-07] |
| GPU vs CPU normals, max error |  |  |  |  | 9.4e-06  [9.4e-06-9.4e-06] |
| GPU vs CPU positions from Clump, max error |  |  |  |  | 1.2e-05 cm [1.2e-05-1.2e-05] |
| GPU vs CPU positions from Interpolate, max error |  |  |  |  | 1.6e-05 cm [1.6e-05-1.6e-05] |
| GPU vs CPU positions from Interpolate, mean error |  |  |  |  | 1.7e-06 cm [1.7e-06-1.7e-06] |
| Frame pixels differing: GPU drag vs CPU commit, same value |  |  |  |  | 0.09 % [0.09-0.09] |
| Frame pixels differing: GPU Clump drag vs CPU commit, same value |  |  |  |  | 0.08 % [0.08-0.08] |
| Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU |  |  |  |  | 0.09 % [0.09-0.09] |
| Frame pixels differing: CPU vs the same CPU frame again |  |  |  |  | 0 % [0-0] |
| Frame pixels differing: CPU vs CPU at a value 1% away |  |  |  |  | 7.27 % [7.27-7.27] |

