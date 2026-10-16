# SPDX-License-Identifier: Apache-2.0
"""Tables from the performance audit's benchmark lines (JSON lines).

The GPU look engine's benchmark (tests/engine/gpu/bench_look_engine.cpp, ctest gpu.bench) and the
viewport benchmark (tests/hosts/maya/gui_bench.py, ctest maya.viewport_bench / _dx11) append one
JSON object a line to GROOMLAB_BENCH_OUT. This prints them as Markdown tables:

    set GROOMLAB_BENCH=1
    set GROOMLAB_BENCH_OUT=%TEMP%\\bench.jsonl
    set GROOMLAB_BENCH_COUNTS=100000,500000,1000000
    ctest --preset ninja-maya2022 -L bench
    python tools/bench/gpu_audit/summarize.py %TEMP%\\bench.jsonl
"""

import json
import sys


def table(head, rows):
    print("| " + " | ".join(head) + " |")
    print("|" + "---|" * len(head))
    for r in rows:
        print("| " + " | ".join(r) + " |")
    print()


def f(x, digits=2):
    return "-" if x is None else ("%.*f" % (digits, x))


def main(paths):
    rows = []
    for path in paths:
        with open(path) as fh:
            rows += [json.loads(line) for line in fh if line.strip()]
    by = lambda kind: [r for r in rows if r.get("bench") == kind]

    if by("cpu"):
        print("### CPU evaluation (24 threads)\n")
        table(["API run", "strands", "first evaluation ms", "Noise tweak ms", "Clump tweak ms"],
              [[r["api"], str(r["strands"]), f(r["firstEvaluationMs"], 0), f(r["noiseTweakMs"], 1), f(r["clumpTweakMs"], 1)]
               for r in by("cpu")])
    gpu = [r for r in by("gpu_from") if "refused" not in r and r.get("inPlace")]
    if gpu:
        print("### GPU look chain: a warm drag from each stage (GPU timestamps, ms)\n")
        table(["API", "strands", "from", "interpolate", "clearance", "clump", "noise", "curl", "width", "lines", "total",
               "run() CPU", "to GPU done", "first run CPU / done", "base upload ms / MB"],
              [[r["api"], str(r["strands"]), r["from"]] +
               [f(r["gpu"][k]) for k in ("interpolate", "clearance", "clump", "noise", "curl", "width", "lines", "total")] +
               [f(r["runCpuMs"], 3), f(r["runDoneMs"]), "%s / %s" % (f(r["firstRunCpuMs"], 1), f(r["firstRunDoneMs"], 1)),
                "%s / %s" % (f(r["baseUploadMs"], 1), f(r["baseUploadMb"], 0))] for r in gpu])
    if by("threads"):
        print("### A thread per point against a thread per strand (ms)\n")
        table(["API", "strands", "threads", "noise", "curl", "lines"],
              [[r["api"], str(r["strands"]), "point" if r["pointThreads"] else "strand",
                f(r["gpu"]["noise"]), f(r["gpu"]["curl"]), f(r["gpu"]["lines"])] for r in by("threads")])
    dabs = [r for r in by("dab") if "refused" not in r]
    if dabs:
        print("### A brush dab without a host (8 guides combed, ms)\n")
        table(["API", "strands", "CPU to Interpolate", "inputs read", "run() CPU", "GPU total", "interpolate", "clearance",
               "end to end"],
              [[r["api"], str(r["strands"]), f(r["cpuEvaluationToInterpolateMs"]), f(r["inputsReadMs"]), f(r["runCpuMs"], 3),
                f(r["gpu"]["total"]), f(r["gpu"]["interpolate"]), f(r["gpu"]["clearance"]), f(r["totalMs"])] for r in dabs])
    if by("preset"):
        print("### Shipped presets on the GPU preview\n")
        table(["preset", "GPU stages", "brush dabs on the GPU", "why it stops / refusals"],
              [[r["preset"], ", ".join(r.get("stages", [])) or "-",
                "yes" if r.get("stages") and r["stages"][0].startswith("interpolate") and
                r.get("from", {}).get(r["stages"][0].split(":")[1]) == "gpu" else "no",
                r.get("why") or r.get("error") or "; ".join("%s: %s" % kv for kv in r.get("from", {}).items() if kv[1] != "gpu")]
               for r in by("preset")])
    if by("preset_dab"):
        print("### Shipped looks: a brush dab (8 guides combed) at about 500k strands, ms\n")
        table(["API", "preset", "strands", "GPU stages", "CPU dab", "GPU dab (end to end)", "GPU total", "refused / drag"],
              [[r["api"], r["preset"], str(r.get("strands", "-")), " ".join(r.get("stages", [])) or "-",
                f(r.get("cpuDabMs") or None, 1), f(r.get("gpuDabMs"), 1), f((r.get("gpu") or {}).get("total")),
                r.get("refused") or ("%s.strength drag: GPU %s, CPU %s" % (r["dragNode"], f(r["gpuDragMs"], 1),
                                                                           f(r["cpuDragMs"], 1)) if "dragNode" in r else "")]
               for r in by("preset_dab")])
    if by("mode"):
        print("### Maya viewport, per display mode\n")
        table(["API", "strands", "mode", "orbit fps", "frame synced ms", "half view fps", "half view synced ms", "first draw ms"],
              [[r["api"], str(r["strands"]), r["mode"], f(r["orbitFps"], 0), f(r["syncedMs"], 1), f(r["halfOrbitFps"], 0),
                f(r["halfSyncedMs"], 1), f(r["firstDrawMs"], 0)] for r in by("mode")])
    if by("cpu_tweak"):
        print("### Maya: a Noise tweak on the CPU, to the drawn frame (GPU waited for)\n")
        table(["API", "strands", "ms", "compute", "display update", "build", "upload", "MB"],
              [[r["api"], str(r["strands"]), f(r["ms"], 1), f(r.get("computeMs"), 1), f(r["updateMs"], 1), f(r["buildMs"], 1),
                f(r["uploadMs"], 1), f(r["mb"], 0)] for r in by("cpu_tweak")])
    if by("gpu_drag"):
        print("### Maya: a Noise drag on the GPU preview, to the drawn frame\n")
        table(["API", "strands", "taken", "ms", "GPU total ms"],
              [[r["api"], str(r["strands"]), str(r.get("taken")), f(r.get("ms"), 1),
                f((r.get("gpu") or {}).get("totalMs"))] for r in by("gpu_drag")])
    if by("msaa"):
        print("### Maya: MSAA (lit lines, width as coverage)\n")
        table(["API", "strands", "MSAA", "orbit fps", "frame synced ms"],
              [[r["api"], str(r["strands"]), str(r["msaa"]), f(r["orbitFps"], 0), f(r["syncedMs"], 1)] for r in by("msaa")])


if __name__ == "__main__":
    main(sys.argv[1:])
