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

# Fails when the Lua Xapiand builds is not the pinned lua/lua commit plus xapiand-limits.patch: the
# checkout's commit, the patch's hash, the patch applying to the pristine commit, and then every file of
# the built tree (not only the patched ones) equal to the patched commit's, with no file added or missing.
# LUA_COMMIT overrides the pin (to show that a wrong one fails).
# Run: cmake -DLUA_SRC=... -DPATCH=... -DVERSION_FILE=... -DWORK=... -P lua_patch_drift.cmake
include("${VERSION_FILE}")
if (LUA_COMMIT)
	set(XAPIAND_LUA_COMMIT "${LUA_COMMIT}")
endif ()

execute_process(COMMAND git -C "${LUA_SRC}" rev-parse HEAD OUTPUT_VARIABLE head OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc)
if (NOT rc EQUAL 0 OR NOT head STREQUAL XAPIAND_LUA_COMMIT)
	message(FATAL_ERROR "lua drift: the Lua checkout is at '${head}', the pin is ${XAPIAND_LUA_COMMIT}")
endif ()
file(SHA256 "${PATCH}" got)
if (NOT got STREQUAL XAPIAND_LUA_PATCH_SHA256)
	message(FATAL_ERROR "lua drift: xapiand-limits.patch: sha256 ${got}, expected ${XAPIAND_LUA_PATCH_SHA256}")
endif ()

# A pristine copy of the pinned commit, then the patch.
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
set(tree "${WORK}/tree")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_directory "${LUA_SRC}" "${tree}" RESULT_VARIABLE rc)
execute_process(COMMAND git -C "${tree}" checkout -q -- . RESULT_VARIABLE rc2)
execute_process(COMMAND git -C "${tree}" clean -fdxq RESULT_VARIABLE rc3)
if (NOT rc EQUAL 0 OR NOT rc2 EQUAL 0 OR NOT rc3 EQUAL 0)
	message(FATAL_ERROR "lua drift: cannot restore the pinned commit's files")
endif ()
execute_process(COMMAND patch -p1 -i "${PATCH}" WORKING_DIRECTORY "${tree}" RESULT_VARIABLE rc OUTPUT_QUIET)
if (NOT rc EQUAL 0)
	message(FATAL_ERROR "lua drift: xapiand-limits.patch does not apply to the pinned commit")
endif ()
file(GLOB_RECURSE expected RELATIVE "${tree}" "${tree}/*")
file(GLOB_RECURSE built RELATIVE "${LUA_SRC}" "${LUA_SRC}/*")
list(FILTER expected EXCLUDE REGEX "^\\.git/|\\.orig$")
list(FILTER built EXCLUDE REGEX "^\\.git/")
list(SORT expected)
list(SORT built)
if (NOT expected STREQUAL built)
	message(FATAL_ERROR "lua drift: the built tree's files differ from the patched commit's:\n  built: ${built}\n  expected: ${expected}")
endif ()
foreach (name IN LISTS expected)
	file(SHA256 "${tree}/${name}" want)
	file(SHA256 "${LUA_SRC}/${name}" got)
	if (NOT got STREQUAL want)
		message(FATAL_ERROR "lua drift: the built ${name}: sha256 ${got}, expected ${want}")
	endif ()
endforeach ()
list(LENGTH expected count)
message(STATUS "lua drift: the build compiles the pinned Lua with the limits patch (${count} files identical)")
