########################################################################
# Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to
#  deal in the Software without restriction, including without limitation the
#  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
#  sell copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
#  IN THE SOFTWARE.
########################################################################


########################################################################
# CMake module to find the native UUID library
# This module defines:
#  UUID_LIBRARIES - the libraries needed to use UUID
#  UUID_INCLUDE_DIRS - where to find UUID library
#  UUID_FOUND - If found UUID library
#
#  GUID_LIBUUID - If the OS is Linux or FreeBSD
#  GUID_CFUUID - If the OS is Darwin
########################################################################


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set (GUID_LIBUUID 1)
	set (UUID_NAME_LIB uuid)
	set (UUID_LIB_PATH uuid/uuid.h)
	set (UUID_ERR_MSG "UUID library (${UUID_LIB_PATH}) not found, You may need to install the uuid-dev or libuuid-devel package")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set (GUID_FREEBSD 1)
	set (UUID_NAME_LIB c++)
	set (UUID_LIB_PATH uuid.h)
	set (UUID_ERR_MSG "UUID library (${UUID_LIB_PATH}) not found, You may need to install the e2fsprogs-devel package")
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
	set (GUID_CFUUID 1)
	set (UUID_NAME_LIB CoreFoundation)
	set (UUID_LIB_PATH CoreFoundation/CFUUID.h)
	set (UUID_ERR_MSG "UUID library (${UUID_LIB_PATH}) not found")
else ()
	message (FATAL_ERROR "This module does not have support for ${CMAKE_SYSTEM_NAME}")
endif ()


find_package (PkgConfig REQUIRED)
pkg_check_modules (PC_UUID QUIET ${UUID_NAME_LIB})
set (UUID_DEFINITIONS ${PC_UUID_CFLAGS_OTHER})


find_path (UUID_INCLUDE_DIR ${UUID_LIB_PATH}
	HINTS ${PC_UUID_INCLUDEDIR} ${PC_UUID_INCLUDE_DIRS})


find_library (UUID_LIBRARY ${UUID_NAME_LIB}
	HINTS ${PC_UUID_LIBDIR} ${PC_UUID_LIBRARY_DIRS})


include(FindPackageHandleStandardArgs)


# handle the QUIETLY and REQUIRED arguments and set UUID_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(UUID ${UUID_ERR_MSG}
	UUID_LIBRARY UUID_INCLUDE_DIR)


mark_as_advanced (UUID_LIBRARY UUID_INCLUDE_DIR)


set (UUID_LIBRARIES ${UUID_LIBRARY})
set (UUID_INCLUDE_DIRS ${UUID_INCLUDE_DIR})
