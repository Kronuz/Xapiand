/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "server_discovery.h"


#ifdef XAPIAND_CLUSTERING

#include <arpa/inet.h>

#include "binary.h"
#include "database_handler.h"
#include "discovery.h"
#include "endpoint.h"
#include "ignore_unused.h"        // for ignore_unused
#include "manager.h"
#include "server.h"


using dispatch_func = void (DiscoveryServer::*)(const std::string&);


DiscoveryServer::DiscoveryServer(const std::shared_ptr<XapiandServer>& server_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, const std::shared_ptr<Discovery>& discovery_)
	: BaseServer(server_, ev_loop_, ev_flags_),
	  discovery(discovery_)
{
	io.start(discovery->sock, ev::READ);
	L_EV("Start discovery's server accept event (sock=%d)", discovery->sock);

	L_OBJ("CREATED DISCOVERY SERVER!");
}


DiscoveryServer::~DiscoveryServer()
{
	L_OBJ("DELETED DISCOVERY SERVER!");
}


void
DiscoveryServer::discovery_server(Discovery::Message type, const std::string& message)
{
	static const dispatch_func dispatch[] = {
		&DiscoveryServer::heartbeat,
		&DiscoveryServer::hello,
		&DiscoveryServer::wave,
		&DiscoveryServer::sneer,
		&DiscoveryServer::enter,
		&DiscoveryServer::bye,
		&DiscoveryServer::db_updated,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		THROW(InvalidArgumentError, errmsg);
	}
	(this->*(dispatch[toUType(type)]))(message);
}


void
DiscoveryServer::_wave(bool heartbeat, const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = std::make_shared<const Node>(Node::unserialise(&p, p_end));

	int32_t region;
	auto local_node_ = local_node.load();
	if (*remote_node == *local_node_) {
		region = local_node_->region;
	} else {
		region = remote_node->region;
	}

	std::shared_ptr<const Node> node = XapiandManager::manager->touch_node(remote_node->name(), region);
	if (node) {
		if (*remote_node != *node && remote_node->lower_name() != local_node_->lower_name()) {
			// After receiving WAVE, if state is still WAITING, flag as WAITING_MORE so it waits just a little longer...
			auto waiting = XapiandManager::State::WAITING;
			XapiandManager::manager->state.compare_exchange_strong(waiting, XapiandManager::State::WAITING_MORE);
			if (heartbeat || node->touched < epoch::now<>() - HEARTBEAT_MAX) {
				XapiandManager::manager->drop_node(remote_node->name());
				L_INFO("Stalled node %s left the party!", remote_node->name());
				if (XapiandManager::manager->put_node(remote_node)) {
					if (heartbeat) {
						L_INFO("Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (1)", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
					} else {
						L_DISCOVERY("Node %s joining the party (1)...", remote_node->name());
					}
					auto local_node_copy = std::make_unique<Node>(*local_node_);
					local_node_copy->regions = -1;
					local_node = std::shared_ptr<const Node>(local_node_copy.release());

					XapiandManager::manager->get_region();
				} else {
					L_ERR("ERROR: Cannot register remote node (1): %s", remote_node->name());
				}
			}
		}
	} else {
		if (XapiandManager::manager->put_node(remote_node)) {
			if (heartbeat) {
				L_INFO("Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (2)", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
			} else {
				L_DISCOVERY("Node %s joining the party (2)...", remote_node->name());
			}
			auto local_node_copy = std::make_unique<Node>(*local_node_);
			local_node_copy->regions = -1;
			local_node = std::shared_ptr<const Node>(local_node_copy.release());

			XapiandManager::manager->get_region();
		} else {
			L_ERR("ERROR: Cannot register remote node (2): %s", remote_node->name());
		}
	}
}


void
DiscoveryServer::heartbeat(const std::string& message)
{
	_wave(true, message);
}


void
DiscoveryServer::hello(const std::string& message)
{
	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = local_node.load();
	if (remote_node == *local_node_) {
		// It's me! ...wave hello!
		discovery->send_message(Discovery::Message::WAVE, local_node_->serialise());
	} else {
		std::shared_ptr<const Node> node = XapiandManager::manager->touch_node(remote_node.name(), remote_node.region);
		if (node) {
			if (remote_node == *node) {
				discovery->send_message(Discovery::Message::WAVE, local_node_->serialise());
			} else {
				discovery->send_message(Discovery::Message::SNEER, remote_node.serialise());
			}
		} else {
			discovery->send_message(Discovery::Message::WAVE, local_node_->serialise());
		}
	}
}


void
DiscoveryServer::wave(const std::string& message)
{
	_wave(false, message);
}


void
DiscoveryServer::sneer(const std::string& message)
{
	if (XapiandManager::manager->state.load() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	auto local_node_ = local_node.load();
	if (remote_node == *local_node_) {
		if (XapiandManager::manager->node_name.empty()) {
			L_DISCOVERY("Node name %s already taken. Retrying other name...", local_node_->name());
			XapiandManager::manager->reset_state();
		} else {
			L_WARNING("Cannot join the party. Node name %s already taken!", local_node_->name());
			XapiandManager::manager->state.store(XapiandManager::State::BAD);
			local_node = std::make_shared<const Node>();
			XapiandManager::manager->shutdown_asap.store(epoch::now<>());
			XapiandManager::manager->shutdown_sig(0);
		}
	}
}


void
DiscoveryServer::enter(const std::string& message)
{
	if (XapiandManager::manager->state.load() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	std::shared_ptr<const Node> remote_node = std::make_shared<Node>(Node::unserialise(&p, p_end));

	XapiandManager::manager->put_node(remote_node);

	L_INFO("Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (1)", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
}


void
DiscoveryServer::bye(const std::string& message)
{
	if (XapiandManager::manager->state.load() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);

	XapiandManager::manager->drop_node(remote_node.name());
	L_INFO("Node %s left the party!", remote_node.name());
	auto local_node_ = local_node.load();
	auto local_node_copy = std::make_unique<Node>(*local_node_);
	local_node_copy->regions = -1;
	local_node = std::shared_ptr<const Node>(local_node_copy.release());
	XapiandManager::manager->get_region();
}


void
DiscoveryServer::db_updated(const std::string& message)
{
	if (XapiandManager::manager->state.load() != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	long long remote_mastery_level = unserialise_length(&p, p_end);
	auto index_path = std::string(unserialise_string(&p, p_end));

	DatabaseHandler db_handler(Endpoints(Endpoint(index_path)), DB_OPEN);
	long long mastery_level = db_handler.get_mastery_level();
	if (mastery_level == -1) {
		return;
	}

	if (mastery_level > remote_mastery_level) {
		L_DISCOVERY("Mastery of remote's %s wins! (local:%llx > remote:%llx) - Updating!", index_path, mastery_level, remote_mastery_level);

		std::shared_ptr<const Node> remote_node = std::make_shared<Node>(Node::unserialise(&p, p_end));

		if (XapiandManager::manager->put_node(remote_node)) {
			L_INFO("Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)! (4)", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
		}

		Endpoint local_endpoint(index_path);
		Endpoint remote_endpoint(index_path, remote_node.get());
#ifdef XAPIAND_CLUSTERING
		// Replicate database from the other node
		L_INFO("Request syncing database from %s...", remote_node->name());
		auto ret = XapiandManager::manager->trigger_replication(remote_endpoint, local_endpoint);
		if (ret.get()) {
			L_INFO("Replication triggered!");
		}
#endif
	} else if (mastery_level != remote_mastery_level) {
		L_DISCOVERY("Mastery of local's %s wins! (local:%llx <= remote:%llx) - Ignoring update!", index_path, mastery_level, remote_mastery_level);
	}
}


void
DiscoveryServer::io_accept_cb(ev::io &watcher, int revents)
{
	L_CALL("DiscoveryServer::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d, fd:%d}", revents, readable_revents(revents), discovery->sock, watcher.fd);

	int fd = discovery->sock;
	if (fd == -1) {
		return;
	}
	ignore_unused(watcher);
	assert(fd == watcher.fd || fd == -1);

	L_DEBUG_HOOK("DiscoveryServer::io_accept_cb", "DiscoveryServer::io_accept_cb(<watcher>, 0x%x (%s)) {fd:%d}", revents, readable_revents(revents), fd);

	if (EV_ERROR & revents) {
		L_EV("ERROR: got invalid discovery event {fd:%d}: %s", fd, strerror(errno));
		return;
	}

	L_EV_BEGIN("DiscoveryServer::io_accept_cb:BEGIN");

	if (revents & EV_READ) {
		while (true) {
			try {
				std::string message;
				Discovery::Message type = static_cast<Discovery::Message>(discovery->get_message(message, static_cast<char>(Discovery::Message::MAX)));
				if (type != Discovery::Message::HEARTBEAT) {
					L_DISCOVERY(">> get_message(%s)", Discovery::MessageNames(type));
				}
				L_DISCOVERY_PROTO("message: %s", repr(message));
				discovery_server(type, message);
			} catch (const DummyException&) {
				break;  // No message.
			} catch (const BaseException& exc) {
				L_WARNING("WARNING: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
				break;
			} catch (...) {
				L_EV_END("DiscoveryServer::io_accept_cb:END %lld", SchedulerQueue::now);
				throw;
			}
		}
	}

	L_EV_END("DiscoveryServer::io_accept_cb:END %lld", SchedulerQueue::now);
}

#endif
