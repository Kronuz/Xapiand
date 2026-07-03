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

#include "database/cleanup.h"

#include <chrono>                             // for std::chrono::seconds

#include "log.h"                              // for L_CALL
#include "manager.h"                          // for XapiandManager
#include "database/pool.h"                    // for DatabasePool (database_pool)
#include "database/schemas_lru.h"             // for SchemasLRU (schemas)


DatabaseCleanup::DatabaseCleanup() :
	finished(false)
{
}


DatabaseCleanup::~DatabaseCleanup() noexcept
{
	try {
		finish();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
DatabaseCleanup::finish()
{
	L_CALL("DatabaseCleanup::finish()");

	{
		std::lock_guard<std::mutex> lk(mtx);
		finished.store(true, std::memory_order_release);
	}
	wakeup.notify_all();
}


void
DatabaseCleanup::operator()()
{
	L_CALL("DatabaseCleanup::operator()()");

	L_EV("Starting database cleanup loop...");
	std::unique_lock<std::mutex> lk(mtx);
	while (!finished.load(std::memory_order_acquire)) {
		wakeup.wait_for(lk, std::chrono::seconds(60), [this] {
			return finished.load(std::memory_order_acquire);
		});
		if (finished.load(std::memory_order_acquire)) {
			break;
		}
		lk.unlock();
		auto manager = XapiandManager::manager();
		if (manager) {
			manager->database_pool->cleanup();
			manager->schemas->cleanup();
		}
		lk.lock();
	}
	L_EV("Database cleanup loop ended!");
}
