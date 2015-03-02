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

#include <assert.h>

#include "utils.h"

#include "threadpool.h"


void Task::inc_ref()
{
	pthread_mutex_lock(&task_mutex);
	refs++;
	pthread_mutex_unlock(&task_mutex);
}

void Task::rel_ref()
{
	pthread_mutex_lock(&task_mutex);
	refs--;
	assert(refs >= 0);
	if (refs == 0) {
		pthread_mutex_unlock(&task_mutex);
		delete this;
	} else {
		pthread_mutex_unlock(&task_mutex);
	}
};

Task::Task() : refs(0)
{
	pthread_mutexattr_init(&task_mutex_attr);
	pthread_mutexattr_settype(&task_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&task_mutex, &task_mutex_attr);
}

Task::~Task()
{
	pthread_mutex_destroy(&task_mutex);
	pthread_mutexattr_destroy(&task_mutex_attr);
}


// Function that retrieves a task from a queue, runs it and deletes it
void *ThreadPool::getWork(void * wq_=NULL) {
	Queue<Task *> *wq = static_cast<Queue<Task *> *>(wq_);
	Task *mw;
	while (wq->pop(mw)) {
		mw->run();
		mw->rel_ref();
	}
	return NULL;
}


// Allocate a thread pool and set them to work trying to get tasks
ThreadPool::ThreadPool(int n) : numThreads(n) {
	LOG_OBJ(this, "Creating a thread pool with %d threads\n", n);

	threads = new pthread_t[numThreads];
	for (int i = 0; i < numThreads; ++i) {
		if (pthread_create(&(threads[i]), 0, getWork, &workQueue) != 0) {
			LOG_CONN(this, "ERROR: thread: %s\n", strerror(errno));
			threads[i] = 0;
		}
	}
}


// Wait for the threads to finish, then delete them
ThreadPool::~ThreadPool() {
	finish();
	join();
	delete [] threads;
}


void ThreadPool::join() {
	for (int i = 0; i < numThreads; ++i) {
		if (threads[i]) {
			if (pthread_join(threads[i], 0) != 0) {
				LOG_CONN(this, "ERROR: thread: %s\n", strerror(errno));
			} else {
				threads[i] = 0;
			}
		}
	}
}


// Add a task
void ThreadPool::addTask(Task *nt) {
	nt->inc_ref();
	workQueue.push(nt);
}


// Tell the tasks to finish and return
void ThreadPool::finish() {
	workQueue.finish();
}
