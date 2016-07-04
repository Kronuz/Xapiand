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

option (BINARY_PROXY "Define to what port binary traffic will be redirected to" OFF)
option (CLUSTERING "Enable remote clustering" ON)
option (DATABASE_WAL "Enable database write ahead log (WAL)" ON)
option (TRACEBACKS "Enable tracebacks for exceptions" OFF)
option (ASAN "Enable address sanitizer (can be slow)" OFF)
option (BUILD_TESTS "Build all tests" OFF)

foreach(opt BINARY_PROXY CLUSTERING DATABASE_WAL TRACEBACKS)
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

if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()

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

# Check for Wall and Wextra flags.
if (CMAKE_BUILD_TYPE MATCHES Release)
	message (STATUS "Release build")
else ()
	message (STATUS "Debug build")
	CHECK_CXX_COMPILER_FLAG(-Wall COMPILER_SUPPORTS_WALL)
	CHECK_CXX_COMPILER_FLAG(-Wextra COMPILER_SUPPORTS_WEXTRA)
	if (COMPILER_SUPPORTS_WALL AND COMPILER_SUPPORTS_WEXTRA)
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g -O2")
	else ()
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O2")
	endif ()
endif ()

# Check for Address Sanitizer.
if (ASAN)
	if (("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 3.1) OR
		("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.8))
		message (STATUS "${READABLE_CXX_COMPILER} supports AddressSanitizer")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
	else ()
		message (WARNING "${READABLE_CXX_COMPILER} doesn't support AddressSanitizer")
	endif ()
endif ()

message (STATUS "Compile flags: ${CMAKE_CXX_FLAGS}")


########################################################################
# Xapiand
########################################################################

find_program (CCACHE_FOUND "ccache")
if (CCACHE_FOUND)
	message (STATUS "Looking for ccache - found")
	set_property (GLOBAL PROPERTY RULE_LAUNCH_COMPILE "ccache")
	set_property (GLOBAL PROPERTY RULE_LAUNCH_LINK "ccache")
else ()
	message (STATUS "Looking for ccache - not found")
endif ()


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
check_function_exists (__builtin_expect HAVE___BUILTIN_EXPECT)


check_function_exists (pthread_getname_np HAVE_PTHREAD_GETNAME_NP_3)
check_function_exists (pthread_get_name_np HAVE_PTHREAD_GET_NAME_NP_1)
# check_function_exists (pthread_get_name_np HAVE_PTHREAD_GET_NAME_NP_3)
check_function_exists (pthread_setname_np HAVE_PTHREAD_SETNAME_NP_1)
# check_function_exists (pthread_setname_np HAVE_PTHREAD_SETNAME_NP_2)
# check_function_exists (pthread_setname_np HAVE_PTHREAD_SETNAME_NP_3)
check_function_exists (pthread_set_name_np HAVE_PTHREAD_SET_NAME_NP_2)


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
# Configuration
########################################################################

find_package (Xapian REQUIRED)
find_package (Threads REQUIRED)
find_library (LIBPTHREAD pthread)
if (LIBPTHREAD)
	set ("HAVE_LIBPTHREAD" 1)
else ()
	set ("HAVE_LIBPTHREAD" 0)
endif ()

include_directories (
	"${XAPIAN_INCLUDE_DIR}"
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/src")


########################################################################

include (CMakePushCheckState)
cmake_push_check_state ()
set (CMAKE_REQUIRED_INCLUDES ${XAPIAN_INCLUDE_DIR})
set (CMAKE_REQUIRED_LIBRARIES ${XAPIAN_LIBRARIES})
include (CheckCXXSourceCompiles)
check_cxx_source_compiles ("
	#include <xapian.h>
	int main() {
		&Xapian::MSet::serialise;
	}" HAVE_REMOTE_PROTOCOL)
check_cxx_source_compiles ("
	#include <xapian.h>
	int main() {
		&Xapian::Database::get_revision_info;
	}" HAVE_DATABASE_REVISION_INFO)
cmake_pop_check_state ()

if (HAVE_REMOTE_PROTOCOL)
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

file (GLOB SRC_LIST
	${PROJECT_SOURCE_DIR}/src/*.c* ${PROJECT_SOURCE_DIR}/src/*.h
	${PROJECT_SOURCE_DIR}/src/ev/*.c* ${PROJECT_SOURCE_DIR}/src/ev/*.h
	${PROJECT_SOURCE_DIR}/src/hash/*.c* ${PROJECT_SOURCE_DIR}/src/hash/*.h
	${PROJECT_SOURCE_DIR}/src/lz4/*.c* ${PROJECT_SOURCE_DIR}/src/lz4/*.h
	${PROJECT_SOURCE_DIR}/src/servers/*.c* ${PROJECT_SOURCE_DIR}/src/servers/*.h
	${PROJECT_SOURCE_DIR}/src/xchange/*.c* ${PROJECT_SOURCE_DIR}/src/xchange/*.h)

list (REMOVE_ITEM SRC_LIST
	${PROJECT_SOURCE_DIR}/src/ev/ev_epoll.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_kqueue.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_poll.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_port.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_select.c
	${PROJECT_SOURCE_DIR}/src/ev/ev_win32.c
	${PROJECT_SOURCE_DIR}/src/xapiand.cc)

add_library (SRC_OBJ OBJECT ${SRC_LIST})

add_executable (${PROJECT_NAME} ${PROJECT_SOURCE_DIR}/src/xapiand.cc $<TARGET_OBJECTS:SRC_OBJ>)
target_link_libraries (${PROJECT_NAME}
	${XAPIAN_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)

install (TARGETS ${PROJECT_NAME} DESTINATION bin)


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

	foreach (VAR_TEST compressor dllist generate_terms geo hash htm
		lru msgpack patcher query queue serialise_stl serialise sort
		storage threadpool utils wal wkt_parser)
		set (PROJECT_TEST "${PROJECT_NAME}_test_${VAR_TEST}")
		add_executable (${PROJECT_TEST}
			${PACKAGE_PATH_TEST}/test_${VAR_TEST}.cc
			${PACKAGE_PATH_TEST}/set_${VAR_TEST}_test.cc
			$<TARGET_OBJECTS:SRC_OBJ>)

		add_dependencies (${PROJECT_TEST} googletest)
		target_link_libraries (${PROJECT_TEST}
			${XAPIAN_LIBRARIES}
			${CMAKE_THREAD_LIBS_INIT}
			gtest
			gtest_main
		)
		add_test (NAME ${VAR_TEST} COMMAND ${PROJECT_TEST})
	endforeach ()
endif ()

########################################################################
