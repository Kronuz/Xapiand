#ifndef XAPIAND_INCLUDED_CLIENT_BINARY_H
#define XAPIAND_INCLUDED_CLIENT_BINARY_H

#include "net/remoteserver.h"

#include "client_base.h"

//
//   A single instance of a non-blocking Xapiand binary protocol handler
//
class BinaryClient : public BaseClient, public RemoteProtocol {
private:
	Database *database;
	std::vector<std::string> dbpaths;

	// Buffers that are pending write
	std::string buffer;
	Queue<Buffer *> messages_queue;

	void read_cb(ev::io &watcher);

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

#endif /* XAPIAND_INCLUDED_CLIENT_BINARY_H */
