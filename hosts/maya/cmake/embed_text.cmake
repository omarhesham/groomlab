# SPDX-License-Identifier: Apache-2.0
# Build step (cmake -P): a text file embedded in a header as a NUL-terminated char array (MSVC's
# string literals stop at 16 KB), `const char <NAME>[]`.
#   -DSOURCE=<text file> -DHEADER=<out .h> -DNAME=<array name>
file(READ "${SOURCE}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
string(REGEX REPLACE "(0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,0x..,)" "\\1\n" bytes "${bytes}")
get_filename_component(source_name "${SOURCE}" NAME)
file(WRITE "${HEADER}" "#pragma once\n// Generated from ${source_name} by hosts/maya/cmake/embed_text.cmake.\ninline constexpr char ${NAME}[] = {\n${bytes}0x00};\n")
