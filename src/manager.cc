/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "manager.h"

#include "utils.h"
#include "server.h"

#include <list>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <sys/socket.h>
#include <unistd.h>


#define TIME_RE "((([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)?"

pcre *XapiandManager::compiled_time_re = NULL;


XapiandManager::XapiandManager(ev::loop_ref *loop_, int http_port_, int binary_port_)
	: loop(loop_ ? loop_: &dynamic_loop),
	  break_loop(*loop),
	  shutdown_asap(0),
	  shutdown_now(0),
	  async_shutdown(*loop),
	  thread_pool("W%d", 10),
	  http_port(http_port_),
	  binary_port(binary_port_)
{
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutexattr_init(&servers_mutex_attr);
	pthread_mutexattr_settype(&servers_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&servers_mutex, &servers_mutex_attr);

	break_loop.set<XapiandManager, &XapiandManager::break_loop_cb>(this);
	break_loop.start();

	async_shutdown.set<XapiandManager, &XapiandManager::shutdown_cb>(this);
	async_shutdown.start();

	bind_http();
#ifdef HAVE_REMOTE_PROTOCOL
	bind_binary();
#endif  /* HAVE_REMOTE_PROTOCOL */

	assert(http_sock != -1 && binary_sock != -1);
	LOG_OBJ(this, "CREATED MANAGER!\n");
}


XapiandManager::~XapiandManager()
{
	destroy();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_mutex_destroy(&servers_mutex);
	pthread_mutexattr_destroy(&servers_mutex_attr);

	LOG_OBJ(this, "DELETED MANAGER!\n");
}


void XapiandManager::check_tcp_backlog(int tcp_backlog)
{
#if defined(NET_CORE_SOMAXCONN)
	int name[3] = {CTL_NET, NET_CORE, NET_CORE_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(this, "ERROR: sysctl: %s\n", strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(this, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"net.core.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#elif defined(KIPC_SOMAXCONN)
	int name[3] = {CTL_KERN, KERN_IPC, KIPC_SOMAXCONN};
	int somaxconn;
	size_t somaxconn_len = sizeof(somaxconn);
	if (sysctl(name, 3, &somaxconn, &somaxconn_len, 0, 0) < 0) {
		LOG_ERR(this, "ERROR: sysctl: %s\n", strerror(errno));
		return;
	}
	if (somaxconn > 0 && somaxconn < tcp_backlog) {
		LOG_ERR(this, "WARNING: The TCP backlog setting of %d cannot be enforced because "
				"kern.ipc.somaxconn"
				" is set to the lower value of %d.\n", tcp_backlog, somaxconn);
	}
#endif
}


void XapiandManager::bind_http()
{
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int error;
	int optval = 1;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};

	http_sock = socket(PF_INET, SOCK_STREAM, 0);

	setsockopt(http_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
#ifdef SO_NOSIGPIPE
	setsockopt(http_sock, SOL_SOCKET, SO_NOSIGPIPE, (char *)&optval, sizeof(optval));
#endif
	error = setsockopt(http_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));

	error = setsockopt(http_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));

	error = setsockopt(http_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", http_sock, strerror(errno));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(http_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(http_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		LOG_ERR(this, "ERROR: http bind error (sock=%d): %s\n", http_sock, strerror(errno));
		::close(http_sock);
		http_sock = -1;
	} else {
		fcntl(http_sock, F_SETFL, fcntl(http_sock, F_GETFL, 0) | O_NONBLOCK);

		check_tcp_backlog(tcp_backlog);
		listen(http_sock, tcp_backlog);
	}
}


#ifdef HAVE_REMOTE_PROTOCOL
void XapiandManager::bind_binary()
{
	int tcp_backlog = XAPIAND_TCP_BACKLOG;
	int error;
	int optval = 1;
	struct sockaddr_in addr;
	struct linger ling = {0, 0};

	binary_sock = socket(PF_INET, SOCK_STREAM, 0);

	setsockopt(binary_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));
#ifdef SO_NOSIGPIPE
	setsockopt(binary_sock, SOL_SOCKET, SO_NOSIGPIPE, (char *)&optval, sizeof(optval));
#endif
	error = setsockopt(binary_sock, SOL_SOCKET, SO_KEEPALIVE, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));

	error = setsockopt(binary_sock, SOL_SOCKET, SO_LINGER, (void *)&ling, sizeof(ling));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));

	error = setsockopt(binary_sock, IPPROTO_TCP, TCP_NODELAY, (void *)&optval, sizeof(optval));
	if (error != 0)
		LOG_ERR(this, "ERROR: setsockopt (sock=%d): %s\n", binary_sock, strerror(errno));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(binary_port);
	addr.sin_addr.s_addr = INADDR_ANY;

	if (bind(binary_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
		LOG_ERR(this, "ERROR: binary bind error (sock=%d): %s\n", binary_sock, strerror(errno));
		::close(binary_sock);
		binary_sock = -1;
	} else {
		fcntl(binary_sock, F_SETFL, fcntl(binary_sock, F_GETFL, 0) | O_NONBLOCK);

		check_tcp_backlog(tcp_backlog);
		listen(binary_sock, tcp_backlog);
	}
}
#endif  /* HAVE_REMOTE_PROTOCOL */


void XapiandManager::sig_shutdown_handler(int sig)
{
	/* SIGINT is often delivered via Ctrl+C in an interactive session.
	 * If we receive the signal the second time, we interpret this as
	 * the user really wanting to quit ASAP without waiting to persist
	 * on disk. */
	time_t now = time(NULL);
	if (shutdown_now && sig != SIGTERM) {
		if (sig && shutdown_now + 1 < now) {
			INFO(this, "You insist... exiting now.\n");
			// remove pid file here, use: getpid();
			exit(1); /* Exit with an error since this was not a clean shutdown. */
		}
	} else if (shutdown_asap && sig != SIGTERM) {
		if (shutdown_asap + 1 < now) {
			shutdown_now = now;
			INFO(this, "Trying immediate shutdown.\n");
		}
	} else {
		shutdown_asap = now;
		switch (sig) {
			case SIGINT:
				INFO(this, "Received SIGINT scheduling shutdown...\n");
				break;
			case SIGTERM:
				INFO(this, "Received SIGTERM scheduling shutdown...\n");
				break;
			default:
				INFO(this, "Received shutdown signal, scheduling shutdown...\n");
		};
	}
	shutdown();
}


void XapiandManager::destroy()
{
	pthread_mutex_lock(&qmtx);
	if (http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	if (http_sock != -1) {
		::close(http_sock);
		http_sock = -1;
	}

	if (binary_sock != -1) {
		::close(binary_sock);
		binary_sock = -1;
	}

	pthread_mutex_unlock(&qmtx);

	LOG_OBJ(this, "DESTROYED MANAGER!\n");
}


void XapiandManager::shutdown_cb(ev::async &watcher, int revents)
{
	sig_shutdown_handler(0);
}


std::list<XapiandServer *>::const_iterator XapiandManager::attach_server(XapiandServer *server)
{
	pthread_mutex_lock(&servers_mutex);
	std::list<XapiandServer *>::const_iterator iterator = servers.insert(servers.end(), server);
	pthread_mutex_unlock(&servers_mutex);
	return iterator;
}


void XapiandManager::detach_server(XapiandServer *server)
{
	pthread_mutex_lock(&servers_mutex);
	if (server->iterator != servers.end()) {
		servers.erase(server->iterator);
		server->iterator = servers.end();
		LOG_OBJ(this, "DETACHED SERVER!\n");
	}
	pthread_mutex_unlock(&servers_mutex);
}


void XapiandManager::break_loop_cb(ev::async &watcher, int revents)
{
	LOG_OBJ(this, "Breaking manager loop!\n");
	loop->break_loop();
}


void XapiandManager::shutdown()
{
	pthread_mutex_lock(&servers_mutex);
	std::list<XapiandServer *>::const_iterator it(servers.begin());
	while (it != servers.end()) {
		XapiandServer *server = *(it++);
		server->shutdown();
	}
	pthread_mutex_unlock(&servers_mutex);

	if (shutdown_asap) {
		destroy();
		LOG_OBJ(this, "Finishing thread pool!\n");
		thread_pool.finish();
	}
	if (shutdown_now) {
		break_loop.send();
	}
}


//
//void XapiandManager::run()
//{
//	XapiandServer * server = new XapiandServer(this, loop, http_sock, binary_sock, &database_pool, &thread_pool);
//	server->run(NULL);
//	delete server;
//}


void XapiandManager::run(int num_servers)
{
#ifdef HAVE_REMOTE_PROTOCOL
	INFO(this, "Listening on %d (http), %d (xapian)...\n", http_port, binary_port);
#else
	INFO(this, "Listening on %d (http)...\n", http_port);
#endif  /* HAVE_REMOTE_PROTOCOL */

	ThreadPool server_pool("S%d", num_servers);
	for (int i = 0; i < num_servers; i++) {
		XapiandServer *server = new XapiandServer(this, NULL, http_sock, binary_sock, &database_pool, &thread_pool);
		server_pool.addTask(server);
	}

	LOG_OBJ(this, "Starting manager loop...\n");
	loop->run();
	LOG_OBJ(this, "Manager loop ended!\n");

	LOG_OBJ(this, "Waiting for threads...\n");

	server_pool.finish();
	server_pool.join();

	LOG_OBJ(this, "Server ended!\n");
}


cJSON* XapiandManager::server_status()
{
	cJSON *root_status = cJSON_CreateObject();
	std::string contet_ser;
	pthread_mutex_lock(&XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status, "Connections", XapiandServer::total_clients);
	cJSON_AddNumberToObject(root_status, "Http connections", XapiandServer::http_clients);
	cJSON_AddNumberToObject(root_status, "Xapian remote connections", XapiandServer::binary_clients);
	pthread_mutex_unlock(&XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status, "Size thread pool", thread_pool.length());
	return root_status;
}


cJSON* XapiandManager::get_stats_time(const std::string &time_req)
{
	cJSON *root_stats = cJSON_CreateObject();
	pos_time_t first_time, second_time;
	int len = (int) time_req.size();
	group_t *g = NULL;
	int ret = pcre_search(time_req.c_str(), len, 0, 0, TIME_RE, &compiled_time_re, &g);
	if (ret == 0 && (g[0].end - g[0].start) == len) {
		if ((g[1].end - g[1].start) > 0) {
			first_time.minute = 60 * (((g[3].end - g[3].start) > 0) ? strtoint(std::string(time_req.c_str() + g[3].start, g[3].end - g[3].start)) : 0);
			first_time.minute += ((g[4].end - g[4].start) > 0) ? strtoint(std::string(time_req.c_str() + g[4].start, g[4].end - g[4].start -1)) : 0;
			first_time.second = ((g[5].end - g[5].start) > 0) ? strtoint(std::string(time_req.c_str() + g[5].start, g[5].end - g[5].start -1)) : 0;
			if ((g[6].end - g[6].start) > 0) {
				second_time.minute = 60 * (((g[8].end - g[8].start) > 0) ? strtoint(std::string(time_req.c_str() + g[8].start, g[8].end - g[8].start)) : 0);
				second_time.minute += ((g[9].end - g[9].start) > 0) ? strtoint(std::string(time_req.c_str() + g[9].start, g[9].end - g[9].start -1)) : 0;
				second_time.second = ((g[10].end - g[10].start) > 0) ? strtoint(std::string(time_req.c_str() + g[10].start, g[10].end - g[10].start -1)) : 0;
			} else {
				second_time.minute = 0;
				second_time.second = 0;
			}
			if (g) {
				free(g);
				g = NULL;
			}
			return get_stats_json(first_time, second_time);
		} else {
			cJSON_AddStringToObject(root_stats, "Error in time argument input", "Incorrect input.");
			return root_stats;
		}
	}

	cJSON_AddStringToObject(root_stats, "Error in time argument input", "Incorrect input.");
	return root_stats;
}


cJSON* XapiandManager::get_stats_json(pos_time_t first_time, pos_time_t second_time) 
{
	cJSON *root_stats = cJSON_CreateObject();
	cJSON *time_period = cJSON_CreateObject();

	pthread_mutex_lock(&XapiandServer::static_mutex);
	update_pos_time();
	time_t now_time = init_time;
	pos_time_t b_time_cpy = b_time;
	times_row_t stats_cnt_cpy = stats_cnt;
	pthread_mutex_unlock(&XapiandServer::static_mutex);

	int aux_second_sec;
	int aux_first_sec;
	int aux_second_min;
	int aux_first_min;
	bool seconds = (first_time.minute == 0) ? true : false;

	if (second_time.minute == 0 && second_time.second == 0) {
		aux_second_sec =  first_time.second;
		aux_first_sec = 0;
		aux_second_min =  first_time.minute;
		aux_first_min = 0;
		second_time.minute = b_time_cpy.minute - first_time.minute;
		second_time.second = b_time_cpy.second - first_time.second;
		first_time.minute = b_time_cpy.minute;
		first_time.second = b_time_cpy.second;
	} else {
		aux_second_sec =  second_time.second;
		aux_first_sec = first_time.second;
		aux_second_min =  second_time.minute;
		aux_first_min = first_time.minute;
		first_time.minute = b_time_cpy.minute - first_time.minute;
		first_time.second = b_time_cpy.second - first_time.second;
		second_time.minute = b_time_cpy.minute - second_time.minute;
		second_time.second = b_time_cpy.second - second_time.second;
	}

	if ((aux_first_min * SLOT_TIME_SECOND + aux_first_sec) > (aux_second_min * SLOT_TIME_SECOND + aux_second_sec)) {
		cJSON_AddStringToObject(root_stats, "Error in time argument input", "First argument must be less or equal than the second.");
	} else {
		int cnt[3] = {0, 0, 0};
		double tm_cnt[3] = {0.0, 0.0, 0.0};
		struct tm *timeinfo = localtime(&now_time);
		cJSON_AddStringToObject(time_period, "System time", asctime(timeinfo));
		if (seconds) {
			for (int i = second_time.second; i <= first_time.second; i++) {
				int j = (i < 0) ? SLOT_TIME_SECOND + i : i;
				cnt[0] += stats_cnt_cpy.index.sec[j];
				cnt[1] += stats_cnt_cpy.search.sec[j];
				cnt[2] += stats_cnt_cpy.del.sec[j];
				tm_cnt[0] += stats_cnt_cpy.index.tm_sec[j];
				tm_cnt[1] += stats_cnt_cpy.search.tm_sec[j];
				tm_cnt[2] += stats_cnt_cpy.del.tm_sec[j];
			}
			time_t p_time = now_time - aux_second_sec;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period start", asctime(timeinfo));
			p_time = now_time - aux_first_sec;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period end", asctime(timeinfo));
		} else {
			for (int i = second_time.minute; i <= first_time.minute; i++) {
				int j = (i < 0) ? SLOT_TIME_MINUTE + i : i;
				cnt[0] += stats_cnt_cpy.index.cnt[j];
				cnt[1] += stats_cnt_cpy.search.cnt[j];
				cnt[2] += stats_cnt_cpy.del.cnt[j];
				tm_cnt[0] += stats_cnt_cpy.index.tm_cnt[j];
				tm_cnt[1] += stats_cnt_cpy.search.tm_cnt[j];
				tm_cnt[2] += stats_cnt_cpy.del.tm_cnt[j];
			}
			time_t p_time = now_time - aux_second_min * SLOT_TIME_SECOND;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period start", asctime(timeinfo));
			p_time = now_time - aux_first_min * SLOT_TIME_SECOND;
			timeinfo = localtime(&p_time);
			cJSON_AddStringToObject(time_period, "Period end", asctime(timeinfo));
		}
		cJSON_AddItemToObject(root_stats, "Time", time_period);
		cJSON_AddNumberToObject(root_stats, "Docs index", cnt[0]);
		cJSON_AddNumberToObject(root_stats, "Number searches", cnt[1]);
		cJSON_AddNumberToObject(root_stats, "Docs deleted", cnt[2]);
		cJSON_AddNumberToObject(root_stats, "Average time indexing", tm_cnt[0] / ((cnt[0] == 0) ? 1 : cnt[0]));
		cJSON_AddNumberToObject(root_stats, "Average search time", tm_cnt[1] / ((cnt[1] == 0) ? 1 : cnt[1]));
		cJSON_AddNumberToObject(root_stats, "Average deletion time", tm_cnt[2] / ((cnt[2] == 0) ? 1 : cnt[2]));
	}

	return root_stats;
}