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

#include "database_autocommit.h"

#include <utility>                           // for std::move

#include "database.h"                        // for Database
#include "database_handler.h"                // for DatabaseHandler
#include "endpoint.h"                        // for Endpoints
#include "log.h"                             // for L_OBJ, L_CALL, L_DEBUG, L_WARNING
#include "repr.hh"                           // for repr
#include "string.hh"                         // for string::from_delta
#include "time_point.hh"                     // for time_point_to_ullong


#define NORMALY_AUTOCOMMIT_AFTER       1s
#define WHEN_BUSY_AUTOCOMMIT_AFTER     3s
#define FORCE_AUTOCOMMIT_AFTER         9s


std::mutex DatabaseAutocommit::statuses_mtx;
std::unordered_map<Endpoints, DatabaseAutocommit::Status> DatabaseAutocommit::statuses;


DatabaseAutocommit::DatabaseAutocommit(bool forced_, Endpoints endpoints_, std::weak_ptr<const Database> weak_database_)
	: forced(forced_),
	  endpoints(std::move(endpoints_)),
	  weak_database(std::move(weak_database_)) { }


void
DatabaseAutocommit::commit(const std::shared_ptr<Database>& database)
{
	L_CALL("DatabaseAutocommit::commit(<database>)");

	std::shared_ptr<DatabaseAutocommit> task;
	unsigned long long next_wakeup_time;

	{
		auto now = std::chrono::system_clock::now();

		std::lock_guard<std::mutex> statuses_lk(DatabaseAutocommit::statuses_mtx);
		auto it = DatabaseAutocommit::statuses.find(database->endpoints);

		DatabaseAutocommit::Status* status;
		if (it == DatabaseAutocommit::statuses.end()) {
			auto& status_ref = DatabaseAutocommit::statuses[database->endpoints] = {
				nullptr,
				time_point_to_ullong(now + FORCE_AUTOCOMMIT_AFTER)
			};
			status = &status_ref;
			next_wakeup_time = time_point_to_ullong(now + NORMALY_AUTOCOMMIT_AFTER);
		} else {
			status = &(it->second);
			next_wakeup_time = time_point_to_ullong(now + WHEN_BUSY_AUTOCOMMIT_AFTER);
		}

		bool forced;
		if (next_wakeup_time > status->max_wakeup_time) {
			next_wakeup_time = status->max_wakeup_time;
			forced = true;
		} else {
			forced = false;
		}

		if (status->task) {
			if (status->task->wakeup_time == next_wakeup_time) {
				return;
			}
			status->task->clear();
		}
		status->task = std::make_shared<DatabaseAutocommit>(forced, database->endpoints, database);
		task = status->task;
	}

	scheduler().add(task, next_wakeup_time);
}


void
DatabaseAutocommit::run()
{
	L_CALL("DatabaseAutocommit::run()");
	L_DEBUG_HOOK("DatabaseAutocommit::run", "DatabaseAutocommit::run()");

	{
		std::lock_guard<std::mutex> statuses_lk(DatabaseAutocommit::statuses_mtx);
		DatabaseAutocommit::statuses.erase(endpoints);
	}

	if (weak_database.lock()) {
		auto start = std::chrono::system_clock::now();

		std::string error;
		try {

			DatabaseHandler db_handler(endpoints, DB_WRITABLE);
			db_handler.commit();

		} catch (const Exception& exc) {
			error = exc.get_message();
		} catch (const Xapian::Error& exc) {
			error = exc.get_description();
		}

		auto end = std::chrono::system_clock::now();

		if (error.empty()) {
			L_DEBUG("Autocommit%s of %s succeeded after %s", forced ? " (forced)" : "", repr(endpoints.to_string()), string::from_delta(start, end));
		} else {
			L_WARNING("Autocommit%s of %s falied after %s: %s", forced ? " (forced)" : "", repr(endpoints.to_string()), string::from_delta(start, end), error);
		}
	}
}
