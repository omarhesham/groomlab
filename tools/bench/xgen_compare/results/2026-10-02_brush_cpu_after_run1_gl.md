# GroomLab vs XGen Interactive Grooming: final1_OpenGL

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab c8f4687. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.39 s [0.38-0.41] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 358 MB [357-358] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 92.5 MB [89.0-96.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.24 ms [3.12-3.36] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 33.3 ms [33.1-33.5] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 26.8 ms [26.3-27.2] |  |  |  |
| Orbit (GUI) |  | 762 fps [758-767] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 56.2 ms [55.9-56.5] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 14.0 ms [13.8-14.1] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 5.40 ms [5.31-5.49] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 58.4 ms [57.8-58.9] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 54.1 ms [54.0-54.3] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 45.3 ms [45.2-45.4] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.66 s [0.66-0.67] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1599 MB [1598-1599] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 484 MB [484-484] |  |  |  |
| Redraw, nothing changed (GUI) |  | 2.62 ms [2.31-2.93] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 138 ms [137-139] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 111 ms [110-113] |  |  |  |
| Orbit (GUI) |  | 171 fps [159-183] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 198 ms [191-205] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 37.8 ms [37.4-38.2] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 10.6 ms [10.4-10.7] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 89.8 ms [85.4-94.2] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 192 ms [192-193] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 179 ms [176-183] |  |  |  |

