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

#include "async_fsync.h"

#include "io.h"         // for fsync, full_fsync
#include "log.h"        // for L_OBJ, L_CALL, L_DEBUG, L_WARNING
#include "string.hh"    // for string::from_delta


std::mutex AsyncFsync::statuses_mtx;
std::unordered_map<int, AsyncFsync::Status> AsyncFsync::statuses;


AsyncFsync::AsyncFsync(bool forced_, int fd_, int mode_)
	: forced(forced_),
	  fd(fd_),
	  mode(mode_) { }


void
AsyncFsync::async_fsync(int fd, bool full_fsync)
{
	L_CALL("AsyncFsync::async_fsync(%d, %s)", fd, full_fsync ? "true" : "false");

	std::shared_ptr<AsyncFsync> task;
	unsigned long long next_wakeup_time;

	{
		auto now = std::chrono::system_clock::now();

		std::lock_guard<std::mutex> statuses_lk(AsyncFsync::statuses_mtx);
		auto it = AsyncFsync::statuses.find(fd);

		AsyncFsync::Status* status;
		if (it == AsyncFsync::statuses.end()) {
			auto& status_ref = AsyncFsync::statuses[fd] = {
				nullptr,
				time_point_to_ullong(now + 3s)
			};
			status = &status_ref;
		} else {
			status = &(it->second);
		}

		bool forced;
		next_wakeup_time = time_point_to_ullong(now + 500ms);
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
		status->task = std::make_shared<AsyncFsync>(forced, fd, full_fsync ? 1 : 2);
		task = status->task;
	}

	scheduler().add(task, next_wakeup_time);
}


void
AsyncFsync::run()
{
	L_CALL("AsyncFsync::run()");
	L_DEBUG_HOOK("AsyncFsync::run", "AsyncFsync::run()");

	{
		std::lock_guard<std::mutex> statuses_lk(AsyncFsync::statuses_mtx);
		AsyncFsync::statuses.erase(fd);
	}

	auto start = std::chrono::system_clock::now();

	int err = -1;
	switch (mode) {
		case 1:
			err = io::unchecked_full_fsync(fd);
			break;
		case 2:
			err = io::unchecked_fsync(fd);
			break;
	}
	auto end = std::chrono::system_clock::now();

	if (err == -1) {
		if (errno == EBADF || errno == EINVAL) {
			L_DEBUG("Async %s%s falied after %s: %s (%d): %s", mode == 1 ? "Full Fsync" : "Fsync", forced ? " (forced)" : "", string::from_delta(start, end), io::strerrno(errno), errno, strerror(errno));
		} else {
			L_WARNING("Async %s%s falied after %s: %s (%d): %s", mode == 1 ? "Full Fsync" : "Fsync", forced ? " (forced)" : "", string::from_delta(start, end), io::strerrno(errno), errno, strerror(errno));
		}
	} else {
		L_DEBUG("Async %s%s succeeded after %s", mode == 1 ? "Full Fsync" : "Fsync", forced ? " (forced)" : "", string::from_delta(start, end));
	}
}
