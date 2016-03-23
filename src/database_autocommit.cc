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
std::mutex DatabaseAutocommit::statuses_mtx;
std::condition_variable DatabaseAutocommit::wakeup_signal;
std::unordered_map<Endpoints, DatabaseAutocommit::Status> DatabaseAutocommit::statuses;
std::atomic<std::time_t> DatabaseAutocommit::next_wakeup_time(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + 10s));


std::chrono::time_point<std::chrono::system_clock>
DatabaseAutocommit::Status::next_wakeup_time()
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
		DatabaseAutocommit::wakeup_signal.wait_until(lk, std::chrono::system_clock::from_time_t(DatabaseAutocommit::next_wakeup_time.load()));

		{
			std::unique_lock<std::mutex> statuses_lk(DatabaseAutocommit::statuses_mtx);

			auto now = std::chrono::system_clock::now();
			DatabaseAutocommit::next_wakeup_time.store(std::chrono::system_clock::to_time_t(now + 20s));

			for (auto it = DatabaseAutocommit::statuses.begin(); it != DatabaseAutocommit::statuses.end(); ) {
				auto status = it->second;
				if (status.weak_database.lock()) {
					auto next_wakeup_time = status.next_wakeup_time();
					if (next_wakeup_time <= now) {
						DatabaseAutocommit::statuses.erase(it);
						statuses_lk.unlock();
						lk.unlock();

						auto endpoints = it->first;
						std::shared_ptr<Database> database;
						if (manager()->database_pool.checkout(database, endpoints, DB_WRITABLE)) {
							if (database->commit()) {
								L_DEBUG(this, "Autocommit: %s%s", endpoints.as_string().c_str(), next_wakeup_time == status.max_commit_time ? " (forced)" : "");
							}
							manager()->database_pool.checkin(database);
						}

						lk.lock();
						statuses_lk.lock();
						it = DatabaseAutocommit::statuses.begin();
					} else if (std::chrono::system_clock::from_time_t(DatabaseAutocommit::next_wakeup_time.load()) > next_wakeup_time) {
						DatabaseAutocommit::next_wakeup_time.store(std::chrono::system_clock::to_time_t(next_wakeup_time));
						++it;
					} else {
						++it;
					}
				} else {
					it = DatabaseAutocommit::statuses.erase(it);
				}
			}
		}
	}

	detach_impl();
}


void
DatabaseAutocommit::commit(const std::shared_ptr<Database>& database)
{
	std::lock_guard<std::mutex> statuses_lk(DatabaseAutocommit::statuses_mtx);
	DatabaseAutocommit::Status& status = DatabaseAutocommit::statuses[database->endpoints];

	auto now = std::chrono::system_clock::now();
	if (!status.weak_database.lock()) {
		status.weak_database = database;
		status.max_commit_time = now + 9s;
	}
	status.commit_time = now + 3s;

	if (std::chrono::system_clock::from_time_t(DatabaseAutocommit::next_wakeup_time.load()) > status.next_wakeup_time()) {
		DatabaseAutocommit::wakeup_signal.notify_one();
	}
}
