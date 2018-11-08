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

#include "worker.h"

#include <thread>

#include "cassert.hh"           // for assert

#include "epoch.hh"             // for epoch::now
#include "ignore_unused.h"
#include "log.h"
#include "readable_revents.hh"   // for readable_revents


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


Worker::~Worker()
{
	L_CALL("Worker::~Worker() %s", __repr__());

	// Make sure to call Worker::deinit() as the last line in the
	// destructor of any subclasses implementing either one of:
	// shutdown_impl(), destroy_impl(), start_impl() or stop_impl().
	// Otherwise the assert bellow will fire!
	assert(_deinited);  // Beware of the note above

	deinit();
}


void
Worker::deinit()
{
	L_CALL("Worker::deinit() %s", __repr__());

	_stopper();
	_destroyer();
	_deinit();
}


void
Worker::_init()
{
	L_CALL("Worker::_init() %s", __repr__());

	if (_parent) {
		_iterator = _parent->_children.end();
	}

	_shutdown_async.set<Worker, &Worker::_shutdown_async_cb>(this);
	_shutdown_async.start();
	L_EV("Start %s async shutdown event", __repr__());

	_break_loop_async.set<Worker, &Worker::_break_loop_async_cb>(this);
	_break_loop_async.start();
	L_EV("Start %s async break_loop event", __repr__());

	_destroy_async.set<Worker, &Worker::_destroy_async_cb>(this);
	_destroy_async.start();
	L_EV("Start %s async destroy event", __repr__());

	_start_async.set<Worker, &Worker::_start_async_cb>(this);
	_start_async.start();
	L_EV("Start %s async start event", __repr__());

	_stop_async.set<Worker, &Worker::_stop_async_cb>(this);
	_stop_async.start();
	L_EV("Start %s async stop event", __repr__());

	_detach_children_async.set<Worker, &Worker::_detach_children_async_cb>(this);
	_detach_children_async.start();
	L_EV("Start %s async detach children event", __repr__());
}


void
Worker::_deinit()
{
	L_CALL("Worker::_deinit() %s", __repr__());

	if (!_deinited) {
		_detach_children_async.stop();
		L_EV("Stop %s async detach children event", __repr__());

		_stop_async.stop();
		L_EV("Stop %s async stop event", __repr__());

		_start_async.stop();
		L_EV("Stop %s async start event", __repr__());

		_destroy_async.stop();
		L_EV("Stop %s async destroy event", __repr__());

		_break_loop_async.stop();
		L_EV("Stop %s async break_loop event", __repr__());

		_shutdown_async.stop();
		L_EV("Stop %s async shutdown event", __repr__());

		_deinited = true;
	}

}


void
Worker::_destroyer()
{
	L_CALL("Worker::_destroyer() %s", __repr__());

	if (!_destroyed) {
		L_EV_BEGIN("Worker::_destroyer:BEGIN");
		destroy_impl();
		L_EV_END("Worker::_destroyer:END");

		_destroyed = true;
	}

}


inline void
Worker::_starter()
{
	L_CALL("Worker::_starter() %s", __repr__());

	if (!_started) {
		L_EV_BEGIN("Worker::_starter:BEGIN");
		start_impl();
		L_EV_END("Worker::_starter:END");

		_started = true;
	}
}


void
Worker::_stopper()
{
	L_CALL("Worker::_stopper() %s", __repr__());

	if (_started) {
		L_EV_BEGIN("Worker::_stopper:BEGIN");
		stop_impl();
		L_EV_END("Worker::_stopper:END");

		_started = false;
	}
}


void
Worker::_shutdown_async_cb()
{
	L_CALL("Worker::_shutdown_async_cb() %s", __repr__());

	L_EV_BEGIN("Worker::_shutdown_async_cb:BEGIN");
	shutdown_impl(_asap, _now);
	L_EV_END("Worker::_shutdown_async_cb:END");
}


void
Worker::_break_loop_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("Worker::_break_loop_async_cb(<watcher>, 0x%x (%s)) %s", revents, readable_revents(revents), __repr__());

	ignore_unused(revents);

	L_EV_BEGIN("Worker::_break_loop_async_cb:BEGIN");
	break_loop_impl();
	L_EV_END("Worker::_break_loop_async_cb:END");
}


void
Worker::_destroy_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("Worker::_destroy_async_cb(<watcher>, 0x%x (%s)) %s", revents, readable_revents(revents), __repr__());

	ignore_unused(revents);

	_destroyer();
}


void
Worker::_start_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("Worker::_start_async_cb(<watcher>, 0x%x (%s)) %s", revents, readable_revents(revents), __repr__());

	ignore_unused(revents);

	_starter();
}


void
Worker::_stop_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("Worker::_stop_async_cb(<watcher>, 0x%x (%s)) %s", revents, readable_revents(revents), __repr__());

	ignore_unused(revents);

	_stopper();
}


void
Worker::_detach_children_async_cb(ev::async& /*unused*/, int revents)
{
	L_CALL("Worker::_detach_children_async_cb(<watcher>, 0x%x (%s)) %s", revents, readable_revents(revents), __repr__());

	ignore_unused(revents);

	L_EV_BEGIN("Worker::_detach_children_async_cb:BEGIN");
	detach_children_impl();
	L_EV_END("Worker::_detach_children_async_cb:END");
}


std::vector<std::weak_ptr<Worker>>
Worker::_gather_children()
{
	L_CALL("Worker::_gather_children() %s", __repr__());

	std::lock_guard<std::recursive_mutex> lk(_mtx);

	// Collect active children
	std::vector<std::weak_ptr<Worker>> weak_children;
	weak_children.reserve(_children.size());
	for (auto it = _children.begin(); it != _children.end();) {
		auto child = *it;
		if (child) {
			weak_children.push_back(child);
			++it;
		} else {
			it = _children.erase(it);
		}
	}
	return weak_children;
}


#ifdef L_WORKER
#define LOG_WORKER
#else
#define L_WORKER L_NOTHING
#endif

void
Worker::_detach_impl(const std::weak_ptr<Worker>& weak_child, int retries)
{
	L_CALL("Worker::_detach_impl(<weak_child>, %d) %s", retries, __repr__());

	std::lock_guard<std::recursive_mutex> lk(_mtx);

#ifdef LOG_WORKER
	std::string child_repr;
	long child_use_count;
#endif

	std::this_thread::yield();

	if (auto child = weak_child.lock()) {
		if (child->_runner && (child->ev_loop->depth() != 0u)) {
			if (retries == 0) {
				L_WORKER(LIGHT_RED + "Worker child (in a running loop) %s (cnt: %ld) cannot be detached from %s (cnt: %ld)", child->__repr__(), child.use_count() - 1, __repr__(), shared_from_this().use_count() - 1);
			} else if (retries > 0) {
				child->redetach(retries - 1);
			}
			return;
		}
		__detach(child);
#ifdef LOG_WORKER
		child_repr = child->__repr__();
		child_use_count = child.use_count();
#endif
	} else {
		return;  // It was already detached
	}

	if (auto child = weak_child.lock()) {
		__attach(child);
		if (retries == 0) {
			L_WORKER(BROWN + "Worker child %s (cnt: %ld) cannot be detached from %s (cnt: %ld)", child_repr, child_use_count - 1, __repr__(), shared_from_this().use_count() - 1);
		} else if (retries > 0) {
			child->redetach(retries - 1);
		}
		return;
	}

	L_WORKER(FOREST_GREEN + "Worker child %s (cnt: %ld) detached from %s (cnt: %ld)", child_repr, child_use_count - 1, __repr__(), shared_from_this().use_count() - 1);
}


auto
Worker::_ancestor(int levels)
{
	L_CALL("Worker::_ancestor(%d) %s", levels, __repr__());

	std::lock_guard<std::recursive_mutex> lk(_mtx);

	auto ancestor = shared_from_this();
	while (ancestor->_parent && levels-- != 0) {
		ancestor = ancestor->_parent;
	}
	return ancestor;
}


std::string
Worker::__repr__(const std::string& name) const
{
	return string::format("<%s at %p, %s of %s %p%s>",
		name,
		static_cast<const void *>(this),
		_runner ? "runner" : "worker",
		ev_loop->depth() != 0u ? "running loop" : "loop",
		static_cast<const void *>(ev_loop->raw_loop),
		_detaching ? " (deteaching)" : ""
	);
}


std::string
Worker::dump_tree(int level)
{
	std::lock_guard<std::recursive_mutex> lk(_mtx);
	std::string ret = "\n";
	for (int l = 0; l < level; ++l) {
		ret += "    ";
	}
	ret += __repr__() + " (cnt: " + std::to_string(shared_from_this().use_count() - 1) + ")";
	for (const auto& c : _children) {
		ret += c->dump_tree(level + 1);
	}
	return ret;
}


void
Worker::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("Worker::shutdown_impl(%d, %d) %s", (int)asap, (int)now, __repr__());

	auto weak_children = _gather_children();
	for (auto& weak_child : weak_children) {
		if (auto child = weak_child.lock()) {
			auto async = (child->ev_loop->raw_loop != ev_loop->raw_loop);
			child->shutdown(asap, now, async);
		}
	}
}


void
Worker::break_loop_impl()
{
	L_CALL("Worker::break_loop_impl() %s", __repr__());

	ev_loop->break_loop();
}


void
Worker::detach_children_impl()
{
	L_CALL("Worker::detach_children_impl() %s", __repr__());

	auto weak_children = _gather_children();
	for (auto& weak_child : weak_children) {
		int retries = 0;
		if (auto child = weak_child.lock()) {
			child->_detach_children(true);
			if (!child->_detaching) {
				continue;
			}
			retries = child->_detaching_retries;
		}
		_detach_impl(weak_child, retries);
	}
}


void
Worker::shutdown(bool async)
{
	L_CALL("Worker::shutdown() %s", __repr__());

	auto now = epoch::now<>();
	shutdown(now, now, async);
}


void
Worker::shutdown(time_t asap, time_t now, bool async)
{
	L_CALL("Worker::shutdown(%d, %d) %s", (int)asap, (int)now, __repr__());

	if (async && ev_loop->depth() != 0u) {
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
	L_CALL("Worker::break_loop() %s", __repr__());

	if (async && ev_loop->depth() != 0u) {
		_break_loop_async.send();
	} else {
		break_loop_impl();
	}
}


void
Worker::destroy(bool async)
{
	L_CALL("Worker::destroy() %s", __repr__());

	if (async && ev_loop->depth() != 0u) {
		_destroy_async.send();
	} else {
		_destroyer();
	}
}


void
Worker::start(bool async)
{
	L_CALL("Worker::start() %s", __repr__());

	if (async && ev_loop->depth() != 0u) {
		_start_async.send();
	} else {
		_starter();
	}
}


void
Worker::stop(bool async)
{
	L_CALL("Worker::stop() %s", __repr__());

	if (async && ev_loop->depth() != 0u) {
		_stop_async.send();
	} else {
		_stopper();
	}
}


void
Worker::_detach_children(bool async)
{
	if (async && ev_loop->depth() != 0u) {
		_detach_children_async.send();
	} else {
		detach_children_impl();
	}
}


void
Worker::detach(int retries, bool async)
{
	L_CALL("Worker::detach() %s", __repr__());

	_detaching = true;
	_detaching_retries = retries;

	_ancestor(1)->_detach_children(async);
}


void
Worker::redetach(int retries, bool async)
{
	L_CALL("Worker::redetach() %s", __repr__());

	// Needs to be run at the end of Workers's run(), to try re-detaching

	if (_detaching) {
		_detaching_retries = retries;
		_ancestor(1)->_detach_children(async);
	}
}


void
Worker::run_loop()
{
	L_CALL("Worker::run_loop() %s", __repr__());

	assert(ev_loop->depth() == 0);

	_runner = true;
	ev_loop->run();
}
