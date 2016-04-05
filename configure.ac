#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# On OS X, use brew:
# brew install automake autoconf libtools pkg-config

AC_PREREQ([2.69])
AC_INIT([Xapiand], [3.0.0-alpha1], [https://github.com/Kronuz/Xapiand])
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

AX_PTHREAD_SET_NAME  # Check what pthread_set_name variant there is
AX_PTHREAD_GET_NAME

# Checks for header files.
AC_CHECK_HEADERS([pthread.h fcntl.h limits.h netinet/in.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

# Checks for libraries.
AC_CHECK_LIB(pthread, pthread_create, [], [], [-lpthread])

# Checks for library functions.
AC_CHECK_FUNCS([getcwd gettimeofday socket pread pwrite fallocate posix_fallocate posix_fadvise __builtin_expect fsync])

# See if we have fdatasync, and what libraries are needed for it.
# We need to actually check for a declaration as OS X has a dummy
# implementation in the library which is not prototyped in any header.
AC_CHECK_DECL([fdatasync(int)], [
    SAVE_LIBS=$LIBS
    AC_SEARCH_LIBS([fdatasync], [rt], [])
    LIBS=$SAVE_LIBS
    AC_CHECK_FUNCS([fdatasync])
  ],
  [ac_cv_func_fdatasync=no],
  [#include <unistd.h>]
)

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
  [AC_LANG_PROGRAM([#include "xapian.h"], [&Xapian::MSet::serialise])],
  [AC_DEFINE([HAVE_REMOTE_PROTOCOL], [1], [Define to 1 if Xapian has RemoteProtocol available.])
  have_remote_protocol=yes],
  [])
CXXFLAGS="$save_CXXFLAGS"
LIBS="$save_LIBS"


# Check for xapian's get_revision_info
save_LIBS="$LIBS"
LIBS="$LIBS $XAPIAN_LIBS"
save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $XAPIAN_CXXFLAGS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM([#include "xapian.h"], [&Xapian::Database::get_revision_info])],
  [AC_DEFINE([HAVE_DATABASE_REVISION_INFO], [1], [Define to 1 if Xapian has Database::get_revision_info() available.])
  have_database_revision_info=yes],
  [])
CXXFLAGS="$save_CXXFLAGS"
LIBS="$save_LIBS"


########################################################################

AC_MSG_CHECKING(["whether compiler understands -Wall -Wextra"])
save_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS -Wall -Wextra"
AC_TRY_COMPILE([],[],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)]
  [CXXFLAGS="$save_CXXFLAGS"])

PKG_CHECK_MODULES(CHECK, [check >= 0.9.4],,[AC_MSG_RESULT([no, testing is disabled])])

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  tests/Makefile
])


########################################################################
# --with-* options
########################################################################


AC_MSG_CHECKING([whether to use a proxy for binary protocol])
AC_ARG_WITH(binary-proxy,
  [AS_HELP_STRING([--with-binary-proxy@<:@=8860@:>@], [use proxy to send binary traffic from 8890->8860])],
  [
    if test "$withval" = "yes"; then withval="8860"; fi
    AC_DEFINE_UNQUOTED([XAPIAND_BINARY_PROXY], [$withval], [Define to what port binary traffic will be redirected to.])
    AC_MSG_RESULT([yes (8890->$withval)])
  ],
  [AC_MSG_RESULT(no)])


AC_ARG_ENABLE(database-wal,
  [AS_HELP_STRING([--disable-database-wal], [use database write ahead log (WAL)])],
  [enable_database_wal="$enableval"],
  [enable_database_wal=yep])
AC_MSG_CHECKING([whether to enable write ahead log (WAL)])
case "$enable_database_wal" in
  y*)
    AC_DEFINE([XAPIAND_DATABASE_WAL], [1], [Enable database write ahead log (WAL).])
    AC_MSG_RESULT(yes)
    ;;
  n*)
    AC_MSG_RESULT(no)
    ;;
  *)
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([bad value $enable_database_wal for --enable-database-wal])
    ;;
esac


AC_ARG_ENABLE(data-storage,
  [AS_HELP_STRING([--enable-data-storage], [use custom database data storage volumes (experimental)])],
  [enable_data_storage="$enableval"],
  [enable_data_storage=no])
AC_MSG_CHECKING([whether to enable custom database data storage volumes])
case "$enable_data_storage" in
  y*)
    AC_DEFINE([XAPIAND_DATA_STORAGE], [1], [Enable custom database data storage volumes.])
    AC_MSG_RESULT(yes)
    ;;
  n*)
    AC_MSG_RESULT(no)
    ;;
  *)
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([bad value $enable_data_storage for --enable-data-storage])
    ;;
esac


AC_ARG_ENABLE(clustering,
  [AS_HELP_STRING([--disable-clustering], [allow remote clustering])],
  [enable_clustering="$enableval"],
  [enable_clustering=yep])
AC_MSG_CHECKING([whether to enable remote clustering])
case "$enable_clustering" in
  y*)
    if ! test "$have_remote_protocol" = "yes"; then
      AC_MSG_RESULT(no)
      if test "$enable_clustering" = "yes"; then
        AC_MSG_ERROR([Xapian's RemoteProtocol is not available!])
      else
        AC_MSG_WARN([Xapian's RemoteProtocol is not available!])
      fi
    elif ! test "$have_database_revision_info" = "yes"; then
      AC_MSG_RESULT(no)
      if test "$enable_clustering" = "yes"; then
        AC_MSG_ERROR([Xapian's Database::get_revision_info() is not available!])
      else
        AC_MSG_WARN([Xapian's Database::get_revision_info() is not available!])
      fi
    else
      AC_DEFINE([XAPIAND_CLUSTERING], [1], [Enable remote clustering.])
      AC_MSG_RESULT(yes)
    fi
    ;;
  n*)
    AC_MSG_RESULT(no)
    ;;
  *)
    AC_MSG_ERROR([bad value $enable_clustering for --enable-clustering])
    ;;
esac


AC_ARG_ENABLE(tracebacks,
  [AS_HELP_STRING([--enable-tracebacks], [enable tracebacks for exceptions (can be slow)])],
  [enable_tracebacks="$enableval"],
  [enable_tracebacks=no])
AC_MSG_CHECKING([whether to enable tracebacks for exceptions])
case "$enable_tracebacks" in
  y*)
    AC_DEFINE([XAPIAND_TRACEBACKS], [1], [Enable tracebacks for exceptions.])
    AC_MSG_RESULT(yes)
    ;;
  n*)
    AC_MSG_RESULT(no)
    ;;
  *)
    AC_MSG_RESULT(no)
    AC_MSG_ERROR([bad value $enable_tracebacks for --enable-tracebacks])
    ;;
esac


########################################################################

CXXFLAGS="$CXXFLAGS -fcolor-diagnostics -fsanitize=address"

AC_OUTPUT
