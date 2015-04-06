/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef XAPIAND_INCLUDED_CLIENT_BINARY_H
#define XAPIAND_INCLUDED_CLIENT_BINARY_H

#include "xapian.h"

#include "client_base.h"

#include <unordered_map>

//
//   A single instance of a non-blocking Xapiand binary protocol handler
//
class BinaryClient : public BaseClient, public RemoteProtocol {
private:
	bool running;
	bool started;

	std::unordered_map<Xapian::Database *, Database *> databases;

	// Buffers that are pending write
	std::string buffer;
	Queue<Buffer *> messages_queue;

	void on_read(const char *buf, ssize_t received);

public:
	message_type get_message(double timeout, std::string & result, message_type required_type = MSG_MAX);
	void send_message(reply_type type, const std::string &message);
	void send_message(reply_type type, const std::string &message, double end_time);

	Xapian::Database * get_db(bool);
	void release_db(Xapian::Database *);
	void select_db(const std::vector<std::string> &, bool, int);
	void shutdown();

	BinaryClient(XapiandServer *server_, ev::loop_ref *loop, int s, DatabasePool *database_pool_, ThreadPool *thread_pool_, double active_timeout_, double idle_timeout_);
	~BinaryClient();

	void run();
};

#endif /* XAPIAND_INCLUDED_CLIENT_BINARY_H */
