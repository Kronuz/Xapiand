ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = xapiand.cc \
	database.cc \
	server.cc \
	threadpool.cc \
	$(XAPIAN_SRCDIR)/net/length.cc

noinst_HEADERS = database.h \
	endpoint.h \
	queue.h \
	server.h \
	threadpool.h

AM_LDFLAGS = $(XAPIAN_LIBS)

AM_CXXFLAGS = -DXAPIAN_LIB_BUILD \
	-I$(XAPIAN_SRCDIR)/ \
	-I$(XAPIAN_SRCDIR)/common
AM_CXXFLAGS += $(XAPIAN_CXXFLAGS)
