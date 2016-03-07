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

#include "log.h"

#include "ev/ev++.h"
#include "exception.h"

#include <list>
#include <mutex>
#include <memory>
#include <cassert>


class Worker : public std::enable_shared_from_this<Worker> {
	using WorkerShared = std::shared_ptr<Worker>;
	using WorkerList = std::list<WorkerShared>;

protected:
	ev::loop_ref *loop;

	template<typename T, typename L>
	Worker(T&& parent, L&& loop_)
		: loop(loop_ ? std::forward<L>(loop_) : &_dynamic_loop),
		  _async_break_loop(*loop),
		  _async_destroy(*loop),
		  _async_detach(*loop),
		  _parent(std::forward<T>(parent))
	{
		if (_parent) {
			_iterator = _parent->_children.end();
		}
		_async_break_loop.set<Worker, &Worker::_async_break_loop_cb>(this);
		_async_break_loop.start();

		_async_destroy.set<Worker, &Worker::_async_destroy_cb>(this);
		_async_destroy.start();

		_async_detach.set<Worker, &Worker::_async_detach_cb>(this);
		_async_detach.start();

		L_OBJ(this, "CREATED WORKER!");
	}

private:

	ev::dynamic_loop _dynamic_loop;

	ev::async _async_break_loop;
	ev::async _async_destroy;
	ev::async _async_detach;

	std::mutex _mtx;

	const WorkerShared _parent;
	WorkerList _children;

	// _iterator should be const_iterator but in linux, std::list member functions
	// use a standard iterator and not const_iterator.
	WorkerList::iterator _iterator;

	template<typename T>
	inline WorkerList::iterator _attach(T&& child) {
		assert(child);
		L_OBJ(this, "Worker child [%p] attached to [%p]", child.get(), this);
		return _children.insert(_children.end(), std::forward<T>(child));
	}

	template<typename T>
	decltype(auto) _detach(T&& child) {
		if (child->_parent && child->_iterator != _children.end()) {
			auto it = _children.erase(child->_iterator);
			child->_iterator = _children.end();
			L_OBJ(this, "Worker child [%p] detached from [%p]", child.get(), this);
			return it;
		}
		return _children.end();
	}

	inline void _async_break_loop_cb(ev::async&, int) {
		L_EV(this, "Worker::_async_break_loop_cb");

		L_EV_BEGIN(this, "Worker::_async_break_loop_cb:BEGIN");
		loop->break_loop();
		L_EV_END(this, "Worker::_async_break_loop_cb:END");
	}

	inline void _async_destroy_cb(ev::async&, int) {
		destroy_impl();
	}

	inline void _async_detach_cb(ev::async&, int) {
		L_EV(this, "Worker::_async_detach_cb");

		L_EV_BEGIN(this, "Worker::_async_detach_cb:BEGIN");
		if (_parent) {
			std::lock_guard<std::mutex> lk(_parent->_mtx);
			_parent->_detach(shared_from_this());
		}
		L_EV_END(this, "Worker::_async_detach_cb:END");
	}

	virtual void destroy_impl() = 0;

public:
	virtual ~Worker() {
		_async_break_loop.stop();
		_async_destroy.stop();
		_async_detach.stop();

		L_OBJ(this, "DELETED WORKER!");
	}

	virtual void shutdown(bool asap=true, bool now=true) {
		std::unique_lock<std::mutex> lk(_mtx);

		L_OBJ(this , "SHUTDOWN WORKER! (%d %d): %zu children", asap, now, _children.size());

		for (auto it = _children.begin(); it != _children.end();) {
			auto child = *it++;
			if (child) {
				lk.unlock();
				child->shutdown(asap, now);
				lk.lock();
			} else {
				it = _children.erase(it);
			}
		}
	}

	inline void break_loop() {
		_async_break_loop.send();
	}

	inline void destroy() {
		_async_destroy.send();
	}

	inline void detach() {
		_async_detach.send();
	}

	template<typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	static inline decltype(auto) make_shared(Args&&... args) {
		/*
		 * std::make_shared only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_shared : T {
			enable_make_shared(Args&&... args) : T(std::forward<Args>(args)...) { }
		};
		auto worker = std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
		if (worker->_parent) {
			std::lock_guard<std::mutex> lk(worker->_parent->_mtx);
			worker->_iterator = worker->_parent->_attach(worker);
		}

		return worker;
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	inline decltype(auto) share_parent() noexcept {
		return std::static_pointer_cast<T>(_parent);
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	inline decltype(auto) share_this() noexcept {
		return std::static_pointer_cast<T>(shared_from_this());
	}
};
