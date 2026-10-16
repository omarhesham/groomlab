# SPDX-License-Identifier: Apache-2.0
# groomlab_gpu: the GPU look preview's engine (include/groomlab/gpu/look_chain.h, look_engine.h,
# look_preview.h).
# The graph's last nodes (Interpolate, Length, Clump, Noise, Curl, Width) as compute kernels for a
# viewport while a param is dragged or guides are brushed. Host-free (the
# core and the GPU API only): Maya's plugin drives it on Viewport 2.0's device, the tests on one of
# their own. Windows only for now: DirectX 11 and OpenGL 4.5 (through the context current on the
# caller's thread, functions from wglGetProcAddress).
#
# The kernels are one Slang source, shaders/look.slang, compiled when the module builds by Slang's
# slangc, a build tool only: the release below, fetched at configure time and pinned by hash like
# the other dependencies (or GROOMLAB_SLANGC: a slangc of your own, to configure offline). Nothing
# of Slang ships or runs in Maya. The plugin embeds, per kernel:
#   DirectX 11: DXBC (cs_5_0; slangc hands its HLSL to Windows' own d3dcompiler_47);
#   OpenGL: GLSL 4.50 compute text, compiled by the driver (its shader cache keeps it).
# SPIR-V (Vulkan) and Metal are compiled by a test (gpu.kernel_targets) and not embedded yet.
if(NOT WIN32)
    # (Its OpenGL backend takes its functions from WGL and its kernels' DXBC from d3dcompiler: a
    # Linux backend, GLX/EGL or Vulkan, is to come. The CPU operators it previews are the core's.)
    message(STATUS "GPU look preview (engine/gpu): Windows only for now, left out with its tests (groomlab_gpu_tests)")
    return()
endif()

set(GROOMLAB_SLANGC "" CACHE FILEPATH "slangc.exe for the GPU preview's kernels (empty: Slang 2026.19 is fetched)")
set(_slangc "${GROOMLAB_SLANGC}")
if(NOT _slangc)
    include(FetchContent)
    FetchContent_Declare(slang
        URL https://github.com/shader-slang/slang/releases/download/v2026.19/slang-2026.19-windows-x86_64.zip
        URL_HASH SHA256=FC922F214B5D115F663932E59E80E0F6B6CC6FAE140979CF307DA6459B6F8F33
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR no-cmake-project)  # binaries only: nothing to add to the build
    groomlab_cached_source(slang 2026.19)
    FetchContent_MakeAvailable(slang)
    groomlab_store_source(slang)
    set(_slangc "${slang_SOURCE_DIR}/bin/slangc.exe")
endif()
message(STATUS "GPU preview kernels: ${_slangc}")

# The compiled DirectX kernels, shared by every build folder on the machine and keyed by each
# kernel's HLSL (cmake/embed_kernel.cmake): d3dcompiler takes minutes over ClearanceCS, so an edit
# recompiles only the kernels it changes, and each Maya version's tree reuses the others'. Empty:
# every kernel compiles each time its source changes.
if(WIN32)
    set(_kernel_cache_default "$ENV{USERPROFILE}/groomlab-deps/kernels")
else()
    set(_kernel_cache_default "")
endif()
set(GROOMLAB_KERNEL_CACHE "${_kernel_cache_default}" CACHE PATH
    "Shared folder of compiled GPU kernels, keyed by their HLSL (empty: no cache)")
set(_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/look.slang")
set(_gen "${CMAKE_CURRENT_BINARY_DIR}/generated")
# (PointsTextureCS and PointsTexturePointsCS write the standalone app's point texture: look_engine.h
# LookTargets::pointTexture.)
set(_kernels InterpolateCS ClearanceCS LengthCS CentersCS ClumpCS NoiseCS NoisePointsCS CurlCS WidthCS LinesCS LinesPointsCS
             PointsTextureCS PointsTexturePointsCS GatherCS UvWidthsCS PointStrandsCS WidthPointsCS ShareCS CurlAxesCS PartCS
             LongestCS FieldCS GravityCS FlyawayCS CollideCS WindCS BlendCS BlendPointsCS
             GridClearCS GridBoundsCS GridLayCS GridMarkCS GridDilateCS GridCompactCS GridZeroCS GridSplatCS GridUnknownsCS GridSorCS GridWalkCS
             SelfCollideNodeCS SelfCollideGradientCS SelfCollidePushCS StaticReachCS StaticMomentsCS StaticNodeCS StaticFieldCS
             StaticPushCS TieFrameCS TieRouteCS TieLayCS BendCS DirectionCS TransformCS DisplaceCS TwistCS TwistAxesCS
             # The shapers reading their neighbours or a reference.
             SmoothCS RestoreLengthCS BlendNeighborsCS TangleCS FlowCS
             # Affect masks, Jiggle with motion, MeshCut.
             AffectCS JiggleCS MeshCutCS)
set(_kernel_headers "")
foreach(_target dxbc glsl)
    set(_includes "")
    foreach(_k IN LISTS _kernels)
        set(_h "${_gen}/look_${_k}_${_target}.h")
        add_custom_command(OUTPUT "${_h}"
            COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_source} -DENTRY=${_k} -DFORMAT=${_target} -DCACHE=${GROOMLAB_KERNEL_CACHE}
                    -DOUT=${_gen}/look_${_k}.${_target} -DHEADER=${_h} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            DEPENDS "${_source}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            COMMENT "slangc: ${_k} (${_target})"
            VERBATIM)
        list(APPEND _kernel_headers "${_h}")
        string(APPEND _includes "#include \"look_${_k}_${_target}.h\"\n")
    endforeach()
    file(CONFIGURE OUTPUT "${_gen}/look_kernels_${_target}.h" CONTENT
        "#pragma once\n// Generated by engine/gpu/CMakeLists.txt: each kernel of shaders/look.slang (${_target}).\n@_includes@"
        @ONLY)
endforeach()
# The lit strand lines' frames from points already on the GPU (line_frames.h): shaders/lines.slang,
# which includes look.slang (its lineFrame).
set(_lines_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/lines.slang")
set(_line_kernels FramesCS WidthsCS RibbonsCS RibbonLayoutCS RibbonColorsCS)
foreach(_target dxbc glsl)
    set(_includes "")
    foreach(_k IN LISTS _line_kernels)
        set(_h "${_gen}/lines_${_k}_${_target}.h")
        add_custom_command(OUTPUT "${_h}"
            COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_lines_source} -DENTRY=${_k} -DFORMAT=${_target} -DCACHE=${GROOMLAB_KERNEL_CACHE}
                    -DOUT=${_gen}/lines_${_k}.${_target} -DHEADER=${_h} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            DEPENDS "${_lines_source}" "${_source}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            COMMENT "slangc: ${_k} (${_target})"
            VERBATIM)
        list(APPEND _kernel_headers "${_h}")
        string(APPEND _includes "#include \"lines_${_k}_${_target}.h\"\n")
    endforeach()
    file(CONFIGURE OUTPUT "${_gen}/line_kernels_${_target}.h" CONTENT
        "#pragma once\n// Generated by engine/gpu/CMakeLists.txt: each kernel of shaders/lines.slang (${_target}).\n@_includes@"
        @ONLY)
endforeach()

# Following the scalp on the GPU (line_frames.h runFollow): shaders/follow.slang, which includes
# lines.slang (and so look.slang).
set(_follow_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/follow.slang")
set(_follow_kernels FollowCS)
foreach(_target dxbc glsl)
    set(_includes "")
    foreach(_k IN LISTS _follow_kernels)
        set(_h "${_gen}/follow_${_k}_${_target}.h")
        add_custom_command(OUTPUT "${_h}"
            COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_follow_source} -DENTRY=${_k} -DFORMAT=${_target} -DCACHE=${GROOMLAB_KERNEL_CACHE}
                    -DOUT=${_gen}/follow_${_k}.${_target} -DHEADER=${_h} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            DEPENDS "${_follow_source}" "${_lines_source}" "${_source}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            COMMENT "slangc: ${_k} (${_target})"
            VERBATIM)
        list(APPEND _kernel_headers "${_h}")
        string(APPEND _includes "#include \"follow_${_k}_${_target}.h\"\n")
    endforeach()
    file(CONFIGURE OUTPUT "${_gen}/follow_kernels_${_target}.h" CONTENT
        "#pragma once\n// Generated by engine/gpu/CMakeLists.txt: each kernel of shaders/follow.slang (${_target}).\n@_includes@"
        @ONLY)
endforeach()

# The hair's density grid for the viewport's self-shadowing (line_frames.h runDensity):
# shaders/density.slang, on its own.
set(_density_source "${CMAKE_CURRENT_SOURCE_DIR}/shaders/density.slang")
set(_density_kernels DensityClearCS DensitySplatCS DensityResolveCS)
foreach(_target dxbc glsl)
    set(_includes "")
    foreach(_k IN LISTS _density_kernels)
        set(_h "${_gen}/density_${_k}_${_target}.h")
        add_custom_command(OUTPUT "${_h}"
            COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_density_source} -DENTRY=${_k} -DFORMAT=${_target} -DCACHE=${GROOMLAB_KERNEL_CACHE}
                    -DOUT=${_gen}/density_${_k}.${_target} -DHEADER=${_h} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            DEPENDS "${_density_source}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_kernel.cmake"
            COMMENT "slangc: ${_k} (${_target})"
            VERBATIM)
        list(APPEND _kernel_headers "${_h}")
        string(APPEND _includes "#include \"density_${_k}_${_target}.h\"\n")
    endforeach()
    file(CONFIGURE OUTPUT "${_gen}/density_kernels_${_target}.h" CONTENT
        "#pragma once\n// Generated by engine/gpu/CMakeLists.txt: each kernel of shaders/density.slang (${_target}).\n@_includes@"
        @ONLY)
endforeach()

add_library(groomlab_gpu STATIC
    src/look_chain.cpp
    src/look_engine.cpp
    src/look_engine_internal.h
    src/look_engine_d3d11.cpp
    src/look_engine_gl.cpp
    src/look_preview.cpp
    src/line_frames.cpp
    shaders/look.slang
    shaders/lines.slang
    shaders/follow.slang
    shaders/density.slang
    ${_kernel_headers})
set_source_files_properties(shaders/look.slang shaders/lines.slang shaders/follow.slang shaders/density.slang PROPERTIES HEADER_FILE_ONLY TRUE)  # slangc builds them
target_include_directories(groomlab_gpu PUBLIC include PRIVATE "${_gen}")
target_link_libraries(groomlab_gpu PUBLIC groomlab::core d3d11 opengl32 PRIVATE groomlab::warnings)
add_library(groomlab::gpu ALIAS groomlab_gpu)

# The targets not embedded yet (SPIR-V, Metal) still compile from the same source.
if(GROOMLAB_BUILD_TESTS)
    add_test(NAME gpu.kernel_targets
        COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_source} "-DENTRIES=${_kernels}"
                -DOUT=${CMAKE_CURRENT_BINARY_DIR}/kernel_targets -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_kernel_targets.cmake")
    set_tests_properties(gpu.kernel_targets PROPERTIES LABELS "quick;gpu")
    add_test(NAME gpu.line_kernel_targets
        COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_lines_source} "-DENTRIES=${_line_kernels}"
                -DOUT=${CMAKE_CURRENT_BINARY_DIR}/line_kernel_targets -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_kernel_targets.cmake")
    set_tests_properties(gpu.line_kernel_targets PROPERTIES LABELS "quick;gpu")
    add_test(NAME gpu.follow_kernel_targets
        COMMAND "${CMAKE_COMMAND}" -DSLANGC=${_slangc} -DSOURCE=${_follow_source} "-DENTRIES=${_follow_kernels}"
                -DOUT=${CMAKE_CURRENT_BINARY_DIR}/follow_kernel_targets -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_kernel_targets.cmake")
    set_tests_properties(gpu.follow_kernel_targets PROPERTIES LABELS "quick;gpu")
endif()
