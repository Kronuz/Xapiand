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

// One valid message of each shard-RPC envelope, extremes included: the codec tests round-trip them
// and the fuzz target's seed corpus (fuzz_rpc_seed.cc) encodes them.

#pragma once

#include <cstdint>  // for std::uint64_t
#include <limits>   // for std::numeric_limits
#include <string>   // for std::string
#include <vector>   // for std::vector

#include "cluster/rpc_methods.h"

namespace rpc_samples {

constexpr std::uint64_t max64 = std::numeric_limits<std::uint64_t>::max();

inline cluster::rpc::Relayed relayed() {
	cluster::rpc::Relayed r;
	r.verb = "PUT";
	r.path = "/twitter/user/42";
	r.query = "ack=quorum&refresh=true";
	r.content_type = "application/json";
	r.accept = "application/x-msgpack";
	r.group = 0x0123456789abcdefull;
	r.incarnation = max64;
	r.hops = 1;
	return r;
}
inline cluster::rpc::Answer answer() {
	return {201, "application/json", {{"Location", "/twitter/user/42"}, {"X-Version", "7"}}};
}
inline cluster::rpc::StatsRequest stats_request() {
	return {{1, 2, max64}, {{"text", "hello"}, {"text", "wörld"}, {"", std::string("\0b", 2)}}};
}
inline cluster::rpc::StatsAnswer stats_answer() {
	cluster::rpc::StatsAnswer a;
	cluster::rpc::GroupStats g{7, 40, 1234, {}};
	g.terms["text"]["hello"] = {3, 5};
	g.terms["text"]["world"] = {0, 0};
	g.terms["title"]["x"] = {max64, max64};
	a.groups.push_back(g);
	a.groups.push_back({8, 0, 0, {}});
	return a;
}
inline cluster::rpc::SearchRequest search_request() {
	cluster::rpc::SearchRequest r;
	r.groups = {7, 8};
	r.query = MsgPack({{"_query", {{"text", "hello"}}}});
	r.params = MsgPack({{"_limit", 10}, {"_aggs", {{"n", {{"_count", {{"_field", "n"}}}}}}}});
	r.stats = MsgPack({{"twitter", {{"n", 40}}}});
	return r;
}
inline cluster::rpc::SearchAnswer search_answer() { return {std::string("\x00\x01partial\xff", 10), 40, {8}}; }

// The decoders the fuzz target picks among by its first byte, and one encoded envelope of each.
constexpr unsigned decoders = 11;
inline std::vector<std::string> envelopes() {
	return {relayed().encode(),
			answer().encode(),
			stats_request().encode(),
			stats_answer().encode(),
			search_request().encode(),
			search_answer().encode(),
			cluster::rpc::CountAnswer{max64, {1, 2}}.encode(),
			cluster::rpc::InfoRequest{{9}}.encode(),
			cluster::rpc::InfoAnswer{MsgPack({{"7", {{"docs", 40}, {"leader", 2}}}})}.encode(),
			cluster::rpc::Keepalive{"node1", "127.0.0.1:8880", "127.0.0.1:9880", max64, 12}.encode(),
			cluster::rpc::KeepaliveAnswer{3, 99}.encode()};
}

} // namespace rpc_samples
