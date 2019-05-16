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

#include <algorithm>                              // for std::lower_bound

#include "config.h"                               // for XAPIAND_*

#include "fs.hh"                                  // for normalize_path
#include "node.h"                                 // for Node::
#include "opts.h"                                 // for opts
#include "serialise.h"                            // for UUIDRepr, Serialise
#include "strings.hh"                             // for strings::format
#include "exception_xapian.h"                     // for SerialisationError


static inline std::string
normalize(const void *p, size_t size)
{
	auto repr = static_cast<UUIDRepr>(opts.uuid_repr);
	std::string serialised_uuid;
	std::string normalized;
	std::string unserialised(static_cast<const char*>(p), size);
	if (Serialise::possiblyUUID(unserialised)) {
		try {
			serialised_uuid = Serialise::uuid(unserialised);
			normalized = Unserialise::uuid(serialised_uuid, repr);
		} catch (const SerialisationError&) { }
	}
	return normalized;
}


static inline std::string
normalize_and_partition(const void *p, size_t size)
{
	auto repr = static_cast<UUIDRepr>(opts.uuid_repr);
	std::string serialised_uuid;
	std::string normalized;
	std::string unserialised(static_cast<const char*>(p), size);
	if (Serialise::possiblyUUID(unserialised)) {
		try {
			serialised_uuid = Serialise::uuid(unserialised);
			normalized = Unserialise::uuid(serialised_uuid, repr);
		} catch (const SerialisationError& exc) {
			return normalized;
		}
	} else {
		return normalized;
	}

	std::string result;
	switch (repr) {
#ifdef XAPIAND_UUID_GUID
		case UUIDRepr::guid:
			// {00000000-0000-1000-8000-010000000000}
			result.reserve(2 + 8 + normalized.size());
			result.append(&normalized[1 + 14], &normalized[1 + 18]);
			result.push_back('/');
			result.append(&normalized[1 + 9], &normalized[1 + 13]);
			result.push_back('/');
			result.append(normalized);
			break;
#endif
#ifdef XAPIAND_UUID_URN
		case UUIDRepr::urn:
			// urn:uuid:00000000-0000-1000-8000-010000000000
			result.reserve(2 + 8 + normalized.size());
			result.append(&normalized[9 + 14], &normalized[9 + 18]);
			result.push_back('/');
			result.append(&normalized[9 + 9], &normalized[9 + 13]);
			result.push_back('/');
			result.append(normalized);
			break;
#endif
#ifdef XAPIAND_UUID_ENCODED
		case UUIDRepr::encoded:
			if (
				// Is UUID condensed
				serialised_uuid.front() != 1 && (
					// and compacted
					((serialised_uuid.back() & 1) != 0) ||
					// or has node multicast bit "on" for (uuid with Data)
					(serialised_uuid.size() > 5 && ((*(serialised_uuid.rbegin() + 5) & 2) != 0))
				)
			) {
				auto cit = normalized.cbegin();
				auto cit_e = normalized.cend();
				result.reserve(4 + normalized.size());
				if (cit == cit_e) { return result; }
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back('/');
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back('/');
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back('/');
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back(*cit++);
				if (cit == cit_e) { return result; }
				result.push_back('/');
				result.append(cit, cit_e);
				break;
			}
			[[fallthrough]];
#endif
		default:
		case UUIDRepr::vanilla:
			// 00000000-0000-1000-8000-010000000000
			result.reserve(2 + 8 + normalized.size());
			result.append(&normalized[14], &normalized[18]);
			result.push_back('/');
			result.append(&normalized[9], &normalized[13]);
			result.push_back('/');
			result.append(normalized);
			break;
	}
	return result;
}


using normalizer_t = std::string (*)(const void *, size_t);
template<normalizer_t normalize>
static inline std::string
normalizer(const void *p, size_t size)
{
	std::string buf;
	buf.reserve(size);
	auto q = static_cast<const char *>(p);
	auto p_end = q + size;
	auto oq = q;
	while (q != p_end) {
		char c = *q++;
		switch (c) {
			case '.':
			case '/': {
				auto sz = q - oq - 1;
				if (sz) {
					auto normalized = normalize(oq, sz);
					if (!normalized.empty()) {
						buf.resize(buf.size() - sz);
						buf.append(normalized);
					}
				}
				buf.push_back(c);
				oq = q;
				break;
			}
			default:
				buf.push_back(c);
		}
	}
	auto sz = q - oq;
	if (sz) {
		auto normalized = normalize(oq, sz);
		if (!normalized.empty()) {
			buf.resize(buf.size() - sz);
			buf.append(normalized);
		}
	}
	return buf;
}

std::string Endpoint::cwd("/");


Endpoint::Endpoint(std::string_view uri, const std::shared_ptr<const Node>& node)
{
	if (node) {
		node_name = node->lower_name();
	}

	auto protocol = slice_before(uri, "://");
	if (protocol.empty()) {
		protocol = "file";
	}
	auto _query = slice_after(uri, "?");
	auto _path = slice_after(uri, "/");
	auto _user = slice_before(uri, "@");
	auto _password = slice_after(_user, ":");
	auto _port = slice_after(uri, ":");

	if (protocol == "file") {
		if (node_name.empty()) {
			node_name = Node::local_node()->lower_name();
		}
		if (_path.empty()) {
			_path = uri;
		} else {
			_path = path = std::string(uri) + "/" + std::string(_path);
		}
	} else {
		if (node_name.empty()) {
			if (_port.empty() && uri.empty()) {
				node_name = Node::local_node()->lower_name();
			} else if (_port.empty()) {
				auto uri_node = Node::get_node(uri);
				if (uri_node) {
					node_name = uri_node->lower_name();
				}
			} else {
				auto remote_port = strict_stoi(nullptr, _port);
				for (auto& uri_node : Node::nodes()) {
					if (uri_node->host() == uri && uri_node->remote_port == remote_port) {
						node_name = uri_node->lower_name();
						break;
					}
				}
			}
		}
		query = _query;
		password = _password;
		user = _user;
	}

	if (!strings::startswith(_path, '/')) {
		_path = path = Endpoint::cwd + std::string(_path);
	}
	path = normalize_path(_path);
	_path = path;
	if (strings::startswith(_path, Endpoint::cwd)) {
		_path.remove_prefix(Endpoint::cwd.size());
	}

	if (opts.uuid_partition) {
		path = normalizer<normalize_and_partition>(_path.data(), _path.size());
	} else {
		path = normalizer<normalize>(_path.data(), _path.size());
	}
}


Endpoint::Endpoint(const Endpoint& other) :
	path(other.path),
	node_name(other.node_name),
	user(other.user),
	password(other.password),
	query(other.query)
{
}


Endpoint::Endpoint(Endpoint&& other) :
	path(std::move(other.path)),
	node_name(std::move(other.node_name)),
	user(std::move(other.user)),
	password(std::move(other.password)),
	query(std::move(other.query))
{
}


Endpoint::Endpoint(const Endpoint& other, const std::shared_ptr<const Node>& node) :
	path(other.path),
	node_name(node->lower_name()),
	user(other.user),
	password(other.password),
	query(other.query)
{
}


Endpoint::Endpoint(Endpoint&& other, const std::shared_ptr<const Node>& node) :
	path(std::move(other.path)),
	node_name(node->lower_name()),
	user(std::move(other.user)),
	password(std::move(other.password)),
	query(std::move(other.query))
{
}


Endpoint&
Endpoint::operator=(const Endpoint& other)
{
	path = other.path;
	node_name = other.node_name;
	user = other.user;
	password = other.password;
	query = other.query;
	return *this;
}


Endpoint&
Endpoint::operator=(Endpoint&& other)
{
	path = std::move(other.path);
	node_name = std::move(other.node_name);
	user = std::move(other.user);
	password = std::move(other.password);
	query = std::move(other.query);
	return *this;
}


inline std::string_view
Endpoint::slice_after(std::string_view& subject, std::string_view delimiter) const
{
	size_t delimiter_location = subject.find(delimiter);
	std::string_view output;
	if (delimiter_location != std::string_view::npos) {
		size_t start = delimiter_location + delimiter.size();
		output = subject.substr(start, subject.size() - start);
		if (!output.empty()) {
			subject = subject.substr(0, delimiter_location);
		}
	}
	return output;
}


inline std::string_view
Endpoint::slice_before(std::string_view& subject, std::string_view delimiter) const
{
	size_t delimiter_location = subject.find(delimiter);
	std::string_view output;
	if (delimiter_location != std::string::npos) {
		size_t start = delimiter_location + delimiter.length();
		output = subject.substr(0, delimiter_location);
		subject = subject.substr(start, subject.length() - start);
	}
	return output;
}


std::string
Endpoint::to_string() const
{
	std::string ret;
	if (path.empty()) {
		return ret;
	}
	ret += "xapian://";
	if (!user.empty() || !password.empty()) {
		ret += user;
		if (!password.empty()) {
			ret += ":" + password;
		}
		ret += "@";
	}
	auto node = Node::get_node(node_name);
	if (node) {
		ret += node->host();
		if (node->remote_port > 0) {
			ret += ":";
			ret += strings::format("{}", node->remote_port);
		}
		if (!node->host().empty() || node->remote_port > 0) {
			ret += "/";
		}
	}
	ret += path;
	if (!query.empty()) {
		ret += "?" + query;
	}
	return ret;
}


bool
Endpoint::empty()
const noexcept
{
	return (
		path.empty() &&
		node_name.empty() &&
		user.empty() &&
		password.empty() &&
		query.empty()
	);
}


bool
Endpoint::operator<(const Endpoint& other) const
{
	return path != other.path ? path < other.path : node_name < other.node_name;
}


std::shared_ptr<const Node>
Endpoint::node() const
{
	return Node::get_node(node_name);
}


bool
Endpoint::is_local() const
{
	auto local_node = Node::local_node();
	return !local_node || node_name == local_node->lower_name();
}


size_t
Endpoint::hash() const
{
	static const std::hash<std::string> hash_fn_string;
	return hash_fn_string(path) ^ hash_fn_string(node_name);
}


std::string
Endpoints::to_string() const
{
	std::string ret;
	auto j = cbegin();
	for (int i = 0; j != cend(); ++j, ++i) {
		if (i != 0) {
			ret += ";";
		}
		ret += (*j).to_string();
	}
	return ret;
}


size_t
Endpoints::hash() const
{
	static const std::hash<Endpoint> hash_fn;
	size_t hash = 0;
	for (auto& e : *this) {
		hash ^= hash_fn(e);
	}
	return hash;
}


void
Endpoints::add(const Endpoint& endpoint)
{
	auto it = std::lower_bound(begin(), end(), endpoint); // find proper position in descending order
	if (it == end() || *it != endpoint) {
		insert(it, endpoint);  // insert before iterator it
	}
}


bool
operator==(const Endpoint& le, const Endpoint& re)
{
	return le.path == re.path && le.node_name == re.node_name;
}


bool
operator==(const Endpoints& le, const Endpoints& re)
{
	if (le.size() != re.size()) {
		return false;
	}
	for (size_t i = 0; i < le.size(); ++i) {
		if (le[i] != re[i]) {
			return false;
		}
	}
	return true;
}


bool
operator!=(const Endpoint& le, const Endpoint& re)
{
	return !(le == re);
}


bool
operator!=(const Endpoints& le, const Endpoints& re)
{
	return !(le == re);
}


size_t
std::hash<Endpoint>::operator()(const Endpoint& e) const
{
	return e.hash();
}


size_t
std::hash<Endpoints>::operator()(const Endpoints& e) const
{
	return e.hash();
}
