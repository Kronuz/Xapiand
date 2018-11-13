/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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
#include "log.h"                              // for L_OBJ
#include "tcp.h"                              // for TCP
#include "worker.h"                           // for Worker


template <typename ServerImpl>
class BaseServer : public TCP, public Worker {
	friend Worker;

protected:
	ev::io io;

	BaseServer(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port, const char* description, int flags) :
		TCP(port, description, flags),
		Worker(parent_, ev_loop_, ev_flags_),
		io(*ev_loop) {
		io.set<ServerImpl, &ServerImpl::io_accept_cb>(static_cast<ServerImpl*>(this));
	}

	~BaseServer() {
		TCP::close();

		Worker::deinit();
	}

	void shutdown_impl(long long asap, long long now) override {
		Worker::shutdown_impl(asap, now);

		stop(false);
		destroy(false);

		if (now != 0) {
			detach();
		}
	}

	void destroy_impl() override {
		Worker::destroy_impl();

		TCP::close();
	}

	void stop_impl() override {
		Worker::stop_impl();

		io.stop();
		L_EV("Stop server accept event");
	}
};
