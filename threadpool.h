#ifndef XAPIAND_INCLUDED_THREADPOOL_H
#define XAPIAND_INCLUDED_THREADPOOL_H

#include <queue>

#include <pthread.h>

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


// Wrapper around std::queue with some mutex protection
class WorkQueue {
private:
	std::queue<Task*> tasks;
	bool finished;
	pthread_mutex_t qmtx;
	pthread_cond_t wcond;

public:
	WorkQueue();

	~WorkQueue();

	// Retrieves the next task from the queue
	Task *nextTask();

	// Add a task
	void addTask(Task *nt);

	// Mark the queue finished
	void finish();
};


class ThreadPool {
private:
	pthread_t *threads;
	int numThreads;
	WorkQueue workQueue;

public:
	// Allocate a thread pool and set them to work trying to get tasks
	ThreadPool(int n);

	// Wait for the threads to finish, then delete them
	~ThreadPool();

	// Add a task
	void addTask(Task *nt);

	// Tell the tasks to finish and return
	void finish();
};

#endif /* XAPIAND_INCLUDED_THREADPOOL_H */
