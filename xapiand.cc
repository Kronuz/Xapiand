/*
c++ xapiand.cc server.cc threadpool.cc -lev `xapian-config-1.3 --libs` `xapian-config-1.3 --cxxflags` -I../../ -I../../common -DXAPIAN_LIB_BUILD -oxapiand
*/

#include <stdlib.h>

#include "server.h"


int main(int argc, char **argv)
{
	int port = 8890;

	if (argc > 1)
		port = atoi(argv[1]);

	ThreadPool tp = ThreadPool(12);

	ev::default_loop loop;

	XapiandServer xapiand(port, &tp);

	loop.run(0);

	return 0;
}
