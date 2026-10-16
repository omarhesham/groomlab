# GroomLab vs XGen Interactive Grooming: final2_DirectX11

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab c8f4687. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.23 s [0.23-0.24] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 257 MB [257-257] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 85.5 MB [81.0-90.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 1.99 ms [1.82-2.15] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 37.3 ms [35.3-39.3] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 31.0 ms [30.3-31.7] |  |  |  |
| Orbit (GUI) |  | 1023 fps [915-1131] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 61.0 ms [49.0-73.0] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 15.4 ms [13.3-17.4] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 5.57 ms [4.58-6.57] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 21.8 ms [18.6-25.0] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 49.6 ms [43.6-55.7] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 52.4 ms [44.3-60.5] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.46 s [0.46-0.47] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1152 MB [1152-1152] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 486 MB [485-486] |  |  |  |
| Redraw, nothing changed (GUI) |  | 1.93 ms [1.75-2.11] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 175 ms [167-183] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 134 ms [133-136] |  |  |  |
| Orbit (GUI) |  | 312 fps [282-341] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 170 ms [170-171] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 35.9 ms [35.6-36.1] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 8.56 ms [8.52-8.60] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 30.3 ms [30.3-30.4] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 153 ms [152-154] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 166 ms [165-167] |  |  |  |

