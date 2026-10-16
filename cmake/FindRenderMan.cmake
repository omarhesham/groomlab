# SPDX-License-Identifier: Apache-2.0
# Finds the RenderMan Pro Server SDK (headers and libprman) under GROOMLAB_RMANTREE, for the
# RenderMan procedural (hosts/renderman). Nothing of Pixar's is copied into the build or the
# install: the procedural links libprman, which prman has loaded when it loads the procedural.
#
# Defines:
#   RenderMan_FOUND, RenderMan_VERSION, RenderMan_INCLUDE_DIR, RenderMan_PRMAN (prman executable)
#   RenderMan::RenderMan   interface target: the headers and libprman

find_path(RenderMan_INCLUDE_DIR ri.h
    HINTS "${GROOMLAB_RMANTREE}"
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)
find_library(RenderMan_PRMAN_LIBRARY NAMES libprman prman HINTS "${GROOMLAB_RMANTREE}" PATH_SUFFIXES lib NO_DEFAULT_PATH)
find_program(RenderMan_PRMAN NAMES prman HINTS "${GROOMLAB_RMANTREE}" PATH_SUFFIXES bin NO_DEFAULT_PATH)

if(RenderMan_INCLUDE_DIR AND EXISTS "${RenderMan_INCLUDE_DIR}/prmanapi.h")
    file(STRINGS "${RenderMan_INCLUDE_DIR}/prmanapi.h" _rman_ver REGEX "#define _PRMANAPI_VERSION_(MAJOR|MINOR)_ +[0-9]+")
    string(REGEX REPLACE ".*VERSION_MAJOR_ +([0-9]+).*" "\\1" _major "${_rman_ver}")
    string(REGEX REPLACE ".*VERSION_MINOR_ +([0-9]+).*" "\\1" _minor "${_rman_ver}")
    set(RenderMan_VERSION "${_major}.${_minor}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(RenderMan
    REQUIRED_VARS RenderMan_INCLUDE_DIR RenderMan_PRMAN_LIBRARY
    VERSION_VAR RenderMan_VERSION)

if(RenderMan_FOUND AND NOT TARGET RenderMan::RenderMan)
    add_library(RenderMan::RenderMan INTERFACE IMPORTED)
    target_include_directories(RenderMan::RenderMan INTERFACE "${RenderMan_INCLUDE_DIR}")
    target_link_libraries(RenderMan::RenderMan INTERFACE "${RenderMan_PRMAN_LIBRARY}")
endif()
