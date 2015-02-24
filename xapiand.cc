/*
c++ xapiand.cc server.cc threadpool.cc ../../net/length.cc -lev `xapian-config-1.3 --libs` `xapian-config-1.3 --cxxflags` -I../../ -I../../common -DXAPIAN_LIB_BUILD -oxapiand
*/

#include <stdlib.h>

#include "config.h"
#include "server.h"


int main(int argc, char **argv)
{
	int http_port = XAPIAND_HTTP_PORT_DEFAULT;
	int binary_port = XAPIAND_BINARY_PORT_DEFAULT;

	if (argc > 2) {
		http_port = atoi(argv[1]);
		binary_port = atoi(argv[2]);
	}

	ev::default_loop loop;

	XapiandServer xapiand(http_port, binary_port, 12);

	loop.run(0);

	return 0;
}
