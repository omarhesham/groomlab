# SPDX-License-Identifier: Apache-2.0
# The core in the browser (docs/WEB.md). With Emscripten (the wasm preset): groomlab.js and
# groomlab.wasm, the WebGroom binding (src/web_groom.h, bound with embind in src/bind.cpp),
# single-threaded (the core's serial parallel backend), copied to GROOMLAB_WEB_PUBLIC_DIR (the
# website's web/public/wasm) after each build. Natively: groomlab_web_probe, the same binding as a
# command line tool, for checking the module's strands against the desktop engine's.
if(EMSCRIPTEN)
    set(GROOMLAB_WEB_PUBLIC_DIR "${PROJECT_SOURCE_DIR}/web/public/wasm" CACHE PATH
        "Where the built groomlab.js and groomlab.wasm are copied (empty: nowhere)")
    add_executable(groomlab_web src/web_groom.cpp src/web_groom.h src/bind.cpp)
    target_link_libraries(groomlab_web PRIVATE groomlab::core nlohmann_json::nlohmann_json groomlab::warnings)
    set_target_properties(groomlab_web PROPERTIES OUTPUT_NAME groomlab SUFFIX ".js")
    target_link_options(groomlab_web PRIVATE
        -lembind
        # A factory, createGroomLab(), loaded by a classic worker (importScripts) or by Node.
        -sMODULARIZE=1
        -sEXPORT_NAME=createGroomLab
        -sENVIRONMENT=web,worker,node
        # Grooms of a million points and more: memory grows as evaluations need it.
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=64MB
        -sMAXIMUM_MEMORY=2GB
        -sSTACK_SIZE=4MB
        # No files in the browser: grooms come in as text (texture maps and caches are not read).
        -sFILESYSTEM=0
        # The loader's license line on top (it is committed: web/public/wasm).
        "--extern-pre-js=${CMAKE_CURRENT_SOURCE_DIR}/src/header.js")
    set_property(TARGET groomlab_web APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/header.js")
    if(GROOMLAB_WEB_PUBLIC_DIR)
        add_custom_command(TARGET groomlab_web POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${GROOMLAB_WEB_PUBLIC_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE_DIR:groomlab_web>/groomlab.js" "$<TARGET_FILE_DIR:groomlab_web>/groomlab.wasm"
                    "${GROOMLAB_WEB_PUBLIC_DIR}")
    endif()
else()
    add_executable(groomlab_web_probe src/web_groom.cpp src/web_groom.h src/web_probe.cpp)
    target_link_libraries(groomlab_web_probe PRIVATE groomlab::core nlohmann_json::nlohmann_json groomlab::warnings)
    if(WIN32 AND GROOMLAB_TBB_RUNTIME)
        add_custom_command(TARGET groomlab_web_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:groomlab_web_probe> $<TARGET_FILE_DIR:groomlab_web_probe>
            COMMAND_EXPAND_LISTS)
    endif()
endif()
