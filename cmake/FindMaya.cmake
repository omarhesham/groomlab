# SPDX-License-Identifier: Apache-2.0
# Finds the Maya devkit headers and libraries under MAYA_LOCATION.
#
# Defines:
#   Maya_FOUND, Maya_INCLUDE_DIR, Maya_LIBRARY_DIR, Maya_API_VERSION
#   Maya::Maya   interface target with includes, platform defines and OpenMaya libs
#   maya_add_plugin(<target> <sources...>)   builds a correctly named plugin library

find_path(Maya_INCLUDE_DIR maya/MFnPlugin.h
    HINTS "${MAYA_LOCATION}"
    PATH_SUFFIXES include
    NO_DEFAULT_PATH)

find_path(Maya_LIBRARY_DIR NAMES OpenMaya.lib libOpenMaya.so
    HINTS "${MAYA_LOCATION}"
    PATH_SUFFIXES lib
    NO_DEFAULT_PATH)

if(Maya_INCLUDE_DIR AND EXISTS "${Maya_INCLUDE_DIR}/maya/MTypes.h")
    file(STRINGS "${Maya_INCLUDE_DIR}/maya/MTypes.h" _maya_api_line
         REGEX "#define MAYA_API_VERSION [0-9]+")
    string(REGEX MATCH "[0-9]+" Maya_API_VERSION "${_maya_api_line}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Maya
    REQUIRED_VARS Maya_INCLUDE_DIR Maya_LIBRARY_DIR
    VERSION_VAR Maya_API_VERSION)

if(Maya_FOUND AND NOT TARGET Maya::Maya)
    add_library(Maya::Maya INTERFACE IMPORTED)
    target_include_directories(Maya::Maya INTERFACE "${Maya_INCLUDE_DIR}")
    target_compile_definitions(Maya::Maya INTERFACE REQUIRE_IOSTREAM _BOOL)
    if(WIN32)
        target_compile_definitions(Maya::Maya INTERFACE NT_PLUGIN _WIN32 NOMINMAX)
    else()
        target_compile_definitions(Maya::Maya INTERFACE LINUX)
    endif()

    foreach(_lib Foundation OpenMaya OpenMayaAnim OpenMayaFX OpenMayaRender OpenMayaUI)
        find_library(Maya_${_lib}_LIBRARY ${_lib} HINTS "${Maya_LIBRARY_DIR}" NO_DEFAULT_PATH)
        target_link_libraries(Maya::Maya INTERFACE "${Maya_${_lib}_LIBRARY}")
    endforeach()
endif()

function(maya_add_plugin target)
    add_library(${target} SHARED ${ARGN})
    target_link_libraries(${target} PRIVATE Maya::Maya)
    set_target_properties(${target} PROPERTIES PREFIX "")
    if(WIN32)
        set_target_properties(${target} PROPERTIES SUFFIX ".mll")
    elseif(APPLE)
        set_target_properties(${target} PROPERTIES SUFFIX ".bundle")
    endif()
endfunction()
