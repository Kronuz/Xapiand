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


Log::Log(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup_) :
	wakeup(wakeup_),
	str_start(str),
	finished(false)
{}


std::string
Log::str_format(const char *file, int line, const char *suffix, const char *prefix, void *, const char *format, va_list argptr) {
	char buffer[1024 * 1024];
	snprintf(buffer, sizeof(buffer), "tid(%s): ../%s:%d: %s", get_thread_name().c_str(), file, line, prefix);
	size_t buffer_len = strlen(buffer);
	vsnprintf(&buffer[buffer_len], sizeof(buffer) - buffer_len, format, argptr);
	buffer_len = strlen(buffer);
	snprintf(&buffer[buffer_len], sizeof(buffer) - buffer_len, "%s", suffix);
	return buffer;

}


std::shared_ptr<Log>
Log::log(std::chrono::time_point<std::chrono::system_clock> wakeup, const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	std::string str(str_format(file, line, suffix, prefix, obj, format, argptr));
	va_end(argptr);

	return print(str, wakeup);
}

void
Log::clear()
{
	finished.store(true);
}


void
Log::unlog(const char *file, int line, const char *suffix, const char *prefix, void *obj, const char *format, ...)
{
	if (finished.exchange(true)) {
		va_list argptr;
		va_start(argptr, format);
		std::string str(str_format(file, line, suffix, prefix, obj, format, argptr));
		va_end(argptr);

		print(str);
	}
}


std::shared_ptr<Log>
Log::add(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup)
{
	static LogThread thread;

	// std::make_shared only can call a public constructor, for this reason
	// it is neccesary wrap the constructor in a struct.
	struct enable_make_shared : Log {
		enable_make_shared(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup) : Log(str, wakeup) {}
	};

	auto l_ptr = std::make_shared<enable_make_shared>(str, wakeup);
	thread.log_list.push_front(l_ptr);

	if (thread.wakeup.load() > l_ptr->wakeup) {
		thread.wakeup_signal.notify_one();
	}
	return l_ptr;
}


std::shared_ptr<Log>
Log::print(const std::string& str, std::chrono::time_point<std::chrono::system_clock> wakeup)
{
	if (wakeup > std::chrono::system_clock::now()) {
		return add(str, wakeup);
	} else {
		static std::mutex log_mutex;
		std::lock_guard<std::mutex> lk(log_mutex);
		std::cerr << str;
		return std::shared_ptr<Log>();
	}
}


LogThread::LogThread() :
	running(true),
	inner_thread(&LogThread::thread_function, this)
{}


LogThread::~LogThread()
{
	running.store(false);
	wakeup_signal.notify_one();
	try {
		inner_thread.join();
	} catch (std::system_error) {
	}
}


void
LogThread::thread_function()
{
	std::mutex mtx;
	std::unique_lock<std::mutex> lk(mtx);
	while (running.load()) {
		auto now = std::chrono::system_clock::now();
		auto next_wakeup = now + 3s;
		for (auto it = log_list.begin(); it != log_list.end();) {
			auto l_ptr = *it;
			if (l_ptr->finished) {
				it = log_list.erase(it);
			} else if (l_ptr->wakeup < now) {
				l_ptr->finished.store(true);
				Log::print(l_ptr->str_start);
				it = log_list.erase(it);
			} else {
				if (l_ptr->wakeup < next_wakeup) {
					next_wakeup = l_ptr->wakeup;
				}
				++it;
			}
		}
		if (next_wakeup < now + 100ms) {
			next_wakeup = now + 100ms;
		}
		wakeup.store(next_wakeup);
		wakeup_signal.wait_until(lk, next_wakeup);
		// Log::print("Wee!!\n");
	}
}
