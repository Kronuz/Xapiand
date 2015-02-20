#include <fcntl.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "server.h"
#include "net/length.h"

#include "xapian.h"


void print_string(const std::string &string) {
    const char *p = string.c_str();
    const char *p_end = p + string.size();
    printf("'");
    while (p != p_end) {
    	if (*p >= ' ' && *p <= '~') {
			printf("%c", *p++);
    	} else {
			printf("\\x%02x", *p++);
    	}
    }
    printf("'\n");
}


//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//
struct Buffer {
	char *data;
	ssize_t len;
	ssize_t pos;

	Buffer(const char *bytes, ssize_t nbytes) {
		pos = 0;
		len = nbytes;
		data = new char[nbytes];
		memcpy(data, bytes, nbytes);
	}

	virtual ~Buffer() {
		delete [] data;
	}

	char *dpos() {
		return data + pos;
	}

	ssize_t nbytes() {
		return len - pos;
	}
};


message_type
custom_get_message(void * obj, double timeout, std::string & result,
			message_type required_type)
{
	printf("get_message");
	return static_cast<message_type>('\00');
}

void
custom_send_message(void * obj, reply_type type, const std::string &message)
{
	char type_as_char = static_cast<char>(type);
    std::string buf(&type_as_char, 1);
    buf += encode_length(message.size());
    buf += message;

	printf("send_message:");
	print_string(buf);

	XapiandClient * instance = static_cast<XapiandClient *>(obj);
	instance->send(buf);
}

Xapian::Database * custom_get_database(void * obj, const std::vector<std::string> &dbpaths)
{
	if (dbpaths.empty()) return NULL;
	Xapian::Database *db = new Xapian::Database(dbpaths[0], Xapian::DB_CREATE_OR_OPEN);
	return db;
}

Xapian::WritableDatabase * custom_get_writable_database(void * obj, const std::vector<std::string> &dbpaths)
{
	if (dbpaths.empty()) return NULL;
	Xapian::WritableDatabase *db = new Xapian::WritableDatabase(dbpaths[0], Xapian::DB_CREATE_OR_OPEN);
	return db;
}



void XapiandClient::callback(ev::io &watcher, int revents) {
	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	if (revents & EV_READ)
		read_cb(watcher);

	if (revents & EV_WRITE)
		write_cb(watcher);

	if (write_queue.empty()) {
		io.set(ev::READ);
	} else {
		io.set(ev::READ|ev::WRITE);
	}
}

void XapiandClient::async_cb(ev::async &watcher, int revents) {
	if (!write_queue.empty()) {
		io.set(ev::READ|ev::WRITE);
	}
}

void XapiandClient::write_cb(ev::io &watcher) {
	if (write_queue.empty()) {
		io.set(ev::READ);
		return;
	}

	Buffer* buffer = write_queue.front();

	printf("sent:");
	print_string(std::string(buffer->dpos(), buffer->nbytes()));

	ssize_t written = write(watcher.fd, buffer->dpos(), buffer->nbytes());
	if (written < 0) {
		perror("read error");
		return;
	}

	buffer->pos += written;
	if (buffer->nbytes() == 0) {
		write_queue.pop_front();
		delete buffer;
	}
}

void XapiandClient::read_cb(ev::io &watcher) {
	char buffer[1024];

	ssize_t nread = recv(watcher.fd, buffer, sizeof(buffer), 0);

	if (nread < 0) {
		perror("read error");
		return;
	}

	if (nread == 0) {
		// Gack - we're deleting ourself inside of ourself!
		delete this;
	} else {
		// Send message bach to the client
		printf("received:");
		print_string(std::string(buffer, nread));
		write_queue.push_back(new Buffer(buffer, nread));
	}
}

XapiandClient::~XapiandClient() {
	if (sock) shutdown(sock, SHUT_RDWR);

	// Stop and free watcher if client socket is closing
	io.stop();
	async.stop();

	delete server;

	if (sock) close(sock);

	printf("%d client(s) connected.\n", --total_clients);
}

void XapiandClient::send(std::string buffer) {
	write_queue.push_back(new Buffer(buffer.c_str(), buffer.size()));
	async.send();
}

XapiandClient::XapiandClient(int s) : sock(s), server(NULL) {
	fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);

	printf("Got connection\n");
	total_clients++;

	io.set<XapiandClient, &XapiandClient::callback>(this);

	io.start(s, ev::READ);

	async.set<XapiandClient, &XapiandClient::async_cb>(this);
	async.start();

	dbpaths.push_back("test");
	server = new RemoteServer(
	  	this,
	  	dbpaths, true,
		custom_get_message,
		custom_send_message,
		custom_get_database,
		custom_get_writable_database
	);
	server->msg_update(std::string());
}



void XapiandServer::io_accept(ev::io &watcher, int revents) {
	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_sd = accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sd < 0) {
		perror("accept error");
		return;
	}

	XapiandClient *client = new XapiandClient(client_sd);
}

void XapiandServer::signal_cb(ev::sig &signal, int revents) {
	signal.loop.break_loop();
}

XapiandServer::XapiandServer(int port, ThreadPool *thread_pool_)
	: thread_pool(thread_pool_)
{
	struct sockaddr_in addr;

	sock = socket(PF_INET, SOCK_STREAM, 0);

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		perror("bind");
		close(sock);
		sock = 0;
	} else {
		printf("Listening on port %d\n", port);
		fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

		listen(sock, 5);

		io.set<XapiandServer, &XapiandServer::io_accept>(this);
		io.start(sock, ev::READ);

		sig.set<&XapiandServer::signal_cb>();
		sig.start(SIGINT);
	}
}

XapiandServer::~XapiandServer() {
	if (sock) shutdown(sock, SHUT_RDWR);

	io.stop();
	sig.stop();

	if (sock) close(sock);

	printf("Done with all work!\n");
}

int XapiandClient::total_clients = 0;
