## Process this file with automake to produce Makefile.in

check_PROGRAMS = set_forward_list_test \
				 set_generate_terms_test \
				 set_geo_test \
				 set_hash_test \
				 set_haystack_test \
				 set_htm_test \
				 set_lru_test \
				 set_query_test \
				 set_queue_test \
				 set_serialise_unserialise_test \
				 set_sort_test \
				 set_threadpool_test \
				 set_wkt_test

TESTS = $(check_PROGRAMS)

headers =   $(top_builddir)/src/cartesian.cc \
			$(top_builddir)/src/cJSON.c \
			$(top_builddir)/src/cJSON_Utils.c \
			$(top_builddir)/src/database.cc \
			$(top_builddir)/src/database_autocommit.cc \
			$(top_builddir)/src/database_utils.cc \
			$(top_builddir)/src/datetime.cc \
			$(top_builddir)/src/endpoint.cc \
			$(top_builddir)/src/exception.cc \
			$(top_builddir)/src/fields.cc \
			$(top_builddir)/src/forward_list.h \
			$(top_builddir)/src/generate_terms.cc \
			$(top_builddir)/src/geometry.cc \
			$(top_builddir)/src/geospatialrange.cc \
			$(top_builddir)/src/hash/md5.cc \
			$(top_builddir)/src/hash/sha256.cc \
			$(top_builddir)/src/haystack.cc \
			$(top_builddir)/src/htm.cc \
			$(top_builddir)/src/length.cc \
			$(top_builddir)/src/log.cc \
			$(top_builddir)/src/lru.h \
			$(top_builddir)/src/multivalue.cc \
			$(top_builddir)/src/multivaluekeymaker.cc \
			$(top_builddir)/src/multivaluerange.cc \
			$(top_builddir)/src/namegen.cc \
			$(top_builddir)/src/queue.h \
			$(top_builddir)/src/schema.cc \
			$(top_builddir)/src/serialise.cc \
			$(top_builddir)/src/threadpool.h \
			$(top_builddir)/src/times.cc \
			$(top_builddir)/src/utils.cc \
			$(top_builddir)/src/wkt_parser.cc


set_forward_list_test_SOURCES = $(headers) set_forward_list_test.cc test_forward_list.cc
set_forward_list_test_LDADD = @CHECK_LIBS@

set_generate_terms_test_SOURCES = $(headers) set_generate_terms_test.cc test_generate_terms.cc
set_generate_terms_test_LDADD = @CHECK_LIBS@

set_geo_test_SOURCES = $(headers) set_geo_test.cc test_geo.cc
set_geo_test_LDADD = @CHECK_LIBS@

set_hash_test_SOURCES = $(headers) set_hash_test.cc test_hash.cc
set_hash_test_LDADD = @CHECK_LIBS@

set_haystack_test_SOURCES = $(headers) set_haystack_test.cc test_haystack.cc
set_haystack_test_LDADD = @CHECK_LIBS@

set_htm_test_SOURCES = $(headers) set_htm_test.cc test_htm.cc
set_htm_test_LDADD = @CHECK_LIBS@

set_lru_test_SOURCES = $(headers) set_lru_test.cc test_lru.cc
set_lru_test_LDADD = @CHECK_LIBS@

set_query_test_SOURCES = $(headers) set_query_test.cc test_query.cc
set_query_test_LDADD = @CHECK_LIBS@

set_queue_test_SOURCES = $(headers) set_queue_test.cc test_queue.cc
set_queue_test_LDADD = @CHECK_LIBS@

set_serialise_unserialise_test_SOURCES = $(headers) set_serialise_unserialise_test.cc test_serialise.cc
set_serialise_unserialise_test_LDADD = @CHECK_LIBS@

set_sort_test_SOURCES = $(headers) set_sort_test.cc test_sort.cc
set_sort_test_LDADD = @CHECK_LIBS@

set_threadpool_test_SOURCES = $(headers) set_threadpool_test.cc test_threadpool.cc
set_threadpool_test_LDADD = @CHECK_LIBS@

set_wkt_test_SOURCES = $(headers) set_wkt_test.cc test_wkt_parser.cc
set_wkt_test_LDADD = @CHECK_LIBS@

AM_LDFLAGS = $(XAPIAN_LIBS)
AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
# FreeBSD not found check.h without -I option flag where the header is
AM_CXXFLAGS += -I/usr/local/include/