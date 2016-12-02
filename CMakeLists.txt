cmake_minimum_required (VERSION 2.8.10)
project (xapiand C CXX)

set (PACKAGE_NAME "Xapiand")
set (PACKAGE_VERSION "1.0.0")
set (PACKAGE_URL "https://github.com/Kronuz/Xapiand")
set (PACKAGE_BUGREPORT "https://github.com/Kronuz/Xapiand/issues")
set (PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set (PACKAGE_TARNAME "${PROJECT_NAME}")
set (PACKAGE_PATH_TEST "${PROJECT_SOURCE_DIR}/tests")


########################################################################
# Options
########################################################################

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


########################################################################
# CMake Includes
########################################################################

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/modules)

include (AC_HEADER_STDC)
include (CheckCXXCompilerFlag)
include (CheckCXXSourceCompiles)
include (CheckFunctionExists)
include (CheckIncludeFileCXX)
include (CheckIncludeFiles)
include (CheckTypeSize)
include (CMakePushCheckState)
include (GNUInstallDirs)


########################################################################
# Targets
########################################################################

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")

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

include_directories (
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/src")


########################################################################
# Compiler Flags
########################################################################

# Check for compiler support for C++14
check_cxx_compiler_flag (-std=c++14 COMPILER_SUPPORTS_CXX14)
if (COMPILER_SUPPORTS_CXX14)
	set (HAVE_CXX14 1)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
else ()
	message (FATAL_ERROR "The compiler ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} has no C++14 support. Please use a different C++ compiler.")
endif ()


# Check for fdiagnostics-color flag.
check_cxx_compiler_flag (-fdiagnostics-color=always COMPILER_SUPPORTS_DIAG_COLOR)
if (COMPILER_SUPPORTS_DIAG_COLOR)
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fdiagnostics-color=always")
else ()
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif ()


# Default value for CMAKE_BUILD_TYPE.
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()


# Setup CMAKE_CXX_FLAGS Debug flags.
set (CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")


# Check for Wall and Wextra flags.
check_cxx_compiler_flag (-Wall COMPILER_SUPPORTS_WALL)
if (COMPILER_SUPPORTS_WALL)
	set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall")
endif ()
check_cxx_compiler_flag (-Wextra COMPILER_SUPPORTS_WEXTRA)
if (COMPILER_SUPPORTS_WEXTRA)
	set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wextra")
endif ()


########################################################################
# Xapiand Flags
########################################################################

check_include_files (pthread_np.h HAVE_PTHREAD_NP_H)
check_include_files (fcntl.h HAVE_FCNTL_H)
check_include_files (limits.h HAVE_LIMITS_H)
check_include_files (netinet/in.h HAVE_NETINET_IN_H)
check_include_files (sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files (sys/time.h HAVE_SYS_TIME_H)
check_include_files (unistd.h HAVE_UNISTD_H)

check_include_file_cxx (sstream HAVE_SSTREAM)
check_include_file_cxx (strstream HAVE_STRSTREAM)

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

check_cxx_source_compiles ("
	int main() {
		if (__builtin_expect(1<2, 1)) {}
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
# libev Flags
########################################################################

check_include_files (sys/epoll.h HAVE_SYS_EPOLL_H)
check_include_files (sys/event.h HAVE_SYS_EVENT_H)
check_include_files (sys/eventfd.h HAVE_SYS_EVENTFD_H)
check_include_files (sys/inotify.h HAVE_SYS_INOTIFY_H)
check_include_files (sys/select.h HAVE_SYS_SELECT_H)
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

# Tweaks
set (HAVE_CLOCK_SYSCALL ${HAVE_CLOCK_GETTIME})


########################################################################
# sparsehash Flags
########################################################################

 # check if type long long is supported
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
# ZLIB Library
########################################################################

find_package (ZLIB)
if (ZLIB_FOUND)
	set ("HAVE_ZLIB" 1)
	include_directories (BEFORE "${ZLIB_INCLUDE_DIRS}")
	target_link_libraries (${PROJECT_NAME} ${ZLIB_LIBRARIES})
else ()
	set ("HAVE_ZLIB" 0)
endif ()


########################################################################
# V8 Engine Library
########################################################################
if (V8)
	find_package (V8 REQUIRED)
	include_directories (BEFORE "${V8_INCLUDE_DIRS}")
	target_link_libraries (${PROJECT_NAME} ${V8_LIBRARIES})
endif ()


########################################################################
# Real Time Library
########################################################################

find_library (RT_LIBRARIES rt)
if (RT_LIBRARIES)
	set ("HAVE_RT" 1)
	target_link_libraries (${PROJECT_NAME} ${RT_LIBRARIES})
else ()
	set ("HAVE_RT" 0)
endif ()


########################################################################
# Math Library
########################################################################

find_library (M_LIBRARIES m)
if (M_LIBRARIES)
	set ("HAVE_M" 1)
	target_link_libraries (${PROJECT_NAME} ${M_LIBRARIES})
else ()
	set ("HAVE_M" 0)
	set (M_LIBRARIES "")
endif ()


########################################################################
# Threads Library
########################################################################

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package (Threads REQUIRED)
target_link_libraries (${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})

if (CMAKE_USE_PTHREADS_INIT)
	set ("HAVE_PTHREADS" 1)
else ()
	set ("HAVE_PTHREADS" 0)
endif ()


########################################################################
# UUID Library
########################################################################

find_package (UUID REQUIRED)
include_directories (BEFORE "${UUID_INCLUDE_DIRS}")
target_link_libraries (${PROJECT_NAME} ${UUID_LIBRARIES})


########################################################################
# Xapian Library
########################################################################

find_package (Xapian REQUIRED)
include_directories (BEFORE "${UUID_INCLUDE_DIRS}")
target_link_libraries (${PROJECT_NAME} ${XAPIAN_LIBRARIES})

cmake_push_check_state ()
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


########################################################################
# Uninstall target
########################################################################

configure_file (
	"${PROJECT_SOURCE_DIR}/cmake_uninstall.cmake.in"
	"${PROJECT_BINARY_DIR}/cmake_uninstall.cmake"
	IMMEDIATE @ONLY)

add_custom_target (uninstall
	COMMAND ${CMAKE_COMMAND} -P ${PROJECT_BINARY_DIR}/cmake_uninstall.cmake)


########################################################################
# Manual page generation
########################################################################

set (MANPAGES ${CMAKE_SOURCE_DIR}/man/xapiand.8)

install (FILES ${MANPAGES} DESTINATION ${CMAKE_INSTALL_MANDIR}/man8 COMPONENT doc)


########################################################################
# Installation of the binary
########################################################################

install (TARGETS ${PROJECT_NAME} DESTINATION bin)


########################################################################
# Check CMAKE_BUILD_TYPE:  Debug Release RelWithDebInfo MinSizeRel
########################################################################

# Remove duplicates compilation flags
separate_arguments (CMAKE_CXX_FLAGS)
list (REMOVE_DUPLICATES CMAKE_CXX_FLAGS)
string (REPLACE ";" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

string (TOLOWER ${CMAKE_BUILD_TYPE} _build_type)
if (_build_type STREQUAL debug)
	set (CMAKE_BUILD_TYPE "Debug")
	separate_arguments (CMAKE_CXX_FLAGS_DEBUG)
	list (REMOVE_DUPLICATES CMAKE_CXX_FLAGS_DEBUG)
	string (REPLACE ";" " " CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
	message (STATUS "Debug build")
	message (STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")

elseif (_build_type STREQUAL relwithdebinfo)
	set (CMAKE_BUILD_TYPE "RelWithDebInfo")
	separate_arguments (CMAKE_CXX_FLAGS_RELWITHDEBINFO)
	list (REMOVE_DUPLICATES CMAKE_CXX_FLAGS_RELWITHDEBINFO)
	string (REPLACE ";" " " CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
	message (STATUS "Release With Debug Info build")
	message (STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")

elseif (_build_type STREQUAL minsizerel)
	set (CMAKE_BUILD_TYPE "MinSizeRel")
	separate_arguments (CMAKE_CXX_FLAGS_MINSIZEREL)
	list (REMOVE_DUPLICATES CMAKE_CXX_FLAGS_MINSIZEREL)
	string (REPLACE ";" " " CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}")
	message (STATUS "Minimum Size Release build")
	message (STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}")

else ()
	set (CMAKE_BUILD_TYPE "Release")
	separate_arguments (CMAKE_CXX_FLAGS_RELEASE)
	list (REMOVE_DUPLICATES CMAKE_CXX_FLAGS_RELEASE)
	string (REPLACE ";" " " CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
	message (STATUS "Release build")
	message (STATUS "Compile flags: ${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
endif ()


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
			${PACKAGE_PATH_TEST}/utils.cc
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
			${M_LIBRARIES}
			gtest
			gtest_main
		)
		add_test (NAME ${VAR_TEST} COMMAND ${PROJECT_TEST})
	endforeach ()
endif ()

########################################################################
