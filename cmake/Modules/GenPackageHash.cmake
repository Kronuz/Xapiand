# GenPackageHash.cmake — resolve the short git commit hash at *build* time and
# write it into a generated header. Run as a script (cmake -P) by a custom target
# so it re-evaluates on every build, but the header is only rewritten when the
# value actually changes (so package.cc recompiles only on a real change).
#
# Fallback chain:
#   1. live git checkout      — `git rev-parse --short=7 HEAD`   (fresh, dev/CI)
#   2. git-archive tarball    — export-subst'd $Format:%h$ in SRC_IN
#   3. "unknown"              — no git, no substitution
#
# Expected -D arguments: GIT_EXECUTABLE, SRCDIR, SRC_IN, DST

set(_hash "")

# 1) Live git working copy — the common dev/CI path; always current.
if(GIT_EXECUTABLE AND IS_DIRECTORY "${SRCDIR}/.git")
	execute_process(
		COMMAND "${GIT_EXECUTABLE}" -C "${SRCDIR}" rev-parse --short=7 HEAD
		OUTPUT_VARIABLE _hash
		OUTPUT_STRIP_TRAILING_WHITESPACE
		ERROR_QUIET
		RESULT_VARIABLE _rc)
	if(NOT _rc EQUAL 0)
		set(_hash "")
	endif()
endif()

# 2) Source tarball from `git archive` — export-subst replaced $Format:%h$ with
#    the hash. In a checkout SRC_IN still holds the literal placeholder, which
#    contains '$' and so fails the hex test below and is correctly ignored.
if(_hash STREQUAL "" AND EXISTS "${SRC_IN}")
	file(READ "${SRC_IN}" _subst)
	string(REGEX MATCH "PACKAGE_ARCHIVE_HASH \"([^\"]*)\"" _m "${_subst}")
	if(CMAKE_MATCH_1 MATCHES "^[0-9a-f]+$")
		set(_hash "${CMAKE_MATCH_1}")
	endif()
endif()

# 3) Nothing to go on.
if(_hash STREQUAL "")
	set(_hash "unknown")
endif()

set(_content "#pragma once\n#define PACKAGE_GIT_HASH \"${_hash}\"\n")

# Rewrite only on change, to avoid a spurious recompile every build.
set(_old "")
if(EXISTS "${DST}")
	file(READ "${DST}" _old)
endif()
if(NOT _old STREQUAL _content)
	file(WRITE "${DST}" "${_content}")
	message(STATUS "Package git hash: ${_hash}")
endif()
