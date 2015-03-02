/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#ifndef XAPIAND_INCLUDED_THREADPOOL_H
#define XAPIAND_INCLUDED_THREADPOOL_H

#include "queue.h"

class ThreadPool;

//
//   Base task for Tasks
//   run() should be overloaded and expensive calculations done there
//
class Task {
	friend ThreadPool;
private:
	pthread_mutex_t task_mutex;
	int refs;
protected:
	void inc_ref();
	void rel_ref();
public:
	Task();
	virtual ~Task();
	virtual void run() = 0;
};


class ThreadPool {
private:
	pthread_t *threads;
	int numThreads;
	Queue<Task *> workQueue;
	static void *getWork(void * wq_);

public:
	// Allocate a thread pool and set them to work trying to get tasks
	ThreadPool(int n);

	// Wait for the threads to finish, then delete them
	~ThreadPool();

	// Add a task
	void addTask(Task *nt, void *param=NULL);

	// Tell the tasks to finish and return
	void finish();

	// Wait for all threads to end
	void join();
};

#endif /* XAPIAND_INCLUDED_THREADPOOL_H */
