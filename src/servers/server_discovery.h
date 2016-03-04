/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "server_base.h"

#ifdef XAPIAND_CLUSTERING

#include "discovery.h"


// Discovery Server
class DiscoveryServer : public BaseServer {
	friend Discovery;

	std::shared_ptr<Discovery> discovery;

	void _wave(bool heartbeat, const std::string& message);
	void _db_wave(bool bossy, const std::string& message);

	void discovery_server(Discovery::Message type, const std::string &message);

	void heartbeat(const std::string& message);
	void hello(const std::string& message);
	void wave(const std::string& message);
	void sneer(const std::string& message);
	void bye(const std::string& message);
	void db(const std::string& message);
	void db_wave(const std::string& message);
	void bossy_db_wave(const std::string& message);
	void db_updated(const std::string& message);

public:
	DiscoveryServer(const std::shared_ptr<XapiandServer>& server_, ev::loop_ref *loop_, const std::shared_ptr<Discovery> &discovery_);
	~DiscoveryServer();

	void io_accept_cb(ev::io &watcher, int revents) override;
};

#endif
