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

// The shard RPC's seven methods with their envelopes (src/cluster/rpc_methods.h):
//   codec      each message round-trips with values, extremes included
//   malformed  truncations, wrong types, missing keys, trailing bytes and 20,000 random
//              mutations decode or throw Malformed, nothing else
//   wire       each method between two hosts with real messages; forward_write and get stream
//              their bodies; a malformed envelope is answered bad_request
//   xapiand_cluster_test_rpc_methods [case]

#include "cluster/rpc_methods.h"
#include "rpc_pair.h"
#include "rpc_samples.h"
#include "test_harness.h"

#include <cstring>  // for std::strcmp
#include <limits>   // for std::numeric_limits
#include <random>   // for std::mt19937_64

namespace {

using namespace rpc_samples;

void test_codec() {
	CHECK(rpc::Relayed::decode(relayed().encode()) == relayed());
	CHECK(rpc::Relayed::decode(rpc::Relayed{}.encode()) == rpc::Relayed{});
	CHECK(rpc::Answer::decode(answer().encode()) == answer());
	CHECK(rpc::StatsRequest::decode(stats_request().encode()) == stats_request());
	CHECK(rpc::StatsAnswer::decode(stats_answer().encode()) == stats_answer());
	const auto s = rpc::SearchRequest::decode(search_request().encode());
	CHECK(s.groups == search_request().groups);
	CHECK(s.query == search_request().query);
	CHECK(s.params == search_request().params);
	CHECK(s.stats == search_request().stats);
	CHECK(rpc::SearchAnswer::decode(search_answer().encode()) == search_answer());
	CHECK((rpc::CountAnswer::decode(rpc::CountAnswer{max64, {1, 2}}.encode()) == rpc::CountAnswer{max64, {1, 2}}));
	CHECK((rpc::InfoRequest::decode(rpc::InfoRequest{{9}}.encode()) == rpc::InfoRequest{{9}}));
	rpc::InfoAnswer info{MsgPack({{"7", {{"docs", 40}, {"leader", 2}}}})};
	CHECK(rpc::InfoAnswer::decode(info.encode()).info == info.info);
	const rpc::Keepalive k{"node1", "127.0.0.1:8880", "127.0.0.1:9880", max64, 12};
	CHECK(rpc::Keepalive::decode(k.encode()) == k);
	CHECK((rpc::KeepaliveAnswer::decode(rpc::KeepaliveAnswer{3, 99}.encode()) == rpc::KeepaliveAnswer{3, 99}));
}

template <class T> bool malformed(std::string_view bytes) {
	try {
		(void)T::decode(bytes);
		return false;
	} catch (const rpc::Malformed&) {
		return true;
	}
}

// Every byte string decodes or throws Malformed; nothing else escapes.
template <class T> bool decodes_or_malformed(std::string_view bytes) {
	try {
		(void)T::decode(bytes);
	} catch (const rpc::Malformed&) {
	} catch (...) {
		return false;
	}
	return true;
}

void test_malformed() {
	const auto good = relayed().encode();
	for (std::size_t n = 0; n < good.size(); ++n)
		CHECK(malformed<rpc::Relayed>(std::string_view(good).substr(0, n)));
	CHECK(malformed<rpc::Relayed>(good + "x"));                                   // trailing bytes
	CHECK(malformed<rpc::Relayed>(MsgPack::ARRAY().serialize()));                 // not a map
	CHECK(malformed<rpc::Relayed>(MsgPack({{"v", "PUT"}}).serialize()));          // missing keys
	auto wrong = MsgPack::unserialize(good);
	wrong["h"] = 256;  // hops past a byte
	CHECK(malformed<rpc::Relayed>(wrong.serialize()));
	wrong = MsgPack::unserialize(good);
	wrong["g"] = -1;  // a negative group
	CHECK(malformed<rpc::Relayed>(wrong.serialize()));
	wrong = MsgPack::unserialize(good);
	wrong["p"] = 7;  // a path that is not a string
	CHECK(malformed<rpc::Relayed>(wrong.serialize()));
	auto stats = MsgPack::unserialize(stats_answer().encode());
	stats["g"].at(0)["t"]["text"]["hello"] = MsgPack({1});  // df without cf
	CHECK(malformed<rpc::StatsAnswer>(stats.serialize()));
	CHECK(malformed<rpc::InfoAnswer>(MsgPack({{"i", 1}}).serialize()));
	// A newer sender's extra key is ignored.
	auto newer = MsgPack::unserialize(good);
	newer["zz"] = "later";
	CHECK(rpc::Relayed::decode(newer.serialize()) == relayed());

	const std::string seeds[] = {good, answer().encode(), stats_request().encode(), stats_answer().encode(),
								 search_request().encode(), search_answer().encode(), rpc::Keepalive{}.encode()};
	std::mt19937_64 random(20261120);
	std::size_t escaped = 0;
	for (int i = 0; i < 20000; ++i) {
		std::string b = seeds[i % std::size(seeds)];
		for (int m = 1 + int(random() % 4); m--;) {
			const auto at = random() % (b.size() + 1);
			switch (random() % 3) {
				case 0: if (at < b.size()) b[at] = char(random()); break;
				case 1: b.insert(b.begin() + std::ptrdiff_t(at), char(random())); break;
				default: b.erase(at, 1 + random() % 4); break;
			}
		}
		escaped += !decodes_or_malformed<rpc::Relayed>(b) + !decodes_or_malformed<rpc::Answer>(b) +
				   !decodes_or_malformed<rpc::StatsRequest>(b) + !decodes_or_malformed<rpc::StatsAnswer>(b) +
				   !decodes_or_malformed<rpc::SearchRequest>(b) + !decodes_or_malformed<rpc::SearchAnswer>(b) +
				   !decodes_or_malformed<rpc::CountAnswer>(b) + !decodes_or_malformed<rpc::InfoRequest>(b) +
				   !decodes_or_malformed<rpc::InfoAnswer>(b) + !decodes_or_malformed<rpc::Keepalive>(b) +
				   !decodes_or_malformed<rpc::KeepaliveAnswer>(b);
	}
	CHECK_EQ(escaped, 0u);
}

void test_wire() {
	Pair p;
	auto& s = *p.service;
	// forward_write: the relayed request's body streamed in, the answer's body streamed out.
	s.serve(rpc::Method::forward_write, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::Relayed>(*call);
		if (!r)
			return;
		auto body = std::make_shared<std::string>();
		call->payload([call, body, r = *r](std::string_view bytes, bool end) {
			body->append(bytes);
			if (!end)
				return;
			rpc::Answer a{201, r.content_type, {{"X-Path", r.path}, {"X-Hops", std::to_string(r.hops)}}};
			const auto echo = std::to_string(body->size()) + ":" + r.verb;
			call->reply(a.encode(), echo.size(), echo);
		});
	});
	// get: the blob streamed out.
	s.serve(rpc::Method::get, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::Relayed>(*call);
		if (!r)
			return;
		const auto blob = bytes_at(r->group, 1 << 20);
		call->reply(answer().encode(), blob.size(), blob);
	});
	s.serve(rpc::Method::stats, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::StatsRequest>(*call);
		if (!r)
			return;
		rpc::StatsAnswer a;
		for (auto g : r->groups) {
			rpc::GroupStats gs{g, g * 10, g * 100, {}};
			for (const auto& [field, term] : r->terms)
				gs.terms[field][term] = {g, g + term.size()};
			a.groups.push_back(gs);
		}
		call->reply(a.encode());
	});
	s.serve(rpc::Method::search_partial, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::SearchRequest>(*call);
		if (!r)
			return;
		call->reply(rpc::SearchAnswer{r->query.serialize(), r->groups.size(), {r->groups.back()}}.encode());
	});
	s.serve(rpc::Method::count, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::SearchRequest>(*call);
		if (!r)
			return;
		call->reply(rpc::CountAnswer{r->groups.size() * 1000, {}}.encode());
	});
	s.serve(rpc::Method::info, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::InfoRequest>(*call);
		if (!r)
			return;
		MsgPack info = MsgPack::MAP();
		for (auto g : r->groups)
			info[std::to_string(g)] = MsgPack({{"docs", g}});
		call->reply(rpc::InfoAnswer{info}.encode());
	});
	s.serve(rpc::Method::keepalive, [](std::shared_ptr<rpc::ServerCall> call) {
		auto r = rpc::request<rpc::Keepalive>(*call);
		if (!r)
			return;
		call->reply(rpc::KeepaliveAnswer{call->node(), r->boot + 1}.encode());  // the caller's node, as authenticated
	});
	p.start();

	auto w = p.unary(rpc::Method::forward_write, relayed().encode(), bytes_at(0, 3 << 20));
	REQUIRE(w->wait());
	REQUIRE(!w->failure);
	const auto wa = rpc::Answer::decode(w->reply.envelope);
	CHECK_EQ(wa.status, 201);
	CHECK((wa.headers == std::vector<std::pair<std::string, std::string>>{{"X-Path", "/twitter/user/42"}, {"X-Hops", "1"}}));
	CHECK(w->reply.payload == std::to_string(3 << 20) + ":PUT");

	auto g = p.unary(rpc::Method::get, relayed().encode());
	REQUIRE(g->wait());
	REQUIRE(!g->failure);
	CHECK(rpc::Answer::decode(g->reply.envelope) == answer());
	CHECK(g->reply.payload == bytes_at(relayed().group, 1 << 20));

	auto st = p.unary(rpc::Method::stats, stats_request().encode());
	REQUIRE(st->wait());
	REQUIRE(!st->failure);
	const auto sa = rpc::StatsAnswer::decode(st->reply.envelope);
	REQUIRE_EQ(sa.groups.size(), 3u);
	CHECK_EQ(sa.groups[1].doc_count, 20u);
	CHECK_EQ(sa.groups[1].total_length, 200u);
	CHECK((sa.groups[1].terms.at("text").at("wörld") == rpc::TermStats{2, 2 + std::string("wörld").size()}));
	CHECK((sa.groups[2].terms.at("").at(std::string("\0b", 2)) == rpc::TermStats{max64, max64 + 2}));

	auto sp = p.unary(rpc::Method::search_partial, search_request().encode());
	REQUIRE(sp->wait());
	REQUIRE(!sp->failure);
	const auto spa = rpc::SearchAnswer::decode(sp->reply.envelope);
	CHECK(MsgPack::unserialize(spa.partial) == search_request().query);
	CHECK_EQ(spa.matches, 2u);
	CHECK((spa.failed == std::vector<std::uint64_t>{8}));

	auto c = p.unary(rpc::Method::count, search_request().encode());
	REQUIRE(c->wait());
	REQUIRE(!c->failure);
	CHECK_EQ(rpc::CountAnswer::decode(c->reply.envelope).count, 2000u);

	auto i = p.unary(rpc::Method::info, rpc::InfoRequest{{5, 6}}.encode());
	REQUIRE(i->wait());
	REQUIRE(!i->failure);
	CHECK(rpc::InfoAnswer::decode(i->reply.envelope).info == MsgPack({{"5", {{"docs", 5}}}, {"6", {{"docs", 6}}}}));

	auto k = p.unary(rpc::Method::keepalive, rpc::Keepalive{"node1", "a:1", "b:2", 41, 0}.encode());
	REQUIRE(k->wait());
	REQUIRE(!k->failure);
	CHECK((rpc::KeepaliveAnswer::decode(k->reply.envelope) == rpc::KeepaliveAnswer{1, 42}));

	// A malformed envelope is answered bad_request, with the decoder's reason.
	auto bad = p.unary(rpc::Method::keepalive, MsgPack({{"n", "x"}}).serialize());
	REQUIRE(bad->wait());
	REQUIRE(bad->failure.has_value());
	CHECK(bad->failure->status == rpc::Status::bad_request);
	CHECK(bad->failure->text.find("lacks") != std::string::npos);
}

} // namespace

int main(int argc, char** argv) {
	struct Case {
		const char* name;
		void (*run)();
	};
	const Case cases[] = {{"codec", test_codec}, {"malformed", test_malformed}, {"wire", test_wire}};
	for (const auto& c : cases) {
		if (argc > 1 && std::strcmp(argv[1], c.name) != 0)
			continue;
		std::printf("%s\n", c.name);
		c.run();
	}
	if (testh::failures) {
		std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
		return 1;
	}
	std::printf("OK: %d checks passed\n", testh::checks);
	return 0;
}
