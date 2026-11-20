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

/*
 * The shard RPC's envelopes (docs/mechanisms/shard-rpc.md, Messages): one MessagePack map per
 * message with short keys, decoded strictly. decode() throws Malformed, which a server answers
 * bad_request. Pre-release bytes (D87): format-freeze reviews them.
 */

#pragma once

#include <cstdint>      // for std::uint8_t, std::uint16_t, std::uint64_t
#include <map>          // for std::map
#include <optional>     // for std::optional
#include <stdexcept>    // for std::runtime_error
#include <string>       // for std::string
#include <string_view>  // for std::string_view
#include <utility>      // for std::pair
#include <vector>       // for std::vector

#include "cluster/rpc.h"  // for ServerCall, Status
#include "msgpack.h"      // for MsgPack

namespace cluster::rpc {

class Malformed : public std::runtime_error {
  public:
	using std::runtime_error::runtime_error;
};

// forward_write and get: a request relayed to the node that serves it (D8), its body the payload.
struct Relayed {
	std::string verb, path, query, content_type, accept;
	std::uint64_t group = 0, incarnation = 0;  // the group the sender believes serves it; 0: by path
	std::uint8_t hops = 0;                     // relays so far; a node relays at most twice (D8)
	std::string encode() const;
	static Relayed decode(std::string_view bytes);
	bool operator==(const Relayed&) const = default;
};
// Its answer, the response body the payload.
struct Answer {
	std::uint16_t status = 200;
	std::string content_type;
	std::vector<std::pair<std::string, std::string>> headers;
	std::string encode() const;
	static Answer decode(std::string_view bytes);
	bool operator==(const Answer&) const = default;
};

// stats: a search's statistics round (D10), per group, as prospect's supplied_statistics.
struct TermStats {
	std::uint64_t df = 0, cf = 0;
	bool operator==(const TermStats&) const = default;
};
struct StatsRequest {
	std::vector<std::uint64_t> groups;
	std::vector<std::pair<std::string, std::string>> terms;  // field, term
	std::string encode() const;
	static StatsRequest decode(std::string_view bytes);
	bool operator==(const StatsRequest&) const = default;
};
struct GroupStats {
	std::uint64_t group = 0, doc_count = 0, total_length = 0;
	std::map<std::string, std::map<std::string, TermStats>> terms;  // by field, then term
	bool operator==(const GroupStats&) const = default;
};
struct StatsAnswer {
	std::vector<GroupStats> groups;
	std::string encode() const;
	static StatsAnswer decode(std::string_view bytes);
	bool operator==(const StatsAnswer&) const = default;
};

// search_partial and count: a node's groups under one query. `query` and `params` are the
// request's DSL and options, `stats` the merged statistics (the stats answer's shape, one per
// index); the answer's `partial` is the engine's serialized partial result.
struct SearchRequest {
	std::vector<std::uint64_t> groups;
	MsgPack query, params, stats;
	std::string encode() const;
	static SearchRequest decode(std::string_view bytes);
};
struct SearchAnswer {
	std::string partial;
	std::uint64_t matches = 0;
	std::vector<std::uint64_t> failed;  // groups this node could not serve
	std::string encode() const;
	static SearchAnswer decode(std::string_view bytes);
	bool operator==(const SearchAnswer&) const = default;
};
struct CountAnswer {
	std::uint64_t count = 0;
	std::vector<std::uint64_t> failed;
	std::string encode() const;
	static CountAnswer decode(std::string_view bytes);
	bool operator==(const CountAnswer&) const = default;
};

// info: what a node holds of some groups (`:info`, health).
struct InfoRequest {
	std::vector<std::uint64_t> groups;
	std::string encode() const;
	static InfoRequest decode(std::string_view bytes);
	bool operator==(const InfoRequest&) const = default;
};
struct InfoAnswer {
	MsgPack info;  // by group id, as decimal strings
	std::string encode() const;
	static InfoAnswer decode(std::string_view bytes);
};

// keepalive: a node to the root leader every 500 ms (D6).
struct Keepalive {
	std::string name, http, peer;  // host:port each
	std::uint64_t boot = 0;        // random per process start: a restart is a new boot
	std::uint64_t groups = 0;      // groups this node holds
	std::string encode() const;
	static Keepalive decode(std::string_view bytes);
	bool operator==(const Keepalive&) const = default;
};
struct KeepaliveAnswer {
	std::uint64_t leader = 0;  // the root leader the answering node knows, 0 when none
	std::uint64_t epoch = 0;   // the directory's version the answer reflects
	std::string encode() const;
	static KeepaliveAnswer decode(std::string_view bytes);
	bool operator==(const KeepaliveAnswer&) const = default;
};

// A served call's envelope as T, or nullopt once the call is answered bad_request.
template <class T> std::optional<T> request(ServerCall& call) {
	try {
		return T::decode(call.envelope());
	} catch (const Malformed& e) {
		call.fail(Status::bad_request, e.what());
		return std::nullopt;
	}
}

} // namespace cluster::rpc
