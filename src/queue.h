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

#pragma once

#include <cerrno>
#include <list>
#include <deque>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <unordered_map>


namespace queue {

	template<typename T, typename Container = std::deque<T>>
	class Queue {
	protected:
		Container _items_queue;

		// A mutex object to control access to the underlying queue object
		mutable std::mutex _mutex;

		// A variable condition to make threads wait on specified condition values
		std::condition_variable _push_cond;
		std::condition_variable _pop_cond;

		std::atomic_bool _ending;
		std::atomic_bool _finished;
		size_t _limit;

		inline void _notify(bool one) noexcept {
			if (one) {
				// Notifiy waiting thread it can push/pop now
				_push_cond.notify_one();
				_pop_cond.notify_one();
			} else {
				// Signal the condition variable in case any threads are waiting
				_push_cond.notify_all();
				_pop_cond.notify_all();
			}
		}

		inline bool _push_wait(double timeout, std::unique_lock<std::mutex>& lk) {
			auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);

			auto check = [this]() {
				return _finished || _ending || _items_queue.size() < _limit;
			};

			if (timeout) {
				if (timeout > 0.0) {
					if (!_pop_cond.wait_until(lk, timeout_tp, check)) {
						return false;
					}
				} else {
					_pop_cond.wait(lk, check);
				}
			} else {
				if (!check()) {
					return false;
				}
			}

			if (_finished || _ending) {
				return true;
			}

			return true;
		}

		inline bool _pop_wait(double timeout, std::unique_lock<std::mutex>& lk) {
			auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);

			auto check = [this]() {
				// While the queue is empty, make the thread that runs this wait
				return _finished || _ending || !_items_queue.empty();
			};

			if (timeout) {
				if (timeout > 0.0) {
					if (!_push_cond.wait_until(lk, timeout_tp, check)) {
						return false;
					}
				} else {
					_push_cond.wait(lk, check);
				}
			} else {
				if (!check()) {
					return false;
				}
			}

			if (_finished || (_ending && _items_queue.empty())) {
				return false;
			}

			return true;
		}

		template<typename E>
		inline bool _push_front_impl(E&& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _push_wait(timeout, lk);
			if (ret) {
				// Insert the element in the queue
				_items_queue.push_front(std::forward<E>(element));
			}
			return ret;
		}

		template<typename E>
		inline bool _push_back_impl(E&& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _push_wait(timeout, lk);
			if (ret) {
				// Insert the element in the queue
				_items_queue.push_back(std::forward<E>(element));
			}
			return ret;
		}

		inline bool _pop_back_impl(T& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _pop_wait(timeout, lk);
			if (ret) {
				//when the condition variable is unlocked, popped the element
				element = std::move(_items_queue.back());

				//pop the element
				_items_queue.pop_back();
			}
			return ret;
		}

		inline bool _pop_front_impl(T& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _pop_wait(timeout, lk);
			if (ret) {
				//when the condition variable is unlocked, popped the element
				element = std::move(_items_queue.front());

				//pop the element
				_items_queue.pop_front();
			}
			return ret;
		}

		inline bool _clear_impl(std::unique_lock<std::mutex>&) noexcept {
			_items_queue.clear();

			if (_finished || _ending) {
				return false;
			}

			return true;
		}

	public:
		Queue(size_t limit=-1) :
		_ending(false),
		_finished(false),
		_limit(limit)
		{}

		// Move Constructor
		Queue(Queue&& q) {
			std::lock_guard<std::mutex> lk(q._mutex);
			_items_queue = std::move(q._items_queue);
			_limit = std::move(q._limit);
			_finished = false;
			_ending = false;
		}

		// Move assigment
		Queue& operator =(Queue&& q) {
			std::lock(_mutex, q._mutex);
			std::lock_guard<std::mutex> self_lock(_mutex, std::adopt_lock);
			std::lock_guard<std::mutex> other_lock(q._mutex, std::adopt_lock);
			_items_queue = std::move(q._items_queue);
			_limit = std::move(q._limit);
			_finished = false;
			_ending = false;
			return *this;
		}

		// Copy Constructor
		Queue(const Queue& q) = delete;
		Queue& operator =(const Queue& q) = delete;

		~Queue() {
			finish();
		}

		void end() {
			if (_ending) {
				return;
			}
			_ending = true;
			_notify(false);
		}

		void finish() {
			if (_finished) {
				return;
			}
			_finished = true;
			_notify(false);
		}

		template<typename E>
		bool push(E&& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(_mutex);
			bool pushed = _push_front_impl(std::forward<E>(element), timeout, lk);
			lk.unlock();
			_notify(pushed);
			return pushed;
		}

		bool pop(T& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(_mutex);
			bool popped = _pop_back_impl(element, timeout, lk);
			lk.unlock();
			_notify(popped);
			return popped;
		}

		void clear() {
			std::lock_guard<std::mutex> lk(_mutex);
			bool cleared = _clear_impl(lk);
			_notify(cleared);
		}

		bool empty() {
			std::lock_guard<std::mutex> lk(_mutex);
			return _items_queue.empty();
		}

		size_t size() {
			std::lock_guard<std::mutex> lk(_mutex);
			return _items_queue.size();
		}

		T& front() {
			std::lock_guard<std::mutex> lk(_mutex);
			return _items_queue.front();
		}
	};


	enum class DupAction {
		update,  // Updates the content of the item in the queue
		leave,   // Leaves the old item in the queue
		renew    // Renews the item in the queue (also moving it to front)
	};

	// A Queue with unique values
	template<typename T>
	class QueueSet : public Queue<T, std::list<T>> {
		typedef Queue<T, std::list<T>> Queue_t;
	protected:
		std::unordered_map<T, typename std::list<T>::iterator > _items_map;

		template<typename E>
		bool _push(E&& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool pushed = Queue_t::_push_front_impl(std::forward<E>(element), timeout, lk);

			if (pushed) {
				auto first = Queue_t::_items_queue.begin();
				_items_map[*first] = first;
			}

			lk.unlock();
			Queue_t::_notify(pushed);
			return pushed;
		}

	public:
		QueueSet(size_t limit=-1) : Queue<T, std::list<T>>(limit) {}

		template<typename E, typename OnDup>
		bool push(E&& element, double timeout, OnDup on_dup) {
			std::unique_lock<std::mutex> lk(Queue_t::_mutex);

			auto it = _items_map.find(element);
			if (it != _items_map.end()) {
				switch (on_dup(*it->second)) {
						// The item is already there...
					case DupAction::update:
						// Update the element object
						*it->second = element;
					case DupAction::leave:
						// Leave it alone
						return true;
					case DupAction::renew:
						// Move it to front
						Queue_t::_items_queue.erase(it->second);
						_items_map.erase(it);
						break;
				}
			}

			return _push(std::forward<E>(element), timeout, lk);
		}

		template<typename E, typename OnDup>
		bool push(E&& element, OnDup on_dup) {
			return push(std::forward<E>(element), -1.0, on_dup);
		}

		template<typename E>
		bool push(E&& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(Queue_t::_mutex);
			auto it = _items_map.find(element);
			if (it != _items_map.end()) {
				// The item is already there, move it to front
				Queue_t::_items_queue.erase(it->second);
				_items_map.erase(it);
			}
			return _push(std::forward<E>(element), timeout, lk);
		}

		bool pop(T& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(Queue_t::_mutex);
			bool popped = Queue_t::_pop_back_impl(element, timeout, lk);

			if (popped) {
				auto it = _items_map.find(element);
				if (it != _items_map.end()) {
					_items_map.erase(it);
				}
			}

			lk.unlock();
			Queue_t::_notify(popped);
			return popped;
		}

		void clear() {
			std::lock_guard<std::mutex> lk(Queue_t::_mutex);
			bool cleared = Queue_t::_clear_impl(lk);

			_items_map.clear();

			Queue_t::_notify(cleared);
		}

		size_t erase(const T& key) {
			std::lock_guard<std::mutex> lk(Queue_t::_mutex);
			size_t items = 0;
			auto it = _items_map.find(key);
			if (it != _items_map.end()) {
				Queue_t::_items_queue.erase(it->second);
				_items_map.erase(it);
				items++;
			}
			return items;
		}
	};

};


#ifdef TESTING_QUEUE
// Use test as: c++ -DTESTING_QUEUE -std=c++14 -g -x c++ queue.h -o test_queue && ./test_queue

#include <cassert>
#include <iostream>

#include <memory>
#include <string>
using namespace queue;

void test_unique()
{
	Queue<std::unique_ptr<std::string>> messages_queue;
	messages_queue.push(std::make_unique<std::string>("This is a unique data"));
	assert(messages_queue.size() == 1);

	std::unique_ptr<std::string> msg;
	assert(messages_queue.pop(msg));

	assert(messages_queue.size() == 0);
	assert(*msg == "This is a unique data");
}


void test_shared()
{
	Queue<std::shared_ptr<std::string>> messages_queue;
	messages_queue.push(std::make_shared<std::string>("This is a shared data"));
	assert(messages_queue.size() == 1);

	std::shared_ptr<std::string> shared = messages_queue.front();
	assert(messages_queue.size() == 1);

	assert(shared.use_count() == 2);

	std::shared_ptr<std::string> msg;
	assert(messages_queue.pop(msg));

	assert(messages_queue.size() == 0);

	assert(*msg == "This is a shared data");
}


void test_queue()
{
	Queue<int> q;
	int val = 1;

	q.push(val);
	q.push(2);
	q.push(3);
	q.push(4);

	int i1, i2, i3, i4;

	assert(q.pop(i1, 0));
	assert(q.pop(i2, 0));
	assert(q.pop(i3, 0));
	assert(q.pop(i4, 0));

	// std::cout << i1 << ' ' << i2 << ' ' << i3 << ' ' << i4 << std::endl;

	assert(i1 == 1);
	assert(i2 == 2);
	assert(i3 == 3);
	assert(i4 == 4);
}

void test_queue_set()
{
	QueueSet<int> q;
	int val = 1;

	q.push(val);
	q.push(2);
	q.push(3);
	q.push(4);
	q.push(1);  // renew by default, doesn't insert a new item

	int i1, i2, i3, i4, i5=789;

	assert(q.pop(i1, 0));
	assert(q.pop(i2, 0));
	assert(q.pop(i3, 0));
	assert(q.pop(i4, 0));
	assert(!q.pop(i5, 0));

	// std::cout << i1 << ' ' << i2 << ' ' << i3 << ' ' << i4 << ' ' << i5 << std::endl;

	assert(i1 == 2);
	assert(i2 == 3);
	assert(i3 == 4);
	assert(i4 == 1);
	assert(i5 == 789);
}

void test_queue_set_on_dup()
{
	QueueSet<int> q;
	int val = 1;

	q.push(val);
	q.push(2);
	q.push(3);
	q.push(4);
	q.push(1, [](int&){ return DupAction::leave; });  // doesn't touch the item
	q.push(2, [](int&){ return DupAction::update; });  // updates the item inplace
	q.push(3, [](int&){ return DupAction::renew; });  // renews the item

	int i1, i2, i3, i4, i5=789;

	assert(q.pop(i1, 0));
	assert(q.pop(i2, 0));
	assert(q.pop(i3, 0));
	assert(q.pop(i4, 0));
	assert(!q.pop(i5, 0));

	// std::cout << i1 << ' ' << i2 << ' ' << i3 << ' ' << i4 << ' ' << i5 << std::endl;

	assert(i1 == 1);
	assert(i2 == 2);
	assert(i3 == 4);
	assert(i4 == 3);
	assert(i5 == 789);
}


void test_queue_move_constructor()
{
	std::pair<int, Queue<int>> foo = std::make_pair(1, Queue<int>());
	foo.second.push(1);
	foo.second.push(2);
	foo.second.push(3);

	int i1, i2, i3;

	assert(foo.second.pop(i1, 0));
	assert(foo.second.pop(i2, 0));
	assert(foo.second.pop(i3, 0));
	assert(foo.second.size() == 0);

}


int main()
{

	test_queue();
	test_queue_set();
	test_queue_set_on_dup();
	test_unique();
	test_shared();
	test_queue_move_constructor();
	return 0;
}

#endif
