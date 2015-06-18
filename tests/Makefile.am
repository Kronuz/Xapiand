## Process this file with automake to produce Makefile.in

TESTS = check_utils
check_PROGRAMS = check_utils
check_utils_SOURCES = check_utils.cc $(top_builddir)/src/utils.cc $(top_builddir)/src/md5.cc $(top_builddir)/src/cartesian.cc $(top_builddir)/src/exception.cc $(top_builddir)/src/tests.cc
#check_utils_HEADERS = $(top_builddir)/src/utils.h $(top_builddir)/src/xapian.h
check_utils_CFLAGS = @CHECK_CFLAGS@ -I$(top_builddir)/src/
check_utils_LDADD = @CHECK_LIBS@

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
