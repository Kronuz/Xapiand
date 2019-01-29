/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#pragma once

#include "../src/threadpool.hh"

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>


static std::mutex mutex;


class TestTask {
	std::string name;
	double sleep;
	std::string& results;

public:
	TestTask(const std::string name_, double sleep_, std::string &results_)
		: name(name_),
		  sleep(sleep_),
		  results(results_) { }

	void run() {
		std::unique_lock<std::mutex> lk(mutex);
		results += "<" + name;
		lk.unlock();
		std::this_thread::sleep_for(std::chrono::duration<double>(sleep));
		lk.lock();
		results += name + ">";
	}
};


struct test_pool_class_t {
	inline int func(int i) noexcept {
		return i * i;
	}

	inline int func_shared(std::shared_ptr<int> i) noexcept {
		return *i * *i;
	}

	inline int func_unique(std::unique_ptr<int> i) noexcept {
		return *i * *i;
	}
};


inline int test_pool_func_func(int i) noexcept {
	return i * i;
}


inline int test_pool_func_func_shared(std::shared_ptr<int> i) noexcept {
	return *i * *i;
}


inline int test_pool_func_func_unique(std::unique_ptr<int> i) noexcept {
	return *i * *i;
}


int test_pool();
int test_pool_limit();
int test_pool_func();
int test_pool_func_shared();
int test_pool_func_unique();
