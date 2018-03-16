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

include (FindTools)
include (FindPackageHandleStandardArgs)

set (GBENCHMARK_ROOT "${CMAKE_CURRENT_BINARY_DIR}/googlebenchmark")


macro (__gbenchmark_find)
	FindTools_find_include(GBENCHMARK_INCLUDE_DIR     GBENCHMARK_ROOT  "benchmark/benchmark.h")
	FindTools_find_library(GBENCHMARK_LIBRARY         GBENCHMARK_ROOT  "benchmark")
	FindTools_find_library(GBENCHMARK_LIBRARY_DEBUG   GBENCHMARK_ROOT  "benchmarkd")
endmacro ()


__gbenchmark_find()
if (NOT GBENCHMARK_INCLUDE_DIR OR NOT GBENCHMARK_LIBRARY)
	include (ExternalProjectInstall)
	ExternalProject_Install(
		gbenchmark
		PREFIX          "googlebenchmark"
		GIT_REPOSITORY  "https://github.com/google/benchmark.git"
		GIT_TAG         "master"
		INSTALL_DIR     "${GBENCHMARK_ROOT}"
		UPDATE_DISCONNECTED ON
		CMAKE_ARGS
			"-DBENCHMARK_ENABLE_TESTING:BOOL=OFF"
		QUIET
	)
	__gbenchmark_find()
endif ()


find_package_handle_standard_args(GBenchmark DEFAULT_MSG GBENCHMARK_INCLUDE_DIR GBENCHMARK_LIBRARY)
if (GBENCHMARK_FOUND)
	set (GBENCHMARK_INCLUDE_DIRS ${GBENCHMARK_INCLUDE_DIR})
	FindTools_append_debugs(GBENCHMARK_LIBRARIES GBENCHMARK_LIBRARY)

	if (NOT TARGET GBenchmark::GBenchmark)
    	FindTools_add_library(GBenchmark::GBenchmark GBENCHMARK)
	endif ()
endif ()
