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

#include "base_server.h"

#include "log.h"                              // for L_CALL


BaseServer::BaseServer(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const char* description, int flags) :
	TCP(description, flags),
	Worker(parent_, ev_loop_, ev_flags_),
	io(*ev_loop)
{
}


BaseServer::~BaseServer() noexcept
{
	try {
		TCP::close();

		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
BaseServer::shutdown_impl(long long asap, long long now)
{
	L_CALL("BaseServer::stop_impl(...)");

	Worker::shutdown_impl(asap, now);

	if (asap) {
		stop(false);
		destroy(false);

		if (now != 0) {
			if (is_runner()) {
				break_loop(false);
			} else {
				detach(false);
			}
		}
	}
}


void
BaseServer::destroy_impl()
{
	L_CALL("BaseServer::destroy_impl()");

	Worker::destroy_impl();

	TCP::close();
}


void
BaseServer::stop_impl()
{
	L_CALL("BaseServer::stop_impl()");

	Worker::stop_impl();

	io.stop();
	L_EV("Stop server accept event");
}


void
BaseServer::operator()()
{
	L_CALL("BaseServer::operator()()");

	L_EV("Starting server loop...");
	run_loop();
	L_EV("Server loop ended!");

	detach();
}
