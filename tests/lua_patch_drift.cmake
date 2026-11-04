# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# Fails when the Lua Xapiand builds is not the pinned tarball plus xapiand-limits.patch: the tarball's
# hash, the patch's hash, the patch applying to the tarball, and then every file of the built tree
# (not only the patched ones) equal to the patched tarball's, with no file added or missing.
# Run: cmake -DLUA_SRC=... -DLUA_TARBALL=... -DPATCH=... -DVERSION_FILE=... -DWORK=... -P lua_patch_drift.cmake
include("${VERSION_FILE}")

function(expect_hash file expected what)
	file(SHA256 "${file}" got)
	if (NOT got STREQUAL expected)
		message(FATAL_ERROR "lua drift: ${what}: sha256 ${got}, expected ${expected}")
	endif ()
endfunction()

expect_hash("${LUA_TARBALL}" "${XAPIAND_LUA_TARBALL_SHA256}" "the Lua tarball")
expect_hash("${PATCH}" "${XAPIAND_LUA_PATCH_SHA256}" "xapiand-limits.patch")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${LUA_TARBALL}" WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE rc)
if (NOT rc EQUAL 0)
	message(FATAL_ERROR "lua drift: cannot unpack the tarball")
endif ()
set(tree "${WORK}/lua-5.4.7")
execute_process(COMMAND patch -p1 -i "${PATCH}" WORKING_DIRECTORY "${tree}" RESULT_VARIABLE rc OUTPUT_QUIET)
if (NOT rc EQUAL 0)
	message(FATAL_ERROR "lua drift: xapiand-limits.patch does not apply to the pinned tarball")
endif ()

file(GLOB expected RELATIVE "${tree}/src" "${tree}/src/*")
file(GLOB built RELATIVE "${LUA_SRC}/src" "${LUA_SRC}/src/*")
list(SORT expected)
list(SORT built)
if (NOT expected STREQUAL built)
	message(FATAL_ERROR "lua drift: the built tree's files differ from the patched tarball's:\n  built: ${built}\n  expected: ${expected}")
endif ()
set(patched 0)
foreach (name IN LISTS expected)
	file(SHA256 "${tree}/src/${name}" want)
	expect_hash("${LUA_SRC}/src/${name}" "${want}" "the built src/${name}")
endforeach ()
list(LENGTH expected count)
message(STATUS "lua drift: the build compiles the pinned Lua with the limits patch (${count} files identical)")
