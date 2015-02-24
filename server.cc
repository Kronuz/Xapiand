#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <xapian.h>

#include "server.h"
#include "client_http.h"
#include "client_binary.h"


const int MSECS_IDLE_TIMEOUT_DEFAULT = 60000;
const int MSECS_ACTIVE_TIMEOUT_DEFAULT = 15000;


//
// Xapian Server
//

XapiandServer::XapiandServer(int http_port_, int binary_port_, int thread_pool_size)
	: http_port(http_port_),
	  binary_port(binary_port_),
	  thread_pool(thread_pool_size)
{
	bind_http();
	bind_binary();

	sig.set<&XapiandServer::signal_cb>();
	sig.start(SIGINT);
}


void
XapiandServer::bind_http() {
	int optval = 1;
	struct sockaddr_in addr;

	http_sock = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(http_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(http_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(http_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		perror("bind");
		close(http_sock);
		http_sock = 0;
	} else {
		printf("Listening http protocol on port %d\n", http_port);
		fcntl(http_sock, F_SETFL, fcntl(http_sock, F_GETFL, 0) | O_NONBLOCK);

		listen(http_sock, 5);

		http_io.set<XapiandServer, &XapiandServer::io_accept_http>(this);
		http_io.start(http_sock, ev::READ);
	}
}


void
XapiandServer::bind_binary() {
	int optval = 1;
	struct sockaddr_in addr;

	binary_sock = socket(PF_INET, SOCK_STREAM, 0);
	setsockopt(binary_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(binary_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(binary_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		perror("bind");
		close(binary_sock);
		binary_sock = 0;
	} else {
		printf("Listening binary protocol on port %d\n", binary_port);
		fcntl(binary_sock, F_SETFL, fcntl(binary_sock, F_GETFL, 0) | O_NONBLOCK);

		listen(binary_sock, 5);

		binary_io.set<XapiandServer, &XapiandServer::io_accept_binary>(this);
		binary_io.start(binary_sock, ev::READ);
	}
}


XapiandServer::~XapiandServer()
{
	shutdown(http_sock, SHUT_RDWR);
	shutdown(binary_sock, SHUT_RDWR);

	sig.stop();
	http_io.stop();
	binary_io.stop();

	close(http_sock);
	close(binary_sock);

	printf("Done with all work!\n");
}


void XapiandServer::io_accept_http(ev::io &watcher, int revents)
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
	new HttpClient(client_sock, &thread_pool, &database_pool, active_timeout, idle_timeout);
}


void XapiandServer::io_accept_binary(ev::io &watcher, int revents)
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
	new BinaryClient(client_sock, &thread_pool, &database_pool, active_timeout, idle_timeout);
}


void XapiandServer::signal_cb(ev::sig &signal, int revents)
{
	signal.loop.break_loop();
}
