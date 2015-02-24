#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>


#include "client_base.h"


const int WRITE_QUEUE_SIZE = 30;


int BaseClient::total_clients = 0;


BaseClient::BaseClient(int sock_, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: sock(sock_),
	  thread_pool(thread_pool_),
	  database_pool(database_pool_),
	  write_queue(WRITE_QUEUE_SIZE)
{
	pthread_mutex_init(&qmtx, 0);

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	io.set<BaseClient, &BaseClient::callback>(this);
	io.start(sock, ev::READ);

	sig.set<BaseClient, &BaseClient::signal_cb>(this);
	sig.start(SIGINT);
}


BaseClient::~BaseClient()
{
	shutdown(sock, SHUT_RDWR);

	// Stop and free watcher if client socket is closing
	io.stop();
	sig.stop();

	close(sock);

	pthread_mutex_destroy(&qmtx);
}


void BaseClient::callback(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	if (revents & EV_READ)
		read_cb(watcher);

	if (revents & EV_WRITE)
		write_cb(watcher);

	pthread_mutex_lock(&qmtx);
	if (write_queue.empty()) {
		io.set(ev::READ);
	} else {
		io.set(ev::READ|ev::WRITE);
	}
	pthread_mutex_unlock(&qmtx);
}


void BaseClient::signal_cb(ev::sig &signal, int revents)
{
	delete this;
}
