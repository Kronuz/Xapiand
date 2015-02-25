#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "utils.h"
#include "client_base.h"


const int WRITE_QUEUE_SIZE = 30;


int BaseClient::total_clients = 0;


BaseClient::BaseClient(ev::loop_ref &loop, int sock_, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: io(loop),
	  sig(loop),
	  finished(false),
	  sock(sock_),
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

	async.set<BaseClient, &BaseClient::async_cb>(this);
	async.start();
}


BaseClient::~BaseClient()
{
	shutdown(sock, SHUT_RDWR);

	// Stop and free watcher if client socket is closing
	io.stop();
	sig.stop();
	async.stop();

	close(sock);

	pthread_mutex_destroy(&qmtx);
}

void BaseClient::finish() {
	finished = true;
}


void BaseClient::async_cb(ev::async &watcher, int revents)
{

	pthread_mutex_lock(&qmtx);
	if (write_queue.empty()) {
		if (finished) {
			pthread_mutex_unlock(&qmtx);
			delete this;
			return;
		}
	} else {
		io.set(ev::READ|ev::WRITE);
	}
	pthread_mutex_unlock(&qmtx);
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
		if (finished) {
			pthread_mutex_unlock(&qmtx);
			delete this;
			return;
		}
		io.set(ev::READ);
	} else {
		io.set(ev::READ|ev::WRITE);
	}
	pthread_mutex_unlock(&qmtx);
}


void BaseClient::write_cb(ev::io &watcher)
{
	pthread_mutex_lock(&qmtx);

	if (write_queue.empty()) {
		io.set(ev::READ);
	} else {
		Buffer* buffer = write_queue.front();

		// printf(">>> ");
		// print_string(std::string(buffer->dpos(), buffer->nbytes()));

		ssize_t written = write(watcher.fd, buffer->dpos(), buffer->nbytes());
		if (written < 0) {
			perror("read error");
		} else {
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				write_queue.pop(buffer);
				delete buffer;
			}
		}
	}

	pthread_mutex_unlock(&qmtx);
}


void BaseClient::signal_cb(ev::sig &signal, int revents)
{
	delete this;
}

void BaseClient::send(const char *buf)
{
	send(buf, strlen(buf));
}

void BaseClient::send(const std::string &buf)
{
	send(buf.c_str(), buf.size());
}

void BaseClient::send(const char *buf, size_t buf_size)
{
	pthread_mutex_lock(&qmtx);
	Buffer *buffer = new Buffer('\0', buf, buf_size);
	write_queue.push(buffer);
	pthread_mutex_unlock(&qmtx);

	async.send();
}
