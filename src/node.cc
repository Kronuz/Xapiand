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

#include "endpoint.h"

#include <cstdlib>              // for atoi

#include "hashes.hh"            // for fnv1ah32::hash
#include "length.h"             // for serialise_length, unserialise_length, ser...
#include "log.h"                // for L_CALL
#include "logger.h"             // for Logging::tab_title, Logging::badge
#include "opts.h"               // for opts
#include "serialise.h"          // for Serialise
#include "string.hh"            // for string::format, string::lower
#include "xapian.h"             // for SerialisationError


#define L_NODE_NODES(args...)

#ifndef L_NODE_NODES
#define L_NODE_NODES(args...) \
	L_SLATE_GREY(args); \
	for (const auto& _ : _nodes) { \
		L_SLATE_GREY("    nodes[{}] -> {{index:{}, name:{}, host:{}, http_port:{}, remote_port:{}, replication_port:{}, touched:{}}}{}{}{}", \
			_.first, _.second->idx, repr(_.second->name()), repr(_.second->host()), _.second->http_port, _.second->remote_port, _.second->replication_port, _.second->touched.load(std::memory_order_relaxed), \
			Node::is_active(_.second) ? " active" : "", \
			Node::is_local(_.second) ? " (local)" : "", \
			Node::is_leader(_.second) ? " (leader)" : ""); \
	}
#endif


static inline void
set_as_title(const std::shared_ptr<const Node>& node)
{
	if (node && !node->name().empty()) {
		// Set window title
		Logging::tab_title(node->idx
			? string::format("[{}] {}", node->idx, node->name())
			: node->name());

		// Set iTerm2 badge
		Logging::badge(node->name());

		// Set tab color
		auto col = node->col();
		Logging::tab_rgb(col.red(), col.green(), col.blue());
	}
}


std::string
Node::serialise() const
{
	return _name.empty()
		? ""
		: serialise_length(_addr.sin_addr.s_addr) +
			serialise_length(http_port) +
			serialise_length(remote_port) +
			serialise_length(replication_port) +
			serialise_length(idx) +
			serialise_string(_name);
}


Node
Node::unserialise(const char **p, const char *end)
{
	const char *ptr = *p;

	Node node;

	node._addr.sin_family = AF_INET;
	node._addr.sin_addr.s_addr = unserialise_length(&ptr, end);
	node.http_port = unserialise_length(&ptr, end);
	node.remote_port = unserialise_length(&ptr, end);
	node.replication_port = unserialise_length(&ptr, end);
	node.idx = unserialise_length(&ptr, end);
	node._name = unserialise_string(&ptr, end);
	if (node._name.empty()) {
		throw Xapian::SerialisationError("Bad Node: No name");
	}

	node._lower_name = string::lower(node._name);
	node._host = inet_ntop(node._addr);

	*p = ptr;

	return node;
}


std::string
Node::__repr__() const
{
	return string::format("<Node {{index:{}, name:{}, host:{}, http_port:{}, remote_port:{}, replication_port:{}, touched:{}}}{}{}{}>",
		idx, repr(name()), repr(host()), http_port, remote_port, replication_port, touched.load(std::memory_order_relaxed),
		is_active() ? " (active)" : "",
		is_local() ? " (local)" : "",
		is_leader() ? " (leader)" : "");
}

#ifdef XAPIAND_CLUSTERING
std::string
Node::dump_nodes(int level)
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::string ret;

	for (auto& node : nodes()) {
		ret += indent;
		ret += node->__repr__();
		ret.push_back('\n');
	}

	return ret;
}
#endif


atomic_shared_ptr<const Node> Node::_local_node{std::make_shared<const Node>()};
atomic_shared_ptr<const Node> Node::_leader_node{std::make_shared<const Node>()};


#ifndef XAPIAND_CLUSTERING

std::shared_ptr<const Node>
Node::local_node(std::shared_ptr<const Node> node)
{
	if (node) {
		set_as_title(node);
		_local_node.store(node, std::memory_order_relaxed);
	}
	return _local_node.load(std::memory_order_relaxed);
}


std::shared_ptr<const Node>
Node::leader_node(std::shared_ptr<const Node> node)
{
	if (node) {
		_leader_node.store(node, std::memory_order_relaxed);
	}
	return _leader_node.load(std::memory_order_relaxed);
}

#else

std::mutex Node::_nodes_mtx;
std::unordered_map<std::string, std::shared_ptr<const Node>> Node::_nodes;
std::vector<std::shared_ptr<const Node>> Node::_nodes_indexed;

std::atomic_size_t Node::_total_nodes;
std::atomic_size_t Node::_active_nodes;
std::atomic_size_t Node::_indexed_nodes;


inline void
Node::_update_nodes(const std::shared_ptr<const Node>& node)
{
	auto local_node_ = _local_node.load(std::memory_order_relaxed);
	if (node != local_node_) {
		if (node->lower_name() == local_node_->lower_name()) {
			if (node->idx != local_node_->idx) {
				set_as_title(node);
			}
			_local_node.store(node, std::memory_order_relaxed);
		}
	}

	auto leader_node_ = _leader_node.load(std::memory_order_relaxed);
	if (node != leader_node_) {
		if (node->lower_name() == leader_node_->lower_name()) {
			_leader_node.store(node, std::memory_order_relaxed);
		}
	}

	size_t cnt = 0;
	_nodes_indexed.clear();
	for (const auto& node_pair : _nodes) {
		const auto& node_ref = node_pair.second;
		if (is_active(node_ref)) {
			++cnt;
		}
		auto idx_ = node_ref->idx;
		if (idx_) {
			if (_nodes_indexed.size() < idx_) {
				_nodes_indexed.resize(idx_);
			}
			_nodes_indexed[idx_ - 1] = node_ref;
		}
	}
	_active_nodes.store(cnt, std::memory_order_relaxed);
	_total_nodes.store(_nodes.size(), std::memory_order_relaxed);
	_indexed_nodes.store(_nodes_indexed.size(), std::memory_order_relaxed);
}


std::shared_ptr<const Node>
Node::local_node(std::shared_ptr<const Node> node)
{
	if (node) {
		L_CALL("Node::local_node({})", node->__repr__());

		auto now = epoch::now<std::chrono::milliseconds>();
		node->touched.store(now, std::memory_order_relaxed);
		set_as_title(node);
		_local_node.store(node, std::memory_order_relaxed);
		auto leader_node_ = _leader_node.load(std::memory_order_relaxed);
		if (node->lower_name() == leader_node_->lower_name()) {
			_leader_node.store(node, std::memory_order_relaxed);
		}

		std::lock_guard<std::mutex> lk(_nodes_mtx);

		auto it = _nodes.find(node->lower_name());
		if (it != _nodes.end()) {
			auto& node_ref = it->second;
			node_ref = node;
		}

		auto idx_ = node->idx;
		if (idx_) {
			if (_nodes_indexed.size() < idx_) {
				_nodes_indexed.resize(idx_);
			}
			_nodes_indexed[idx_ - 1] = node;
		}
		_indexed_nodes = _nodes_indexed.size();

		L_NODE_NODES("local_node({})", node->__repr__());
	} else {
		L_CALL("Node::local_node()");
	}
	return _local_node.load(std::memory_order_relaxed);
}


std::shared_ptr<const Node>
Node::leader_node(std::shared_ptr<const Node> node)
{
	if (node) {
		L_CALL("Node::leader_node({})", node->__repr__());

		auto now = epoch::now<std::chrono::milliseconds>();
		node->touched.store(now, std::memory_order_relaxed);
		_leader_node.store(node, std::memory_order_relaxed);
		auto local_node_ = _local_node.load(std::memory_order_relaxed);
		if (node->lower_name() == local_node_->lower_name()) {
			if (node->idx != local_node_->idx) {
				set_as_title(node);
			}
			_local_node.store(node, std::memory_order_relaxed);
		}

		std::lock_guard<std::mutex> lk(_nodes_mtx);

		auto it = _nodes.find(node->lower_name());
		if (it != _nodes.end()) {
			auto& node_ref = it->second;
			node_ref = node;
		}

		auto idx_ = node->idx;
		if (idx_) {
			if (_nodes_indexed.size() < idx_) {
				_nodes_indexed.resize(idx_);
			}
			_nodes_indexed[idx_ - 1] = node;
		}
		_indexed_nodes = _nodes_indexed.size();

		L_NODE_NODES("leader_node({})", node->__repr__());
	} else {
		L_CALL("Node::leader_node()");
	}
	return _leader_node.load(std::memory_order_relaxed);
}


std::shared_ptr<const Node>
Node::get_node(std::string_view _node_name)
{
	L_CALL("Node::get_node({})", repr(_node_name));

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(string::lower(_node_name));
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		// L_NODE_NODES("get_node({}) -> {}", _node_name, node_ref->__repr__());
		return node_ref;
	}

	L_NODE_NODES("get_node({}) -> nullptr", _node_name);
	return nullptr;
}


std::shared_ptr<const Node>
Node::get_node(size_t idx)
{
	L_CALL("Node::get_node({})", idx);

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	if (idx > 0 && idx <= _nodes_indexed.size()) {
		auto& node_ref = _nodes_indexed[idx - 1];
		// L_NODE_NODES("get_node({}) -> {}", idx, node_ref->__repr__());
		return node_ref;
	}

	L_NODE_NODES("get_node({}) -> nullptr", idx);
	return nullptr;
}


std::pair<std::shared_ptr<const Node>, bool>
Node::touch_node(const Node& node, bool activate)
{
	L_CALL("Node::touch_node({}, {})", node.__repr__(), activate);

	auto now = epoch::now<std::chrono::milliseconds>();

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	size_t idx = 0;

	auto it = _nodes.find(node.lower_name());
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		if (Node::is_superset(node_ref, node)) {
			auto modified = false;
			if (
				(!node_ref->idx && node.idx) ||
				(!node_ref->_addr.sin_addr.s_addr && node._addr.sin_addr.s_addr) ||
				(!node_ref->http_port && node.http_port) ||
				(!node_ref->remote_port && node.remote_port) ||
				(!node_ref->replication_port && node.replication_port)
			) {
				auto node_ref_copy = std::make_unique<Node>(*node_ref);
				if (!node_ref->idx && node.idx) {
					node_ref_copy->idx = node.idx;
					if (node_ref_copy->idx >= 1 && node_ref_copy->idx <= _nodes_indexed.size()) {
						auto& indexed_node = _nodes_indexed[node_ref_copy->idx - 1];
						if (indexed_node && node_ref_copy->lower_name() != indexed_node->lower_name()) {
							L_NODE_NODES("touch_node({}) -> nullptr (1)", node.__repr__());
							return std::make_pair(nullptr, false);
						}
					}
				}
				if (!node_ref->_addr.sin_addr.s_addr && node._addr.sin_addr.s_addr) {
					node_ref_copy->_addr = node._addr;
					node_ref_copy->_host = inet_ntop(node_ref_copy->_addr);
				}
				if (!node_ref->http_port && node.http_port) {
					node_ref_copy->http_port = node.http_port;
				}
				if (!node_ref->remote_port && node.remote_port) {
					node_ref_copy->remote_port = node.remote_port;
				}
				if (!node_ref->replication_port && node.replication_port) {
					node_ref_copy->replication_port = node.replication_port;
				}
				node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
				_update_nodes(node_ref);
				modified = true;
			}
			if (activate || is_active(node_ref)) {
				if (node_ref->touched.exchange(now, std::memory_order_relaxed) == 0) {
					modified = true;
				}
			}
			L_NODE_NODES("touch_node({}) -> {} (1)", node_ref->__repr__(), modified);
			return std::make_pair(node_ref, modified);
		} else if (is_active(node_ref)) {
			L_NODE_NODES("touch_node({}) -> nullptr (2)", node.__repr__());
			return std::make_pair(nullptr, false);
		}
		idx = node_ref->idx;
	}

	auto new_node_copy = std::make_unique<Node>(node);
	if (!new_node_copy->idx && idx) {
		new_node_copy->idx = idx;
		if (new_node_copy->idx >= 1 && new_node_copy->idx <= _nodes_indexed.size()) {
			auto& indexed_node = _nodes_indexed[new_node_copy->idx - 1];
			if (indexed_node && new_node_copy->lower_name() != indexed_node->lower_name()) {
				L_NODE_NODES("touch_node({}) -> nullptr (3)", new_node_copy->__repr__());
				return std::make_pair(nullptr, false);
			}
		}
	}
	auto new_node = std::shared_ptr<const Node>(new_node_copy.release());
	if (activate || is_active(new_node)) {
		new_node->touched.store(now, std::memory_order_relaxed);
	}
	_nodes[new_node->lower_name()] = new_node;
	_update_nodes(new_node);

	L_NODE_NODES("touch_node({}) -> true", new_node->__repr__());
	return std::make_pair(new_node, true);
}


void
Node::drop_node(std::string_view _node_name)
{
	L_CALL("Node::drop_node({})", repr(_node_name));

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(string::lower(_node_name));
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		node_ref->touched.store(0, std::memory_order_relaxed);
		auto node_ref_copy = std::make_unique<Node>(*node_ref);
		node_ref_copy->_addr = sockaddr_in{};
		node_ref_copy->_host.clear();
		node_ref_copy->http_port = 0;
		node_ref_copy->remote_port = 0;
		node_ref_copy->replication_port = 0;
		node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
		_update_nodes(node_ref);
	}

	L_NODE_NODES("drop_node({})", _node_name);
}


void
Node::reset()
{
	L_CALL("Node::reset()");

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	_nodes.clear();
}


std::vector<std::shared_ptr<const Node>>
Node::nodes()
{
	L_CALL("Node::nodes()");

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	std::vector<std::shared_ptr<const Node>> nodes;
	for (const auto& node_pair : _nodes) {
		nodes.push_back(node_pair.second);
	}

	return nodes;
}

#endif


color
Node::col() const
{
	double hue = fnv1ah64::hash(_name);
	hue = hue + (hue / 0.618033988749895);
	double saturation = 0.6;
	double value = 0.75;
	double red, green, blue;
	hsv2rgb(hue, saturation, value, red, green, blue);
	return color(red * 255, green * 255, blue * 255);
}


bool
Node::is_simmilar(const Node& other) const
{
	return (this == &other || (
		(!idx || !other.idx || idx == other.idx) &&
		(!_addr.sin_addr.s_addr || !other._addr.sin_addr.s_addr || _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr) &&
		(!http_port || !other.http_port || http_port == other.http_port) &&
		(!remote_port || !other.remote_port || remote_port == other.remote_port) &&
		(!replication_port || !other.replication_port || replication_port == other.replication_port) &&
		_lower_name == other._lower_name
	));
}


bool
Node::is_superset(const Node& other) const
{
	return (this == &other || (
		(!idx || !other.idx || idx == other.idx) &&
		(!_addr.sin_addr.s_addr || _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr) &&
		(!http_port || http_port == other.http_port) &&
		(!remote_port || remote_port == other.remote_port) &&
		(!replication_port || replication_port == other.replication_port) &&
		_lower_name == other._lower_name
	));
}


bool
Node::is_subset(const Node& other) const
{
	return (this == &other || (
		(!idx || !other.idx || idx == other.idx) &&
		(!other._addr.sin_addr.s_addr || _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr) &&
		(!other.http_port || http_port == other.http_port) &&
		(!other.remote_port || remote_port == other.remote_port) &&
		(!other.replication_port || replication_port == other.replication_port) &&
		_lower_name == other._lower_name
	));
}
