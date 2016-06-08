# message (STATUS "Hello")

cmake_minimum_required (VERSION 2.8.8)
project (xapiand C CXX)

set (PACKAGE_NAME "Xapiand")
set (PACKAGE_VERSION "3.0.0-beta1")
set (PACKAGE_URL "https://github.com/Kronuz/Xapiand")
set (PACKAGE_BUGREPORT "https://github.com/Kronuz/Xapiand/issues")
set (PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set (PACKAGE_TARNAME "${PROJECT_NAME}")

option (BINARY_PROXY "Define to what port binary traffic will be redirected to" OFF)
option (CLUSTERING "Enable remote clustering" ON)
option (DATABASE_WAL "Enable database write ahead log (WAL)" ON)
option (TRACEBACKS "Enable tracebacks for exceptions" OFF)

foreach(opt BINARY_PROXY CLUSTERING DATABASE_WAL TRACEBACKS)
	if (${opt})
		set ("XAPIAND_${opt}" 1)
	else ()
		set ("XAPIAND_${opt}" 0)
	endif ()
endforeach ()

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)


########################################################################
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()

if (CMAKE_BUILD_TYPE MATCHES Release)
	message("Release build.")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fdiagnostics-color=always")
else ()
	message("Debug build.")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fdiagnostics-color=always -Wall -Wextra -g -O2")
endif ()


########################################################################
# Xapiand
########################################################################

find_program(CCACHE_FOUND "ccache")
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "ccache")
endif(CCACHE_FOUND)


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
	check_function_exists(fdatasync HAVE_FDATASYNC)
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
foreach(opt "long long" "uint16_t" "u_int16_t" "__uint16" "_Bool")
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


########################################################################
# Configuration
########################################################################

configure_file (
	${PROJECT_SOURCE_DIR}/src/config.h.cmake
	${PROJECT_BINARY_DIR}/src/config.h)

aux_source_directory (src SRC_LIST)
aux_source_directory (src/xchange SRC_LIST)
aux_source_directory (src/servers SRC_LIST)
aux_source_directory (src/lz4 SRC_LIST)
aux_source_directory (src/hash SRC_LIST)
aux_source_directory (src/ev SRC_LIST)
list (REMOVE_ITEM SRC_LIST
	src/ev/ev_win32.c
	src/ev/ev_iocp.c
	src/ev/ev_port.c
	src/ev/ev_kqueue.c
	src/ev/ev_epoll.c
	src/ev/ev_poll.c
	src/ev/ev_select.c)

add_executable (${PROJECT_NAME} ${SRC_LIST})
target_link_libraries (${PROJECT_NAME}
	${XAPIAN_LIBRARIES}
	${CMAKE_THREAD_LIBS_INIT}
)

install (TARGETS ${PROJECT_NAME} DESTINATION bin)


########################################################################
# Testing
########################################################################

enable_testing ()

add_subdirectory (${PROJECT_SOURCE_DIR}/ext/gtest)

set (PROJECT_TEST_NAME ${PROJECT_NAME}_test)
include_directories (${GTEST_INCLUDE_DIRS} ${COMMON_INCLUDES})

file (GLOB TEST_SRC_FILES ${PROJECT_SOURCE_DIR}/test/*.cc)

link_directories (${GTEST_LIBS})

add_executable (${PROJECT_TEST_NAME} EXCLUDE_FROM_ALL ${TEST_SRC_FILES})
add_dependencies (${PROJECT_TEST_NAME} googletest)
target_link_libraries (${PROJECT_TEST_NAME}
	${CMAKE_THREAD_LIBS_INIT}
	gtest gtest_main
)

########################################################################

add_test(test1 ${PROJECT_TEST_NAME})
