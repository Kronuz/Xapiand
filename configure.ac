#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# On OS X, use brew:
# brew install automake autoconf libtools pkg-config

AC_PREREQ([2.69])
AC_INIT([Xapiand], [3.0.0-alpha], [https://github.com/Kronuz/Xapiand])
AC_CONFIG_SRCDIR([src/xapiand.cc])
AC_CONFIG_HEADERS([src/config.h])
AC_CONFIG_MACRO_DIRS([m4])

LT_INIT

AM_INIT_AUTOMAKE([subdir-objects])

# Checks for programs.
AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CXX

# Use the C++ compiler for the compile tests
AC_LANG(C++)

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE

AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AX_CXX_COMPILE_STDCXX_14([noext], [mandatory])  # This needs autoconf-archive installed
AX_CXX_HAVE_SSTREAM

# AX_PROG_CCACHE_CC
AX_PROG_CCACHE_CXX

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for library functions.
AC_CHECK_FUNCS([getcwd gettimeofday socket])
# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR([pthread not found, Xapiand requires pthread-devel])], [-lpthread])


########################################################################
# sparsehash
########################################################################

AC_CHECK_FUNCS(memcpy)
AC_HEADER_STDC
AC_CHECK_TYPES([uint16_t])     # defined in C99 systems
AC_CHECK_TYPES([u_int16_t])    # defined in BSD-derived systems, and gnu
AC_CHECK_TYPES([__uint16])     # defined in some windows systems (vc7)
AC_CHECK_TYPES([long long])    # probably defined everywhere, but...


########################################################################
# libev
########################################################################

# Checks for header files and library funcions for libev.

AC_CHECK_HEADERS([sys/inotify.h sys/epoll.h sys/event.h port.h poll.h sys/select.h sys/eventfd.h sys/signalfd.h])

AC_CHECK_FUNCS([inotify_init epoll_ctl kqueue port_create poll select eventfd signalfd])

AC_CHECK_FUNCS(clock_gettime, [], [
	if test $(uname) = Linux; then
		# Un Linux, try syscall wrapper first
		AC_MSG_CHECKING(for clock_gettime syscall)
		AC_LINK_IFELSE(
			[
				AC_LANG_PROGRAM(
					[
						#include <unistd.h>
						#include <sys/syscall.h>
						#include <time.h>
					],
					[struct timespec ts; int status = syscall(SYS_clock_gettime, CLOCK_REALTIME, &ts)]
				)
			],
			[
				ac_have_clock_syscall=1
				AC_DEFINE(HAVE_CLOCK_SYSCALL, 1, Define to 1 to use the syscall interface for clock_gettime)
				AC_MSG_RESULT(yes)
			],
			[
				AC_MSG_RESULT(no)
			]
		)
	fi
	if test -z "$ac_have_clock_syscall"; then
		AC_CHECK_LIB(rt, clock_gettime)
		unset ac_cv_func_clock_gettime
		AC_CHECK_FUNCS(clock_gettime)
	fi
])

AC_CHECK_FUNCS(nanosleep, [], [
	AC_CHECK_LIB(rt, nanosleep)
	unset ac_cv_func_nanosleep
	AC_CHECK_FUNCS(nanosleep)
])


########################################################################
# libpcre
########################################################################

AC_CHECK_FUNCS(bcopy memmove strerror)

AC_DEFINE([LINK_SIZE], [2], [Internal link size for PCRE (2, 3, or 4 allowed; default=2)])

AC_DEFINE([SUPPORT_PCRE8], [], [
  Define to any value to enable the 8 bit PCRE library.])

AC_DEFINE_UNQUOTED([POSIX_MALLOC_THRESHOLD], [10], [
  When calling PCRE via the POSIX interface, additional working storage
  is required for holding the pointers to capturing substrings because
  PCRE requires three integers per substring, whereas the POSIX
  interface provides only two. If the number of expected substrings is
  small, the wrapper function uses space on the stack, because this is
  faster than using malloc() for each call. The threshold above which
  the stack is no longer used is defined by POSIX_MALLOC_THRESHOLD.])

AC_DEFINE_UNQUOTED([PARENS_NEST_LIMIT], [250], [
  The value of PARENS_NEST_LIMIT specifies the maximum depth of nested
  parentheses (of any kind) in a pattern. This limits the amount of system
  stack that is used while compiling a pattern.])

AC_DEFINE_UNQUOTED([MATCH_LIMIT], [10000000], [
  The value of MATCH_LIMIT determines the default number of times the
  internal match() function can be called during a single execution of
  pcre_exec(). There is a runtime interface for setting a different
  limit. The limit exists in order to catch runaway regular
  expressions that take for ever to determine that they do not match.
  The default is set very large so that it does not accidentally catch
  legitimate cases.])

AC_DEFINE_UNQUOTED([MATCH_LIMIT_RECURSION], [MATCH_LIMIT], [
  The above limit applies to all calls of match(), whether or not they
  increase the recursion depth. In some environments it is desirable
  to limit the depth of recursive calls of match() more strictly, in
  order to restrict the maximum amount of stack (or heap, if
  NO_RECURSE is defined) that is used. The value of
  MATCH_LIMIT_RECURSION applies only to recursive calls of match(). To
  have any useful effect, it must be less than the value of
  MATCH_LIMIT. The default is to use the same value as MATCH_LIMIT.
  There is a runtime method for setting a different limit.])

AC_DEFINE([MAX_NAME_SIZE], [32], [
  This limit is parameterized just in case anybody ever wants to
  change it. Care must be taken if it is increased, because it guards
  against integer overflow caused by enormously large patterns.])

AC_DEFINE([MAX_NAME_COUNT], [10000], [
  This limit is parameterized just in case anybody ever wants to
  change it. Care must be taken if it is increased, because it guards
  against integer overflow caused by enormously large patterns.])

AC_DEFINE_UNQUOTED([NEWLINE], [10], [
  The value of NEWLINE determines the default newline character sequence. PCRE
  client programs can override this by selecting other values at run time. In
  ASCII environments, the value can be 10 (LF), 13 (CR), or 3338 (CRLF); in
  EBCDIC environments the value can be 21 or 37 (LF), 13 (CR), or 3349 or 3365
  (CRLF) because there are two alternative codepoints (0x15 and 0x25) that are
  used as the NL line terminator that is equivalent to ASCII LF. In both ASCII
  and EBCDIC environments the value can also be -1 (ANY), or -2 (ANYCRLF).])


########################################################################
# xapian
########################################################################

# Check for xapian library.
XO_LIB_XAPIAN([], [], [xapian-config-1.3])

# Check for xapian's remote protocol
save_LIBS="$LIBS"
LIBS="$LIBS $XAPIAN_LIBS"
save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $XAPIAN_CXXFLAGS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include "xapian.h"], [class RemoteClient : public RemoteProtocol {}])],
  [AC_DEFINE([HAVE_REMOTE_PROTOCOL], 1, [Define to 1 if Xapian has RemoteProtocol available.])],
  [AC_MSG_WARN(["Xapian's RemoteProtocol is not available!"])])
CXXFLAGS="$save_CXXFLAGS"
LIBS="$save_LIBS"


########################################################################

AC_MSG_CHECKING(["Whether compiler understands -Wall -Wextra"])
save_CXXFLAGS="$CFLAGS"
CXXFLAGS="$CXXFLAGS -Wall -Wextra"
AC_TRY_COMPILE([],[],
  AC_MSG_RESULT(yes),
  AC_MSG_RESULT(no)
  CXXFLAGS="$save_CXXFLAGS")

PKG_CHECK_MODULES(CHECK, [check >= 0.9.4],,[AC_MSG_RESULT([no, testing is disabled])])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	tests/Makefile
])

AC_OUTPUT
