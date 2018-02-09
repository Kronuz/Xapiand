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
# CMake module to find the ChaiScript include directory
# This module defines:
#  CHAISCRIPT_FOUND - if ChaiScript was found
#  CHAISCRIPT_INCLUDE_DIRS - where to find include directory
########################################################################


find_path(CHAISCRIPT_INCLUDE_DIR chaiscript/chaiscript.hpp
	$ENV{CHAISCRIPT_DIR}/include
	$ENV{CHAISCRIPT_DIR}
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


include(FindPackageHandleStandardArgs)


# handle the QUIETLY and REQUIRED arguments and set CHAISCRIPT_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(CHAISCRIPT "System ChaiScript library not found, using included one."
	CHAISCRIPT_INCLUDE_DIR)


mark_as_advanced(CHAISCRIPT_INCLUDE_DIR)


set(CHAISCRIPT_INCLUDE_DIRS ${CHAISCRIPT_INCLUDE_DIR})
