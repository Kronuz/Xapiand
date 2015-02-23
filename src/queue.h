#ifndef XAPIAND_INCLUDED_QUEUE_H
#define XAPIAND_INCLUDED_QUEUE_H

#include <queue>
#include <cerrno>

#include <sys/time.h>
#include <pthread.h>


template<class T>
class Queue : public std::queue<T> {
private:
	// A mutex object to control access to the std::queue
	pthread_mutex_t qmtx;
	// A variable condition to make threads wait on specified condition values
	pthread_cond_t wcond;

	bool finished;

public:
	Queue();
	~Queue();

	void finish();
	void push(T& element);
	bool pop(T& element, double timeout=-1.0);

	size_t size();
	bool empty();
};


template<class T>
Queue<T>::Queue()
	: finished(false)
{
	pthread_cond_init(&wcond, 0);
	pthread_mutex_init(&qmtx, 0);
}


template<class T>
Queue<T>::~Queue()
{
	finish();
	pthread_mutex_destroy(&qmtx);
	pthread_cond_destroy(&wcond);
}


template<class T>
void Queue<T>::finish()
{
	pthread_mutex_lock(&qmtx);

	finished = true;

	pthread_mutex_unlock(&qmtx);

	// Signal the condition variable in case any threads are waiting
	pthread_cond_broadcast(&wcond);
}


template<class T>
void Queue<T>::push(T& element)
{
	pthread_mutex_lock(&qmtx);

	if (!finished) {
		// Insert the element in the FIFO queue
		std::queue<T>::push(element);
	}

	// Now we need to unlock the mutex otherwise waiting threads will not be able
	// to wake and lock the mutex by time before push is locking again
	pthread_mutex_unlock(&qmtx);

	// Notifiy waiting thread they can pop/push now
	pthread_cond_signal(&wcond);
}


template<class T>
bool Queue<T>::empty()
{
	pthread_mutex_lock(&qmtx);
	bool empty = std::queue<T>::empty();
	pthread_mutex_unlock(&qmtx);
	return empty;
}


template<class T>
bool Queue<T>::pop(T& element, double timeout)
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
	while(std::queue<T>::empty()) {
		if (!finished && timeout != 0.0) {
			if (timeout > 0.0) {
				if (pthread_cond_timedwait(&wcond, &qmtx, &ts) == ETIMEDOUT) {
					pthread_mutex_unlock(&qmtx);
					return false;
				}
			} else {
				pthread_cond_wait(&wcond, &qmtx);
			}
		} else {
			pthread_mutex_unlock(&qmtx);
			return false;
		}
	}

	//when the condition variable is unlocked, popped the element
	element = std::queue<T>::front();

	//pop the element
	std::queue<T>::pop();

	pthread_mutex_unlock(&qmtx);
	return true;
};


template<class T>
size_t Queue<T>::size()
{
	pthread_mutex_lock(&qmtx);
	size_t size = std::queue<T>::size();
	pthread_mutex_unlock(&qmtx);
	return size;
};

#endif /* XAPIAND_INCLUDED_QUEUE_H */
