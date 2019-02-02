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

#include "config.h"         // for XAPIAND_*

#include "fs.hh"            // for normalize_path
#include "opts.h"           // for opts
#include "serialise.h"      // for UUIDRepr, Serialise
#include "string.hh"        // for string::Number
#include "xapian.h"         // for SerialisationError


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
			/* FALLTHROUGH */
#endif
		default:
		case UUIDRepr::simple:
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


Endpoint::Endpoint(std::string_view uri, const Node* node_, std::string_view node_name_)
{
	if (node_ == nullptr) {
		auto local_node = Node::local_node();
		node = *local_node;
	} else {
		node = *node_;
	}

	if (!node_name_.empty()) {
		node.name(node_name_);
	}

	auto protocol = slice_before(uri, "://");
	if (protocol.empty()) {
		protocol = "file";
	}
	auto _search = slice_after(uri, "?");
	auto _path = slice_after(uri, "/");
	auto _user = slice_before(uri, "@");
	auto _password = slice_after(_user, ":");
	auto _port = slice_after(uri, ":");

	if (protocol == "file") {
		if (_path.empty()) {
			_path = uri;
		} else {
			_path = path = std::string(uri) + "/" + std::string(_path);
		}
	} else {
		node.host(uri);
		node.remote_port = strict_stoi(nullptr, _port);
		search = _search;
		password = _password;
		user = _user;
	}

	if (!string::startswith(_path, '/')) {
		_path = path = Endpoint::cwd + std::string(_path);
	}
	path = normalize_path(_path, true);
	_path = path;
	if (string::startswith(_path, Endpoint::cwd)) {
		_path.remove_prefix(Endpoint::cwd.size());
		if (_path.empty()) {
			_path = "./";
		}
	}

	if (opts.uuid_partition) {
		path = normalizer<normalize_and_partition>(_path.data(), _path.size());
	} else {
		path = normalizer<normalize>(_path.data(), _path.size());
	}
}


Endpoint::Endpoint(const Endpoint& other, const Node* node_) :
	node{node_ ? *node_ : other.node},
	user{other.user},
	password{other.password},
	path{other.path},
	search{other.search}
{
}


Endpoint::Endpoint(Endpoint&& other, const Node* node_) :
	node{node_ ? *node_ : std::move(other.node)},
	user{std::move(other.user)},
	password{std::move(other.password)},
	path{std::move(other.path)},
	search{std::move(other.search)}
{
}


Endpoint&
Endpoint::operator=(const Endpoint& other)
{
	node = other.node;
	user = other.user;
	password = other.password;
	path = other.path;
	search = other.search;
	return *this;
}


Endpoint&
Endpoint::operator=(Endpoint&& other)
{
	node = std::move(other.node);
	user = std::move(other.user);
	password = std::move(other.password);
	path = std::move(other.path);
	search = std::move(other.search);
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
	ret += node.host();
	if (node.remote_port > 0) {
		ret += ":";
		ret += string::Number(node.remote_port);
	}
	if (!node.host().empty() || node.remote_port > 0) {
		ret += "/";
	}
	ret += path;
	if (!search.empty()) {
		ret += "?" + search;
	}
	return ret;
}


bool
Endpoint::operator<(const Endpoint& other) const
{
	return hash() < other.hash();
}


bool
Endpoint::is_local() const
{
	auto local_node = Node::local_node();
	int remote_port = local_node->remote_port;
	return (node.host() == local_node->host() || node.host() == "127.0.0.1" || node.host() == "localhost") && node.remote_port == remote_port;
}


size_t
Endpoint::hash() const
{
	static const std::hash<std::string> hash_fn_string;
	static const std::hash<int> hash_fn_int;
	return (
		hash_fn_string(path) ^
		hash_fn_string(user) ^
		hash_fn_string(password) ^
		hash_fn_string(node.host()) ^
		hash_fn_int(node.remote_port) ^
		hash_fn_string(search)
	);
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
	size_t hash = 0;
	std::hash<Endpoint> hash_fn;
	auto j = cbegin();
	for (int i = 0; j != cend(); ++j, ++i) {
		hash ^= hash_fn(*j);
	}
	return hash;
}


bool
operator==(const Endpoint& le, const Endpoint& re)
{
	std::hash<Endpoint> hash_fn;
	return hash_fn(le) == hash_fn(re);
}


bool
operator==(const Endpoints& le, const Endpoints& re)
{
	std::hash<Endpoints> hash_fn;
	return hash_fn(le) == hash_fn(re);
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
