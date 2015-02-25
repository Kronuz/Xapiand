/*
c++ xapiand.cc server.cc threadpool.cc ../../net/length.cc -lev `xapian-config-1.3 --libs` `xapian-config-1.3 --cxxflags` -I../../ -I../../common -DXAPIAN_LIB_BUILD -oxapiand
*/

#include <stdlib.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "config.h"
#include "server.h"


int bind_http(int http_port)
{
	int optval = 1;
	struct sockaddr_in addr;
	
	int http_sock = socket(PF_INET, SOCK_STREAM, 0);
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
	}

	return http_sock;
}


int bind_binary(int binary_port)
{
	int optval = 1;
	struct sockaddr_in addr;
	
	int binary_sock = socket(PF_INET, SOCK_STREAM, 0);
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
	
	int http_sock = bind_http(http_port);
	int binary_sock = bind_binary(binary_port);

	int tasks = 8;

	if (http_sock && binary_sock) {
		ThreadPool thread_pool(tasks);
		for (int i = 0; i < tasks; i++) {
			thread_pool.addTask(new XapiandServer(http_sock, binary_sock));
		}

		printf("Starting...\n");
		ev::default_loop loop;
		loop.run();

		printf("Joining...\n");

		thread_pool.finish();
		thread_pool.join();
		
		printf("Done!\n");
	}

	if (http_sock) {
		shutdown(http_sock, SHUT_RDWR);
		close(http_sock);
	}

	if (binary_sock) {
		shutdown(binary_sock, SHUT_RDWR);
		close(binary_sock);
	}
	
	printf("Done with all work!\n");

	return 0;
}
