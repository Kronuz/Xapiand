#ifndef XAPIAND_INCLUDED_SERVER_H
#define XAPIAND_INCLUDED_SERVER_H

#include <unistd.h>
#include <ev++.h>
#include <list>

#include "threadpool.h"

#include "net/remoteserver.h"

struct Buffer;


class XapiandServer {
private:
	ev::io io;
	ev::sig sig;

	int sock;

	ThreadPool *thread_pool;

public:
	void io_accept(ev::io &watcher, int revents);

	static void signal_cb(ev::sig &signal, int revents);

	XapiandServer(int port, ThreadPool *thread_pool);

	virtual ~XapiandServer();
};


//
//   A single instance of a non-blocking Xapiand handler
//
class XapiandClient {
private:
	ev::io io;
	ev::async async;
	ev::sig sig;

	int sock;

	static int total_clients;
	RemoteServer *server;
	ThreadPool *thread_pool;
	std::vector<std::string> dbpaths;

	// Buffers that are pending write
	std::list<Buffer*> write_queue;

	void async_cb(ev::async &watcher, int revents);

	// Generic callback
	void callback(ev::io &watcher, int revents);

	// Socket is writable
	void write_cb(ev::io &watcher);

	// Receive message from client socket
	void read_cb(ev::io &watcher);

	void signal_cb(ev::sig &signal, int revents);

	// effictivly a close and a destroy
	virtual ~XapiandClient();

public:
	void send(std::string buffer);

	XapiandClient(int s, ThreadPool *thread_pool);
};

#endif /* XAPIAND_INCLUDED_SERVER_H */
