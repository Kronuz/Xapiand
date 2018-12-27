#
# Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

cmake_minimum_required(VERSION 3.5)
project(xapiand C CXX)

# Make sure we can import out CMake functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set (PACKAGE_NAME "Xapiand")
set (PACKAGE_VERSION "0.8.8")
set (PACKAGE_URL "https://github.com/Kronuz/Xapiand")
set (PACKAGE_BUGREPORT "https://github.com/Kronuz/Xapiand/issues")
set (PACKAGE_TARNAME "${PROJECT_NAME}")


########################################################################
# Options
########################################################################

option(LINK_WHAT_YOU_USE    "Enable link-what-you-use"              OFF)
option(INCLUDE_WHAT_YOU_USE "Enable include-what-you-use"           OFF)
option(CLANG_TIDY           "Enable Clang Tidy"                     OFF)
option(CPPLINT              "Enable CppLint"                        OFF)

option(BUILD_TESTS "Build all tests"                                OFF)
option(BUILD_BENCHMARKS "Build all benchmarks"                      OFF)
option(DEBUGINFO_EXTERNALIZE "Externalize debug information"        OFF)

option(BINARY_PROXY  "Define a proxy port for binary protocol"      OFF)
option(CLUSTERING    "Enable remote clustering"                      ON)
option(DATABASE_WAL  "Enable database write ahead log (WAL)"         ON)
option(DATA_STORAGE  "Enable data storage"                           ON)
option(TRACEBACKS    "Enable tracebacks (on by default in Debug)"   OFF)
option(ASSERTS       "Enable asserts (on by default in Debug)"      OFF)
option(TRACKED_MEM   "Enable tracked memory allocator"              OFF)
option(CHECK_IO_FDES "Check file descriptors"                       OFF)
option(V8            "Enable v8 engine"                             OFF)
option(CHAISCRIPT    "Enable ChaiScript engine"                      ON)
option(UUID_ENCODED  "Allow encoded (base59) as UUID encoding"       ON)
option(UUID_GUID     "Allow GUID as UUID encoding"                  OFF)
option(UUID_URN      "Allow URN as UUID encoding"                   OFF)
option(RANDOM_ERRORS "Allow adding randomized errors"               OFF)
option(CHECK_SIZES   "Print the objects sizes"                      OFF)


foreach (opt BINARY_PROXY CLUSTERING DATABASE_WAL DATA_STORAGE TRACEBACKS ASSERTS CHECK_IO_FDES TRACKED_MEM V8 CHAISCRIPT UUID_ENCODED UUID_GUID UUID_URN RANDOM_ERRORS)
	if (${opt})
		set ("XAPIAND_${opt}" 1)
		message(STATUS "Option ${opt} - on")
	else ()
		set ("XAPIAND_${opt}" 0)
	endif ()
endforeach ()

if (APPLE)
	set (CMAKE_FIND_FRAMEWORK "LAST")
endif ()


########################################################################
# Check CMAKE_BUILD_TYPE:  Debug Release RelWithDebInfo MinSizeRel
########################################################################

# Default value for CMAKE_BUILD_TYPE.
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()
string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_CMAKE_BUILD_TYPE)

# set (CMAKE_CXX_FLAGS_DEBUG "-g")
# set (CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
# set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
# set (CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")


########################################################################
# CMake Includes
########################################################################

include (AC_HEADER_STDC)
include (AddCXXCompilerFlag)
include (CheckCXXSourceCompiles)
include (CheckFunctionExists)
include (CheckIncludeFileCXX)
include (CheckIncludeFiles)
include (CheckLibraryExists)
include (CheckTypeSize)
include (CMakePushCheckState)
include (GetGitVersion)
include (GNUInstallDirs)
include (RemoveDuplicateSubstrings)



########################################################################
#  _____           _
# |_   _|__   ___ | |___
#   | |/ _ \ / _ \| / __|
#   | | (_) | (_) | \__ \
#   |_|\___/ \___/|_|___/
#
########################################################################

# -DCMAKE_CMAKE_LINK_WHAT_YOU_USE:BOOL=ON

if (LINK_WHAT_YOU_USE)
	message(STATUS "Enabling link-what-you-use - ok")
	set (CMAKE_LINK_WHAT_YOU_USE ON)
endif (LINK_WHAT_YOU_USE)


# Enable Google's Include What You Use
# ------------------------------------
# -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE:STRING="include-what-you-use;-Xiwyu;--mapping_file=../iwyu.imp"

if (INCLUDE_WHAT_YOU_USE)
	find_program(INCLUDE_WHAT_YOU_USE_PATH NAMES include-what-you-use iwyu)
	if (INCLUDE_WHAT_YOU_USE_PATH)
		message(STATUS "Looking for include-what-you-use - found")
		set (INCLUDE_WHAT_YOU_USE_OPTS
			${INCLUDE_WHAT_YOU_USE_PATH}
			"-Xiwyu"
			"--mapping_file=${PROJECT_SOURCE_DIR}/iwyu.imp"
		)
		set (CMAKE_C_INCLUDE_WHAT_YOU_USE "${INCLUDE_WHAT_YOU_USE_OPTS}")
		set (CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${INCLUDE_WHAT_YOU_USE_OPTS}")
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ferror-limit=0")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=0")
	else ()
		message(STATUS "Looking for include-what-you-use - not found")
	endif ()
endif (INCLUDE_WHAT_YOU_USE)


# Enable Clang Tidy
# -----------------
# -DCMAKE_CXX_CLANG_TIDY:STRING="clang-tidy;-checks=-*,readability-*"

if (CLANG_TIDY)
	find_program(CLANG_TIDY_PATH NAMES clang-tidy)
	if (CLANG_TIDY_PATH)
		message(STATUS "Looking for clang-tidy - found")
		set (CLANG_TIDY_OPTS
			${CLANG_TIDY_PATH}
			# "-fix"
			"-checks=-*,readability-*,modernize-*"
		)
		set (CMAKE_C_CLANG_TIDY "${CLANG_TIDY_OPTS}")
		set (CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_OPTS}")
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ferror-limit=0")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=0")
	else ()
		message(STATUS "Looking for clang-tidy - not found")
	endif ()
endif (CLANG_TIDY)


# Enable CppLint
# --------------
# -DCMAKE_CXX_CPPLINT:STRING="cpplint;--linelength=79"

if (CPPLINT)
	find_program(CPPLINT_PATH NAMES cpplint)
	if (CPPLINT_PATH)
		message(STATUS "Looking for CppLint - found")
		set (CPPLINT_OPTS
			${CPPLINT_PATH}
			"--linelength=79"
		)
		set (CMAKE_C_CPPLINT "${CPPLINT_OPTS}")
		set (CMAKE_CXX_CPPLINT "${CPPLINT_OPTS}")
		set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ferror-limit=0")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ferror-limit=0")
	else ()
		message(STATUS "Looking for CppLint - not found")
	endif ()
endif (CPPLINT)



########################################################################
#  _____                    _
# |_   _|_ _ _ __ __ _  ___| |_ ___
#   | |/ _` | '__/ _` |/ _ \ __/ __|
#   | | (_| | | | (_| |  __/ |_\__ \
#   |_|\__,_|_|  \__, |\___|\__|___/
#                |___/
########################################################################

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

set (FIXTURES_PATH "${PROJECT_SOURCE_DIR}/oldtests")
set (STOPWORDS_PATH "${CMAKE_INSTALL_FULL_DATAROOTDIR}/xapiand/stopwords")


include_directories(
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/src")


########################################################################
# Boolean parser library target
########################################################################


file(GLOB BOOLEAN_PARSER_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/booleanParser/*.c*" "${PROJECT_SOURCE_DIR}/src/booleanParser/*.h"
)
add_library(BOOLEAN_PARSER_OBJ OBJECT ${BOOLEAN_PARSER_SRC_LIST})


########################################################################
# Libev library target
########################################################################


file(GLOB LIBEV_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/ev/*.c*" "${PROJECT_SOURCE_DIR}/src/ev/*.h"
)
list(REMOVE_ITEM LIBEV_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/ev/ev_epoll.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_kqueue.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_poll.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_port.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_select.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_win32.c"
)
add_library(LIBEV_OBJ OBJECT ${LIBEV_SRC_LIST})


########################################################################
# LZ4 library target
########################################################################


file(GLOB LZ4_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/lz4/*.c*" "${PROJECT_SOURCE_DIR}/src/lz4/*.h"
)
add_library(LZ4_OBJ OBJECT ${LZ4_SRC_LIST})


########################################################################
# Condensed UUID library target
########################################################################


file(GLOB UUID_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/cuuid/*.c*" "${PROJECT_SOURCE_DIR}/src/cuuid/*.h"
)
add_library(UUID_OBJ OBJECT ${UUID_SRC_LIST})


########################################################################
# Prometheus library target
########################################################################


file(GLOB PROMETHEUS_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/prometheus/*.c*" "${PROJECT_SOURCE_DIR}/src/prometheus/*.h"
)
add_library(PROMETHEUS_OBJ OBJECT ${PROMETHEUS_SRC_LIST})


########################################################################
# Xapiand package configuration target
########################################################################


add_library(PACKAGE_OBJ OBJECT "${PROJECT_SOURCE_DIR}/src/package.cc")


########################################################################
# Xapiand main() target
########################################################################


add_library(MAIN_OBJ OBJECT "${PROJECT_SOURCE_DIR}/src/xapiand.cc")


########################################################################
# Xapiand library target
########################################################################


file(GLOB XAPIAND_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/*.c*" "${PROJECT_SOURCE_DIR}/src/*.h"
	"${PROJECT_SOURCE_DIR}/src/fmt/*.c*" "${PROJECT_SOURCE_DIR}/src/fmt/*.h"
	"${PROJECT_SOURCE_DIR}/src/geospatial/*.c*" "${PROJECT_SOURCE_DIR}/src/geo/*.h"
	"${PROJECT_SOURCE_DIR}/src/hash/*.c*" "${PROJECT_SOURCE_DIR}/src/hash/*.h"
	"${PROJECT_SOURCE_DIR}/src/multivalue/*.c*" "${PROJECT_SOURCE_DIR}/src/multivalue/*.h"
	"${PROJECT_SOURCE_DIR}/src/server/*.c*" "${PROJECT_SOURCE_DIR}/src/server/*.h"
	"${PROJECT_SOURCE_DIR}/src/xchange/*.c*" "${PROJECT_SOURCE_DIR}/src/xchange/*.h"
)
list(REMOVE_ITEM XAPIAND_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/package.cc"
	"${PROJECT_SOURCE_DIR}/src/xapiand.cc"
)
add_library(XAPIAND_OBJ OBJECT ${XAPIAND_SRC_LIST})


########################################################################
# Primary target
########################################################################

add_executable(${PROJECT_NAME}
	"$<TARGET_OBJECTS:MAIN_OBJ>"
	"$<TARGET_OBJECTS:PACKAGE_OBJ>"
	"$<TARGET_OBJECTS:XAPIAND_OBJ>"
	"$<TARGET_OBJECTS:BOOLEAN_PARSER_OBJ>"
	"$<TARGET_OBJECTS:LIBEV_OBJ>"
	"$<TARGET_OBJECTS:LZ4_OBJ>"
	"$<TARGET_OBJECTS:UUID_OBJ>"
	"$<TARGET_OBJECTS:PROMETHEUS_OBJ>"
)


########################################################################
# Copy stopwords from source to build directory
########################################################################

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/stopwords/ ${CMAKE_INSTALL_DATAROOTDIR}/xapiand/stopwords)


########################################################################
# Install target
########################################################################

# Copy stopwords during the installation
install(DIRECTORY ${CMAKE_SOURCE_DIR}/stopwords/ DESTINATION ${STOPWORDS_PATH})

# Manual page generation
install(FILES ${CMAKE_SOURCE_DIR}/man/xapiand.8 DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 COMPONENT doc)

# Install binary
install(TARGETS ${PROJECT_NAME} DESTINATION bin)


########################################################################
# Uninstall target
########################################################################

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
	"${PROJECT_BINARY_DIR}/cmake_uninstall.cmake"
	@ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${PROJECT_BINARY_DIR}/cmake_uninstall.cmake)



########################################################################
#  _     _ _                    _
# | |   (_) |__  _ __ __ _ _ __(_) ___  ___
# | |   | | '_ \| '__/ _` | '__| |/ _ \/ __|
# | |___| | |_) | | | (_| | |  | |  __/\__ \
# |_____|_|_.__/|_|  \__,_|_|  |_|\___||___/
#
########################################################################


########################################################################
# ZLIB Library
########################################################################

find_package(ZLIB)
if (ZLIB_FOUND)
	set (HAVE_ZLIB 1)
	target_include_directories(${PROJECT_NAME} BEFORE PRIVATE "${ZLIB_INCLUDE_DIRS}")
	target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
else ()
	set (HAVE_ZLIB 0)
endif ()


########################################################################
if (V8)
	# V8 Engine Library
	####################################################################
	find_package(V8 REQUIRED)
	target_include_directories(${PROJECT_NAME} BEFORE PRIVATE "${V8_INCLUDE_DIRS}")
	target_link_libraries(${PROJECT_NAME} PRIVATE ${V8_LIBRARIES})

elseif (CHAISCRIPT)
	# ChaiScript Engine Library
	####################################################################
	find_package(CHAISCRIPT)
	if (CHAISCRIPT_FOUND)
		target_include_directories(${PROJECT_NAME} BEFORE PRIVATE "${CHAISCRIPT_INCLUDE_DIRS}")
	endif ()
endif ()


########################################################################
# Exec Info Library
########################################################################

find_library(EXECINFO_LIBRARIES execinfo)
if (EXECINFO_LIBRARIES)
	set (HAVE_EXECINFO 1)
	target_link_libraries(${PROJECT_NAME} PRIVATE ${EXECINFO_LIBRARIES})
else ()
	set (HAVE_EXECINFO 0)
endif ()


########################################################################
# Real Time Library
########################################################################

find_library(RT_LIBRARIES rt)
if (RT_LIBRARIES)
	set (HAVE_RT 1)
	target_link_libraries(${PROJECT_NAME} PRIVATE ${RT_LIBRARIES})
else ()
	set (HAVE_RT 0)
endif ()


########################################################################
# Math Library
########################################################################

find_library(M_LIBRARIES m)
if (M_LIBRARIES)
	set (HAVE_M 1)
	target_link_libraries(${PROJECT_NAME} PRIVATE ${M_LIBRARIES})
else ()
	set (HAVE_M 0)
	set (M_LIBRARIES "")
endif ()


########################################################################
# Threads Library
########################################################################

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CMAKE_THREAD_LIBS_INIT})

if (CMAKE_USE_PTHREADS_INIT)
	set (HAVE_PTHREADS 1)
else ()
	set (HAVE_PTHREADS 0)
endif ()


########################################################################
# UUID Library
########################################################################

find_package(UUID REQUIRED)
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE "${UUID_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} PRIVATE ${UUID_LIBRARIES})


########################################################################
# Xapian Library
########################################################################

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DXAPIAN_MOVE_SEMANTICS")

find_package(Xapian REQUIRED)
include_directories(BEFORE "${XAPIAN_INCLUDE_DIR}") # <xapian.h> is imported everywhere (for exceptions)
target_link_libraries(${PROJECT_NAME} PRIVATE ${XAPIAN_LIBRARIES})

cmake_push_check_state()
set (CMAKE_REQUIRED_INCLUDES "${XAPIAN_INCLUDE_DIR}")
set (CMAKE_REQUIRED_LIBRARIES "${XAPIAN_LIBRARIES}")
check_cxx_source_compiles("
	#include <xapian.h>
	int main() {
		Xapian::MSet mset;
		mset.serialise();
	}" HAVE_XAPIAN_REMOTE_PROTOCOL)
check_cxx_source_compiles("
	#include <xapian.h>
	int main() {
		Xapian::Database db;
		db.get_revision();
	}" HAVE_XAPIAN_DATABASE_GET_REVISION)
check_cxx_source_compiles("
	#include <xapian.h>
	int main() {
		Xapian::Database db;
		db.get_document(0, Xapian::DOC_ASSUME_VALID);
	}" HAVE_XAPIAN_DATABASE_GET_DOCUMENT_WITH_FLAGS)
cmake_pop_check_state()

if (CLUSTERING AND HAVE_XAPIAN_REMOTE_PROTOCOL)
	set ("XAPIAND_CLUSTERING" 1)
else ()
	set ("XAPIAND_CLUSTERING" 0)
endif ()



########################################################################
#  _____ _
# |  ___| | __ _  __ _ ___
# | |_  | |/ _` |/ _` / __|
# |  _| | | (_| | (_| \__ \
# |_|   |_|\__,_|\__, |___/
#                |___/
########################################################################


########################################################################
# Version
########################################################################

# Read the git tags to determine the project version
get_git_revision(GIT_REVISION)
if (GIT_REVISION)
	set (PACKAGE_REVISION "${GIT_REVISION}")
else ()
	get_date_revision(DATE_REVISION)
	if (DATE_REVISION)
		set (PACKAGE_REVISION "${DATE_REVISION}")
	endif ()
endif ()

get_git_hash(GIT_HASH)
if (GIT_HASH)
	set (PACKAGE_HASH "${GIT_HASH}")
endif ()

get_git_version(GIT_VERSION)
if (GIT_VERSION)
	set (PACKAGE_VERSION "${GIT_VERSION}")
endif ()


########################################################################
# Compiler Flags
########################################################################
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")

# Setup CMAKE_CXX_FLAGS Debug flags.
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG -fstack-protector-strong")

# Add compiler flags.
add_required_cxx_compiler_flag("-std=c++1z")

add_cxx_compiler_flag("-fdiagnostics-color=always")

add_cxx_compiler_flag("-Wall")
add_cxx_compiler_flag("-Wextra")
add_cxx_compiler_flag("-Wshadow")
# add_cxx_compiler_flag("-Wshorten-64-to-32") ##
# add_cxx_compiler_flag("-Wfloat-equal") ##

# Disable certain warnings.
add_cxx_compiler_flag("-Wnon-virtual-dtor")
add_cxx_compiler_flag("-Wno-unknown-pragmas")
add_cxx_compiler_flag("-Wno-variadic-macros")

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	# using Clang
	add_cxx_compiler_flag("-pedantic")
	add_cxx_compiler_flag("-pedantic-errors")
	add_cxx_compiler_flag("-Werror" RELEASE)
	add_cxx_compiler_flag("-Werror" RELWITHDEBINFO)
	add_cxx_compiler_flag("-Werror" MINSIZEREL)
	add_cxx_compiler_flag("-Wstrict-aliasing")
	add_cxx_compiler_flag("-Wsuggest-final-types")
	add_cxx_compiler_flag("-Wno-gnu-zero-variadic-macro-arguments")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	# using GCC
	add_cxx_compiler_flag("-Wno-shadow")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Intel")
	# using Intel C++
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
	# using Visual Studio C++
endif()


########################################################################
# Xapiand Flags
########################################################################

check_include_files("pthread_np.h" HAVE_PTHREAD_NP_H)
check_include_files("fcntl.h" HAVE_FCNTL_H)
check_include_files("limits.h" HAVE_LIMITS_H)
check_include_files("netinet/in.h" HAVE_NETINET_IN_H)
check_include_files("sys/socket.h" HAVE_SYS_SOCKET_H)
check_include_files("sys/time.h" HAVE_SYS_TIME_H)
check_include_files("unistd.h" HAVE_UNISTD_H)
check_include_files("execinfo.h" HAVE_EXECINFO_H)
check_include_files("libunwind.h" HAVE_LIBUNWIND_H)
check_include_files("sys/sysctl.h" HAVE_SYS_SYSCTL_H)
check_include_files("sys/capability.h" HAVE_SYS_CAPABILITY_H)

check_include_file_cxx("sstream" HAVE_SSTREAM)
check_include_file_cxx("strstream" HAVE_STRSTREAM)

check_function_exists("fallocate" HAVE_FALLOCATE)
check_function_exists("fsync" HAVE_FSYNC)
check_function_exists("getcwd" HAVE_GETCWD)
check_function_exists("gettimeofday" HAVE_GETTIMEOFDAY)
check_function_exists("memcpy" HAVE_MEMCPY)
check_function_exists("posix_fadvise" HAVE_POSIX_FADVISE)
check_function_exists("posix_fallocate" HAVE_POSIX_FALLOCATE)
check_function_exists("pread" HAVE_PREAD)
check_function_exists("pwrite" HAVE_PWRITE)
check_function_exists("socket" HAVE_SOCKET)
check_function_exists("setresuid" HAVE_SETRESUID)

check_library_exists(pthread "pthread_getname_np" "" HAVE_PTHREAD_GETNAME_NP)
check_library_exists(pthread "pthread_get_name_np" "" HAVE_PTHREAD_GET_NAME_NP)
check_library_exists(pthread "pthread_setname_np" "" HAVE_PTHREAD_SETNAME_NP)
check_library_exists(pthread "pthread_set_name_np" "" HAVE_PTHREAD_SET_NAME_NP)
check_library_exists(pthread "pthread_attr_setaffinity_np" "" HAVE_PTHREAD_ATTR_SETAFFINITY_NP)

check_cxx_source_compiles("
	int main() {
		if (__builtin_expect(1<2, 1)) {}
	}" HAVE___BUILTIN_EXPECT)


if (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
	# FIXME: See if we have fdatasync, and what libraries are needed for it.
	# We need to actually check for a declaration as OS X has a dummy
	# implementation in the library which is not prototyped in any header.
	set (HAVE_FDATASYNC 0)
else ()
	check_function_exists("fdatasync" HAVE_FDATASYNC)
endif ()


########################################################################
# libev Flags
########################################################################

check_include_files("sys/epoll.h" HAVE_SYS_EPOLL_H)
check_include_files("sys/types.h;sys/event.h" HAVE_SYS_EVENT_H)
check_include_files("sys/eventfd.h" HAVE_SYS_EVENTFD_H)
check_include_files("sys/inotify.h" HAVE_SYS_INOTIFY_H)
check_include_files("sys/select.h" HAVE_SYS_SELECT_H)
check_include_files("sys/signalfd.h" HAVE_SYS_SIGNALFD_H)
check_include_files("port.h" HAVE_PORT_H)
check_include_files("poll.h" HAVE_POLL_H)

check_function_exists("inotify_init" HAVE_INOTIFY_INIT)
check_function_exists("epoll_ctl" HAVE_EPOLL_CTL)
check_function_exists("kqueue" HAVE_KQUEUE)
check_function_exists("select" HAVE_SELECT)
check_function_exists("eventfd" HAVE_EVENTFD)
check_function_exists("signalfd" HAVE_SIGNALFD)
check_function_exists("port_create" HAVE_PORT_CREATE)
check_function_exists("poll" HAVE_POLL)
check_function_exists("clock_gettime" HAVE_CLOCK_GETTIME)
check_function_exists("nanosleep" HAVE_NANOSLEEP)

# Tweaks
set (HAVE_CLOCK_SYSCALL ${HAVE_CLOCK_GETTIME})


########################################################################
# sparsehash Flags
########################################################################

 # check if type long long is supported
foreach (opt "long long" "uint16_t" "u_int16_t" "__uint16" "_Bool")
	string(REPLACE " " "_" OPT "${opt}")
	string(TOUPPER "${OPT}" OPT)
	check_type_size("${opt}" "${OPT}")
	if (${OPT})
		set (HAVE_${OPT} 1)
	else ()
		set (HAVE_${OPT} 0)
	endif ()
endforeach ()


########################################################################
# Configuration files
########################################################################

configure_file(
	"${PROJECT_SOURCE_DIR}/package_config.h.in"
	"${PROJECT_BINARY_DIR}/src/package_config.h"
	@ONLY)

configure_file(
	"${PROJECT_SOURCE_DIR}/config.h.in"
	"${PROJECT_BINARY_DIR}/src/config.h"
	@ONLY)


########################################################################
# Check for sanitizers
########################################################################

if (NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
	find_package(Sanitizers QUIET)
endif ()


########################################################################
# Externalize debug information
########################################################################

if (DEBUGINFO_EXTERNALIZE AND APPLE)
	if (NOT DEBUGINFO_EXTERNALIZE_SKIP_STRIP)
		set (strip_command COMMAND strip -Sl $<TARGET_FILE:${PROJECT_NAME}>)
	endif ()
	add_custom_command(
		TARGET ${PROJECT_NAME}
		POST_BUILD
		COMMAND dsymutil $<TARGET_FILE:${PROJECT_NAME}>
		${strip_command})
endif ()


########################################################################
# Enable ccache for builds if it exists.
# Set CCACHE_CPP2 envvar, to avoid an error due to a strange ccache/clang++
# interaction.  Specifically, errors about "argument unused during compilation".
# Reference: http://peter.eisentraut.org/blog/2014/12/01/ccache-and-clang-part-3/
########################################################################

find_program(CCACHE_FOUND "ccache")
if (CCACHE_FOUND)
	message(STATUS "Looking for ccache - found")
	set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "ccache")
	set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "ccache")
	if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		set (ENV{CCACHE_CPP2} "yes")
		add_compile_options(-Qunused-arguments)
	endif ()
else ()
	message(STATUS "Looking for ccache - not found")
endif ()


########################################################################
# Remove duplicates compilation flags
########################################################################

remove_duplicate_substrings("${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)
remove_duplicate_substrings("${CMAKE_CXX_FLAGS_DEBUG}" CMAKE_CXX_FLAGS_DEBUG)
remove_duplicate_substrings("${CMAKE_CXX_FLAGS_RELEASE}" CMAKE_CXX_FLAGS_RELEASE)
remove_duplicate_substrings("${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CMAKE_CXX_FLAGS_RELWITHDEBINFO)
remove_duplicate_substrings("${CMAKE_CXX_FLAGS_MINSIZEREL}" CMAKE_CXX_FLAGS_MINSIZEREL)

message(STATUS "${CMAKE_BUILD_TYPE} build")
message(STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${UPPER_CMAKE_BUILD_TYPE}}")



########################################################################
#  _____         _
# |_   _|__  ___| |_ ___
#   | |/ _ \/ __| __/ __|
#   | |  __/\__ \ |_\__ \
#   |_|\___||___/\__|___/
#
########################################################################

if (BUILD_TESTS OR BUILD_BENCHMARKS)
	enable_testing()
	add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND})
endif ()


# Testing
# -------
# You can skip this section if you aren't interested in testing.
#
# The tests are not built by default To build them, set the
# BUILD_TESTS option to ON. You can do it by specifying the
# -DBUILD_TESTS=ON flag when running cmake.

if (BUILD_TESTS)
	find_package(GTest)
	if (NOT GTEST_FOUND)
		message(FATAL_ERROR "GTest not found!")
	else ()
		foreach (VAR_TEST string)
			set (PROJECT_TEST "${PROJECT_NAME}_test_${VAR_TEST}")
			add_executable(${PROJECT_TEST}
				"${PROJECT_SOURCE_DIR}/tests/test_${VAR_TEST}.cc"
			)
			target_include_directories(${PROJECT_TEST} PRIVATE ${GTEST_INCLUDE_DIRS})
			target_link_libraries(${PROJECT_TEST} PRIVATE ${GTEST_BOTH_LIBRARIES})
			add_test(NAME "test_${VAR_TEST}" COMMAND ${PROJECT_TEST})
			add_dependencies(check "${PROJECT_TEST}")
		endforeach ()

		### OLD:
		foreach (VAR_TEST
			boolparser compressor endpoint fieldparser generate_terms geospatial
			geospatial_query uuid hash lru msgpack patcher phonetic query queue
			serialise serialise_list sort storage string_metric threadpool
			url_parser wal
		)
			set (PROJECT_TEST "${PROJECT_NAME}_test_${VAR_TEST}")
			add_executable(${PROJECT_TEST}
				"${PROJECT_SOURCE_DIR}/oldtests/test_${VAR_TEST}.cc"
				"${PROJECT_SOURCE_DIR}/oldtests/set_${VAR_TEST}_test.cc"
				"${PROJECT_SOURCE_DIR}/oldtests/utils.cc"
				"$<TARGET_OBJECTS:PACKAGE_OBJ>"
				"$<TARGET_OBJECTS:XAPIAND_OBJ>"
				"$<TARGET_OBJECTS:BOOLEAN_PARSER_OBJ>"
				"$<TARGET_OBJECTS:LIBEV_OBJ>"
				"$<TARGET_OBJECTS:LZ4_OBJ>"
				"$<TARGET_OBJECTS:UUID_OBJ>"
				"$<TARGET_OBJECTS:PROMETHEUS_OBJ>"
			)
			target_include_directories(${PROJECT_TEST} PRIVATE ${GTEST_INCLUDE_DIRS})
			target_link_libraries(${PROJECT_TEST} PRIVATE
				${GTEST_BOTH_LIBRARIES}
				${XAPIAN_LIBRARIES}
				${CMAKE_THREAD_LIBS_INIT}
				${UUID_LIBRARIES}
				${CHAISCRIPT_LIBRARIES}
				${V8_LIBRARIES}
				${M_LIBRARIES}
				${ZLIB_LIBRARIES}
			)
			add_test(NAME "test_${VAR_TEST}" COMMAND ${PROJECT_TEST})
			add_dependencies(check "${PROJECT_TEST}")
		endforeach ()
		###

	endif ()
endif ()


# Benchmarking
# ------------
# You can skip this section if you aren't interested in benchmarking.
#
# The benchmarks are not built by default. To build them, set the
# BUILD_BENCHMARKS option to ON. You can do it by specifying the
# -DBUILD_BENCHMARKS=ON flag when running cmake.

if (BUILD_BENCHMARKS)
	find_package(GBenchmark)
	if (NOT GBENCHMARK_FOUND)
		message(WARNING "GBenchmark not found!")
	else ()
		foreach (VAR_BENCHMARK string)
			set (PROJECT_BENCHMARK "${PROJECT_NAME}_benchmark_${VAR_BENCHMARK}")
			add_executable(${PROJECT_BENCHMARK}
				"${PROJECT_SOURCE_DIR}/benchmarks/benchmark_${VAR_BENCHMARK}.cc"
			)
			target_include_directories(${PROJECT_BENCHMARK} PRIVATE ${GBENCHMARK_INCLUDE_DIRS})
			target_link_libraries(${PROJECT_BENCHMARK} PRIVATE ${GBENCHMARK_LIBRARIES})
			add_test(NAME "benchmark_${VAR_BENCHMARK}" COMMAND ${PROJECT_BENCHMARK})
			add_dependencies(check "${PROJECT_BENCHMARK}")
		endforeach ()
	endif ()
endif ()
