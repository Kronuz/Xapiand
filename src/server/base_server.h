/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <memory>                             // for std::shared_ptr

#include "ev/ev++.h"                          // for ev::io, ev::loop_ref
#include "tcp.h"                              // for TCP
#include "worker.h"                           // for Worker


class BaseServer : public TCP, public Worker {
	friend Worker;

protected:
	ev::io io;

	BaseServer(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* description, int flags);

	~BaseServer() noexcept;

	void destroy_impl() override;
	void stop_impl() override;

public:
	void operator()();
};


template <typename ServerImpl>
class MetaBaseServer : public BaseServer {
public:
	MetaBaseServer(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* description, int flags) :
		BaseServer(parent_, ev_loop_, ev_flags_, description, flags) {
		io.set<ServerImpl, &ServerImpl::io_accept_cb>(static_cast<ServerImpl*>(this));
	}
};
