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

#define MOODYCAMEL

#ifdef MOODYCAMEL
#include "moodycamel/concurrentqueue.h"
#else

#include <deque>
#include <memory>
#include <mutex>

namespace moodycamel {

struct ConcurrentQueueDefaultTraits {
	static const size_t BLOCK_SIZE = 32;
};


struct ProducerToken {
public:
	template <typename Q>
	ProducerToken(Q&) {}
};


template <typename T>
class ConcurrentQueue {
protected:
	std::unique_ptr<std::mutex> mtx;
	std::deque<T> queue;

public:
	ConcurrentQueue() :
		mtx(std::make_unique<std::mutex>()) {}

	ConcurrentQueue(size_t) :
		mtx(std::make_unique<std::mutex>()) {}

	bool enqueue(const T& item) {
		std::lock_guard<std::mutex> lk(*mtx);
		queue.push_back(item);
		return true;
	}

	bool enqueue(T&& item) {
		std::lock_guard<std::mutex> lk(*mtx);
		queue.push_back(std::forward<T>(item));
		return true;
	}

	bool enqueue(const ProducerToken&, const T& item) {
		return enqueue(item);
	}

	bool enqueue(const ProducerToken&, T&& item) {
		return enqueue(std::forward<T>(item));
	}

	template<typename It>
	bool enqueue_bulk(It itemFirst, size_t count) {
		std::lock_guard<std::mutex> lk(*mtx);
		while (count--) {
			queue.push_back(std::forward<T>(*itemFirst++));
		}
		return true;
	}

	template<typename U>
	bool try_dequeue(U& item) {
		std::lock_guard<std::mutex> lk(*mtx);
		if (!queue.empty()) {
			item = std::move(queue.front());
			queue.pop_front();
			return true;
		}
		return false;
	}

	template<typename It>
	size_t try_dequeue_bulk(It itemFirst, size_t count) {
		size_t dequeued = 0;
		while (!queue.empty() && count--) {
			*itemFirst = std::move(queue.front());
			queue.pop_front();
			++dequeued;
		}
		return dequeued;
	}
};

}

#endif

using namespace moodycamel;
