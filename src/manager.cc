/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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
#include "length.h"

#include <list>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <net/if.h> /* for IFF_LOOPBACK */
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <unistd.h>


#define TIME_RE "((([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)?"


const uint16_t XAPIAND_DISCOVERY_PROTOCOL_VERSION = XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION | XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION << 8;

pcre *XapiandManager::compiled_time_re = NULL;


std::string Node::serialise()
{
	std::string node_str;
	if (!name.empty()) {
		node_str.append(encode_length(addr.sin_addr.s_addr));
		node_str.append(encode_length(http_port));
		node_str.append(encode_length(binary_port));
		node_str.append(serialise_string(name));
	}
	return node_str;
}


size_t Node::unserialise(const char **p, const char *end)
{
	size_t length;
	const char *ptr = *p;

	if ((length = decode_length(&ptr, end, false)) == -1) {
		return -1;
	}
	addr.sin_addr.s_addr = (int)length;

	if ((length = decode_length(&ptr, end, false)) == -1) {
		return -1;
	}
	http_port = (int)length;

	if ((length = decode_length(&ptr, end, false)) == -1) {
		return -1;
	}
	binary_port = (int)length;

	name.clear();
	if (unserialise_string(name, &ptr, end) == -1 || name.empty()) {
		return -1;
	}

	*p = ptr;
	return end - ptr;
}

size_t Node::unserialise(const std::string &s)
{
	const char *ptr = s.data();
	return unserialise(&ptr, ptr + s.size());
}


XapiandManager::XapiandManager(ev::loop_ref *loop_, const std::string &cluster_name_, const std::string &node_name_, const std::string &discovery_group_, int discovery_port_, int http_port_, int binary_port_, size_t dbpool_size)
	: loop(loop_ ? loop_: &dynamic_loop),
	  state(STATE_RESET),
	  break_loop(*loop),
	  discovery_heartbeat(*loop),
	  database_pool(dbpool_size),
	  shutdown_asap(0),
	  shutdown_now(0),
	  async_shutdown(*loop),
	  thread_pool("W%d", 10),
	  cluster_name(cluster_name_),
	  node_name(node_name_),
	  discovery_port(discovery_port_)
{

	this_node.http_port = http_port_;
	this_node.binary_port = binary_port_;

	struct sockaddr_in addr;

	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutexattr_init(&nodes_mtx_attr);
	pthread_mutexattr_settype(&nodes_mtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&nodes_mtx, &qmtx_attr);

	pthread_mutexattr_init(&servers_mutex_attr);
	pthread_mutexattr_settype(&servers_mutex_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&servers_mutex, &servers_mutex_attr);

	break_loop.set<XapiandManager, &XapiandManager::break_loop_cb>(this);
	break_loop.start();

	async_shutdown.set<XapiandManager, &XapiandManager::shutdown_cb>(this);
	async_shutdown.start();

	this_node.addr = host_address();

	if (discovery_port == 0) {
		discovery_port = XAPIAND_DISCOVERY_SERVERPORT;
	}
	bind_udp("discovery", discovery_sock, discovery_port, discovery_addr, 1, discovery_group_.c_str());

	int http_tries = 1;
	if (this_node.http_port == 0) {
		this_node.http_port = XAPIAND_HTTP_SERVERPORT;
		http_tries = 10;
	}
	bind_tcp("http", http_sock, this_node.http_port, addr, http_tries);

#ifdef HAVE_REMOTE_PROTOCOL
	int binary_tries = 1;
	if (this_node.binary_port == 0) {
		this_node.binary_port = XAPIAND_BINARY_SERVERPORT;
		binary_tries = 10;
	}
	bind_tcp("binary", binary_sock, this_node.binary_port, addr, binary_tries);
#endif  /* HAVE_REMOTE_PROTOCOL */

	assert(discovery_sock != -1 && http_sock != -1 && binary_sock != -1);

	discovery_heartbeat.set<XapiandManager, &XapiandManager::discovery_heartbeat_cb>(this);
	discovery_heartbeat.start(0, 1);

	LOG_OBJ(this, "CREATED MANAGER!\n");
}


XapiandManager::~XapiandManager()
{
	discovery(DISCOVERY_BYE, this_node.serialise());

	destroy();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	pthread_mutex_destroy(&servers_mutex);
	pthread_mutexattr_destroy(&servers_mutex_attr);

	LOG_OBJ(this, "DELETED MANAGER!\n");
}

void XapiandManager::reset_state()
{
	if (state != STATE_RESET) {
		state = STATE_RESET;
		discovery_heartbeat.set(0, 1);
	}
}


bool XapiandManager::put_node(Node &node)
{
	pthread_mutex_lock(&nodes_mtx);
	std::string node_name_lower = stringtolower(node.name);
	if (node_name_lower == stringtolower(this_node.name)) {
		this_node.touched = time(NULL);
		pthread_mutex_unlock(&nodes_mtx);
		return false;
	} else {
		try {
			Node &node_ref = nodes.at(node_name_lower);
			if (node == node_ref) {
				node_ref.touched = time(NULL);
			}
		} catch (const std::out_of_range& err) {
			Node &node_ref = nodes[node_name_lower];
			node_ref = node;
			node_ref.touched = time(NULL);
			pthread_mutex_unlock(&nodes_mtx);
			return true;
		} catch(...) {
			pthread_mutex_unlock(&nodes_mtx);
			throw;
		}
	}
	pthread_mutex_unlock(&nodes_mtx);
	return false;
}


bool XapiandManager::touch_node(std::string &node_name, Node *node)
{
	pthread_mutex_lock(&nodes_mtx);
	std::string node_name_lower = stringtolower(node_name);
	if (node_name_lower == stringtolower(this_node.name)) {
		this_node.touched = time(NULL);
		if (node) *node = this_node;
		pthread_mutex_unlock(&nodes_mtx);
		return true;
	} else {
		try {
			Node &node_ref = nodes.at(node_name_lower);
			node_ref.touched = time(NULL);
			if (node) *node = node_ref;
			pthread_mutex_unlock(&nodes_mtx);
			return true;
		} catch (const std::out_of_range& err) {
		} catch(...) {
			pthread_mutex_unlock(&nodes_mtx);
			throw;
		}
	}
	pthread_mutex_unlock(&nodes_mtx);
	return false;
}


void XapiandManager::drop_node(std::string &node_name)
{
	pthread_mutex_lock(&nodes_mtx);
	nodes.erase(stringtolower(node_name));
	pthread_mutex_unlock(&nodes_mtx);
}


struct sockaddr_in XapiandManager::host_address()
{
	struct sockaddr_in addr;
	struct ifaddrs *if_addr_struct;
	if (getifaddrs(&if_addr_struct) < 0) {
		LOG_ERR(this, "ERROR: getifaddrs: %s\n", strerror(errno));
	} else {
		for (struct ifaddrs *ifa = if_addr_struct; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET && !(ifa->ifa_flags & IFF_LOOPBACK)) { // check it is IP4
				char ip[INET_ADDRSTRLEN];
				addr = *(struct sockaddr_in *)ifa->ifa_addr;
				inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
				LOG_DISCOVERY(this, "Using %s, IP address = %s\n", ifa->ifa_name, ip);
				break;
			}
		}
		freeifaddrs(if_addr_struct);
	}
	return addr;
}


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
	if (discovery_sock == -1 && http_sock == -1 && binary_sock == -1) {
		pthread_mutex_unlock(&qmtx);
		return;
	}

	if (discovery_sock != -1) {
		::close(discovery_sock);
		discovery_sock = -1;
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


void XapiandManager::discovery_heartbeat_cb(ev::timer &watcher, int revents)
{
	switch (state) {
		case STATE_RESET:
			if (!this_node.name.empty()) {
				drop_node(this_node.name);
			}
			if (node_name.empty()) {
				this_node.name = name_generator();
			} else {
				this_node.name = node_name;
			}
		case STATE_WAITING:
			discovery(DISCOVERY_HELLO, this_node.serialise());
			break;

		case STATE_READY:
			discovery(DISCOVERY_PING, serialise_string(this_node.name));
			break;
	}
	if (state && state-- == 1) {
		discovery_heartbeat.set(0, 20);
	}
}


void XapiandManager::discovery(const char *buf, size_t buf_size)
{
	pthread_mutex_lock(&qmtx);
	if (discovery_sock != -1) {
		LOG_DISCOVERY_WIRE(this, "(sock=%d) <<-- '%s'\n", discovery_sock, repr(buf, buf_size).c_str());

#ifdef MSG_NOSIGNAL
		ssize_t written = ::sendto(discovery_sock, buf, buf_size, MSG_NOSIGNAL, (struct sockaddr *)&discovery_addr, sizeof(discovery_addr));
#else
		ssize_t written = ::sendto(discovery_sock, buf, buf_size, 0, (struct sockaddr *)&discovery_addr, sizeof(discovery_addr));
#endif

		if (written < 0) {
			if (errno != EAGAIN && discovery_sock != -1) {
				LOG_ERR(this, "ERROR: sendto error (sock=%d): %s\n", discovery_sock, strerror(errno));
				destroy();
			}
		} else if (written == 0) {
			// nothing written?
		} else {

		}
	}
	pthread_mutex_unlock(&qmtx);
}


void XapiandManager::discovery(discovery_type type, const std::string &content)
{
	if (!content.empty()) {
		std::string message((const char *)&type, 1);
		message.append(std::string((const char *)&XAPIAND_DISCOVERY_PROTOCOL_VERSION, sizeof(uint16_t)));
		message.append(serialise_string(cluster_name));
		message.append(content);
		discovery(message.c_str(), message.size());
	}
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
		discovery(DISCOVERY_BYE, this_node.serialise());
		destroy();
		LOG_OBJ(this, "Finishing thread pool!\n");
		thread_pool.finish();
	}
	if (shutdown_now) {
		break_loop.send();
	}
}


void XapiandManager::run(int num_servers)
{
	std::string msg("Listening on ");
	if (this_node.http_port != -1) {
		msg += "tcp:" + std::to_string(this_node.http_port) + " (http), ";
	}
	if (this_node.binary_port != -1) {
		msg += "tcp:" + std::to_string(this_node.binary_port) + " (xapian v" + std::to_string(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION) + "), ";
	}
	if (discovery_port != -1) {
		msg += "udp:" + std::to_string(discovery_port) + " (discovery v" + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION) + "), ";
	}
	msg += "at pid:" + std::to_string(getpid()) + "...\n";

	INFO(this, msg.c_str());

	INFO(this, "Starting %d server worker thread%s.\n", num_servers, (num_servers == 1) ? "" : "s");

	ThreadPool server_pool("S%d", num_servers);
	for (int i = 0; i < num_servers; i++) {
		XapiandServer *server = new XapiandServer(this, NULL, discovery_sock, http_sock, binary_sock, &database_pool, &thread_pool);
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