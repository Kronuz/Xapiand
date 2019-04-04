/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <list>
#include <mutex>
#include <unordered_map>

#include "cassert.h"    // for ASSERT


namespace queue {
	using namespace std::chrono_literals;

	struct QueueState {
		size_t _hard_limit;
		size_t _soft_limit;
		size_t _threshold;

		// A mutex object to control access to the underlying queue object
		mutable std::mutex _mutex;

		// A variable condition to make threads wait on specified condition values
		std::condition_variable _pop_cond;
		std::condition_variable _push_cond;
		std::atomic_size_t _cnt;

		QueueState(size_t hard_limit=-1, size_t soft_limit=-1, size_t threshold=-1)
			: _hard_limit(hard_limit),
			  _soft_limit(soft_limit),
			  _threshold(threshold),
			  _cnt(0) { }
	};

	template<typename T, typename Container = std::deque<T>>
	class Queue {
	protected:
		Container _items_queue;

		std::atomic_bool _ending;
		std::atomic_bool _finished;

		std::shared_ptr<QueueState> _state;

		bool _push_wait(double timeout, std::unique_lock<std::mutex>& lk) {
			auto push_wait_pred = [this]() {
				return _finished || _ending || _state->_cnt < _state->_hard_limit;
			};
			if (timeout) {
				if (timeout > 0.0) {
					auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
					if (!_state->_push_cond.wait_until(lk, timeout_tp, push_wait_pred)) {
						return false;
					}
				} else {
					while (!_state->_push_cond.wait_for(lk, 1s, push_wait_pred)) {}
				}
			} else {
				if (!push_wait_pred()) {
					return false;
				}
			}

			if (_finished || _ending) {
				return false;
			}

			return _state->_cnt < _state->_soft_limit;
		}

		bool _pop_wait(double timeout, std::unique_lock<std::mutex>& lk) {
			auto pop_wait_pred = [this]() {
				// While the queue is empty, make the thread that runs this wait
				return _finished || _ending || !_items_queue.empty();
			};
			if (timeout) {
				if (timeout > 0.0) {
					auto timeout_tp = std::chrono::system_clock::now() + std::chrono::duration<double>(timeout);
					if (!_state->_pop_cond.wait_until(lk, timeout_tp, pop_wait_pred)) {
						return false;
					}
				} else {
					while (!_state->_pop_cond.wait_for(lk, 1s, pop_wait_pred)) {}
				}
			} else {
				if (!pop_wait_pred()) {
					return false;
				}
			}

			if (_finished || (_ending && _items_queue.empty())) {
				return false;
			}

			return true;
		}

		template<typename E>
		bool _push_front_impl(E&& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _push_wait(timeout, lk);
			if (ret) {
				// Insert the element in the queue
				_items_queue.push_front(std::forward<E>(element));
				++_state->_cnt;
			}
			return ret;
		}

		template<typename E>
		bool _push_back_impl(E&& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _push_wait(timeout, lk);
			if (ret) {
				// Insert the element in the queue
				_items_queue.push_back(std::forward<E>(element));
				++_state->_cnt;
			}
			return ret;
		}

		bool _pop_back_impl(T& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _pop_wait(timeout, lk);
			if (ret) {
				//when the condition variable is unlocked, popped the element
				element = std::move(_items_queue.back());

				//pop the element
				_items_queue.pop_back();
				ASSERT(_state->_cnt > 0);
				--_state->_cnt;
			}
			return ret;
		}

		bool _pop_front_impl(T& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool ret = _pop_wait(timeout, lk);
			if (ret) {
				//when the condition variable is unlocked, popped the element
				element = std::move(_items_queue.front());

				//pop the element
				_items_queue.pop_front();
				ASSERT(_state->_cnt > 0);
				--_state->_cnt;
			}
			return ret;
		}

		bool _clear_impl(std::unique_lock<std::mutex>&) noexcept {
			auto size = _items_queue.size();
			_items_queue.clear();
			ASSERT(_state->_cnt >= size);
			_state->_cnt -= size;

			if (_finished || _ending) {
				return false;
			}

			return true;
		}

	public:
		Queue(size_t hard_limit=-1, size_t soft_limit=-1, size_t threshold=-1)
			: _ending(false),
			  _finished(false),
			  _state(std::make_shared<QueueState>(hard_limit, soft_limit, threshold)) { }

		explicit Queue(const std::shared_ptr<QueueState>& state)
			: _ending(false),
			  _finished(false),
			  _state(state) { }

		// Move Constructor
		Queue(Queue&& q) {
			std::lock_guard<std::mutex> lk(q._state->_mutex);
			_items_queue = std::move(q._items_queue);
			_ending = false;
			_finished = false;
			_state = std::move(q._state);
		}

		// Move assigment
		Queue& operator=(Queue&& q) {
			std::lock(_state->_mutex, q._state->_mutex);
			std::lock_guard<std::mutex> self_lock(_state->_mutex, std::adopt_lock);
			std::lock_guard<std::mutex> other_lock(q._state->_mutex, std::adopt_lock);
			_items_queue = std::move(q._items_queue);
			_ending = false;
			_finished = false;
			_state = std::move(q._state);
			return *this;
		}

		// Copy Constructor
		Queue(const Queue& q) = delete;

		// Copy assigment
		Queue& operator=(const Queue& q) = delete;

		~Queue() {
			if (_state) {
				std::lock_guard<std::mutex> lk(_state->_mutex);
				auto size = _items_queue.size();
				ASSERT(_state->_cnt >= size);
				_state->_cnt -= size;
				finish();
			}
		}

		void end() noexcept {
			if (_ending) {
				return;
			}
			_ending = true;

			// Signal the condition variable in case any threads are waiting
			_state->_pop_cond.notify_all();
			_state->_push_cond.notify_all();

		}

		void finish() noexcept {
			if (_finished) {
				return;
			}
			_finished = true;

			// Signal the condition variable in case any threads are waiting
			_state->_pop_cond.notify_all();
			_state->_push_cond.notify_all();
		}

		template<typename E>
		bool push_back(E&& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(_state->_mutex);
			bool pushed = _push_back_impl(std::forward<E>(element), timeout, lk);
			lk.unlock();

			if (pushed) {
				// Notifiy waiting thread it can push/pop now
				_state->_pop_cond.notify_one();
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				_state->_pop_cond.notify_all();
				_state->_push_cond.notify_all();
			}

			return pushed;
		}

		template<typename E>
		bool push_front(E&& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(_state->_mutex);
			bool pushed = _push_front_impl(std::forward<E>(element), timeout, lk);
			lk.unlock();

			if (pushed) {
				// Notifiy waiting thread it can push/pop now
				_state->_pop_cond.notify_one();
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				_state->_pop_cond.notify_all();
				_state->_push_cond.notify_all();
			}

			return pushed;
		}

		template<typename E>
		bool push(E&& element, double timeout=-1.0) {
			return push_back(std::forward<E>(element), timeout);
		}

		bool pop_back(T& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(_state->_mutex);
			bool popped = _pop_back_impl(element, timeout, lk);
			auto size = _items_queue.size();
			lk.unlock();

			if (popped) {
				if (size < _state->_threshold) {
					// Notifiy waiting thread it can push/pop now
					_state->_push_cond.notify_one();
				}
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				_state->_pop_cond.notify_all();
				_state->_push_cond.notify_all();
			}

			return popped;
		}

		bool pop_front(T& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(_state->_mutex);
			bool popped = _pop_front_impl(element, timeout, lk);
			auto size = _items_queue.size();
			lk.unlock();

			if (popped) {
				if (size < _state->_threshold) {
					// Notifiy waiting thread it can push/pop now
					_state->_push_cond.notify_one();
				}
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				_state->_pop_cond.notify_all();
				_state->_push_cond.notify_all();
			}

			return popped;
		}

		bool pop(T& element, double timeout=-1.0) {
			return pop_front(element, timeout);
		}

		void clear() {
			std::unique_lock<std::mutex> lk(_state->_mutex);
			bool cleared = _clear_impl(lk);
			lk.unlock();

			if (cleared) {
				// Notifiy waiting thread it can push/pop now
				_state->_push_cond.notify_one();
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				_state->_pop_cond.notify_all();
				_state->_push_cond.notify_all();
			}
		}

		template <typename F>
		bool empty(F f) {
			std::lock_guard<std::mutex> lk(_state->_mutex);
			auto empty = _items_queue.empty();
			f(empty);
			return empty;
		}

		bool empty() const {
			std::lock_guard<std::mutex> lk(_state->_mutex);
			return _items_queue.empty();
		}

		size_t size() const {
			std::lock_guard<std::mutex> lk(_state->_mutex);
			return _items_queue.size();
		}

		size_t count() const {
			return _state->_cnt;
		}

		T& front() {
			std::lock_guard<std::mutex> lk(_state->_mutex);
			return _items_queue.front();
		}

		bool front(T& element) {
			std::lock_guard<std::mutex> lk(_state->_mutex);
			if (_items_queue.empty()) {
				return false;
			}
			element = _items_queue.front();
			return true;
		}
	};


	enum class DupAction {
		update,  // Updates the content of the item in the queue
		leave,   // Leaves the old item in the queue
		renew    // Renews the item in the queue (also moving it to front)
	};

	// A Queue with unique values
	template<typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
	class QueueSet : public Queue<Key, std::list<Key>> {
		using Queue_t = Queue<Key, std::list<Key>>;

	protected:
		std::unordered_map<Key, typename std::list<Key>::iterator, Hash, KeyEqual> _items_map;

		template<typename E>
		bool _push(E&& element, double timeout, std::unique_lock<std::mutex>& lk) {
			bool pushed = Queue_t::_push_front_impl(std::forward<E>(element), timeout, lk);

			if (pushed) {
				auto first = Queue_t::_items_queue.begin();
				_items_map[*first] = first;
			}

			lk.unlock();

			if (pushed) {
				// Notifiy waiting thread it can push/pop now
				Queue_t::_state->_pop_cond.notify_one();
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				Queue_t::_state->_pop_cond.notify_all();
				Queue_t::_state->_push_cond.notify_all();
			}

			return pushed;
		}

	public:
		QueueSet(size_t hard_limit=-1, size_t soft_limit=-1, size_t threshold=-1)
			: Queue<Key, std::list<Key>>(hard_limit, soft_limit, threshold) { }

		template<typename E, typename OnDup>
		bool push(E&& element, double timeout, OnDup on_dup) {
			std::unique_lock<std::mutex> lk(Queue_t::_state->_mutex);

			auto it = _items_map.find(element);
			if (it != _items_map.end()) {
				switch (on_dup(*it->second)) {
						// The item is already there...
					case DupAction::update:
						// Update the element object
						*it->second = std::forward<E>(element);
					case DupAction::leave:
						// Leave it alone
						return true;
					case DupAction::renew:
						// Move it to front
						Queue_t::_items_queue.erase(it->second);
						_items_map.erase(it);
						ASSERT(Queue_t::_state->_cnt > 0);
						--Queue_t::_state->_cnt;
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
			std::unique_lock<std::mutex> lk(Queue_t::_state->_mutex);
			auto it = _items_map.find(element);
			if (it != _items_map.end()) {
				// The item is already there, move it to front
				Queue_t::_items_queue.erase(it->second);
				_items_map.erase(it);
				ASSERT(Queue_t::_state->_cnt > 0);
				--Queue_t::_state->_cnt;
			}
			return _push(std::forward<E>(element), timeout, lk);
		}

		bool pop(Key& element, double timeout=-1.0) {
			std::unique_lock<std::mutex> lk(Queue_t::_state->_mutex);
			bool popped = Queue_t::_pop_back_impl(element, timeout, lk);

			if (popped) {
				auto it = _items_map.find(element);
				if (it != _items_map.end()) {
					_items_map.erase(it);
				}
			}

			auto size = Queue_t::_items_queue.size();

			lk.unlock();

			if (popped) {
				if (size < Queue_t::_state->_threshold) {
					// Notifiy waiting thread it can push/pop now
					Queue_t::_state->_push_cond.notify_one();
				}
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				Queue_t::_state->_pop_cond.notify_all();
				Queue_t::_state->_push_cond.notify_all();
			}

			return popped;
		}

		void clear() {
			std::lock_guard<std::mutex> lk(Queue_t::_state->_mutex);
			bool cleared = Queue_t::_clear_impl(lk);

			_items_map.clear();

			if (cleared) {
				// Notifiy waiting thread it can push/pop now
				Queue_t::_state->_push_cond.notify_one();
			} else {
				// FIXME: This block shouldn't be needed!
				// Signal the condition variable in case any threads are waiting
				Queue_t::_state->_pop_cond.notify_all();
				Queue_t::_state->_push_cond.notify_all();
			}
		}

		size_t erase(const Key& key) {
			std::lock_guard<std::mutex> lk(Queue_t::_state->_mutex);
			auto it = _items_map.find(key);
			if (it == _items_map.end()) {
				return 0;
			}
			Queue_t::_items_queue.erase(it->second);
			_items_map.erase(it);
			ASSERT(Queue_t::_state->_cnt > 0);
			--Queue_t::_state->_cnt;
			return 1;
		}
	};
}
