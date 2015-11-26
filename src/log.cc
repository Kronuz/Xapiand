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

#include "log.h"
#include "utils.h"

std::mutex log_mutex;
std::atomic_bool log_runner(true);
slist<std::shared_ptr<Log>> log_list;

std::unique_ptr<ThreadLog> log_thread = std::make_unique<ThreadLog>([]() {
	while (log_runner.load()) {
		auto now = epoch::now<std::chrono::milliseconds>();
		for (auto it = log_list.begin(); it != log_list.end(); ++it) {
			if ((*it)->finished) {
				log_list.erase(it);
			} else if (now > (*it)->epoch_end) {
				std::unique_lock<std::mutex> lk(log_mutex);
				std::cerr << (*it)->str_start;
				lk.unlock();
				(*it)->finished.store(true);
				log_list.erase(it);
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
});


Log::Log(const char *file, int line, int timeout, void *, const char *format, va_list argptr)
	: epoch_end(epoch::now<std::chrono::milliseconds>() + timeout),
	  finished(false)
{
	char buffer[1024 * 1024];
	snprintf(buffer, sizeof(buffer), "tid(%s): ../%s:%d: ", get_thread_name().c_str(), file, line);
	size_t buffer_len = strlen(buffer);
	vsnprintf(&buffer[buffer_len], sizeof(buffer) - buffer_len, format, argptr);
	str_start = buffer;
}


std::shared_ptr<Log>
Log::timed(const char *file, int line, int timeout, void *obj, const char *format, ...)
{
	// std::make_shared only can call a public constructor, for this reason
	// it is neccesary wrap the constructor in a struct.
	struct enable_make_shared : Log {
		enable_make_shared(const char *file, int line, int timeout, void *obj, const char *format, va_list argptr)
			: Log(file, line, timeout, obj, format, argptr) { }
	};

	va_list argptr;
	va_start(argptr, format);

	if (timeout) {
		std::shared_ptr<Log> l_ptr = std::make_shared<enable_make_shared>(file, line, timeout, obj, format, argptr);
		log_list.push_front(l_ptr->shared_from_this());
		va_end(argptr);
		return l_ptr;
	} else {
		std::lock_guard<std::mutex> lk(log_mutex);
		fprintf(stderr, "tid(%s): ../%s:%d: ", get_thread_name().c_str(), file, line);
		vfprintf(stderr, format, argptr);
		va_end(argptr);
		return std::shared_ptr<Log>();
	}
}


void
Log::end(std::shared_ptr<Log>&& l, const char *file, int line, void *, const char *format, ...)
{
	if (l) {
		if (l->finished) {
			va_list argptr;
			va_start(argptr, format);
			std::lock_guard<std::mutex> lk(log_mutex);
			fprintf(stderr, "tid(%s): ../%s:%d: ", get_thread_name().c_str(), file, line);
			vfprintf(stderr, format, argptr);
			va_end(argptr);
		} else {
			l->finished = true;
		}
	} else {
		va_list argptr;
		va_start(argptr, format);
		std::lock_guard<std::mutex> lk(log_mutex);
		fprintf(stderr, "tid(%s): ../%s:%d: ", get_thread_name().c_str(), file, line);
		vfprintf(stderr, format, argptr);
		va_end(argptr);
	}
}


void
Log::log(const char *file, int line, void *, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	std::lock_guard<std::mutex> lk(log_mutex);
	fprintf(stderr, "tid(%s): ../%s:%d: ", get_thread_name().c_str(), file, line);
	vfprintf(stderr, format, argptr);
	va_end(argptr);
}
