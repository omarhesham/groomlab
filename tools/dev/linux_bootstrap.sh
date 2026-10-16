#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# User-space build tools for a Linux (or WSL) machine where you cannot run sudo: everything goes
# under ~/opt, nothing touches the system. tools/dev/linux_build.sh picks them up when the system
# has no CMake >= 3.24, no Ninja, or no TBB / Python headers.
#
#   ~/opt/cmake-<v>-linux-x86_64/   CMake release binaries (github.com/Kitware/CMake/releases, SHA-256 checked)
#   ~/opt/ninja-<v>/ninja           Ninja release binary (github.com/ninja-build/ninja/releases, SHA-256 pinned)
#   ~/opt/sysroot/usr/...           Ubuntu's own -dev packages unpacked (apt-get download: fetched from the
#                                   distribution's archive and hash-checked by apt; dpkg -x, no install):
#                                   oneTBB (libtbb-dev), Python headers (python3.X-dev), GL/EGL headers
#
# With root, the system packages do the same (and then this script is not needed):
#   sudo apt install -y ninja-build python3-dev libtbb-dev && python3 -m pip install --user "cmake>=3.24"
set -euo pipefail

CMAKE_VERSION=3.30.5
NINJA_VERSION=1.12.1
NINJA_SHA256=6f98805688d19672bd699fbbfa2c2cf0fc054ac3df1f0e6a47664d963d530255  # ninja-linux.zip of 1.12.1
OPT="${GROOMLAB_OPT:-$HOME/opt}"

mkdir -p "$OPT/dl"
cd "$OPT/dl"

if [ ! -x "$OPT/cmake-$CMAKE_VERSION-linux-x86_64/bin/cmake" ]; then
    echo "== CMake $CMAKE_VERSION"
    curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-linux-x86_64.tar.gz"
    curl -fsSLO "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-SHA-256.txt"
    grep "cmake-$CMAKE_VERSION-linux-x86_64.tar.gz" "cmake-$CMAKE_VERSION-SHA-256.txt" | sha256sum -c -
    tar -xzf "cmake-$CMAKE_VERSION-linux-x86_64.tar.gz" -C "$OPT"
fi

if [ ! -x "$OPT/ninja-$NINJA_VERSION/ninja" ]; then
    echo "== Ninja $NINJA_VERSION"
    curl -fsSLO "https://github.com/ninja-build/ninja/releases/download/v$NINJA_VERSION/ninja-linux.zip"
    echo "$NINJA_SHA256  ninja-linux.zip" | sha256sum -c -
    mkdir -p "$OPT/ninja-$NINJA_VERSION"
    # No unzip on a minimal Ubuntu: Python's zipfile.
    python3 -I -c "import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])" \
        ninja-linux.zip "$OPT/ninja-$NINJA_VERSION"
    chmod +x "$OPT/ninja-$NINJA_VERSION/ninja"
fi

# The -dev packages, only where apt-get is (Debian / Ubuntu). The Python headers match the system
# python3's minor version (any patch release of it has the same ABI); when the archive no longer
# has the exact version apt's lists name (stale lists, no sudo for apt update), the release
# pocket's is taken.
if command -v apt-get >/dev/null && command -v dpkg >/dev/null; then
    PYV=$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])')
    mkdir -p "$OPT/debs" && cd "$OPT/debs"
    pkgs=(libtbb-dev libtbb12 libtbbmalloc2 libgl-dev libegl-dev libglvnd-dev libgles-dev libx11-dev x11proto-dev
          libxcb1-dev libxau-dev libxdmcp-dev)
    apt-get download "${pkgs[@]}"
    if ! apt-get download "python$PYV-dev" "libpython$PYV-dev" 2>/dev/null; then
        for v in $(apt-cache madison "python$PYV-dev" | awk '{print $3}'); do
            if apt-get download "python$PYV-dev=$v" "libpython$PYV-dev=$v"; then break; fi
        done
    fi
    mkdir -p "$OPT/sysroot"
    for d in *.deb; do dpkg -x "$d" "$OPT/sysroot"; done
fi

"$OPT/cmake-$CMAKE_VERSION-linux-x86_64/bin/cmake" --version | head -1
echo "ninja $("$OPT/ninja-$NINJA_VERSION/ninja" --version)"
[ -d "$OPT/sysroot/usr/include" ] && echo "sysroot: $OPT/sysroot ($(ls "$OPT/sysroot/usr/include" | tr '\n' ' '))"
