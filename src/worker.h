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


class Worker : public std::enable_shared_from_this<Worker> {
	using WorkerList = std::list<std::shared_ptr<Worker>>;

protected:
	ev::loop_ref *loop;

	ev::dynamic_loop _dynamic_loop;

	ev::async _async_break_loop;

	std::mutex _mtx;

	const std::shared_ptr<Worker> _parent;
	WorkerList _children;

	// _iterator should be const_iterator but in linux, std::list member functions
	// use a standard iterator and not const_iterator.
	WorkerList::iterator _iterator;

	template<typename T, typename L>
	Worker(T&& parent, L&& loop_)
		: loop(loop_ ? std::forward<L>(loop_) : &_dynamic_loop),
		  _async_break_loop(*loop),
		  _parent(std::forward<T>(parent))
	{
		if (_parent) {
			_iterator = _parent->_children.end();
		}
		_async_break_loop.set<Worker, &Worker::_async_break_loop_cb>(this);
		_async_break_loop.start();
		L_OBJ(this, "CREATED WORKER! [%llx]", this);
	}

	void _create() {
		if (_parent) {
			_iterator = _parent->_attach(shared_from_this());
		}
	}

	template<typename T>
	WorkerList::iterator _attach(T&& child) {
		L_OBJ(this, "Worker child [%llx] attached to [%llx]", child.get(), this);
		std::lock_guard<std::mutex> lk(_mtx);
		return _children.insert(_children.end(), std::forward<T>(child));
	}

	template<typename T>
	void _detach(T&& child) {
		std::lock_guard<std::mutex> lk(_mtx);
		if (child->_parent && child->_iterator != _children.end()) {
			_children.erase(child->_iterator);
			child->_iterator = _children.end();
		}
		L_OBJ(this, "Worker child [%llx] detached from [%llx]", child.get(), this);
	}

	void _async_break_loop_cb(ev::async &, int) {
		L_EV_BEGIN(this, "Worker::_async_break_loop_cb:BEGIN");
		loop->break_loop();
		L_EV_END(this, "Worker::_async_break_loop_cb:END");
	}

public:
	virtual ~Worker() {
		_async_break_loop.stop();

		L_OBJ(this, "DELETED WORKER! [%llx]", this);
	}

	virtual void shutdown() {
		L_OBJ(this , "SHUTDOWN WORKER! [%llx]", this);

		std::unique_lock<std::mutex> lk(_mtx);
		for (auto it = _children.begin(); it != _children.end();) {
			auto child = *it++;
			lk.unlock();
			child->shutdown();
			child->detach();
			lk.lock();
		}
	}

	void break_loop() {
		_async_break_loop.send();
	}

	template<typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	static inline decltype(auto) create(Args&&... args) {
		/*
		 * std::make_shared only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_shared : T {
			enable_make_shared(Args&&... args) : T(std::forward<Args>(args)...) { }
		};
		auto worker = std::make_shared<enable_make_shared>(std::forward<Args>(args)...);

		worker->_create();
		return worker;
	}

	void detach() {
		if (_parent) {
			_parent->_detach(shared_from_this());
		}
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	inline decltype(auto) share_this() noexcept {
		return std::static_pointer_cast<T>(shared_from_this());
	}
};
