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


DatabaseAutocommit::DatabaseAutocommit(const std::shared_ptr<XapiandManager>& manager_)
	: running(true),
	  manager(manager_)
{
		L_OBJ(this, "CREATED DATABASE AUTOCOMMIT! [%llx]", this);
}


DatabaseAutocommit::~DatabaseAutocommit()
{
	running.store(false);
	L_OBJ(this , "DELETED DATABASE AUTOCOMMIT! [%llx]", this);
}


void
DatabaseAutocommit::shutdown()
{
	running.store(false);
	wakeup_signal.notify_all();
}


void
DatabaseAutocommit::signal_changed(const std::shared_ptr<Database>& database)
{
	//Window open perhaps
	//std::unique_lock<std::mutex> lk(DatabaseAutocommit::mtx, std::defer_lock);

	DatabaseCommitStatus& status = DatabaseAutocommit::databases[database->endpoints];

	auto now = std::chrono::system_clock::now();
	if (!status.database.lock()) {
		status.database = database;
		status.max_commit_time = now + 9s;
	}
	status.commit_time = now + 3s;

	if (DatabaseAutocommit::next_wakeup_time > status.next_wakeup_time()) {
		DatabaseAutocommit::wakeup_signal.notify_one();
	}
}


void
DatabaseAutocommit::run()
{
	L_DEBUG(this, "Committer started...");
	while (running) {
		std::unique_lock<std::mutex> lk(DatabaseAutocommit::mtx);
		DatabaseAutocommit::wakeup_signal.wait_until(lk, DatabaseAutocommit::next_wakeup_time);

		auto now = std::chrono::system_clock::now();
		DatabaseAutocommit::next_wakeup_time = now + 20s;

		for (auto it = DatabaseAutocommit::databases.begin(); it != DatabaseAutocommit::databases.end(); ) {
			auto endpoints = it->first;
			auto status = it->second;
			if (status.database.lock()) {
				auto next_wakeup_time = status.next_wakeup_time();
				if (next_wakeup_time <= now) {
					DatabaseAutocommit::databases.erase(it);
					lk.unlock();
					std::shared_ptr<Database> database;
					if (manager->database_pool.checkout(database, endpoints, DB_WRITABLE)) {
						database->commit();
						manager->database_pool.checkin(database);
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
	L_DEBUG(this, "Committer ended!...");
}
