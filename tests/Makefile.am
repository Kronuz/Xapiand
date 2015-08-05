## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = -Wno-portability
#Not portable due to shell
check:
	$(shell rm -rf .db_test1.db .db_test2.db)

TESTS = set_general_test set_geo_test set_query_test
check_PROGRAMS = set_general_test set_geo_test set_query_test

set_general_test_SOURCES = set_general_test.cc $(top_builddir)/src/utils.cc $(top_builddir)/src/md5.cc $(top_builddir)/src/cartesian.cc $(top_builddir)/src/exception.cc $(top_builddir)/src/geometry.cc $(top_builddir)/src/htm.cc $(top_builddir)/src/wkt_parser.cc $(top_builddir)/src/datetime.cc test_serialise.cc test_htm.cc test_wkt_parser.cc
#set_general_test_HEADERS = $(top_builddir)/src/utils.h $(top_builddir)/src/xapian.h
set_general_test_CFLAGS = @CHECK_CFLAGS@ -I$(top_builddir)/src/
set_general_test_LDADD = @CHECK_LIBS@

set_geo_test_SOURCES = set_geo_test.cc test_geo.cc $(top_builddir)/src/cJSON.c $(top_builddir)/src/utils.cc $(top_builddir)/src/database.cc $(top_builddir)/src/endpoint.cc $(top_builddir)/src/md5.cc $(top_builddir)/src/datetime.cc $(top_builddir)/src/cartesian.cc $(top_builddir)/src/exception.cc $(top_builddir)/src/geometry.cc $(top_builddir)/src/htm.cc $(top_builddir)/src/wkt_parser.cc $(top_builddir)/src/cJSON_Utils.c $(top_builddir)/src/times.cc $(top_builddir)/src/fields.cc $(top_builddir)/src/multivalue.cc $(top_builddir)/src/length.cc
set_geo_test_LDADD = @CHECK_LIBS@

set_query_test_SOURCES = set_query_test.cc test_query.cc $(top_builddir)/src/cJSON.c $(top_builddir)/src/utils.cc $(top_builddir)/src/database.cc $(top_builddir)/src/endpoint.cc $(top_builddir)/src/md5.cc $(top_builddir)/src/datetime.cc $(top_builddir)/src/cartesian.cc $(top_builddir)/src/exception.cc $(top_builddir)/src/geometry.cc $(top_builddir)/src/htm.cc $(top_builddir)/src/wkt_parser.cc $(top_builddir)/src/cJSON_Utils.c $(top_builddir)/src/times.cc $(top_builddir)/src/fields.cc $(top_builddir)/src/multivalue.cc $(top_builddir)/src/length.cc
set_query_test_LDADD = @CHECK_LIBS@

AM_LDFLAGS = $(XAPIAN_LIBS)
AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
# FreeBSD not found check.h without -I option flag where the header is
AM_CXXFLAGS += -I/usr/local/include/