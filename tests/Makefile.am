## Process this file with automake to produce Makefile.in

check_PROGRAMS = set_generate_terms_test \
				 set_geo_test \
				 set_htm_test \
				 set_query_test \
				 set_serialise_unserialise_test \
				 set_sort_test \
				 set_wkt_test

TESTS = $(check_PROGRAMS)

headers =   $(top_builddir)/src/cartesian.cc \
			$(top_builddir)/src/cJSON.c \
			$(top_builddir)/src/cJSON_Utils.c \
			$(top_builddir)/src/database.cc \
			$(top_builddir)/src/database_utils.cc \
			$(top_builddir)/src/datetime.cc \
			$(top_builddir)/src/endpoint.cc \
			$(top_builddir)/src/exception.cc \
			$(top_builddir)/src/fields.cc \
			$(top_builddir)/src/generate_terms.cc \
			$(top_builddir)/src/geometry.cc \
			$(top_builddir)/src/geospatialrange.cc \
			$(top_builddir)/src/htm.cc \
			$(top_builddir)/src/length.cc \
			$(top_builddir)/src/md5.cc \
			$(top_builddir)/src/multivalue.cc \
			$(top_builddir)/src/multivaluekeymaker.cc \
			$(top_builddir)/src/multivaluerange.cc \
			$(top_builddir)/src/serialise.cc \
			$(top_builddir)/src/times.cc \
			$(top_builddir)/src/utils.cc \
			$(top_builddir)/src/wkt_parser.cc

set_generate_terms_test_SOURCES = $(headers) set_generate_terms_test.cc test_generate_terms.cc
set_generate_terms_test_LDADD = @CHECK_LIBS@

set_geo_test_SOURCES = $(headers) set_geo_test.cc test_geo.cc
set_geo_test_LDADD = @CHECK_LIBS@

set_htm_test_SOURCES = $(headers) set_htm_test.cc test_htm.cc
set_htm_test_LDADD = @CHECK_LIBS@

set_query_test_SOURCES = $(headers) set_query_test.cc test_query.cc
set_query_test_LDADD = @CHECK_LIBS@

set_serialise_unserialise_test_SOURCES = $(headers) set_serialise_unserialise_test.cc test_serialise.cc
set_serialise_unserialise_test_LDADD = @CHECK_LIBS@

set_sort_test_SOURCES = $(headers) set_sort_test.cc test_sort.cc
set_sort_test_LDADD = @CHECK_LIBS@

set_wkt_test_SOURCES = $(headers) set_wkt_test.cc test_wkt_parser.cc
set_wkt_test_LDADD = @CHECK_LIBS@

AM_LDFLAGS = $(XAPIAN_LIBS)
AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
# FreeBSD not found check.h without -I option flag where the header is
AM_CXXFLAGS += -I/usr/local/include/