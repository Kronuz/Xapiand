## Process this file with automake to produce Makefile.in

TESTS = check_tests
check_PROGRAMS = check_tests
check_tests_SOURCES = check_tests.cc $(top_builddir)/src/utils.cc $(top_builddir)/src/md5.cc $(top_builddir)/src/cartesian.cc $(top_builddir)/src/exception.cc $(top_builddir)/src/geometry.cc $(top_builddir)/src/htm.cc $(top_builddir)/src/wkt_parser.cc $(top_builddir)/src/datetime.cc  test_serialise.cc test_htm.cc test_wkt_parser.cc
#check_tests_HEADERS = $(top_builddir)/src/utils.h $(top_builddir)/src/xapian.h
check_tests_CFLAGS = @CHECK_CFLAGS@ -I$(top_builddir)/src/
check_tests_LDADD = @CHECK_LIBS@

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
