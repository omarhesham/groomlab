# SPDX-License-Identifier: Apache-2.0
# RenderMan procedural: renders groom files at render time in prman (a "DynamicLoad" DSO, Procedural2
# and the older Procedural API RenderMan for Maya's RenderManProcedural node uses). Needs only RenderMan Pro Server's SDK (GROOMLAB_RMANTREE), not Maya. Links libprman (the Ri
# calls), which prman has loaded when it loads the DSO; nothing of Pixar's is installed with it.
add_library(groomlab_renderman MODULE groomlab_renderman.cpp)
target_link_libraries(groomlab_renderman PRIVATE groomlab::core groomlab::alembic RenderMan::RenderMan)
set_target_properties(groomlab_renderman PROPERTIES
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/procedurals/renderman"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/procedurals/renderman")
# No groomlab::warnings: RenderMan's headers trip /W4.

# prman runs without Maya's DLLs, and Windows does not search a DSO's own folder for its
# dependencies: TBB is delay-loaded, installed beside the procedural and loaded from there when the
# host has not loaded it already (prman has its own TBB built in; Maya with RenderMan for Maya has
# Maya's tbb.dll), as the Arnold procedural does.
if(WIN32 AND GROOMLAB_TBB_RUNTIME)
    target_link_options(groomlab_renderman PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:groomlab::tbb>")
    target_link_libraries(groomlab_renderman PRIVATE delayimp)
    target_compile_definitions(groomlab_renderman PRIVATE "GROOMLAB_TBB_DLL=L\"$<TARGET_FILE_NAME:groomlab::tbb>\"")
    add_custom_command(TARGET groomlab_renderman POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:groomlab_renderman> $<TARGET_FILE_DIR:groomlab_renderman>
        COMMAND_EXPAND_LISTS)
    install(FILES $<TARGET_RUNTIME_DLLS:groomlab_renderman> DESTINATION "GroomLab/procedurals/renderman" COMPONENT GroomLab)
endif()

# Installed into the Maya module beside the Arnold procedural, with the example scene.
install(TARGETS groomlab_renderman
    LIBRARY DESTINATION "GroomLab/procedurals/renderman" COMPONENT GroomLab
    RUNTIME DESTINATION "GroomLab/procedurals/renderman" COMPONENT GroomLab)
install(FILES examples/groom.rib DESTINATION "GroomLab/procedurals/renderman" COMPONENT GroomLab)
# Linux: the PRMANEXPORT entry points exported alone, libtbb.so.12 beside the DSO ($ORIGIN).
groomlab_hide_static_symbols(groomlab_renderman)
groomlab_install_runtime(groomlab_renderman "GroomLab/procedurals/renderman" GroomLab)
