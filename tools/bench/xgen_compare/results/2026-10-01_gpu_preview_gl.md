# GroomLab vs XGen Interactive Grooming: 2026-10-01_gpu_preview_gl

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1692). GroomLab 4fee040. Medians of 2 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  |  |  |  | 0.24 s [0.23-0.24] |
| Creation working set delta (GUI, incl. draw buffers) |  |  |  |  | 350 MB [348-352] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  |  |  | 99.0 MB [99.0-99.0] |
| Redraw, nothing changed (GUI) |  |  |  |  | 2.50 ms [2.24-2.77] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 37.7 ms [37.5-37.9] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 38.0 ms [34.2-41.9] |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Creation to first drawn frame (GUI) |  |  |  |  | 0.54 s [0.52-0.55] |
| Creation working set delta (GUI, incl. draw buffers) |  |  |  |  | 1591 MB [1590-1592] |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  |  |  | 490 MB [486-494] |
| Redraw, nothing changed (GUI) |  |  |  |  | 3.80 ms [3.54-4.05] |
| Noise tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 127 ms [126-128] |
| Clump tweak to drawn frame, GPU waited for: CPU evaluation |  |  |  |  | 157 ms [156-157] |

