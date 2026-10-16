# SPDX-License-Identifier: Apache-2.0
# The showcase renders and benchmarks at 1080p (run from a build with the app: build/app, or pass -Build).
# The assets come from the scripts beside this one (glb_to_obj.py, make_head_asset.py, make_scalp.py)
# in %USERPROFILE%/groomlab-deps/assets/lee_perry_smith, never in the repository. Head scan: Lee Perry-Smith / Infinite Realities, CC BY 3.0.
param([string]$Build = "build\app")
Set-Location (Resolve-Path "$PSScriptRoot\..\..\..\..")  # the repository root
$env:QT_QPA_PLATFORM = "offscreen"
$d = "$env:USERPROFILE\groomlab-deps\assets\lee_perry_smith"
$out = "$env:USERPROFILE\groomlab-deps\assets\renders"
New-Item -ItemType Directory -Force $out | Out-Null
$exe = ".\$Build\hosts\app\RelWithDebInfo\GroomLab.exe"
$head = @("--prop", "$d\head.obj", "--prop-material", "skin")  # the scan as it is: lids closed
$groom = @("$d\head_scalp.obj", "--graph", "tests\data\app\scan_head_graph.json", "--hide-scalp")
$size = "1920x1080"
$log = "$out\showcase_log.txt"
"Showcase renders, $(Get-Date). Head scan: Lee Perry-Smith / Infinite Realities, CC BY 3.0" | Out-File $log -Encoding utf8

# Tee-Object appends UTF-16 to a UTF-8 file: the log stays UTF-8 through this.
function Log { process { $_ | Out-File $log -Append -Encoding utf8; $_ } }

function Run($name, $argList) {
    $t = Get-Date
    $text = & $exe @argList 2>&1 | Out-String
    $line = ($text -split "`n" | Select-String "wrote|ms/frame|Cannot|No " | ForEach-Object { $_.Line.Trim() }) -join " | "
    "$name : $line ($([int]((Get-Date) - $t).TotalSeconds) s)" | Log
}

# Full head with the groom, three lighting setups.
Run "head_groom_studio" ($groom + $head + @("--lighting", "studio", "--orbit", "-25", "-12", "--dolly", "0.5", "--screenshot", "$out\head_groom_studio.png", "--size", $size, "--frames", "64"))
Run "head_groom_daylight" ($groom + $head + @("--lighting", "daylight", "--orbit", "-35", "-10", "--dolly", "0.5", "--screenshot", "$out\head_groom_daylight.png", "--size", $size, "--frames", "64"))
Run "head_groom_sunset" ($groom + $head + @("--lighting", "sunset", "--orbit", "40", "-8", "--dolly", "0.5", "--screenshot", "$out\head_groom_sunset.png", "--size", $size, "--frames", "64"))
Run "head_groom_rim" ($groom + $head + @("--lighting", "rim", "--orbit", "150", "-5", "--dolly", "0.5", "--screenshot", "$out\head_groom_rim.png", "--size", $size, "--frames", "64"))
# Skin close-up, with and without the groom's shadows.
Run "skin_closeup_studio" ($groom + $head + @("--lighting", "studio", "--look-at", "1.5,3.5,10", "--distance", "17", "--orbit", "-25", "-8", "--screenshot", "$out\skin_closeup_studio.png", "--size", $size, "--frames", "64"))
Run "skin_closeup_daylight" ($head + @("--lighting", "daylight", "--look-at", "1.5,4.5,10", "--distance", "17", "--orbit", "-20", "-5", "--screenshot", "$out\skin_closeup_daylight.png", "--size", $size, "--frames", "64"))
# Before (plain Lambert with the albedo: the old scalp shading) and after (skin), the same view.
$before = @("--prop", "$d\head.obj", "--prop-material", "clay", "--prop-set", "roughness=1", "--prop-set", "specular=0", "--prop-set", "baseColor.r=1", "--prop-set", "baseColor.g=1", "--prop-set", "baseColor.b=1", "--prop-set", "normalStrength=0", "--prop-set", "occlusion=0")
Run "before_lambert" ($before + @("--lighting", "studio", "--orbit", "-25", "-12", "--dolly", "0.42", "--screenshot", "$out\before_lambert.png", "--size", $size, "--frames", "64"))
Run "after_skin" ($head + @("--lighting", "studio", "--orbit", "-25", "-12", "--dolly", "0.42", "--screenshot", "$out\after_skin.png", "--size", $size, "--frames", "64"))
Run "after_skin_no_sss" ($head + @("--prop-set", "sssStrength=0", "--prop-set", "transmission=0", "--prop-set", "lobe2Weight=0", "--lighting", "studio", "--orbit", "-25", "-12", "--dolly", "0.42", "--screenshot", "$out\after_skin_no_sss.png", "--size", $size, "--frames", "64"))
# The pre-integrated fallback (no screen-space blur) in the same view, and the key alone (the shadows and the terminator by themselves).
Run "after_skin_preintegrated" ($head + @("--prop-set", "sssMode=preintegrated", "--lighting", "studio", "--orbit", "-25", "-12", "--dolly", "0.42", "--screenshot", "$out\after_skin_preintegrated.png", "--size", $size, "--frames", "64"))
Run "skin_key_only" ($head + @("--lighting", "key", "--orbit", "-25", "-12", "--dolly", "0.42", "--screenshot", "$out\skin_key_only.png", "--size", $size, "--frames", "64"))
# The silhouette at a grazing view (no dark rim), and the ear against a back light (light through it).
Run "skin_grazing_studio" ($head + @("--lighting", "studio", "--orbit", "80", "-5", "--dolly", "0.42", "--screenshot", "$out\skin_grazing_studio.png", "--size", $size, "--frames", "64"))
Run "ear_backlit" ($head + @("--lighting", "key", "--look-at", "-7.5,5,0", "--distance", "14", "--orbit", "150", "0", "--screenshot", "$out\ear_backlit_skin.png", "--size", $size, "--frames", "32"))
# The app against Arnold (kick, standard_surface randomwalk): arnold_skin_<view>.png side by sides (python, PIL).
python tests\data\app\scan_head\arnold_skin_compare.py --build $Build --view all --size $size --frames 64 2>&1 | Select-String "mean \|" | ForEach-Object { $_.Line } | Log
# Frame times at 1080p: interactive frames and refine frames, head with the groom, and the head alone.
Run "bench_groom_interactive" ($groom + $head + @("--lighting", "studio", "--quality", "interactive", "--orbit", "-25", "-12", "--dolly", "0.5", "--benchmark", "40", "--size", $size))
Run "bench_groom_refine" ($groom + $head + @("--lighting", "studio", "--quality", "refine", "--orbit", "-25", "-12", "--dolly", "0.5", "--benchmark", "40", "--size", $size))
Run "bench_head_interactive" ($head + @("--lighting", "studio", "--quality", "interactive", "--orbit", "-25", "-12", "--dolly", "0.42", "--benchmark", "40", "--size", $size))
Run "bench_head_refine" ($head + @("--lighting", "studio", "--quality", "refine", "--orbit", "-25", "-12", "--dolly", "0.42", "--benchmark", "40", "--size", $size))
Run "bench_lambert_interactive" ($before + @("--lighting", "studio", "--quality", "interactive", "--orbit", "-25", "-12", "--dolly", "0.42", "--benchmark", "40", "--size", $size))
"done" | Log

