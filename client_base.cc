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
	  async(loop),
	  destroyed(false),
	  closed(false),
	  sock(sock_),
	  thread_pool(thread_pool_),
	  database_pool(database_pool_),
	  write_queue(WRITE_QUEUE_SIZE)
{
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
	destroy();
	log("DELETED!\n");
}

void BaseClient::destroy()
{
	if (destroyed) {
		return;
	}

	destroyed = true;

	async.send();

	close();

	shutdown(sock, SHUT_RDWR);
	
	// Stop and free watcher if client socket is closing
	io.stop();
	sig.stop();
	async.stop();
	
	::close(sock);
	log("DESTROYED!\n");
}


void BaseClient::close() {
	if (closed) {
		return;
	}

	closed = true;
	log("CLOSED!\n");
}


void BaseClient::async_cb(ev::async &watcher, int revents)
{
	if (destroyed) {
		return;
	}

	if (write_queue.empty()) {
		if (closed) {
			destroy();
		}
	} else {
		io.set(ev::READ|ev::WRITE);
	}
	
	if (destroyed) {
		delete this;
	}
}


void BaseClient::callback(ev::io &watcher, int revents)
{
	if (destroyed) {
		return;
	}

	if (revents & EV_ERROR) {
		perror("got invalid event");
		destroy();
		return;
	}

	if (revents & EV_READ)
		read_cb(watcher);

	if (revents & EV_WRITE)
		write_cb(watcher);

	if (write_queue.empty()) {
		if (closed) {
			destroy();
		} else {
			io.set(ev::READ);
		}
	} else {
		io.set(ev::READ|ev::WRITE);
	}

	if (destroyed) {
		delete this;
	}
}


void BaseClient::write_cb(ev::io &watcher)
{
	if (write_queue.empty()) {
		io.set(ev::READ);
	} else {
		Buffer* buffer = write_queue.front();

		log(">>> ");
		fprint_string(stderr, std::string(buffer->dpos(), buffer->nbytes()));

		ssize_t written = ::write(watcher.fd, buffer->dpos(), buffer->nbytes());

		if (written < 0) {
			perror("write error");
			destroy();
		} else {
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				write_queue.pop(buffer);
				delete buffer;
			}
		}
	}
}


void BaseClient::signal_cb(ev::sig &signal, int revents)
{
	destroy();
	delete this;
}


void BaseClient::write(const char *buf, size_t buf_size)
{
	Buffer *buffer = new Buffer('\0', buf, buf_size);
	write_queue.push(buffer);

	async.send();
}
