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
# CMake module to enable sanitizers for compiling and linking sources.
#
# The sanitizers are disabled by default. You can enable it by specifying the
# -D{ASAN, TSAN, MSAN or UBSAN}=ON flag when running cmake.
########################################################################


option (ASAN "Enable AddressSanitizer (aka ASAN)" OFF)
option (TSAN "Enable ThreadSanitizer (aka TSAN)" OFF)
option (MSAN "Enable MemorySanitizer (aka MSAN)" OFF)
option (UBSAN "Enable UndefinedBehaviorSanitizer (aka UBSAN)" OFF)


function (check_sanitizer_flags SAN_DESCRIPTION SAN_FLAGS)
	include (CheckCXXCompilerFlag)
	set (READABLE_CXX_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

	set (CMAKE_REQUIRED_FLAGS_CACHE ${CMAKE_REQUIRED_FLAGS})
	set (CMAKE_REQUIRED_FLAGS "${SAN_FLAGS}")
	CHECK_CXX_COMPILER_FLAG("${SAN_FLAGS}" COMPILER_SUPPORTS_FLAGS)
	set (CMAKE_REQUIRED_FLAGS ${CMAKE_REQUIRED_FLAGS_CACHE})

	if (COMPILER_SUPPORTS_FLAGS)
		message (STATUS "${READABLE_CXX_COMPILER} supports ${SAN_DESCRIPTION}")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SAN_FLAGS}" PARENT_SCOPE)
	else ()
		message (WARNING "${READABLE_CXX_COMPILER} doesn't support ${SAN_DESCRIPTION}")
	endif()
endfunction ()


# Check for Address Sanitizer.
if (ASAN)
	if (TSAN OR MSAN)
		message (FATAL_ERROR "ASAN is not compatible with TSAN or MSAN")
	endif ()

	set (ASAN_DESCRIPTION "AddressSanitizer")
	set (ASAN_FLAGS  "-g -O2 -fsanitize=address -fno-omit-frame-pointer")
	check_sanitizer_flags (${ASAN_DESCRIPTION} ${ASAN_FLAGS})
endif ()


# Check for Thread Sanitizer.
if (TSAN)
	if (MSAN)
		message (FATAL_ERROR "TSAN is not compatible with MSAN")
	endif ()

	set (TSAN_DESCRIPTION "ThreadSanitizer")
	set (TSAN_FLAGS  "-g -fsanitize=thread")
	check_sanitizer_flags (${TSAN_DESCRIPTION} ${TSAN_FLAGS})
endif ()


# Check for Memory Sanitizer.
if (MSAN)
	set (MSAN_DESCRIPTION "MemorySanitizer")
	set (MSAN_FLAGS  "-g -fsanitize=memory -fsanitize-memory-track-origins -fno-omit-frame-pointer")
	check_sanitizer_flags (${MSAN_DESCRIPTION} ${MSAN_FLAGS})
endif ()


# Check for Undefined Behavior Sanitizer.
if (UBSAN)
	set (UBSAN_DESCRIPTION "UndefinedBehaviorSanitizer")
	set (UBSAN_FLAGS  "-g -fsanitize=undefined -fno-omit-frame-pointer")
	check_sanitizer_flags (${UBSAN_DESCRIPTION} ${UBSAN_FLAGS})
endif ()
