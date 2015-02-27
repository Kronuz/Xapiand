#include <assert.h>
#include <sys/socket.h>

#include "utils.h"
#include "client_base.h"


const int WRITE_QUEUE_SIZE = -1;


int BaseClient::total_clients = 0;


BaseClient::BaseClient(ev::loop_ref *loop, int sock_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: io_read(*loop),
	  io_write(*loop),
	  closed(false),
	  sock(sock_),
	  database_pool(database_pool_),
	  write_queue(WRITE_QUEUE_SIZE)
{
	sig.set<BaseClient, &BaseClient::signal_cb>(this);
	sig.start(SIGINT);

	io_read.set<BaseClient, &BaseClient::io_cb>(this);
	io_read.start(sock, ev::READ);

	io_write.set<BaseClient, &BaseClient::io_cb>(this);
	io_write.start(sock, ev::WRITE);
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
	if (sock == -1) {
		return;
	}

	close();
	
	// Stop and free watcher if client socket is closing
	io_read.stop();
	io_write.stop();
	
	::close(sock);
	sock = -1;

	LOG_OBJ(this, "DESTROYED!\n");
}


void BaseClient::close() {
	if (closed) {
		return;
	}

	closed = true;
	LOG_OBJ(this, "CLOSED!\n");
}


void BaseClient::io_update() {
	if (sock != -1) {
		if (write_queue.empty()) {
			if (closed) {
				destroy();
			} else {
				io_write.stop();
			}
		} else {
			io_write.start();
		}
	}
	
	if (sock == -1) {
		delete this;
	}

}


void BaseClient::io_cb(ev::io &watcher, int revents)
{
	if (revents & EV_ERROR) {
		LOG_ERR(this, "ERROR: got invalid event (sock=%d): %s\n", sock, strerror(errno));
		destroy();
		return;
	}

	LOG_EV(this, "IO_CB (sock=%d) %x\n", sock, revents);

	if (sock == -1) {
		return;
	}
	
	assert(sock == watcher.fd);

	if (sock != -1 && revents & EV_WRITE) {
		write_cb();
	}

	if (sock != -1 && revents & EV_READ) {
		read_cb();
	}
	
	io_update();
}


void BaseClient::write_cb()
{
	if (!write_queue.empty()) {
		Buffer* buffer = write_queue.front();
		
		size_t buf_size = buffer->nbytes();
		const char * buf = buffer->dpos();

		LOG_CONN(this, "(sock=%d) <<-- '%s'\n", sock, repr(buf, buf_size).c_str());

		ssize_t written = ::write(sock, buf, buf_size);
		
		if (written < 0) {
			if (errno != EAGAIN) {
				LOG_ERR(this, "ERROR: write error (sock=%d): %s\n", sock, strerror(errno));
				destroy();
			}
		} else if (written == 0) {
			// nothing written?
		} else {
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				write_queue.pop(buffer);
				delete buffer;
			}
		}
	}
}

void BaseClient::read_cb()
{
	char buf[1024];

	ssize_t received = ::read(sock, buf, sizeof(buf));
	
	if (received < 0) {
		if (errno != EAGAIN) {
			LOG_ERR(this, "ERROR: read error (sock=%d): %s\n", sock, strerror(errno));
			destroy();
		}
	} else if (received == 0) {
		// The peer has closed its half side of the connection.
		LOG_CONN(this, "Received EOF (sock=%d)!\n", sock);
		destroy();
	} else {
		LOG_CONN(this, "(sock=%d) -->> '%s'\n", sock, repr(buf, received).c_str());
		on_read(buf, received);
	}
}


void BaseClient::write(const char *buf, size_t buf_size)
{
	LOG_CONN(this, "(sock=%d) <ENQUEUE> '%s'\n", sock, repr(buf, buf_size).c_str());

	Buffer *buffer = new Buffer('\0', buf, buf_size);
	write_queue.push(buffer);

	io_update();
}
