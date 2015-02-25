#ifndef XAPIAND_INCLUDED_THREADPOOL_H
#define XAPIAND_INCLUDED_THREADPOOL_H

#include "queue.h"

//
//   Base task for Tasks
//   run() should be overloaded and expensive calculations done there
//
class Task {
public:
	Task() {}
	virtual ~Task() {}
	virtual void run() = 0;
};


class ThreadPool {
private:
	pthread_t *threads;
	int numThreads;
	Queue<Task *> workQueue;

public:
	// Allocate a thread pool and set them to work trying to get tasks
	ThreadPool(int n);

	// Wait for the threads to finish, then delete them
	~ThreadPool();

	// Add a task
	void addTask(Task *nt);

	// Tell the tasks to finish and return
	void finish();

	// Wait for all threads to end
	void join();
};

#endif /* XAPIAND_INCLUDED_THREADPOOL_H */
