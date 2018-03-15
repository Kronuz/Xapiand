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

########################################################################
# CMake module to find the V8 library
# This module defines:
#  V8_FOUND - if v8 was found
#  V8_LIBRARIES - the libraries needed to use v8
#  V8_INCLUDE_DIRS - where to find the headers
#
#  ??? - If the OS is Linux or FreeBSD
#  ??? - If the OS is Darwin
########################################################################


find_path(V8_INCLUDE_DIR v8.h
	$ENV{V8_DIR}/include
	$ENV{V8_DIR}
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


find_library(V8_LIBRARY
	NAMES v8
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
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

find_library(V8_BASE_LIBRARY
	NAMES v8_base libv8_base v8_base.ia32 v8_base.x64
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
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

find_library(V8_NOSNAPSHOT_LIBRARY
	NAMES v8_nosnapshot libv8_nosnapshot
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
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

find_library(V8_LIBBASE_LIBRARY
	NAMES v8_libbase libv8_libbase
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
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

find_library(V8_LIBPLATFORM_LIBRARY
	NAMES v8_libplatform libv8_libplatform
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
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

if (V8_INCLUDE_DIR AND V8_LIBRARY)
	set (V8_FOUND TRUE)
	set (V8_INCLUDE_DIRS ${V8_INCLUDE_DIR})
	set (V8_LIBRARIES ${V8_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_LIBBASE_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_LIBPLATFORM_LIBRARY})
	message(STATUS "Found v8: ${V8_INCLUDE_DIRS}")
elseif (V8_INCLUDE_DIR AND V8_BASE_LIBRARY)
	set (V8_FOUND TRUE)
	set (V8_INCLUDE_DIRS ${V8_INCLUDE_DIR})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_BASE_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_NOSNAPSHOT_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_LIBBASE_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_LIBPLATFORM_LIBRARY})
	message(STATUS "Found v8: ${V8_INCLUDE_DIRS}")
else ()
	set (V8_FOUND FALSE)
	if (V8_FIND_REQUIRED)
		message(FATAL_ERROR "V8 library not found.")
	endif ()
endif ()
