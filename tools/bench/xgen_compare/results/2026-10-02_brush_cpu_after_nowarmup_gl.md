# GroomLab vs XGen Interactive Grooming: nowarm_OpenGL

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab c8f4687. Medians of 1 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.39 s [0.39-0.39] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 356 MB [356-356] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 97.0 MB [97.0-97.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.40 ms [3.40-3.40] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 33.7 ms [33.7-33.7] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 26.6 ms [26.6-26.6] |  |  |  |
| Orbit (GUI) |  | 711 fps [711-711] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 55.4 ms [55.4-55.4] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 14.0 ms [14.0-14.0] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 14.3 ms [14.3-14.3] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 89.2 ms [89.2-89.2] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 55.9 ms [55.9-55.9] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 44.8 ms [44.8-44.8] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.74 s [0.74-0.74] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1598 MB [1598-1598] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 484 MB [484-484] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.16 ms [3.16-3.16] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 148 ms [148-148] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 112 ms [112-112] |  |  |  |
| Orbit (GUI) |  | 226 fps [226-226] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 191 ms [191-191] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 40.1 ms [40.1-40.1] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 42.6 ms [42.6-42.6] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 166 ms [166-166] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 194 ms [194-194] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 178 ms [178-178] |  |  |  |

