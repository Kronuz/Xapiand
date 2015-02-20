#ifndef XAPIAND_INCLUDED_THREADPOOL_H
#define XAPIAND_INCLUDED_THREADPOOL_H

#include <queue>

#include <pthread.h>

//
//   Base task for Tasks
//   run() should be overloaded and expensive calculations done there
//   showTask() is for debugging and can be deleted if not used
//
class Task {
public:
	Task() {}
	virtual ~Task() {}
	virtual void run() = 0;
	virtual void showTask() = 0;
};

// Wrapper around std::queue with some mutex protection
class WorkQueue {
public:
	WorkQueue();

	~WorkQueue();

	// Retrieves the next task from the queue
	Task *nextTask();

	// Add a task
	void addTask(Task *nt);

	// Mark the queue finished
	void finish();

	// Check if there's work
	bool hasWork();

private:
	std::queue<Task*> tasks;
	bool finished;
	pthread_mutex_t qmtx;
	pthread_cond_t wcond;
};


class ThreadPool {
public:
	// Allocate a thread pool and set them to work trying to get tasks
	ThreadPool(int n);

	// Wait for the threads to finish, then delete them
	~ThreadPool();

	// Add a task
	void addTask(Task *nt);
	// Tell the tasks to finish and return
	void finish();

	// Checks if there is work to do
	bool hasWork();
	// Super inefficient way to wait for all tasks to finish
	void waitForCompletion();

private:
	pthread_t *threads;
	int _numThreads;
	WorkQueue workQueue;
};

#endif /* XAPIAND_INCLUDED_THREADPOOL_H */
