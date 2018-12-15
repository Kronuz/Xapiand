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

#pragma once

#include "config.h"             // for XAPIAND_CLUSTERING

#include <arpa/inet.h>          // for inet_addr
#include <atomic>               // for std::atomic_llong, std::atomic_size_t
#include <cstddef>              // for size_t
#include <functional>           // for std::hash
#include <memory>               // for std::shared_ptr
#include <mutex>                // for std::mutex
#include <netinet/in.h>         // for sockaddr_in, INET_ADDRSTRLEN, in_addr
#include <string.h>             // for memset
#include <string>               // for std::string
#include "string_view.hh"       // for std::string_view
#include <unordered_map>        // for std::unordered_map
#include <utility>              // for std::pair, std::move
#include <vector>               // for std::vector

#include "atomic_shared_ptr.h"  // for atomic_shared_ptr
#include "color_tools.hh"       // for color, hsv2rgb
#include "epoch.hh"             // for epoch::now
#include "net.hh"               // for fast_inet_ntop4
#include "string.hh"            // for string::lower
#include "stringified.hh"       // for stringified


constexpr long long NODE_LIFESPAN = 120000;  // in milliseconds


class Node {
	std::string _host;
	std::string _name;
	std::string _lower_name;
	struct sockaddr_in _addr;

public:
	size_t idx;

	int http_port;
	int binary_port;

	mutable std::atomic_llong touched;

	Node() : idx{0}, http_port{0}, binary_port{0}, touched{0} {
		memset(&_addr, 0, sizeof(_addr));
	}

	// Move constructor
	Node(Node&& other)
		: _host{std::move(other._host)},
		  _name{std::move(other._name)},
		  _lower_name{std::move(other._lower_name)},
		  _addr{std::move(other._addr)},
		  idx{std::move(other.idx)},
		  http_port{std::move(other.http_port)},
		  binary_port{std::move(other.binary_port)},
		  touched{other.touched.load(std::memory_order_relaxed)} { }

	// Copy Constructor
	Node(const Node& other)
		: _host{other._host},
		  _name{other._name},
		  _lower_name{other._lower_name},
		  _addr{other._addr},
		  idx{other.idx},
		  http_port{other.http_port},
		  binary_port{other.binary_port},
		  touched{other.touched.load(std::memory_order_relaxed)} { }

	// Move assignment
	Node& operator=(Node&& other) {
		_host = std::move(other._host);
		_name = std::move(other._name);
		_lower_name = std::move(other._lower_name);
		_addr = std::move(other._addr);
		idx = std::move(other.idx);
		http_port = std::move(other.http_port);
		binary_port = std::move(other.binary_port);
		touched = other.touched.load(std::memory_order_relaxed);
		return *this;
	}

	// Copy assignment
	Node& operator=(const Node& other) {
		_host = other._host;
		_name = other._name;
		_lower_name = other._lower_name;
		_addr = other._addr;
		idx = other.idx;
		http_port = other.http_port;
		binary_port = other.binary_port;
		touched = other.touched.load(std::memory_order_relaxed);
		return *this;
	}

	void clear() {
		_host.clear();
		_name.clear();
		_lower_name.clear();
		memset(&_addr, 0, sizeof(_addr));
		idx = 0;
		http_port = 0;
		binary_port = 0;
		touched.store(0, std::memory_order_relaxed);
	}

	bool empty() const noexcept {
		return _name.empty();
	}

	std::string serialise() const;
	static Node unserialise(const char **p, const char *end);

	std::string __repr__() const;

	void name(std::string_view name) {
		_name = std::string(name);
		_lower_name = string::lower(_name);
	}

	const std::string& name() const noexcept {
		return _name;
	}

	const std::string& lower_name() const noexcept {
		return _lower_name;
	}

	void addr(const struct sockaddr_in& addr) {
		_addr = addr;
		_host = fast_inet_ntop4(_addr.sin_addr);
	}

	const struct sockaddr_in& addr() const noexcept {
		return _addr;
	}

	void host(std::string_view host) {
		_addr.sin_addr.s_addr = inet_addr(stringified(host).c_str());
		_host = fast_inet_ntop4(_addr.sin_addr);
	}

	const std::string& host() const noexcept {
		return _host;
	}

	bool operator==(const Node& other) const {
		return
			(!idx || !other.idx || idx == other.idx) &&
			(!_addr.sin_addr.s_addr || !other._addr.sin_addr.s_addr || _addr.sin_addr.s_addr == other._addr.sin_addr.s_addr) &&
			(!http_port || !other.http_port || http_port == other.http_port) &&
			(!binary_port || !other.binary_port || binary_port == other.binary_port) &&
			_lower_name == other._lower_name;
	}

	bool operator!=(const Node& other) const {
		return !operator==(other);
	}

	std::string to_string() const {
		return _name;
	}

	color col() const;

	bool is_equal(const std::shared_ptr<const Node>& other) const {
		return other && (this == other.get() || *this == *other);
	}

	bool is_local() const {
		return is_equal(_local_node.load(std::memory_order_relaxed));
	}

	bool is_leader() const {
		return is_equal(_leader_node.load(std::memory_order_relaxed));
	}

	bool is_active() const {
		return (touched.load(std::memory_order_relaxed) >= epoch::now<std::chrono::milliseconds>() - NODE_LIFESPAN || is_local());
	}

	static bool is_equal(const std::shared_ptr<const Node>& a, const std::shared_ptr<const Node>& b) {
		return a && b && (a == b || *a == *b);
	}

	static bool is_local(const std::shared_ptr<const Node>& node) {
		return node && node->is_local();
	}

	static bool is_leader(const std::shared_ptr<const Node>& node) {
		return node && node->is_leader();
	}

	static bool is_active(const std::shared_ptr<const Node>& node) {
		return node && node->is_active();
	}

	static std::shared_ptr<const Node> local_node(std::shared_ptr<const Node> node = nullptr);
	static std::shared_ptr<const Node> leader_node(std::shared_ptr<const Node> node = nullptr);

private:
	static atomic_shared_ptr<const Node> _local_node;
	static atomic_shared_ptr<const Node> _leader_node;

	static std::atomic_size_t _total_nodes;
	static std::atomic_size_t _active_nodes;
	static std::atomic_size_t _indexed_nodes;

#ifdef XAPIAND_CLUSTERING
	static std::mutex _nodes_mtx;
	static std::unordered_map<std::string, std::shared_ptr<const Node>> _nodes;
	static std::vector<std::shared_ptr<const Node>> _nodes_indexed;

	static void _update_nodes(const std::shared_ptr<const Node>& node);

public:
	static size_t total_nodes() {
		return _total_nodes.load(std::memory_order_relaxed);
	}

	static size_t active_nodes() {
		return _active_nodes.load(std::memory_order_relaxed);
	}

	static size_t indexed_nodes() {
		return _indexed_nodes.load(std::memory_order_relaxed);
	}

	static std::shared_ptr<const Node> get_node(size_t idx);
	static std::shared_ptr<const Node> get_node(std::string_view node_name);
	static std::pair<std::shared_ptr<const Node>, bool> put_node(std::shared_ptr<const Node> node, bool touch = true);
	static std::shared_ptr<const Node> touch_node(const Node& node);
	static void drop_node(std::string_view node_name);
	static void reset();

	static std::vector<std::shared_ptr<const Node>> nodes();

	static std::string dump_nodes(int level = 1);
#endif
};
