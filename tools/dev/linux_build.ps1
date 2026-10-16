# SPDX-License-Identifier: Apache-2.0
<#
.SYNOPSIS
    Builds and tests GroomLab for Linux in WSL, from Windows, in one command.

.DESCRIPTION
    1. Lists this tree's files with Windows git (tracked + untracked, .gitignore respected, so
       uncommitted edits are included) and copies them with rsync into the WSL filesystem,
       ~/groomlab-src/<tree folder name> (building on /mnt/c is several times slower).
    2. Runs tools/dev/linux_build.sh there: configures the Linux preset, builds, runs ctest.
    3. With -Dist (the default), stages the artifacts (groomlab-cli, libgroomlab.so + SDK, the
       Python module, procedurals when their SDKs were found, the TBB runtime) into dist/linux/.

    Needs WSL 2 with a distribution that has gcc/g++, git, rsync, curl and python3 (Ubuntu 22.04
    has them all). CMake >= 3.25, Ninja, TBB and the Python headers come from the system when
    installed (sudo apt install -y ninja-build python3-dev libtbb-dev; pip install --user cmake),
    else from ~/opt: -Bootstrap runs tools/dev/linux_bootstrap.sh, which puts them there without root.

.EXAMPLE
    powershell -File tools/dev/linux_build.ps1                 # sync, build linux-core, quick tests, dist/linux
    powershell -File tools/dev/linux_build.ps1 -Tests all      # every test
    powershell -File tools/dev/linux_build.ps1 -Tests none -Clean
    powershell -File tools/dev/linux_build.ps1 -Bootstrap      # first time, without sudo in WSL
#>
param(
    [string]$Preset = "linux-core",
    # ctest label (quick, core, io, python...), "all", or "none".
    [string]$Tests = "quick",
    [string]$Distro = "",
    [int]$Jobs = 0,
    [switch]$Clean,
    # Build every target that can be built (ninja -k 0) and list all errors.
    [switch]$KeepGoing,
    [switch]$Bootstrap,
    [switch]$NoDist
)
$ErrorActionPreference = "Stop"

$repo = (git -C $PSScriptRoot rev-parse --show-toplevel).Trim()
if (-not $repo) { throw "Not in a git tree: $PSScriptRoot" }
$name = Split-Path $repo -Leaf

$wsl = @()
if ($Distro) { $wsl += @("-d", $Distro) }

function ConvertTo-WslPath([string]$path) {
    $p = (& wsl.exe @wsl -e wslpath -a ($path -replace '\\', '/'))
    if ($LASTEXITCODE -ne 0 -or -not $p) { throw "wslpath failed for $path (is WSL installed?)" }
    return $p.Trim()
}

# The files to copy: what git would commit (tracked, and untracked but not ignored), never the
# nested worktrees under .claude/.
$list = Join-Path $repo "build/linux-sync-files.txt"
New-Item -ItemType Directory -Force (Split-Path $list) | Out-Null
$files = git -C $repo -c core.quotepath=false ls-files -co --exclude-standard |
    Where-Object { $_ -and -not $_.EndsWith("/") -and -not $_.StartsWith(".claude/") }
[IO.File]::WriteAllLines($list, [string[]]$files, (New-Object Text.UTF8Encoding($false)))

$wslRepo = ConvertTo-WslPath $repo
$wslList = ConvertTo-WslPath $list
$home_ = (& wsl.exe @wsl -e sh -c 'printf %s "$HOME"').Trim()
$dest = "$home_/groomlab-src/$name"

$shArgs = @("--sync-from", $wslRepo, "--files", $wslList, "--dest", $dest, "--preset", $Preset, "--tests", $Tests)
if ($Jobs -gt 0) { $shArgs += @("--jobs", "$Jobs") }
if ($Clean) { $shArgs += "--clean" }
if ($KeepGoing) { $shArgs += "--keep-going" }
if ($Bootstrap) { $shArgs += "--bootstrap" }
if (-not $NoDist) {
    $dist = Join-Path $repo "dist/linux"
    New-Item -ItemType Directory -Force $dist | Out-Null
    $shArgs += @("--dist", (ConvertTo-WslPath $dist))
}

Write-Host "WSL: $dest ($Preset, tests: $Tests)"
# Build output on stderr (compiler warnings, ctest) must not stop the script when it is redirected.
$ErrorActionPreference = "Continue"
& wsl.exe @wsl -e bash -l "$wslRepo/tools/dev/linux_build.sh" @shArgs
$code = $LASTEXITCODE
if ($code -ne 0) { Write-Host "linux_build.sh exited with $code" -ForegroundColor Red }
exit $code
