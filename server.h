#ifndef XAPIAND_INCLUDED_SERVER_H
#define XAPIAND_INCLUDED_SERVER_H

#include <ev++.h>

#include "threadpool.h"
#include "database.h"


const int XAPIAND_HTTP_PORT_DEFAULT = 8880;
const int XAPIAND_BINARY_PORT_DEFAULT = 8890;


class XapiandServer : public Task {
private:
	ev::dynamic_loop loop;
	ev::sig sig;

	ev::io http_io;
	int http_sock;
	int http_port;

	ev::io binary_io;
	int binary_sock;
	int binary_port;

	ThreadPool thread_pool;
	DatabasePool database_pool;

	void bind_http();
	void bind_binary();

public:
	void io_accept_http(ev::io &watcher, int revents);
	void io_accept_binary(ev::io &watcher, int revents);

	void signal_cb(ev::sig &signal, int revents);

	XapiandServer(int http_port_, int binary_port_, int thread_pool_size);
	~XapiandServer();
	
	void run();
};

#endif /* XAPIAND_INCLUDED_SERVER_H */
