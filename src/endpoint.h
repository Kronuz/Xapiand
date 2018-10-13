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

#include "xapiand.h"            // for XAPIAND_BINARY_SERVERPORT

#include <algorithm>            // for move
#include <atomic>
#include <cstddef>              // for size_t
#include <functional>           // for hash
#include <memory>               // for shared_ptr
#include <netinet/in.h>         // for sockaddr_in, INET_ADDRSTRLEN, in_addr
#include <string.h>             // for memset
#include <string>               // for string, allocator, operator==, operator+
#include "string_view.hh"       // for std::string_view
#include <sys/socket.h>         // for AF_INET
#include <sys/types.h>          // for int32_t, uint64_t
#include <time.h>               // for time_t
#include <utility>              // for pair

#include "atomic_shared_ptr.h"  // for atomic_shared_ptr
#include "utils.h"              // for fast_inet_ntop4, string::lower


class Node {
	std::string _host;
	std::string _name;
	std::string _lower_name;
	struct sockaddr_in _addr;

public:
	int http_port;
	int binary_port;

	int32_t regions;
	int32_t region;
	time_t touched;

	Node() : http_port(0), binary_port(0), regions(1), region(0), touched(0) {
		memset(&_addr, 0, sizeof(_addr));
	}

	// Move constructor
	Node(Node&& other)
		: _host(std::move(other._host)),
		  _name(std::move(other._name)),
		  _lower_name(std::move(other._lower_name)),
		  _addr(std::move(other._addr)),
		  http_port(std::move(other.http_port)),
		  binary_port(std::move(other.binary_port)),
		  regions(std::move(other.regions)),
		  region(std::move(other.region)),
		  touched(std::move(other.touched)) { }

	// Copy Constructor
	Node(const Node& other)
		: _host(other._host),
		  _name(other._name),
		  _lower_name(other._lower_name),
		  _addr(other._addr),
		  http_port(other.http_port),
		  binary_port(other.binary_port),
		  regions(other.regions),
		  region(other.region),
		  touched(other.touched) { }

	// Move assignment
	Node& operator=(Node&& other) {
		_host = std::move(other._host);
		_name = std::move(other._name);
		_lower_name = std::move(other._lower_name);
		_addr = std::move(other._addr);
		http_port = std::move(other.http_port);
		binary_port = std::move(other.binary_port);
		regions = std::move(other.regions);
		region = std::move(other.region);
		touched = std::move(other.touched);
		return *this;
	}

	// Copy assignment
	Node& operator=(const Node& other) {
		_host = other._host;
		_name = other._name;
		_lower_name = other._lower_name;
		_addr = other._addr;
		http_port = other.http_port;
		binary_port = other.binary_port;
		regions = other.regions;
		region = other.region;
		touched = other.touched;
		return *this;
	}

	void clear() {
		_host.clear();
		_name.clear();
		_lower_name.clear();
		regions = 1;
		region = 0;
		memset(&_addr, 0, sizeof(_addr));
		http_port = 0;
		binary_port = 0;
		touched = 0;
	}

	bool empty() const noexcept {
		return _name.empty();
	}

	std::string serialise() const;
	static Node unserialise(const char **p, const char *end);

	void name(const std::string& name) {
		_name = name;
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

	const std::string& host() const noexcept {
		return _host;
	}

	bool operator==(const Node& other) const {
		return
			_addr.sin_addr.s_addr == other._addr.sin_addr.s_addr &&
			http_port == other.http_port &&
			binary_port == other.binary_port &&
			_lower_name == other._lower_name;
	}

	bool operator!=(const Node& other) const {
		return !operator==(other);
	}

	std::string to_string() const {
		return _name;
	}
};


extern atomic_shared_ptr<const Node> local_node;
extern atomic_shared_ptr<const Node> master_node;


#include <unordered_set>        // for unordered_set
#include <vector>               // for vector


class Endpoint;
class Endpoints;


namespace std {
	template<>
	struct hash<Endpoint> {
		size_t operator()(const Endpoint& e) const;
	};


	template<>
	struct hash<Endpoints> {
		size_t operator()(const Endpoints& e) const;
	};
}


bool operator==(const Endpoint& le, const Endpoint& re);
bool operator==(const Endpoints& le, const Endpoints& re);
bool operator!=(const Endpoint& le, const Endpoint& re);
bool operator!=(const Endpoints& le, const Endpoints& re);


class Endpoint {
	std::string_view slice_after(std::string_view& subject, std::string_view delimiter) const;
	std::string_view slice_before(std::string_view& subject, std::string_view delimiter) const;

public:
	static std::string cwd;

	int port;
	std::string user, password, host, path, search;

	std::string node_name;
	long long mastery_level;

	Endpoint();
	Endpoint(std::string_view uri, const Node* node_=nullptr, long long mastery_level_=-1, std::string_view node_name_="");

	bool is_local() const {
		auto local_node_ = local_node.load();
		int binary_port = local_node_->binary_port;
		if (!binary_port) binary_port = XAPIAND_BINARY_SERVERPORT;
		return (host == local_node_->host() || host == "127.0.0.1" || host == "localhost") && port == binary_port;
	}

	size_t hash() const;
	std::string to_string() const;

	bool operator<(const Endpoint& other) const;
	bool operator==(const Node& other) const;

	struct compare {
		constexpr bool operator() (const Endpoint& a, const Endpoint& b) const noexcept {
			return b.mastery_level > a.mastery_level;
		}
	};
};


class Endpoints : private std::vector<Endpoint> {
	std::unordered_set<Endpoint> endpoints;

public:
	using std::vector<Endpoint>::empty;
	using std::vector<Endpoint>::size;
	using std::vector<Endpoint>::operator[];
	using std::vector<Endpoint>::begin;
	using std::vector<Endpoint>::end;
	using std::vector<Endpoint>::cbegin;
	using std::vector<Endpoint>::cend;
	using std::vector<Endpoint>::front;
	using std::vector<Endpoint>::back;

	Endpoints() = default;

	template <typename T, typename = std::enable_if_t<std::is_same<Endpoint, std::decay_t<T>>::value>>
	explicit Endpoints(T&& endpoint) {
		add(std::forward<T>(endpoint));
	}

	size_t hash() const;
	std::string to_string() const;

	void clear() noexcept {
		endpoints.clear();
		std::vector<Endpoint>::clear();
	}

	void add(const Endpoint& endpoint) {
		auto p = endpoints.insert(endpoint);
		if (p.second) {
			push_back(endpoint);
		}
	}
};
