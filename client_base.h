#ifndef XAPIAND_INCLUDED_CLIENT_BASE_H
#define XAPIAND_INCLUDED_CLIENT_BASE_H

#include <ev++.h>

#include "threadpool.h"
#include "database.h"

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//

class Buffer {
	char *data;
	size_t len;

public:
	size_t pos;
	char type;

	Buffer(char type_, const char *bytes, size_t nbytes)
		: pos(0),
		  len(nbytes),
		  type(type_),
		  data(new char [nbytes])
	{
		memcpy(data, bytes, nbytes);
	}

	virtual ~Buffer() {
		delete [] data;
	}

	const char *dpos() {
		return data + pos;
	}

	size_t nbytes() {
		return len - pos;
	}
};


class BaseClient {
protected:
	ev::io io;
	ev::sig sig;
	ev::async async;

	bool destroyed;
	bool closed;
	int sock;
	static int total_clients;

	DatabasePool *database_pool;

	Endpoints endpoints;

	Queue<Buffer *> write_queue;

	void signal_cb(ev::sig &signal, int revents);
	void async_cb(ev::async &watcher, int revents);

	// Generic callback
	void io_cb(ev::io &watcher, int revents);

	// Socket is writable
	void write_cb(ev::io &watcher);

	// Receive message from client socket
	void read_cb(ev::io &watcher);
	
	virtual void on_read(const char *buf, ssize_t received) = 0;

	inline void write(const char *buf)
	{
		write(buf, strlen(buf));
	}
	
	inline void write(const std::string &buf)
	{
		write(buf.c_str(), buf.size());
	}

	void write(const char *buf, size_t buf_size);
	
	void close();
	void destroy();

public:
	BaseClient(ev::loop_ref &loop, int s, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
	virtual ~BaseClient();
};


#endif  /* XAPIAND_INCLUDED_CLIENT_BASE_H */
