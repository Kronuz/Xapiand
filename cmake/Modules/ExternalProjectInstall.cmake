#
# Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#

if (__external_project_install)
	return()
endif ()

set (__external_project_install INCLUDED)

set (DOWNLOAD_NAME_DIR "${CMAKE_CURRENT_LIST_DIR}")

include (CMakeParseArguments)

function (ExternalProject_Install)
	set (CMAKE_BUILD_TYPE Release)  # FIXME: Should it use installer's CMAKE_BUILD_TYPE?

	set (options QUIET)
	set (oneValueArgs
		PREFIX
		DOWNLOAD_DIR
		INSTALL_DIR
	)
	set (multiValueArgs CMAKE_ARGS)

	if (NOT ARGV0)
		message(FATAL_ERROR "You must specify an external project name.")
	endif ()
	set (_ARGS_NAME "${ARGV0}")
	list(REMOVE_AT ARGN 0)
	cmake_parse_arguments(_ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" "${ARGN}")

	# Hide output if requested
	if (_ARGS_QUIET)
		set (OUTPUT_QUIET "OUTPUT_QUIET")
	else ()
		unset(OUTPUT_QUIET)
		message(STATUS "Downloading/updating ${_ARGS_NAME}")
	endif ()

	# Set up where we will put our temporary CMakeLists.txt file and also
	# the base point below which the default source and binary dirs will be.
	# The prefix must always be an absolute path.
	if (NOT _ARGS_PREFIX)
		set (_ARGS_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/${_ARGS_NAME}-prefix")
	else ()
		get_filename_component(_ARGS_PREFIX "${_ARGS_PREFIX}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
	endif ()

	if (NOT _ARGS_DOWNLOAD_DIR)
		set (_ARGS_DOWNLOAD_DIR "${_ARGS_PREFIX}/src")
	endif ()

	if (NOT _ARGS_INSTALL_DIR)
		set (_ARGS_INSTALL_DIR "${_ARGS_PREFIX}")
	endif ()

	string(TOUPPER "${_ARGS_NAME}_ROOT" SANITIZED_ROOT)
	string(REPLACE "+" "X" SANITIZED_ROOT ${SANITIZED_ROOT})
	string(REGEX REPLACE "[^A-Za-z_0-9]" "_" SANITIZED_ROOT ${SANITIZED_ROOT})
	string(REGEX REPLACE "_+" "_" SANITIZED_ROOT ${SANITIZED_ROOT})
	set ("${SANITIZED_ROOT}" "${_ARGS_INSTALL_DIR}" PARENT_SCOPE)

	# Create and build a separate CMake project to carry out the installation.
	# If we've already previously done these steps, they will not cause
	# anything to be updated, so extra rebuilds of the project won't occur.
	# Make sure to pass through CMAKE_MAKE_PROGRAM in case the main project
	# has this set to something not findable on the PATH.
	string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_CMAKE_BUILD_TYPE)
	file(WRITE "${_ARGS_DOWNLOAD_DIR}/CMakeLists.txt"
"cmake_minimum_required(VERSION 3.2)
project(${_ARGS_NAME} NONE)
include (ExternalProject)
ExternalProject_Add(
	${_ARGS_NAME}
	PREFIX       \"${_ARGS_PREFIX}\"
	DOWNLOAD_DIR \"${_ARGS_DOWNLOAD_DIR}\"
	INSTALL_DIR  \"${_ARGS_INSTALL_DIR}\"
	CMAKE_ARGS
		\"-DCMAKE_INSTALL_PREFIX:PATH=${_ARGS_INSTALL_DIR}\"
		\"-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}\"
		\"-DCMAKE_CXX_FLAGS:STRING=${CMAKE_CXX_FLAGS}\"
		\"-DCMAKE_CXX_FLAGS_${UPPER_CMAKE_BUILD_TYPE}:STRING=${CMAKE_CXX_FLAGS_${UPPER_CMAKE_BUILD_TYPE}}\"
		\"${_ARGS_CMAKE_ARGS}\"
	${_ARGS_UNPARSED_ARGUMENTS}
)"
	)
	execute_process(
		COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
		-D "CMAKE_MAKE_PROGRAM:FILE=${CMAKE_MAKE_PROGRAM}"
		.
		RESULT_VARIABLE result
		${OUTPUT_QUIET}
		WORKING_DIRECTORY "${_ARGS_DOWNLOAD_DIR}"
	)
	if (result)
		message(FATAL_ERROR "CMake step for ${_ARGS_NAME} failed: ${result}")
	endif ()
	execute_process(
		COMMAND ${CMAKE_COMMAND} --build .
		RESULT_VARIABLE result
		${OUTPUT_QUIET}
		WORKING_DIRECTORY "${_ARGS_DOWNLOAD_DIR}"
	)
	if (result)
		message(FATAL_ERROR "Build step for ${_ARGS_NAME} failed: ${result}")
	endif ()
endfunction ()
