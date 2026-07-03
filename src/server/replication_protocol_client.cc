/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "replication_protocol_client.h"

#ifdef XAPIAND_CLUSTERING

#include <cassert>                            // for assert
#include <errno.h>                            // for errno
#include <fcntl.h>
#include <limits.h>                           // for PATH_MAX
#include <poll.h>                             // for poll (bounded blocking writes)
#include <sys/socket.h>
#include <sys/stat.h>
#include <sysexits.h>
#include <unistd.h>

#include "database/lock.h"                    // for lock_shard
#include "database/shard.h"                   // for Shard
#include "database/wal.h"                     // for DatabaseWAL
#include "error.hh"                           // for error:name, error::description
#include "exception_xapian.h"                 // for InvalidArgumentError
#include "fs.hh"                              // for delete_files, build_path_index
#include "io.hh"                              // for io::*
#include "index_resolver_lru.h"               // for IndexSettings
#include "length.h"                           // for serialise_length, unserialise_length
#include "manager.h"                          // for XapiandManager
#include "metrics.h"                          // for Metrics::metrics
#include <netdb.h>                            // for getaddrinfo, addrinfo, gai_strerror
#include <netinet/in.h>                       // for IPPROTO_TCP
#include <netinet/tcp.h>                      // for TCP_NODELAY
#include "random.hh"                          // for random_int
#include "repr.hh"                            // for repr
#include "utype.hh"                           // for toUType
#include "xapian/net/serialise-error.h"       // for serialise_error, unserialise_error

#include "flume.h"                            // for flume::Sender (whole-DB-file streaming, Zstd)


// A libev-free clone of the old server-lib TCP::connect: its tcp.h dragged in worker.h ->
// libev just to reach this static socket helper. Resolves host:servname, opens a
// non-blocking TCP socket with the same options (KEEPALIVE / LINGER 0 / NODELAY, and
// NOSIGPIPE where available), and starts a connect -- EINPROGRESS is expected, since the
// caller adopts the fd into an Asio socket and awaits writability. Returns the fd, or -1.
static int tcp_connect(const char* hostname, const char* servname) noexcept
{
	L_CALL("tcp_connect({}, {})", hostname, servname);

	struct addrinfo hints = {};
	hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV;
	hints.ai_family = PF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	struct addrinfo* addrinfo;
	if (int err = getaddrinfo(hostname, servname, &hints, &addrinfo)) {
		L_ERR("Couldn't resolve host {}:{}: {}", hostname, servname, gai_strerror(err));
		return -1;
	}

	for (auto ai = addrinfo; ai != nullptr; ai = ai->ai_next) {
		int conn_sock;
		int optval = 1;

		if ((conn_sock = io::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) == -1) {
			if (ai->ai_next == nullptr) {
				L_ERR("ERROR: {}:{} socket: {} ({}): {}", hostname, servname, error::name(errno), errno, error::description(errno));
				freeaddrinfo(addrinfo);
				return -1;
			}
			continue;
		}

		if (io::fcntl(conn_sock, F_SETFL, io::fcntl(conn_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
			io::close(conn_sock);
			freeaddrinfo(addrinfo);
			return -1;
		}

#ifdef SO_NOSIGPIPE
		if (io::setsockopt(conn_sock, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1) {
			io::close(conn_sock);
			freeaddrinfo(addrinfo);
			return -1;
		}
#endif
		if (io::setsockopt(conn_sock, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) == -1) {
			io::close(conn_sock);
			freeaddrinfo(addrinfo);
			return -1;
		}

		struct linger linger;
		linger.l_onoff = 1;
		linger.l_linger = 0;
		if (io::setsockopt(conn_sock, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger)) == -1) {
			io::close(conn_sock);
			freeaddrinfo(addrinfo);
			return -1;
		}

		if (io::setsockopt(conn_sock, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) == -1) {
			io::close(conn_sock);
			freeaddrinfo(addrinfo);
			return -1;
		}

		if (io::connect(conn_sock, ai->ai_addr, ai->ai_addrlen) == -1 && errno != EINPROGRESS && errno != EALREADY) {
			io::close(conn_sock);
			freeaddrinfo(addrinfo);
			return -1;
		}

		freeaddrinfo(addrinfo);
		return conn_sock;
	}

	L_ERR("ERROR: connect error to {}:{}: {} ({}): {}", hostname, servname, error::name(errno), errno, error::description(errno));
	freeaddrinfo(addrinfo);
	return -1;
}


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_REPLICATION
// #define L_REPLICATION L_ROSY_BROWN
// #undef L_CONN
// #define L_CONN L_GREEN
// #undef L_REPLICA_WIRE
// #define L_REPLICA_WIRE L_ORANGE
// #undef L_REPLICA
// #define L_REPLICA L_TEAL
// #undef L_REPLICA_PROTO
// #define L_REPLICA_PROTO L_TEAL
// #undef L_TIMED_VAR
// #define L_TIMED_VAR _L_TIMED_VAR


/*  ____            _ _           _   _
 * |  _ \ ___ _ __ | (_) ___ __ _| |_(_) ___  _ __
 * | |_) / _ \ '_ \| | |/ __/ _` | __| |/ _ \| '_ \
 * |  _ <  __/ |_) | | | (_| (_| | |_| | (_) | | | |
 * |_| \_\___| .__/|_|_|\___\__,_|\__|_|\___/|_| |_|
 *           |_|
 */


// Write every byte to the socket fd, blocking (via poll) when the send buffer is full. Runs
// on the reactor pool thread during a dispatch (the coroutine is suspended, so nothing else
// touches the socket). Returns false on a real error -- including when the connection's
// abortable shuts the fd down at server stop, which makes the pending poll return POLLHUP.
static bool blocking_write_all(int fd, const char* data, std::size_t size) {
	while (size > 0) {
		ssize_t w = io::write(fd, data, size);
		if (w > 0) { data += w; size -= static_cast<std::size_t>(w); continue; }
		if (w < 0 && errno == EINTR) { continue; }
		if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			struct pollfd pfd;
			pfd.fd = fd;
			pfd.events = POLLOUT;
			pfd.revents = 0;
			int pr = ::poll(&pfd, 1, -1);
			if (pr < 0 && errno == EINTR) { continue; }
			if (pr <= 0 || (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) { return false; }
			continue;
		}
		return false;
	}
	return true;
}

// A flume Writer that streams compressed file blocks to the socket fd (bounded memory),
// tallying the bytes into the connection's sent counter.
struct SocketWriter {
	int fd;
	std::size_t* sent;
	bool write(std::string_view v) {
		if (!blocking_write_all(fd, v.data(), v.size())) { return false; }
		if (sent != nullptr) { *sent += v.size(); }
		return true;
	}
};


ReplicationProtocolClient::ReplicationProtocolClient(bool cluster_database_)
	:
#ifdef SAVE_LAST_MESSAGES
	  last_message_received('\xff'),
	  last_message_sent('\xff'),
#endif
	  temp_file_template("xapiand.XXXXXX"),
	  cluster_database(cluster_database_),
	  sock_fd_(-1),
	  closing_(false),
	  total_sent_bytes(0),
	  current_revision(0),
	  changesets(0)
{
	auto manager = XapiandManager::manager();
	if (manager) {
		++manager->replication_clients;
	}

	Metrics::metrics()
		.xapiand_replication_connections
		.Increment();

	L_CONN("New Replication Protocol Client, {} client(s) of a total of {} connected.", manager ? manager->replication_clients.load() : 0, manager ? manager->total_clients.load() : 0);
}


ReplicationProtocolClient::~ReplicationProtocolClient() noexcept
{
	try {
		reset();
		lk_shard_ptr.reset();

		if (auto manager = XapiandManager::manager()) {
			if (manager->replication_clients.fetch_sub(1) == 0) {
				L_CRIT("Inconsistency in number of replication clients");
				sig_exit(-EX_SOFTWARE);
			}
		}

		for (const auto& filename : temp_files) {
			io::unlink(filename.c_str());
		}

		if (!temp_directory.empty()) {
			delete_files(temp_directory.c_str());
		}

		if (cluster_database) {
			L_CRIT("Cannot synchronize cluster database!");
			sig_exit(-EX_CANTCREAT);
		}
	} catch (...) {
		L_EXC("Unhandled exception in destructor");
	}
}


std::pair<int, std::string>
ReplicationProtocolClient::new_temp_file()
{
	// Create (+ track, for the destructor's cleanup) a temp file to stream an incoming
	// FILE_FOLLOWS file into. Ported verbatim from the classic on_read FILE_FOLLOWS path.
	char path[PATH_MAX + 1];
	if (temp_directory.empty()) {
		if (temp_directory_template.empty()) {
			temp_directory = "/tmp";
		} else {
			strncpy(path, temp_directory_template.c_str(), PATH_MAX);
			build_path_index(temp_directory_template);
			if (io::mkdtemp(path) == nullptr) {
				L_ERR("Directory {} not created: {} ({}): {}", temp_directory_template, error::name(errno), errno, error::description(errno));
				return {-1, std::string()};
			}
			temp_directory = path;
		}
	}
	strncpy(path, (temp_directory + "/" + temp_file_template).c_str(), PATH_MAX);
	int fd = io::mkstemp(path);
	if (fd == -1) {
		L_ERR("Cannot create temporary file: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return {-1, std::string()};
	}
	temp_files.push_back(path);
	return {fd, std::string(path)};
}


void
ReplicationProtocolClient::reset()
{
	wal.reset();

	if (switch_shard) {
		switch_shard->close();
		auto manager = XapiandManager::manager();
		if (manager) {
			manager->database_pool->checkin(switch_shard);
		}
	}

	if (!switch_shard_path.empty()) {
		delete_files(switch_shard_path.c_str());
		switch_shard_path.clear();
	}

	if (log) {
		log->clear();
		log.reset();
	}

	changesets = 0;
}


bool
ReplicationProtocolClient::init_replication_protocol(const std::string& host, int port, const Endpoint &src_endpoint, const Endpoint &dst_endpoint) noexcept
{
	L_CALL("ReplicationProtocolClient::init_replication_protocol({}, {})", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));

	// Get fast write lock for replication or retry later
	try {
		lk_shard_ptr = std::make_unique<lock_shard>(dst_endpoint, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT | DB_REPLICA, false);
		lk_shard_ptr->lock(0, [=] {
			// If it cannot checkout because database is busy, retry when ready...
			trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), dst_endpoint.path, src_endpoint, dst_endpoint);
		});

		temp_directory_template = dst_endpoint.path + "/.tmp.XXXXXX";
	} catch (const Xapian::DatabaseNotAvailableError&) {
		lk_shard_ptr.reset();
		L_REPLICATION("Replication deferred (not available): {} -->  {}", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
		return false;
	} catch (...) {
		lk_shard_ptr.reset();
		L_EXC("ERROR: Replication initialization ended with an unhandled exception (1)");
		return false;
	}

	int client_sock = tcp_connect(host.c_str(), std::to_string(port).c_str());
	if (client_sock == -1) {
		lk_shard_ptr.reset();
		try {
			// If it cannot replicate because the other end is down, retry in a bit...
			trigger_replication()->delayed_debounce(std::chrono::milliseconds(random_int(0, 3000)), dst_endpoint.path, src_endpoint, dst_endpoint);
			L_REPLICATION("Replication deferred (cannot connect): {} -->  {}", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
			return false;
		} catch (...) {
			L_EXC("ERROR: Replication initialization ended with an unhandled exception (2)");
			return false;
		}
	}
	L_CONN("Connected to {}! (in socket {})", repr(src_endpoint.to_string()), client_sock);

	sock_fd_ = client_sock;

	L_REPLICATION("Replication initialized: {} -->  {}", repr(src_endpoint.to_string()), repr(dst_endpoint.to_string()));
	return true;
}


void
ReplicationProtocolClient::send_message(ReplicationMessageType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::send_message({}, <message>)", enum_name(type));

	L_REPLICA_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));

	send_message(toUType(type), message);
}


void
ReplicationProtocolClient::send_message(ReplicationReplyType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::send_message({}, <message>)", enum_name(type));

	L_REPLICA_PROTO("<< send_message ({}): {}", enum_name(type), repr(message));

	send_message(toUType(type), message);
}


void
ReplicationProtocolClient::send_file(ReplicationReplyType type, int fd)
{
	L_CALL("ReplicationProtocolClient::send_file({}, <fd>)", enum_name(type));

	L_REPLICA_PROTO("<< send_file ({}): {}", enum_name(type), fd);

	send_file(toUType(type), fd);
}


void
ReplicationProtocolClient::replication_server(ReplicationMessageType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::replication_server({}, <message>)", enum_name(type));

	L_OBJ_BEGIN("ReplicationProtocolClient::replication_server:BEGIN {{type:{}}}", enum_name(type));
	L_OBJ_END("ReplicationProtocolClient::replication_server:END {{type:{}}}", enum_name(type));

	L_DEBUG("{} ({}) -> {}", enum_name(type), strings::from_bytes(message.size()), repr(endpoint.to_string()));

	try {
		switch (type) {
			case ReplicationMessageType::MSG_GET_CHANGESETS:
				msg_get_changesets(message);
				return;
			case ReplicationMessageType::MSG_SET_REVISION:
				msg_set_revision(message);
				return;
			default: {
				std::string errmsg("Unexpected message type ");
				errmsg += std::to_string(toUType(type));
				THROW(InvalidArgumentError, errmsg);
			}
		}
	} catch (const Xapian::NetworkTimeoutError& exc) {
		L_EXC("ERROR: Dispatching replication protocol message");
		try {
			// We've had a timeout, so the client may not be listening, if we can't
			// send the message right away, just exit and the client will cope.
			send_message(ReplicationReplyType::REPLY_EXCEPTION, serialise_error(exc));
		} catch (...) { }
		reset();
		destroy();
		detach();
	} catch (const Xapian::NetworkError&) {
		// All other network errors mean we are fatally confused and are unlikely
		// to be able to communicate further across this connection. So we don't
		// try to propagate the error to the client, but instead just log the
		// exception and close the connection.
		L_EXC("ERROR: Dispatching replication protocol message");
		reset();
		close();
		destroy();
		detach();
	} catch (const Xapian::Error& exc) {
		// Propagate the exception to the client, then return to the main
		// message handling loop.
		send_message(ReplicationReplyType::REPLY_EXCEPTION, serialise_error(exc));
		reset();
	} catch (...) {
		L_EXC("ERROR: Dispatching replication protocol message");
		send_message(ReplicationReplyType::REPLY_EXCEPTION, std::string());
		reset();
		destroy();
		detach();
	}
}


void
ReplicationProtocolClient::msg_get_changesets(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::msg_get_changesets(<message>)");

	L_REPLICATION("GET_CHANGESETS");

	size_t _total_sent_bytes = total_sent_bytes;
	auto begins = std::chrono::steady_clock::now();

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_uuid = unserialise_string(&p, p_end);
	auto remote_revision = unserialise_length(&p, p_end);
	auto endpoint_path = unserialise_string(&p, p_end);

	if (endpoint_path.empty()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Database must have a valid path");
		reset();
		destroy();
		detach();

		if (opts.log_replicas) {
			auto ends = std::chrono::steady_clock::now();
			_total_sent_bytes = total_sent_bytes - _total_sent_bytes;
			L(LOG_NOTICE, rgb(190, 30, 10), "SENDING {}: FAILURE {} {}", repr(endpoint_path), strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
		}
		return;
	}

	auto index_settings = XapiandManager::resolve_index_settings(endpoint_path, true);
	if (index_settings.shards.size() != 1) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Ignore getting changesets of unknown index");
		return;
	}
	const auto& shard_nodes = index_settings.shards[0].nodes;
	if (shard_nodes.empty()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Ignore getting changesets of misconfigured index");
		return;
	}
	auto node = Node::get_node(shard_nodes[0]);
	if (!node) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Ignore getting changesets of unexistent node");
		return;
	}
	if (!node->is_local()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Ignore getting changesets of a replicated index");
		return;
	}

	lock_shard lk_shard(Endpoint{endpoint_path}, DB_OPEN | DB_WRITABLE | DB_DISABLE_WRITES | DB_DISABLE_WAL | DB_DISABLE_AUTOCOMMIT, false);

	auto db = lk_shard.lock()->db();
	auto uuid = db->get_uuid();
	auto db_revision = db->get_revision();
	lk_shard.unlock();

	auto from_revision = remote_revision;
	if (from_revision && uuid != remote_uuid) {
		L_REPLICATION("GET_CHANGESETS: UUID mismatch for {} ({} vs. {})", repr(endpoint_path), uuid, remote_uuid);
		from_revision = 0;
	}

	wal = std::make_unique<DatabaseWAL>(endpoint_path);
	if (from_revision && db_revision != from_revision && wal->locate_revision(from_revision).first == DatabaseWAL::max_rev) {
		L_REPLICATION("GET_CHANGESETS: Cannot locate revision {} for {}", from_revision, repr(endpoint_path));
		from_revision = 0;
	}

	auto to_revision = from_revision;

	if (to_revision < db_revision) {
		if (to_revision == 0) {
			int whole_db_copies_left = 5;

			while (true) {
				// Send the current revision number in the header.
				send_message(ReplicationReplyType::REPLY_DB_HEADER,
					serialise_string(uuid) +
					serialise_length(db_revision));

				static std::array<const std::string, 7> filenames = {
					"termlist.glass",
					"synonym.glass",
					"spelling.glass",
					"docdata.glass",
					"position.glass",
					"postlist.glass",
					"iamglass"
				};

				for (const auto& filename : filenames) {
					auto path = strings::format("{}/{}", endpoint_path, filename);
					int fd = io::open(path.c_str());
					if (fd != -1) {
						send_message(ReplicationReplyType::REPLY_DB_FILENAME, filename);
						send_file(ReplicationReplyType::REPLY_DB_FILEDATA, fd);
					}
				}

				for (size_t volume = 0; true; ++volume) {
					auto filename = strings::format("docdata.{}", volume);
					auto path = strings::format("{}/{}", endpoint_path, filename);
					int fd = io::open(path.c_str());
					if (fd != -1) {
						send_message(ReplicationReplyType::REPLY_DB_FILENAME, filename);
						send_file(ReplicationReplyType::REPLY_DB_FILEDATA, fd);
						continue;
					}
					break;
				}

				db = lk_shard.lock()->db();
				auto final_revision = db->get_revision();
				lk_shard.unlock();

				send_message(ReplicationReplyType::REPLY_DB_FOOTER, serialise_length(final_revision));

				if (db_revision == final_revision) {
					to_revision = db_revision;
					break;
				}

				if (whole_db_copies_left == 0) {
					send_message(ReplicationReplyType::REPLY_FAIL, "Database changing too fast");
					reset();
					destroy();
					detach();

					if (opts.log_replicas) {
						auto ends = std::chrono::steady_clock::now();
						_total_sent_bytes = total_sent_bytes - _total_sent_bytes;
						L(LOG_NOTICE, rgb(190, 30, 10), "SENDING {}: FAILURE {} {}", repr(endpoint_path), strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
					}
					return;
				} else if (--whole_db_copies_left == 0) {
					db = lk_shard.lock()->db();
					uuid = db->get_uuid();
					db_revision = db->get_revision();
				} else {
					db = lk_shard.lock()->db();
					uuid = db->get_uuid();
					db_revision = db->get_revision();
					lk_shard.unlock();
				}
			}
			lk_shard.unlock();
		}

		int wal_iterations = 5;
		do {
			// Send WAL operations.
			std::vector<std::string> reply_changesets;
			for (auto wal_it = wal->find(to_revision); wal_it != wal->end(); ++wal_it) {
				auto& line = *wal_it;
				const char *lp = line.data();
				const char *lp_end = lp + line.size();
				auto revision = unserialise_length(&lp, lp_end);
				if (revision >= db_revision) {
					break;
				}
				auto type = static_cast<DatabaseWAL::Type>(unserialise_length(&lp, lp_end));
				if (type == DatabaseWAL::Type::COMMIT) {
					for (auto& reply_changeset : reply_changesets) {
						send_message(ReplicationReplyType::REPLY_CHANGESET, reply_changeset);
					}
					send_message(ReplicationReplyType::REPLY_CHANGESET, line);
					reply_changesets.clear();
					++to_revision;
				} else {
					reply_changesets.push_back(std::move(line));
				}
			}
			db = lk_shard.lock()->db();
			db_revision = db->get_revision();
			lk_shard.unlock();
		} while (to_revision < db_revision && --wal_iterations != 0);
	}

	send_message(ReplicationReplyType::REPLY_END_OF_CHANGES);

	if (opts.log_replicas) {
		auto ends = std::chrono::steady_clock::now();
		_total_sent_bytes = total_sent_bytes - _total_sent_bytes;
		if (from_revision != to_revision) {
			L(LOG_DEBUG, rgb(55, 100, 79), "SENDING {}: From revision {} to {} {} {}", repr(endpoint_path), from_revision, to_revision, strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
		} else {
			L(LOG_DEBUG, rgb(116, 100, 77), "SENDING {}: No changes at revision {} {}", repr(endpoint_path), remote_revision, strings::from_bytes(_total_sent_bytes), strings::from_delta(begins, ends));
		}
	}
}


void
ReplicationProtocolClient::msg_set_revision(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::msg_set_revision(<message>)");

	L_REPLICATION("SET_REVISION");

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node_lower_name = std::string(unserialise_string(&p, p_end));
	auto remote_uuid = unserialise_string(&p, p_end);
	auto remote_revision = unserialise_length(&p, p_end);
	auto endpoint_path = unserialise_string(&p, p_end);

	if (endpoint_path.empty()) {
		send_message(ReplicationReplyType::REPLY_FAIL, "Database must have a valid path");
		reset();
		destroy();
		detach();
		return;
	}

	lock_shard lk_shard(Endpoint{endpoint_path}, DB_OPEN | DB_WRITABLE | DB_DISABLE_AUTOCOMMIT, false);

	auto db = lk_shard.lock()->db();
	auto uuid = db->get_uuid();
	if (uuid == remote_uuid) {
		lk_shard->endpoint.set_revision(remote_node_lower_name, remote_revision);
	}
	lk_shard.unlock();

	send_message(ReplicationReplyType::REPLY_DONE);
}


void
ReplicationProtocolClient::replication_client(ReplicationReplyType type, const std::string& message)
{
	L_CALL("ReplicationProtocolClient::replication_client({}, <message>)", enum_name(type));

	L_OBJ_BEGIN("ReplicationProtocolClient::replication_client:BEGIN {{type:{}}}", enum_name(type));
	L_OBJ_END("ReplicationProtocolClient::replication_client:END {{type:{}}}", enum_name(type));

	L_DEBUG("{} ({}) -> {}", enum_name(type), strings::from_bytes(message.size()), repr(endpoint.to_string()));

	try {
		switch (type) {
			case ReplicationReplyType::REPLY_WELCOME:
				reply_welcome(message);
				return;
			case ReplicationReplyType::REPLY_EXCEPTION:
				reply_exception(message);
				return;
			case ReplicationReplyType::REPLY_END_OF_CHANGES:
				reply_end_of_changes(message);
				return;
			case ReplicationReplyType::REPLY_FAIL:
				reply_fail(message);
				return;
			case ReplicationReplyType::REPLY_DB_HEADER:
				reply_db_header(message);
				return;
			case ReplicationReplyType::REPLY_DB_FILENAME:
				reply_db_filename(message);
				return;
			case ReplicationReplyType::REPLY_DB_FILEDATA:
				reply_db_filedata(message);
				return;
			case ReplicationReplyType::REPLY_DB_FOOTER:
				reply_db_footer(message);
				return;
			case ReplicationReplyType::REPLY_CHANGESET:
				reply_changeset(message);
				return;
			case ReplicationReplyType::REPLY_DONE:
				reply_done(message);
				return;
			default: {
				std::string errmsg("Unexpected message type ");
				errmsg += std::to_string(toUType(type));
				THROW(InvalidArgumentError, errmsg);
			}
		}
	} catch (const BaseException& exc) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	} catch (const Xapian::DatabaseNotFoundError& exc) {
	} catch (const Xapian::Error& exc) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	} catch (const std::exception& exc) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	} catch (...) {
		assert(lk_shard_ptr);
		L_EXC("ERROR: Replicating database: {}", (*lk_shard_ptr)->endpoint.path);
	}

	reset();
	close();  // client closes on error, take no more messages!
	destroy();
	detach();
}


void
ReplicationProtocolClient::reply_welcome(const std::string&)
{
	std::string message;

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();
	auto db = shard->db();

	message.append(serialise_string(db->get_uuid()));
	message.append(serialise_length(db->get_revision()));
	message.append(serialise_string(shard->endpoint.path));

	send_message(ReplicationMessageType::MSG_GET_CHANGESETS, message);
}


void
ReplicationProtocolClient::reply_exception(const std::string& message)
{
	unserialise_error(message, "REPLICATION:", "");
}


void
ReplicationProtocolClient::reply_end_of_changes(const std::string&)
{
	L_CALL("ReplicationProtocolClient::reply_end_of_changes(<message>)");

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();

	L_REPLICATION("END_OF_CHANGES");

	bool switching = !switch_shard_path.empty();

	if (switching) {
		// Close internal databases
		shard->do_close(false, false, Shard::Transaction::none);

		auto manager = XapiandManager::manager();
		if (switch_shard) {
			switch_shard->close();
			if (manager) {
				manager->database_pool->checkin(switch_shard);
			}
		}

		// get exclusive lock
		if (manager) {
			manager->database_pool->lock(shard);
		}

		// Now we are sure no readers are using the database before moving the files
		XapiandManager::manager(true)->wal_writer->delete_wal(
			shard->is_synchronous_wal(),
			shard->endpoint.path
		);
		delete_files(shard->endpoint.path, {"*glass", "flintlock"});
		move_files(switch_shard_path, shard->endpoint.path);

		// release exclusive lock
		if (manager) {
			manager->database_pool->unlock(shard);
		}
	}

	auto db = shard->db();

	if (opts.log_replicas) {
		if (switching && changesets) {
			L(LOG_DEBUG, rgb(55, 100, 79), "RECEIVED {} at revision {}: From a full copy and a set of {} {}{}", repr(shard->endpoint.path),  db->get_revision(), changesets, changesets == 1 ? "changeset" : "changesets", switch_shard ? " (to switch database)" : "");
		} else if (changesets) {
			L(LOG_DEBUG, rgb(55, 100, 79), "RECEIVED {} at revision {}: From a set of {} {}{}", repr(shard->endpoint.path),  db->get_revision(), changesets, changesets == 1 ? "changeset" : "changesets", switch_shard ? " (to switch database)" : "");
		} else if (switching) {
			L(LOG_DEBUG, rgb(55, 100, 79), "RECEIVED {} at revision {}: From a full copy{}", repr(shard->endpoint.path),  db->get_revision(), switch_shard ? " (to switch database)" : "");
		} else {
			L(LOG_DEBUG, rgb(116, 100, 77), "RECEIVED {} at revision {}: No changes{}", repr(shard->endpoint.path),  db->get_revision(), switch_shard ? " (to switch database)" : "");
		}
	}

	if (cluster_database) {
		cluster_database = false;
		XapiandManager::set_cluster_database_ready();
	}

	auto local_node = Node::get_local_node();
	assert(local_node && !local_node->lower_name().empty());

	std::string reply;
	reply.append(serialise_string(local_node->lower_name()));
	reply.append(serialise_string(db->get_uuid()));
	reply.append(serialise_length(db->get_revision()));
	reply.append(serialise_string(shard->endpoint.path));

	send_message(ReplicationMessageType::MSG_SET_REVISION, reply);
}


void
ReplicationProtocolClient::reply_fail(const std::string& msg)
{
	L_CALL("ReplicationProtocolClient::reply_fail(<message>)");

	assert(lk_shard_ptr);
	L_REPLICATION("FAIL: {}", repr((*lk_shard_ptr)->endpoint.path));

	if (opts.log_replicas) {
		L(LOG_NOTICE, rgb(190, 30, 10), "RECEIVED {}: FAILURE: {}", repr((*lk_shard_ptr)->endpoint.path), msg);
	}

	reset();
	close();
	destroy();
	detach();
}


void
ReplicationProtocolClient::reply_db_header(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::reply_db_header(<message>)");

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();

	const char *p = message.data();
	const char *p_end = p + message.size();

	current_uuid = unserialise_string(&p, p_end);
	current_revision = unserialise_length(&p, p_end);

	reset();

	char path[PATH_MAX + 1];
	strncpy(path, temp_directory_template.c_str(), PATH_MAX);
	build_path_index(temp_directory_template);
	if (io::mkdtemp(path) == nullptr) {
		L_ERR("Directory {} not created: {} ({}): {}", path, error::name(errno), errno, error::description(errno));
		detach();
		return;
	}
	switch_shard_path = path;

	L_REPLICATION("DB_HEADER: {} in {} ({} rev:{})", repr(shard->endpoint.path), repr(switch_shard_path), current_uuid, current_revision);
	L_TIMED_VAR(log, 1s,
		"Replication of whole database taking too long: {}",
		"Replication of whole database took too long: {}",
		repr(shard->endpoint.path));
}


void
ReplicationProtocolClient::reply_db_filename(const std::string& filename)
{
	L_CALL("ReplicationProtocolClient::reply_db_filename(<filename>)");

	assert(lk_shard_ptr);

	assert(!switch_shard_path.empty());

	file_path = switch_shard_path + "/" + filename;

	L_REPLICATION("DB_FILENAME({}): {}", repr(filename), repr((*lk_shard_ptr)->endpoint.path));
}


void
ReplicationProtocolClient::reply_db_filedata(const std::string& tmp_file)
{
	L_CALL("ReplicationProtocolClient::reply_db_filedata(<tmp_file>)");

	assert(lk_shard_ptr);

	assert(!switch_shard_path.empty());

	if (::rename(tmp_file.c_str(), file_path.c_str()) == -1) {
		L_ERR("Cannot rename temporary file {} to {}: {} ({}): {}", tmp_file, file_path, error::name(errno), errno, error::description(errno));
		detach();
		return;
	}

	L_REPLICATION("DB_FILEDATA({} -> {}): {}", repr(tmp_file), repr(file_path), repr((*lk_shard_ptr)->endpoint.path));
}


void
ReplicationProtocolClient::reply_db_footer(const std::string& message)
{
	L_CALL("ReplicationProtocolClient::reply_db_footer(<message>)");

	assert(lk_shard_ptr);

	const char *p = message.data();
	const char *p_end = p + message.size();
	size_t revision = unserialise_length(&p, p_end);

	assert(!switch_shard_path.empty());

	if (revision != current_revision) {
		delete_files(switch_shard_path.c_str());
		switch_shard_path.clear();
	}

	L_REPLICATION("DB_FOOTER{}: {}", revision != current_revision ? " (ignored files)" : "", repr((*lk_shard_ptr)->endpoint.path));
}


void
ReplicationProtocolClient::reply_changeset(const std::string& line)
{
	L_CALL("ReplicationProtocolClient::reply_changeset(<line>)");

	assert(lk_shard_ptr);
	auto shard = lk_shard_ptr->locked();

	bool switching = !switch_shard_path.empty();

	if (!wal) {
		if (switching) {
			if (!switch_shard) {
				switch_shard = XapiandManager::manager(true)->database_pool->checkout(Endpoint{switch_shard_path}, DB_CREATE_OR_OPEN | DB_WRITABLE | DB_DISABLE_AUTOCOMMIT | DB_REPLICA | DB_SYNCHRONOUS_WAL);
			}
			switch_shard->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(switch_shard.get());
		} else {
			shard->begin_transaction(false);
			wal = std::make_unique<DatabaseWAL>(shard.get());
		}
		L_TIMED_VAR(log, 1s,
			"Replication of {}changesets taking too long: {}",
			"Replication of {}changesets took too long: {}",
			switching ? "whole database with " : "",
			repr(shard->endpoint.path));
	}

	wal->execute_line(line, false, false);

	++changesets;
	L_REPLICATION("CHANGESET ({} changesets{}): {}", changesets, switch_shard ? " to a new database" : "", repr(shard->endpoint.path));
}


void
ReplicationProtocolClient::reply_done(const std::string&)
{
	L_CALL("ReplicationProtocolClient::reply_done(<message>)");

	reset();
	close();
	destroy();
	detach();
}


void
ReplicationProtocolClient::send_message(char type_as_char, const std::string &message)
{
	L_CALL("ReplicationProtocolClient::send_message(<type_as_char>, <message>)");

#ifdef SAVE_LAST_MESSAGES
	last_message_sent.store(type_as_char, std::memory_order_relaxed);
#endif

	std::string buf;
	buf += type_as_char;
	buf += serialise_length(message.size());
	buf += message;
	if (!blocking_write_all(sock_fd_, buf.data(), buf.size())) { closing_ = true; return; }
	total_sent_bytes += buf.size();
}


void
ReplicationProtocolClient::send_file(char type_as_char, int fd)
{
	L_CALL("ReplicationProtocolClient::send_file(<type_as_char>, <fd>)");

	std::string buf;
	buf += FILE_FOLLOWS;
	buf += type_as_char;
	if (!blocking_write_all(sock_fd_, buf.data(), buf.size())) { closing_ = true; return; }
	total_sent_bytes += buf.size();

	// Stream the file, compressed + framed, in bounded memory (flume, Zstd L6).
	SocketWriter writer{sock_fd_, &total_sent_bytes};
	flume::Sender<SocketWriter> sender(writer, fd);
	if (!sender.send()) { closing_ = true; }
}


std::string
ReplicationProtocolClient::__repr__() const
{
	return strings::format(STEEL_BLUE + "<ReplicationProtocolClient {{ fd:{} }}{}>",
		sock_fd_,
		closing_ ? " " + ORANGE + "(closing)" + STEEL_BLUE : "");
}

#endif  /* XAPIAND_CLUSTERING */
