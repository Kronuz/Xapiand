#
# Copyright (c) 2015-2019 Dubalu LLC
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

message(STATUS "CMake v${CMAKE_VERSION}")

cmake_minimum_required(VERSION 3.12)
project(xapiand C CXX)

cmake_policy(SET CMP0074 NEW)

# Make sure we can import out CMake functions
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set (PACKAGE_NAME "Xapiand")
set (PACKAGE_VERSION "0.40.0")
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

option(CLUSTERING    "Enable remote clustering"                      ON)
option(DATABASE_WAL  "Enable database write ahead log (WAL)"         ON)
option(DATA_STORAGE  "Enable data storage"                           ON)
option(TRACEBACKS    "Enable tracebacks (on by default in Debug)"   OFF)
option(ASSERTS       "Enable asserts (on by default in Debug)"      OFF)
option(TRACKED_MEM   "Enable tracked memory allocator"              OFF)
option(CHECK_IO_FDES "Check file descriptors"                       OFF)
option(LUA           "Enable Lua scripting engine"               ON)
option(UUID_ENCODED  "Allow encoded (base59) as UUID encoding"       ON)
option(UUID_GUID     "Allow GUID as UUID encoding"                  OFF)
option(UUID_URN      "Allow URN as UUID encoding"                   OFF)
option(RANDOM_ERRORS "Allow adding randomized errors"               OFF)
option(CHECK_SIZES   "Print the objects sizes"                      OFF)
option(LTO           "Link Time Optimizations"                       ON)


foreach (opt CLUSTERING DATABASE_WAL DATA_STORAGE TRACEBACKS ASSERTS TRACKED_MEM CHECK_IO_FDES LUA UUID_ENCODED UUID_GUID UUID_URN RANDOM_ERRORS CHECK_SIZES LTO)
	if (${opt})
		set ("XAPIAND_${opt}" 1)
		message(STATUS "Option ${opt} - on")
	else ()
		set ("XAPIAND_${opt}" 0)
	endif ()
endforeach ()

if (APPLE)
	set (CMAKE_FIND_FRAMEWORK "LAST")
	set (HOMEBREW_OPT_DIR "/usr/local/opt")
	message(STATUS "HOMEBREW_OPT_DIR is ${HOMEBREW_OPT_DIR}")
endif ()


########################################################################
# Check CMAKE_BUILD_TYPE:  Debug Release RelWithDebInfo MinSizeRel
########################################################################

# Default value for CMAKE_BUILD_TYPE.
if (NOT CMAKE_BUILD_TYPE)
	set (CMAKE_BUILD_TYPE Release)
endif ()
string(TOUPPER "${CMAKE_BUILD_TYPE}" UPPER_CMAKE_BUILD_TYPE)

# Setup CMAKE_CXX_FLAGS flags.
set (CMAKE_CXX_FLAGS_DEBUG "-g -DDEBUG -fstack-protector-strong")
if (XAPIAND_ASSERTS)
	set (CMAKE_CXX_FLAGS_RELEASE "-O3")
	set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")
	set (CMAKE_CXX_FLAGS_MINSIZEREL "-Os")
else ()
	set (CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
	set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g -DNDEBUG")
	set (CMAKE_CXX_FLAGS_MINSIZEREL "-Os -DNDEBUG")
endif ()


########################################################################
# CMake Includes
########################################################################

include (AC_HEADER_STDC)
include (AddCXXCompilerFlag)
include (CheckSymbolExists)
include (CheckCXXCompilerFlag)
include (CheckCXXSourceCompiles)
include (CheckCXXSourceRuns)
include (CheckCXXSymbolExists)
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
#  _     _ _                    _
# | |   (_) |__  _ __ __ _ _ __(_) ___  ___
# | |   | | '_ \| '__/ _` | '__| |/ _ \/ __|
# | |___| | |_) | | | (_| | |  | |  __/\__ \
# |_____|_|_.__/|_|  \__,_|_|  |_|\___||___/
#
########################################################################


########################################################################
# Use libc++ when available
########################################################################

check_cxx_compiler_flag("-stdlib=libc++" HAVE_FLAG_STDLIB_LIBCPP)
if (HAVE_FLAG_STDLIB_LIBCPP)
	add_cxx_compiler_flag("-stdlib=libc++")
	find_library(LIBCPP_LIBRARIES c++)
	if (LIBCPP_LIBRARIES)
		set (HAVE_LIBCPP 1)
		find_library(LIBCPPABI_LIBRARIES c++abi)
		if (LIBCPPABI_LIBRARIES)
			set (HAVE_LIBCPPABI 1)
			set (LIBCPP_INCLUDE_DIRS ${LIBCPP_INCLUDE_DIRS} ${LIBCPPABI_INCLUDE_DIRS})
			set (LIBCPP_LIBRARIES ${LIBCPP_LIBRARIES} ${LIBCPPABI_LIBRARIES})
		else ()
			set (HAVE_LIBCPPABI 0)
			set (LIBCPP_LIBRARIES "")
		endif ()
	else ()
		set (HAVE_LIBCPP 0)
		set (LIBCPP_LIBRARIES "")
	endif ()
endif ()


########################################################################
# ZLIB Library
########################################################################

find_package(ZLIB)
if (ZLIB_FOUND)
	set (HAVE_ZLIB 1)
else ()
	set (HAVE_ZLIB 0)
endif ()


########################################################################
# Exec Info Library
########################################################################

find_library(EXECINFO_LIBRARIES execinfo)
if (EXECINFO_LIBRARIES)
	set (HAVE_EXECINFO 1)
else ()
	set (HAVE_EXECINFO 0)
	set (EXECINFO_LIBRARIES "")
endif ()


########################################################################
# Real Time Library
########################################################################

find_library(RT_LIBRARIES rt)
if (RT_LIBRARIES)
	set (HAVE_RT 1)
else ()
	set (HAVE_RT 0)
	set (RT_LIBRARIES "")
endif ()


########################################################################
# Math Library
########################################################################

find_library(M_LIBRARIES m)
if (M_LIBRARIES)
	set (HAVE_M 1)
else ()
	set (HAVE_M 0)
	set (M_LIBRARIES "")
endif ()


########################################################################
# Dynamic Linking Library (for dladdr)
########################################################################

find_library(DL_LIBRARIES dl)
if (DL_LIBRARIES)
	set (HAVE_DL 1)
else ()
	set (HAVE_DL 0)
	set (DL_LIBRARIES "")
endif ()


########################################################################
# Threads Library
########################################################################

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
if (CMAKE_USE_PTHREADS_INIT)
	set (HAVE_PTHREADS 1)
else ()
	set (HAVE_PTHREADS 0)
endif ()


########################################################################
# Capabilities Library
########################################################################

find_library(CAP_LIBRARIES cap)
if (CAP_LIBRARIES)
	set (HAVE_CAP 1)
else ()
	set (HAVE_CAP 0)
	set (CAP_LIBRARIES "")
endif ()


########################################################################
# UUID Library
########################################################################

find_package(UUID REQUIRED)


########################################################################
# ICU Library
########################################################################
# ICU's UTF-8 word boundary code was broken before 54.1.

if (APPLE AND NOT ICU_ROOT)
  set (ICU_ROOT "${HOMEBREW_OPT_DIR}/icu4c")
endif ()
find_package(ICU 54.1 COMPONENTS uc)
if (ICU_FOUND)
	set (USE_ICU 1)
else ()
	set (USE_ICU 0)
endif ()



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
set (XAPIAND_DATA_PATH "${CMAKE_INSTALL_FULL_DATAROOTDIR}/xapiand")


include_directories(
	"${PROJECT_SOURCE_DIR}/src"
	"${PROJECT_BINARY_DIR}/src"
	${LIBCPP_INCLUDE_DIRS}
)

link_libraries(
	${CMAKE_THREAD_LIBS_INIT}
	${LIBCPP_LIBRARIES}
)


########################################################################
# Extracted leaf libraries (fetched from their own repositories)
########################################################################
# These small, dependency-free utilities were split out of Xapiand into
# standalone repositories and are now consumed directly instead of being
# vendored under src/. Pinned to commit SHAs for reproducible builds; bump to a
# release tag once the libraries cut releases. For offline/hermetic builds, set
# FETCHCONTENT_SOURCE_DIR_<NAME> (or populate FETCHCONTENT_BASE_DIR) to a local
# checkout.
include(FetchContent)
FetchContent_Declare(perfect_hash GIT_REPOSITORY https://github.com/Kronuz/perfect-hash.git GIT_TAG main)
FetchContent_Declare(split        GIT_REPOSITORY https://github.com/Kronuz/split.git        GIT_TAG main)
FetchContent_Declare(lru_cache    GIT_REPOSITORY https://github.com/Kronuz/lru-cache.git    GIT_TAG main)
FetchContent_Declare(errno_names  GIT_REPOSITORY https://github.com/Kronuz/errno-names.git  GIT_TAG main)
FetchContent_Declare(stash        GIT_REPOSITORY https://github.com/Kronuz/stash.git        GIT_TAG main)  # track tip
FetchContent_Declare(uinteger_t   GIT_REPOSITORY https://github.com/Kronuz/uinteger_t.git   GIT_TAG master)
FetchContent_Declare(base_x       GIT_REPOSITORY https://github.com/Kronuz/base-x.git        GIT_TAG master)
FetchContent_Declare(located_exception GIT_REPOSITORY https://github.com/Kronuz/located-exception.git GIT_TAG main)
# uinteger_t is declared before base_x so base_x's own FetchContent of it dedups
# to this pinned copy.
FetchContent_MakeAvailable(perfect_hash split lru_cache errno_names stash uinteger_t base_x located_exception)

# Header-only libs, included by name ("phf.hh", "split.h", "lru.h", "error.hh"
# +"errnos.h", "stash.h", "uinteger_t.hh", "base_x.hh"), so put their source dirs
# on the global include path now that the vendored copies are gone.
include_directories(
	"${perfect_hash_SOURCE_DIR}"
	"${split_SOURCE_DIR}"
	"${lru_cache_SOURCE_DIR}"
	"${errno_names_SOURCE_DIR}"
	"${stash_SOURCE_DIR}"
	"${uinteger_t_SOURCE_DIR}"
	"${base_x_SOURCE_DIR}"
)

# located-exception ships exception.h + exception.cc. Xapiand keeps a thin
# src/exception.h wrapper that includes the library header by its explicit path
# (LOCATED_EXCEPTION_HEADER, to avoid the same-name collision that breaks
# #include_next when the includer lives in src/) and re-adds the Xapiand-specific
# subclasses (ClientError, etc.) the standalone library drops. The library's
# exception.cc is compiled as part of Xapiand below (C++20, so it uses std::format
# like the rest of the tree) rather than via the library's own target.
add_compile_definitions(LOCATED_EXCEPTION_HEADER="${located_exception_SOURCE_DIR}/exception.h")

# threadpool: the thread pool + Thread primitive, extracted from Xapiand. Like
# located-exception, we compile its one out-of-line file (thread.cc) as part of
# Xapiand (below) so it picks up Xapiand's config.h and the trace injection,
# rather than linking the library's own target. The trace header maps the
# library's no-op hooks back onto Xapiand's log.h + traceback.h, and the name
# prefix restores the "Xapiand:" thread-name prefix.
FetchContent_Declare(threadpool GIT_REPOSITORY https://github.com/Kronuz/threadpool.git GIT_TAG main)
FetchContent_GetProperties(threadpool)
if(NOT threadpool_POPULATED)
	FetchContent_Populate(threadpool)
endif()
include_directories("${threadpool_SOURCE_DIR}")
add_compile_definitions(
	THREADPOOL_TRACE_HEADER="xapiand_threadpool_trace.h"
	THREADPOOL_THREAD_NAME_PREFIX="Xapiand:"
)

# scheduler: the timer-wheel scheduler + debouncer, extracted from Xapiand. It is
# header-driven (scheduler.h, debouncer.h, callable_traits.hh) and builds on stash
# + threadpool (fetched above). Its random_time helper is provided by Xapiand's
# own random.cc, so we populate headers only and do NOT compile the library's
# random.cc. The trace header restores L_SCHEDULER/L_DEBUG_HOOK/L_EXC/L_CALL and
# the color palette onto Xapiand's log.h + colors.h.
FetchContent_Declare(scheduler GIT_REPOSITORY https://github.com/Kronuz/scheduler.git GIT_TAG main)  # track tip
FetchContent_GetProperties(scheduler)
if(NOT scheduler_POPULATED)
	FetchContent_Populate(scheduler)
endif()
include_directories("${scheduler_SOURCE_DIR}")
add_compile_definitions(SCHEDULER_TRACE_HEADER="xapiand_scheduler_trace.h")

# logger: the lock-free, cancellable, deferred logger extracted from Xapiand and
# modernized (std::source_location, iTerm2, backpressure, pluggable hooks). Built
# on the scheduler. We populate headers and compile its logger.cc into XAPIAND_OBJ
# below; src/logger_fwd.h + log.h + colors.h adapt its macro surface, and main.cc
# wires its config/hooks to opts + traceback at startup. Its logger.h replaces
# Xapiand's in-tree one (src/logger.h is now a thin shim).
FetchContent_Declare(logger GIT_REPOSITORY https://github.com/Kronuz/logger.git GIT_TAG main)  # track tip
FetchContent_GetProperties(logger)
if(NOT logger_POPULATED)
	FetchContent_Populate(logger)
endif()
include_directories("${logger_SOURCE_DIR}")

# NOTE: the extracted traceback box (github.com/Kronuz/traceback) is the reusable,
# modern core of this file's stack-trace code, published standalone. Xapiand keeps
# its own src/traceback.cc here because that file also carries the signal-time
# all-thread crash dumper (out of scope for the box), and the logger's backtrace
# hook is wired to Xapiand's richer traceback() (atos file:line) in main.cc. The
# two headers share the name "traceback.h"; fully de-vendoring Xapiand onto the
# extracted core (a rename + dropping the in-tree copy) is a follow-up.

# Compile-time string toolkit, extracted from Xapiand into four standalone repos.
# All header-only: populate and put on the include path. hashes.hh includes
# static_string.hh + chars.hh, so all four dirs go on the path; its runtime
# xxHash fast paths light up automatically because xxhash.h (from lz4) is on the
# path too.
FetchContent_Declare(static_string GIT_REPOSITORY https://github.com/Kronuz/static-string.git GIT_TAG main)
FetchContent_Declare(char_classify GIT_REPOSITORY https://github.com/Kronuz/char-classify.git GIT_TAG main)
FetchContent_Declare(strict_stox  GIT_REPOSITORY https://github.com/Kronuz/strict-stox.git  GIT_TAG main)
FetchContent_Declare(hashes       GIT_REPOSITORY https://github.com/Kronuz/hashes.git       GIT_TAG main)
foreach(dep static_string char_classify strict_stox hashes)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${static_string_SOURCE_DIR}"   # static_string.hh
	"${char_classify_SOURCE_DIR}"   # chars.hh
	"${strict_stox_SOURCE_DIR}"     # strict_stox.hh
	"${hashes_SOURCE_DIR}"          # hashes.hh
)

# term-color: compile-time ANSI color palette, extracted from Xapiand. The
# library ships only the palette + escape machinery; Xapiand keeps a thin
# src/colors.h wrapper that re-adds its colored-logging shortcut macros (L_RED,
# ...). The wrapper pulls the palette by absolute path (TERM_COLOR_PALETTE_HEADER)
# to avoid the same-name collision, the way located-exception is wired. ansi_color.hh
# and color_tools.hh are consumed directly from the library's dir on the path.
FetchContent_Declare(term_color GIT_REPOSITORY https://github.com/Kronuz/term-color.git GIT_TAG main)  # track tip
FetchContent_GetProperties(term_color)
if(NOT term_color_POPULATED)
	FetchContent_Populate(term_color)
endif()
include_directories("${term_color_SOURCE_DIR}")
add_compile_definitions(TERM_COLOR_PALETTE_HEADER="${term_color_SOURCE_DIR}/colors.h")

# Six more utilities extracted from Xapiand into standalone repos: endian (header
# only) and repr / escape / times / strings / url_parser (each a small .cc compiled
# into XAPIAND_OBJ below, the logger way). Their headers go on the include path and
# Xapiand's in-tree copies are dropped; they build on char-classify / split /
# static-string / term-color (already on the path above) + repr.
FetchContent_Declare(endian     GIT_REPOSITORY https://github.com/Kronuz/endian.git     GIT_TAG main)
FetchContent_Declare(repr       GIT_REPOSITORY https://github.com/Kronuz/repr.git       GIT_TAG main)
FetchContent_Declare(escape     GIT_REPOSITORY https://github.com/Kronuz/escape.git     GIT_TAG main)
FetchContent_Declare(times      GIT_REPOSITORY https://github.com/Kronuz/times.git      GIT_TAG main)
FetchContent_Declare(strings    GIT_REPOSITORY https://github.com/Kronuz/strings.git    GIT_TAG main)
FetchContent_Declare(url_parser GIT_REPOSITORY https://github.com/Kronuz/url-parser.git GIT_TAG main)
foreach(dep endian repr escape times strings url_parser)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${endian_SOURCE_DIR}"
	"${repr_SOURCE_DIR}"
	"${escape_SOURCE_DIR}"
	"${times_SOURCE_DIR}"
	"${url_parser_SOURCE_DIR}"
)
# strings is special: Xapiand files historically obtained log.h transitively through
# strings.hh (the extracted library correctly no longer pulls it). So strings_SOURCE_DIR
# is deliberately NOT on the include path; Xapiand keeps a thin src/strings.hh wrapper
# that re-adds log.h and chains to the real header by absolute path (STRINGS_HEADER),
# the same trick as src/colors.h / src/namegen.h. strings.cc finds its own strings.hh
# in its source dir (a quoted include searches there first).
add_compile_definitions(STRINGS_HEADER="${strings_SOURCE_DIR}/strings.hh")
# times.cc's Apple fallback redefines clock_gettime unless HAVE_CLOCK_GETTIME is
# set. It is available on every Xapiand target platform (macOS 10.12+, Linux), and
# the extracted times.h no longer pulls Xapiand's config.h.
add_compile_definitions(HAVE_CLOCK_GETTIME)

# fantasyname: the C++ fantasy name generator (German's, upstreamed to and based on
# skeeto/fantasyname). We compile its c++/namegen.cc into XAPIAND_OBJ below; Xapiand
# keeps a thin src/namegen.h wrapper that re-adds its name_generator() helper. The
# wrapper pulls the library header by absolute path (FANTASYNAME_HEADER) to avoid the
# same-name "namegen.h" collision -- the c++/ dir is deliberately NOT put on the
# include path, the way colors.h is wired for term-color.
FetchContent_Declare(fantasyname GIT_REPOSITORY https://github.com/Kronuz/fantasyname.git GIT_TAG master)  # track tip
FetchContent_GetProperties(fantasyname)
if(NOT fantasyname_POPULATED)
	FetchContent_Populate(fantasyname)
endif()
add_compile_definitions(FANTASYNAME_HEADER="${fantasyname_SOURCE_DIR}/c++/namegen.h")

# enum-reflection: reflective ENUM_CLASS (enum <-> string, perfect-hash lookup),
# extracted from Xapiand. Header-only; enum.h includes phf.hh + hashes.hh, which
# are already on the include path above. boolean-parser: the boolean-expression
# query parser (lexer + shunting-yard + AST + RPN), compiled into BOOLEAN_PARSER_OBJ
# from the fetched sources below. Its headers sit at the repo root with a Boolean
# prefix on the generic ones, so they go on the include path directly.
FetchContent_Declare(enum_reflection GIT_REPOSITORY https://github.com/Kronuz/enum-reflection.git GIT_TAG main)
FetchContent_Declare(boolean_parser  GIT_REPOSITORY https://github.com/Kronuz/boolean-parser.git  GIT_TAG main)
foreach(dep enum_reflection boolean_parser)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${enum_reflection_SOURCE_DIR}"   # enum.h
	"${boolean_parser_SOURCE_DIR}"    # BooleanParser.h, LexicalException.h, ...
)

# datetime: the pure ISO-8601 + date-math + time/timedelta string parser,
# extracted from Xapiand. Like located-exception, its datetime.cc compiles as
# part of XAPIAND_OBJ (below). The MsgPack structured-object glue stays in
# Xapiand as src/datetime_msgpack.{h,cc}. datetime.cc uses strict_stox, already
# on the path.
FetchContent_Declare(datetime GIT_REPOSITORY https://github.com/Kronuz/datetime.git GIT_TAG main)
FetchContent_GetProperties(datetime)
if(NOT datetime_POPULATED)
	FetchContent_Populate(datetime)
endif()
include_directories("${datetime_SOURCE_DIR}")

# More extracted utilities, consumed via FetchContent in place of the in-tree
# copies under src/. md5/sha256 are content digests (they link endian, already on
# the path); random/epoch/nanosleep/stringified are small standalone utilities;
# allocators is the tracked-memory allocator; cartesian is the WGS84/geodesy core.
# Header-only ones just need their dir on the include path; the compiled ones
# (md5/sha256/random/allocators/cartesian) get their .cc added to XAPIAND_OBJ below.
FetchContent_Declare(md5         GIT_REPOSITORY https://github.com/Kronuz/md5.git         GIT_TAG main)
FetchContent_Declare(sha256      GIT_REPOSITORY https://github.com/Kronuz/sha256.git      GIT_TAG main)
FetchContent_Declare(random      GIT_REPOSITORY https://github.com/Kronuz/random.git      GIT_TAG main)
FetchContent_Declare(epoch       GIT_REPOSITORY https://github.com/Kronuz/epoch.git       GIT_TAG main)
FetchContent_Declare(nanosleep   GIT_REPOSITORY https://github.com/Kronuz/nanosleep.git   GIT_TAG main)
FetchContent_Declare(stringified GIT_REPOSITORY https://github.com/Kronuz/stringified.git GIT_TAG main)
FetchContent_Declare(allocators  GIT_REPOSITORY https://github.com/Kronuz/allocators.git  GIT_TAG main)
FetchContent_Declare(cartesian   GIT_REPOSITORY https://github.com/Kronuz/cartesian.git   GIT_TAG main)
foreach(dep md5 sha256 random epoch nanosleep stringified allocators cartesian)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${md5_SOURCE_DIR}"           # md5.h
	"${sha256_SOURCE_DIR}"        # sha256.h
	"${random_SOURCE_DIR}"        # random.hh
	"${epoch_SOURCE_DIR}"         # epoch.hh
	"${nanosleep_SOURCE_DIR}"     # nanosleep.h
	"${stringified_SOURCE_DIR}"   # stringified.hh
	"${allocators_SOURCE_DIR}"    # allocators.h
	"${cartesian_SOURCE_DIR}"     # cartesian.h (+ its decoupled CartesianError)
)
# allocators dropped its #include "config.h"; supply the one feature macro it reads
# when tracking is enabled. HAS_THREAD_LOCAL is self-detected by the library (its
# own in-source probe), so it needs nothing here.
if(TRACKED_MEM)
	add_compile_definitions(XAPIAND_TRACKED_MEM)
endif()

# Two more header-only utilities. queue is the bounded blocking double-ended MPMC
# queue (pure std, self-contained); bloom-filter is the probabilistic set (depends
# on hashes, already on the include path). Header-only, so they just go on the path.
FetchContent_Declare(queue        GIT_REPOSITORY https://github.com/Kronuz/queue.git        GIT_TAG main)
FetchContent_Declare(bloom_filter GIT_REPOSITORY https://github.com/Kronuz/bloom-filter.git GIT_TAG main)
foreach(dep queue bloom_filter)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${queue_SOURCE_DIR}"          # queue.h
	"${bloom_filter_SOURCE_DIR}"   # bloom_filter.hh
)

# soundex: the language Soundex encoders (english/french/german/spanish). The
# library is the pure algorithm; the serialise()/unserialise() persistence Xapiand
# needs is re-attached Xapiand-side in src/metrics/soundex_metric.h
# (SerialisableSoundex), so it stays out of the library. Depends on strings
# (already on the path); header-only, so it just goes on the include path.
FetchContent_Declare(soundex GIT_REPOSITORY https://github.com/Kronuz/soundex.git GIT_TAG main)
FetchContent_GetProperties(soundex)
if(NOT soundex_POPULATED)
	FetchContent_Populate(soundex)
endif()
include_directories("${soundex_SOURCE_DIR}")

# string-similarity: the pure string-metric algorithms (Levenshtein, Jaro,
# Jaro-Winkler, Jaccard, Sorensen-Dice, LCSubstr, LCSubsequence, and the
# encoder-agnostic PhoneticMetric). Header-only, headers flat at the repo root.
# Like soundex, the library is the pure algorithms and deliberately drops the
# serialise()/unserialise() persistence (that pulled length.h + GPL
# serialise-double); Xapiand re-attaches it Xapiand-side via the thin Serialisable*
# subclasses in src/metric_serialise.h, pulled in by src/string_metric.h. Depends
# on strings + soundex (both already on the path); just goes on the include path.
# double-metaphone: the Double Metaphone phonetic encoder, a selectable encoder
# alongside the soundex family for PhoneticMetric. Its one out-of-line file
# (double_metaphone.cc) compiles as part of Xapiand below (the cartesian/htm way),
# and its header goes on the include path.
FetchContent_Declare(string_similarity GIT_REPOSITORY https://github.com/Kronuz/string-similarity.git GIT_TAG main)
FetchContent_Declare(double_metaphone  GIT_REPOSITORY https://github.com/Kronuz/double-metaphone.git  GIT_TAG main)
foreach(dep string_similarity double_metaphone)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${string_similarity_SOURCE_DIR}"   # basic_string_metric.h, levenshtein.h, ..., phonetic_metric.h
	"${double_metaphone_SOURCE_DIR}"    # double_metaphone.h
)

# htm: the Hierarchical Triangular Mesh spatial index, with the Constraint
# spherical-cap primitive and the earth-radius constants absorbed in (they used to
# live in src/geospatial/geometry.{h,cc}). Xapiand's geometry.h now gets Constraint
# + those constants from the library transitively (htm.h -> constraint.h), so their
# definitions are removed from geometry.h; geometry.cc held ONLY the Constraint
# impl, so it is dropped entirely in favor of the library's constraint.cc. The
# Geometry/Collection-coupled debug viz writers the library dropped were already
# dead in Xapiand (only a commented-out call). Depends on cartesian (already on the
# path); compiled (htm.cc + constraint.cc into XAPIAND_OBJ).
FetchContent_Declare(htm GIT_REPOSITORY https://github.com/Kronuz/htm.git GIT_TAG main)
FetchContent_GetProperties(htm)
if(NOT htm_POPULATED)
	FetchContent_Populate(htm)
endif()
include_directories("${htm_SOURCE_DIR}")

# stash traces through injectable hooks (no-op by default). Point it at a small
# Xapiand trace header that restores the colored, logged output by mapping
# stash's hooks onto Xapiand's log.h + colors.h. Applies only to Xapiand's own
# targets (defined below), not the fetched libraries.
add_compile_definitions(STASH_TRACE_HEADER="xapiand_stash_trace.h")


########################################################################
# Vendored third-party header-only libraries, now pulled via FetchContent
# instead of living under src/. We only need their headers, so populate the
# sources and add them to the include path; their own CMakeLists are not run.
########################################################################
# (msgpack stays vendored under src/msgpack/: its exception.hpp is forked to make
# every msgpack exception inherit Xapiand's BaseException, so it can't be swapped
# for stock upstream without reworking the MsgPack wrapper's error handling.)
FetchContent_Declare(rapidjson GIT_REPOSITORY https://github.com/Tencent/rapidjson.git  GIT_TAG v1.1.0)
FetchContent_Declare(cppcodec  GIT_REPOSITORY https://github.com/tplgy/cppcodec.git     GIT_TAG v0.2)
FetchContent_Declare(cli11     GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git      GIT_TAG v2.4.2)
# lz4 is compiled (its lib/*.c is built into LZ4_OBJ below); we still only need
# its sources populated, not its own CMake.
FetchContent_Declare(lz4       GIT_REPOSITORY https://github.com/lz4/lz4.git            GIT_TAG v1.8.1)
foreach(dep rapidjson cppcodec cli11 lz4)
	FetchContent_GetProperties(${dep})
	if(NOT ${dep}_POPULATED)
		FetchContent_Populate(${dep})
	endif()
endforeach()
include_directories(
	"${rapidjson_SOURCE_DIR}/include"   # rapidjson/...
	"${cppcodec_SOURCE_DIR}"            # cppcodec/...
	"${cli11_SOURCE_DIR}/include"       # CLI/CLI.hpp (header-only)
	"${lz4_SOURCE_DIR}/lib"             # lz4.h, lz4frame.h, lz4hc.h, xxhash.h
)

# (yaml/libyaml and libev stay vendored under src/: libyaml's build is wired into
# Xapiand's generated config.h feature detection and its sources are entangled
# with the vendored y2j wrapper in YAML_OBJ; libev likewise depends on Xapiand's
# config.h feature macros and carries local warning-suppression pragmas. Neither
# swaps cleanly to stock upstream without reworking that build glue.)


########################################################################
# Lua + sol2 — the document-scripting engine
########################################################################
if(LUA)
	# Lua 5.4: official sources ship no CMake, so build a static object lib from
	# src/*.c (minus the interpreter/compiler driver mains).
	FetchContent_Declare(lua URL https://www.lua.org/ftp/lua-5.4.7.tar.gz)
	FetchContent_MakeAvailable(lua)
	file(GLOB LUA_SRC_LIST "${lua_SOURCE_DIR}/src/*.c")
	list(REMOVE_ITEM LUA_SRC_LIST
		"${lua_SOURCE_DIR}/src/lua.c"
		"${lua_SOURCE_DIR}/src/luac.c"
		"${lua_SOURCE_DIR}/src/onelua.c")
	add_library(LUA_OBJ OBJECT ${LUA_SRC_LIST})
	target_include_directories(LUA_OBJ PUBLIC "${lua_SOURCE_DIR}/src")

	# sol2: header-only C++ binding; use its headers directly.
	FetchContent_Declare(sol2 GIT_REPOSITORY https://github.com/ThePhD/sol2.git GIT_TAG develop)
	FetchContent_GetProperties(sol2)
	if(NOT sol2_POPULATED)
		FetchContent_Populate(sol2)
	endif()

	include_directories(
		"${lua_SOURCE_DIR}/src"
		"${sol2_SOURCE_DIR}/include"
	)
endif()


########################################################################
# Xapian library target
########################################################################

# Xapian Exceptions
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src/xapian")

add_custom_command(
	OUTPUT "${PROJECT_BINARY_DIR}/src/xapian/error.h" "${PROJECT_BINARY_DIR}/src/xapian/errordispatch.h"
	COMMAND perl
		-w
		-I "${PROJECT_SOURCE_DIR}/src/xapian"
		"${PROJECT_SOURCE_DIR}/src/xapian/generate-exceptions"
	WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/src"
	DEPENDS
		"${PROJECT_SOURCE_DIR}/src/xapian/generate-exceptions"
		"${PROJECT_SOURCE_DIR}/src/xapian/exception_data.pm"
)
list(APPEND XAPIAN_H_LIST "${PROJECT_BINARY_DIR}/src/xapian/error.h" "${PROJECT_BINARY_DIR}/src/xapian/errordispatch.h")

# Snowball

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src/xapian/languages")

file(GLOB SNOWBALL_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/xapian/languages/compiler/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/languages/compiler/*.h*"
)
add_executable(snowball EXCLUDE_FROM_ALL ${SNOWBALL_SRC_LIST})
set_target_properties(snowball PROPERTIES COMPILE_FLAGS "-DDISABLE_CSHARP -DDISABLE_GO -DDISABLE_JAVA -DDISABLE_JS -DDISABLE_PASCAL -DDISABLE_PYTHON -DDISABLE_RUST")

file(GLOB LANGUAGES_SBL_LIST
	"${PROJECT_SOURCE_DIR}/src/xapian/languages/*.sbl"
)
foreach (language_sbl ${LANGUAGES_SBL_LIST})
	get_filename_component(Language ${language_sbl} NAME_WE)
	string(SUBSTRING ${Language} 0 1 first_letter)
	string(TOUPPER ${first_letter} first_letter)
	string(REGEX REPLACE "^.(.*)" "${first_letter}\\1" Language "${Language}")
	string(REGEX REPLACE "^${PROJECT_SOURCE_DIR}/(.*).sbl" "${PROJECT_BINARY_DIR}/\\1" language "${language_sbl}")
	# Run snowball command
	add_custom_command(
		OUTPUT "${language}.cc" "${language}.h"
		COMMAND snowball ${language_sbl} -o ${language} -c++ -u -n InternalStem${Language} -p SnowballStemImplementation
		DEPENDS snowball ${language_sbl}
	)
	list(APPEND LANGUAGES_SRC_LIST "${language}.cc" "${language}.h")
endforeach ()

add_custom_command(
	OUTPUT "${PROJECT_BINARY_DIR}/src/xapian/languages/allsnowballheaders.h" "${PROJECT_BINARY_DIR}/src/xapian/languages/sbl-dispatch.h"
	COMMAND perl
		-I "${PROJECT_SOURCE_DIR}/src/xapian/common"
		-I "${PROJECT_SOURCE_DIR}/src/xapian/languages"
		"${PROJECT_SOURCE_DIR}/src/xapian/languages/collate-sbl"
		"''"
		${LANGUAGES_SBL_LIST}
	WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/src/xapian"
	DEPENDS ${LANGUAGES_SBL_LIST}
)
list(APPEND LANGUAGES_SRC_LIST "${PROJECT_BINARY_DIR}/src/xapian/languages/allsnowballheaders.h" "${PROJECT_BINARY_DIR}/src/xapian/languages/sbl-dispatch.h")

# Unicode

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src/xapian/unicode")

add_custom_command(
	OUTPUT "${PROJECT_BINARY_DIR}/src/xapian/unicode/unicode-data.cc"
	COMMAND tclsh
		"${PROJECT_SOURCE_DIR}/src/xapian/unicode/uniParse.tcl"
		"${PROJECT_SOURCE_DIR}/src/xapian/unicode/UnicodeData.txt" 11.0.0
		"${PROJECT_BINARY_DIR}/src/xapian/unicode/unicode-data.cc"
	DEPENDS
		"${PROJECT_SOURCE_DIR}/src/xapian/unicode/uniParse.tcl"
		"${PROJECT_SOURCE_DIR}/src/xapian/unicode/UnicodeData.txt"
)
list(APPEND UNICODE_SRC_LIST "${PROJECT_BINARY_DIR}/src/xapian/unicode/unicode-data.cc")

add_custom_command(
	OUTPUT "${PROJECT_BINARY_DIR}/src/xapian/unicode/c_istab.h"
	COMMAND perl
		"${PROJECT_SOURCE_DIR}/src/xapian/unicode/gen_c_istab"
		"${PROJECT_BINARY_DIR}/src/xapian/unicode/c_istab.h"
	DEPENDS
		"${PROJECT_SOURCE_DIR}/src/xapian/unicode/gen_c_istab"
)
list(APPEND UNICODE_SRC_LIST "${PROJECT_BINARY_DIR}/src/xapian/unicode/c_istab.h")

# QueryParser

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/src/xapian/queryparser")

add_executable(lemon EXCLUDE_FROM_ALL "${PROJECT_SOURCE_DIR}/src/xapian/queryparser/lemon.c")

add_custom_command(
	OUTPUT "${PROJECT_BINARY_DIR}/src/xapian/queryparser/queryparser_internal.cc" "${PROJECT_BINARY_DIR}/src/xapian/queryparser/queryparser_token.h"
	COMMAND lemon -q
		-o"${PROJECT_BINARY_DIR}/src/xapian/queryparser/queryparser_internal.cc"
		-h"${PROJECT_BINARY_DIR}/src/xapian/queryparser/queryparser_token.h"
		"${PROJECT_SOURCE_DIR}/src/xapian/queryparser/queryparser.lemony"
	DEPENDS lemon "${PROJECT_SOURCE_DIR}/src/xapian/queryparser/queryparser.lemony"
)
list(APPEND QUERYPARSER_SRC_LIST "${PROJECT_BINARY_DIR}/src/xapian/queryparser/queryparser_internal.cc" "${PROJECT_BINARY_DIR}/src/xapian/queryparser/queryparser_token.h")

# Xapian

file(GLOB XAPIAN_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/xapian/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/weight/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/weight/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/cluster/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/cluster/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/queryparser/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/queryparser/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/expand/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/expand/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/net/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/net/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/unicode/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/unicode/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/backends/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/backends/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/backends/multi/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/backends/multi/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/backends/glass/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/backends/glass/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/backends/honey/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/backends/honey/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/backends/inmemory/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/backends/inmemory/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/backends/remote/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/backends/remote/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/languages/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/languages/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/matcher/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/matcher/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/common/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/common/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/diversify/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/diversify/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/api/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/api/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xapian/geospatial/*.c*" "${PROJECT_SOURCE_DIR}/src/xapian/geospatial/*.h*"
)
list(REMOVE_ITEM XAPIAN_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/xapian/version_h.cc"
	"${PROJECT_SOURCE_DIR}/src/xapian/queryparser/lemon.c"
)
list(APPEND XAPIAN_SRC_LIST ${XAPIAN_H_LIST})
list(APPEND XAPIAN_SRC_LIST ${LANGUAGES_SRC_LIST})
list(APPEND XAPIAN_SRC_LIST ${UNICODE_SRC_LIST})
list(APPEND XAPIAN_SRC_LIST ${QUERYPARSER_SRC_LIST})
add_library(XAPIAN_OBJ OBJECT ${XAPIAN_SRC_LIST})

target_include_directories(XAPIAN_OBJ PUBLIC
	${ZLIB_INCLUDE_DIRS}
	${UUID_INCLUDE_DIRS}
	${ICU_INCLUDE_DIRS}
)

target_link_libraries(XAPIAN_OBJ PUBLIC
	${ZLIB_LIBRARIES}
	${UUID_LIBRARIES}
	${ICU_LIBRARIES}
)


########################################################################
# Boolean parser library target
########################################################################

file(GLOB BOOLEAN_PARSER_SRC_LIST
	"${boolean_parser_SOURCE_DIR}/*.cc"
)
add_library(BOOLEAN_PARSER_OBJ OBJECT ${BOOLEAN_PARSER_SRC_LIST})


########################################################################
# Libev library target
########################################################################

file(GLOB LIBEV_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/ev/*.c*" "${PROJECT_SOURCE_DIR}/src/ev/*.h*"
)
list(REMOVE_ITEM LIBEV_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/ev/ev_epoll.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_kqueue.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_poll.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_port.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_select.c"
	"${PROJECT_SOURCE_DIR}/src/ev/ev_win32.c"
)
list(APPEND LIBEV_SRC_LIST ${XAPIAN_H_LIST})
add_library(LIBEV_OBJ OBJECT ${LIBEV_SRC_LIST})


########################################################################
# LZ4 library target
########################################################################

file(GLOB LZ4_SRC_LIST
	"${lz4_SOURCE_DIR}/lib/*.c"
)
list(APPEND LZ4_SRC_LIST ${XAPIAN_H_LIST})
add_library(LZ4_OBJ OBJECT ${LZ4_SRC_LIST})


########################################################################
# Condensed UUID library target
########################################################################

file(GLOB CUUID_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/cuuid/*.c*" "${PROJECT_SOURCE_DIR}/src/cuuid/*.h*"
)
list(APPEND CUUID_SRC_LIST ${XAPIAN_H_LIST})
add_library(CUUID_OBJ OBJECT ${CUUID_SRC_LIST})

target_include_directories(CUUID_OBJ PUBLIC
	${UUID_INCLUDE_DIRS}
)

target_link_libraries(CUUID_OBJ PUBLIC
	${UUID_LIBRARIES}
)


########################################################################
# Prometheus library target
########################################################################

file(GLOB PROMETHEUS_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/prometheus/*.c*" "${PROJECT_SOURCE_DIR}/src/prometheus/*.h*"
)
list(APPEND PROMETHEUS_SRC_LIST ${XAPIAN_H_LIST})
add_library(PROMETHEUS_OBJ OBJECT ${PROMETHEUS_SRC_LIST})


########################################################################
# yaml library target
########################################################################

file(GLOB YAML_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/yaml/*.c*" "${PROJECT_SOURCE_DIR}/src/yaml/*.h*"
	"${PROJECT_SOURCE_DIR}/src/y2j/*.c*" "${PROJECT_SOURCE_DIR}/src/y2j/*.h*"
)
add_library(YAML_OBJ OBJECT ${YAML_SRC_LIST})


########################################################################
# Xapiand package configuration target
########################################################################


########################################################################
# Xapiand package configuration target
########################################################################

list(APPEND XAPIAND_PACKAGE_SRC_LIST "${PROJECT_SOURCE_DIR}/src/package.cc")
list(APPEND XAPIAND_PACKAGE_SRC_LIST ${XAPIAN_H_LIST})
add_library(XAPIAND_PACKAGE_OBJ OBJECT ${XAPIAND_PACKAGE_SRC_LIST})


########################################################################
# Xapiand library target
########################################################################

file(GLOB XAPIAND_SRC_LIST
	"${PROJECT_SOURCE_DIR}/src/*.c*" "${PROJECT_SOURCE_DIR}/src/*.h*"
	"${PROJECT_SOURCE_DIR}/src/lua/*.c*" "${PROJECT_SOURCE_DIR}/src/lua/*.h*"
	"${PROJECT_SOURCE_DIR}/src/database/*.c*" "${PROJECT_SOURCE_DIR}/src/database/*.h*"
	"${PROJECT_SOURCE_DIR}/src/aggregations/*.c*" "${PROJECT_SOURCE_DIR}/src/aggregations/*.h*"
	"${PROJECT_SOURCE_DIR}/src/geospatial/*.c*" "${PROJECT_SOURCE_DIR}/src/geo/*.h*"
	"${PROJECT_SOURCE_DIR}/src/hash/*.c*" "${PROJECT_SOURCE_DIR}/src/hash/*.h*"
	"${PROJECT_SOURCE_DIR}/src/multivalue/*.c*" "${PROJECT_SOURCE_DIR}/src/multivalue/*.h*"
	"${PROJECT_SOURCE_DIR}/src/server/*.c*" "${PROJECT_SOURCE_DIR}/src/server/*.h*"
	"${PROJECT_SOURCE_DIR}/src/utils/*.c*" "${PROJECT_SOURCE_DIR}/src/utils/*.h*"
	"${PROJECT_SOURCE_DIR}/src/xchange/*.c*" "${PROJECT_SOURCE_DIR}/src/xchange/*.h*"
)
list(REMOVE_ITEM XAPIAND_SRC_LIST
"${PROJECT_SOURCE_DIR}/src/main.cc"
"${PROJECT_SOURCE_DIR}/src/package.cc"
"${PROJECT_SOURCE_DIR}/src/logger.cc"
"${PROJECT_SOURCE_DIR}/src/namegen.cc"
"${PROJECT_SOURCE_DIR}/src/repr.cc"
"${PROJECT_SOURCE_DIR}/src/escape.cc"
"${PROJECT_SOURCE_DIR}/src/times.cc"
"${PROJECT_SOURCE_DIR}/src/strings.cc"
"${PROJECT_SOURCE_DIR}/src/url_parser.cc"
"${PROJECT_SOURCE_DIR}/src/hash/md5.cc"
"${PROJECT_SOURCE_DIR}/src/hash/sha256.cc"
"${PROJECT_SOURCE_DIR}/src/random.cc"
"${PROJECT_SOURCE_DIR}/src/allocators.cc"
"${PROJECT_SOURCE_DIR}/src/geospatial/cartesian.cc"
"${PROJECT_SOURCE_DIR}/src/geospatial/htm.cc"
"${PROJECT_SOURCE_DIR}/src/geospatial/geometry.cc"
)
list(APPEND XAPIAND_SRC_LIST ${XAPIAN_H_LIST})
# Compile the located-exception library's out-of-line implementation as part of
# Xapiand (C++20 + std::format), instead of linking its standalone target.
list(APPEND XAPIAND_SRC_LIST "${located_exception_SOURCE_DIR}/exception.cc")
# Same for the threadpool library's out-of-line implementation.
list(APPEND XAPIAND_SRC_LIST "${threadpool_SOURCE_DIR}/thread.cc")
# Same for the datetime library's pure ISO-8601/date-math core; the MsgPack
# structured-object glue (src/datetime_msgpack.cc) stays in Xapiand and is
# globbed above.
list(APPEND XAPIAND_SRC_LIST "${datetime_SOURCE_DIR}/datetime.cc")
# The extracted logger's out-of-line implementation, in place of the removed
# src/logger.cc. (Xapiand keeps its own src/traceback.cc; see the note above.)
list(APPEND XAPIAND_SRC_LIST "${logger_SOURCE_DIR}/logger.cc")
# The fantasy name generator's out-of-line implementation, in place of the removed
# src/namegen.cc. It includes its own "namegen.h" (same dir), so c++/ does not need
# to be on the include path; src/namegen.h reaches it via FANTASYNAME_HEADER.
list(APPEND XAPIAND_SRC_LIST "${fantasyname_SOURCE_DIR}/c++/namegen.cc")
# The six just-extracted utilities' out-of-line implementations, in place of the
# removed src/{repr,escape,times,strings,url_parser}.cc (endian is header-only).
list(APPEND XAPIAND_SRC_LIST "${repr_SOURCE_DIR}/repr.cc")
list(APPEND XAPIAND_SRC_LIST "${escape_SOURCE_DIR}/escape.cc")
list(APPEND XAPIAND_SRC_LIST "${times_SOURCE_DIR}/times.cc")
list(APPEND XAPIAND_SRC_LIST "${strings_SOURCE_DIR}/strings.cc")
list(APPEND XAPIAND_SRC_LIST "${url_parser_SOURCE_DIR}/url_parser.cc")
# The newly extracted compiled utilities, in place of the removed
# src/hash/{md5,sha256}.cc, src/random.cc, src/allocators.cc,
# src/geospatial/cartesian.cc (the rest are header-only).
list(APPEND XAPIAND_SRC_LIST "${md5_SOURCE_DIR}/md5.cc")
list(APPEND XAPIAND_SRC_LIST "${sha256_SOURCE_DIR}/sha256.cc")
list(APPEND XAPIAND_SRC_LIST "${random_SOURCE_DIR}/random.cc")
list(APPEND XAPIAND_SRC_LIST "${allocators_SOURCE_DIR}/allocators.cc")
list(APPEND XAPIAND_SRC_LIST "${cartesian_SOURCE_DIR}/cartesian.cc")
# htm (Hierarchical Triangular Mesh) + the Constraint primitive it absorbed, in
# place of the removed src/geospatial/htm.cc and src/geospatial/geometry.cc.
list(APPEND XAPIAND_SRC_LIST "${htm_SOURCE_DIR}/htm.cc")
list(APPEND XAPIAND_SRC_LIST "${htm_SOURCE_DIR}/constraint.cc")
# The Double Metaphone encoder's out-of-line implementation (the string-similarity
# metrics are header-only; only this encoder ships a .cc).
list(APPEND XAPIAND_SRC_LIST "${double_metaphone_SOURCE_DIR}/double_metaphone.cc")
add_library(XAPIAND_OBJ OBJECT ${XAPIAND_SRC_LIST})

target_include_directories(XAPIAND_OBJ PUBLIC
	${ZLIB_INCLUDE_DIRS}
	${UUID_INCLUDE_DIRS}
	${ICU_INCLUDE_DIRS}
)

target_link_libraries(XAPIAND_OBJ PUBLIC
	${ZLIB_LIBRARIES}
	${UUID_LIBRARIES}
	${M_LIBRARIES}
	${DL_LIBRARIES}
	${CAP_LIBRARIES}
	${RT_LIBRARIES}
	${EXECINFO_LIBRARIES}
)


########################################################################
# Primary target
########################################################################

add_executable(${PROJECT_NAME} "${PROJECT_SOURCE_DIR}/src/main.cc")

target_link_libraries(${PROJECT_NAME}
	XAPIAND_PACKAGE_OBJ
	XAPIAND_OBJ
	XAPIAN_OBJ
	BOOLEAN_PARSER_OBJ
	LIBEV_OBJ
	LZ4_OBJ
	CUUID_OBJ
	PROMETHEUS_OBJ
	YAML_OBJ
	$<TARGET_NAME_IF_EXISTS:LUA_OBJ>
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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/stopwords/ DESTINATION ${XAPIAND_DATA_PATH}/stopwords)

# Copy mime types file
install(FILES ${CMAKE_SOURCE_DIR}/mime.types DESTINATION ${XAPIAND_DATA_PATH})

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

# Add compiler flags.
add_required_cxx_compiler_flag("-std=c++20")
add_cxx_compiler_flag("-fno-common")

if (XAPIAND_LTO)
	add_cxx_compiler_flag("-flto" RELEASE)
endif ()

# add_cxx_compiler_flag("-fno-gnu-keywords")
add_cxx_compiler_flag("-fdiagnostics-color=always")

if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
	#   ____ _
	#  / ___| | __ _ _ __   __ _
	# | |   | |/ _` | '_ \ / _` |
	# | |___| | (_| | | | | (_| |
	#  \____|_|\__,_|_| |_|\__, |
	#                      |___/
	message(STATUS "Compiler: Clang (${CMAKE_CXX_COMPILER_ID})")
	add_cxx_compiler_flag("-Qunused-arguments")

	add_cxx_compiler_flag("-pedantic")
	add_cxx_compiler_flag("-pedantic-errors")

	# Treat warnings as errors.
	# add_cxx_compiler_flag("-Werror" RELEASE)
	# add_cxx_compiler_flag("-Werror" MINSIZEREL)

	# Enable warnings.
	add_cxx_compiler_flag("-Wall")
	add_cxx_compiler_flag("-Wextra")
	add_cxx_compiler_flag("-Wshadow")
	add_cxx_compiler_flag("-Wpointer-arith")
	add_cxx_compiler_flag("-Wformat-security")
	add_cxx_compiler_flag("-Wredundant-decls")
	add_cxx_compiler_flag("-Woverloaded-virtual")
	add_cxx_compiler_flag("-Wmissing-declarations")
	add_cxx_compiler_flag("-Wimplicit-fallthrough")
	add_cxx_compiler_flag("-Winit-self")
	add_cxx_compiler_flag("-Wcast-align")
	add_cxx_compiler_flag("-Wstrict-aliasing")
	add_cxx_compiler_flag("-Wsuggest-final-types")
	# add_cxx_compiler_flag("-Wundef") ##
	# add_cxx_compiler_flag("-Wcast-qual") ##
	# add_cxx_compiler_flag("-Wshorten-64-to-32") ##
	# add_cxx_compiler_flag("-Wfloat-equal") ##

	# Disable certain warnings.
	add_cxx_compiler_flag("-Wno-unknown-pragmas")
	add_cxx_compiler_flag("-Wno-variadic-macros")
	add_cxx_compiler_flag("-Wno-gnu-anonymous-struct")
	add_cxx_compiler_flag("-Wno-gnu-zero-variadic-macro-arguments")

elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
	#    ____  ____ ____
	#  / ___|/ ___/ ___|
	# | |  _| |  | |
	# | |_| | |__| |___
	# \____|\____\____|
	#
	message(STATUS "Compiler: GCC (${CMAKE_CXX_COMPILER_ID})")

	# Disable certain warnings.
	add_cxx_compiler_flag("-Wno-attributes")
	add_cxx_compiler_flag("-Wno-subobject-linkage")

else ()
	message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()


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
check_include_files("sys/prctl.h" HAVE_SYS_PRCTL_H)

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

check_cxx_source_compiles("int main() { __builtin_expect(0, 0); return 0; }" HAVE_DECL___BUILTIN_EXPECT)

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
# xapian Flags
########################################################################

check_symbol_exists(exp10 "math.h" HAVE_DECL_EXP10)
check_symbol_exists(log2 "math.h" HAVE_DECL_LOG2)
check_symbol_exists(__exp10 "math.h" HAVE_DECL___EXP10)

check_cxx_symbol_exists(strerror_r "string.h" HAVE_DECL_STRERROR_R)
check_cxx_symbol_exists(_byteswap_uint64 "stdlib.h" HAVE_DECL__BYTESWAP_UINT64)
check_cxx_symbol_exists(_byteswap_ulong "stdlib.h" HAVE_DECL__BYTESWAP_ULONG)
check_cxx_symbol_exists(_byteswap_ushort "stdlib.h" HAVE_DECL__BYTESWAP_USHORT)
check_cxx_symbol_exists(_putenv_s "stdlib.h" HAVE_DECL__PUTENV_S)
check_cxx_symbol_exists(__popcnt "intrin.h" HAVE_DECL___POPCNT)
check_cxx_symbol_exists(__popcnt64 "intrin.h" HAVE_DECL___POPCNT64)

check_function_exists("closefrom" HAVE_CLOSEFROM)
check_function_exists("fork" HAVE_FORK)
check_function_exists("ftime" HAVE_FTIME)
check_function_exists("ftruncate" HAVE_FTRUNCATE)
check_function_exists("getdirentries" HAVE_GETDIRENTRIES)
check_function_exists("gethostname" HAVE_GETHOSTNAME)
check_function_exists("getrlimit" HAVE_GETRLIMIT)
check_function_exists("getrusage" HAVE_GETRUSAGE)
check_function_exists("link" HAVE_LINK)
check_function_exists("nftw" HAVE_NFTW)
check_function_exists("random" HAVE_RANDOM)
check_function_exists("setenv" HAVE_SETENV)
check_function_exists("sigaction" HAVE_SIGACTION)
check_function_exists("sigsetjmp" HAVE_SIGSETJMP)
check_function_exists("sleep" HAVE_SLEEP)
check_function_exists("socketpair" HAVE_SOCKETPAIR)
check_function_exists("srandom" HAVE_SRANDOM)
check_function_exists("strerror_r" HAVE_STRERROR_R)
check_function_exists("sysconf" HAVE_SYSCONF)
check_function_exists("timer_create" HAVE_TIMER_CREATE)
check_function_exists("times" HAVE_TIMES)
check_function_exists("writev" HAVE_WRITEV)

check_cxx_source_compiles("
	#include <errno.h>
	#include <stdio.h>
	int main() {
		printf(\"%s\", sys_errlist[sys_nerr - 1]);
		return 0;
	}" HAVE_SYS_ERRLIST_AND_SYS_NERR)

check_cxx_source_compiles("
	#include <errno.h>
	#include <stdio.h>
	int main() {
		printf(\"%s\", _sys_errlist[_sys_nerr - 1]);
		return 0;
	}" HAVE__SYS_ERRLIST_AND__SYS_NERR)

check_cxx_source_compiles("int main() { __builtin_exp10(0.0); return 0; }" HAVE___BUILTIN_EXP10)

check_cxx_source_compiles("
	#include <sys/timeb.h>
	int main() {
		struct timeb tp;
		return ftime(&tp);
	}" FTIME_RETURNS_INT)
if (NOT FTIME_RETURNS_INT)
	set (FTIME_RETURNS_VOID 1)
endif ()

check_cxx_source_compiles("
	int main() {
		char buf[100];
		char x = *strerror_r(0, buf, sizeof buf);
		char *p = strerror_r(0, buf, sizeof buf);
		return !p || x;
	}" STRERROR_R_CHAR_P)

check_cxx_source_compiles("int main() { int r; __builtin_add_overflow(0, 0, &r); return 0; }" HAVE_DECL___BUILTIN_ADD_OVERFLOW)
check_cxx_source_compiles("int main() { __builtin_bswap16(0); return 0; }" HAVE_DECL___BUILTIN_BSWAP16)
check_cxx_source_compiles("int main() { __builtin_bswap32(0); return 0; }" HAVE_DECL___BUILTIN_BSWAP32)
check_cxx_source_compiles("int main() { __builtin_bswap64(0); return 0; }" HAVE_DECL___BUILTIN_BSWAP64)
check_cxx_source_compiles("int main() { __builtin_clz(0); return 0; }" HAVE_DECL___BUILTIN_CLZ)
check_cxx_source_compiles("int main() { __builtin_clzl(0); return 0; }" HAVE_DECL___BUILTIN_CLZL)
check_cxx_source_compiles("int main() { __builtin_clzll(0); return 0; }" HAVE_DECL___BUILTIN_CLZLL)
check_cxx_source_compiles("int main() { __builtin_ctz(0); return 0; }" HAVE_DECL___BUILTIN_CTZ)
check_cxx_source_compiles("int main() { __builtin_ctzl(0); return 0; }" HAVE_DECL___BUILTIN_CTZL)
check_cxx_source_compiles("int main() { __builtin_ctzll(0); return 0; }" HAVE_DECL___BUILTIN_CTZLL)
check_cxx_source_compiles("int main() { int r; __builtin_mul_overflow(0, 0, &r); return 0; }" HAVE_DECL___BUILTIN_MUL_OVERFLOW)
check_cxx_source_compiles("int main() { __builtin_popcount(0); return 0; }" HAVE_DECL___BUILTIN_POPCOUNT)
check_cxx_source_compiles("int main() { __builtin_popcountl(0); return 0; }" HAVE_DECL___BUILTIN_POPCOUNTL)
check_cxx_source_compiles("int main() { __builtin_popcountll(0); return 0; }" HAVE_DECL___BUILTIN_POPCOUNTLL)

check_cxx_source_compiles("
	#include <type_traits>
	int main() {
		std::is_trivially_copyable<int>;
		return 0;
	}" HAVE_STD_IS_TRIVIALLY_COPYABLE)

check_include_files("memory.h" HAVE_MEMORY_H)
check_include_files("sys/resource.h" HAVE_SYS_RESOURCE_H)
check_include_files("sys/uio.h" HAVE_SYS_UIO_H)
check_include_files("sys/utsname.h" HAVE_SYS_UTSNAME_H)
check_include_files("uuid.h" HAVE_UUID_H)
check_include_files("uuid/uuid.h" HAVE_UUID_UUID_H)
check_include_files("zlib.h" HAVE_ZLIB_H)

if (LINUX)
	set (USE_PROC_FOR_UUID 1)
endif ()

if (WIN32)
	set (USE_WIN32_UUID_API 1)
endif ()

foreach (_socklen_t socklen_t int size_t unsigned long "unsigned long")
	check_cxx_source_compiles("
		#include <sys/types.h>
		#if defined __WIN32__ || defined _WIN32
		# include <winsock2.h>
		#else
			# include <sys/socket.h>
		#endif
		int main() {
			${_socklen_t} len;
			getsockopt(0, 0, 0, 0, &len);
			return 0;
		}" SOCKLEN_T)
	if (SOCKLEN_T)
		set (SOCKLEN_T "${_socklen_t}")
		break ()
	endif ()
endforeach ()

foreach (_snprintf "snprintf" "_snprintf")
	check_cxx_source_runs("
		#include <cstdio>
		#include <cstring>
		using namespace std;
		int main() {
			char buffer[4] = \"abc\";
			int res1 = ${_snprintf}(buffer, 2, \"%s\", \"XYZ\");
			if (memcmp(buffer, \"X\\\\0c\", 4) != 0) return 2;
			int res2 = ${_snprintf}(buffer, 2, \"%x\", 0x12);
			if (memcmp(buffer, \"1\\\\0c\", 4) != 0) return 3;
			if (res1 == -1 && res2 == -1) return 15; /* Pre-ISO semantics. */
			if (res1 != 3) return 4;
			if (res2 != 2) return 5;
			return 0;
		}" SNPRINTF)
	if (SNPRINTF)
		set (SNPRINTF "${_snprintf}")
		if (NOT SNPRINTF_EXITCODE)
			set (SNPRINTF_ISO "${_snprintf}")
		endif ()
		break ()
	endif ()
endforeach ()

# Check if IEEE-754 is followed for representing floating point numbers by the platform.
check_cxx_source_runs("
#include <cstdint>
#include <cstring>
using namespace std;
int main() {
	double a = 2353.3523423;
	uint64_t bit_pattern =
	0b0100000010100010011000101011010001100110001101011011111011111111;
	if (memcmp(&bit_pattern, &a, sizeof(double)) == 0) return 0;
	return 1;
}" IEEE)
if (IEEE)
	if (NOT IEEE_EXITCODE)
		set (FOLLOWS_IEEE 1)
	endif ()
endif ()

foreach (_pread_prototype "" "extern \"C\" ssize_t pread(int, void *, size_t, off_t) throw ();" "extern \"C\" ssize_t pread(int, void *, size_t, off_t);")
	check_cxx_source_compiles("
		#include <sys/types.h>
		#include <unistd.h>
		${_pread_prototype}
		int main() {
			char b[256];
			pread(1, b, 256, 20);
			return 0;
		}" PREAD_PROTOTYPE)
	if (PREAD_PROTOTYPE)
		set (PREAD_PROTOTYPE "${_pread_prototype}")
		break ()
	endif ()
endforeach ()

foreach (_pwrite_prototype "" "extern \"C\" ssize_t pwrite(int, const void *, size_t, off_t) throw ();" "extern \"C\" ssize_t pwrite(int, const void *, size_t, off_t);")
	check_cxx_source_compiles("
		#include <sys/types.h>
		#include <unistd.h>
		${_pwrite_prototype}
		int main() {
			const char *p = \"hello\";
			pwrite(1, p, 5, 20);
			return 0;
		}" PWRITE_PROTOTYPE)
	if (PWRITE_PROTOTYPE)
		set (PWRITE_PROTOTYPE "${_pwrite_prototype}")
		break ()
	endif ()
endforeach ()


########################################################################

# Fix error: '_Atomic' does not name a type (GCC under macOS)
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
	check_cxx_source_runs("
	#include <stdatomic.h>
	int main() {
	#ifndef _Atomic
		return 1;
	#else
		return 0;
	#endif
	}
	}" ATOMIC_AVAILABLE)
	if (NOT ATOMIC_AVAILABLE)
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_Atomic=volatile")
	endif ()
endif ()


########################################################################
# sparsehash Flags
########################################################################

 # check if type long long is supported
foreach (_type "long long" "uint16_t" "u_int16_t" "__uint16" "_Bool")
	string(REPLACE " " "_" TYPE "${_type}")
	string(TOUPPER "${TYPE}" TYPE)
	check_type_size("${_type}" "${TYPE}")
	if (${TYPE})
		set (HAVE_${TYPE} 1)
	else ()
		set (HAVE_${TYPE} 0)
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

if (APPLE AND DEBUGINFO_EXTERNALIZE)
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
			)
			target_link_libraries(${PROJECT_TEST}
				XAPIAND_PACKAGE_OBJ
				XAPIAND_OBJ
				XAPIAN_OBJ
				BOOLEAN_PARSER_OBJ
				LIBEV_OBJ
				LZ4_OBJ
				CUUID_OBJ
				PROMETHEUS_OBJ
				YAML_OBJ
			)

			target_include_directories(${PROJECT_TEST} PRIVATE ${GTEST_INCLUDE_DIRS})
			target_link_libraries(${PROJECT_TEST} PRIVATE
				${GTEST_BOTH_LIBRARIES}
				${CMAKE_THREAD_LIBS_INIT}
				${UUID_LIBRARIES}
				${M_LIBRARIES}
				${DL_LIBRARIES}
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
