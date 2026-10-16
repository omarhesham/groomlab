# SPDX-License-Identifier: Apache-2.0
add_executable(groomlab-cli main.cpp)
target_link_libraries(groomlab-cli PRIVATE groomlab::core groomlab::warnings)
if(TARGET groomlab_alembic)
    target_link_libraries(groomlab-cli PRIVATE groomlab::alembic)
    target_compile_definitions(groomlab-cli PRIVATE GROOMLAB_HAS_ALEMBIC=1)
else()
    target_compile_definitions(groomlab-cli PRIVATE GROOMLAB_HAS_ALEMBIC=0)
endif()

if(WIN32 AND GROOMLAB_TBB_RUNTIME)
    add_custom_command(TARGET groomlab-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_RUNTIME_DLLS:groomlab-cli> $<TARGET_FILE_DIR:groomlab-cli>
        COMMAND_EXPAND_LISTS)
endif()

# Shipped in the Maya module (GroomLab/bin) with its runtime DLLs, for farms.
install(TARGETS groomlab-cli RUNTIME DESTINATION "GroomLab/bin" COMPONENT GroomLab)
groomlab_install_legal("GroomLab" GroomLab)  # also without the Maya plugin (a Linux farm build)
groomlab_install_runtime(groomlab-cli "GroomLab/bin" GroomLab)
if(WIN32 AND GROOMLAB_TBB_RUNTIME)
    install(FILES $<TARGET_RUNTIME_DLLS:groomlab-cli> DESTINATION "GroomLab/bin" COMPONENT GroomLab)
endif()
