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

#pragma once

// #define MOODYCAMEL

#ifdef MOODYCAMEL
#include "moodycamel/blockingconcurrentqueue.h"
#else

#include <condition_variable>

#include "concurrent_queue.h"
#include "likely.h"

namespace moodycamel {

template <typename T>
class BlockingConcurrentQueue : public ConcurrentQueue<T> {
	std::condition_variable cond;

public:
	BlockingConcurrentQueue() :
		ConcurrentQueue<T>() {}

	BlockingConcurrentQueue(size_t) :
		ConcurrentQueue<T>() {}

	bool enqueue(const T& item) {
		if likely(ConcurrentQueue<T>::enqueue(item)) {
			cond.notify_one();
			return true;
		}
		return false;
	}

	bool enqueue(T&& item) {
		if likely(ConcurrentQueue<T>::enqueue(std::forward<T>(item))) {
			cond.notify_one();
			return true;
		}
		return false;
	}

	bool enqueue(const ProducerToken&, T&& item) {
		return enqueue(std::forward<T>(item));
	}

	template<typename It>
	bool enqueue_bulk(It itemFirst, size_t count) {
		if likely(ConcurrentQueue<T>::enqueue_bulk(itemFirst, count)) {
			cond.notify_all();
			return true;
		}
		return false;
	}

	template<typename U>
	void wait_dequeue(U& item) {
		std::unique_lock<std::mutex> lk(*ConcurrentQueue<T>::mtx);
		cond.wait(lk, [&]{
			return !ConcurrentQueue<T>::queue.empty();
		});
		item = std::move(ConcurrentQueue<T>::queue.front());
		ConcurrentQueue<T>::queue.pop_front();
	}
};

}

#endif

using namespace moodycamel;
