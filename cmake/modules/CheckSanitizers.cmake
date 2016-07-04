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


# Check for Address Sanitizer.
if (ASAN)
	if (TSAN OR MSAN)
		message (FATAL_ERROR "ASAN is not compatible with TSAN or MSAN")
	endif ()

	set (ASAN_FLAGS  "-fsanitize=address -fno-omit-frame-pointer")
	set (CMAKE_REQUIRED_FLAGS "${ASAN_FLAGS}")

	CHECK_CXX_COMPILER_FLAG("${ASAN_FLAGS}" COMPILER_SUPPORTS_ASAN)
	if (COMPILER_SUPPORTS_ASAN)
		message (STATUS "${READABLE_CXX_COMPILER} supports AddressSanitizer")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
	else ()
		message (WARNING "${READABLE_CXX_COMPILER} doesn't support AddressSanitizer")
	endif()
endif ()


# Check for Thread Sanitizer.
if (TSAN)
	if (MSAN)
		message (FATAL_ERROR "TSAN is not compatible with MSAN")
	endif ()

	set (TSAN_FLAGS  "-fsanitize=thread -fPIE -pie")
	set (CMAKE_REQUIRED_FLAGS "${TSAN_FLAGS}")

	CHECK_CXX_COMPILER_FLAG("${TSAN_FLAGS}" COMPILER_SUPPORTS_TSAN)
	if (COMPILER_SUPPORTS_TSAN)
		message (STATUS "${READABLE_CXX_COMPILER} supports ThreadSanitizer")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TSAN_FLAGS}")
	else ()
		message (WARNING "${READABLE_CXX_COMPILER} doesn't support ThreadSanitizer")
	endif()
endif ()


# Check for Memory Sanitizer.
if (MSAN)
	set (MSAN_FLAGS  "-fsanitize=memory -fsanitize-memory-track-origins -fPIE -pie -fno-omit-frame-pointer")
	set (CMAKE_REQUIRED_FLAGS "${MSAN_FLAGS}")

	CHECK_CXX_COMPILER_FLAG("${MSAN_FLAGS}" COMPILER_SUPPORTS_MSAN)
	if (COMPILER_SUPPORTS_MSAN)
		message (STATUS "${READABLE_CXX_COMPILER} supports MemorySanitizer")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MSAN_FLAGS}")
	else ()
		message (WARNING "${READABLE_CXX_COMPILER} doesn't support MemorySanitizer")
	endif()
endif ()


# Check for Undefined Behavior Sanitizer.
if (UBSAN)
	set (UBSAN_FLAGS  "-fsanitize=undefined -fno-omit-frame-pointer")
	set (CMAKE_REQUIRED_FLAGS "${UBSAN_FLAGS}")

	CHECK_CXX_COMPILER_FLAG("${UBSAN_FLAGS}" COMPILER_SUPPORTS_UBSAN)
	if (COMPILER_SUPPORTS_UBSAN)
		message (STATUS "${READABLE_CXX_COMPILER} supports UndefinedBehaviorSanitizer")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${UBSAN_FLAGS}")
	else ()
		message (WARNING "${READABLE_CXX_COMPILER} doesn't support UndefinedBehaviorSanitizer")
	endif()
endif ()
