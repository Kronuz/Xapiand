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

#include "database_updater.h"

#ifdef XAPIAND_CLUSTERING

#include <utility>                            // for std::move

#include "log.h"                              // for L_OBJ, L_CALL, L_DEBUG, L_WARNING
#include "manager.h"                          // for XapiandManager::manager
#include "server/discovery.h"                 // for Discovery::signal_db_update
#include "string.hh"                          // for string::from_delta
#include "time_point.hh"                      // for time_point_to_ullong


#define NORMALY_UPDATE_AFTER       5s
#define WHEN_BUSY_UPDATE_AFTER    15s
#define FORCE_UPDATE_AFTER        75s


std::mutex DatabaseUpdater::statuses_mtx;
std::unordered_map<DatabaseUpdate, DatabaseUpdater::Status> DatabaseUpdater::statuses;


DatabaseUpdater::DatabaseUpdater(bool forced_, DatabaseUpdate&& update_)
	: forced(forced_),
	  update(std::move(update_)) { }


void
DatabaseUpdater::send(DatabaseUpdate&& update)
{
	L_CALL("DatabaseUpdater::send(<update>)");

	std::shared_ptr<DatabaseUpdater> task;
	unsigned long long next_wakeup_time;

	{
		auto now = std::chrono::system_clock::now();

		std::lock_guard<std::mutex> statuses_lk(DatabaseUpdater::statuses_mtx);
		auto it = DatabaseUpdater::statuses.find(update);

		DatabaseUpdater::Status* status;
		if (it == DatabaseUpdater::statuses.end()) {
			auto& status_ref = DatabaseUpdater::statuses[update] = {
				nullptr,
				time_point_to_ullong(now + FORCE_UPDATE_AFTER)
			};
			status = &status_ref;
			next_wakeup_time = time_point_to_ullong(now + NORMALY_UPDATE_AFTER);
		} else {
			status = &(it->second);
			next_wakeup_time = time_point_to_ullong(now + WHEN_BUSY_UPDATE_AFTER);
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
		status->task = std::make_shared<DatabaseUpdater>(forced, std::move(update));
		task = status->task;
	}

	scheduler().add(task, next_wakeup_time);
}


void
DatabaseUpdater::run()
{
	L_CALL("DatabaseUpdater::run()");
	L_DEBUG_HOOK("DatabaseUpdater::run", "DatabaseUpdater::run()");

	{
		std::lock_guard<std::mutex> statuses_lk(DatabaseUpdater::statuses_mtx);
		DatabaseUpdater::statuses.erase(update);
	}

	auto start = std::chrono::system_clock::now();

	std::string error;
	try {

		if (auto discovery = XapiandManager::manager->weak_discovery.lock()) {
			discovery->signal_db_update(update);
			L_DEBUG("Replicators where informed about the database update: %s", repr(update.endpoint.to_string()));
		}

	} catch (const Exception& exc) {
		error = exc.get_message();
	}

	auto end = std::chrono::system_clock::now();

	if (error.empty()) {
		L_DEBUG("Updater%s succeeded after %s", forced ? " (forced)" : "", string::from_delta(start, end));
	} else {
		L_WARNING("Updater%s falied after %s: %s", forced ? " (forced)" : "", string::from_delta(start, end), error);
	}
}

#endif
