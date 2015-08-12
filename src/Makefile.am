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
	datetime.cc \
	endpoint.cc \
	endpoint_resolver.cc \
	exception.cc \
	fields.cc \
	geometry.cc \
	htm.cc \
	http_parser.c \
	length.cc \
	manager.cc \
	md5.cc \
	multivalue.cc \
	multivaluekeymaker.cc \
	replicator.cc \
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
	datetime.h \
	endpoint.h \
	endpoint_resolver.h \
	exception.h \
	fields.h \
	geometry.h \
	htm.h \
	http_parser.h \
	length.h \
	lru.h \
	manager.h \
	md5.h \
	multivalue.h \
	multivaluekeymaker.h \
	queue.h \
	replicator.h \
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
