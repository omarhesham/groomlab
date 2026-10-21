# SPDX-License-Identifier: Apache-2.0
# Finds the Maya devkit headers and libraries under MAYA_LOCATION.
#
# macOS: MAYA_LOCATION is the devkit (headers, maybe lib/); the libraries are found there or in the
# Maya install's Maya.app/Contents/MacOS (GROOMLAB_MAYA_APP). Found nowhere, the plugin is linked
# with -undefined dynamic_lookup: Maya's symbols resolve when Maya loads it.
#
# Defines:
#   Maya_FOUND, Maya_INCLUDE_DIR, Maya_LIBRARY_DIR, Maya_API_VERSION
#   Maya::Maya   interface target with includes, platform defines and OpenMaya libs
#   maya_add_plugin(<target> <sources...>)   builds a correctly named plugin library

find_path(Maya_INCLUDE_DIR maya/MFnPlugin.h
    HINTS "${MAYA_LOCATION}"
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)

if(APPLE)
    find_path(Maya_LIBRARY_DIR NAMES libOpenMaya.dylib
        HINTS "${MAYA_LOCATION}" "${GROOMLAB_MAYA_APP}"
        PATH_SUFFIXES lib MacOS
        NO_DEFAULT_PATH)
else()
    find_path(Maya_LIBRARY_DIR NAMES OpenMaya.lib libOpenMaya.so
        HINTS "${MAYA_LOCATION}"
        PATH_SUFFIXES lib
        NO_DEFAULT_PATH)
endif()

if(Maya_INCLUDE_DIR AND EXISTS "${Maya_INCLUDE_DIR}/maya/MTypes.h")
    file(STRINGS "${Maya_INCLUDE_DIR}/maya/MTypes.h" _maya_api_line
         REGEX "#define MAYA_API_VERSION [0-9]+")
    string(REGEX MATCH "[0-9]+" Maya_API_VERSION "${_maya_api_line}")
endif()

include(FindPackageHandleStandardArgs)
if(APPLE)
    find_package_handle_standard_args(Maya
        REQUIRED_VARS Maya_INCLUDE_DIR
        VERSION_VAR Maya_API_VERSION)
else()
    find_package_handle_standard_args(Maya
        REQUIRED_VARS Maya_INCLUDE_DIR Maya_LIBRARY_DIR
        VERSION_VAR Maya_API_VERSION)
endif()

if(Maya_FOUND AND NOT TARGET Maya::Maya)
    add_library(Maya::Maya INTERFACE IMPORTED)
    target_include_directories(Maya::Maya INTERFACE "${Maya_INCLUDE_DIR}")
    target_compile_definitions(Maya::Maya INTERFACE REQUIRE_IOSTREAM _BOOL)
    if(WIN32)
        target_compile_definitions(Maya::Maya INTERFACE NT_PLUGIN _WIN32 NOMINMAX)
    elseif(APPLE)
        # The devkit's macOS define (its cmake/pluginEntry.cmake; the headers tell macOS by __APPLE__).
        target_compile_definitions(Maya::Maya INTERFACE MAC_PLUGIN)
    else()
        target_compile_definitions(Maya::Maya INTERFACE LINUX)
    endif()

    if(APPLE AND NOT Maya_LIBRARY_DIR)
        message(STATUS "Maya libraries not found (devkit lib/, GROOMLAB_MAYA_APP/MacOS): linking with -undefined dynamic_lookup")
        target_link_options(Maya::Maya INTERFACE "LINKER:-undefined,dynamic_lookup")
    else()
        foreach(_lib Foundation OpenMaya OpenMayaAnim OpenMayaFX OpenMayaRender OpenMayaUI)
            find_library(Maya_${_lib}_LIBRARY ${_lib} HINTS "${Maya_LIBRARY_DIR}" NO_DEFAULT_PATH)
            target_link_libraries(Maya::Maya INTERFACE "${Maya_${_lib}_LIBRARY}")
        endforeach()
    endif()
endif()

function(maya_add_plugin target)
    if(APPLE)
        # A loadable bundle (MH_BUNDLE, -bundle) named .bundle, as the devkit builds Maya plugins.
        add_library(${target} MODULE ${ARGN})
    else()
        add_library(${target} SHARED ${ARGN})
    endif()
    target_link_libraries(${target} PRIVATE Maya::Maya)
    set_target_properties(${target} PROPERTIES PREFIX "")
    if(WIN32)
        set_target_properties(${target} PROPERTIES SUFFIX ".mll")
    elseif(APPLE)
        set_target_properties(${target} PROPERTIES SUFFIX ".bundle")
    endif()
endfunction()
