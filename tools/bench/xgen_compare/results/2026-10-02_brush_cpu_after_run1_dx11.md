# GroomLab vs XGen Interactive Grooming: final1_DirectX11

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab c8f4687. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.24 s [0.24-0.24] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 256 MB [256-257] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 194 MB [98-291] |  |  |  |
| Redraw, nothing changed (GUI) |  | 1.90 ms [1.76-2.04] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 35.5 ms [35.1-35.9] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 29.5 ms [29.1-29.9] |  |  |  |
| Orbit (GUI) |  | 1101 fps [1097-1106] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 73.3 ms [49.7-96.9] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 12.9 ms [12.8-13.1] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 4.49 ms [4.45-4.52] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 20.8 ms [18.4-23.2] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 45.5 ms [42.8-48.2] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 42.9 ms [42.7-43.2] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.47 s [0.45-0.49] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1149 MB [1146-1152] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 481 MB [477-485] |  |  |  |
| Redraw, nothing changed (GUI) |  | 1.78 ms [1.78-1.78] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 165 ms [163-166] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 136 ms [133-140] |  |  |  |
| Orbit (GUI) |  | 357 fps [342-372] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 177 ms [169-184] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 35.5 ms [35.5-35.6] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 8.73 ms [8.52-8.95] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 31.2 ms [30.0-32.4] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 151 ms [150-153] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 160 ms [160-161] |  |  |  |

