ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = xapiand.cc \
	database.cc \
	server.cc \
	threadpool.cc \
	client_base.cc \
	client_binary.cc \
	client_http.cc \
	http_parser.c \
	cJSON.cc \
	utils.cc \
	$(XAPIAN_SRCDIR)/net/length.cc

noinst_HEADERS = xapiand.h \
	queue.h \
	endpoint.h \
	database.h \
	server.h \
	threadpool.h \
	client_base.h \
	client_binary.h \
	client_http.h \
	http_parser.h \
	cJSON.h \
	utils.h

EXTRA_DIST = Xapiand.xcodeproj/project.pbxproj

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = -std=c++11 \
	-DXAPIAN_LIB_BUILD \
	-I$(XAPIAN_SRCDIR)/ \
	-I$(XAPIAN_SRCDIR)/common
AM_CXXFLAGS += $(XAPIAN_CXXFLAGS)
