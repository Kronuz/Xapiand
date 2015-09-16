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

#ifndef XAPIAND_INCLUDED_WORKER_H
#define XAPIAND_INCLUDED_WORKER_H


#include <list>
#include "ev/ev++.h"
#include <pthread.h>


class Worker
{
protected:
	ev::loop_ref *loop;

	ev::dynamic_loop _dynamic_loop;
	ev::async _break_loop;

	pthread_mutex_t _mtx;
	pthread_mutexattr_t _mtx_attr;

	Worker *_parent;
	std::list<Worker *> _children;
	std::list<Worker *>::iterator _iterator;
	/*should be const_iterator but in linux, std::list member functions use a standard iterator and not const_iterator*/

	std::list<Worker *>::iterator _attach(Worker *child) {
		pthread_mutex_lock(&_mtx);
		std::list<Worker *>::iterator iterator = _children.insert(_children.end(), child);
		pthread_mutex_unlock(&_mtx);
		return iterator;
	}

	void _detach(Worker *child) {
		pthread_mutex_lock(&_mtx);
		if (child->_iterator != _children.end()) {
			_children.erase(child->_iterator);
			child->_iterator = _children.end();
		}
		pthread_mutex_unlock(&_mtx);
	}

	void _break_loop_cb(ev::async &watcher, int revents) {
		loop->break_loop();
	}

public:
	Worker(Worker *parent, ev::loop_ref *loop_) :
		loop(loop_ ? loop_: &_dynamic_loop),
		_break_loop(*loop),
		_parent(parent),
		_iterator(parent ? parent->_attach(this) : std::list<Worker *>::iterator())
	{
		pthread_mutexattr_init(&_mtx_attr);
		pthread_mutexattr_settype(&_mtx_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&_mtx, &_mtx_attr);

		_break_loop.set<Worker, &Worker::_break_loop_cb>(this);
		_break_loop.start();
	}

	virtual ~Worker() {
		_break_loop.stop();

		pthread_mutex_destroy(&_mtx);
		pthread_mutexattr_destroy(&_mtx_attr);

		if (_parent) _parent->_detach(this);
	}

	virtual void shutdown() {
		pthread_mutex_lock(&_mtx);
		std::list<Worker *>::iterator it(_children.begin());
		while (it != _children.end()) {
			Worker *child = *(it++);
			child->shutdown();
		}
		pthread_mutex_unlock(&_mtx);
	}

	void break_loop() {
		_break_loop.send();
	}
};


#endif /* XAPIAND_INCLUDED_WORKER_H */
