ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = xapiand.cc \
	endpoint.cc \
	database.cc \
	manager.cc \
	server.cc \
	threadpool.cc \
	client_base.cc \
	client_binary.cc \
	client_http.cc \
	http_parser.c \
	cJSON.cc \
	md5.cc \
	utils.cc \
	length.cc \
	fields.cc

noinst_HEADERS = xapiand.h \
	queue.h \
	endpoint.h \
	database.h \
	manager.h \
	server.h \
	threadpool.h \
	client_base.h \
	client_binary.h \
	client_http.h \
	http_parser.h \
	cJSON.h \
	md5.h \
	utils.h \
	length.h \
	fields.h

EXTRA_DIST = Xapiand.xcodeproj/project.pbxproj

AM_LDFLAGS = $(XAPIAN_LIBS) -lpcre

AM_CXXFLAGS = -std=c++11
AM_CXXFLAGS += $(XAPIAN_CXXFLAGS)
