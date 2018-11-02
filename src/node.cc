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

#include "endpoint.h"

#include <cstdlib>              // for atoi
#include <xapian.h>             // for SerialisationError

#include "length.h"             // for serialise_length, unserialise_length, ser...
#include "log.h"                // for L_CALL
#include "logger.h"             // for Logging::tab_title, Logging::badge
#include "opts.h"               // for opts
#include "serialise.h"          // for Serialise
#include "string.hh"            // for string::Number


#define L_NODE_NODES(args...)

#ifndef L_NODE_NODES
#define L_NODE_NODES(args...) \
	L_SLATE_GREY(args); \
	for (const auto& _ : _nodes) { \
		L_SLATE_GREY("    nodes[%s] -> {index:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}%s%s", \
			_.first, _.second->idx, _.second->name(), _.second->http_port, _.second->binary_port, _.second->touched, \
			Node::is_local(_.second) ? " (local)" : "", \
			Node::is_leader(_.second) ? " (leader)" : ""); \
	}
#endif


static inline void
set_as_title(const std::shared_ptr<const Node>& node)
{
	if (node && !node->name().empty()) {
		auto title = node->idx
			? string::format("[%d] %s", node->idx, node->name())
			: node->name();
		Logging::badge(node->name());
		Logging::tab_title(title, !!node->idx);
	}
}


std::string
Node::serialise() const
{
	return _name.empty()
		? ""
		: serialise_length(_addr.sin_addr.s_addr) +
			serialise_length(http_port) +
			serialise_length(binary_port) +
			serialise_length(idx) +
			serialise_string(_name);
}


Node
Node::unserialise(const char **p, const char *end)
{
	const char *ptr = *p;

	Node node;

	node._addr.sin_addr.s_addr = unserialise_length(&ptr, end);
	node.http_port = unserialise_length(&ptr, end);
	node.binary_port = unserialise_length(&ptr, end);
	node.idx = unserialise_length(&ptr, end);
	node._name = unserialise_string(&ptr, end);
	if (node._name.empty()) {
		throw Xapian::SerialisationError("Bad Node: No name");
	}

	node._lower_name = string::lower(node._name);
	node._host = fast_inet_ntop4(node._addr.sin_addr);

	*p = ptr;

	return node;
}


atomic_shared_ptr<const Node> Node::_local_node{std::make_shared<const Node>()};
atomic_shared_ptr<const Node> Node::_leader_node{std::make_shared<const Node>()};


#ifndef XAPIAND_CLUSTERING

static std::shared_ptr<const Node>
Node::local_node(std::shared_ptr<const Node> node)
{
	atomic_shared_ptr<const Node> _local_node{std::make_shared<const Node>()};
	if (node) {
		set_as_title(node);
		_local_node.store(node);
	}
	return _local_node.load();
}


static std::shared_ptr<const Node>
Node::leader_node(std::shared_ptr<const Node> node)
{
	atomic_shared_ptr<const Node> _leader_node{std::make_shared<const Node>()};
	if (node) {
		_leader_node.store(node);
	}
	return _leader_node.load();
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
	auto local_node_ = _local_node.load();
	if (node->lower_name() == local_node_->lower_name()) {
		if (node->idx != local_node_->idx) {
			set_as_title(node);
		}
		_local_node.store(node);
	}

	auto leader_node_ = _leader_node.load();
	if (node->lower_name() == leader_node_->lower_name()) {
		_leader_node.store(node);
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
	_active_nodes = cnt;
	_total_nodes = _nodes.size();
	_indexed_nodes = _nodes_indexed.size();
}


std::shared_ptr<const Node>
Node::local_node(std::shared_ptr<const Node> node)
{
	if (node) {
		L_CALL("Node::local_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld})", node->idx, node->name(), node->http_port, node->binary_port, node->touched);

		auto now = epoch::now<>();
		auto node_copy = std::make_unique<Node>(*node);
		node_copy->touched = now;
		node = std::shared_ptr<const Node>(node_copy.release());
		set_as_title(node);
		_local_node.store(node);
		auto leader_node_ = _leader_node.load();
		if (node->lower_name() == leader_node_->lower_name()) {
			_leader_node.store(node);
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

		L_NODE_NODES("local_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld})", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
	} else {
		L_CALL("Node::local_node()");
	}
	return _local_node.load();
}


std::shared_ptr<const Node>
Node::leader_node(std::shared_ptr<const Node> node)
{
	if (node) {
		L_CALL("Node::leader_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld})", node->idx, node->name(), node->http_port, node->binary_port, node->touched);

		auto now = epoch::now<>();
		auto node_copy = std::make_unique<Node>(*node);
		node_copy->touched = now;
		node = std::shared_ptr<const Node>(node_copy.release());
		_leader_node.store(node);
		auto local_node_ = _local_node.load();
		if (node->lower_name() == local_node_->lower_name()) {
			if (node->idx != local_node_->idx) {
				set_as_title(node);
			}
			_local_node.store(node);
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

		L_NODE_NODES("leader_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld})", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
	} else {
		L_CALL("Node::leader_node()");
	}
	return _leader_node.load();
}


std::shared_ptr<const Node>
Node::get_node(std::string_view _node_name)
{
	L_CALL("Node::get_node(%s)", repr(_node_name));

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(string::lower(_node_name));
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		// L_NODE_NODES("get_node(%s) -> {idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}", _node_name, node_ref->idx, node_ref->name(), node_ref->http_port, node_ref->binary_port, node_ref->touched);
		return node_ref;
	}

	L_NODE_NODES("get_node(%s) -> nullptr", _node_name);
	return nullptr;
}


std::shared_ptr<const Node>
Node::get_node(size_t idx)
{
	L_CALL("Node::get_node(%zu)", idx);

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	if (idx > 0 && idx <= _nodes_indexed.size()) {
		auto& node_ref = _nodes_indexed[idx - 1];
		// L_NODE_NODES("get_node(%zu) -> {idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}", idx, node_ref->idx, node_ref->name(), node_ref->http_port, node_ref->binary_port, node_ref->touched);
		return node_ref;
	}

	L_NODE_NODES("get_node(%zu) -> nullptr", idx);
	return nullptr;
}


std::pair<std::shared_ptr<const Node>, bool>
Node::put_node(std::shared_ptr<const Node> node, bool touch)
{
	L_CALL("Node::put_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}, %s)", node->idx, node->name(), node->http_port, node->binary_port, node->touched, touch ? "true" : "false");

	auto now = epoch::now<>();

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	size_t idx = 0;

	auto it = _nodes.find(node->lower_name());
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		if (is_equal(node_ref, node)) {
			auto node_copy = std::make_unique<Node>(*node_ref);
			if (touch) {
				node_copy->touched = now;
			}
			if (!node_copy->idx && node->idx) {
				node_copy->idx = node->idx;
				if (node_copy->idx >= 1 && node_copy->idx <= _nodes_indexed.size()) {
					auto& indexed_node = _nodes_indexed[node_copy->idx - 1];
					if (indexed_node && node_copy->lower_name() != indexed_node->lower_name()) {
						L_NODE_NODES("put_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> nullptr (1)", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
						return std::make_pair(nullptr, false);
					}
				}
			}
			node_ref = std::shared_ptr<const Node>(node_copy.release());
			_update_nodes(node_ref);

			L_NODE_NODES("put_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> false (1)", node_ref->idx, node_ref->name(), node_ref->http_port, node_ref->binary_port, node_ref->touched);
			return std::make_pair(node_ref, false);
		} else if (is_active(node_ref)) {
			L_NODE_NODES("put_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> false (2)", node_ref->idx, node_ref->name(), node_ref->http_port, node_ref->binary_port, node_ref->touched);
			return std::make_pair(node_ref, false);
		}
		idx = node_ref->idx;
	}

	auto node_copy = std::make_unique<Node>(*node);
	if (touch) {
		node_copy->touched = now;
	}
	if (!node_copy->idx && idx) {
		node_copy->idx = idx;
		if (node_copy->idx >= 1 && node_copy->idx <= _nodes_indexed.size()) {
			auto& indexed_node = _nodes_indexed[node_copy->idx - 1];
			if (indexed_node && node_copy->lower_name() != indexed_node->lower_name()) {
				L_NODE_NODES("put_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> nullptr (1)", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
				return std::make_pair(nullptr, false);
			}
		}
	}
	node = std::shared_ptr<const Node>(node_copy.release());
	_nodes[node->lower_name()] = node;
	_update_nodes(node);

	L_NODE_NODES("put_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> true", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
	return std::make_pair(node, true);
}


std::shared_ptr<const Node>
Node::touch_node(std::shared_ptr<const Node> node)
{
	L_CALL("Node::touch_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld})", node->idx, node->name(), node->http_port, node->binary_port, node->touched);

	auto now = epoch::now<>();

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(node->lower_name());
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		if (!is_active(node_ref) && !is_equal(node_ref, node)) {
			L_NODE_NODES("touch_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> nullptr (1)", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
			return nullptr;
		}
		auto node_ref_copy = std::make_unique<Node>(*node_ref);
		node_ref_copy->touched = now;
		node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
		_update_nodes(node_ref);

		// L_NODE_NODES("touch_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> {idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}", node->idx, node->name(), node->http_port, node->binary_port, node->touched, node_ref->idx, node_ref->name(), node_ref->http_port, node_ref->binary_port, node_ref->touched);
		return node_ref;
	}

	L_NODE_NODES("touch_node({idx:%zu, name:%s, http_port:%d, binary_port:%d, touched:%ld}) -> nullptr (2)", node->idx, node->name(), node->http_port, node->binary_port, node->touched);
	return nullptr;
}


void
Node::drop_node(std::string_view _node_name)
{
	L_CALL("Node::drop_node(%s)", repr(_node_name));

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(string::lower(_node_name));
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		auto node_ref_copy = std::make_unique<Node>(*node_ref);
		node_ref_copy->touched = 0;
		node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
		_update_nodes(node_ref);
	}

	L_NODE_NODES("drop_node(%s)", _node_name);
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
