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
#include <pthread.h>


class Worker
{
	pthread_mutex_t _mtx;
	pthread_mutexattr_t _mtx_attr;

public:
	Worker *_parent;
	std::list<Worker *> _children;
	std::list<Worker *>::const_iterator _iterator;

	std::list<Worker *>::const_iterator _attach(Worker *child) {
		pthread_mutex_lock(&_mtx);
		std::list<Worker *>::const_iterator iterator = _children.insert(_children.end(), child);
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
public:
	Worker(Worker *parent) :
		_parent(parent),
		_iterator(parent ? parent->_attach(this) : std::list<Worker *>::const_iterator())
	{
		pthread_mutexattr_init(&_mtx_attr);
		pthread_mutexattr_settype(&_mtx_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&_mtx, &_mtx_attr);
	}

	~Worker() {
		if (_parent) _parent->_detach(this);
	}

	virtual void shutdown() {
		pthread_mutex_lock(&_mtx);
		std::list<Worker *>::const_iterator it(_children.begin());
		while (it != _children.end()) {
			Worker *child = *(it++);
			child->shutdown();
		}
		pthread_mutex_unlock(&_mtx);
	}
};


#endif /* XAPIAND_INCLUDED_WORKER_H */
