/*
 * Copyright (C) 2018 Dubalu LLC. All rights reserved.
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
#include "thread.hh"                          // for Thread, ThreadPolicyType::*
#include "worker.h"                           // for Worker


class DatabaseCleanup : public Worker, public Thread<DatabaseCleanup, ThreadPolicyType::regular> {
	friend Worker;

protected:
	ev::timer cleanup;

	void shutdown_impl(long long asap, long long now) override;
	void start_impl() override;
	void stop_impl() override;

	void cleanup_cb(ev::timer& watcher, int revents);

public:
	DatabaseCleanup(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_);

	~DatabaseCleanup();

	const char* name() const noexcept {
		return "DBCL";
	}

	void operator()();
};
