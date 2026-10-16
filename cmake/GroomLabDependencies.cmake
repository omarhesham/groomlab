# SPDX-License-Identifier: Apache-2.0
# Third-party dependencies fetched at configure time.
include(FetchContent)

# The fetched sources, shared by every build folder on the machine: each dependency's sources are
# fetched once into GROOMLAB_FETCH_CACHE (a folder per name and version) and every later configure,
# in any tree, builds from that folder instead of fetching again. Read-only for the builds (each
# build folder still builds them under its own _deps). Empty: every build folder fetches its own.
if(WIN32)
    set(_groomlab_fetch_default "$ENV{USERPROFILE}/groomlab-deps/fetch")
elseif(DEFINED ENV{GROOMLAB_DEPS} AND NOT "$ENV{GROOMLAB_DEPS}" STREQUAL "")
    set(_groomlab_fetch_default "$ENV{GROOMLAB_DEPS}/fetch")
elseif(DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
    set(_groomlab_fetch_default "$ENV{XDG_CACHE_HOME}/groomlab-deps/fetch")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
    set(_groomlab_fetch_default "$ENV{HOME}/.cache/groomlab-deps/fetch")
else()
    set(_groomlab_fetch_default "")
endif()
set(GROOMLAB_FETCH_CACHE "${_groomlab_fetch_default}" CACHE PATH
    "Shared folder of fetched dependency sources (empty: fetch into each build folder)")

# groomlab_cached_source(<name> <version>): before FetchContent_MakeAvailable(<name>), point
# FetchContent at the shared copy of these sources when there is one.
macro(groomlab_cached_source name version)
    string(TOUPPER "${name}" _groomlab_upper)
    set(_groomlab_cached_${name} "")
    if(GROOMLAB_FETCH_CACHE AND NOT FETCHCONTENT_SOURCE_DIR_${_groomlab_upper})
        set(_groomlab_cached_${name} "${GROOMLAB_FETCH_CACHE}/${name}-${version}")
        if(EXISTS "${_groomlab_cached_${name}}/.groomlab-complete")
            set(FETCHCONTENT_SOURCE_DIR_${_groomlab_upper} "${_groomlab_cached_${name}}")
            set(_groomlab_cached_${name} "")  # nothing to store
        endif()
    endif()
endmacro()

# groomlab_store_source(<name>): after FetchContent_MakeAvailable(<name>), copy sources just fetched
# into the shared folder (whole, then renamed into place, so a half-written copy is never used).
function(groomlab_store_source name)
    set(dest "${_groomlab_cached_${name}}")
    string(TOLOWER "${name}" lower)
    set(src "${${lower}_SOURCE_DIR}")
    if(NOT dest OR NOT src OR EXISTS "${dest}")
        return()
    endif()
    string(RANDOM LENGTH 8 tag)
    set(tmp "${dest}.tmp-${tag}")
    file(MAKE_DIRECTORY "${tmp}")
    file(COPY "${src}/" DESTINATION "${tmp}" PATTERN ".git" EXCLUDE)
    file(TOUCH "${tmp}/.groomlab-complete")
    file(RENAME "${tmp}" "${dest}" RESULT renamed)
    if(NOT renamed STREQUAL "0")
        file(REMOVE_RECURSE "${tmp}")  # another configure stored it first
    else()
        message(STATUS "Stored ${name}'s sources in ${dest}")
    endif()
endfunction()

# pybind11 2.13.x is the last series supporting Python 3.7 (Maya 2022's mayapy).
macro(groomlab_fetch_pybind11)
    FetchContent_Declare(pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG        v2.13.6
        GIT_SHALLOW    TRUE)
    groomlab_cached_source(pybind11 v2.13.6)
    FetchContent_MakeAvailable(pybind11)
    groomlab_store_source(pybind11)
endmacro()

# Header-only JSON for graph files; used privately by the core.
macro(groomlab_fetch_json)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    groomlab_cached_source(nlohmann_json 3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
    groomlab_store_source(nlohmann_json)
endmacro()

# Alembic 1.8 + Imath 3.1, built static and without tools, tests or HDF5.
# Linked privately so their symbols stay inside our binaries (Maya 2022 ships
# its own, older Alembic; Imath 3 lives in its own Imath_3_1 namespace).
#
# Alembic's install(EXPORT) needs Imath's install rules, so both keep them but
# land in the "ThirdParty" component. Install the plugin with
# `cmake --install <build> --component GroomLab`.
macro(groomlab_fetch_alembic)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(IMATH_INSTALL ON CACHE BOOL "" FORCE)
    set(IMATH_INSTALL_PKG_CONFIG OFF CACHE BOOL "" FORCE)
    set(_groomlab_saved_component "${CMAKE_INSTALL_DEFAULT_COMPONENT_NAME}")
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME ThirdParty)
    FetchContent_Declare(Imath
        GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/Imath.git
        GIT_TAG        v3.1.12
        GIT_SHALLOW    TRUE
        SYSTEM
        OVERRIDE_FIND_PACKAGE)

    set(ALEMBIC_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(ALEMBIC_BUILD_LIBS ON CACHE BOOL "" FORCE)
    set(ALEMBIC_DEBUG_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
    set(USE_BINARIES OFF CACHE BOOL "" FORCE)
    set(USE_TESTS OFF CACHE BOOL "" FORCE)
    set(USE_HDF5 OFF CACHE BOOL "" FORCE)
    set(USE_PYALEMBIC OFF CACHE BOOL "" FORCE)
    set(USE_ARNOLD OFF CACHE BOOL "" FORCE)
    set(USE_MAYA OFF CACHE BOOL "" FORCE)
    set(USE_PRMAN OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(Alembic
        GIT_REPOSITORY https://github.com/alembic/alembic.git
        GIT_TAG        1.8.8
        GIT_SHALLOW    TRUE
        SYSTEM)

    groomlab_cached_source(Imath v3.1.12)
    groomlab_cached_source(Alembic 1.8.8)
    FetchContent_MakeAvailable(Imath Alembic)
    groomlab_store_source(Imath)
    groomlab_store_source(Alembic)
    set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "${_groomlab_saved_component}")
endmacro()

macro(groomlab_fetch_googletest)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE)
    # Match the dynamic CRT that Maya and the rest of the build use.
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    groomlab_cached_source(googletest v1.15.2)
    FetchContent_MakeAvailable(googletest)
    groomlab_store_source(googletest)
endmacro()
