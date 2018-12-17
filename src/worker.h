/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <atomic>	    // for std::atomic_bool
#include <list>         // for list
#include <memory>       // for shared_ptr, enable_shared_from_this
#include <mutex>        // for mutex
#include <string>	    // for string
#include <vector>       // for vector

#include "cassert.h"    // for ASSERT
#include "ev/ev++.h"


class Worker : public std::enable_shared_from_this<Worker> {
	ev::dynamic_loop _dynamic_ev_loop;

protected:
	unsigned int ev_flags;
	ev::loop_ref *ev_loop;

private:
	std::atomic_llong _asap;
	std::atomic_llong _now;

	ev::async _shutdown_async;
	ev::async _break_loop_async;
	ev::async _start_async;
	ev::async _stop_async;
	ev::async _destroy_async;
	ev::async _detach_children_async;

	mutable std::recursive_mutex _mtx;
	std::atomic_bool _runner;
	std::atomic_bool _detaching;

	bool _started;
	bool _destroyed;
	bool _deinited;

	std::weak_ptr<Worker> _parent;
	std::list<std::shared_ptr<Worker>> _children;
	std::list<std::shared_ptr<Worker>>::iterator _iterator;

protected:
	template<typename T, typename L>
	Worker(T&& parent, L&& ev_loop_, unsigned int ev_flags_)
		: _dynamic_ev_loop(ev_flags_),
		  ev_flags(ev_flags_),
		  ev_loop(ev_loop_ ? std::forward<L>(ev_loop_) : &_dynamic_ev_loop),
		  _asap(0),
		  _now(0),
		  _shutdown_async(*ev_loop),
		  _break_loop_async(*ev_loop),
		  _start_async(*ev_loop),
		  _stop_async(*ev_loop),
		  _destroy_async(*ev_loop),
		  _detach_children_async(*ev_loop),
		  _runner(false),
		  _detaching(false),
		  _started(false),
		  _destroyed(false),
		  _deinited(false),
		  _parent(std::forward<T>(parent))
	{
		_init();
	}

	void deinit();

private:
	void _init();
	void _deinit();

	std::list<std::shared_ptr<Worker>>::iterator __attach(const std::shared_ptr<Worker>& child);
	std::list<std::shared_ptr<Worker>>::iterator __detach(const std::shared_ptr<Worker>& child);

	void _shutdown_async_cb();
	void _break_loop_async_cb(ev::async&, int revents);
	void _start_async_cb(ev::async&, int revents);
	void _stop_async_cb(ev::async&, int revents);
	void _destroy_async_cb(ev::async&, int revents);
	void _detach_children_async_cb(ev::async&, int revents);

	void _break_loop_impl();
	void _start_impl();
	void _stop_impl();
	void _destroy_impl();
	void _detach_impl(const std::weak_ptr<Worker>& weak_child);
	void _detach_children_impl();

	void _detach_children(bool async);

public:
	std::string dump_tree(int level=1);

	virtual std::string __repr__() const;

	virtual ~Worker() noexcept;

	virtual void shutdown_impl(long long asap, long long now);
	virtual void start_impl() {}
	virtual void stop_impl() {}
	virtual void destroy_impl() {}

	void shutdown(bool async = true);
	void shutdown(long long asap, long long now, bool async = true);

	void break_loop(bool async = true);
	void start(bool async = true);
	void stop(bool async = true);
	void destroy(bool async = true);
	void detach(bool async = true);
	void redetach(bool async = true);

	auto is_deinited() const {
		return _deinited;
	}

	auto is_runner() const {
		return _runner.load(std::memory_order_relaxed);
	}

	auto is_detaching() const {
		return _detaching.load(std::memory_order_relaxed);
	}

	auto is_running_loop() const {
		return ev_loop && ev_loop->raw_loop && ev_loop->depth() != 0u;
	}

	auto use_count() const {
		return shared_from_this().use_count() - 1;
	}

	void run_loop();

	void finish();

	std::vector<std::weak_ptr<Worker>> gather_children() const;
	std::shared_ptr<Worker> parent() const;

	template<typename T, typename... Args, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	static auto make_shared(Args&&... args) {
		/*
		 * std::make_shared only can call a public constructor, for this reason
		 * it is neccesary wrap the constructor in a struct.
		 */
		struct enable_make_shared : T {
			enable_make_shared(Args&&... _args) : T(std::forward<Args>(_args)...) { }
		};
		auto child = std::make_shared<enable_make_shared>(std::forward<Args>(args)...);
		std::lock_guard<std::recursive_mutex> child_lk(child->_mtx);
		if (auto parent = child->_parent.lock()) {
			std::lock_guard<std::recursive_mutex> parent_lk(parent->_mtx);
			parent->__attach(child);
		}
		return child;
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	auto share_parent() noexcept {
		return std::static_pointer_cast<T>(parent());
	}

	template<typename T, typename = std::enable_if_t<std::is_base_of<Worker, std::decay_t<T>>::value>>
	auto share_this() noexcept {
		return std::static_pointer_cast<T>(shared_from_this());
	}
};
