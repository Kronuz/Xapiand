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

set (GTEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/googletest")


macro (__gtest__find)
	FindTools_find_include(GTEST_INCLUDE_DIR         GTEST_ROOT  "gtest/gtest.h")
	FindTools_find_library(GTEST_LIBRARY             GTEST_ROOT  "gtest")
	FindTools_find_library(GTEST_LIBRARY_DEBUG       GTEST_ROOT  "gtestd")
	FindTools_find_library(GTEST_MAIN_LIBRARY        GTEST_ROOT  "gtest_main")
	FindTools_find_library(GTEST_MAIN_LIBRARY_DEBUG  GTEST_ROOT  "gtest_maind")

	FindTools_find_include(GMOCK_INCLUDE_DIR         GTEST_ROOT  "gmock/gmock.h")
	FindTools_find_library(GMOCK_LIBRARY             GTEST_ROOT  "gmock")
	FindTools_find_library(GMOCK_LIBRARY_DEBUG       GTEST_ROOT  "gmockd")
	FindTools_find_library(GMOCK_MAIN_LIBRARY        GTEST_ROOT  "gmock_main")
	FindTools_find_library(GMOCK_MAIN_LIBRARY_DEBUG  GTEST_ROOT  "gmock_maind")
endmacro ()


__gtest__find()
if (NOT GTEST_INCLUDE_DIR OR NOT GTEST_LIBRARY OR NOT GTEST_MAIN_LIBRARY)
	include (ExternalProjectInstall)
	ExternalProject_Install(
		gtest
		PREFIX          "googletest"
		GIT_REPOSITORY  "https://github.com/google/googletest.git"
		GIT_TAG         "master"
		INSTALL_DIR     "${GTEST_ROOT}"
		UPDATE_DISCONNECTED ON
		QUIET
	)
	__gtest__find()
endif ()


find_package_handle_standard_args(GTest DEFAULT_MSG GTEST_INCLUDE_DIR GTEST_LIBRARY GTEST_MAIN_LIBRARY)
if (GTEST_FOUND)
	set (GTEST_INCLUDE_DIRS ${GTEST_INCLUDE_DIR})
	FindTools_append_debugs(GTEST_LIBRARIES      GTEST_LIBRARY)
	FindTools_append_debugs(GTEST_MAIN_LIBRARIES GTEST_MAIN_LIBRARY)
	set (GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})

	if (NOT TARGET GTest::GTest)
    	FindTools_add_library(GTest::GTest GTEST)
		find_package(Threads QUIET)
		if (TARGET Threads::Threads)
			set_target_properties(GTest::GTest PROPERTIES
				INTERFACE_LINK_LIBRARIES Threads::Threads)
		endif ()
	endif ()

	if (NOT TARGET GTest::Main)
    	FindTools_add_library(GTest::Main GTEST_MAIN)
	endif ()
endif ()


find_package_handle_standard_args(GMock DEFAULT_MSG GMOCK_INCLUDE_DIR GMOCK_LIBRARY GMOCK_MAIN_LIBRARY)
if (GMOCK_FOUND)
	set (GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIR})
	FindTools_append_debugs(GMOCK_LIBRARIES      GMOCK_LIBRARY)
	FindTools_append_debugs(GMOCK_MAIN_LIBRARIES GMOCK_MAIN_LIBRARY)
	set (GMOCK_BOTH_LIBRARIES ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES})

    if (NOT TARGET GMock::GMock)
    	FindTools_add_library(GMock::GMock GMOCK)
    	find_package(Threads QUIET)
        if (TARGET Threads::Threads)
            set_target_properties(GMock::GMock PROPERTIES
                INTERFACE_LINK_LIBRARIES Threads::Threads)
        endif ()
    endif ()

    if (NOT TARGET GMock::Main)
    	FindTools_add_library(GMock::Main GMOCK_MAIN)
    endif ()
endif ()
