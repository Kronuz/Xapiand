/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
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

#pragma once

#include "config.h"  // for XAPIAND_CLUSTERING

#ifdef XAPIAND_CLUSTERING

// XapiandRaftDelegate -- the seam between the generic cluster::Raft consensus library and
// Xapiand: it implements the ~16 RaftDelegate hooks over Xapiand's Node registry, the
// cluster-join state machine, and the manager's command dispatch. The node type is
// std::shared_ptr<const Node> (Xapiand's zero-copy node handle), so consensus never copies
// a node record. broadcast is injected (a std::function set by the transport adapter that
// maps a cluster::RaftMessage to its DiscoveryMessage wire value + bus.send), so this
// header does not depend on discovery.h -- no circular include.

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

// IMPORTANT include order: the Xapiand headers pull in libev (ev/ev.h), whose `enum
// EV_ERROR` collides with macOS <sys/event.h>'s `#define EV_ERROR` that Asio pulls in. libev
// must be included FIRST (its enum defined before the macro shadows the name), exactly as
// manager.cc orders ev before http_asio.h. So: Node/manager (ev) before raft.h (Asio).
#include "node.h"        // Node
#include "manager.h"     // XapiandManager (State, Command, get_state/exchange_state/...)

#include "raft.h"        // cluster::RaftDelegate / RaftMessage (Kronuz/cluster) -- Asio


class XapiandRaftDelegate : public cluster::RaftDelegate<std::shared_ptr<const Node>> {
	using NodePtr = std::shared_ptr<const Node>;

	std::function<void(cluster::RaftMessage, const std::string&)> broadcast_fn;

public:
	explicit XapiandRaftDelegate(std::function<void(cluster::RaftMessage, const std::string&)> broadcast_fn_)
		: broadcast_fn(std::move(broadcast_fn_)) {}

	// --- transport ---
	void broadcast(cluster::RaftMessage msg, const std::string& payload) override {
		broadcast_fn(msg, payload);
	}

	// --- node model / serialization (shared_ptr<const Node>, empty == null) ---
	NodePtr local_node() override {
		return Node::get_local_node();
	}
	std::string serialise(const NodePtr& n) override {
		return n ? n->serialise() : Node().serialise();
	}
	std::optional<NodePtr> parse_node(const char** p, const char* end) override {
		auto node = Node::unserialise(p, end);
		auto put = Node::touch_node(node, false);   // activate=false, matching the raft handlers
		if (!put.first) {
			return std::nullopt;
		}
		return put.first;
	}
	std::string node_id(const NodePtr& n) override {
		return n ? n->lower_name() : std::string();
	}

	// --- membership / quorum ---
	std::size_t total_nodes() override { return Node::total_nodes(); }
	std::size_t alive_nodes() override { return Node::alive_nodes(); }
	bool quorum(std::size_t total, std::size_t count) override { return Node::quorum(total, count); }
	bool prefers(const NodePtr& a, const NodePtr& b) override { return Node::is_superset(a, b); }
	bool is_alive(const std::string& id) override { return Node::is_alive(id); }

	// --- cluster-lifecycle gates + hooks ---
	bool active() override {
		switch (XapiandManager::get_state()) {
			case XapiandManager::State::JOINING:
			case XapiandManager::State::SETUP:
			case XapiandManager::State::READY:
				return true;
			default:
				return false;
		}
	}
	bool ready() override { return XapiandManager::get_state() == XapiandManager::State::READY; }
	bool joining() override { return XapiandManager::get_state() == XapiandManager::State::JOINING; }

	void ensure_setup() override {
		if (XapiandManager::exchange_state(XapiandManager::State::JOINING, XapiandManager::State::SETUP,
				std::chrono::seconds(4), "Node setup is taking too long...", "Node setup is finally done!")) {
			XapiandManager::setup_node();
		}
	}
	void set_leader(const NodePtr& leader) override {
		// Xapiand's _raft_set_leader_node passes an empty (non-null) Node for "no leader".
		auto node = leader ? leader : std::make_shared<const Node>();
		if (Node::set_leader_node(node)) {
			XapiandManager::dispatch_command(XapiandManager::Command::RAFT_SET_LEADER_NODE);
		}
	}
	void apply(const std::string& command) override {
		XapiandManager::dispatch_command(XapiandManager::Command::RAFT_APPLY_COMMAND, command);
	}
};

#endif  // XAPIAND_CLUSTERING
