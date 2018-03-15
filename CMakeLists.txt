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

cmake_minimum_required(VERSION 3.2)
project(xapiand C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

set (PACKAGE_NAME "Xapiand")
set (PACKAGE_VERSION "1.0.0")
set (PACKAGE_URL "https://github.com/Kronuz/Xapiand")
set (PACKAGE_BUGREPORT "https://github.com/Kronuz/Xapiand/issues")
set (PACKAGE_TARNAME "${PROJECT_NAME}")


########################################################################
# Options
########################################################################

option(IWYU "Enable include-what-you-use"                           OFF)
option(BUILD_TESTS "Build all tests"                                OFF)
option(BUILD_BENCHMARKS "Build all benchmarks"                      OFF)
option(DEBUGINFO_EXTERNALIZE "Externalize debug information"        OFF)

option(BINARY_PROXY  "Define a proxy port for binary protocol"      OFF)
option(CLUSTERING    "Enable remote clustering"                     OFF)
option(DATABASE_WAL  "Enable database write ahead log (WAL)"         ON)
option(DATA_STORAGE  "Enable data storage"                           ON)
option(TRACEBACKS    "Enable tracebacks for exceptions"             OFF)
option(TRACKED_MEM   "Enable tracked memory allocator"               ON)
option(V8            "Enable v8 engine"                             OFF)
option(CHAISCRIPT    "Enable ChaiScript engine"                      ON)
option(UUID_ENCODED  "Allow encoded (base59) as UUID encoding"       ON)
option(UUID_GUID     "Allow GUID as UUID encoding"                  OFF)
option(UUID_URN      "Allow URN as UUID encoding"                   OFF)

foreach (opt BINARY_PROXY CLUSTERING DATABASE_WAL DATA_STORAGE TRACEBACKS TRACKED_MEM V8 CHAISCRIPT UUID_ENCODED UUID_GUID UUID_URN)
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
# Version
########################################################################

find_package(Git)

configure_file(
	"${PROJECT_SOURCE_DIR}/package_config.cmake.in"
	"${PROJECT_BINARY_DIR}/package_config.cmake"
	@ONLY)

add_custom_target(package_config
	BYPRODUCTS "${PROJECT_BINARY_DIR}/src/package_config.h"
	COMMAND ${CMAKE_COMMAND} -P "${PROJECT_BINARY_DIR}/package_config.cmake")


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
include (GNUInstallDirs)


########################################################################
# Targets
########################################################################

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

set (PATH_STOPWORDS "${CMAKE_INSTALL_FULL_DATAROOTDIR}/xapiand/stopwords")


file(GLOB XAPIAND_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/*.c*" "${PROJECT_SOURCE_DIR}/src/*.h"
	"${PROJECT_SOURCE_DIR}/src/geospatial/*.c*" "${PROJECT_SOURCE_DIR}/src/geo/*.h"
	"${PROJECT_SOURCE_DIR}/src/hash/*.c*" "${PROJECT_SOURCE_DIR}/src/hash/*.h"
	"${PROJECT_SOURCE_DIR}/src/multivalue/*.c*" "${PROJECT_SOURCE_DIR}/src/multivalue/*.h"
	"${PROJECT_SOURCE_DIR}/src/servers/*.c*" "${PROJECT_SOURCE_DIR}/src/servers/*.h"
	"${PROJECT_SOURCE_DIR}/src/xchange/*.c*" "${PROJECT_SOURCE_DIR}/src/xchange/*.h"
)
list(REMOVE_ITEM XAPIAND_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/xapiand.cc"
	"${PROJECT_SOURCE_DIR}/src/package.cc"
)
add_library(XAPIAND_OBJ OBJECT ${XAPIAND_SRC_LIST})


file(GLOB BOOLEAN_PARSER_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/booleanParser/*.c*" "${PROJECT_SOURCE_DIR}/src/booleanParser/*.h"
)
add_library(BOOLEAN_PARSER_OBJ OBJECT ${BOOLEAN_PARSER_SRC_LIST})


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


file(GLOB LZ4_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/lz4/*.c*" "${PROJECT_SOURCE_DIR}/src/lz4/*.h"
)
add_library(LZ4_OBJ OBJECT ${LZ4_SRC_LIST})


file(GLOB UUID_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/cuuid/*.c*" "${PROJECT_SOURCE_DIR}/src/cuuid/*.h"
)
add_library(UUID_OBJ OBJECT ${UUID_SRC_LIST})

add_executable(${PROJECT_NAME}
	"${PROJECT_SOURCE_DIR}/src/xapiand.cc"
	"${PROJECT_SOURCE_DIR}/src/package.cc"
	"$<TARGET_OBJECTS:XAPIAND_OBJ>"
	"$<TARGET_OBJECTS:BOOLEAN_PARSER_OBJ>"
	"$<TARGET_OBJECTS:LIBEV_OBJ>"
	"$<TARGET_OBJECTS:LZ4_OBJ>"
	"$<TARGET_OBJECTS:UUID_OBJ>"
)

add_dependencies(${PROJECT_NAME} package_config)

include_directories(
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/src")


########################################################################
# Compiler Flags
########################################################################

# Setup CMAKE_CXX_FLAGS Debug flags.
set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

# Add compiler flags.
add_required_cxx_compiler_flag("-std=c++14")

add_cxx_compiler_flag("-fdiagnostics-color=always")
add_cxx_compiler_flag("-pedantic")
add_cxx_compiler_flag("-pedantic-errors")

add_cxx_compiler_flag("-Wall")
add_cxx_compiler_flag("-Wextra")
add_cxx_compiler_flag("-Wshadow")
add_cxx_compiler_flag("-Werror" RELEASE)
add_cxx_compiler_flag("-Werror" RELWITHDEBINFO)
add_cxx_compiler_flag("-Werror" MINSIZEREL)
add_cxx_compiler_flag("-Wsuggest-final-types")
add_cxx_compiler_flag("-Wshorten-64-to-32") ##
add_cxx_compiler_flag("-Wfloat-equal") ##
add_cxx_compiler_flag("-Wfstrict-aliasing") ##

# Disable certain warnings.
add_cxx_compiler_flag("-Wnon-virtual-dtor")
add_cxx_compiler_flag("-Wno-unknown-pragmas")
add_cxx_compiler_flag("-Wno-variadic-macros")
add_cxx_compiler_flag("-Wno-gnu-zero-variadic-macro-arguments")


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

check_library_exists(pthread "pthread_getname_np" "" HAVE_PTHREAD_GETNAME_NP_3)
check_library_exists(pthread "pthread_get_name_np" "" HAVE_PTHREAD_GET_NAME_NP_1)
# check_library_exists(pthread "pthread_get_name_np" "" HAVE_PTHREAD_GET_NAME_NP_3)
check_library_exists(pthread "pthread_setname_np" "" HAVE_PTHREAD_SETNAME_NP_1)
# check_library_exists(pthread "pthread_setname_np" "" HAVE_PTHREAD_SETNAME_NP_2)
# check_library_exists(pthread "pthread_setname_np" "" HAVE_PTHREAD_SETNAME_NP_3)
check_library_exists(pthread "pthread_set_name_np" "" HAVE_PTHREAD_SET_NAME_NP_2)

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
# Check for sanitizers
########################################################################

if (NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
	find_package(Sanitizers QUIET)
endif ()


########################################################################
# Add Check for sanitizers.
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
# ZLIB Library
########################################################################

find_package(ZLIB)
if (ZLIB_FOUND)
	set (HAVE_ZLIB 1)
	include_directories(BEFORE "${ZLIB_INCLUDE_DIRS}")
	target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES})
else ()
	set (HAVE_ZLIB 0)
endif ()


########################################################################
# V8 Engine Library
########################################################################
if (V8)
	find_package(V8 REQUIRED)
	include_directories(BEFORE "${V8_INCLUDE_DIRS}")
	target_link_libraries(${PROJECT_NAME} ${V8_LIBRARIES})
########################################################################
# ChaiScript Engine Library
########################################################################
elseif (CHAISCRIPT)
	find_package(CHAISCRIPT)
	if (CHAISCRIPT_FOUND)
		include_directories(BEFORE "${CHAISCRIPT_INCLUDE_DIRS}")
	endif ()
endif ()


########################################################################
# Exec Info Library
########################################################################

find_library(EXECINFO_LIBRARIES execinfo)
if (EXECINFO_LIBRARIES)
	set (HAVE_EXECINFO 1)
	target_link_libraries(${PROJECT_NAME} ${EXECINFO_LIBRARIES})
else ()
	set (HAVE_EXECINFO 0)
endif ()


########################################################################
# Real Time Library
########################################################################

find_library(RT_LIBRARIES rt)
if (RT_LIBRARIES)
	set (HAVE_RT 1)
	target_link_libraries(${PROJECT_NAME} ${RT_LIBRARIES})
else ()
	set (HAVE_RT 0)
endif ()


########################################################################
# Math Library
########################################################################

find_library(M_LIBRARIES m)
if (M_LIBRARIES)
	set (HAVE_M 1)
	target_link_libraries(${PROJECT_NAME} ${M_LIBRARIES})
else ()
	set (HAVE_M 0)
	set (M_LIBRARIES "")
endif ()


########################################################################
# Threads Library
########################################################################

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

if (CMAKE_USE_PTHREADS_INIT)
	set (HAVE_PTHREADS 1)
else ()
	set (HAVE_PTHREADS 0)
endif ()


########################################################################
# UUID Library
########################################################################

find_package(UUID REQUIRED)
include_directories(BEFORE "${UUID_INCLUDE_DIRS}")
target_link_libraries(${PROJECT_NAME} ${UUID_LIBRARIES})


########################################################################
# Xapian Library
########################################################################

find_package(Xapian REQUIRED)
include_directories(BEFORE "${XAPIAN_INCLUDE_DIR}")
target_link_libraries(${PROJECT_NAME} ${XAPIAN_LIBRARIES})

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
# Configuration
########################################################################

configure_file(
	"${PROJECT_SOURCE_DIR}/config.h.in"
	"${PROJECT_BINARY_DIR}/src/config.h"
	@ONLY)


########################################################################
# Uninstall target
########################################################################

configure_file(
	"${PROJECT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${PROJECT_BINARY_DIR}/cmake_uninstall.cmake"
	@ONLY)

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -P ${PROJECT_BINARY_DIR}/cmake_uninstall.cmake)


########################################################################
# Copy stopwords from source to build directory
########################################################################

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/stopwords/ ${CMAKE_INSTALL_DATAROOTDIR}/xapiand/stopwords)


########################################################################
# Copy stopwords during the installation
########################################################################

install(DIRECTORY ${CMAKE_SOURCE_DIR}/stopwords/ DESTINATION ${PATH_STOPWORDS})


########################################################################
# Manual page generation
########################################################################

install(FILES ${CMAKE_SOURCE_DIR}/man/xapiand.8 DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 COMPONENT doc)


########################################################################
# Installation of the binary
########################################################################

install(TARGETS ${PROJECT_NAME} DESTINATION bin)


########################################################################
# Check CMAKE_BUILD_TYPE:  Debug Release RelWithDebInfo MinSizeRel
########################################################################

# Default value for CMAKE_BUILD_TYPE.
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()

# Remove duplicates compilation flags
separate_arguments(CMAKE_CXX_FLAGS)
list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS)
string(REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

string(TOLOWER ${CMAKE_BUILD_TYPE} _build_type)
if (_build_type STREQUAL debug)
	set (CMAKE_BUILD_TYPE "Debug")
	separate_arguments(CMAKE_CXX_FLAGS_DEBUG)
	list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS_DEBUG)
	string(REPLACE ";" " " CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	message(STATUS "Debug build")
	message(STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")

elseif (_build_type STREQUAL relwithdebinfo)
	set (CMAKE_BUILD_TYPE "RelWithDebInfo")
	separate_arguments(CMAKE_CXX_FLAGS_RELWITHDEBINFO)
	list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS_RELWITHDEBINFO)
	string(REPLACE ";" " " CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
	message(STATUS "Release With Debug Info build")
	message(STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

elseif (_build_type STREQUAL minsizerel)
	set (CMAKE_BUILD_TYPE "MinSizeRel")
	separate_arguments(CMAKE_CXX_FLAGS_MINSIZEREL)
	list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS_MINSIZEREL)
	string(REPLACE ";" " " CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
	message(STATUS "Minimum Size Release build")
	message(STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}")

else ()
	set (CMAKE_BUILD_TYPE "Release")
	separate_arguments(CMAKE_CXX_FLAGS_RELEASE)
	list(REMOVE_DUPLICATES CMAKE_CXX_FLAGS_RELEASE)
	string(REPLACE ";" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	message(STATUS "Release build")
	message(STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
endif ()


########################################################################
# Enable Google's Include What You Use
########################################################################

if (IWYU)
	find_program(IWYU_PATH NAMES include-what-you-use iwyu)
	if (IWYU_PATH)
		message(STATUS "Looking for include-what-you-use - found")
		set (IWYU_OPTS
			${IWYU_PATH}
			-Xiwyu
			--mapping_file=${PROJECT_SOURCE_DIR}/iwyu.imp)
		set_property(TARGET XAPIAND_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET XAPIAND_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET BOOLEAN_PARSER_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET BOOLEAN_PARSER_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET LIBEV_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET LIBEV_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET LZ4_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET LZ4_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET UUID_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET UUID_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET ${PROJECT_NAME} PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property(TARGET ${PROJECT_NAME} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
	else ()
		message(STATUS "Looking for include-what-you-use - not found")
	endif ()
endif (IWYU)


#####################################################################
# Testing.
#
# You can skip this section if you aren't interested in testing.
#
# The tests are not built by default To build them, set the
# BUILD_TESTS option to ON. You can do it by specifying the
# -DBUILD_TESTS=ON flag when running cmake.
#

if (BUILD_TESTS)
	enable_testing()
	set (EXTERNAL_PROJECTS_DIR ${PROJECT_SOURCE_DIR}/external)

	# Build googletest Tests
	add_subdirectory(${EXTERNAL_PROJECTS_DIR}/gtest)

	include_directories(${GTEST_INCLUDE_DIR})
	link_directories(${GTEST_LIBRARY_DIR})

	foreach (VAR_TEST string)
		set (PROJECT_TEST "${PROJECT_NAME}_test_${VAR_TEST}")
		add_executable(${PROJECT_TEST}
			"${PROJECT_SOURCE_DIR}/tests/test_${VAR_TEST}.cc"
		)
		add_dependencies(${PROJECT_TEST} googletest)
		target_link_libraries(${PROJECT_TEST}
			gtest_main gtest
		)
		add_test(NAME "test_${VAR_TEST}" COMMAND ${PROJECT_TEST})
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
			"${PROJECT_SOURCE_DIR}/src/package.cc"
			"$<TARGET_OBJECTS:XAPIAND_OBJ>"
			"$<TARGET_OBJECTS:BOOLEAN_PARSER_OBJ>"
			"$<TARGET_OBJECTS:LIBEV_OBJ>"
			"$<TARGET_OBJECTS:LZ4_OBJ>"
			"$<TARGET_OBJECTS:UUID_OBJ>"
		)
		add_dependencies(${PROJECT_TEST} googletest)
		target_link_libraries(${PROJECT_TEST}
			gtest_main gtest
			${XAPIAN_LIBRARIES}
			${CMAKE_THREAD_LIBS_INIT}
			${UUID_LIBRARIES}
			${CHAISCRIPT_LIBRARIES}
			${V8_LIBRARIES}
			${M_LIBRARIES}
			${ZLIB_LIBRARIES}
		)
		add_test(NAME "test_${VAR_TEST}" COMMAND ${PROJECT_TEST})
	endforeach ()
	###

endif ()


#####################################################################
# Benchmarking.
#
# You can skip this section if you aren't interested in benchmarking.
#
# The benchmarks are not built by default. To build them, set the
# BUILD_BENCHMARKS option to ON. You can do it by specifying the
# -DBUILD_BENCHMARKS=ON flag when running cmake.
#

if (BUILD_BENCHMARKS)
	enable_testing()
	set (EXTERNAL_PROJECTS_DIR ${PROJECT_SOURCE_DIR}/external)

	# Build googlebenchmark Benchmarks
	add_subdirectory(${EXTERNAL_PROJECTS_DIR}/gbenchmark)

	include_directories(${GBENCHMARK_INCLUDE_DIR})
	link_directories(${GBENCHMARK_LIBRARY_DIR})

	foreach (VAR_BENCHMARK string)
		set (PROJECT_BENCHMARK "${PROJECT_NAME}_benchmark_${VAR_BENCHMARK}")
		add_executable(${PROJECT_BENCHMARK}
			"${PROJECT_SOURCE_DIR}/benchmarks/benchmark_${VAR_BENCHMARK}.cc"
		)
		add_dependencies(${PROJECT_BENCHMARK} googlebenchmark)
		target_link_libraries(${PROJECT_BENCHMARK}
			benchmark
		)
		add_test(NAME "benchmark_${VAR_BENCHMARK}" COMMAND ${PROJECT_BENCHMARK})
	endforeach ()
endif ()
