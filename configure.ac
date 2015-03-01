#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Xapiand], [3.0.0-alpha], [https://github.com/Kronuz/Xapiand])
AC_CONFIG_SRCDIR([xapiand.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

LT_INIT

AM_INIT_AUTOMAKE([subdir-objects no-dependencies])

AC_ARG_VAR(XAPIAN_SRCDIR, Location of the xapian-core source code directory)

# Check for Xapian source code
if test -z "$XAPIAN_SRCDIR"; then
	XAPIAN_SRCDIR="$(dirname -- $(find .. -type d -name queryparser -maxdepth 2 | grep xapian-core | head -n1))"
	if test -z "$XAPIAN_SRCDIR"; then
		AC_MSG_ERROR([xapian-core source code directory not found, define XAPIAN_SRCDIR environment variable])
	fi
fi

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR([pthread not found, Xapiand requires pthread-devel])], [-lpthread])
AC_CHECK_LIB(ev, ev_run, [], [AC_MSG_ERROR([libev not found, Xapiand requires libev-devel])], [-lev])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Check for xapian.
XO_LIB_XAPIAN([], [], [xapian-config-1.3])

# Checks for library functions.
AC_CHECK_FUNCS([getcwd gettimeofday socket])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_OUTPUT(Makefile)
