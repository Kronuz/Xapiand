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

#include "log.h"                              // for L_CALL
#include "manager.h"                          // for XapiandManager
#include "database/pool.h"                    // for DatabasePool (database_pool)
#include "database/schemas_lru.h"             // for SchemasLRU (schemas)


DatabaseCleanup::DatabaseCleanup(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_) :
	Worker(parent_, ev_loop_, ev_flags_),
	cleanup(*ev_loop)
{
	cleanup.set<DatabaseCleanup, &DatabaseCleanup::cleanup_cb>(this);
}


DatabaseCleanup::~DatabaseCleanup() noexcept
{
	try {
		Worker::deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
DatabaseCleanup::shutdown_impl(long long asap, long long now)
{
	L_CALL("DatabaseCleanup::stop_impl(...)");

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
DatabaseCleanup::start_impl()
{
	L_CALL("DatabaseCleanup::start_impl()");

	Worker::start_impl();

	cleanup.repeat = 60.0;
	cleanup.again();
	L_EV("Start cleanup event");
}


void
DatabaseCleanup::stop_impl()
{
	L_CALL("DatabaseCleanup::stop_impl()");

	Worker::stop_impl();

	cleanup.stop();
	L_EV("Stop cleanup event");
}


void
DatabaseCleanup::cleanup_cb(ev::timer& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("DatabaseCleanup::cleanup_cb(<timer>, {:#04x} ({}))", revents, readable_revents(revents));

	XapiandManager::database_pool()->cleanup();

	XapiandManager::schemas()->cleanup();
}


void
DatabaseCleanup::operator()()
{
	L_CALL("DatabaseCleanup::operator()()");

	L_EV("Starting database cleanup loop...");
	run_loop();
	L_EV("Database cleanup loop ended!");

	detach(false);
}


std::string
DatabaseCleanup::__repr__() const
{
	return strings::format(STEEL_BLUE + "<DatabaseCleanup {{cnt:{}}}{}{}{}>",
		use_count(),
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}
