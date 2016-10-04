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
# CMake module to find the V8 library
# This module defines:
#  V8_FOUND - if v8 was found
#  V8_LIBRARIES - the libraries needed to use v8
#  V8_INCLUDE_DIRS - where to find the headers
#
#  ??? - If the OS is Linux or FreeBSD
#  ??? - If the OS is Darwin
########################################################################


find_path (V8_INCLUDE_DIR v8.h
	$ENV{V8_DIR}/include
	$ENV{V8_DIR}
	~/Library/Frameworks
	/Library/Frameworks
	/usr/local/include
	/usr/include
	/sw/include # Fink
	/opt/local/include # DarwinPorts
	/opt/csw/include # Blastwave
	/opt/include
	/usr/freeware/include
	/devel
)


find_library (V8_LIBRARY
	NAMES v8
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
)

find_library (V8_BASE_LIBRARY
	NAMES v8_base libv8_base v8_base.ia32 v8_base.x64
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
)

find_library (V8_SNAPSHOT_LIBRARY
	NAMES v8_snapshot libv8_snapshot
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
)

find_library (V8_LIBBASE_LIBRARY
	NAMES v8_libbase libv8_libbase
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
)

find_library (V8_LIBPLATFORM_LIBRARY
	NAMES v8_libplatform libv8_libplatform
	PATHS
		$ENV{V8_DIR}
		$ENV{V8_DIR}/lib
		~/Library/Frameworks
		/Library/Frameworks
		/usr/local/lib
		/usr/lib
		/sw/lib
		/opt/local/lib
		/opt/csw/lib
		/opt/lib
		/usr/freeware/lib64
)


if (V8_INCLUDE_DIR AND V8_BASE_LIBRARY)
	set (V8_FOUND "YES")
	set (V8_INCLUDE_DIRS ${V8_INCLUDE_DIR})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_BASE_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_SNAPSHOT_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_LIBBASE_LIBRARY})
	set (V8_LIBRARIES ${V8_LIBRARIES} ${V8_LIBPLATFORM_LIBRARY})
elseif (V8_INCLUDE_DIR AND V8_LIBRARY)
	set (V8_FOUND "YES")
	set (V8_INCLUDE_DIRS ${V8_INCLUDE_DIR})
	set (V8_LIBRARIES ${V8_LIBRARY})
else ()
	set (V8_FOUND "NO")
endif ()
