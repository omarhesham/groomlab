# SPDX-License-Identifier: MIT
# GroomLab SDK samples (MIT, LICENSE.txt). Two ways to build them:
#   - From an installed SDK:  cmake -S <sdk>/samples -B build  &&  cmake --build build --config Release
#     (finds the SDK this folder sits in, or the one CMAKE_PREFIX_PATH / GroomLabSDK_DIR names).
#   - Inside the GroomLab build: engine/capi adds this folder with GroomLab::groomlab already defined.
cmake_minimum_required(VERSION 3.20...3.31)
project(GroomLabSamples LANGUAGES C CXX)

if(NOT TARGET GroomLab::groomlab)
    # GroomLab::groomlab: the C API library and its headers. GROOMLAB_SDK_RUNTIME_DLLS: the DLLs a
    # program needs beside it on Windows (groomlab.dll).
    if(NOT GroomLabSDK_DIR AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../cmake/GroomLabSDKConfig.cmake")
        set(GroomLabSDK_DIR "${CMAKE_CURRENT_LIST_DIR}/../cmake")
    endif()
    find_package(GroomLabSDK 1.0 CONFIG REQUIRED)
endif()

# A C++ bridge, through the header-only C++17 wrapper (groomlab/groomlab.hpp).
add_executable(groom_to_obj groom_to_obj.cpp)
target_compile_features(groom_to_obj PRIVATE cxx_std_17)
target_link_libraries(groom_to_obj PRIVATE GroomLab::groomlab)

# The plain C API (C99).
add_executable(groom_counts groom_counts.c)
set_target_properties(groom_counts PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
target_link_libraries(groom_counts PRIVATE GroomLab::groomlab)

# Windows finds DLLs beside the executable: copy groomlab.dll there after each build.
if(WIN32)
    foreach(sample groom_to_obj groom_counts)
        add_custom_command(TARGET ${sample} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:${sample}> ${GROOMLAB_SDK_RUNTIME_DLLS} $<TARGET_FILE_DIR:${sample}>
            COMMAND_EXPAND_LISTS)
    endforeach()
endif()
