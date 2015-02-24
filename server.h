#ifndef XAPIAND_INCLUDED_SERVER_H
#define XAPIAND_INCLUDED_SERVER_H

#include <unistd.h>
#include <ev++.h>

#include "queue.h"
#include "threadpool.h"
#include "endpoint.h"
#include "database.h"

#include "http_parser.h"
#include "net/remoteserver.h"


const int XAPIAND_HTTP_PORT_DEFAULT = 8880;
const int XAPIAND_BINARY_PORT_DEFAULT = 8890;


struct Buffer;


class XapiandServer {
private:
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

	static void signal_cb(ev::sig &signal, int revents);

	XapiandServer(int http_port_, int binary_port_, int thread_pool_size);
	~XapiandServer();
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

//
//   A single instance of a non-blocking Xapiand HTTP protocol handler
//
class HttpClient : public BaseClient {
	struct http_parser parser;

	void write_cb(ev::io &watcher);
	void read_cb(ev::io &watcher);

	static const http_parser_settings settings;
	static int on_info(http_parser* p);
	static int on_data(http_parser* p, const char *at, size_t length);

public:
	void run() {}

	HttpClient(int s, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
	~HttpClient();
};


//
//   A single instance of a non-blocking Xapiand binary protocol handler
//
class BinaryClient : public BaseClient, public RemoteProtocol {
private:
	ev::async async;

	Database *database;
	std::vector<std::string> dbpaths;

	// Buffers that are pending write
	std::string buffer;
	Queue<Buffer *> messages_queue;

	void write_cb(ev::io &watcher);
	void read_cb(ev::io &watcher);

	void async_cb(ev::async &watcher, int revents);

public:
    void run();

	message_type get_message(double timeout, std::string & result, message_type required_type = MSG_MAX);
	void send_message(reply_type type, const std::string &message);
	void send_message(reply_type type, const std::string &message, double end_time);

	Xapian::Database * get_db(bool);
	void release_db(Xapian::Database *);
	void select_db(const std::vector<std::string> &, bool);

	BinaryClient(int s, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
	~BinaryClient();
};

#endif /* XAPIAND_INCLUDED_SERVER_H */
