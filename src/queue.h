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

#ifndef XAPIAND_INCLUDED_QUEUE_H
#define XAPIAND_INCLUDED_QUEUE_H

#include <cerrno>
#include <queue>
#include <list>

#ifdef HAVE_CXX11
#  include <unordered_map>
#else
#  include <map>
#endif

#include <sys/time.h>
#include <pthread.h>


template<class T, class Container = std::deque<T> >
class Queue {
	typedef typename std::queue<T, Container> Queue_t;

private:
	Queue_t _items_queue;

protected:
	// A mutex object to control access to the underlying queue object
	pthread_mutex_t qmtx;
	pthread_mutexattr_t qmtx_attr;

	// A variable condition to make threads wait on specified condition values
	pthread_cond_t push_cond;
	pthread_cond_t pop_cond;

	bool finished;
	size_t limit;

public:
	Queue(size_t limit_=-1);
	~Queue();

	void finish();
	void clear();
	bool push(T & element, double timeout=-1.0);
	bool pop(T & element, double timeout=-1.0);

	size_t size();
	T & front();
	bool empty();
};


template<class T, class Container>
Queue<T, Container>::Queue(size_t limit_)
	: finished(false),
	  limit(limit_)
{
	pthread_cond_init(&push_cond, 0);
	pthread_cond_init(&pop_cond, 0);

	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);
}


template<class T, class Container>
Queue<T, Container>::~Queue()
{
	finish();
	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_cond_destroy(&push_cond);
	pthread_cond_destroy(&pop_cond);
}


template<class T, class Container>
void Queue<T, Container>::finish()
{
	pthread_mutex_lock(&qmtx);

	finished = true;

	pthread_mutex_unlock(&qmtx);

	// Signal the condition variable in case any threads are waiting
	pthread_cond_broadcast(&push_cond);
	pthread_cond_broadcast(&pop_cond);

}


template<class T, class Container>
bool Queue<T, Container>::push(T & element, double timeout)
{
	struct timespec ts;
	if (timeout > 0.0) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec + int(timeout);
		ts.tv_nsec = int((timeout - int(timeout)) * 1e9);
	}

	pthread_mutex_lock(&qmtx);

	if (!finished) {
		size_t size = _items_queue.size();
		while (limit > 0 && limit < size) {
			if (!finished && timeout != 0.0) {
				if (timeout > 0.0) {
					if (pthread_cond_timedwait(&pop_cond, &qmtx, &ts) == ETIMEDOUT) {
						pthread_mutex_unlock(&qmtx);
						return false;
					}
				} else {
					pthread_cond_wait(&pop_cond, &qmtx);
				}
			} else {
				pthread_mutex_unlock(&qmtx);
				return false;
			}
			size = _items_queue.size();
		}
		// Insert the element in the FIFO queue
		_items_queue.push(element);
	}

	// Now we need to unlock the mutex otherwise waiting threads will not be able
	// to wake and lock the mutex by time before push is locking again
	pthread_mutex_unlock(&qmtx);

	// Notifiy waiting thread they can pop now
	pthread_cond_signal(&push_cond);

	return true;
}


template<class T, class Container>
bool Queue<T, Container>::pop(T & element, double timeout)
{
	struct timespec ts;
	if (timeout > 0.0) {
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ts.tv_sec = tv.tv_sec + int(timeout);
		ts.tv_nsec = int((timeout - int(timeout)) * 1e9);
	}

	pthread_mutex_lock(&qmtx);

	// While the queue is empty, make the thread that runs this wait
	while(_items_queue.empty()) {
		if (!finished && timeout != 0.0) {
			if (timeout > 0.0) {
				if (pthread_cond_timedwait(&push_cond, &qmtx, &ts) == ETIMEDOUT) {
					pthread_mutex_unlock(&qmtx);
					return false;
				}
			} else {
				pthread_cond_wait(&push_cond, &qmtx);
			}
		} else {
			pthread_mutex_unlock(&qmtx);
			return false;
		}
	}

	//when the condition variable is unlocked, popped the element
	element = _items_queue.front();

	//pop the element
	_items_queue.pop();

	pthread_mutex_unlock(&qmtx);

	// Notifiy waiting thread they can push/push now
	pthread_cond_signal(&push_cond);
	pthread_cond_signal(&pop_cond);

	return true;
};


template<class T, class Container>
void Queue<T, Container>::clear()
{
	pthread_mutex_lock(&qmtx);
	while (!_items_queue.empty()) {
		_items_queue.pop();
	}
	pthread_mutex_unlock(&qmtx);

	// Notifiy waiting thread they can push/push now
	pthread_cond_signal(&pop_cond);
}


template<class T, class Container>
bool Queue<T, Container>::empty()
{
	pthread_mutex_lock(&qmtx);
	bool empty = _items_queue.empty();
	pthread_mutex_unlock(&qmtx);
	return empty;
}


template<class T, class Container>
size_t Queue<T, Container>::size()
{
	pthread_mutex_lock(&qmtx);
	size_t size = _items_queue.size();
	pthread_mutex_unlock(&qmtx);
	return size;
};


template<class T, class Container>
T & Queue<T, Container>::front()
{
	pthread_mutex_lock(&qmtx);
	T & front = _items_queue.front();
	pthread_mutex_unlock(&qmtx);
	return front;
};


#endif /* XAPIAND_INCLUDED_QUEUE_H */
