cmake_minimum_required (VERSION 2.8.10)
project (xapiand C CXX)

set (PACKAGE_NAME "Xapiand")
set (PACKAGE_VERSION "3.0.0-beta1")
set (PACKAGE_URL "https://github.com/Kronuz/Xapiand")
set (PACKAGE_BUGREPORT "https://github.com/Kronuz/Xapiand/issues")
set (PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set (PACKAGE_TARNAME "${PROJECT_NAME}")
set (PACKAGE_PATH_TEST "${PROJECT_SOURCE_DIR}/tests")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

option (IWYU "Enable include-what-you-use" OFF)
option (BINARY_PROXY "Define to what port binary traffic will be redirected to" OFF)
option (CLUSTERING "Enable remote clustering" OFF)
option (DATABASE_WAL "Enable database write ahead log (WAL)" ON)
option (TRACEBACKS "Enable tracebacks for exceptions" OFF)
option (V8 "Enable v8 engine" ON)
option (BUILD_TESTS "Build all tests" OFF)

foreach (opt BINARY_PROXY CLUSTERING DATABASE_WAL TRACEBACKS V8)
	if (${opt})
		set ("XAPIAND_${opt}" 1)
	else ()
		set ("XAPIAND_${opt}" 0)
	endif ()
endforeach ()


set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)


########################################################################
# Check compiler flags
########################################################################

include (CheckCXXCompilerFlag)

set (READABLE_CXX_COMPILER "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")

# Check for compiler support for C++14
CHECK_CXX_COMPILER_FLAG(-std=c++14 COMPILER_SUPPORTS_CXX14)
if (NOT COMPILER_SUPPORTS_CXX14)
	message (FATAL_ERROR "The compiler ${READABLE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
endif ()

# Check for fdiagnostics-color flag.
CHECK_CXX_COMPILER_FLAG(-fdiagnostics-color=always COMPILER_SUPPORTS_DIAG_COLOR)
if (COMPILER_SUPPORTS_DIAG_COLOR)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fdiagnostics-color=always")
else ()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif ()

# Default value for CMAKE_BUILD_TYPE.
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()

# Check for Wall and Wextra flags.
string (TOLOWER ${CMAKE_BUILD_TYPE} _build_type)
if (_build_type STREQUAL release)
	message (STATUS "Release build")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
else ()
	message (STATUS "Debug build")
	CHECK_CXX_COMPILER_FLAG(-Wall COMPILER_SUPPORTS_WALL)
	CHECK_CXX_COMPILER_FLAG(-Wextra COMPILER_SUPPORTS_WEXTRA)
	if (COMPILER_SUPPORTS_WALL AND COMPILER_SUPPORTS_WEXTRA)
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O0")
	else ()
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
	endif ()
endif ()


########################################################################
# Add Check for sanitizers.
########################################################################
if (NOT "${CMAKE_GENERATOR}" MATCHES "Xcode")
	find_package (Sanitizers QUIET)
endif ()


########################################################################
# Enable ccache for builds if it exists.
# Set CCACHE_CPP2 envvar, to avoid an error due to a strange ccache/clang++
# interaction.  Specifically, errors about "argument unused during compilation".
# Reference: http://peter.eisentraut.org/blog/2014/12/01/ccache-and-clang-part-3/
########################################################################

find_program (CCACHE_FOUND "ccache")
if (CCACHE_FOUND)
	message (STATUS "Looking for ccache - found")
	set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "ccache")
	set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK "ccache")
	if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		set (ENV{CCACHE_CPP2} "yes")
		add_compile_options (-Qunused-arguments)
	endif ()
else ()
	message (STATUS "Looking for ccache - not found")
endif ()


########################################################################
# Remove duplicates compilation flags
########################################################################
separate_arguments (CMAKE_CXX_FLAGS)
list (REMOVE_DUPLICATES CMAKE_CXX_FLAGS)
string (REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
message (STATUS "Compile flags: ${CMAKE_CXX_FLAGS}")


########################################################################
# Xapiand
########################################################################

include (AC_HEADER_STDC)

include (CheckIncludeFiles)
check_include_files (pthread.h HAVE_PTHREAD_H)
check_include_files (fcntl.h HAVE_FCNTL_H)
check_include_files (limits.h HAVE_LIMITS_H)
check_include_files (netinet/in.h HAVE_NETINET_IN_H)
check_include_files (sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files (sys/time.h HAVE_SYS_TIME_H)
check_include_files (unistd.h HAVE_UNISTD_H)

include (CheckIncludeFileCXX)
check_include_file_cxx (sstream HAVE_SSTREAM)
check_include_file_cxx (strstream HAVE_STRSTREAM)

include (CheckFunctionExists)
check_function_exists (fallocate HAVE_FALLOCATE)
check_function_exists (fsync HAVE_FSYNC)
check_function_exists (getcwd HAVE_GETCWD)
check_function_exists (gettimeofday HAVE_GETTIMEOFDAY)
check_function_exists (memcpy HAVE_MEMCPY)
check_function_exists (posix_fadvise HAVE_POSIX_FADVISE)
check_function_exists (posix_fallocate HAVE_POSIX_FALLOCATE)
check_function_exists (pread HAVE_PREAD)
check_function_exists (pwrite HAVE_PWRITE)
check_function_exists (socket HAVE_SOCKET)

check_function_exists (pthread_getname_np HAVE_PTHREAD_GETNAME_NP_3)
check_function_exists (pthread_get_name_np HAVE_PTHREAD_GET_NAME_NP_1)
# check_function_exists (pthread_get_name_np HAVE_PTHREAD_GET_NAME_NP_3)
check_function_exists (pthread_setname_np HAVE_PTHREAD_SETNAME_NP_1)
# check_function_exists (pthread_setname_np HAVE_PTHREAD_SETNAME_NP_2)
# check_function_exists (pthread_setname_np HAVE_PTHREAD_SETNAME_NP_3)
check_function_exists (pthread_set_name_np HAVE_PTHREAD_SET_NAME_NP_2)

include (CheckCXXSourceCompiles)
check_cxx_source_compiles ("
	int main() {
		if(__builtin_expect(1<2, 1)) {}
	}" HAVE___BUILTIN_EXPECT)


if (${CMAKE_SYSTEM_NAME} MATCHES Darwin)
	# FIXME: See if we have fdatasync, and what libraries are needed for it.
	# We need to actually check for a declaration as OS X has a dummy
	# implementation in the library which is not prototyped in any header.
	set (HAVE_FDATASYNC 0)
else ()
	check_function_exists (fdatasync HAVE_FDATASYNC)
endif ()


########################################################################
# libev
########################################################################

check_include_files (sys/inotify.h HAVE_SYS_INOTIFY_H)
check_include_files (sys/epoll.h HAVE_SYS_EPOLL_H)
check_include_files (sys/event.h HAVE_SYS_EVENT_H)
check_include_files (sys/select.h HAVE_SYS_SELECT_H)
check_include_files (sys/eventfd.h HAVE_SYS_EVENTFD_H)
check_include_files (sys/signalfd.h HAVE_SYS_SIGNALFD_H)
check_include_files (port.h HAVE_PORT_H)
check_include_files (poll.h HAVE_POLL_H)

check_function_exists (inotify_init HAVE_INOTIFY_INIT)
check_function_exists (epoll_ctl HAVE_EPOLL_CTL)
check_function_exists (kqueue HAVE_KQUEUE)
check_function_exists (select HAVE_SELECT)
check_function_exists (eventfd HAVE_EVENTFD)
check_function_exists (signalfd HAVE_SIGNALFD)
check_function_exists (port_create HAVE_PORT_CREATE)
check_function_exists (poll HAVE_POLL)
check_function_exists (clock_gettime HAVE_CLOCK_GETTIME)
check_function_exists (nanosleep HAVE_NANOSLEEP)

find_library (LIBRT rt)
if (LIBRT)
	set ("HAVE_LIBRT" 1)
else ()
	set ("HAVE_LIBRT" 0)
endif ()

# Tweaks
set (HAVE_CLOCK_SYSCALL ${HAVE_CLOCK_GETTIME})


########################################################################
# sparsehash
########################################################################

 # check if type long long is supported
include (CheckTypeSize)
foreach (opt "long long" "uint16_t" "u_int16_t" "__uint16" "_Bool")
	string (REPLACE " " "_" OPT "${opt}")
	string (TOUPPER "${OPT}" OPT)
	check_type_size ("${opt}" "${OPT}")
	if (${OPT})
		set ("HAVE_${OPT}" 1)
	else ()
		set ("HAVE_${OPT}" 0)
	endif ()
endforeach ()


########################################################################
# V8 Engine
########################################################################
if (V8)
	find_package (V8)
	if (V8_FOUND MATCHES "NO")
		message (FATAL_ERROR "V8 package not found")
	else ()
		include_directories(BEFORE "${V8_INCLUDE_DIRS}")
	endif ()
endif ()


########################################################################
# Configuration
########################################################################

find_package (Xapian REQUIRED)
find_package (Threads REQUIRED)
find_package (UUID REQUIRED)
find_library (LIBPTHREAD pthread)
if (LIBPTHREAD)
	set ("HAVE_LIBPTHREAD" 1)
else ()
	set ("HAVE_LIBPTHREAD" 0)
endif ()

include_directories (
	"${XAPIAN_INCLUDE_DIR}"
	"${UUID_INCLUDE_DIRS}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/src")


########################################################################

include (CMakePushCheckState)
cmake_push_check_state ()
include (CheckCXXSourceCompiles)
set (CMAKE_REQUIRED_INCLUDES "${XAPIAN_INCLUDE_DIR}")
set (CMAKE_REQUIRED_LIBRARIES "${XAPIAN_LIBRARIES}")
check_cxx_source_compiles ("
	#include <xapian.h>
	int main() {
		&Xapian::MSet::serialise;
	}" HAVE_REMOTE_PROTOCOL)
check_cxx_source_compiles ("
	#include <xapian.h>
	int main() {
		&Xapian::Database::get_revision;
	}" HAVE_DATABASE_REVISION)
cmake_pop_check_state ()

if (CLUSTERING AND HAVE_REMOTE_PROTOCOL)
	set ("XAPIAND_CLUSTERING" 1)
else ()
	set ("XAPIAND_CLUSTERING" 0)
endif ()


########################################################################
# Configuration
########################################################################

configure_file (
	${PROJECT_SOURCE_DIR}/config.h.cmake
	${PROJECT_BINARY_DIR}/src/config.h)


file (GLOB XAPIAND_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/*.c* ${PROJECT_SOURCE_DIR}/src/*.h
	${PROJECT_SOURCE_DIR}/src/servers/*.c* ${PROJECT_SOURCE_DIR}/src/servers/*.h
	${PROJECT_SOURCE_DIR}/src/geo/*.c* ${PROJECT_SOURCE_DIR}/src/geo/*.h
	${PROJECT_SOURCE_DIR}/src/xchange/*.c* ${PROJECT_SOURCE_DIR}/src/xchange/*.h
	${PROJECT_SOURCE_DIR}/src/hash/*.c* ${PROJECT_SOURCE_DIR}/src/hash/*.h
	${PROJECT_SOURCE_DIR}/src/multivalue/*.c* ${PROJECT_SOURCE_DIR}/src/multivalue/*.h
)
list (REMOVE_ITEM XAPIAND_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/xapiand.cc
)
add_library (XAPIAND_OBJ OBJECT ${XAPIAND_SRC_LIST})


file (GLOB BOOLEAN_PARSER_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/booleanParser/*.c* ${PROJECT_SOURCE_DIR}/src/booleanParser/*.h
)
add_library (BOOLEAN_PARSER_OBJ OBJECT ${BOOLEAN_PARSER_SRC_LIST})


file (GLOB LIBEV_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/ev/*.c* ${PROJECT_SOURCE_DIR}/src/ev/*.h
)
list (REMOVE_ITEM LIBEV_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/ev/ev_epoll.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_kqueue.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_poll.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_port.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_select.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_win32.c
)
add_library (LIBEV_OBJ OBJECT ${LIBEV_SRC_LIST})


file (GLOB LZ4_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/lz4/*.c* ${PROJECT_SOURCE_DIR}/src/lz4/*.h
)
add_library (LZ4_OBJ OBJECT ${LZ4_SRC_LIST})


file (GLOB GUID_SRC_LIST
	${PROJECT_SOURCE_DIR}/src/guid/*.c* ${PROJECT_SOURCE_DIR}/src/guid/*.h
)
add_library (GUID_OBJ OBJECT ${GUID_SRC_LIST})

add_executable (${PROJECT_NAME}
	${PROJECT_SOURCE_DIR}/src/xapiand.cc
	$<TARGET_OBJECTS:XAPIAND_OBJ>
	$<TARGET_OBJECTS:BOOLEAN_PARSER_OBJ>
	$<TARGET_OBJECTS:LIBEV_OBJ>
	$<TARGET_OBJECTS:LZ4_OBJ>
	$<TARGET_OBJECTS:GUID_OBJ>
)
target_link_libraries (${PROJECT_NAME}
	${V8_LIBRARIES}
	${UUID_LIBRARIES}
	${XAPIAN_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
	m
)

########################################################################
# Manual page generation
########################################################################

set(MANPAGES ${CMAKE_SOURCE_DIR}/man/xapiand.8)

install (FILES ${MANPAGES} DESTINATION man)

install (TARGETS ${PROJECT_NAME} DESTINATION bin)

########################################################################
# Enable Google's Include What You Use
########################################################################

if (IWYU)
	find_program (IWYU_PATH NAMES include-what-you-use iwyu)
	if (IWYU_PATH)
		message (STATUS "Looking for include-what-you-use - found")
		set (IWYU_OPTS
			${IWYU_PATH}
			-Xiwyu
			--mapping_file=${PROJECT_SOURCE_DIR}/iwyu.imp)
		set_property (TARGET XAPIAND_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET XAPIAND_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET BOOLEAN_PARSER_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET BOOLEAN_PARSER_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET LIBEV_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET LIBEV_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET LZ4_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET LZ4_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET GUID_OBJ PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET GUID_OBJ PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET ${PROJECT_NAME} PROPERTY C_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
		set_property (TARGET ${PROJECT_NAME} PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${IWYU_OPTS})
	else ()
		message (STATUS "Looking for include-what-you-use - not found")
	endif ()
endif (IWYU)


########################################################################
# Testing.
#
# You can skip this section if you aren't interested in testing.
#
# The tests are not built by default.  To build them, set the
# BUILD_TESTS option to ON.  You can do it by specifying the
# -DBUILD_TESTS=ON flag when running cmake.
########################################################################
if (BUILD_TESTS)
	enable_testing ()

	add_subdirectory (${PROJECT_SOURCE_DIR}/ext/gtest)

	include_directories (${GTEST_INCLUDE_DIRS} ${COMMON_INCLUDES})

	link_directories (${GTEST_LIBS})

	foreach (VAR_TEST boolparser compressor endpoint fieldparser generate_terms geo guid hash
		htm lru msgpack patcher phonetic query queue serialise_stl serialise
		sort storage string_metric threadpool url_parser wal wkt_parser)
		set (PROJECT_TEST "${PROJECT_NAME}_test_${VAR_TEST}")
		add_executable (${PROJECT_TEST}
			${PACKAGE_PATH_TEST}/test_${VAR_TEST}.cc
			${PACKAGE_PATH_TEST}/set_${VAR_TEST}_test.cc
			$<TARGET_OBJECTS:XAPIAND_OBJ>
			$<TARGET_OBJECTS:BOOLEAN_PARSER_OBJ>
			$<TARGET_OBJECTS:LIBEV_OBJ>
			$<TARGET_OBJECTS:LZ4_OBJ>
			$<TARGET_OBJECTS:GUID_OBJ>
		)

		add_dependencies (${PROJECT_TEST} googletest)
		target_link_libraries (${PROJECT_TEST}
			${XAPIAN_LIBRARIES}
			${CMAKE_THREAD_LIBS_INIT}
			${UUID_LIBRARIES}
			${V8_LIBRARIES}
			m
			gtest
			gtest_main
		)
		add_test (NAME ${VAR_TEST} COMMAND ${PROJECT_TEST})
	endforeach ()
endif ()

########################################################################
