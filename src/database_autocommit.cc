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

#include "database_autocommit.h"


std::mutex DatabaseAutocommit::mtx;
std::condition_variable DatabaseAutocommit::wakeup_signal;
std::unordered_map<Endpoints, DatabaseCommitStatus> DatabaseAutocommit::databases;
std::chrono::time_point<std::chrono::system_clock> DatabaseAutocommit::next_wakeup_time(std::chrono::system_clock::now() + 10s);


std::chrono::time_point<std::chrono::system_clock>
DatabaseCommitStatus::next_wakeup_time()
{
	return max_commit_time < commit_time ? max_commit_time : commit_time;
}


DatabaseAutocommit::DatabaseAutocommit(const std::shared_ptr<XapiandManager>& manager_, ev::loop_ref *loop_)
	: Worker(std::move(manager_), loop_),
	  running(true)
{
	L_OBJ(this, "CREATED AUTOCOMMIT!");
}


DatabaseAutocommit::~DatabaseAutocommit()
{
	destroy_impl();

	L_OBJ(this , "DELETED AUTOCOMMIT!");
}


void
DatabaseAutocommit::destroy_impl()
{
	running.store(false);
	wakeup_signal.notify_all();
}


void
DatabaseAutocommit::shutdown_impl(time_t asap, time_t now)
{
	L_OBJ(this , "SHUTDOWN AUTOCOMMIT! (%d %d)", asap, now);

	Worker::shutdown_impl(asap, now);

	// Call implementation directly, as we don't use a loop. Object gets
	// detached when run() ends:

	destroy_impl();
}


void
DatabaseAutocommit::run()
{
	while (running) {
		std::unique_lock<std::mutex> lk(DatabaseAutocommit::mtx);
		DatabaseAutocommit::wakeup_signal.wait_until(lk, DatabaseAutocommit::next_wakeup_time);

		auto now = std::chrono::system_clock::now();
		DatabaseAutocommit::next_wakeup_time = now + 20s;

		for (auto it = DatabaseAutocommit::databases.begin(); it != DatabaseAutocommit::databases.end(); ) {
			auto endpoints = it->first;
			auto status = it->second;
			if (status.weak_database.lock()) {
				auto next_wakeup_time = status.next_wakeup_time();
				if (next_wakeup_time <= now) {
					DatabaseAutocommit::databases.erase(it);
					lk.unlock();
					std::shared_ptr<Database> database;
					if (manager()->database_pool.checkout(database, endpoints, DB_WRITABLE)) {
						if (database->commit()) {
							L_DEBUG(this, "Autocommit: %s%s", endpoints.as_string().c_str(), next_wakeup_time == status.max_commit_time ? " (forced)" : "");
						}
						manager()->database_pool.checkin(database);
					}
					lk.lock();
					it = DatabaseAutocommit::databases.begin();
				} else if (DatabaseAutocommit::next_wakeup_time > next_wakeup_time) {
					DatabaseAutocommit::next_wakeup_time = next_wakeup_time;
					++it;
				} else {
					++it;
				}
			} else {
				it = DatabaseAutocommit::databases.erase(it);
			}
		}
	}

	detach_impl();
}


void
DatabaseAutocommit::signal_changed(const std::shared_ptr<Database>& database)
{
	// Window open perhaps
	// std::unique_lock<std::mutex> lk(DatabaseAutocommit::mtx, std::defer_lock);

	DatabaseCommitStatus& status = DatabaseAutocommit::databases[database->endpoints];

	auto now = std::chrono::system_clock::now();
	if (!status.weak_database.lock()) {
		status.weak_database = database;
		status.max_commit_time = now + 9s;
	}
	status.commit_time = now + 3s;

	if (DatabaseAutocommit::next_wakeup_time > status.next_wakeup_time()) {
		DatabaseAutocommit::wakeup_signal.notify_one();
	}
}
