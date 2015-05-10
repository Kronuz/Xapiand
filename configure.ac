#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# On OS X, use brew:
# brew install automake autoconf autoconf-archive

AC_PREREQ([2.69])
AC_INIT([Xapiand], [3.0.0-alpha], [https://github.com/Kronuz/Xapiand])
AC_CONFIG_SRCDIR([xapiand.cc])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])

LT_INIT

AM_INIT_AUTOMAKE([subdir-objects])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX

# Use the C++ compiler for the compile tests
AC_LANG(C++)

# Checks for typedefs, structures, and compiler characteristics.
AX_CXX_COMPILE_STDCXX_11([noext], [optional])  # This needs autoconf-archive installed
AX_CXX_HAVE_SSTREAM
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for library functions.
AC_CHECK_FUNCS([getcwd gettimeofday socket sranddev])

# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_create, [], [AC_MSG_ERROR([pthread not found, Xapiand requires pthread-devel])], [-lpthread])
AC_CHECK_LIB(ev, ev_run, [], [AC_MSG_ERROR([libev not found, Xapiand requires libev-devel])], [-lev])

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
  [AC_MSG_WARN([Xapian's RemoteProtocol is not available!])])
CXXFLAGS="$save_CXXFLAGS"
LIBS="$save_LIBS"

AC_OUTPUT(Makefile)
