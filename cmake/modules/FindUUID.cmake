#
# Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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

########################################################################
# CMake module to find the native UUID library
# This module defines:
#  UUID_LIBRARIES - the libraries needed to use UUID
#  UUID_INCLUDE_DIRS - where to find UUID library
#  UUID_FOUND - If found UUID library
#
#  UUID_LIBUUID - If the OS is Linux or FreeBSD
#  UUID_CFUUID - If the OS is Darwin
########################################################################


if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set (UUID_FREEBSD 1)
	set (UUID_LIB_PATH uuid.h)
	set (UUID_ERR_MSG "UUID library(${UUID_LIB_PATH}) not found, You may need to install the e2fsprogs-devel package")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set (UUID_LIBUUID 1)
	set (UUID_LIB_PATH uuid/uuid.h)
	# set (UUID_CFUUID 1)
	# set (UUID_NAME_LIB CoreFoundation)
	# set (UUID_LIB_PATH CoreFoundation/CFUUID.h)
	# set (UUID_ERR_MSG "UUID library(${UUID_LIB_PATH}) not found")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set (UUID_LIBUUID 1)
	set (UUID_NAME_LIB uuid)
	set (UUID_LIB_PATH uuid/uuid.h)
	set (UUID_ERR_MSG "UUID library(${UUID_LIB_PATH}) not found, You may need to install the uuid-dev or libuuid-devel package")
else ()
	message(FATAL_ERROR "This module does not have support for ${CMAKE_SYSTEM_NAME}")
endif ()


find_path(UUID_INCLUDE_DIR ${UUID_LIB_PATH}
	$ENV{UUID_DIR}/include
	$ENV{UUID_DIR}
	/usr/local/include
	/usr/include
	/sw/include # Fink
	/opt/local/include # DarwinPorts
	/opt/csw/include # Blastwave
	/opt/include
	/usr/freeware/include
	/devel
	~/Library/Frameworks
	/Library/Frameworks
)

if (UUID_NAME_LIB)
	find_library(UUID_LIBRARY ${UUID_NAME_LIB}
		$ENV{UUID_DIR}
		$ENV{UUID_DIR}/lib
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
		~/Library/Frameworks
		/Library/Frameworks
	)

	# handle the QUIETLY and REQUIRED arguments and set UUID_FOUND to TRUE
	# if all listed variables are TRUE
	include(FindPackageHandleStandardArgs)
	find_package_handle_standard_args(UUID ${UUID_ERR_MSG}
		UUID_LIBRARY UUID_INCLUDE_DIR)

	find_package(PkgConfig REQUIRED)
	pkg_check_modules(PC_UUID QUIET ${UUID_NAME_LIB})
	set (UUID_DEFINITIONS ${PC_UUID_CFLAGS_OTHER})
else ()
	set (UUID_LIBRARY "")
endif ()

mark_as_advanced(UUID_LIBRARY UUID_INCLUDE_DIR)

set (UUID_LIBRARIES ${UUID_LIBRARY})
set (UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
