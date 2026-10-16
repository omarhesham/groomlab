# GroomLab vs XGen Interactive Grooming: base_DirectX11

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab 609ef20. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.36 s [0.29-0.43] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 265 MB [265-266] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 98.5 MB [80.0-117.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 2.88 ms [2.88-2.89] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 38.5 ms [36.2-40.9] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 32.5 ms [30.7-34.4] |  |  |  |
| Orbit (GUI) |  | 954 fps [806-1102] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 669 ms [635-704] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 41.8 ms [40.4-43.2] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 188 ms [186-190] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 575 ms [557-593] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.66 s [0.61-0.71] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1168 MB [1151-1184] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 476 MB [448-503] |  |  |  |
| Redraw, nothing changed (GUI) |  | 2.59 ms [2.40-2.78] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 205 ms [187-223] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 168 ms [153-184] |  |  |  |
| Orbit (GUI) |  | 317 fps [293-342] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 4249 ms [4174-4325] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 339 ms [333-344] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 1534 ms [1344-1725] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 3183 ms [3127-3238] |  |  |  |

