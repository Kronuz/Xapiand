/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

#include "cluster/rpc_methods.h"

#include <limits>   // for std::numeric_limits

namespace cluster::rpc {

namespace {

// A map's required key; unknown keys are ignored, so a newer sender may add fields.
const MsgPack& at(const MsgPack& m, std::string_view key) {
	if (!m.is_map())
		throw Malformed("shard rpc: envelope is not a map");
	auto it = m.find(key);
	if (it == m.end())
		throw Malformed("shard rpc: envelope lacks '" + std::string(key) + "'");
	return it.value();
}
std::uint64_t u64(const MsgPack& v, std::uint64_t most = std::numeric_limits<std::uint64_t>::max()) {
	if (v.get_type() != MsgPack::Type::POSITIVE_INTEGER)
		throw Malformed("shard rpc: expected an unsigned integer");
	const auto n = v.u64();
	if (n > most)
		throw Malformed("shard rpc: integer out of range");
	return n;
}
std::string str(const MsgPack& v) {
	if (!v.is_string())
		throw Malformed("shard rpc: expected a string");
	return v.str();
}
const MsgPack& array(const MsgPack& v) {
	if (!v.is_array())
		throw Malformed("shard rpc: expected an array");
	return v;
}
MsgPack ids(const std::vector<std::uint64_t>& v) {
	MsgPack a = MsgPack::ARRAY();
	for (auto id : v)
		a.push_back(id);
	return a;
}
std::vector<std::uint64_t> ids(const MsgPack& v) {
	std::vector<std::uint64_t> r;
	for (const auto& id : array(v))
		r.push_back(u64(id));
	return r;
}
MsgPack root(std::string_view bytes) {
	std::size_t used = 0;
	MsgPack m = MsgPack::unserialize(bytes, used);
	if (used != bytes.size())
		throw Malformed("shard rpc: bytes after the envelope");
	if (!m.is_map())
		throw Malformed("shard rpc: envelope is not a map");
	return m;
}
// Runs a decoder: whatever MsgPack throws on bytes that are not one envelope becomes Malformed.
template <class F> auto strictly(F f) -> decltype(f()) {
	try {
		return f();
	} catch (const Malformed&) {
		throw;
	} catch (const std::exception& e) {
		throw Malformed(std::string("shard rpc: ") + e.what());
	} catch (...) {
		throw Malformed("shard rpc: undecodable envelope");
	}
}

} // namespace

std::string Relayed::encode() const {
	MsgPack m = MsgPack::MAP();
	m["v"] = verb;
	m["p"] = path;
	m["q"] = query;
	m["ct"] = content_type;
	m["ac"] = accept;
	m["g"] = group;
	m["i"] = incarnation;
	m["h"] = std::uint64_t(hops);
	return m.serialize();
}
Relayed Relayed::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		Relayed r;
		r.verb = str(at(m, "v"));
		r.path = str(at(m, "p"));
		r.query = str(at(m, "q"));
		r.content_type = str(at(m, "ct"));
		r.accept = str(at(m, "ac"));
		r.group = u64(at(m, "g"));
		r.incarnation = u64(at(m, "i"));
		r.hops = std::uint8_t(u64(at(m, "h"), 255));
		return r;
	});
}

std::string Answer::encode() const {
	MsgPack m = MsgPack::MAP();
	m["s"] = std::uint64_t(status);
	m["ct"] = content_type;
	MsgPack h = MsgPack::ARRAY();
	for (const auto& [k, v] : headers)
		h.push_back(MsgPack({k, v}));
	m["hd"] = h;
	return m.serialize();
}
Answer Answer::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		Answer a;
		a.status = std::uint16_t(u64(at(m, "s"), 999));
		a.content_type = str(at(m, "ct"));
		for (const auto& kv : array(at(m, "hd"))) {
			if (!kv.is_array() || kv.size() != 2)
				throw Malformed("shard rpc: a header is not a pair");
			a.headers.emplace_back(str(kv.at(0)), str(kv.at(1)));
		}
		return a;
	});
}

std::string StatsRequest::encode() const {
	MsgPack m = MsgPack::MAP();
	m["g"] = ids(groups);
	MsgPack t = MsgPack::ARRAY();
	for (const auto& [field, term] : terms)
		t.push_back(MsgPack({field, term}));
	m["t"] = t;
	return m.serialize();
}
StatsRequest StatsRequest::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		StatsRequest r;
		r.groups = ids(at(m, "g"));
		for (const auto& ft : array(at(m, "t"))) {
			if (!ft.is_array() || ft.size() != 2)
				throw Malformed("shard rpc: a term is not a field and a term");
			r.terms.emplace_back(str(ft.at(0)), str(ft.at(1)));
		}
		return r;
	});
}

std::string StatsAnswer::encode() const {
	MsgPack m = MsgPack::MAP();
	MsgPack a = MsgPack::ARRAY();
	for (const auto& g : groups) {
		MsgPack e = MsgPack::MAP();
		e["g"] = g.group;
		e["n"] = g.doc_count;
		e["l"] = g.total_length;
		MsgPack fields = MsgPack::MAP();
		for (const auto& [field, by_term] : g.terms) {
			MsgPack terms = MsgPack::MAP();
			for (const auto& [term, t] : by_term)
				terms[term] = MsgPack({t.df, t.cf});
			fields[field] = terms;
		}
		e["t"] = fields;
		a.push_back(e);
	}
	m["g"] = a;
	return m.serialize();
}
StatsAnswer StatsAnswer::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		StatsAnswer r;
		for (const auto& e : array(at(m, "g"))) {
			GroupStats g;
			g.group = u64(at(e, "g"));
			g.doc_count = u64(at(e, "n"));
			g.total_length = u64(at(e, "l"));
			const auto& fields = at(e, "t");
			if (!fields.is_map())
				throw Malformed("shard rpc: term statistics are not a map");
			for (auto f = fields.begin(); f != fields.end(); ++f) {
				const auto& terms = f.value();
				if (!terms.is_map())
					throw Malformed("shard rpc: a field's terms are not a map");
				auto& by_term = g.terms[str(*f)];
				for (auto t = terms.begin(); t != terms.end(); ++t) {
					const auto& v = t.value();
					if (!v.is_array() || v.size() != 2)
						throw Malformed("shard rpc: a term's statistics are not df and cf");
					by_term[str(*t)] = {u64(v.at(0)), u64(v.at(1))};
				}
			}
			r.groups.push_back(std::move(g));
		}
		return r;
	});
}

std::string SearchRequest::encode() const {
	MsgPack m = MsgPack::MAP();
	m["g"] = ids(groups);
	m["q"] = query;
	m["o"] = params;
	m["st"] = stats;
	return m.serialize();
}
SearchRequest SearchRequest::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		SearchRequest r;
		r.groups = ids(at(m, "g"));
		r.query = at(m, "q");
		r.params = at(m, "o");
		r.stats = at(m, "st");
		return r;
	});
}

std::string SearchAnswer::encode() const {
	MsgPack m = MsgPack::MAP();
	m["r"] = partial;
	m["m"] = matches;
	m["f"] = ids(failed);
	return m.serialize();
}
SearchAnswer SearchAnswer::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		SearchAnswer r;
		r.partial = str(at(m, "r"));
		r.matches = u64(at(m, "m"));
		r.failed = ids(at(m, "f"));
		return r;
	});
}

std::string CountAnswer::encode() const {
	MsgPack m = MsgPack::MAP();
	m["c"] = count;
	m["f"] = ids(failed);
	return m.serialize();
}
CountAnswer CountAnswer::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		CountAnswer r;
		r.count = u64(at(m, "c"));
		r.failed = ids(at(m, "f"));
		return r;
	});
}

std::string InfoRequest::encode() const {
	MsgPack m = MsgPack::MAP();
	m["g"] = ids(groups);
	return m.serialize();
}
InfoRequest InfoRequest::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		InfoRequest r;
		r.groups = ids(at(m, "g"));
		return r;
	});
}

std::string InfoAnswer::encode() const {
	MsgPack m = MsgPack::MAP();
	m["i"] = info;
	return m.serialize();
}
InfoAnswer InfoAnswer::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		InfoAnswer r;
		r.info = at(m, "i");
		if (!r.info.is_map())
			throw Malformed("shard rpc: info is not a map");
		return r;
	});
}

std::string Keepalive::encode() const {
	MsgPack m = MsgPack::MAP();
	m["n"] = name;
	m["h"] = http;
	m["p"] = peer;
	m["b"] = boot;
	m["g"] = groups;
	return m.serialize();
}
Keepalive Keepalive::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		Keepalive k;
		k.name = str(at(m, "n"));
		k.http = str(at(m, "h"));
		k.peer = str(at(m, "p"));
		k.boot = u64(at(m, "b"));
		k.groups = u64(at(m, "g"));
		return k;
	});
}

std::string KeepaliveAnswer::encode() const {
	MsgPack m = MsgPack::MAP();
	m["l"] = leader;
	m["e"] = epoch;
	return m.serialize();
}
KeepaliveAnswer KeepaliveAnswer::decode(std::string_view bytes) {
	return strictly([&] {
		const auto m = root(bytes);
		KeepaliveAnswer r;
		r.leader = u64(at(m, "l"));
		r.epoch = u64(at(m, "e"));
		return r;
	});
}

} // namespace cluster::rpc
