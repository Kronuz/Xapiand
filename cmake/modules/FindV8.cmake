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
#  V8_LIBRARY - the libraries needed to use v8
#  V8_FOUND - if v8 was found
#  V8_INCLUDE_DIR - where to find the headers
#
#  ??? - If the OS is Linux or FreeBSD
#  ??? - If the OS is Darwin
########################################################################


if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set (V8_DIR "/usr/local/opt/v8-315/") # TODO: Fix the path for linux
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set (V8_DIR "/usr/local/opt/v8-315/") # TODO: Fix the path for FreeBSD
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
   set (V8_DIR "/usr/local/opt/v8-315/") # TODO: Fix the path for Darwin
else ()
    message (FATAL_ERROR "This module does not have support for ${CMAKE_SYSTEM_NAME}")
endif ()


FIND_PATH(V8_INCLUDE_DIR v8.h
    ${V8_DIR}/include
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

# On non-Unix platforms (Mac and Windows specifically based on the forum),
# V8 builds separate shared (or at least linkable) libraries for v8_base and v8_snapshot
IF(NOT UNIX)
    FIND_LIBRARY(V8_BASE_LIBRARY
        NAMES v8_base v8_base.ia32 v8_base.x64 libv8_base
        PATHS
        ${V8_DIR}
        ${V8_DIR}/include
        ${V8_DIR}/lib
        ${V8_DIR}/build/Release/lib
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

    FIND_LIBRARY(V8_SNAPSHOT_LIBRARY
        NAMES v8_snapshot libv8_snapshot
        PATHS
        ${V8_DIR}
        ${V8_DIR}/lib
        ${V8_DIR}/build/Release/lib
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

# On Linux, there is just a libv8.so shared library built.
# (well, there are pseudo-static libraries libv8_base.a and libv8_snapshot.a
# but they don't seem to link correctly)
ELSE()
    FIND_LIBRARY(V8_LIBRARY
        NAMES v8
        PATHS
        ${V8_DIR}
        ${V8_DIR}/lib
        ${V8_DIR}/build/Release/lib
        # Having both architectures listed is problematic if both have been
        # built (which is the default)
        ${V8_DIR}/out/ia32.release/lib.target/
        ${V8_DIR}/out/x64.release/lib.target/
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

ENDIF(NOT UNIX)


SET(V8_FOUND "NO")
IF(NOT UNIX)
    IF(V8_BASE_LIBRARY AND V8_SNAPSHOT_LIBRARY AND V8_INCLUDE_DIR)
        SET(V8_FOUND "YES")
        SET (V8_MAJOR_VERSION 3)
        SET (V8_MINOR_VERSION 15)
    ENDIF(V8_BASE_LIBRARY AND V8_SNAPSHOT_LIBRARY AND V8_INCLUDE_DIR)
ELSEIF(V8_LIBRARY AND V8_INCLUDE_DIR)
    SET(V8_FOUND "YES")
    SET (V8_MAJOR_VERSION 3)
    SET (V8_MINOR_VERSION 15)
ENDIF(NOT UNIX)

