/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "async_fsync.h"

#include "io_utils.h"   // for fsync, full_fsync
#include "log.h"        // for Log, L_OBJ, L_CALL, L_DEBUG, L_WARNING
#include "utils.h"      // for delta_string


std::mutex AsyncFsync::statuses_mtx;
std::unordered_map<int, std::shared_ptr<AsyncFsync::Status>> AsyncFsync::statuses;


AsyncFsync::AsyncFsync(bool forced_, int fd_, int mode_)
	: forced(forced_),
	  fd(fd_),
	  mode(mode_) { }


void
AsyncFsync::async_fsync(int fd, bool full_fsync)
{
	L_CALL(nullptr, "AsyncFsync::async_fsync(%d, %s)", fd, full_fsync ? "true" : "false");

	std::shared_ptr<AsyncFsync> task;
	std::chrono::time_point<std::chrono::system_clock> next_wakeup_time;

	{
		auto now = std::chrono::system_clock::now();

		std::lock_guard<std::mutex> statuses_lk(AsyncFsync::statuses_mtx);
		auto& status = AsyncFsync::statuses[fd];

		if (!status) {
			status = std::make_shared<AsyncFsync::Status>();
			status->max_wakeup_time = now + 3s;
			status->wakeup_time = now;
		}

		bool forced;
		next_wakeup_time = now + 500ms;
		if (next_wakeup_time > status->max_wakeup_time) {
			next_wakeup_time = status->max_wakeup_time;
			forced = true;
		} else {
			forced = false;
		}
		if (next_wakeup_time == status->wakeup_time) {
			return;
		}

		if (status->task) {
			status->task->clear();
		}
		status->wakeup_time = next_wakeup_time;
		status->task = std::make_shared<AsyncFsync>(forced, fd, full_fsync ? 1 : 2);
		task = status->task;
	}

	scheduler().add(task, next_wakeup_time);
}


void
AsyncFsync::run()
{
	L_CALL(this, "AsyncFsync::run()");
	L_INFO_HOOK_LOG("AsyncFsync::run", this, "AsyncFsync::run()");

	{
		std::lock_guard<std::mutex> statuses_lk(AsyncFsync::statuses_mtx);
		AsyncFsync::statuses.erase(fd);
	}

	bool successful = false;
	auto start = std::chrono::system_clock::now();
	switch (mode) {
		case 1:
			successful = (io::full_fsync(fd) == 0);
			break;
		case 2:
			successful = (io::fsync(fd) == 0);
			break;
	}
	auto end = std::chrono::system_clock::now();

	if (successful) {
		L_DEBUG(this, "Async %s: %d%s (took %s)", mode == 1 ? "Full Fsync" : "Fsync", fd, forced ? " (forced)" : "", delta_string(start, end).c_str());
	} else {
		L_WARNING(this, "Async %s falied: %d%s (took %s)", mode == 1 ? "Full Fsync" : "Fsync", fd, forced ? " (forced)" : "", delta_string(start, end).c_str());
	}
}
