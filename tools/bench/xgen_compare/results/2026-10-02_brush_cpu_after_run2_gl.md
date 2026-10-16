# GroomLab vs XGen Interactive Grooming: final2_OpenGL

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab c8f4687. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.45 s [0.40-0.51] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 355 MB [355-355] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 97.0 MB [97.0-97.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.67 ms [3.44-3.91] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 36.2 ms [34.0-38.3] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 30.1 ms [28.4-31.8] |  |  |  |
| Orbit (GUI) |  | 669 fps [653-685] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 64.0 ms [59.5-68.5] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 15.9 ms [14.7-17.0] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 6.19 ms [6.08-6.30] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 65.8 ms [63.1-68.5] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 63.7 ms [58.7-68.7] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 52.4 ms [49.2-55.6] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.81 s [0.79-0.84] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1597 MB [1596-1598] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 485 MB [485-485] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.60 ms [3.21-3.99] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 154 ms [145-162] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 123 ms [117-130] |  |  |  |
| Orbit (GUI) |  | 165 fps [54-276] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 199 ms [194-204] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 39.6 ms [39.3-40.0] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 11.0 ms [10.4-11.7] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 103 ms [91-115] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 208 ms [204-212] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 190 ms [189-192] |  |  |  |

