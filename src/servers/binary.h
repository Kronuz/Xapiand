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

#include "tcp_base.h"

#ifdef XAPIAND_CLUSTERING

#include "server_binary.h"
#include "../endpoint.h"
#include "../ev/ev++.h"


// Configuration data for Binary
class Binary : public BaseTCP {
	friend BinaryServer;

	std::mutex bsmtx;
	void async_signal_send();

	std::vector<std::weak_ptr<BinaryServer>> servers;
	TaskQueue<const std::shared_ptr<BinaryServer>&> tasks;

public:
	Binary(const std::shared_ptr<XapiandManager>& manager_, int port_);
	~Binary();

	std::string getDescription() const noexcept override;

	int connection_socket();

	void add_server(const std::shared_ptr<BinaryServer> &server);

	std::future<bool> trigger_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint);
};


#endif  /* XAPIAND_CLUSTERING */
