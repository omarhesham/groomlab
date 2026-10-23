# GroomLab vs XGen Interactive Grooming: 2026-10-23_groomlab_nocurl_0.27

Machine: AMD Ryzen Threadripper PRO 5945WX 12-Cores (12 cores / 24 threads), 127.9 GB RAM, NVIDIA GeForce RTX 3090 (driver 32.0.16.1742). GroomLab 722a59d. Medians of 3 runs (min-max in brackets).

## 100000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Strands made |  |  | 99994  [99994-99994] |  |  |
| Plugin load |  |  | 0.15 s [0.15-0.16] |  |  |
| Creation, cold (batch: create + first evaluation) |  |  | 0.18 s [0.18-0.18] |  |  |
| Creation, warm (batch) |  |  | 0.15 s [0.15-0.15] |  |  |
| Creation working set delta (batch, cold) |  |  | 175 MB [174-177] |  |  |
| Creation private bytes delta (batch, cold) |  |  | 172 MB [171-174] |  |  |
| Clump tweak to evaluated (batch) |  |  | 14.5 ms [13.9-15.0] |  |  |
| Noise tweak to evaluated (batch) |  |  | 11.2 ms [10.7-11.4] |  |  |
| Scene .ma size |  |  | 2.83 MB [2.83-2.83] |  |  |
| Scene .mb size |  |  | 2.24 MB [2.24-2.24] |  |  |
| Scene .mb save |  |  | 0.02 s [0.02-0.02] |  |  |
| Open saved .mb + evaluate |  |  | 0.11 s [0.11-0.11] |  |  |
| Arnold .ass export, static |  |  | 0.18 s [0.18-0.18] |  |  |
| .ass size, static |  |  | 34.2 MB [34.2-34.2] |  |  |
| Creation to first drawn frame (GUI) |  |  | 0.27 s [0.27-0.28] |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  |  | 285 MB [283-286] |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  | 146 MB [123-146] |  |  |
| Redraw, nothing changed (GUI) |  |  | 3.46 ms [3.29-3.62] |  |  |
| Clump tweak to redrawn frame (GUI) |  |  | 28.1 ms [27.1-28.2] |  |  |
| Noise tweak to redrawn frame (GUI) |  |  | 21.8 ms [21.7-22.4] |  |  |
| Orbit (GUI) |  |  | 568 fps [418-573] |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  |  | 56.5 ms [55.0-59.1] |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  |  | 15.9 ms [15.9-16.0] |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  |  | 6.95 ms [6.93-7.03] |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  |  | 15.1 ms [14.7-15.2] |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  |  | 47.8 ms [47.6-48.1] |  |  |
| Brush release (commit), to redrawn frame (GUI) |  |  | 52.1 ms [51.9-53.4] |  |  |

## 500000 strands, 16 points each

| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |
| --- | --- | --- | --- | --- | --- |
| Strands made |  |  | 500008  [500008-500008] |  |  |
| Plugin load |  |  | 0.16 s [0.15-0.16] |  |  |
| Creation, cold (batch: create + first evaluation) |  |  | 0.32 s [0.32-0.32] |  |  |
| Creation, warm (batch) |  |  | 0.27 s [0.26-0.27] |  |  |
| Creation working set delta (batch, cold) |  |  | 569 MB [568-572] |  |  |
| Creation private bytes delta (batch, cold) |  |  | 566 MB [565-570] |  |  |
| Clump tweak to evaluated (batch) |  |  | 60.4 ms [59.6-60.4] |  |  |
| Noise tweak to evaluated (batch) |  |  | 47.2 ms [47.1-47.4] |  |  |
| Scene .ma size |  |  | 2.83 MB [2.83-2.83] |  |  |
| Scene .mb size |  |  | 2.24 MB [2.24-2.24] |  |  |
| Scene .mb save |  |  | 0.02 s [0.02-0.02] |  |  |
| Open saved .mb + evaluate |  |  | 0.25 s [0.25-0.25] |  |  |
| Arnold .ass export, static |  |  | 0.58 s [0.58-0.58] |  |  |
| .ass size, static |  |  | 169 MB [169-169] |  |  |
| Creation to first drawn frame (GUI) |  |  | 0.55 s [0.44-0.55] |  |  |
| Creation working set delta (GUI, incl. draw buffers) |  |  | 1077 MB [1077-1083] |  |  |
| GPU memory delta, whole device (GUI, nvidia-smi) |  |  | 446 MB [430-447] |  |  |
| Redraw, nothing changed (GUI) |  |  | 3.54 ms [2.99-4.30] |  |  |
| Clump tweak to redrawn frame (GUI) |  |  | 74.4 ms [73.6-75.9] |  |  |
| Noise tweak to redrawn frame (GUI) |  |  | 61.2 ms [61.0-62.9] |  |  |
| Orbit (GUI) |  |  | 198 fps [165-200] |  |  |
| Brush stroke, 9 dabs + commit, to redrawn frame (GUI) |  |  | 150 ms [149-152] |  |  |
| Brush dab while dragging, to redrawn frame (GUI) |  |  | 41.9 ms [41.7-41.9] |  |  |
| Brush press (first dab), to redrawn frame (GUI) |  |  | 14.5 ms [14.4-14.6] |  |  |
| Brush press, the session's first stroke, to redrawn frame (GUI) |  |  | 46.5 ms [45.1-46.8] |  |  |
| Brush release, the session's first stroke, to redrawn frame (GUI) |  |  | 144 ms [142-147] |  |  |
| Brush release (commit), to redrawn frame (GUI) |  |  | 160 ms [157-162] |  |  |

