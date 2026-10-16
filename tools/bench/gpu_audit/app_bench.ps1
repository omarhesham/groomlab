# SPDX-License-Identifier: Apache-2.0
<#
.SYNOPSIS
The standalone app's viewport benchmark for the performance audit: frame times, orbits and drags
at 100k and 500k strands (every strand shown: --density 1 --point-budget 0), offscreen.

.DESCRIPTION
    powershell -NoProfile -ExecutionPolicy Bypass -File tools/bench/gpu_audit/app_bench.ps1 -Out <folder>

Writes the bench scene first (AppBench.WritesTheSceneForScreenshots: bench_100000.groom, the
xgen_compare head with its scalp cache), then runs GroomLab.exe once per case and keeps its
"GroomLab:" lines in <folder>/app_bench.txt. --benchmark frames wait for the GPU (offscreen.cpp
waitForGpu: an event query on Direct3D 11, a fence on 12, glFinish on OpenGL).
#>
param(
    [Parameter(Mandatory = $true)][string]$Out,
    [string]$Build = "build/ninja-app"
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$exe = Join-Path $root "$Build\hosts\app\RelWithDebInfo\GroomLab.exe"
$tests = Join-Path $root "$Build\hosts\app\RelWithDebInfo\groomlab_app_tests.exe"
New-Item -ItemType Directory -Force $Out | Out-Null
$sceneDir = Join-Path $Out "scene"
$env:GROOMLAB_APP_BENCH_SCENE_DIR = $sceneDir
& $tests --gtest_filter=AppBench.WritesTheSceneForScreenshots | Out-Null
$scene = Join-Path $sceneDir "bench_100000.groom"
# The scene's Scatter density for 100k strands (count / the head's area), and 5x for 500k.
$text = Get-Content $scene -Raw
$density = [double]([regex]::Match($text, '"name":"scatter","params":\{"density":([0-9.eE+-]+)').Groups[1].Value)
$log = Join-Path $Out "app_bench.txt"
Set-Content $log "" -Encoding utf8
function Run($label, $extra) {
    Add-Content $log "=== $label" -Encoding utf8
    $stdout = Join-Path $Out "one.txt"
    Start-Process -FilePath $exe -ArgumentList (@($scene) + $extra) -NoNewWindow -Wait -RedirectStandardOutput $stdout -RedirectStandardError (Join-Path $Out "err.txt") | Out-Null
    Get-Content $stdout | Select-String -Pattern "ms/frame|fps|benchmark drag" | ForEach-Object { Add-Content $log $_.Line -Encoding utf8 }
}
$density5 = 5.0 * $density
foreach ($case in @(@("100k", $density), @("500k", $density5))) {
    $n = $case[0]
    $set = @("--density", "1", "--point-budget", "0", "--set", "scatter.density=$($case[1])")
    Run "$n lookdev 1080p" ($set + @("--size", "1920x1080", "--benchmark", "30"))
    Run "$n lookdev 960x540" ($set + @("--size", "960x540", "--benchmark", "30"))
    Run "$n lines 1080p" ($set + @("--size", "1920x1080", "--no-lookdev", "--benchmark", "30"))
    Run "$n lookdev orbit 1080p" ($set + @("--size", "1920x1080", "--benchmark-orbit", "60"))
    Run "$n lines orbit 1080p" ($set + @("--size", "1920x1080", "--no-lookdev", "--benchmark-orbit", "60"))
    Run "$n lookdev drag noise 1080p" ($set + @("--size", "1920x1080", "--benchmark-drag", "noise.amplitude=0.4,0.6,12"))
}
Get-Content $log
