# SPDX-License-Identifier: Apache-2.0
# Test (ctest gpu.kernel_targets, cmake -P): every kernel of shaders/look.slang compiles for the
# targets the plugin does not embed yet, SPIR-V (Vulkan) and Metal (source), so the one source keeps
# serving the hosts to come (the standalone app, macOS). Nothing runs them.
#   -DSLANGC=<slangc.exe> -DSOURCE=<look.slang> -DOUT=<scratch folder> -DENTRIES=<a;b;...>
file(MAKE_DIRECTORY "${OUT}")
set(failed "")
foreach(entry IN LISTS ENTRIES)
    foreach(target spirv metal)
        set(extra "")
        if(target STREQUAL "metal")
            set(extra -DGROOMLAB_SLANG_LAYOUT -DGROOMLAB_SLANG_METAL)  # one buffer table: Slang lays it out (look.slang)
        endif()
        execute_process(
            COMMAND "${SLANGC}" "${SOURCE}" -entry ${entry} -target ${target} ${extra} -O3 -warnings-as-errors all
                    -o "${OUT}/look_${entry}.${target}"
            RESULT_VARIABLE result
            OUTPUT_VARIABLE output
            ERROR_VARIABLE errors)
        if(NOT result EQUAL 0)
            string(APPEND failed "${entry} (${target}): ${output}${errors}\n")
        else()
            file(SIZE "${OUT}/look_${entry}.${target}" size)
            message(STATUS "${entry} ${target}: ${size} bytes")
        endif()
    endforeach()
endforeach()
if(failed)
    message(FATAL_ERROR "look.slang does not compile for every target:\n${failed}")
endif()
