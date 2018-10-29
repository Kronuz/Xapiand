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

#include "discovery.h"

#ifdef XAPIAND_CLUSTERING

#include "cassert.hh"            // for assert

#include "database.h"            // for DatabaseUpdate
#include "database_handler.h"
#include "epoch.hh"              // for epoch::now
#include "ignore_unused.h"
#include "manager.h"
#include "namegen.h"             // for name_generator
#include "node.h"                // for Node, local_node
#include "readable_revents.hh"   // for readable_revents
#include "repr.hh"               // for repr
#include "utype.hh"              // for toUType


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_DISCOVERY
// #define L_DISCOVERY L_SALMON


using dispatch_func = void (Discovery::*)(Discovery::Message, const std::string&);


Discovery::Discovery(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_, const std::string& group_)
	: UDP(port_, "Discovery", XAPIAND_DISCOVERY_PROTOCOL_VERSION, group_),
	  Worker(parent_, ev_loop_, ev_flags_),
	  discovery(*ev_loop)
{
	io.set<Discovery, &Discovery::io_accept_cb>(this);

	discovery.set<Discovery, &Discovery::discovery_cb>(this);

	L_OBJ("CREATED DISCOVERY");
}


Discovery::~Discovery()
{
	destroyer();

	L_OBJ("DELETED DISCOVERY");
}


void
Discovery::shutdown_impl(time_t asap, time_t now)
{
	L_CALL("Discovery::shutdown_impl(%d, %d)", (int)asap, (int)now);

	Worker::shutdown_impl(asap, now);

	destroy();

	if (now != 0) {
		detach();
	}
}


void
Discovery::destroy_impl()
{
	L_CALL("Discovery::destroy_impl()");

	destroyer();
}


void
Discovery::destroyer()
{
	L_CALL("Discovery::destroyer()");

	discovery.stop();
	L_EV("Stop discovery's discovery event");

	io.stop();
	L_EV("Stop discovery's io event");
}


void
Discovery::send_message(Message type, const std::string& message)
{
	L_CALL("Discovery::send_message(%s, <message>)", MessageNames(type));

	L_DISCOVERY_PROTO("<< send_message (%s): %s", MessageNames(type), repr(message));
	UDP::send_message(toUType(type), message);
}


void
Discovery::io_accept_cb(ev::io &watcher, int revents)
{
	L_CALL("Discovery::io_accept_cb(<watcher>, 0x%x (%s)) {sock:%d, fd:%d}", revents, readable_revents(revents), sock, watcher.fd);

	int fd = sock;
	if (fd == -1) {
		return;
	}
	ignore_unused(watcher);
	assert(fd == watcher.fd || fd == -1);

	L_DEBUG_HOOK("Discovery::io_accept_cb", "Discovery::io_accept_cb(<watcher>, 0x%x (%s)) {fd:%d}", revents, readable_revents(revents), fd);

	if (EV_ERROR & revents) {
		L_EV("ERROR: got invalid discovery event {fd:%d}: %s", fd, strerror(errno));
		return;
	}

	L_EV_BEGIN("Discovery::io_accept_cb:BEGIN");

	if (revents & EV_READ) {
		while (true) {
			try {
				std::string message;
				auto raw_type = get_message(message, static_cast<char>(Message::MAX));
				if (raw_type == '\xff') {
					break;  // no message
				}
				Message type = static_cast<Message>(raw_type);
				L_DISCOVERY_PROTO(">> get_message (%s): %s", MessageNames(type), repr(message));
				discovery_server(type, message);
			} catch (const BaseException& exc) {
				L_WARNING("WARNING: %s", *exc.get_context() ? exc.get_context() : "Unkown Exception!");
				break;
			} catch (...) {
				L_EV_END("Discovery::io_accept_cb:END %lld", SchedulerQueue::now);
				throw;
			}
		}
	}

	L_EV_END("Discovery::io_accept_cb:END %lld", SchedulerQueue::now);
}


void
Discovery::discovery_server(Message type, const std::string& message)
{
	L_CALL("Discovery::discovery_server(%s, <message>)", MessageNames(type));

	static const dispatch_func dispatch[] = {
		&Discovery::hello,
		&Discovery::wave,
		&Discovery::sneer,
		&Discovery::enter,
		&Discovery::bye,
		&Discovery::db_updated,
	};
	if (static_cast<size_t>(type) >= sizeof(dispatch) / sizeof(dispatch[0])) {
		std::string errmsg("Unexpected message type ");
		errmsg += std::to_string(toUType(type));
		THROW(InvalidArgumentError, errmsg);
	}
	(this->*(dispatch[toUType(type)]))(type, message);
}


void
Discovery::hello(Message type, const std::string& message)
{
	L_CALL("Discovery::hello(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = std::make_shared<const Node>(Node::unserialise(&p, p_end));
	L_DISCOVERY(">> %s [from %s]", MessageNames(type), remote_node->name());

	auto local_node_ = Node::local_node();
	if (!Node::is_equal(remote_node, local_node_)) {
		auto node = Node::touch_node(remote_node);
		if (node) {
			if (Node::is_equal(remote_node, node)) {
				send_message(Message::WAVE, local_node_->serialise());
			} else {
				send_message(Message::SNEER, remote_node->serialise());
			}
		} else {
			send_message(Message::WAVE, local_node_->serialise());
		}
	}
}


void
Discovery::wave(Message type, const std::string& message)
{
	L_CALL("Discovery::wave(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = std::make_shared<const Node>(Node::unserialise(&p, p_end));
	L_DISCOVERY(">> %s [from %s]", MessageNames(type), remote_node->name());

	auto put = Node::put_node(remote_node);
	remote_node = put.first;
	if (put.second) {
		L_INFO("Node %s is at the party on ip:%s, tcp:%d (http), tcp:%d (xapian)!", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
	}

	// After receiving WAVE, flag as WAITING_MORE so it waits just a little longer
	// (prevent it from switching to slow waiting)
	auto waiting = XapiandManager::State::WAITING;
	if (XapiandManager::manager->state.compare_exchange_strong(waiting, XapiandManager::State::WAITING_MORE)) {
		// L_DEBUG("State changed: %s -> %s", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	}
}


void
Discovery::sneer(Message type, const std::string& message)
{
	L_CALL("Discovery::sneer(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	if (XapiandManager::manager->state != XapiandManager::State::RESET &&
		XapiandManager::manager->state != XapiandManager::State::WAITING &&
		XapiandManager::manager->state != XapiandManager::State::WAITING_MORE &&
		XapiandManager::manager->state != XapiandManager::State::JOINING) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> %s [from %s]", MessageNames(type), remote_node.name());

	auto local_node_ = Node::local_node();
	if (remote_node == *local_node_) {
		if (XapiandManager::manager->node_name.empty()) {
			L_DISCOVERY("Node name %s already taken. Retrying other name...", local_node_->name());
			XapiandManager::manager->reset_state();
		} else {
			XapiandManager::manager->state.store(XapiandManager::State::BAD);
			L_WARNING("Cannot join the party. Node name %s already taken!", local_node_->name());
			Node::local_node(std::make_shared<const Node>());
			XapiandManager::manager->shutdown_asap.store(epoch::now<>());
			XapiandManager::manager->shutdown_sig(0);
		}
	}
}


void
Discovery::enter(Message type, const std::string& message)
{
	L_CALL("Discovery::enter(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = std::make_shared<const Node>(Node::unserialise(&p, p_end));
	L_DISCOVERY(">> %s [from %s]", MessageNames(type), remote_node->name());

	auto put = Node::put_node(remote_node);
	remote_node = put.first;
	if (put.second) {
		L_INFO("Node %s joined the party on ip:%s, tcp:%d (http), tcp:%d (xapian)!", remote_node->name(), remote_node->host(), remote_node->http_port, remote_node->binary_port);
	}
}


void
Discovery::bye(Message type, const std::string& message)
{
	L_CALL("Discovery::bye(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	if (XapiandManager::manager->state != XapiandManager::State::JOINING &&
		XapiandManager::manager->state != XapiandManager::State::SETUP &&
		XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	Node remote_node = Node::unserialise(&p, p_end);
	L_DISCOVERY(">> %s [from %s]", MessageNames(type), remote_node.name());

	Node::drop_node(remote_node.name());

	auto leader_node_ = Node::leader_node();
	if (*leader_node_ == remote_node) {
		L_INFO("Leader node %s left the party!", remote_node.name());

		Node::leader_node(std::make_shared<const Node>());
		XapiandManager::manager->renew_leader();
	} else {
		L_INFO("Node %s left the party!", remote_node.name());
	}
}


void
Discovery::db_updated(Message type, const std::string& message)
{
	L_CALL("Discovery::db_updated(%s, <message>) {state:%s}", MessageNames(type), XapiandManager::StateNames(XapiandManager::manager->state.load()));
	ignore_unused(type);

	if (XapiandManager::manager->state != XapiandManager::State::READY) {
		return;
	}

	const char *p = message.data();
	const char *p_end = p + message.size();

	auto remote_node = std::make_shared<const Node>(Node::unserialise(&p, p_end));

	auto local_node_ = Node::local_node();
	if (Node::is_equal(remote_node, local_node_)) {
		// It's just me, do nothing!
		return;
	}

	auto path = std::string(p, p_end);

	bool replicated = false;

	if (path == ".") {
		// Cluster database is always updated
		replicated = true;
	}

	if (!replicated && exists(std::string(path) + "/iamglass")) {
		// If database is already there, its also always updated
		replicated = true;
	}

	if (!replicated) {
		// Otherwise, check if the local node resolves as replicator
		auto nodes = XapiandManager::manager->resolve_index_nodes(path);
		for (const auto& node : nodes) {
			if (Node::is_equal(node, local_node_)) {
				replicated = true;
				break;
			}
		}
	}

	if (replicated) {
		L_DISCOVERY(">> %s [from %s]: %s", MessageNames(type), remote_node->name(), repr(path));

		auto node = Node::touch_node(remote_node);
		if (node) {
			Endpoint remote_endpoint(path, node.get());
			if (!remote_endpoint.is_local()) {
				Endpoint local_endpoint(path);
				// Replicate database from the other node
				L_INFO("Request syncing database [%s]...", node->name());
				auto ret = XapiandManager::manager->trigger_replication(remote_endpoint, local_endpoint);
				if (ret.get()) {
					L_INFO("Replication triggered!");
				}
			}
		}
	}
}


void
Discovery::discovery_cb(ev::timer&, int revents)
{
	auto state = XapiandManager::manager->state.load();

	L_CALL("Discovery::discovery_cb(<watcher>, 0x%x (%s)) {state:%s}", revents, readable_revents(revents), XapiandManager::StateNames(state));

	ignore_unused(revents);

	L_EV_BEGIN("Discovery::discovery_cb:BEGIN");

	switch (state) {
		case XapiandManager::State::RESET: {
			auto local_node_ = Node::local_node();
			auto node_copy = std::make_unique<Node>(*local_node_);
			std::string drop = node_copy->name();

			if (XapiandManager::manager->node_name.empty()) {
				node_copy->name(name_generator());
			} else {
				node_copy->name(XapiandManager::manager->node_name);
			}
			Node::local_node(std::shared_ptr<const Node>(node_copy.release()));

			if (!drop.empty()) {
				Node::drop_node(drop);
			}

			local_node_ = Node::local_node();
			auto reset = XapiandManager::State::RESET;
			if (XapiandManager::manager->state.compare_exchange_strong(reset, XapiandManager::State::WAITING)) {
				// L_DEBUG("State changed: %s -> %s", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::manager->state.load()));
			}
			L_INFO("Advertising as %s...", local_node_->name());
			send_message(Message::HELLO, local_node_->serialise());
			break;
		}
		case XapiandManager::State::WAITING: {
			// We're here because no one sneered nor entered during
			// WAITING_FAST, wait longer then...

			discovery.repeat = WAITING_SLOW;
			discovery.again();
			L_EV("Reset discovery's discovery event (%f)", discovery.repeat);

			auto waiting = XapiandManager::State::WAITING;
			if (XapiandManager::manager->state.compare_exchange_strong(waiting, XapiandManager::State::WAITING_MORE)) {
				// L_DEBUG("State changed: %s -> %s", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::manager->state.load()));
			}
			break;
		}
		case XapiandManager::State::WAITING_MORE: {
			discovery.stop();
			L_EV("Stop discovery's discovery event");

			auto waiting_more = XapiandManager::State::WAITING_MORE;
			if (XapiandManager::manager->state.compare_exchange_strong(waiting_more, XapiandManager::State::JOINING)) {
				// L_DEBUG("State changed: %s -> %s", XapiandManager::StateNames(state), XapiandManager::StateNames(XapiandManager::manager->state.load()));
			}

			auto local_node_ = Node::local_node();
			send_message(Message::ENTER, local_node_->serialise());

			XapiandManager::manager->join_cluster();
			break;
		}
		default: {
			break;
		}
	}

	L_EV_END("Discovery::discovery_cb:END");
}

void
Discovery::signal_db_update(const DatabaseUpdate& update)
{
	L_CALL("Discovery::signal_db_update(%s)", repr(endpoint.to_string()));

	auto local_node_ = Node::local_node();
	send_message(Message::DB_UPDATED,
		local_node_->serialise() +   // The node where the index is at
		update.endpoint.path);  // The path of the index
}


void
Discovery::start()
{
	L_CALL("Discovery::start()");

	discovery.start(0, WAITING_FAST);
	L_EV("Start discovery's discovery exploring event (%f)", discovery.repeat);

	io.start(sock, ev::READ);
	L_EV("Start discovery's server accept event (sock=%d)", sock);

	L_DISCOVERY("Discovery was started! (exploring)");
}


void
Discovery::stop()
{
	L_CALL("Discovery::stop()");

	discovery.stop();
	L_EV("Stop discovery's discovery event");

	auto local_node_ = Node::local_node();
	send_message(Message::BYE, local_node_->serialise());

	io.stop();
	L_EV("Stop discovery's server accept event");

	L_DISCOVERY("Discovery was stopped!");
}


std::string
Discovery::getDescription() const noexcept
{
	L_CALL("Raft::getDescription()");

	return "UDP:" + std::to_string(port) + " (" + description + " v" + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MAJOR_VERSION) + "." + std::to_string(XAPIAND_DISCOVERY_PROTOCOL_MINOR_VERSION) + ")";
}

#endif
