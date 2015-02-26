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
	sig.set<BaseClient, &BaseClient::signal_cb>(this);
	sig.start(SIGINT);

	async.set<BaseClient, &BaseClient::async_cb>(this);
	async.start();

	io.set<BaseClient, &BaseClient::io_cb>(this);
	io.start(sock, ev::READ);
}


BaseClient::~BaseClient()
{
	destroy();
	sig.stop();
	LOG_OBJ(this, "DELETED!\n");
}


void BaseClient::signal_cb(ev::sig &signal, int revents)
{
	LOG_EV(this, "Signaled destroy!!\n");
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
	LOG_OBJ(this, "DESTROYED!\n");
}


void BaseClient::close() {
	if (closed) {
		return;
	}

	closed = true;
	LOG_OBJ(this, "CLOSED!\n");
}


void BaseClient::async_cb(ev::async &watcher, int revents)
{
	if (destroyed) {
		return;
	}

	LOG_EV(this, "ASYNC_CB (sock=%d) %x\n", sock, revents);

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


void BaseClient::io_cb(ev::io &watcher, int revents)
{
	if (destroyed) {
		return;
	}

	LOG_EV(this, "IO_CB (sock=%d) %x\n", sock, revents);

	if (revents & EV_ERROR) {
		LOG_ERR(this, "ERROR: got invalid event (sock=%d): %s\n", sock, strerror(errno));
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

		size_t buff_size = buffer->nbytes();
		const char * buff = buffer->dpos();

		LOG_CONN(this, "(sock=%d) <<-- '%s'\n", sock, repr(buff, buff_size).c_str());

		ssize_t written = ::write(watcher.fd, buff, buff_size);

		if (written < 0) {
			LOG_ERR(this, "ERROR: write error (sock=%d): %s\n", sock, strerror(errno));
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
