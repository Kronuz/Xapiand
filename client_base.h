#ifndef XAPIAND_INCLUDED_CLIENT_BASE_H
#define XAPIAND_INCLUDED_CLIENT_BASE_H

#include <ev++.h>

#include "threadpool.h"
#include "database.h"

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//

struct Buffer {
	char type;
	char *data;
	size_t len;
	size_t pos;

	Buffer(char type_, const char *bytes, size_t nbytes) {
		pos = 0;
		len = nbytes;
		type = type_;
		data = new char[nbytes];
		memcpy(data, bytes, nbytes);
	}

	virtual ~Buffer() {
		delete [] data;
	}

	char *dpos() {
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

	int sock;
	static int total_clients;

	ThreadPool *thread_pool;
	DatabasePool *database_pool;

	pthread_mutex_t qmtx;

	Endpoints endpoints;

	Queue<Buffer *> write_queue;

	void signal_cb(ev::sig &signal, int revents);

	// Generic callback
	void callback(ev::io &watcher, int revents);

	// Socket is writable
	virtual void write_cb(ev::io &watcher) = 0;

	// Receive message from client socket
	virtual void read_cb(ev::io &watcher) = 0;

public:
	virtual void run() = 0;

	BaseClient(int s, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
	virtual ~BaseClient();
};



class ClientWorker : public Task {
private:
	BaseClient *client;

public:
	ClientWorker(BaseClient *client_) : Task(), client(client_) {}

	~ClientWorker() {}

	virtual void run() {
		client->run();
	}
};

#endif  /* XAPIAND_INCLUDED_CLIENT_BASE_H */
