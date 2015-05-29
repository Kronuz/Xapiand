ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = \
	cJSON.c \
	cJSON_Utils.c \
	client_base.cc \
	client_binary.cc \
	client_http.cc \
	database.cc \
	endpoint.cc \
	endpoint_resolver.cc \
	fields.cc \
	http_parser.c \
	length.cc \
	\
	manager.cc \
	md5.cc \
	multivalue.cc \
	\
	replicator.cc \
	server.cc \
	tests.cc \
	threadpool.cc \
	times.c \
	utils.cc \
	\
	xapiand.cc

noinst_HEADERS = \
	cJSON.h \
	cJSON_Utils.h \
	client_base.h \
	client_binary.h \
	client_http.h \
	database.h \
	endpoint.h \
	endpoint_resolver.h \
	fields.h \
	http_parser.h \
	length.h \
	lru.h \
	manager.h \
	md5.h \
	multivalue.h \
	queue.h \
	replicator.h \
	server.h \
	tests.h \
	threadpool.h \
	times.h \
	utils.h \
	worker.h \
	xapiand.h

EXTRA_DIST = Xapiand.xcodeproj/project.pbxproj

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
