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

#include "node.h"

#include <cstdlib>              // for atoi
#include <algorithm>            // for std::lower_bound

#include "hashes.hh"            // for fnv1ah32::hash
#include "length.h"             // for serialise_length, unserialise_length, ser...
#include "log.h"                // for L_CALL
#include "logger.h"             // for Logging::tab_title, Logging::badge
#include "serialise.h"          // for Serialise
#include "strings.hh"           // for strings::format, strings::lower
#include "xapian.h"             // for SerialisationError


#define L_NODE_NODES L_NOTHING


// #undef L_NODE_NODES
// #define L_NODE_NODES L_SLATE_GREY


static inline void
set_as_title(const std::shared_ptr<const Node>& node)
{
	if (node && !node->name().empty()) {
		// Set window title
		Logging::tab_title(node->name());

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
	std::string serialised;
	serialised.append(serialise_string(_name));
	serialised.append(serialise_length(_addr.sin_addr.s_addr));
	serialised.append(serialise_length(http_port));
#ifdef XAPIAND_CLUSTERING
	serialised.append(serialise_length(remote_port));
	serialised.append(serialise_length(replication_port));
#endif
	return serialised;
}


Node
Node::unserialise(const char **pp, const char *p_end)
{
	Node node;

	const char *p = *pp;

	node._addr.sin_family = AF_INET;
	node._addr.sin_addr.s_addr = unserialise_length(&p, p_end);
	node.http_port = unserialise_length(&p, p_end);
	node._name = unserialise_string(&p, p_end);
#ifdef XAPIAND_CLUSTERING
	if (p != p_end) {
		node.remote_port = unserialise_length(&p, p_end);
		node.replication_port = unserialise_length(&p, p_end);
	}
#endif
	node._lower_name = strings::lower(node._name);
	node._host = inet_ntop(node._addr);

	*pp = p;

	return node;
}


std::string
Node::__repr__() const
{
	return strings::format(STEEL_BLUE + "<Node {{"
			"name:{}, "
			"host:{}, "
			"http_port:{}, "
#ifdef XAPIAND_CLUSTERING
			"remote_port:{}, "
			"replication_port:{}, "
#endif
			"activated:{}, "
			"touched:{}"
		"}}{}{}{}{}>",
		repr(name()),
		repr(host()),
		http_port,
#ifdef XAPIAND_CLUSTERING
		remote_port,
		replication_port,
#endif
		activated.load(std::memory_order_acquire) ? "true" : "false",
		std::chrono::duration_cast<std::chrono::milliseconds>(touched.load(std::memory_order_acquire).time_since_epoch()).count(),
		is_alive() ? " " + DARK_STEEL_BLUE + "(alive)" + STEEL_BLUE : "",
		is_active() ? " " + DARK_STEEL_BLUE + "(active)" + STEEL_BLUE : "",
		is_local() ? " " + DARK_STEEL_BLUE + "(local)" + STEEL_BLUE : "",
		is_leader() ? " " + DARK_STEEL_BLUE + "(leader)" + STEEL_BLUE : "");
}


std::string
Node::dump_nodes(int level)
{
	std::string indent;
	for (int l = 0; l < level; ++l) {
		indent += "    ";
	}

	std::string ret;
	ret += indent;
	ret += strings::format(STEEL_BLUE + "<Nodes {{total_nodes:{}, alive_nodes:{}, active_nodes:{}}}>",
		Node::total_nodes(), Node::alive_nodes(), Node::active_nodes());
	ret.push_back('\n');

	for (auto& node : nodes()) {
		ret += indent + indent;
		ret += node->__repr__();
		ret.push_back('\n');
	}

	return ret;
}


atomic_shared_ptr<const Node> Node::_local_node(std::make_shared<const Node>());
atomic_shared_ptr<const Node> Node::_leader_node(std::make_shared<const Node>());


std::mutex Node::_nodes_mtx;
std::unordered_map<std::string, std::shared_ptr<const Node>> Node::_nodes;

std::atomic_size_t Node::_total_nodes;
std::atomic_size_t Node::_alive_nodes;
std::atomic_size_t Node::_active_nodes;


bool
Node::set_local_node(std::shared_ptr<const Node> node)
{
	L_CALL("Node::set_local_node({})", node ? node->__repr__() : "null");

	assert(node);

	auto now = std::chrono::steady_clock::now();

	node->activated.store(true, std::memory_order_release);
	node->touched.store(now, std::memory_order_release);
	set_as_title(node);
	auto old_node = _local_node.exchange(node, std::memory_order_acq_rel);

	auto name = node->lower_name();
	if (!name.empty()) {
		std::lock_guard<std::mutex> lk(_nodes_mtx);
		_nodes[name] = node;
		_update_nodes(node);
	}

	L_NODE_NODES("set_local_node({})", node->__repr__());

	return !Node::is_simmilar(old_node, node);
}


std::shared_ptr<const Node>
Node::get_local_node()
{
	L_CALL("Node::get_local_node()");

	auto node = _local_node.load(std::memory_order_acquire);

	L_NODE_NODES("get_local_node() => {}", node->__repr__());

	assert(node);

	return node;
}


bool
Node::set_leader_node(std::shared_ptr<const Node> node)
{
	L_CALL("Node::set_leader_node({})", node ? node->__repr__() : "null");

	assert(node);

	auto now = std::chrono::steady_clock::now();

	node->activated.store(true, std::memory_order_release);
	node->touched.store(now, std::memory_order_release);
	auto old_node = _leader_node.exchange(node, std::memory_order_acq_rel);

	auto name = node->lower_name();
	if (!name.empty()) {
		std::lock_guard<std::mutex> lk(_nodes_mtx);
		_nodes[name] = node;
		_update_nodes(node);
	}

	L_NODE_NODES("set_leader_node({})", node->__repr__());

	return !Node::is_simmilar(old_node, node);
}


std::shared_ptr<const Node>
Node::get_leader_node()
{
	L_CALL("Node::get_leader_node()");

	auto node = _leader_node.load(std::memory_order_acquire);

	L_NODE_NODES("get_leader_node() => {}", node->__repr__());

	assert(node);

	return node;
}


inline void
Node::_update_nodes(const std::shared_ptr<const Node>& node)
{
	auto local_node_ = _local_node.load(std::memory_order_acquire);
	if (node != local_node_) {
		if (node->lower_name() == local_node_->lower_name()) {
			if (node->name() != local_node_->name()) {
				set_as_title(node);
			}
			_local_node.store(node, std::memory_order_release);
		}
	}

	auto leader_node_ = _leader_node.load(std::memory_order_acquire);
	if (node != leader_node_) {
		if (node->lower_name() == leader_node_->lower_name()) {
			_leader_node.store(node, std::memory_order_release);
		}
	}

	size_t alive_nodes_cnt = 0;
	size_t active_nodes_cnt = 0;
	for (const auto& node_pair : _nodes) {
		const auto& node_ref = node_pair.second;
		if (is_active(node_ref)) {
			++active_nodes_cnt;
		}
		if (is_alive(node_ref)) {
			++alive_nodes_cnt;
		}
	}
	_total_nodes.store(_nodes.size(), std::memory_order_release);
	_alive_nodes.store(alive_nodes_cnt, std::memory_order_release);
	_active_nodes.store(active_nodes_cnt, std::memory_order_release);
}


std::shared_ptr<const Node>
Node::get_node(std::string_view _node_name)
{
	L_CALL("Node::get_node({})", repr(_node_name));

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(strings::lower(_node_name));
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		// L_NODE_NODES("get_node({}) -> {}", _node_name, node_ref->__repr__());
		return node_ref;
	}

	L_NODE_NODES("get_node({}) -> nullptr", _node_name);
	return nullptr;
}


std::pair<std::shared_ptr<const Node>, bool>
Node::touch_node(const Node& node, bool activate, bool touch)
{
	L_CALL("Node::touch_node({}, {}, {})", node.__repr__(), activate, touch);

	auto now = std::chrono::steady_clock::now();

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(node.lower_name());
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		if (Node::is_superset(node_ref, node)) {
			auto modified = false;
			if ((!node_ref->_addr.sin_addr.s_addr && node._addr.sin_addr.s_addr) ||
#ifdef XAPIAND_CLUSTERING
				(!node_ref->remote_port && node.remote_port) ||
				(!node_ref->replication_port && node.replication_port) ||
#endif
				(!node_ref->http_port && node.http_port)) {
				auto node_ref_copy = std::make_unique<Node>(*node_ref);
				if (!node_ref->_addr.sin_addr.s_addr && node._addr.sin_addr.s_addr) {
					node_ref_copy->_addr = node._addr;
					node_ref_copy->_host = inet_ntop(node_ref_copy->_addr);
				}
				if (!node_ref->http_port && node.http_port) {
					node_ref_copy->http_port = node.http_port;
				}
#ifdef XAPIAND_CLUSTERING
				if (!node_ref->remote_port && node.remote_port) {
					node_ref_copy->remote_port = node.remote_port;
				}
				if (!node_ref->replication_port && node.replication_port) {
					node_ref_copy->replication_port = node.replication_port;
				}
#endif
				node_ref = std::shared_ptr<const Node>(node_ref_copy.release());
				if (activate) {
					node_ref->activated.store(true, std::memory_order_release);
				}
				if (touch || is_active(node_ref)) {
					node_ref->touched.store(now, std::memory_order_release);
				}
				_update_nodes(node_ref);
				modified = true;
			} else {
				if (activate) {
					if (node_ref->activated.exchange(true, std::memory_order_release) == false) {
						modified = true;
					}
				}
				if (touch || is_active(node_ref)) {
					node_ref->touched.store(now, std::memory_order_release);
				}
				_update_nodes(node_ref);
			}
			L_NODE_NODES("touch_node({}) -> {} (1)", node_ref->__repr__(), modified);
			return std::make_pair(node_ref, modified);
		} else if (is_active(node_ref)) {
			L_NODE_NODES("touch_node({}) -> nullptr (2)", node.__repr__());
			return std::make_pair(nullptr, false);
		}
	}

	auto new_node = std::make_shared<const Node>(node);
	if (activate) {
		new_node->activated.store(true, std::memory_order_release);
	}
	if (touch || is_active(new_node)) {
		new_node->touched.store(now, std::memory_order_release);
	}

	auto name = new_node->lower_name();
	if (!name.empty()) {
		_nodes[name] = new_node;
		_update_nodes(new_node);
	}

	L_NODE_NODES("touch_node({}) -> true", new_node->__repr__());
	return std::make_pair(new_node, true);
}


void
Node::drop_node(std::string_view _node_name)
{
	L_CALL("Node::drop_node({})", repr(_node_name));

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	auto it = _nodes.find(strings::lower(_node_name));
	if (it != _nodes.end()) {
		auto& node_ref = it->second;
		node_ref->activated.store(false, std::memory_order_release);
		node_ref->touched.store(std::chrono::steady_clock::time_point::min(), std::memory_order_release);
		auto node_ref_copy = std::make_unique<Node>(*node_ref);
		node_ref_copy->_addr = sockaddr_in{};
		node_ref_copy->_host.clear();
		node_ref_copy->http_port = 0;
#ifdef XAPIAND_CLUSTERING
		node_ref_copy->remote_port = 0;
		node_ref_copy->replication_port = 0;
#endif
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


bool
operator<(const std::shared_ptr<const Node>& a, const std::shared_ptr<const Node>& b)
{
	return !a || (b && a->lower_name() < b->lower_name());
}


std::vector<std::shared_ptr<const Node>>
Node::nodes()
{
	L_CALL("Node::nodes()");

	std::lock_guard<std::mutex> lk(_nodes_mtx);

	std::vector<std::shared_ptr<const Node>> nodes;
	for (const auto& node_pair : _nodes) {
		auto it = std::lower_bound(nodes.begin(), nodes.end(), node_pair.second);
		nodes.insert(it, node_pair.second);
	}

	return nodes;
}


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
		(!other._addr.sin_addr.s_addr || _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr) &&
		(!other.http_port || http_port == other.http_port) &&
		(!other.remote_port || remote_port == other.remote_port) &&
		(!other.replication_port || replication_port == other.replication_port) &&
		_lower_name == other._lower_name
	));
}
