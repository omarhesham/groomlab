# SPDX-License-Identifier: Apache-2.0
# How fast the build builds, never what it builds: the debug information's form, precompiled
# headers and an optional compiler cache. Each leaves the binaries' code, the install and the tests
# as they are (docs/DEVELOPING.md, "Faster builds", has the measurements behind these defaults).

set(_groomlab_vs_generator OFF)
if(CMAKE_GENERATOR MATCHES "Visual Studio")
    set(_groomlab_vs_generator ON)
endif()

# --- Debug information ---------------------------------------------------------------------------
# Ninja: /Z7 (each object carries its own debug information) instead of /Zi (one compiler PDB per
# target, written through mspdbsrv by every cl.exe at once: the PDB contention behind LNK1201 under
# a high -j, and uncacheable by sccache). The linker still writes the full .pdb beside each binary
# (/DEBUG), so debuggers and crash_log's symbolized stacks read the same .pdb as before.
# Visual Studio: /Zi as before, and /MP, so a project compiles its files side by side (MSBuild's -m
# only builds projects side by side).
if(MSVC)
    if(_groomlab_vs_generator)
        add_compile_options($<$<COMPILE_LANGUAGE:C,CXX>:/MP>)
    else()
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")
    endif()
endif()

# --- Compiler cache (opt-in) ----------------------------------------------------------------------
# sccache in front of cl.exe, for the Ninja presets (Visual Studio ignores launchers). OFF by
# default: with MSVC sccache preprocesses every file before it looks it up, so a miss (every file an
# edit touches) costs about a third more than compiling, and a hit still costs the preprocessing;
# with Ninja and the precompiled headers a whole build takes about a minute and a half, so the cache
# pays off only for builds that are mostly hits (a fresh worktree, a branch switched back and
# forth). AUTO uses sccache when found (on PATH: winget install Mozilla.sccache; or where
# tools/dev/get_sccache.ps1 puts the pinned release, %USERPROFILE%/groomlab-deps/sccache); a path
# names one. It turns the precompiled headers off by default: sccache cannot cache a compile that
# uses one.
#
# Every source tree (the main checkout, each worktree) has a cache and an sccache server of its own
# (build/.sccache, a port from the tree's path), its root stripped from the cache keys
# (SCCACHE_BASEDIRS), so a worktree whose cache was seeded from the main checkout's
# hits for every file the two trees have alike, in build folders of the same name.
# tools/dev/sccache_launcher.py runs each compile through it and maps the include list a hit
# replays (cl /showIncludes, written by the tree that compiled it) to this tree's files, so Ninja's
# dependencies stay this tree's own.
set(GROOMLAB_COMPILER_CACHE "OFF" CACHE STRING
    "Compiler cache: OFF, AUTO (sccache if found), or the path of an sccache executable")
# Where a worktree's cache was seeded from (the main checkout), when it was.
set(GROOMLAB_SEED_ROOT "" CACHE PATH "The source tree whose compiler cache seeded this one's")
set(GROOMLAB_COMPILER_CACHE_USED OFF)
if(NOT GROOMLAB_COMPILER_CACHE STREQUAL "OFF" AND NOT _groomlab_vs_generator AND MSVC)
    set(_sccache "")
    if(GROOMLAB_COMPILER_CACHE STREQUAL "AUTO" OR GROOMLAB_COMPILER_CACHE STREQUAL "ON")
        file(GLOB _pinned LIST_DIRECTORIES true "$ENV{USERPROFILE}/groomlab-deps/sccache/*")
        find_program(GROOMLAB_SCCACHE_EXECUTABLE sccache HINTS ${_pinned}
            DOC "sccache for GROOMLAB_COMPILER_CACHE=AUTO")
        if(GROOMLAB_SCCACHE_EXECUTABLE)
            set(_sccache "${GROOMLAB_SCCACHE_EXECUTABLE}")
        endif()
    else()
        set(_sccache "${GROOMLAB_COMPILER_CACHE}")
    endif()
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(_sccache AND EXISTS "${_sccache}" AND Python3_Interpreter_FOUND)
        file(TO_NATIVE_PATH "${PROJECT_SOURCE_DIR}" _root)
        file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}" _build)
        set(_launcher "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/dev/sccache_launcher.py"
            "--sccache=${_sccache}" "--root=${_root}" "--build=${_build}")
        if(GROOMLAB_SEED_ROOT)
            file(TO_NATIVE_PATH "${GROOMLAB_SEED_ROOT}" _seed)
            list(APPEND _launcher "--seed-root=${_seed}")
        endif()
        if(CMAKE_CL_SHOWINCLUDES_PREFIX)
            list(APPEND _launcher "--includes-prefix=${CMAKE_CL_SHOWINCLUDES_PREFIX}")
        endif()
        list(APPEND _launcher "--")
        set(CMAKE_C_COMPILER_LAUNCHER ${_launcher})
        set(CMAKE_CXX_COMPILER_LAUNCHER ${_launcher})
        set(GROOMLAB_COMPILER_CACHE_USED ON)
        message(STATUS "Compiler cache: ${_sccache}")
    else()
        message(WARNING "GROOMLAB_COMPILER_CACHE=${GROOMLAB_COMPILER_CACHE}: no sccache or no Python 3 found, "
                        "building without a cache (tools/dev/get_sccache.ps1 fetches sccache)")
    endif()
endif()

# --- Precompiled headers --------------------------------------------------------------------------
# The standard library and the third-party headers a target's files include most (nlohmann/json,
# GoogleTest, pybind11, the Maya API, Qt), parsed once per target. Never GroomLab's own headers:
# they change all the time, and a PCH holding one rebuilds the whole target on every edit.
# MSVC only (the Linux builds are left as they were), and off under the compiler cache.
if(MSVC AND NOT GROOMLAB_COMPILER_CACHE_USED)
    set(_groomlab_pch_default ON)
else()
    set(_groomlab_pch_default OFF)
endif()
option(GROOMLAB_PCH "Precompile the standard and third-party headers (MSVC)" ${_groomlab_pch_default})

set(GROOMLAB_PCH_STD
    <algorithm> <array> <atomic> <chrono> <cmath> <cstddef> <cstdint> <cstdio> <cstring> <filesystem>
    <fstream> <functional> <limits> <map> <memory> <mutex> <optional> <set> <sstream> <stdexcept>
    <string> <unordered_map> <utility> <vector>)

# groomlab_precompile(<target> [BEFORE_STD headers...] [headers...]): the standard set and the
# headers given; BEFORE_STD ones come first (Python.h, through pybind11, before any standard header).
function(groomlab_precompile target)
    if(NOT GROOMLAB_PCH)
        return()
    endif()
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "BEFORE_STD")
    target_precompile_headers(${target} PRIVATE ${arg_BEFORE_STD} ${GROOMLAB_PCH_STD} ${arg_UNPARSED_ARGUMENTS})
endfunction()

# --- Paths compiled into binaries -----------------------------------------------------------------
# groomlab_path_literal(<var> <path>): a C++ expression for a path of this source tree, for a
# compile definition (GROOMLAB_TEST_DATA, GROOMLAB_SHIPPED_PRESETS). It carries the tree's own
# token beside the path, so a compiler cache that strips the tree's root from its keys never
# hands one tree's object, and the other tree's path in it, to another.
string(MD5 _groomlab_tree_id "${PROJECT_SOURCE_DIR}")
string(SUBSTRING "${_groomlab_tree_id}" 0 12 GROOMLAB_TREE_ID)
function(groomlab_path_literal var path)
    set(${var} "((void)\"tree ${GROOMLAB_TREE_ID}\", \"${path}\")" PARENT_SCOPE)
endfunction()
