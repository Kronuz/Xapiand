#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([Xapiand Server], [3.0], [german.mb@gmail.com])
AC_CONFIG_SRCDIR([xapiand.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

LT_INIT

AM_INIT_AUTOMAKE([subdir-objects])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Check for xapian-core.
XO_LIB_XAPIAN([], [], [xapian-config-1.3])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([getcwd gettimeofday socket])

AC_OUTPUT(Makefile)
