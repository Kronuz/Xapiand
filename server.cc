#include <fcntl.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "server.h"
#include "net/length.h"

#include "xapian.h"

const int WRITE_QUEUE_SIZE = 30;

const int MSECS_IDLE_TIMEOUT_DEFAULT = 60000;
const int MSECS_ACTIVE_TIMEOUT_DEFAULT = 15000;


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
		client->run();
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
				write_queue.pop(buffer);
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

			message_type type = static_cast<message_type>(*p++);
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

			Buffer *msg = new Buffer(type, data.c_str(), data.size());

			messages_queue.push(msg);

			if (type != MSG_GETMSET && type != MSG_SHUTDOWN) {
				thread_pool->addTask(new XapianWorker(this));
			}

		}
	}
}


void XapiandClient::signal_cb(ev::sig &signal, int revents)
{
	delete this;
}


message_type XapiandClient::get_message(double timeout, std::string & result, message_type required_type)
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
	
	message_type type = static_cast<message_type>(msg->type);
	result.assign(msg->dpos(), msg->nbytes());
	
	delete msg;
	return type;
}


void XapiandClient::send_message(reply_type type, const std::string &message) {
	char type_as_char = static_cast<char>(type);
	std::string buf(&type_as_char, 1);
	buf += encode_length(message.size());
	buf += message;
	
	printf("send_message:");
	print_string(buf);
	
	pthread_mutex_lock(&qmtx);
	Buffer *buffer = new Buffer(type, buf.c_str(), buf.size());
	write_queue.push(buffer);
	pthread_mutex_unlock(&qmtx);
	
	async.send();
}


void XapiandClient::send_message(reply_type type, const std::string &message, double end_time)
{
	send_message(type, message);
}


Xapian::Database * XapiandClient::get_db(bool writable_)
{
	if (endpoints.empty()) {
		return NULL;
	}
	if (!database_pool->checkout(&database, endpoints, writable_)) {
		return NULL;
	}
	return database->db;
}


void XapiandClient::release_db(Xapian::Database *db_)
{
	if (database) {
		database_pool->checkin(&database);
	}
}


void XapiandClient::select_db(const std::vector<std::string> &dbpaths_, bool writable_)
{
	std::vector<std::string>::const_iterator i(dbpaths_.begin());
	for (; i != dbpaths_.end(); i++) {
		Endpoint endpoint = Endpoint(*i, std::string(), 8890);
		endpoints.push_back(endpoint);
	}
	dbpaths = dbpaths_;
}


void XapiandClient::run()
{
	try {
		run_one();
	} catch (const Xapian::NetworkError &e) {
		printf("ERROR: %s\n", e.get_msg().c_str());
	} catch (...) {
		printf("ERROR!\n");
	}
}


XapiandClient::XapiandClient(int sock_, ThreadPool *thread_pool_, DatabasePool *database_pool_, double active_timeout_, double idle_timeout_)
	: RemoteProtocol(std::vector<std::string>(), active_timeout_, idle_timeout_, true),
	  sock(sock_),
	  thread_pool(thread_pool_),
	  database_pool(database_pool_),
	  write_queue(WRITE_QUEUE_SIZE),
	  database(NULL)
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

	try {
		msg_update(std::string());
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

	double active_timeout = MSECS_ACTIVE_TIMEOUT_DEFAULT * 1e-3;
	double idle_timeout = MSECS_IDLE_TIMEOUT_DEFAULT * 1e-3;
	new XapiandClient(client_sock, &thread_pool, &database_pool, active_timeout, idle_timeout);
}


void XapiandServer::signal_cb(ev::sig &signal, int revents)
{
	signal.loop.break_loop();
}


XapiandServer::XapiandServer(int port, int thread_pool_size)
	: thread_pool(thread_pool_size)
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
