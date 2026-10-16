# SPDX-License-Identifier: GPL-3.0-or-later
# cmake -DBUILD_DIR=<build> -DCONFIG=<config> -DSTAGE=<dir> -DZIP=<zip> -P assemble_extension.cmake
#
# Installs the build's Blender component (the add-on and bin/ with the helper) into
# <STAGE>/GroomLabBlender and zips that folder's contents (blender_manifest.toml at the zip's root,
# as Blender's "Install from Disk" and extension repositories take it).

foreach(var BUILD_DIR CONFIG STAGE ZIP)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "assemble_extension.cmake needs -D${var}=")
    endif()
endforeach()

set(_dir "${STAGE}/GroomLabBlender")
file(REMOVE_RECURSE "${_dir}")
file(REMOVE "${ZIP}" "${STAGE}/assembled.stamp")
execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --config "${CONFIG}" --component Blender --prefix "${STAGE}"
    RESULT_VARIABLE _rc OUTPUT_QUIET)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "installing the Blender component failed (${_rc})")
endif()
foreach(_need blender_manifest.toml LICENSE bin/LICENSE bin/NOTICE bin/THIRD_PARTY_NOTICES.txt bin/LICENSE-HELPER.txt)
    if(NOT EXISTS "${_dir}/${_need}")
        message(FATAL_ERROR "no ${_need} in ${_dir}")
    endif()
endforeach()
file(GLOB_RECURSE _all LIST_DIRECTORIES true "${_dir}/*")
foreach(c ${_all})
    get_filename_component(_name "${c}" NAME)
    if(_name STREQUAL "__pycache__")
        file(REMOVE_RECURSE "${c}")
    endif()
endforeach()

file(GLOB _entries RELATIVE "${_dir}" "${_dir}/*")
list(SORT _entries)
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${ZIP}" --format=zip -- ${_entries}
    WORKING_DIRECTORY "${_dir}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "zipping the extension failed (${_rc})")
endif()
file(TOUCH "${STAGE}/assembled.stamp")
