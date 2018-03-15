# - Returns a version string from Git tags
#
# This function inspects the annotated git tags for the project and returns a string
# into a CMake variable
#
#  get_git_version(<var>)
#
# - Example
#
# include (GetGitVersion)
# get_git_version(GIT_VERSION)
#
# Requires CMake 2.8.11+
find_package(Git)

if (__get_git_version)
	return()
endif ()

set (__get_git_version INCLUDED)


function (get_date_revision var)
	set (ENV{TZ} "UTC")
	execute_process(
		COMMAND date "+%S"
		WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
		RESULT_VARIABLE status
		OUTPUT_VARIABLE DATE_REVISION
		ERROR_QUIET)
	if (NOT ${status})
		message(STATUS "Date Revision: ${DATE_REVISION}")
		set (${var} "${DATE_REVISION}" PARENT_SCOPE)
	endif ()
endfunction ()


function (get_git_revision var)
	if (GIT_EXECUTABLE)
		set (ENV{TZ} "UTC")
		execute_process(
			COMMAND ${GIT_EXECUTABLE} --git-dir ./.git show --quiet --date=format-local:%Y%m%d%H%M%S --format=%cd
			WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
			RESULT_VARIABLE status
			OUTPUT_VARIABLE GIT_REVISION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if (NOT ${status})
			message(STATUS "Git Revision: ${GIT_REVISION}")
			set (${var} "${GIT_REVISION}" PARENT_SCOPE)
		endif ()
	endif ()
endfunction ()


function (get_git_hash var)
	if (GIT_EXECUTABLE)
		execute_process(
			COMMAND ${GIT_EXECUTABLE} --git-dir ./.git rev-parse --short HEAD
			WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
			RESULT_VARIABLE status
			OUTPUT_VARIABLE GIT_HASH
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if (NOT ${status})
			message(STATUS "Git Hash: ${GIT_HASH}")
			set (${var} "${GIT_HASH}" PARENT_SCOPE)
		endif ()
	endif ()
endfunction ()


function (get_git_version var)
	if (GIT_EXECUTABLE)
		execute_process(
			COMMAND ${GIT_EXECUTABLE} --git-dir ./.git describe --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=8
			WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
			RESULT_VARIABLE status
			OUTPUT_VARIABLE GIT_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if (NOT ${status})
			string(SUBSTRING "${GIT_VERSION}" 1 -1 GIT_VERSION)
			string(REGEX REPLACE "-[0-9]+-g.*" "" GIT_VERSION "${GIT_VERSION}")

			message(STATUS "Git Version: ${GIT_VERSION}")
			set (${var} "${GIT_VERSION}" PARENT_SCOPE)
		endif ()
	endif ()
endfunction ()


function (get_git_full_version var)
	if (GIT_EXECUTABLE)
		execute_process(
			COMMAND ${GIT_EXECUTABLE} --git-dir ./.git describe --match "v[0-9]*.[0-9]*.[0-9]*" --abbrev=8
			WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
			RESULT_VARIABLE status
			OUTPUT_VARIABLE GIT_VERSION
			OUTPUT_STRIP_TRAILING_WHITESPACE
			ERROR_QUIET)
		if (NOT ${status})
			string(REGEX REPLACE "-[0-9]+-g" "-" GIT_VERSION "${GIT_VERSION}")

			# Work out if the repository is dirty
			execute_process(
				COMMAND ${GIT_EXECUTABLE} --git-dir ./.git update-index -q --refresh
				WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
				OUTPUT_QUIET
				ERROR_QUIET)
			execute_process(
				COMMAND ${GIT_EXECUTABLE} --git-dir ./.git diff-index --name-only HEAD --
				WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
				OUTPUT_VARIABLE GIT_DIFF_INDEX
				ERROR_QUIET)
			string(COMPARE NOTEQUAL "${GIT_DIFF_INDEX}" "" GIT_DIRTY)
			if (${GIT_DIRTY})
				set (GIT_VERSION "${GIT_VERSION}-dirty")
			endif ()

			message(STATUS "Git Version: ${GIT_VERSION}")
			set (${var} "${GIT_VERSION}" PARENT_SCOPE)
		endif ()
	endif ()
endfunction ()
