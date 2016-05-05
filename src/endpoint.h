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

#pragma once

#include "xapiand.h"
#include "utils.h"

#include <string>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <atomic>

struct Node {
	uint64_t id;
	std::string name;
	struct sockaddr_in addr;
	int http_port;
	int binary_port;

	mutable std::atomic<int32_t> regions;
	mutable std::atomic<int32_t> region;
	mutable std::atomic<time_t> touched;

	Node() : id(0), http_port(0), binary_port(0), regions(1), region(0), touched(0) {
		memset(&addr, 0, sizeof(addr));
	}

	// move constructor, takes a rvalue reference &&
	Node(const Node&& other) {
		id = std::move(other.id);
		name = std::move(other.name);
		addr = std::move(other.addr);
		http_port = std::move(other.http_port);
		binary_port = std::move(other.binary_port);
		regions = other.regions.load();   /* should be exist move a copy constructor? */
		region = other.region.load();
		touched = other.touched.load();
	}

	Node(const Node& other) {
		id = other.id;
		name = other.name;
		addr = other.addr;
		http_port = other.http_port;
		binary_port = other.binary_port;
		regions = other.regions.load();
		region = other.region.load();
		touched = other.touched.load();
	}

	// move assignment, takes a rvalue reference &&
	Node& operator=(const Node&& other) {
		id = std::move(other.id);
		name = std::move(other.name);
		addr = std::move(other.addr);
		http_port = std::move(other.http_port);
		binary_port = std::move(other.binary_port);
		regions = other.regions.load();
		region = other.region.load();
		touched = other.touched.load();
		return *this;
	}

	Node& operator=(const Node& other) {
		id = other.id;
		name = other.name;
		addr = other.addr;
		http_port = other.http_port;
		binary_port = other.binary_port;
		regions = other.regions.load();
		region = other.region.load();
		touched = other.touched.load();
		return *this;
	}

	void clear() {
		name.clear();
		id = 0;
		regions.store(1);
		region.store(0);
		memset(&addr, 0, sizeof(addr));
		http_port = 0;
		binary_port = 0;
		touched = 0;
	}

	bool empty() const {
		return name.empty();
	}

	std::string serialise() const;
	static Node unserialise(const char **p, const char *end);

	std::string host() const {
		char ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
		return std::string(ip);
	}

	inline bool operator==(const Node& other) const {
		return
			lower_string(name) == lower_string(other.name) &&
			addr.sin_addr.s_addr == other.addr.sin_addr.s_addr &&
			http_port == other.http_port &&
			binary_port == other.binary_port;
	}

	inline bool operator!=(const Node& other) const {
		return !operator==(other);
	}
};

extern std::shared_ptr<const Node> local_node;

class Endpoint;
class Endpoints;


#include <vector>
#include <unordered_set>


namespace std {
	template<>
	struct hash<Endpoint> {
		size_t operator()(const Endpoint &e) const;
	};


	template<>
	struct hash<Endpoints> {
		size_t operator()(const Endpoints &e) const;
	};
}

bool operator == (Endpoint const& le, Endpoint const& re);
bool operator == (Endpoints const& le, Endpoints const& re);


class Endpoint {
	inline std::string slice_after(std::string &subject, std::string delimiter);
	inline std::string slice_before(std::string &subject, std::string delimiter);

public:
	static std::string cwd;

	int port;
	std::string user, password, host, path, search;

	std::string node_name;
	long long mastery_level;

	Endpoint();
	Endpoint(const std::string &path_, const Node* node_=nullptr, long long mastery_level_=-1, const std::string& node_name="");

	bool is_local() const {
		auto node = std::atomic_load(&local_node);
		int binary_port = node->binary_port;
		if (!binary_port) binary_port = XAPIAND_BINARY_SERVERPORT;
		return (host == node->host() || host == "127.0.0.1" || host == "localhost") && port == binary_port;
	}

	size_t hash() const;
	std::string as_string() const;
	bool operator<(const Endpoint & other) const;
	bool operator==(const Node &other) const;
	struct compare {
		constexpr bool operator() (const Endpoint &a, const Endpoint &b) const noexcept {
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

	Endpoints() {}
	Endpoints(const Endpoint &endpoint) {
		add(endpoint);
	}

	size_t hash() const;
	std::string as_string() const;

	void clear() {
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
