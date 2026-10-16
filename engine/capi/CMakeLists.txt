# SPDX-License-Identifier: Apache-2.0
# The GroomLab C API: a shared library `groomlab` (groomlab.dll / libgroomlab.so) exporting only
# the extern "C" functions of include/groomlab/groomlab_c.h, the core linked in statically and
# hidden. include/groomlab/groomlab.hpp is its header-only C++17 wrapper.
#
# No TBB: the library is loaded into hosts (Houdini, Blender...) that may have loaded a TBB of
# their own, and a process holds one tbb.dll. It links groomlab_core_private, the core's objects
# with a parallel backend of the library's own threads (engine/core/src/parallel_pool.cpp), so it
# depends on the system and the C++ runtime only.

add_library(groomlab_capi SHARED src/groomlab_c.cpp)
add_library(groomlab::capi ALIAS groomlab_capi)
set_target_properties(groomlab_capi PROPERTIES
    OUTPUT_NAME groomlab
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

# Users see only the public headers (they need nothing of the core's).
target_include_directories(groomlab_capi PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(groomlab_capi PRIVATE GROOMLAB_C_BUILD)
target_link_libraries(groomlab_capi PRIVATE groomlab_core_private nlohmann_json::nlohmann_json groomlab::warnings)

if(MSVC)
    # The DLL exports only what is marked dllexport (the gl_ functions). No incremental linking:
    # the whole core goes in (see engine/python's note on the growing .pdb).
    target_link_options(groomlab_capi PRIVATE /INCREMENTAL:NO)
elseif(NOT APPLE)
    # The static core's symbols are not hidden by the visibility preset (it is compiled without
    # it): the version script exports gl_* only.
    set(_capi_map "${CMAKE_CURRENT_BINARY_DIR}/groomlab_c.map")
    file(WRITE "${_capi_map}" "{\n  global: gl_*;\n  local: *;\n};\n")
    # nodelete: the parallel backend's threads run the library's code until the process ends, so
    # a host's dlclose must not unmap it (on Windows the backend pins the DLL).
    target_link_options(groomlab_capi PRIVATE "LINKER:--version-script=${_capi_map}" "LINKER:--exclude-libs,ALL"
                        "LINKER:-z,nodelete")
    set_property(TARGET groomlab_capi APPEND PROPERTY LINK_DEPENDS "${_capi_map}")
endif()

# The SDK samples (samples/: MIT, built here as they build from an installed SDK), against this
# build's library under the SDK package's target name; the core's warnings on them too.
add_library(GroomLab::groomlab ALIAS groomlab_capi)
set(GROOMLAB_SDK_RUNTIME_DLLS "")  # the library needs no DLL of its own beside it
add_subdirectory(samples)
target_link_libraries(groom_to_obj PRIVATE groomlab::warnings)
if(MSVC)
    target_compile_options(groom_counts PRIVATE /W4 /WX)
else()
    target_compile_options(groom_counts PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

# ---- The SDK install component -----------------------------------------------------------------
# What a third party builds a bridge with, separate from the GroomLab (Maya module) component:
#
#   cmake --install build/<preset> --config RelWithDebInfo --component SDK --prefix <dir>
#
#   <dir>/GroomLabSDK/
#     bin/      groomlab.dll (Windows; it needs no other DLL of GroomLab's beside it)
#     lib/      groomlab.lib (the import library), libgroomlab.so elsewhere
#     include/  groomlab/groomlab_c.h, groomlab/groomlab.hpp
#     cmake/    GroomLabSDKConfig.cmake (+ version): find_package(GroomLabSDK), GroomLab::groomlab
#     samples/  groom_to_obj.cpp, groom_counts.c, CMakeLists.txt, LICENSE.txt (MIT)
#     LICENSE, NOTICE, THIRD_PARTY_NOTICES.txt
#
# No TBB in the SDK: the library runs its parallel loops on its own threads (see the top).
set(GROOMLAB_SDK_DIR "GroomLabSDK")
install(TARGETS groomlab_capi
    RUNTIME DESTINATION "${GROOMLAB_SDK_DIR}/bin" COMPONENT SDK
    LIBRARY DESTINATION "${GROOMLAB_SDK_DIR}/lib" COMPONENT SDK
    ARCHIVE DESTINATION "${GROOMLAB_SDK_DIR}/lib" COMPONENT SDK)
install(DIRECTORY include/ DESTINATION "${GROOMLAB_SDK_DIR}/include" COMPONENT SDK)
install(FILES samples/groom_to_obj.cpp samples/groom_counts.c samples/CMakeLists.txt samples/LICENSE.txt
        DESTINATION "${GROOMLAB_SDK_DIR}/samples" COMPONENT SDK)
# The package's version is the C API's, read from the header (its single source).
file(STRINGS include/groomlab/groomlab_c.h _capi_version REGEX "^#define GROOMLAB_C_API_VERSION_(MAJOR|MINOR) ")
string(REGEX REPLACE ".*MAJOR ([0-9]+).*MINOR ([0-9]+).*" "\\1.\\2" _capi_version "${_capi_version}")
include(CMakePackageConfigHelpers)
write_basic_package_version_file("${CMAKE_CURRENT_BINARY_DIR}/GroomLabSDKConfigVersion.cmake"
    VERSION ${_capi_version} COMPATIBILITY SameMajorVersion)
install(FILES cmake/GroomLabSDKConfig.cmake "${CMAKE_CURRENT_BINARY_DIR}/GroomLabSDKConfigVersion.cmake"
        DESTINATION "${GROOMLAB_SDK_DIR}/cmake" COMPONENT SDK)
groomlab_install_legal("${GROOMLAB_SDK_DIR}" SDK)
