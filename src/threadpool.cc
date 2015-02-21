#include "threadpool.h"


// Function that retrieves a task from a queue, runs it and deletes it
static void *getWork(void * param) {
	Task *mw = 0;
	WorkQueue *wq = (WorkQueue*)param;
	while ((mw = wq->nextTask())) {
		mw->run();
		delete mw;
	}
	return 0;
}


WorkQueue::WorkQueue() {
	// wcond is a condition variable that's signaled when new work arrives
	pthread_cond_init(&wcond, 0);

	// Initialize the mutex protecting the queue
	pthread_mutex_init(&qmtx, 0);
}

WorkQueue::~WorkQueue() {
	// Cleanup pthreads
	pthread_mutex_destroy(&qmtx);

	pthread_cond_destroy(&wcond);
}

// Retrieves the next task from the queue
Task *WorkQueue::nextTask() {
	// Lock the queue mutex
	pthread_mutex_lock(&qmtx);

	// The return value
	Task *nt = NULL;

	while(true) {
		// Check if there's work
		if (finished && tasks.size() == 0) {
			// If not return null (0)
			nt = 0;
		} else {
			// Not finished, but there are no tasks, so wait for
			// wcond to be signalled
			if (tasks.size() == 0) {
				pthread_cond_wait(&wcond, &qmtx);
				continue;
			}
			// get the next task
			nt = tasks.front();
			tasks.pop();
		}
		break;
	}

	// Unlock the mutex and return
	pthread_mutex_unlock(&qmtx);

	return nt;
}

// Add a task
void WorkQueue::addTask(Task *nt) {
	pthread_mutex_lock(&qmtx);

	// Only add the task if the queue isn't marked finished
	if (!finished) {
		tasks.push(nt); // Add the task
		pthread_cond_signal(&wcond); // signal there's new work
	}

	pthread_mutex_unlock(&qmtx);
}

// Mark the queue finished
void WorkQueue::finish() {
	pthread_mutex_lock(&qmtx);

	finished = true;

	// Signal the condition variable in case any threads are waiting
	pthread_cond_broadcast(&wcond);

	pthread_mutex_unlock(&qmtx);
}


// Allocate a thread pool and set them to work trying to get tasks
ThreadPool::ThreadPool(int n) : numThreads(n) {
	printf("Creating a thread pool with %d threads\n", n);

	threads = new pthread_t[numThreads];
	for (int i = 0; i < numThreads; ++i) {
		pthread_create(&(threads[i]), 0, getWork, &workQueue);
	}
}

// Wait for the threads to finish, then delete them
ThreadPool::~ThreadPool() {
	finish();
	for (int i = 0; i < numThreads; ++i) {
		pthread_join(threads[i], 0);
	}
	delete [] threads;
}

// Add a task
void ThreadPool::addTask(Task *nt) {
	workQueue.addTask(nt);
}

// Tell the tasks to finish and return
void ThreadPool::finish() {
	workQueue.finish();
}
