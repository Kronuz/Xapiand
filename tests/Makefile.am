## Process this file with automake to produce Makefile.in

check_PROGRAMS = \
	set_compressor_test \
	set_forward_list_test \
	set_generate_terms_test \
	set_geo_test \
	set_hash_test \
	set_htm_test \
	set_lru_test \
	set_msgpack_test \
	set_patcher_test \
	set_query_test \
	set_queue_test \
	set_serialise_stl_test \
	set_serialise_unserialise_test \
	set_sort_test \
	set_storage_test \
	set_threadpool_test \
	set_utils_test \
	set_wal_test \
	set_wkt_test

TESTS = $(check_PROGRAMS)

common_files = \
	$(top_builddir)/src/async_fsync.cc \
	$(top_builddir)/src/cartesian.cc \
	$(top_builddir)/src/client_base.cc \
	$(top_builddir)/src/client_binary.cc \
	$(top_builddir)/src/client_http.cc \
	$(top_builddir)/src/database.cc \
	$(top_builddir)/src/database_autocommit.cc \
	$(top_builddir)/src/database_handler.cc \
	$(top_builddir)/src/database_utils.cc \
	$(top_builddir)/src/datetime.cc \
	$(top_builddir)/src/endpoint.cc \
	$(top_builddir)/src/endpoint_resolver.cc \
	$(top_builddir)/src/ev/ev.c \
	$(top_builddir)/src/ev/event.c \
	$(top_builddir)/src/exception.cc \
	$(top_builddir)/src/fields.cc \
	$(top_builddir)/src/forward_list.h \
	$(top_builddir)/src/generate_terms.cc \
	$(top_builddir)/src/geometry.cc \
	$(top_builddir)/src/geospatialrange.cc \
	$(top_builddir)/src/hash/md5.cc \
	$(top_builddir)/src/hash/sha256.cc \
	$(top_builddir)/src/htm.cc \
	$(top_builddir)/src/http_parser.c \
	$(top_builddir)/src/io_utils.cc \
	$(top_builddir)/src/length.cc \
	$(top_builddir)/src/log.cc \
	$(top_builddir)/src/lru.h \
	$(top_builddir)/src/lz4/lz4.c \
	$(top_builddir)/src/lz4/lz4frame.c \
	$(top_builddir)/src/lz4/lz4hc.c \
	$(top_builddir)/src/lz4/xxhash.c \
	$(top_builddir)/src/lz4_compressor.cc \
	$(top_builddir)/src/manager.cc \
	$(top_builddir)/src/msgpack_patcher.cc \
	$(top_builddir)/src/multivalue.cc \
	$(top_builddir)/src/multivaluekeymaker.cc \
	$(top_builddir)/src/multivaluerange.cc \
	$(top_builddir)/src/namegen.cc \
	$(top_builddir)/src/queue.h \
	$(top_builddir)/src/replicator.cc \
	$(top_builddir)/src/schema.cc \
	$(top_builddir)/src/serialise.cc \
	$(top_builddir)/src/stl_serialise.cc \
	$(top_builddir)/src/storage.h \
	$(top_builddir)/src/threadpool.h \
	$(top_builddir)/src/servers/binary.cc \
	$(top_builddir)/src/servers/discovery.cc \
	$(top_builddir)/src/servers/http.cc \
	$(top_builddir)/src/servers/raft.cc \
	$(top_builddir)/src/servers/server.cc \
	$(top_builddir)/src/servers/server_base.cc \
	$(top_builddir)/src/servers/server_binary.cc \
	$(top_builddir)/src/servers/server_discovery.cc \
	$(top_builddir)/src/servers/server_http.cc \
	$(top_builddir)/src/servers/server_raft.cc \
	$(top_builddir)/src/servers/tcp_base.cc \
	$(top_builddir)/src/servers/udp_base.cc \
	$(top_builddir)/src/times.cc \
	$(top_builddir)/src/utils.cc \
	$(top_builddir)/src/wkt_parser.cc \
	$(top_builddir)/src/xchange/convert.cc \
	utils.h


set_compressor_test_SOURCES = $(common_files) set_compressor_test.cc test_compressor.cc
set_compressor_test_LDADD = @CHECK_LIBS@

set_forward_list_test_SOURCES = $(common_files) set_forward_list_test.cc test_forward_list.cc
set_forward_list_test_LDADD = @CHECK_LIBS@

set_generate_terms_test_SOURCES = $(common_files) set_generate_terms_test.cc test_generate_terms.cc
set_generate_terms_test_LDADD = @CHECK_LIBS@

set_geo_test_SOURCES = $(common_files) set_geo_test.cc test_geo.cc
set_geo_test_LDADD = @CHECK_LIBS@

set_hash_test_SOURCES = $(common_files) set_hash_test.cc test_hash.cc
set_hash_test_LDADD = @CHECK_LIBS@

set_htm_test_SOURCES = $(common_files) set_htm_test.cc test_htm.cc
set_htm_test_LDADD = @CHECK_LIBS@

set_lru_test_SOURCES = $(common_files) set_lru_test.cc test_lru.cc
set_lru_test_LDADD = @CHECK_LIBS@

set_msgpack_test_SOURCES = $(common_files) set_msgpack_test.cc test_msgpack.cc
set_msgpack_test_LDADD = @CHECK_LIBS@

set_patcher_test_SOURCES = $(common_files) set_patcher_test.cc test_patcher.cc
set_patcher_test_LDADD = @CHECK_LIBS@

set_query_test_SOURCES = $(common_files) set_query_test.cc test_query.cc
set_query_test_LDADD = @CHECK_LIBS@

set_queue_test_SOURCES = $(common_files) set_queue_test.cc test_queue.cc
set_queue_test_LDADD = @CHECK_LIBS@

set_serialise_stl_test_SOURCES = $(common_files) set_serialise_stl_test.cc test_serialise_stl.cc
set_serialise_stl_test_LDADD = @CHECK_LIBS@

set_serialise_unserialise_test_SOURCES = $(common_files) set_serialise_unserialise_test.cc test_serialise.cc
set_serialise_unserialise_test_LDADD = @CHECK_LIBS@

set_sort_test_SOURCES = $(common_files) set_sort_test.cc test_sort.cc
set_sort_test_LDADD = @CHECK_LIBS@

set_storage_test_SOURCES = $(common_files) set_storage_test.cc test_storage.cc
set_storage_test_LDADD = @CHECK_LIBS@

set_threadpool_test_SOURCES = $(common_files) set_threadpool_test.cc test_threadpool.cc
set_threadpool_test_LDADD = @CHECK_LIBS@

set_utils_test_SOURCES = $(common_files) set_utils_test.cc test_utils.cc
set_utils_test_LDADD = @CHECK_LIBS@

set_wkt_test_SOURCES = $(common_files) set_wkt_test.cc test_wkt_parser.cc
set_wkt_test_LDADD = @CHECK_LIBS@

set_wal_test_SOURCES = $(common_files) set_wal_test.cc test_wal.cc
set_wal_test_LDADD = @CHECK_LIBS@

AM_LDFLAGS = $(XAPIAN_LIBS)
AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
# FreeBSD not found check.h without -I option flag where the header is
AM_CXXFLAGS += -I/usr/local/include/
