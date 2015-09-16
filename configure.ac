#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# On OS X, use brew:
# brew install automake autoconf autoconf-archive

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

AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])  # This needs autoconf-archive installed
AX_CXX_HAVE_SSTREAM

AX_PROG_CCACHE_CC
AX_PROG_CCACHE_CXX

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h sys/inotify.h sys/epoll.h sys/event.h port.h poll.h sys/select.h sys/eventfd.h sys/signalfd.h])
 
# Checks for library functions.
AC_CHECK_FUNCS([getcwd gettimeofday socket sranddev inotify_init epoll_ctl kqueue port_create poll select eventfd signalfd])

# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR([pthread not found, Xapiand requires pthread-devel])], [-lpthread])
AC_CHECK_LIB(pcre, pcre_compile, [], [AC_MSG_ERROR([libpcre not found, Xapiand requires libpcre-devel])], [-lpcre])


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


# Check for xapian.
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

#AM_PATH_CHECK()
PKG_CHECK_MODULES(CHECK, [check >= 0.9.4],,[AC_MSG_RESULT([no, testing is disabled])])

AC_CONFIG_FILES([
	Makefile
	src/Makefile
	tests/Makefile
])
AC_OUTPUT
