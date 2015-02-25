#include <sys/socket.h>

#include "utils.h"
#include "client_base.h"


const int WRITE_QUEUE_SIZE = 30;


int BaseClient::total_clients = 0;


BaseClient::BaseClient(ev::loop_ref &loop, int sock_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: io(loop),
	  async(loop),
	  destroyed(false),
	  closed(false),
	  sock(sock_),
	  database_pool(database_pool_),
	  write_queue(WRITE_QUEUE_SIZE)
{
	io.set<BaseClient, &BaseClient::callback>(this);
	io.start(sock, ev::READ);

	async.set<BaseClient, &BaseClient::async_cb>(this);
	async.start();

	sig.set<BaseClient, &BaseClient::signal_cb>(this);
	sig.start(SIGINT);
}


BaseClient::~BaseClient()
{
	destroy();
	sig.stop();
	log(this, "DELETED!\n");
}


void BaseClient::signal_cb(ev::sig &signal, int revents)
{
	log(this, "Signaled destroy!!\n");
	destroy();
	delete this;
}


void BaseClient::destroy()
{
	if (destroyed) {
		return;
	}

	destroyed = true;

	close();
	
	// Stop and free watcher if client socket is closing
	io.stop();
	async.stop();
	
	::close(sock);
	log(this, "DESTROYED!\n");
}


void BaseClient::close() {
	if (closed) {
		return;
	}

	closed = true;
	log(this, "CLOSED!\n");
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

		log(this, ">>> '%s'\n", repr_string(std::string(buffer->dpos(), buffer->nbytes())).c_str());

		ssize_t written = ::send(watcher.fd, buffer->dpos(), buffer->nbytes(), 0);

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


void BaseClient::write(const char *buf, size_t buf_size)
{
	Buffer *buffer = new Buffer('\0', buf, buf_size);
	write_queue.push(buffer);

	async.send();
}
