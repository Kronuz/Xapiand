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

#include <algorithm>            // for std::find, std::sort
#include <vector>               // for std::vector

#include "node.h"


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

	Node node;
	std::string user, password, path, search;

	Endpoint() = default;
	Endpoint(std::string_view uri, const Node* node_ = nullptr, std::string_view node_name_ = "");

	Endpoint(const Endpoint& other, const Node* node_ = nullptr);
	Endpoint(Endpoint&& other, const Node* node_ = nullptr);

	Endpoint& operator=(const Endpoint& other);
	Endpoint& operator=(Endpoint&& other);

	bool is_local() const;

	size_t hash() const;
	std::string to_string() const;

	bool empty() const noexcept {
		return path.empty() || (
			node.binary_port == -1 &&
			user.empty() &&
			password.empty() &&
			node.host().empty() &&
			search.empty() &&
			node.name().empty()
		);
	}

	bool operator<(const Endpoint& other) const;
};


class Endpoints : public std::vector<Endpoint> {
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
		push_back(std::forward<T>(endpoint));
	}

	size_t hash() const;
	std::string to_string() const;

	void add(const Endpoint& endpoint) {
		if (std::find(begin(), end(), endpoint) == end()) {
			push_back(endpoint);
			std::sort(begin(), end());
		}
	}
};
