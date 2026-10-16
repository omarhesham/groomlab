# GroomLab vs XGen Interactive Grooming: nowarm_DirectX11

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab c8f4687. Medians of 1 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.24 s [0.24-0.24] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 278 MB [278-278] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 98.0 MB [98.0-98.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 1.79 ms [1.79-1.79] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 35.4 ms [35.4-35.4] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 29.8 ms [29.8-29.8] |  |  |  |
| Orbit (GUI) |  | 1051 fps [1051-1051] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 50.1 ms [50.1-50.1] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 12.7 ms [12.7-12.7] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 13.1 ms [13.1-13.1] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 39.4 ms [39.4-39.4] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 45.2 ms [45.2-45.2] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 42.9 ms [42.9-42.9] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.50 s [0.50-0.50] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1209 MB [1209-1209] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 484 MB [484-484] |  |  |  |
| Redraw, nothing changed (GUI) |  | 1.78 ms [1.78-1.78] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 168 ms [168-168] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 133 ms [133-133] |  |  |  |
| Orbit (GUI) |  | 352 fps [352-352] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 312 ms [312-312] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 35.5 ms [35.5-35.5] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 37.5 ms [37.5-37.5] |  |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  | 108 ms [108-108] |  |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  | 152 ms [152-152] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 159 ms [159-159] |  |  |  |

