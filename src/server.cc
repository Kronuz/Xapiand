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
			printf("\\x%02x", *p++ & 0xff);
		}
	}
	printf("'\n");
}

class XapianWorker : public Task {
private:
	XapiandClient *client;

public:
	XapianWorker(XapiandClient *client_) : Task(), client(client_) {}

	~XapianWorker() {}

	virtual void run() {
		client->run_one();
	}
};

//
//   Buffer class - allow for output buffering such that it can be written out
//                                 into async pieces
//
struct Buffer {
	char type;
	char *data;
	ssize_t len;
	ssize_t pos;

	Buffer(char type_, const char *bytes, ssize_t nbytes) {
		pos = 0;
		len = nbytes;
		type = type_;
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
	XapiandClient * client = static_cast<XapiandClient *>(obj);
	return client->get_message(timeout, result);
}

void
custom_send_message(void * obj, reply_type type, const std::string &message)
{
	XapiandClient * client = static_cast<XapiandClient *>(obj);
	client->send_message(type, message);
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



void XapiandClient::callback(ev::io &watcher, int revents)
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

void XapiandClient::async_cb(ev::async &watcher, int revents)
{
	pthread_mutex_lock(&qmtx);
	if (!write_queue.empty()) {
		io.set(ev::READ|ev::WRITE);
	}
	pthread_mutex_unlock(&qmtx);
}

void XapiandClient::write_cb(ev::io &watcher)
{
	pthread_mutex_lock(&qmtx);

	if (write_queue.empty()) {
		io.set(ev::READ);
	} else {
		Buffer* buffer = write_queue.front();

		// printf("sent:");
		// print_string(std::string(buffer->dpos(), buffer->nbytes()));

		ssize_t written = write(watcher.fd, buffer->dpos(), buffer->nbytes());
		if (written < 0) {
			perror("read error");
		} else {
			buffer->pos += written;
			if (buffer->nbytes() == 0) {
				write_queue.pop_front();
				delete buffer;
			}
		}
	}

	pthread_mutex_unlock(&qmtx);
}

void XapiandClient::read_cb(ev::io &watcher)
{
	char buf[1024];

	ssize_t received = recv(watcher.fd, buf, sizeof(buf), 0);

	if (received < 0) {
		perror("read error");
		return;
	}

	if (received == 0) {
		// Gack - we're deleting ourself inside of ourself!
		delete this;
	} else {
		buffer.append(buf, received);
		if (buffer.length() >= 2) {
			const char *o = buffer.data();
			const char *p = o;
			const char *p_end = p + buffer.size();

			message_type required_type = static_cast<message_type>(*p++);
			size_t len;
			try {
				len = decode_length(&p, p_end, true);
			} catch (const Xapian::NetworkError & e) {
				return;
			}
			std::string data = std::string(p, len);
			buffer.erase(0, p - o + len);

			// printf("received:");
			// print_string(data);

			Buffer *msg = new Buffer(required_type, data.c_str(), data.size());

			messages_queue.push(msg);

			if (required_type == MSG_QUERY) {
				thread_pool->addTask(new XapianWorker(this));
			}

		}
	}
}


void XapiandClient::signal_cb(ev::sig &signal, int revents)
{
	delete this;
}


message_type XapiandClient::get_message(double timeout, std::string & result)
{
	Buffer* msg;
	if (!messages_queue.pop(msg)) {
		throw Xapian::NetworkError("No message available");
	}

	std::string buf(&msg->type, 1);
	buf += encode_length(msg->nbytes());
	buf += std::string(msg->dpos(), msg->nbytes());
	printf("get_message:");
	print_string(buf);

	message_type required_type = static_cast<message_type>(msg->type);
	result.assign(msg->dpos(), msg->nbytes());

	delete msg;
	return required_type;
}


void XapiandClient::send_message(reply_type type, const std::string &message)
{
	char type_as_char = static_cast<char>(type);
	std::string buf(&type_as_char, 1);
	buf += encode_length(message.size());
	buf += message;

	printf("send_message:");
	print_string(buf);

	pthread_mutex_lock(&qmtx);
	write_queue.push_back(new Buffer(type, buf.c_str(), buf.size()));
	pthread_mutex_unlock(&qmtx);

	async.send();
}


void XapiandClient::run_one()
{
	try {
		server->run_one();
	} catch (const Xapian::NetworkError &e) {
		printf("ERROR: %s\n", e.get_msg().c_str());
	} catch (...) {
		printf("ERROR!\n");
	}
}


XapiandClient::XapiandClient(int sock_, ThreadPool *thread_pool_)
	: sock(sock_),
	  thread_pool(thread_pool_),
	  server(NULL)
{
	pthread_mutex_init(&qmtx, 0);

	fcntl(sock, F_SETFL, fcntl(sock, F_GETFL, 0) | O_NONBLOCK);

	printf("Got connection, %d client(s) connected.\n", ++total_clients);

	io.set<XapiandClient, &XapiandClient::callback>(this);
	io.start(sock, ev::READ);

	sig.set<XapiandClient, &XapiandClient::signal_cb>(this);
	sig.start(SIGINT);

	async.set<XapiandClient, &XapiandClient::async_cb>(this);
	async.start();

	dbpaths.push_back("/Users/kronuz/Development/Dubalu/Xapian/xapian-core-1.3.2/bin/Xapiand/test");
	server = new RemoteServer(
		this,
		dbpaths, true,
		custom_get_message,
		custom_send_message,
		custom_get_database,
		custom_get_writable_database
	);

	try {
		server->msg_update(std::string());
	} catch (const Xapian::NetworkError &e) {
		printf("ERROR: %s\n", e.get_msg().c_str());
	} catch (...) {
		printf("ERROR!\n");
	}
}

XapiandClient::~XapiandClient()
{
	shutdown(sock, SHUT_RDWR);

	// Stop and free watcher if client socket is closing
	io.stop();
	sig.stop();
	async.stop();

	close(sock);

	printf("Lost connection, %d client(s) connected.\n", --total_clients);

	pthread_mutex_destroy(&qmtx);

	delete server;
}


void XapiandServer::io_accept(ev::io &watcher, int revents)
{
	if (EV_ERROR & revents) {
		perror("got invalid event");
		return;
	}

	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_sock = accept(watcher.fd, (struct sockaddr *)&client_addr, &client_len);

	if (client_sock < 0) {
		perror("accept error");
		return;
	}

	new XapiandClient(client_sock, this->thread_pool);
}


void XapiandServer::signal_cb(ev::sig &signal, int revents)
{
	signal.loop.break_loop();
}


XapiandServer::XapiandServer(int port, ThreadPool *thread_pool_)
	: thread_pool(thread_pool_)
{
	int optval = 1;
	struct sockaddr_in addr;

	sock = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

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


XapiandServer::~XapiandServer()
{
	shutdown(sock, SHUT_RDWR);

	io.stop();
	sig.stop();

	close(sock);

	printf("Done with all work!\n");
}


int XapiandClient::total_clients = 0;
