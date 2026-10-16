# SPDX-License-Identifier: Apache-2.0
# The GroomLab SDK as a CMake package (installed as <sdk>/cmake/GroomLabSDKConfig.cmake):
#
#   find_package(GroomLabSDK 1.0 CONFIG REQUIRED)   # CMAKE_PREFIX_PATH=<sdk>, or GroomLabSDK_DIR=<sdk>/cmake
#   target_link_libraries(my_bridge PRIVATE GroomLab::groomlab)
#
# GroomLab::groomlab: the C API library (groomlab.dll + groomlab.lib, libgroomlab.so) with the
# include directory holding groomlab/groomlab_c.h and groomlab/groomlab.hpp.
# GROOMLAB_SDK_RUNTIME_DLLS (Windows): every DLL in <sdk>/bin, which a program needs beside it:
# groomlab.dll alone (it brings no TBB; its parallel loops run on its own threads).
# The package version is the C API's (major.minor): a newer minor serves an older request.

get_filename_component(_groomlab_sdk_root "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT TARGET GroomLab::groomlab)
    add_library(GroomLab::groomlab SHARED IMPORTED)
    set_target_properties(GroomLab::groomlab PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_groomlab_sdk_root}/include")
    if(WIN32)
        set_target_properties(GroomLab::groomlab PROPERTIES
            IMPORTED_LOCATION "${_groomlab_sdk_root}/bin/groomlab.dll"
            IMPORTED_IMPLIB "${_groomlab_sdk_root}/lib/groomlab.lib")
    elseif(APPLE)
        set_target_properties(GroomLab::groomlab PROPERTIES
            IMPORTED_LOCATION "${_groomlab_sdk_root}/lib/libgroomlab.dylib")
    else()
        set_target_properties(GroomLab::groomlab PROPERTIES
            IMPORTED_LOCATION "${_groomlab_sdk_root}/lib/libgroomlab.so")
    endif()
endif()

set(GROOMLAB_SDK_RUNTIME_DLLS "")
if(WIN32)
    file(GLOB GROOMLAB_SDK_RUNTIME_DLLS "${_groomlab_sdk_root}/bin/*.dll")
endif()
set(GroomLabSDK_INCLUDE_DIR "${_groomlab_sdk_root}/include")
unset(_groomlab_sdk_root)
