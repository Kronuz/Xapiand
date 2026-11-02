# Fails when the Lua Xapiand builds is not the pinned tarball plus lstrlib-step-budget.patch:
# the tarball's hash, the patch applying to it, the patched file's hash, and the built tree's file.
# Run: cmake -DLUA_SRC=... -DLUA_TARBALL=... -DPATCH=... -DVERSION_FILE=... -DWORK=... -P lua_patch_drift.cmake
include("${VERSION_FILE}")

function(expect_hash file expected what)
	file(SHA256 "${file}" got)
	if (NOT got STREQUAL expected)
		message(FATAL_ERROR "lua drift: ${what}: sha256 ${got}, expected ${expected}")
	endif ()
endfunction()

expect_hash("${LUA_TARBALL}" "${XAPIAND_LUA_TARBALL_SHA256}" "the Lua tarball")
file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E tar xzf "${LUA_TARBALL}" WORKING_DIRECTORY "${WORK}" RESULT_VARIABLE rc)
if (NOT rc EQUAL 0)
	message(FATAL_ERROR "lua drift: cannot unpack the tarball")
endif ()
set(tree "${WORK}/lua-5.4.7")
expect_hash("${tree}/src/lstrlib.c" "${XAPIAND_LUA_LSTRLIB_PRISTINE_SHA256}" "the pristine lstrlib.c")
execute_process(COMMAND patch -p1 -i "${PATCH}" WORKING_DIRECTORY "${tree}" RESULT_VARIABLE rc OUTPUT_QUIET)
if (NOT rc EQUAL 0)
	message(FATAL_ERROR "lua drift: lstrlib-step-budget.patch does not apply to the pinned tarball")
endif ()
expect_hash("${tree}/src/lstrlib.c" "${XAPIAND_LUA_LSTRLIB_PATCHED_SHA256}" "the patched lstrlib.c")
expect_hash("${LUA_SRC}/src/lstrlib.c" "${XAPIAND_LUA_LSTRLIB_PATCHED_SHA256}" "the lstrlib.c the build compiles")
message(STATUS "lua drift: the build compiles the pinned Lua with the step budget")
