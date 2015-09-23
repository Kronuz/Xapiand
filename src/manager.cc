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
#include "replicator.h"
#include "length.h"
#include "endpoint.h"
#include "client_binary.h"
#include "server_http.h"
#include "server_binary.h"
#include "server_discovery.h"

#include <list>
#include <stdlib.h>
#include <assert.h>
#include <sys/sysctl.h>
#include <fcntl.h>
#include <net/if.h> /* for IFF_LOOPBACK */
#include <ifaddrs.h>
#include <unistd.h>

#define TIME_RE "((([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)(\\.\\.(([01]?[0-9]|2[0-3])h)?([0-5]?[0-9]m)?([0-5]?[0-9]s)?)?"

#define REGIONS_NUMBER 1

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


ssize_t Node::unserialise(const char **p, const char *end)
{
	ssize_t length;
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


ssize_t Node::unserialise(const std::string &s)
{
	const char *ptr = s.data();
	return unserialise(&ptr, ptr + s.size());
}


XapiandManager::XapiandManager(ev::loop_ref *loop_, const opts_t &o)
	: Worker(NULL, loop_),
	  discovery_heartbeat(*loop),
	  discovery_port(o.discovery_port),
	  database_pool(o.dbpool_size),
	  thread_pool("W%d", (int)o.threadpool_size),
	  shutdown_asap(0),
	  shutdown_now(0),
	  async_shutdown(*loop),
	  endp_r(o.endpoints_list_size),
	  state(STATE_RESET),
	  cluster_name(o.cluster_name),
	  node_name(o.node_name)
{
	pthread_mutexattr_init(&qmtx_attr);
	pthread_mutexattr_settype(&qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&qmtx, &qmtx_attr);

	pthread_mutexattr_init(&nodes_mtx_attr);
	pthread_mutexattr_settype(&nodes_mtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&nodes_mtx, &nodes_mtx_attr);

	// Setup node from node database directory
	std::string node_name_(get_node_name());
	if (!node_name_.empty()) {
		if (!node_name.empty() && stringtolower(node_name) != stringtolower(node_name_)) {
			LOG_ERR(this, "Node name %s doesn't match with the one in the cluster's database: %s!\n", node_name.c_str(), node_name_.c_str());
			assert(false);
		}
		node_name = node_name_;
	}

	// Set the id in local node.
	local_node.id = get_node_id();

	// Bind tcp:HTTP, tcp:xapian-binary and udp:discovery ports
	local_node.http_port = o.http_port;
	local_node.binary_port = o.binary_port;

	struct sockaddr_in addr;

	local_node.addr = host_address();

	if (discovery_port == 0) {
		discovery_port = XAPIAND_DISCOVERY_SERVERPORT;
	}
	discovery_sock = bind_udp("discovery", discovery_port, discovery_addr, 1, o.discovery_group.c_str());

	int http_tries = 1;
	if (local_node.http_port == 0) {
		local_node.http_port = XAPIAND_HTTP_SERVERPORT;
		http_tries = 10;
	}
	http_sock = bind_tcp("http", local_node.http_port, addr, http_tries);


	int binary_tries = 1;
	if (local_node.binary_port == 0) {
		local_node.binary_port = XAPIAND_BINARY_SERVERPORT;
		binary_tries = 10;
	}
#ifdef HAVE_REMOTE_PROTOCOL
	binary_sock = bind_tcp("binary", local_node.binary_port, addr, binary_tries);
#endif  /* HAVE_REMOTE_PROTOCOL */

	if (discovery_sock == -1 || http_sock == -1 || binary_sock == -1) {
		LOG_ERR(this, "Cannot bind to sockets!\n");
		assert(false);
	}

	async_shutdown.set<XapiandManager, &XapiandManager::shutdown_cb>(this);
	async_shutdown.start();
	LOG_EV(this, "\tStart async shutdown event\n");

	discovery_heartbeat.set<XapiandManager, &XapiandManager::discovery_heartbeat_cb>(this);
	discovery_heartbeat.set(0, HEARTBEAT_INIT);
	LOG_EV(this, "\tSet heartbeat event\n");

	LOG_OBJ(this, "CREATED MANAGER!\n");
}


XapiandManager::~XapiandManager()
{
	discovery(DISCOVERY_BYE, local_node.serialise());

	destroy();

	pthread_mutex_destroy(&qmtx);
	pthread_mutexattr_destroy(&qmtx_attr);

	LOG_OBJ(this, "DELETED MANAGER!\n");
}


std::string
XapiandManager::get_node_name()
{
	size_t length = 0;
	unsigned char buf[512];
	int fd = open("nodename", O_RDONLY | O_CLOEXEC);
	if (fd >= 0) {
		length = read(fd, (char *)buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			for (size_t i = 0, j = 0; (buf[j] = buf[i]); j += !isspace(buf[i++]));
		}
		close(fd);
	}
	return std::string((const char *)buf, length);
}


bool
XapiandManager::set_node_name(const std::string &node_name_)
{
	if (node_name_.empty()) {
		pthread_mutex_unlock(&qmtx);
		return false;
	}

	node_name = get_node_name();
	if (!node_name.empty() && stringtolower(node_name) != stringtolower(node_name_)) {
		pthread_mutex_unlock(&qmtx);
		return false;
	}

	if (stringtolower(node_name) != stringtolower(node_name_)) {
		node_name = node_name_;

		int fd = open("nodename", O_WRONLY | O_CREAT, 0644);
		if (fd >= 0) {
			if (write(fd, node_name.c_str(), node_name.size()) != static_cast<ssize_t>(node_name.size())) {
				assert(false);
			}
			close(fd);
		} else {
			assert(false);
		}
	}

	INFO(this, "Node %s accepted to the party!\n", node_name.c_str());
	return true;
}


uint64_t
XapiandManager::get_node_id()
{
	return random_int(0, UINT64_MAX);
}


bool
XapiandManager::set_node_id()
{
	if (random_real(0, 1.0) < 0.5) return false;

	local_node.id = get_node_id();

	return true;
}


void
XapiandManager::setup_node(XapiandServer *server)
{
	int new_cluster = 0;

	pthread_mutex_lock(&qmtx);

	// Open cluster database
	Database *cluster_database = NULL;
	cluster_endpoints.clear();
	Endpoint cluster_endpoint(".");
	cluster_endpoints.insert(cluster_endpoint);
	if (!database_pool.checkout(&cluster_database, cluster_endpoints, DB_WRITABLE | DB_PERSISTENT)) {
		new_cluster = 1;
		INFO(this, "Cluster database doesn't exist. Generating database...\n");
		if (!database_pool.checkout(&cluster_database, cluster_endpoints, DB_WRITABLE | DB_SPAWN | DB_PERSISTENT)) {
			assert(false);
		}
	}
	database_pool.checkin(&cluster_database);

	// Get a node (any node)
	pthread_mutex_lock(&nodes_mtx);

	nodes_map_t::const_iterator it(nodes.cbegin());
	for ( ; it != nodes.cend(); it++) {
		const Node &node = it->second;
		Endpoint remote_endpoint(".", &node);
		// Replicate database from the other node
#ifdef HAVE_REMOTE_PROTOCOL
		INFO(this, "Syncing cluster data from %s...\n", node.name.c_str());
		if (trigger_replication(remote_endpoint, *cluster_endpoints.begin(), server)) {
			INFO(this, "Cluster data being synchronized from %s...\n", node.name.c_str());
			new_cluster = 2;
			break;
		}
#endif
	}

	pthread_mutex_unlock(&nodes_mtx);

	// Set node as ready!
	set_node_name(local_node.name);
	state = STATE_READY;

	switch (new_cluster) {
		case 0:
			INFO(this, "Joined cluster %s: It is now online!\n", cluster_name.c_str());
			break;
		case 1:
			INFO(this, "Joined new cluster %s: It is now online!\n", cluster_name.c_str());
			break;
		case 2:
			INFO(this, "Joined cluster %s: It was already online!\n", cluster_name.c_str());
			break;
	}

	pthread_mutex_unlock(&qmtx);
}


void XapiandManager::reset_state()
{
	if (state != STATE_RESET) {
		state = STATE_RESET;
		discovery_heartbeat.set(0, HEARTBEAT_INIT);
	}
}


bool XapiandManager::put_node(const Node &node)
{
	pthread_mutex_lock(&nodes_mtx);
	std::string node_name_lower(stringtolower(node.name));
	if (node_name_lower == stringtolower(local_node.name)) {
		local_node.touched = time(NULL);
		pthread_mutex_unlock(&nodes_mtx);
		return false;
	} else {
		try {
			Node &node_ref = nodes.at(node_name_lower);
			if (node == node_ref) {
				node_ref.touched = time(NULL);
			}
		} catch (const std::out_of_range &err) {
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

bool XapiandManager::get_node(const std::string &node_name, const Node **node)
{
	try {
		std::string node_name_lower(stringtolower(node_name));
		const Node &node_ref = nodes.at(node_name_lower);
		*node = &node_ref;
		return true;
	} catch (const std::out_of_range &err) {
		return false;
	}
}


bool XapiandManager::touch_node(const std::string &node_name, const Node **node)
{
	pthread_mutex_lock(&nodes_mtx);
	std::string node_name_lower(stringtolower(node_name));
	if (node_name_lower == stringtolower(local_node.name)) {
		local_node.touched = time(NULL);
		if (node) *node = &local_node;
		pthread_mutex_unlock(&nodes_mtx);
		return true;
	} else {
		try {
			Node &node_ref = nodes.at(node_name_lower);
			node_ref.touched = time(NULL);
			if (node) *node = &node_ref;
			pthread_mutex_unlock(&nodes_mtx);
			return true;
		} catch (const std::out_of_range &err) {
		} catch(...) {
			pthread_mutex_unlock(&nodes_mtx);
			throw;
		}
	}
	pthread_mutex_unlock(&nodes_mtx);
	return false;
}


void XapiandManager::drop_node(const std::string &node_name)
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
				LOG_DISCOVERY(this, "Node IP address is %s on interface %s\n", ip, ifa->ifa_name);
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

	async_shutdown.stop();
	LOG_EV(this, "\tStop async shutdown event\n");

	discovery_heartbeat.stop();
	LOG_EV(this, "\tStop heartbeat event\n");

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


#ifdef HAVE_REMOTE_PROTOCOL
bool XapiandManager::trigger_replication(const Endpoint &src_endpoint, const Endpoint &dst_endpoint, XapiandServer *server)
{
	int client_sock;
	if ((client_sock = connection_socket()) < 0) {
		return false;
	}

	BinaryClient *client = new BinaryClient(server, server->loop, client_sock, &database_pool, &thread_pool, active_timeout, idle_timeout);

	if (!client->init_replication(src_endpoint, dst_endpoint)) {
		delete client;
		return false;
	}

	return true;
}
#endif /* HAVE_REMOTE_PROTOCOL */


void XapiandManager::discovery_heartbeat_cb(ev::timer &, int)
{
	double next_heartbeat;
	XapiandServer *server;

	switch (state) {
		case STATE_RESET:
			if (!local_node.name.empty()) {
				drop_node(local_node.name);
			}
			if (node_name.empty()) {
				local_node.name = name_generator();
			} else {
				local_node.name = node_name;
			}
			INFO(this, "Advertising as %s (id: %llu)...\n", local_node.name.c_str(), local_node.id);

		case STATE_WAITING:
			discovery(DISCOVERY_HELLO, local_node.serialise());

		default:
			state--;
			// LOG(this, "Waiting for responses %d...\n", state);
			break;

		case STATE_SETUP:
			server = static_cast<XapiandServer *>(*_children.begin());
			server->async_setup_node.send();
			break;

		case STATE_READY:
			discovery(DISCOVERY_HEARTBEAT, local_node.serialise());
			next_heartbeat = random_real(HEARTBEAT_MIN, HEARTBEAT_MAX);
			// LOG(this, "Planning next heartbeat in %f ms.\n", next_heartbeat * 1000.0);
			discovery_heartbeat.set(next_heartbeat, HEARTBEAT_INIT);
			break;
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
			if (discovery_sock != -1 && !ignored_errorno(errno, true)) {
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


void XapiandManager::shutdown_cb(ev::async &, int)
{
	sig_shutdown_handler(0);
}


void XapiandManager::shutdown()
{
	Worker::shutdown();

	if (shutdown_asap) {
		discovery(DISCOVERY_BYE, local_node.serialise());
		destroy();
		LOG_OBJ(this, "Finishing thread pool!\n");
		thread_pool.finish();
	}
	if (shutdown_now) {
		break_loop();
	}
}


void XapiandManager::run(int num_servers, int num_replicators)
{
	std::string msg("Listening on ");
	if (local_node.http_port != -1) {
		msg += "tcp:" + std::to_string(local_node.http_port) + " (http), ";
	}
#ifdef HAVE_REMOTE_PROTOCOL
	if (local_node.binary_port != -1) {
		msg += "tcp:" + std::to_string(local_node.binary_port) + " (xapian v" + std::to_string(XAPIAN_REMOTE_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAN_REMOTE_PROTOCOL_MINOR_VERSION) + "), ";
	}
#endif
	if (discovery_port != -1) {
		msg += "udp:" + std::to_string(discovery_port) + " (discovery v" + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION) + "), ";
	}
	msg += "at pid:" + std::to_string(getpid()) + "...\n";

	INFO(this, msg.c_str());

	INFO(this, "Starting %d server worker thread%s and %d replicator%s.\n", num_servers, (num_servers == 1) ? "" : "s", num_replicators, (num_replicators == 1) ? "" : "s");

	ThreadPool server_pool("S%d", num_servers);
	for (int i = 0; i < num_servers; i++) {
		XapiandServer *server = new XapiandServer(this, NULL);
		server->register_server(new HttpServer(server, server->loop, http_sock, &database_pool, &thread_pool));
#ifdef HAVE_REMOTE_PROTOCOL
		server->register_server(new BinaryServer(server, server->loop, binary_sock, &database_pool, &thread_pool));
#endif
		server->register_server(new DiscoveryServer(server, server->loop, discovery_sock, &database_pool, &thread_pool));
		server_pool.addTask(server);
	}

	ThreadPool replicator_pool("R%d", num_replicators);
	for (int i = 0; i < num_replicators; i++) {
		XapiandReplicator *replicator = new XapiandReplicator(this, NULL, &database_pool);
		replicator_pool.addTask(replicator);
	}

	INFO(this, "Joining cluster %s...\n", cluster_name.c_str());

	discovery_heartbeat.start();
	LOG_EV(this, "\tStart heartbeat event\n");

	LOG_EV(this, "\tStarting manager loop...\n");
	loop->run();
	LOG_EV(this, "\tManager loop ended!\n");

	LOG_OBJ(this, "Waiting for servers...\n");
	server_pool.finish();
	server_pool.join();

	LOG_OBJ(this, "Waiting for replicators...\n");
	replicator_pool.finish();
	replicator_pool.join();

	LOG_OBJ(this, "Server ended!\n");
}


int XapiandManager::get_region(const std::string &db_name)
{
	if (local_node.regions == -1) {
		local_node.regions = sqrt(nodes.size());
		LOG(this, "Regions: %d\n", local_node.regions);
	}
	std::hash<std::string> hash_fn;
	return jump_consistent_hash(hash_fn(db_name), local_node.regions);
}


int XapiandManager::get_region()
{
	if (local_node.regions == -1) {
		local_node.regions = sqrt(nodes.size());
		LOG(this, "Regions: %d\n", local_node.regions);
		local_node.region = jump_consistent_hash(local_node.id, local_node.regions);
	}
	return local_node.region;
}


unique_cJSON XapiandManager::server_status()
{
	unique_cJSON root_status(cJSON_CreateObject(), cJSON_Delete);
	std::string contet_ser;
	pthread_mutex_lock(&XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status.get(), "Connections", XapiandServer::total_clients);
	cJSON_AddNumberToObject(root_status.get(), "Http connections", XapiandServer::http_clients);
	cJSON_AddNumberToObject(root_status.get(), "Xapian remote connections", XapiandServer::binary_clients);
	pthread_mutex_unlock(&XapiandServer::static_mutex);
	cJSON_AddNumberToObject(root_status.get(), "Size thread pool", thread_pool.length());
	return std::move(root_status);
}


unique_cJSON XapiandManager::get_stats_time(const std::string &time_req)
{
	unique_cJSON root_stats(cJSON_CreateObject(), cJSON_Delete);
	pos_time_t first_time, second_time;
	int len = (int) time_req.size();
	unique_group unique_gr;
	int ret = pcre_search(time_req.c_str(), len, 0, 0, TIME_RE, &compiled_time_re, unique_gr);
	group_t *g = unique_gr.get();

	if (ret == 0 && (g[0].end - g[0].start) == len) {
		if ((g[1].end - g[1].start) > 0) {
			first_time.minute = 60 * (((g[3].end - g[3].start) > 0) ? strtol(std::string(time_req.c_str() + g[3].start, g[3].end - g[3].start)) : 0);
			first_time.minute += ((g[4].end - g[4].start) > 0) ? strtol(std::string(time_req.c_str() + g[4].start, g[4].end - g[4].start -1)) : 0;
			first_time.second = ((g[5].end - g[5].start) > 0) ? strtol(std::string(time_req.c_str() + g[5].start, g[5].end - g[5].start -1)) : 0;
			if ((g[6].end - g[6].start) > 0) {
				second_time.minute = 60 * (((g[8].end - g[8].start) > 0) ? strtol(std::string(time_req.c_str() + g[8].start, g[8].end - g[8].start)) : 0);
				second_time.minute += ((g[9].end - g[9].start) > 0) ? strtol(std::string(time_req.c_str() + g[9].start, g[9].end - g[9].start -1)) : 0;
				second_time.second = ((g[10].end - g[10].start) > 0) ? strtol(std::string(time_req.c_str() + g[10].start, g[10].end - g[10].start -1)) : 0;
			} else {
				second_time.minute = 0;
				second_time.second = 0;
			}

			return get_stats_json(first_time, second_time);
		}
	}

	cJSON_AddStringToObject(root_stats.get(), "Error in time argument input", "Incorrect input.");
	return std::move(root_stats);
}


unique_cJSON XapiandManager::get_stats_json(pos_time_t first_time, pos_time_t second_time)
{
	unique_cJSON root_stats(cJSON_CreateObject(), cJSON_Delete);
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
		cJSON_AddStringToObject(root_stats.get(), "Error in time argument input", "First argument must be less or equal than the second.");
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
		cJSON_AddItemToObject(root_stats.get(), "Time", time_period);
		cJSON_AddNumberToObject(root_stats.get(), "Docs index", cnt[0]);
		cJSON_AddNumberToObject(root_stats.get(), "Number searches", cnt[1]);
		cJSON_AddNumberToObject(root_stats.get(), "Docs deleted", cnt[2]);
		cJSON_AddNumberToObject(root_stats.get(), "Average time indexing", tm_cnt[0] / ((cnt[0] == 0) ? 1 : cnt[0]));
		cJSON_AddNumberToObject(root_stats.get(), "Average search time", tm_cnt[1] / ((cnt[1] == 0) ? 1 : cnt[1]));
		cJSON_AddNumberToObject(root_stats.get(), "Average deletion time", tm_cnt[2] / ((cnt[2] == 0) ? 1 : cnt[2]));
	}

	return std::move(root_stats);
}
