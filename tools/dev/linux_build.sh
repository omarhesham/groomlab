#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Configure, build and test GroomLab on Linux (natively, or in WSL through tools/dev/linux_build.ps1),
# and optionally stage the farm/pipeline artifacts in a dist folder.
#
#   tools/dev/linux_build.sh [--preset linux-core] [--tests quick|all|none|<ctest -L regex>]
#                            [--jobs N] [--dist DIR] [--clean] [--bootstrap]
#
# From Windows, tools/dev/linux_build.ps1 runs this with --sync-from: the tree is first copied into
# the Linux filesystem (building on /mnt/c is several times slower), then the copy's own script
# runs with the remaining options.
#
#   --sync-from DIR --files LIST --dest DIR   rsync the files named in LIST (one path per line, relative
#                                             to DIR: `git ls-files -co --exclude-standard`) from DIR
#                                             into DEST, delete what LIST no longer names (build/ kept),
#                                             then continue in DEST
#
# Tools: cmake (>= 3.25, CMakePresets.json version 6) and ninja from PATH, else ~/.local/bin (pip's
# cmake), else what tools/dev/linux_bootstrap.sh put under ~/opt. TBB and the Python headers: the
# system's, else ~/opt/sysroot (linux_bootstrap.sh); without Python headers the bindings are left out.
set -euo pipefail

PRESET=linux-core
TESTS=quick
JOBS=$(nproc)
DIST=""
CLEAN=0
KEEP_GOING=0
BOOTSTRAP=0
SYNC_FROM=""
FILES=""
DEST=""
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OPT="${GROOMLAB_OPT:-$HOME/opt}"

args=("$@")
while [ $# -gt 0 ]; do
    case "$1" in
        --preset) PRESET="$2"; shift 2 ;;
        --tests) TESTS="$2"; shift 2 ;;
        --jobs) JOBS="$2"; shift 2 ;;
        --dist) DIST="$2"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        --keep-going) KEEP_GOING=1; shift ;;
        --bootstrap) BOOTSTRAP=1; shift ;;
        --sync-from) SYNC_FROM="$2"; shift 2 ;;
        --files) FILES="$2"; shift 2 ;;
        --dest) DEST="$2"; shift 2 ;;
        -h|--help) sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *) echo "linux_build.sh: unknown option $1" >&2; exit 2 ;;
    esac
done

# ---- Sync into the Linux filesystem, then run the copy's script ----------------------------------
if [ -n "$SYNC_FROM" ]; then
    [ -n "$FILES" ] && [ -n "$DEST" ] || { echo "--sync-from needs --files and --dest" >&2; exit 2; }
    mkdir -p "$DEST"
    list=$(mktemp)
    tr -d '\r' < "$FILES" | sed '/^$/d' | sort -u > "$list"
    echo "== Sync $(wc -l < "$list") files: $SYNC_FROM -> $DEST"
    rsync -a --checksum --ignore-missing-args --files-from="$list" "$SYNC_FROM/" "$DEST/"
    # Prune files the list no longer names (deleted or renamed in the source), outside build output.
    have=$(mktemp)
    (cd "$DEST" && find . -path ./build -prune -o -path ./install -prune -o -path ./dist -prune -o \
        -type f -print | sed 's|^\./||' | sort) > "$have"
    comm -23 "$have" "$list" | while IFS= read -r stale; do rm -f "$DEST/$stale"; done
    find "$DEST" -path "$DEST/build" -prune -o -type d -empty -delete 2>/dev/null || true
    rm -f "$list" "$have"
    # Continue in the copy, minus the sync options.
    rest=()
    i=0
    while [ $i -lt ${#args[@]} ]; do
        case "${args[$i]}" in
            --sync-from|--files|--dest) i=$((i + 2)) ;;
            *) rest+=("${args[$i]}"); i=$((i + 1)) ;;
        esac
    done
    exec bash "$DEST/tools/dev/linux_build.sh" "${rest[@]}"
fi

cd "$SRC"
if [ "$BOOTSTRAP" = 1 ]; then
    bash tools/dev/linux_bootstrap.sh
fi

# ---- Tools ----------------------------------------------------------------------------------------
cmake_ok() {  # cmake_ok <exe>: CMake 3.25 or later
    local v
    v=$("$1" --version 2>/dev/null | head -1 | sed -n 's/^cmake version \([0-9]*\)\.\([0-9]*\).*/\1 \2/p')
    [ -n "$v" ] && set -- $v && { [ "$1" -gt 3 ] || { [ "$1" -eq 3 ] && [ "$2" -ge 25 ]; }; }
}
CMAKE=""
for c in "$(command -v cmake || true)" "$HOME/.local/bin/cmake" $(ls -d "$OPT"/cmake-*/bin/cmake 2>/dev/null | sort -V -r); do
    if [ -n "$c" ] && [ -x "$c" ] && cmake_ok "$c"; then CMAKE="$c"; break; fi
done
[ -n "$CMAKE" ] || { echo "No CMake >= 3.25: run tools/dev/linux_bootstrap.sh (or pip install --user cmake)" >&2; exit 1; }
CMAKE_BIN=$(dirname "$CMAKE")
if ! command -v ninja >/dev/null; then
    n=$(ls -d "$OPT"/ninja-*/ninja 2>/dev/null | sort -V -r | head -1 || true)
    [ -n "$n" ] || { echo "No ninja: run tools/dev/linux_bootstrap.sh (or apt install ninja-build)" >&2; exit 1; }
    export PATH="$(dirname "$n"):$PATH"
fi
export PATH="$CMAKE_BIN:$PATH"
echo "== $(cmake --version | head -1), ninja $(ninja --version), $(c++ --version | head -1)"

# ---- Configure ------------------------------------------------------------------------------------
extra=()
SYSROOT="$OPT/sysroot/usr"
PYV=$(python3 -c 'import sys; print("%d.%d" % sys.version_info[:2])' 2>/dev/null || true)
if [ -d "$SYSROOT/lib/x86_64-linux-gnu/cmake/TBB" ] && [ ! -d /usr/lib/x86_64-linux-gnu/cmake/TBB ]; then
    extra+=("-DCMAKE_PREFIX_PATH=$SYSROOT")
fi
if [ -n "$PYV" ] && [ -f "/usr/include/python$PYV/Python.h" ]; then
    extra+=("-DPython_EXECUTABLE=$(command -v python3)")
elif [ -n "$PYV" ] && [ -f "$SYSROOT/include/python$PYV/Python.h" ]; then
    # Debian's python3.X/pyconfig.h includes <x86_64-linux-gnu/python3.X/pyconfig.h>: the sysroot's
    # include folder goes on the C++ include path too.
    extra+=("-DPython_EXECUTABLE=$(command -v python3)" "-DPython_INCLUDE_DIR=$SYSROOT/include/python$PYV"
            "-DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=$SYSROOT/include")
else
    echo "== No Python $PYV headers (apt install python3-dev, or tools/dev/linux_bootstrap.sh): bindings left out"
    extra+=("-DGROOMLAB_BUILD_PYTHON=OFF")
fi

BUILD="$SRC/build/$PRESET"
if [ "$CLEAN" = 1 ]; then rm -rf "$BUILD"; fi
echo "== Configure $PRESET"
cmake --preset "$PRESET" "${extra[@]}"

# ---- Build ----------------------------------------------------------------------------------------
echo "== Build ($JOBS jobs)"
if [ "$KEEP_GOING" = 1 ]; then
    cmake --build --preset "$PRESET" --parallel "$JOBS" -- -k 0
else
    cmake --build --preset "$PRESET" --parallel "$JOBS"
fi

# ---- Test -----------------------------------------------------------------------------------------
status=0
# The performance budgets (test_budgets*.cpp) are set for the reference machine's Windows build,
# where they gate; here they report (a WSL VM's allocator and scheduler time differently). Set
# GROOMLAB_ENFORCE_BUDGETS=1 to gate on a Linux machine sized like the reference.
export GROOMLAB_ENFORCE_BUDGETS="${GROOMLAB_ENFORCE_BUDGETS:-0}"
if [ "$TESTS" != none ]; then
    label=()
    [ "$TESTS" = all ] || label=(-L "$TESTS")
    echo "== ctest ${label[*]:-(all)}"
    ctest --preset "$PRESET" "${label[@]}" -j "$JOBS" --output-junit "$BUILD/ctest-junit.xml" || status=$?
fi

# ---- Stage the artifacts --------------------------------------------------------------------------
if [ -n "$DIST" ]; then
    stage="$BUILD/dist"
    version=$(sed -n 's/^project(GroomLab VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
    rm -rf "$stage"
    # GroomLab/bin (groomlab-cli) and GroomLab/procedurals (when their SDKs were found), the C API
    # SDK (GroomLabSDK), the Python module (python/) and the Blender helper (GroomLabBlender), each
    # binary with libtbb.so.12 beside it where it links TBB. Stripped (the build tree keeps the
    # debug information).
    for component in GroomLab SDK Python Blender; do
        cmake --install "$BUILD" --component "$component" --prefix "$stage" --strip >/dev/null
    done
    # What a render node needs at least: the newest glibc and libstdc++ symbol versions the
    # binaries ask for (a farm's distribution must have them: Ubuntu 22.04's glibc 2.35 here).
    elves=$(find "$stage" -type f \( -name "*.so*" -o -perm -u+x \) -exec sh -c 'head -c 4 "$1" | grep -q "ELF"' _ {} \; -print)
    {
        echo "GroomLab $version, built $(date -u +%Y-%m-%dT%H:%MZ) on $(. /etc/os-release && echo "$PRETTY_NAME"), $(c++ --version | head -1)"
        for f in $elves; do
            glibc=$(objdump -T "$f" | grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)
            glibcxx=$(objdump -T "$f" | grep -o 'GLIBCXX_[0-9.]*' | sort -uV | tail -1)
            echo "${f#"$stage"/}: needs ${glibc:-no glibc} ${glibcxx:+, $glibcxx}"
        done
    } > "$stage/BUILDINFO.txt"
    [ -f "$BUILD/ctest-junit.xml" ] && cp "$BUILD/ctest-junit.xml" "$stage/"
    # One archive keeps the executable bits and the libtbb.so.12 name for copying to Linux machines
    # (a Windows folder does not).
    tarball="groomlab-$version-linux-x86_64.tar.gz"
    tar -C "$stage" -czf "$BUILD/$tarball" .
    mkdir -p "$DIST"
    rm -rf "${DIST:?}"/*
    cp -r "$stage"/. "$DIST"/
    cp "$BUILD/$tarball" "$DIST/"
    echo "== Artifacts: $DIST ($tarball)"
    (cd "$stage" && find . -path ./GroomLabBlender/bin/presets -prune -o -path ./GroomLabBlender/icons -prune -o \
        \( -type f -o -type l \) -print | sort | sed 's|^\./|   |')
    sed 's|^|   |' "$stage/BUILDINFO.txt"
fi
exit $status
