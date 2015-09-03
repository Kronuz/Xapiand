ACLOCAL_AMFLAGS = -I ../m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = \
	cartesian.cc \
	cJSON.c \
	cJSON_Utils.c \
	client_base.cc \
	client_binary.cc \
	client_http.cc \
	database.cc \
	database_utils.cc \
	datetime.cc \
	endpoint.cc \
	endpoint_resolver.cc \
	exception.cc \
	fields.cc \
	generate_terms.cc \
	geometry.cc \
	geospatialrange.cc \
	htm.cc \
	http_parser.c \
	length.cc \
	lz4/lz4.c \
	lz4/lz4frame.c \
	lz4/lz4hc.c \
	lz4/xxhash.c \
	manager.cc \
	md5.cc \
	multivalue.cc \
	multivaluekeymaker.cc \
	multivaluerange.cc \
	replicator.cc \
	serialise.cc \
	server.cc \
	threadpool.cc \
	times.cc \
	utils.cc \
	wkt_parser.cc \
	xapiand.cc

noinst_HEADERS = \
	cartesian.h \
	cJSON.h \
	cJSON_Utils.h \
	client_base.h \
	client_binary.h \
	client_http.h \
	database.h \
	database_utils.h \
	datetime.h \
	endpoint.h \
	endpoint_resolver.h \
	exception.h \
	fields.h \
	generate_terms.h \
	geometry.h \
	geospatialrange.h \
	htm.h \
	http_parser.h \
	length.h \
	lru.h \
	lz4/lz4.h \
	lz4/lz4frame_static.h \
	lz4/lz4frame.h \
	lz4/lz4hc.h \
	lz4/xxhash.h \
	manager.h \
	md5.h \
	multivalue.h \
	multivaluekeymaker.h \
	multivaluerange.h \
	queue.h \
	replicator.h \
	serialise.cc \
	server.h \
	threadpool.h \
	times.h \
	utils.h \
	wkt_parser.h \
	worker.h \
	xapiand.h

EXTRA_DIST = Xapiand.xcodeproj/project.pbxproj

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
