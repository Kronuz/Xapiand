#ifndef XAPIAND_INCLUDED_SERVER_H
#define XAPIAND_INCLUDED_SERVER_H

#include <list>
#include <unordered_map>

#include <unistd.h>
#include <ev++.h>

#include "queue.h"
#include "threadpool.h"
#include "endpoint.h"

#include "net/remoteserver.h"

struct Buffer;


#include <queue>
#include "xapian.h"

struct Database {
	size_t hash;
	Endpoints endpoints;

	Xapian::Database *db;

	~Database() {
		delete db;
	}
};

class DatabaseQueue : public Queue<Database *> {
public:
	~DatabaseQueue() {
//		std::queue<Database *>::const_iterator i(queue.begin());
//		for (; i != databases.end(); ++i) {
//			(*i).second.finish();
//		}		
	}
};

class DatabasePool {
private:
	bool finished = false;
	std::unordered_map<size_t, DatabaseQueue> databases;
	pthread_mutex_t qmtx;

public:
	DatabasePool() {
		pthread_mutex_init(&qmtx, 0);
	}

	~DatabasePool() {
		pthread_mutex_lock(&qmtx);

		finished = true;

		pthread_mutex_lock(&qmtx);

		pthread_mutex_destroy(&qmtx);
	}
	
	bool checkout(Database **database, Endpoints &endpoints, bool writable) {
		Database *database_ = NULL;

		pthread_mutex_lock(&qmtx);

		if (!finished && *database == NULL) {
			size_t hash = endpoints.hash(writable);
			DatabaseQueue &queue = databases[hash];

			if (!queue.pop(database_, 0)) {
				database_ = new Database();
				database_->endpoints = endpoints;
				database_->hash = hash;
				if (writable) {
					database_->db = new Xapian::WritableDatabase(endpoints[0].path, Xapian::DB_CREATE_OR_OPEN);
				} else {
					database_->db = new Xapian::Database(endpoints[0].path, Xapian::DB_CREATE_OR_OPEN);
					if (!writable) {
						std::vector<Endpoint>::const_iterator i(endpoints.begin());
						for (++i; i != endpoints.end(); ++i) {
							database_->db->add_database(Xapian::Database((*i).path));
						}
					} else if (endpoints.size() != 1) {
						printf("ERROR: Expecting exactly one database.");
					}
				}
			}
			*database = database_;
		}
	
		pthread_mutex_unlock(&qmtx);
		
		return database_ != NULL;
	}
	void checkin(Database **database) {
		pthread_mutex_lock(&qmtx);

		DatabaseQueue &queue = databases[(*database)->hash];

		queue.push(*database);

		*database = NULL;

		pthread_mutex_unlock(&qmtx);
	}
};


class XapiandServer {
private:
	ev::io io;
	ev::sig sig;

	int sock;
	ThreadPool thread_pool;
	DatabasePool database_pool;

public:
	void io_accept(ev::io &watcher, int revents);

	static void signal_cb(ev::sig &signal, int revents);

	XapiandServer(int port, int thread_pool_size);

	virtual ~XapiandServer();
};


//
//   A single instance of a non-blocking Xapiand handler
//
class XapiandClient : public RemoteProtocol {
private:
	ev::io io;
	ev::async async;
	ev::sig sig;

	int sock;
	ThreadPool *thread_pool;
	DatabasePool *database_pool;
	Database *database;

	std::vector<std::string> dbpaths;
	Endpoints endpoints;

	static int total_clients;

	pthread_mutex_t qmtx;

	// Buffers that are pending write
	Queue<Buffer *> messages_queue;
	std::list<Buffer *> write_queue;
	std::string buffer;

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
    void run();

	message_type get_message(double timeout, std::string & result, message_type required_type = MSG_MAX);
	void send_message(reply_type type, const std::string &message);
	void send_message(reply_type type, const std::string &message, double end_time);
	
	Xapian::Database * get_db(bool);
	void release_db(Xapian::Database *);
	void select_db(const std::vector<std::string> &, bool);
	
	XapiandClient(int s, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_);
};

#endif /* XAPIAND_INCLUDED_SERVER_H */
