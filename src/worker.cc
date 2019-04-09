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

#include "worker.h"

#include <thread>

#include "cassert.h"            // for ASSERT
#include "epoch.hh"             // for epoch::now
#include "log.h"                // for L_CALL
#include "readable_revents.hh"  // for readable_revents


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #define L_WORKER L_SALMON
// #undef L_EV
// #define L_EV L_MEDIUM_PURPLE
// #undef L_EV_BEGIN
// #define L_EV_BEGIN L_DELAYED_200
// #undef L_EV_END
// #define L_EV_END L_DELAYED_N_UNLOG


Worker::~Worker() noexcept
{
	try {
		// Make sure to call Worker::deinit() as the last line in the
		// destructor of any subclasses implementing either one of:
		// shutdown_impl(), destroy_impl(), start_impl() or stop_impl().
		// Otherwise the assert bellow will fire!
		ASSERT(_deinited);  // Beware of the note above

		deinit();
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


void
Worker::deinit()
{
	L_CALL("Worker::deinit()");

	_stop_impl();
	_destroy_impl();
	_deinit();
}


void
Worker::_init()
{
	L_CALL("Worker::_init()");

	if (auto parent = _parent.lock()) {
		_iterator = parent->_children.end();
	}

	_shutdown_async.set<Worker, &Worker::_shutdown_async_cb>(this);
	_shutdown_async.start();
	L_EV("Start {} async shutdown event", __repr__());

	_break_loop_async.set<Worker, &Worker::_break_loop_async_cb>(this);
	_break_loop_async.start();
	L_EV("Start {} async break_loop event", __repr__());

	_destroy_async.set<Worker, &Worker::_destroy_async_cb>(this);
	_destroy_async.start();
	L_EV("Start {} async destroy event", __repr__());

	_start_async.set<Worker, &Worker::_start_async_cb>(this);
	_start_async.start();
	L_EV("Start {} async start event", __repr__());

	_stop_async.set<Worker, &Worker::_stop_async_cb>(this);
	_stop_async.start();
	L_EV("Start {} async stop event", __repr__());

	_detach_children_async.set<Worker, &Worker::_detach_children_async_cb>(this);
	_detach_children_async.start();
	L_EV("Start {} async detach children event", __repr__());
}


void
Worker::_deinit()
{
	L_CALL("Worker::_deinit()");

	if (!_deinited) {
		_detach_children_async.stop();
		L_EV("Stop {} async detach children event", __repr__());

		_stop_async.stop();
		L_EV("Stop {} async stop event", __repr__());

		_start_async.stop();
		L_EV("Stop {} async start event", __repr__());

		_destroy_async.stop();
		L_EV("Stop {} async destroy event", __repr__());

		_break_loop_async.stop();
		L_EV("Stop {} async break_loop event", __repr__());

		_shutdown_async.stop();
		L_EV("Stop {} async shutdown event", __repr__());

		_deinited = true;
	}
}


std::list<std::shared_ptr<Worker>>::iterator
Worker::__detach(const std::shared_ptr<Worker>& child)
{
	L_CALL("Worker::__detach(<child>)", __repr__());

	ASSERT(child);
	std::lock_guard<std::recursive_mutex> lk(child->_mtx);
	if (child->_iterator != _children.end()) {
		child->_parent.reset();
		auto it = _children.erase(child->_iterator);
		child->_iterator = _children.end();
		return it;
	}
	return _children.end();
}


std::list<std::shared_ptr<Worker>>::iterator
Worker::__attach(const std::shared_ptr<Worker>& child)
{
	L_CALL("Worker::__attach(<child>)", __repr__());

	ASSERT(child);
	std::lock_guard<std::recursive_mutex> lk(child->_mtx);
	if (child->_iterator == _children.end()) {
		ASSERT(std::find(_children.begin(), _children.end(), child) == _children.end());
		child->_parent = shared_from_this();
		auto it = _children.insert(_children.begin(), child);
		child->_iterator = it;
		return it;
	}
	return _children.end();
}


void
Worker::_shutdown_async_cb()
{
	L_CALL("Worker::_shutdown_async_cb() {}", __repr__());

	L_EV_BEGIN("Worker::_shutdown_async_cb:BEGIN");
	L_EV_END("Worker::_shutdown_async_cb:END");

	shutdown_impl(_asap, _now);
}


void
Worker::_break_loop_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("Worker::_break_loop_async_cb(<watcher>, {:#x} ({})) {}", revents, readable_revents(revents), __repr__());

	L_EV_BEGIN("Worker::_break_loop_async_cb:BEGIN");
	L_EV_END("Worker::_break_loop_async_cb:END");

	_break_loop_impl();
}


void
Worker::_start_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("Worker::_start_async_cb(<watcher>, {:#x} ({})) {}", revents, readable_revents(revents), __repr__());

	_start_impl();
}


void
Worker::_stop_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("Worker::_stop_async_cb(<watcher>, {:#x} ({})) {}", revents, readable_revents(revents), __repr__());

	_stop_impl();
}


void
Worker::_destroy_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("Worker::_destroy_async_cb(<watcher>, {:#x} ({})) {}", revents, readable_revents(revents), __repr__());

	_destroy_impl();
}


void
Worker::_detach_children_async_cb(ev::async& /*unused*/, [[maybe_unused]] int revents)
{
	L_CALL("Worker::_detach_children_async_cb(<watcher>, {:#x} ({})) {}", revents, readable_revents(revents), __repr__());

	L_EV_BEGIN("Worker::_detach_children_async_cb:BEGIN");
	L_EV_END("Worker::_detach_children_async_cb:END");

	_detach_children_impl();
}


std::vector<std::weak_ptr<Worker>>
Worker::gather_children() const
{
	L_CALL("Worker::gather_children() {}", __repr__());

	std::lock_guard<std::recursive_mutex> lk(_mtx);

	// Collect active children
	std::vector<std::weak_ptr<Worker>> weak_children;
	weak_children.reserve(_children.size());
	for (auto& child : _children) {
		if (child) {
			weak_children.push_back(child);
		}
	}
	return weak_children;
}


std::shared_ptr<Worker>
Worker::parent() const
{
	L_CALL("Worker::parent() {}", __repr__());

	std::lock_guard<std::recursive_mutex> lk(_mtx);

	return _parent.lock();
}


#ifdef L_WORKER
#define LOG_WORKER
#else
#define L_WORKER L_NOTHING
#endif


std::string
Worker::__repr__() const
{
	return string::format(STEEL_BLUE + "<Worker {{cnt:{}}}{}{}{}>",
		use_count(),
		is_runner() ? " " + DARK_STEEL_BLUE + "(runner)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(worker)" + STEEL_BLUE,
		is_running_loop() ? " " + DARK_STEEL_BLUE + "(running loop)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(stopped loop)" + STEEL_BLUE,
		is_detaching() ? " " + ORANGE + "(detaching)" + STEEL_BLUE : "");
}


std::string
Worker::dump_tree(int level)
{
	std::string ret;
	for (int l = 0; l < level; ++l) {
		ret += "    ";
	}
	ret += __repr__();
	ret.push_back('\n');

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			ret += child->dump_tree(level + 1);
		}
	}

	return ret;
}


void
Worker::shutdown_impl(long long asap, long long now)
{
	L_CALL("Worker::shutdown_impl({}, {}) {}", asap, now, __repr__());

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			auto async = (child->ev_loop->raw_loop != ev_loop->raw_loop);
			child->shutdown(asap, now, async);
		}
	}
}


void
Worker::_break_loop_impl()
{
	L_CALL("Worker::_break_loop_impl() {}", __repr__());

	ev_loop->break_loop();
}


inline void
Worker::_start_impl()
{
	L_CALL("Worker::_start_impl()");

	L_EV_BEGIN("Worker::_start_impl:BEGIN");
	L_EV_END("Worker::_start_impl:END");

	if (!_started) {
		start_impl();
		_started = true;
	}
}


void
Worker::_stop_impl()
{
	L_CALL("Worker::_stop_impl()");

	L_EV_BEGIN("Worker::_stop_impl:BEGIN");
	L_EV_END("Worker::_stop_impl:END");

	if (_started) {
		stop_impl();
		_started = false;
	}
}

void
Worker::_destroy_impl()
{
	L_CALL("Worker::_destroy_impl()");

	L_EV_BEGIN("Worker::_destroy_impl:BEGIN");
	L_EV_END("Worker::_destroy_impl:END");

	if (!_destroyed) {
		destroy_impl();
		_destroyed = true;
	}
}


void
Worker::_detach_impl(const std::weak_ptr<Worker>& weak_child)
{
	L_CALL("Worker::_detach_impl(<weak_child>, {}) {}", __repr__());

	std::unique_lock<std::recursive_mutex> lk(_mtx);

#ifdef LOG_WORKER
	std::string child_repr;
	long child_use_count;
#endif

	std::this_thread::yield();

	if (auto child = weak_child.lock()) {
		if (child->is_runner() && child->is_running_loop()) {
			return;
		}
#ifdef LOG_WORKER
		child_repr = child->__repr__();
		child_use_count = child.use_count();
#endif
		__detach(child);
		lk.unlock();
		child.reset();
		lk.lock();
	} else {
		return;  // It was already detached
	}

	if (auto child = weak_child.lock()) {
		// Object still lives, re-attach
		__attach(child);
		return;
	}

	L_WORKER(FOREST_GREEN + "Worker child {} (cnt: {}) detached from {} (cnt: {})", child_repr, child_use_count - 1, __repr__(), shared_from_this().use_count() - 1);
}


void
Worker::_detach_children_impl()
{
	L_CALL("Worker::_detach_children_impl() {}", __repr__());

	auto weak_children = gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			auto async = (child->ev_loop->raw_loop != ev_loop->raw_loop);
			child->_detach_children(async);
			if (!child->_detaching || async) {
				continue;
			}
		}
		_detach_impl(weak_child);
	}
}


void
Worker::shutdown(bool async)
{
	L_CALL("Worker::shutdown() {}", __repr__());

	auto now = epoch::now<>();
	shutdown(now, 0, async);
}


void
Worker::shutdown(long long asap, long long now, bool async)
{
	L_CALL("Worker::shutdown({}, {}) {}", asap, now, __repr__());

	if (async) {
		_asap = asap;
		_now = now;
		_shutdown_async.send();
	} else {
		shutdown_impl(asap, now);
	}
}


void
Worker::break_loop(bool async)
{
	L_CALL("Worker::break_loop() {}", __repr__());

	if (async) {
		_break_loop_async.send();
	} else {
		_break_loop_impl();
	}
}


void
Worker::destroy(bool async)
{
	L_CALL("Worker::destroy() {}", __repr__());

	if (async) {
		_destroy_async.send();
	} else {
		_destroy_impl();
	}
}


void
Worker::start(bool async)
{
	L_CALL("Worker::start() {}", __repr__());

	if (async) {
		_start_async.send();
	} else {
		_start_impl();
	}
}


void
Worker::stop(bool async)
{
	L_CALL("Worker::stop() {}", __repr__());

	if (async) {
		_stop_async.send();
	} else {
		_stop_impl();
	}
}


void
Worker::_detach_children(bool async)
{
	L_CALL("Worker::_detach_children() {}", __repr__());

	if (async) {
		_detach_children_async.send();
	} else {
		_detach_children_impl();
	}
}


void
Worker::detach(bool async)
{
	L_CALL("Worker::detach() {}", __repr__());

	_detaching = true;
	auto p = parent();
	if (p) {
		p->_detach_children(async);
	}
}


void
Worker::redetach(bool async)
{
	L_CALL("Worker::redetach() {}", __repr__());

	// Needs to be run at the end of Workers's run(), to try re-detaching

	if (_detaching) {
		auto p = parent();
		if (p) {
			p->_detach_children(async);
		}
	}
}


void
Worker::run_loop()
{
	L_CALL("Worker::run_loop() {}", __repr__());

	ASSERT(!is_running_loop());

	if (!_runner.exchange(true)) {
		ev_loop->run();
		_runner = false;
	}
}

void
Worker::finish()
{
	L_CALL("Worker::finish() {}", __repr__());

	stop();
	destroy();
	if (is_runner()) {
		break_loop();
	} else {
		detach();
	}
}
