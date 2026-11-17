#
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

# Based on official CMake's FindGTest.cmake, whose license follows.
#
# Copyright 2000-2026 Kitware, Inc. and Contributors
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# * Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# * Neither the name of Kitware, Inc. nor the names of Contributors
#   may be used to endorse or promote products derived from this
#   software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

include (FindPackageTools)
include (FindPackageHandleStandardArgs)

set (GTEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/googletest")


macro (__gtest__find)
	find_package_tools_find_include(GTEST_INCLUDE_DIR         GTEST_ROOT  "gtest/gtest.h")
	find_package_tools_find_library(GTEST_LIBRARY             GTEST_ROOT  "gtest")
	find_package_tools_find_library(GTEST_LIBRARY_DEBUG       GTEST_ROOT  "gtestd")
	find_package_tools_find_library(GTEST_MAIN_LIBRARY        GTEST_ROOT  "gtest_main")
	find_package_tools_find_library(GTEST_MAIN_LIBRARY_DEBUG  GTEST_ROOT  "gtest_maind")

	find_package_tools_find_include(GMOCK_INCLUDE_DIR         GTEST_ROOT  "gmock/gmock.h")
	find_package_tools_find_library(GMOCK_LIBRARY             GTEST_ROOT  "gmock")
	find_package_tools_find_library(GMOCK_LIBRARY_DEBUG       GTEST_ROOT  "gmockd")
	find_package_tools_find_library(GMOCK_MAIN_LIBRARY        GTEST_ROOT  "gmock_main")
	find_package_tools_find_library(GMOCK_MAIN_LIBRARY_DEBUG  GTEST_ROOT  "gmock_maind")
endmacro ()


__gtest__find()
if (NOT GTEST_INCLUDE_DIR OR NOT GTEST_LIBRARY OR NOT GTEST_MAIN_LIBRARY)
	include (ExternalProjectInstall)
	message(STATUS "Installing external project: GTest")
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
	find_package_tools_append_debugs(GTEST_LIBRARIES      GTEST_LIBRARY)
	find_package_tools_append_debugs(GTEST_MAIN_LIBRARIES GTEST_MAIN_LIBRARY)
	set (GTEST_BOTH_LIBRARIES ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES})

	if (NOT TARGET GTest::GTest)
		find_package_tools_add_library(GTest::GTest GTEST)
		find_package(Threads QUIET)
		if (TARGET Threads::Threads)
			set_target_properties(GTest::GTest PROPERTIES
				INTERFACE_LINK_LIBRARIES Threads::Threads)
		endif ()
	endif ()

	if (NOT TARGET GTest::Main)
		find_package_tools_add_library(GTest::Main GTEST_MAIN)
	endif ()
endif ()


find_package_handle_standard_args(GMock DEFAULT_MSG GMOCK_INCLUDE_DIR GMOCK_LIBRARY GMOCK_MAIN_LIBRARY)
if (GMOCK_FOUND)
	set (GMOCK_INCLUDE_DIRS ${GMOCK_INCLUDE_DIR})
	find_package_tools_append_debugs(GMOCK_LIBRARIES      GMOCK_LIBRARY)
	find_package_tools_append_debugs(GMOCK_MAIN_LIBRARIES GMOCK_MAIN_LIBRARY)
	set (GMOCK_BOTH_LIBRARIES ${GMOCK_LIBRARIES} ${GMOCK_MAIN_LIBRARIES})

	if (NOT TARGET GMock::GMock)
		find_package_tools_add_library(GMock::GMock GMOCK)
		find_package(Threads QUIET)
		if (TARGET Threads::Threads)
			set_target_properties(GMock::GMock PROPERTIES
				INTERFACE_LINK_LIBRARIES Threads::Threads)
		endif ()
	endif ()

	if (NOT TARGET GMock::Main)
		find_package_tools_add_library(GMock::Main GMOCK_MAIN)
	endif ()
endif ()
