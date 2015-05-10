ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = xapiand.cc \
	endpoint.cc \
	database.cc \
	manager.cc \
	discovery.cc \
	server.cc \
	threadpool.cc \
	client_base.cc \
	client_binary.cc \
	client_http.cc \
	http_parser.c \
	cJSON.cc \
	cJSON_Utils.c \
	md5.cc \
	utils.cc \
	length.cc \
	fields.cc \
	multivalue.cc \
	tests.cc

noinst_HEADERS = xapiand.h \
	queue.h \
	endpoint.h \
	database.h \
	manager.h \
	discovery.h \
	server.h \
	threadpool.h \
	client_base.h \
	client_binary.h \
	client_http.h \
	http_parser.h \
	cJSON.h \
	cJSON_Utils.h \
	md5.h \
	utils.h \
	length.h \
	fields.h \
	multivalue.h \
	tests.h

EXTRA_DIST = Xapiand.xcodeproj/project.pbxproj

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = $(XAPIAN_CXXFLAGS)
