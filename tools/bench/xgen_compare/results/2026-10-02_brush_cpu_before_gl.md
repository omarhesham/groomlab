# GroomLab vs XGen Interactive Grooming: base_OpenGL

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab 609ef20. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.49 s [0.47-0.51] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 353 MB [352-354] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 89.0 MB [73.0-105.0] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.67 ms [3.28-4.05] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 34.5 ms [33.9-35.1] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 28.2 ms [27.4-29.1] |  |  |  |
| Orbit (GUI) |  | 697 fps [619-775] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 113 ms [113-113] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 15.3 ms [14.9-15.7] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 15.8 ms [15.6-16.0] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 88.2 ms [84.1-92.2] |  |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  | 0.87 s [0.87-0.88] |  |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  | 1616 MB [1597-1635] |  |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  | 502 MB [498-507] |  |  |  |
| Redraw, nothing changed (GUI) |  | 3.58 ms [3.39-3.76] |  |  |  |
| Clump tweak to redrawn frame (GUI) |  | 150 ms [149-151] |  |  |  |
| Noise tweak to redrawn frame (GUI) |  | 121 ms [119-123] |  |  |  |
| Orbit (GUI) |  | 229 fps [205-253] |  |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  | 305 ms [284-325] |  |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  | 46.5 ms [45.3-47.6] |  |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  | 49.2 ms [49.1-49.3] |  |  |  |
| Brush release (commit), to redrawn frame (GUI) |  | 249 ms [244-253] |  |  |  |

