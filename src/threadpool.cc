#include "threadpool.h"


// Function that retrieves a task from a queue, runs it and deletes it
static void *getWork(void * param) {
	Queue<Task *> *wq = (Queue<Task *> *)param;
	Task *mw = NULL;
	while (wq->pop(mw)) {
		mw->run();
		delete mw;
	}
	return NULL;
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
	workQueue.push(nt);
}

// Tell the tasks to finish and return
void ThreadPool::finish() {
	workQueue.finish();
}
