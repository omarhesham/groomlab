# SPDX-License-Identifier: Apache-2.0
"""GroomLab vs XGen Interactive Grooming: the benchmark driver. See README.md.

    python run_bench.py [--runs 5] [--counts 100000,500000] [--parts batch,render,gui]
                        [--build build/wt] [--maya "C:/Program Files/Autodesk/Maya2022"]

Runs every measurement in a fresh Maya process (mayapy for batch work, maya.exe for the GUI
ones), alternating which system goes first on every run, renders the exported .ass files with
kick, and writes results/<date>.json (every raw number, the machine, its load) and
results/<date>.md (the medians, with the caveats). Standard library only: any Python 3 (or
mayapy) runs it.
"""

import argparse
import datetime
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", ".."))


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


def powershell_json(command):
    try:
        out = subprocess.run(["powershell", "-NoProfile", "-Command", command + " | ConvertTo-Json -Compress"],
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, universal_newlines=True,
                             timeout=60).stdout
        return json.loads(out) if out.strip() else None
    except Exception as e:
        return "error: %s" % e


def machine():
    return {
        "cpu": powershell_json("Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,"
                               "NumberOfLogicalProcessors,MaxClockSpeed"),
        "ram_gb": powershell_json("[math]::Round((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1GB,1)"),
        "gpu": powershell_json("Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion"),
        "os": platform.platform(),
        "power_plan": powershell_json("(powercfg /getactivescheme) -join ' '"),
    }


def load_now():
    """The machine's load right now: CPU %, free RAM, the busiest other processes."""
    return powershell_json(
        "$c = (Get-CimInstance Win32_Processor | Measure-Object -Property LoadPercentage -Average).Average; "
        "$m = [math]::Round((Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory/1MB,1); "
        "$p = Get-Process | Sort-Object CPU -Descending | Select-Object -First 5 | ForEach-Object { $_.ProcessName }; "
        "@{cpu_percent=$c; free_ram_gb=$m; top_cpu_time=$p}")


def seed_prefs(app_dir, trusted, draw_api=None):
    """A MAYA_APP_DIR of our own, like tests/hosts/maya/trust_plugin_folder.cmake: Maya's version stamp
    (else it resets the prefs as a first launch), no first-run screens, and the plugin folder on
    the trusted list (else secure loading asks). Written before Maya starts, never at runtime.
    draw_api "DirectX11" (or "OpenGLCoreProfileStrict"...): Viewport 2.0's rendering engine, as an
    artist's prefs set it (fresh prefs draw with OpenGL Core Profile)."""
    prefs = os.path.join(app_dir, "2022", "prefs", "userPrefs.mel")
    os.makedirs(os.path.dirname(prefs), exist_ok=True)
    text = open(prefs).read() if os.path.exists(prefs) else ""
    if "optionVar -version" not in text:
        text = ('//Maya Preference 2022 (Release 1)\n//\n//\noptionVar -version 3;\n'
                'optionVar -iv "isFirstTimeRunningMaya" 0;\n') + text
    if '"%s"' % trusted not in text:
        text += '\noptionVar -stringValueAppend "SafeModeAllowedlistPaths" "%s";\n' % trusted
    if draw_api:
        text = re.sub(r'\noptionVar -sv "vp2RenderingEngine" "[^"]*";\n', "\n", text)
        text += '\noptionVar -sv "vp2RenderingEngine" "%s";\n' % draw_api
    with open(prefs, "w") as f:
        f.write(text)


def run_job(command, env, log_path, timeout):
    t = time.perf_counter()
    with open(log_path, "w") as f:
        p = subprocess.Popen(command, env=env, stdout=f, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL)
        try:
            p.wait(timeout=timeout)
            status = p.returncode
        except subprocess.TimeoutExpired:
            p.kill()
            p.wait()
            status = "timeout"
    return status, time.perf_counter() - t


def kick(ass, image, env, maya, log_path, resolution=(960, 540), timeout=900):
    arnold = os.path.join(os.path.dirname(maya), "Arnold", "maya2022")
    command = [os.path.join(arnold, "bin", "kick.exe"), "-i", ass, "-l", os.path.join(arnold, "procedurals"),
               "-l", os.path.join(arnold, "shaders"), "-o", image, "-r", str(resolution[0]), str(resolution[1]),
               "-dw", "-dp", "-sl", "-v", "2"]
    status, wall = run_job(command, env, log_path, timeout)
    text = open(log_path, errors="replace").read()
    out = {"wall_s": wall, "status": status}
    m = re.findall(r"render done in (\d+):(\d+\.\d+)", text)
    if m:
        out["render_s"] = int(m[-1][0]) * 60 + float(m[-1][1])
    m = re.search(r"node initialization done in (\d+):(\d+\.\d+)", text)
    if m:
        out["node_init_s"] = int(m.group(1)) * 60 + float(m.group(2))
    m = re.findall(r"peak CPU memory used\s+([\d.]+)MB", text)
    if m:
        out["peak_mb"] = float(m[-1])
    out["watermark"] = "watermark" in text
    out["errors"] = [line for line in text.splitlines() if "ERROR" in line][:5]
    return out


def median(xs):
    xs = sorted(x for x in xs if isinstance(x, (int, float)))
    n = len(xs)
    return None if not n else (xs[n // 2] if n % 2 else 0.5 * (xs[n // 2 - 1] + xs[n // 2]))


def get(d, path):
    for k in path.split("."):
        if isinstance(d, list):
            d = d[int(k)] if k.isdigit() and int(k) < len(d) else None
        elif isinstance(d, dict):
            d = d.get(k)
        else:
            return None
    return d


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--counts", default="100000,500000")
    ap.add_argument("--parts", default="batch,render,gui")
    ap.add_argument("--build", default=os.path.join(REPO, "build", "wt"))
    ap.add_argument("--maya", default=r"C:\Program Files\Autodesk\Maya2022")
    ap.add_argument("--work", default=os.path.join(os.environ.get("TEMP", HERE), "xgen_compare_work"))
    ap.add_argument("--out", default=os.path.join(HERE, "results"))
    ap.add_argument("--tag", default=datetime.date.today().isoformat())
    ap.add_argument("--tweaks", type=int, default=7)
    ap.add_argument("--keep", action="store_true", help="keep every run's exports and scenes")
    ap.add_argument("--systems", default="xgen,groomlab,groomlab_nocurl,groomlab_follow",
                    help="which systems run (any subset): a GroomLab-only run leaves out xgen; groomlab_gpu "
                         "(GUI only) times the GPU look preview against the CPU in one session")
    ap.add_argument("--draw-api", default="", help="Viewport 2.0's engine for the GUI jobs: DirectX11, "
                    "OpenGLCoreProfileStrict, ... (default: what fresh prefs give, OpenGL Core Profile)")
    ap.add_argument("--phases", default="static,anim", help="static, anim or both")
    a = ap.parse_args()

    plugin = os.path.join(a.build, "hosts", "maya", "RelWithDebInfo", "GroomLab.mll")
    extensions = os.path.join(a.build, "hosts", "maya", "extensions", "RelWithDebInfo")
    scripts = os.path.join(REPO, "hosts", "maya", "scripts")
    # groomlab_core, the engine's Python module the scripts import
    python = os.path.join(a.build, "engine", "python", "RelWithDebInfo")
    for p in (plugin, extensions, scripts, python):
        if not os.path.exists(p):
            sys.exit("missing %s (build first: see README.md)" % p)
    counts = [int(c) for c in a.counts.split(",") if c]
    parts = set(a.parts.split(","))
    systems = set(a.systems.split(","))
    phases = set(a.phases.split(","))
    os.makedirs(a.work, exist_ok=True)
    os.makedirs(a.out, exist_ok=True)

    batch_app = os.path.join(a.work, "app_batch")
    gui_app = os.path.join(a.work, "app_gui" + ("_" + a.draw_api if a.draw_api else ""))
    os.makedirs(batch_app, exist_ok=True)
    seed_prefs(gui_app, os.path.dirname(plugin).replace("\\", "/"), a.draw_api or None)
    env = dict(os.environ)
    env["MAYA_APP_DIR"] = batch_app
    env["PATH"] = os.pathsep.join([os.path.join(a.maya, "bin"), os.path.join(a.maya, "plug-ins", "xgen", "bin"),
                                   env.get("PATH", "")])
    gui_env = dict(env)
    gui_env["MAYA_APP_DIR"] = gui_app
    mayapy = os.path.join(a.maya, "bin", "mayapy.exe")
    maya_exe = os.path.join(a.maya, "bin", "maya.exe")

    result_path = os.path.join(a.out, "%s.json" % a.tag)
    results = {"tag": a.tag, "started": datetime.datetime.now().isoformat(timespec="seconds"),
               "machine": machine(), "maya": a.maya, "plugin": plugin,
               "groomlab_commit": subprocess.run(["git", "-C", REPO, "rev-parse", "--short", "HEAD"],
                                                 stdout=subprocess.PIPE, universal_newlines=True).stdout.strip(),
               "counts": counts, "runs": a.runs, "draw_api": a.draw_api or "default", "jobs": []}

    def save():
        with open(result_path, "w") as f:
            json.dump(results, f, indent=1)

    def job(kind, system, count, phase, run):
        name = "%s_%s_%s_%d_r%d" % (kind, system, phase, count, run)
        work = os.path.join(a.work, name)
        shutil.rmtree(work, ignore_errors=True)
        os.makedirs(work)
        args = {"system": system, "count": count, "phase": phase, "out": os.path.join(work, "result.json"),
                "work": work, "plugin": plugin, "scripts": scripts, "python": python, "extensions": extensions,
                "tweaks": a.tweaks, "self": os.path.join(HERE, "xgc_gui.py")}
        args_path = os.path.join(work, "args.json")
        with open(args_path, "w") as f:
            json.dump(args, f)
        entry = {"kind": kind, "system": system, "count": count, "phase": phase, "run": run,
                 "load_before": load_now()}
        log("start", name)
        if kind == "batch":
            status, wall = run_job([mayapy, os.path.join(HERE, "xgc_batch.py"), args_path], env,
                                   os.path.join(work, "log.txt"), 3600)
        else:
            mel = os.path.join(work, "run.mel")
            with open(mel, "w") as f:
                f.write("python(\"exec(open(r'%s').read())\");\n" % os.path.join(HERE, "xgc_gui.py").replace("\\", "/"))
            genv = dict(gui_env)
            genv["XGC_ARGS"] = args_path
            # The GPU look preview is on by default; the CPU systems' numbers are the CPU's alone.
            genv["GROOMLAB_GPU_PREVIEW"] = "1" if system == "groomlab_gpu" else "0"
            status, wall = run_job([maya_exe, "-script", mel], genv, os.path.join(work, "log.txt"), 3600)
        entry.update({"status": status, "process_wall_s": wall})
        try:
            with open(args["out"]) as f:
                entry["result"] = json.load(f)
        except Exception as e:
            entry["result"] = {"errors": ["no result: %s" % e]}
        if kind == "batch" and "render" in parts:
            for key in ("ass_static", "ass_motion"):
                ass = get(entry["result"], key + ".path")
                if ass and os.path.exists(ass):
                    entry["result"][key]["kick"] = kick(ass, os.path.splitext(ass)[0] + ".exr", env, a.maya,
                                                        os.path.splitext(ass)[0] + "_kick.txt")
        if run > 0 and not a.keep:
            # Big outputs (500k strands: GBs of Alembic) are kept for the first run only.
            for root, _, names in os.walk(work):
                for n in names:
                    if os.path.splitext(n)[1].lower() in (".abc", ".ass", ".exr", ".ma", ".mb", ".xgsd"):
                        os.remove(os.path.join(root, n))
        entry["load_after"] = load_now()
        errs = get(entry, "result.errors")
        log("done", name, status, "%.1fs" % wall, ("ERRORS: " + errs[0][-300:]) if errs else "")
        results["jobs"].append(entry)
        save()

    for run in range(a.runs):
        # Alternate who goes first. Static: XGen, GroomLab, GroomLab without Curl (XGen's
        # operators only); deforming: XGen, GroomLab, GroomLab with followScalp.
        static_order = [s for s in ["xgen", "groomlab", "groomlab_nocurl", "groomlab_gpu"]
                        if s in systems and "static" in phases]
        anim_order = [s for s in ["xgen", "groomlab", "groomlab_follow"] if s in systems and "anim" in phases]
        if run % 2:
            static_order.reverse()
            anim_order.reverse()
        for count in counts:
            if "batch" in parts:
                for s in static_order:
                    if s != "groomlab_gpu":  # a viewport measure only
                        job("batch", s, count, "static", run)
                for s in anim_order:
                    job("batch", s, count, "anim", run)
            if "gui" in parts:
                for s in static_order:
                    job("gui", s, count, "static", run)
                for s in anim_order:
                    job("gui", s, count, "anim", run)
    results["finished"] = datetime.datetime.now().isoformat(timespec="seconds")
    results["summary"] = summarize(results)
    save()
    with open(os.path.join(a.out, "%s.md" % a.tag), "w") as f:
        f.write(markdown(results))
    log("wrote", result_path)


# -- Summary -------------------------------------------------------------------------------------

METRICS = [
    # (label, kind, phase, path in a job's result, unit, scale)
    ("Strands made", "batch", "static", "creation_cold.count", "", 1),
    ("Plugin load", "batch", "static", "plugin_load_s", "s", 1),
    ("Creation, cold (batch: create + first evaluation)", "batch", "static", "creation_cold.create_s", "s", 1),
    ("Creation, warm (batch)", "batch", "static", "creation_warm.0.create_s", "s", 1),
    ("Creation working set delta (batch, cold)", "batch", "static", "creation_cold.ws_delta_mb", "MB", 1),
    ("Creation private bytes delta (batch, cold)", "batch", "static", "creation_cold.private_delta_mb", "MB", 1),
    ("Clump tweak to evaluated (batch)", "batch", "static", "tweak.clump_median_s", "ms", 1000),
    ("Noise tweak to evaluated (batch)", "batch", "static", "tweak.noise_median_s", "ms", 1000),
    ("Scene .ma size", "batch", "static", "scene.ma.size_bytes", "MB", 1e-6),
    ("Scene .mb size", "batch", "static", "scene.mb.size_bytes", "MB", 1e-6),
    ("Scene .mb save", "batch", "static", "scene.mb.save_s", "s", 1),
    ("Open saved .mb + evaluate", "batch", "static", "reopen.open_and_eval_s", "s", 1),
    ("Arnold .ass export, static", "batch", "static", "ass_static.export_s", "s", 1),
    (".ass size, static", "batch", "static", "ass_static.size_bytes", "MB", 1e-6),
    ("kick render, static (wall)", "batch", "static", "ass_static.kick.wall_s", "s", 1),
    ("kick render, static (render done in)", "batch", "static", "ass_static.kick.render_s", "s", 1),
    ("Per-frame evaluation, deforming (batch, no draw)", "batch", "anim", "frames.median_s", "ms", 1000),
    ("Alembic export, 24 frames", "batch", "anim", "alembic.export_s", "s", 1),
    ("Alembic size, 24 frames", "batch", "anim", "alembic.size_bytes", "MB", 1e-6),
    ("Arnold .ass export, 2-key motion blur", "batch", "anim", "ass_motion.export_s", "s", 1),
    (".ass size, motion blur", "batch", "anim", "ass_motion.size_bytes", "MB", 1e-6),
    ("kick render, motion blur (wall)", "batch", "anim", "ass_motion.kick.wall_s", "s", 1),
    ("kick render, motion blur (render done in)", "batch", "anim", "ass_motion.kick.render_s", "s", 1),
    ("Creation to first drawn frame (GUI)", "gui", "static", "creation.to_first_frame_s", "s", 1),
    ("Creation working set delta (GUI, incl. draw buffers)", "gui", "static", "creation.ws_delta_mb", "MB", 1),
    ("GPU memory delta, whole device (GUI, nvidia-smi)", "gui", "static", "creation.gpu_device_delta_mb", "MB", 1),
    ("Redraw, nothing changed (GUI)", "gui", "static", "creation.redraw_s", "ms", 1000),
    ("Clump tweak to redrawn frame (GUI)", "gui", "static", "tweak.clump_median_s", "ms", 1000),
    ("Noise tweak to redrawn frame (GUI)", "gui", "static", "tweak.noise_median_s", "ms", 1000),
    ("Orbit (GUI)", "gui", "static", "orbit.fps", "fps", 1),
    ("Brush stroke, 9 dabs + commit, to redrawn frame (GUI)", "gui", "static", "brush.median_s", "ms", 1000),
    ("Brush dab while dragging, to redrawn frame (GUI)", "gui", "static", "brush.dab_median_s", "ms", 1000),
    ("Brush press (first dab), to redrawn frame (GUI)", "gui", "static", "brush.press_median_s", "ms", 1000),
    ("Brush press, the session's first stroke, to redrawn frame (GUI)", "gui", "static", "brush.first_press_s", "ms", 1000),
    ("Brush release, the session's first stroke, to redrawn frame (GUI)", "gui", "static", "brush.first_release_s", "ms", 1000),
    ("Brush release (commit), to redrawn frame (GUI)", "gui", "static", "brush.release_median_s", "ms", 1000),
    ("Playback, deforming scalp, 1st pass (GUI, play)", "gui", "anim", "playback.play0.fps", "fps", 1),
    ("Playback, deforming scalp, 2nd pass (GUI, play)", "gui", "anim", "playback.play1.fps", "fps", 1),
    ("Stepping frames + redraw, deforming (GUI)", "gui", "anim", "playback.stepped.fps", "fps", 1),
    # The GPU look preview (groomlab_gpu: CPU and GPU in one session, each tweak timed to its drawn
    # frame with the GPU waited for, groomLabGpuPreview -sync).
    ("Noise tweak to drawn frame, GPU waited for: CPU evaluation", "gui", "static", "gpu.cpu_noise_median_s", "ms", 1000),
    ("Clump tweak to drawn frame, GPU waited for: CPU evaluation", "gui", "static", "gpu.cpu_clump_median_s", "ms", 1000),
    ("Noise drag to drawn frame: GPU preview", "gui", "static", "gpu.gpu_noise_median_s", "ms", 1000),
    ("Noise drag to drawn frame: GPU preview, GPU at idle clocks", "gui", "static", "gpu.cold_median_s", "ms", 1000),
    ("Curl drag to drawn frame: GPU preview", "gui", "static", "gpu.gpu_curl_median_s", "ms", 1000),
    ("Width drag to drawn frame: GPU preview", "gui", "static", "gpu.gpu_width_median_s", "ms", 1000),
    ("Clump drag to drawn frame: GPU preview (Clump, Noise, Curl, Width)", "gui", "static", "gpu.gpu_clump_median_s", "ms", 1000),
    ("Interpolate drag to drawn frame: GPU preview (from Interpolate on)", "gui", "static", "gpu.gpu_interpolate_median_s", "ms",
     1000),
    ("GPU kernels of a Clump drag", "gui", "static", "gpu.kernels_clump_drag_total_median_ms", "ms", 1),
    ("GPU kernels of an Interpolate drag", "gui", "static", "gpu.kernels_interpolate_drag_total_median_ms", "ms", 1),
    ("GPU Interpolate kernel (an Interpolate drag)", "gui", "static", "gpu.kernels_interpolate_drag_median_ms.interpolate", "ms", 1),
    ("GPU Clump kernel and its centers (an Interpolate drag)", "gui", "static", "gpu.kernels_interpolate_drag_median_ms.clump", "ms",
     1),
    ("GPU kernels (timestamps; with the copy into VP2's buffers where there is one)", "gui", "static",
     "gpu.kernels_total_median_ms", "ms", 1),
    ("GPU Noise kernel", "gui", "static", "gpu.kernels_noise_median_ms", "ms", 1),
    ("GPU Curl kernel", "gui", "static", "gpu.kernels_curl_median_ms", "ms", 1),
    ("GPU lit frames kernel", "gui", "static", "gpu.kernels_lines_median_ms", "ms", 1),
    ("GPU copy into VP2 buffers (0: written in place)", "gui", "static", "gpu.kernels_copy_median_ms", "ms", 1),
    ("GPU preview apply on the CPU (params, dispatch)", "gui", "static", "gpu.apply_cpu_median_ms", "ms", 1),
    ("First drag (engine made, strands uploaded)", "gui", "static", "gpu.first_s", "ms", 1000),
    ("First drag: GPU preview apply on the CPU", "gui", "static", "gpu.first_stats.cpuMs", "ms", 1),
    ("Noise drag after a clump change (CPU to Clump + upload + GPU)", "gui", "static", "gpu.after_clump_s", "ms", 1000),
    ("Upload of Clump's strands (CPU side)", "gui", "static", "gpu.upload_ms", "ms", 1),
    ("Upload size", "gui", "static", "gpu.upload_mb", "MB", 1),
    ("Noise drag after a brush stroke (CPU stroke + partial upload + GPU)", "gui", "static", "gpu.after_stroke_s", "ms", 1000),
    ("Upload after the stroke (CPU side): the stroke's strands", "gui", "static", "gpu.stroke_upload_ms", "ms", 1),
    ("Upload size after the stroke", "gui", "static", "gpu.stroke_upload_mb", "MB", 1),
    ("Strands uploaded after the stroke", "gui", "static", "gpu.stroke_upload_strands", "", 1),
    ("GPU preview engine buffers (VRAM)", "gui", "static", "gpu.video_mb", "MB", 1),
    ("GPU preview's own vertex buffers during a drag (VRAM, DirectX 11)", "gui", "static", "gpu.display_mb", "MB", 1),
    ("Release: drag ends, CPU commit to drawn frame", "gui", "static", "gpu.commit_median_s", "ms", 1000),
    ("Release of a Clump drag: CPU commit to drawn frame", "gui", "static", "gpu.clump_commit_median_s", "ms", 1000),
    ("Brush dabs drawn by the GPU (of the strokes timed)", "gui", "static", "brush.gpu_stats.stroke.runs", "", 1),
    ("GPU vs CPU positions, max error", "gui", "static", "gpu.check.positions.max", "cm", 1),
    ("GPU vs CPU positions, mean error", "gui", "static", "gpu.check.positions.mean", "cm", 1),
    ("GPU vs CPU normals, max error", "gui", "static", "gpu.check.normals.max", "", 1),
    ("GPU vs CPU positions from Clump, max error", "gui", "static", "gpu.check_clump.positions.max", "cm", 1),
    ("GPU vs CPU positions from Interpolate, max error", "gui", "static", "gpu.check_interpolate.positions.max", "cm", 1),
    ("GPU vs CPU positions from Interpolate, mean error", "gui", "static", "gpu.check_interpolate.positions.mean", "cm", 1),
    ("Frame pixels differing: GPU drag vs CPU commit, same value", "gui", "static",
     "gpu.frames_compared.gpu_vs_cpu.share", "%", 100),
    ("Frame pixels differing: GPU Clump drag vs CPU commit, same value", "gui", "static",
     "gpu.frames_compared.gpu_clump_vs_cpu.share", "%", 100),
    ("Frame pixels differing: GPU drag in the frame a clump change evaluates vs CPU", "gui", "static",
     "gpu.frames_compared.gpu_in_cpu_frame_vs_cpu.share", "%", 100),
    ("Frame pixels differing: CPU vs the same CPU frame again", "gui", "static",
     "gpu.frames_compared.cpu_vs_same_cpu.share", "%", 100),
    ("Frame pixels differing: CPU vs CPU at a value 1% away", "gui", "static",
     "gpu.frames_compared.cpu_vs_cpu_1pc_away.share", "%", 100),
]
SYSTEMS = ["xgen", "groomlab", "groomlab_nocurl", "groomlab_follow", "groomlab_gpu"]


def summarize(results):
    out = {}
    for label, kind, phase, path, unit, scale in METRICS:
        for count in results["counts"]:
            for s in SYSTEMS:
                vals = [get(j, "result." + path) for j in results["jobs"]
                        if j["kind"] == kind and j["phase"] == phase and j["system"] == s and j["count"] == count]
                vals = [v * scale for v in vals if isinstance(v, (int, float))]
                if vals:
                    out.setdefault(label, {}).setdefault(str(count), {})[s] = {
                        "median": median(vals), "min": min(vals), "max": max(vals), "n": len(vals), "unit": unit}
    return out


def markdown(results):
    lines = ["# GroomLab vs XGen Interactive Grooming: %s" % results["tag"], ""]
    m = results["machine"]
    cpu = m.get("cpu") or {}
    gpu = m.get("gpu")
    gpu = gpu[0] if isinstance(gpu, list) else gpu
    lines += ["Machine: %s (%s cores / %s threads), %s GB RAM, %s (driver %s). GroomLab %s. Medians of %d runs "
              "(min-max in brackets)." % (cpu.get("Name", "?").strip(), cpu.get("NumberOfCores"),
                                          cpu.get("NumberOfLogicalProcessors"), m.get("ram_gb"),
                                          (gpu or {}).get("Name"), (gpu or {}).get("DriverVersion"),
                                          results.get("groomlab_commit"), results["runs"]), ""]
    for count in results["counts"]:
        lines += ["## %d strands, %d points each" % (count, 16), "",
                  "| Measure | XGen IGS | GroomLab | GroomLab without Curl | GroomLab followScalp | GroomLab GPU preview |",
                  "| --- | --- | --- | --- | --- | --- |"]
        for label, kind, phase, path, unit, scale in METRICS:
            row = (results["summary"].get(label) or {}).get(str(count)) or {}
            if not row:
                continue
            cells = []
            for s in SYSTEMS:
                v = row.get(s)
                if not v:
                    cells.append("")
                    continue
                fmt = ("%.0f" if v["median"] >= 100 else "%.1f" if v["median"] >= 10 else "%.2f" if v["median"] >= 0.01
                       else "%.2g")
                cells.append(("%s %s" % (fmt, unit.replace("%", "%%"))) % v["median"] + (" [" + fmt + "-" + fmt + "]") % (v["min"], v["max"]))
            lines.append("| %s | %s |" % (label, " | ".join(cells)))
        lines.append("")
    return "\n".join(lines) + "\n"


if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--summarize":
        # Rebuild the summary and the .md from a results JSON (after editing METRICS, say).
        with open(sys.argv[2]) as f:
            r = json.load(f)
        r["summary"] = summarize(r)
        with open(sys.argv[2], "w") as f:
            json.dump(r, f, indent=1)
        with open(os.path.splitext(sys.argv[2])[0] + ".md", "w") as f:
            f.write(markdown(r))
    else:
        main()
