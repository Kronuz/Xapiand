/*
c++ xapiand.cc server.cc threadpool.cc ../../net/length.cc -lev `xapian-config-1.3 --libs` `xapian-config-1.3 --cxxflags` -I../../ -I../../common -DXAPIAN_LIB_BUILD -oxapiand
*/

#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <sys/socket.h>

#include "utils.h"
#include "config.h"
#include "server.h"


int bind_http(int http_port)
{
	int error;
	int optval = 1;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};
	
	int http_sock = socket(PF_INET, SOCK_STREAM, 0);

	setsockopt(http_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	error = setsockopt(http_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_CONN((void *)NULL, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));
	
	error = setsockopt(http_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (error != 0)
		LOG_CONN((void *)NULL, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));

	error = setsockopt(http_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_CONN((void *)NULL, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(http_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(http_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		LOG_CONN((void *)NULL, "ERROR: http bind error (sock=%d): %s\n", http_sock, strerror(errno));
		close(http_sock);
		http_sock = -1;
	} else {
		LOG_CONN((void *)NULL, "Listening http protocol on port %d\n", http_port);
		fcntl(http_sock, F_SETFL, fcntl(http_sock, F_GETFL, 0) | O_NONBLOCK);
		
		listen(http_sock, 5);
	}

	return http_sock;
}


int bind_binary(int binary_port)
{
	int error;
	int optval = 1;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};

	int binary_sock = socket(PF_INET, SOCK_STREAM, 0);

	setsockopt(binary_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
	error = setsockopt(binary_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_CONN((void *)NULL, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));
	
	error = setsockopt(binary_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (error != 0)
		LOG_CONN((void *)NULL, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));
	
	error = setsockopt(binary_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_CONN((void *)NULL, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(binary_port);
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(binary_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		LOG_CONN((void *)NULL, "ERROR: binary bind error (sock=%d): %s\n", binary_sock, strerror(errno));
		close(binary_sock);
		binary_sock = -1;
	} else {
		LOG_CONN((void *)NULL, "Listening binary protocol on port %d\n", binary_port);
		fcntl(binary_sock, F_SETFL, fcntl(binary_sock, F_GETFL, 0) | O_NONBLOCK);
		
		listen(binary_sock, 5);
	}

	return binary_sock;
}



int main(int argc, char **argv)
{
	int http_port = XAPIAND_HTTP_PORT_DEFAULT;
	int binary_port = XAPIAND_BINARY_PORT_DEFAULT;

	if (argc > 2) {
		http_port = atoi(argv[1]);
		binary_port = atoi(argv[2]);
	}

	LOG((void *)NULL, "Starting Xapiand.\n");

	int http_sock = bind_http(http_port);
	int binary_sock = bind_binary(binary_port);

	int tasks = 0;

	if (http_sock != -1 && binary_sock != -1) {
		ev::default_loop loop;
		if (tasks) {
			ThreadPool *thread_pool = new ThreadPool(tasks);

			for (int i = 0; i < tasks; i++) {
				XapiandServer * server = new XapiandServer(NULL, http_sock, binary_sock);
				thread_pool->addTask(server);
			}
			
			loop.run();
			
			LOG_OBJ((void *)NULL, "Waiting for threads...\n");
			
			thread_pool->finish();
			thread_pool->join();

			delete thread_pool;
		} else {
			XapiandServer * server = new XapiandServer(&loop, http_sock, binary_sock);
			server->run();
			delete server;
		}
	}

	if (http_sock != -1) {
		close(http_sock);
	}

	if (binary_sock != -1) {
		close(binary_sock);
	}
	
	LOG((void *)NULL, "Done with all work!\n");

	return 0;
}
