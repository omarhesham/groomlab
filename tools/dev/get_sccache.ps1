# SPDX-License-Identifier: Apache-2.0
<#
.SYNOPSIS
Fetches the pinned sccache release into %USERPROFILE%/groomlab-deps/sccache, where the build's
GROOMLAB_COMPILER_CACHE=AUTO finds it (cmake/GroomLabBuildSpeed.cmake).

.DESCRIPTION
    powershell -NoProfile -ExecutionPolicy Bypass -File tools/dev/get_sccache.ps1

A build tool only: nothing of it ships. The download is checked against the release's SHA-256 before
anything is unpacked. (winget install Mozilla.sccache works as well: sccache on PATH is found too.)
#>
$ErrorActionPreference = "Stop"
$version = "0.18.0"
$name = "sccache-v$version-x86_64-pc-windows-msvc"
$sha256 = "8965C74D5E8A225244F741E18AD2F3F504F48228DC1BAC948FC22761A348363D"
$url = "https://github.com/mozilla/sccache/releases/download/v$version/$name.zip"

$root = Join-Path $env:USERPROFILE "groomlab-deps\sccache"
$exe = Join-Path $root "$name\sccache.exe"
if (Test-Path $exe) {
    Write-Output "sccache ${version}: $exe"
    exit 0
}
New-Item -ItemType Directory -Force $root | Out-Null
$zip = Join-Path $root "$name.$PID.zip"
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
$ProgressPreference = "SilentlyContinue"
Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile $zip
$actual = (Get-FileHash -Algorithm SHA256 $zip).Hash
if ($actual -ne $sha256) {
    Remove-Item -Force $zip
    throw "sccache download $url has SHA-256 $actual, expected $sha256"
}
$tmp = Join-Path $root "$name.tmp.$PID"
Expand-Archive -Path $zip -DestinationPath $tmp -Force
Remove-Item -Force $zip
$unpacked = Join-Path $tmp $name
if (-not (Test-Path (Join-Path $unpacked "sccache.exe"))) { throw "sccache.exe not found in $url" }
if (Test-Path (Join-Path $root $name)) {
    Remove-Item -Recurse -Force $tmp  # another run unpacked it first
} else {
    Move-Item $unpacked (Join-Path $root $name)
    Remove-Item -Recurse -Force $tmp
}
Write-Output "sccache ${version}: $exe"
