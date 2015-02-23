ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = xapiand
xapiand_SOURCES = xapiand.cc \
	database.cc \
	server.cc \
	threadpool.cc \
	../../net/length.cc

AM_LDFLAGS = -lev
AM_LDFLAGS += $(XAPIAN_LIBS)

AM_CXXFLAGS = -DXAPIAN_LIB_BUILD \
	-I../../ \
	-I../../common
AM_CXXFLAGS += $(XAPIAN_CXXFLAGS)
