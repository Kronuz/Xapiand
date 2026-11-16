/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Pins for the registry's tests: a request's pin by path, and a use with a pin of its own (as lock_shard makes one).

#include <string>
#include <thread>

#include "database/pool.h"
#include "database/shard.h"
#include "engine/host_asio.h"
#include "engine/sync.h"
#include "reactor.h"
#include "task.hh"

// The control loop a test's pools run their catalog's timers on, as a node's: one reactor on a thread of its own,
// for the whole test run.
inline engine::AsioHost& control_loop() {
	struct Loop {
		reactor::Reactor reactor;
		engine::AsioHost host{reactor.scheduler()};
		asio::executor_work_guard<asio::io_context::executor_type> guard{reactor.io().get_executor()};
		std::thread thread{[this] {
			host.bind_loop_thread();
			reactor.run();
		}};
		~Loop() {
			guard.reset();
			reactor.io().stop();
			thread.join();
		}
	};
	static Loop loop;
	return loop.host;
}

inline IndexPin pin_index(DatabasePool& pool, const std::string& path, int flags) {
	return engine::sync_wait(pool.pin(Endpoint(path), flags));
}

inline Shard use_index(const IndexPin& entry, int flags, task::cancel_token cancel = {}) {
	return Shard(engine::sync_wait(entry.pool()->pin(Endpoint(entry->path), flags)), flags, std::move(cancel));
}
