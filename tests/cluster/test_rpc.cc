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

// The shard RPC's call layer between two hosts over loopback TLS (docs/mechanisms/shard-rpc.md):
// framing, streamed request and answer payloads under credit, each Status and lane outcome as
// the caller sees it, deadlines, cancellation, the version range and the envelope bound.
//   xapiand_cluster_test_rpc [case]

#include "rpc_pair.h"
#include "test_harness.h"

#include <atomic>     // for std::atomic
#include <cstring>    // for std::strcmp
#include <mutex>      // for std::mutex

namespace {

// get: the envelope reversed, the payload echoed; an envelope past one chunk, a 1 MiB payload.
void test_unary() {
	Pair p;
	p.service->serve(rpc::Method::get, [](std::shared_ptr<rpc::ServerCall> call) {
		auto payload = std::make_shared<std::string>();
		std::string envelope(call->envelope().rbegin(), call->envelope().rend());
		call->payload([call, payload, envelope](std::string_view bytes, bool end) {
			payload->append(bytes);
			if (end)
				call->reply(envelope, payload->size(), *payload);
		});
	});
	p.start();
	const auto envelope = bytes_at(7, 100'000), payload = bytes_at(11, 1 << 20);
	auto o = p.unary(rpc::Method::get, envelope, payload);
	REQUIRE(o->wait());
	CHECK(!o->failure);
	CHECK(o->reply.envelope == std::string(envelope.rbegin(), envelope.rend()));
	CHECK(o->reply.payload == payload);
	// An empty envelope and payload both ways.
	auto e = p.unary(rpc::Method::get, "");
	REQUIRE(e->wait());
	CHECK(!e->failure);
	CHECK(e->reply.envelope.empty());
	CHECK(e->reply.payload.empty());
}

// forward_write streams a 32 MiB request in pushes on credit; the server checks every byte and
// answers its count. get streams a 32 MiB answer the same way.
void test_streams() {
	constexpr std::uint64_t size = 32u << 20;
	Pair p;
	std::atomic<std::uint64_t> server_mismatched{0}, server_total{0};
	// Handlers run on a loop thread: they record, and the main thread checks.
	p.service->serve(rpc::Method::forward_write, [&](std::shared_ptr<rpc::ServerCall> call) {
		server_total = call->payload_total();
		auto got = std::make_shared<std::uint64_t>(0);
		call->payload([&, call, got](std::string_view bytes, bool end) {
			for (std::size_t i = 0; i != bytes.size(); ++i)
				server_mismatched += bytes[i] != byte_at(*got + i);
			*got += bytes.size();
			if (end)
				call->reply(std::to_string(*got));
		});
	});
	p.service->serve(rpc::Method::get, [](std::shared_ptr<rpc::ServerCall> call) {
		auto sent = std::make_shared<std::uint64_t>(0);
		call->reply("blob", size);
		auto fill = [call, sent] {
			while (*sent < size && call->queued() < (1u << 20)) {
				const auto n = std::min<std::uint64_t>(256 << 10, size - *sent);
				call->push(bytes_at(*sent, n));
				*sent += n;
			}
		};
		call->writable = fill;
		fill();
	});
	p.start();

	auto up = std::make_shared<Outcome>();
	p.one.at(2, [&] {
		auto sent = std::make_shared<std::uint64_t>(0);
		auto call = std::make_shared<std::shared_ptr<rpc::Call>>();
		rpc::Call::Handlers h;
		h.reply = [up](std::string& e) { up->reply.envelope = e; };
		h.writable = [call, sent] {
			while (*sent < size && *call && (*call)->queued() < (1u << 20)) {
				const auto n = std::min<std::uint64_t>(256 << 10, size - *sent);
				(*call)->push(bytes_at(*sent, n));
				*sent += n;
			}
		};
		h.done = [up](const std::optional<rpc::Failure>& f) { up->end(f); };
		*call = p.client->call(2, rpc::Method::forward_write, "write", size, rpc::Clock::now() + 30s, h);
		h.writable();
	});
	REQUIRE(up->wait(30s));
	CHECK(!up->failure);
	CHECK(up->reply.envelope == std::to_string(size));
	CHECK_EQ(server_total.load(), size);
	CHECK_EQ(server_mismatched.load(), 0u);

	auto down = std::make_shared<Outcome>();
	p.one.at(2, [&] {
		rpc::Call::Handlers h;
		h.reply = [down](std::string& e) { down->reply.envelope = e; };
		h.payload = [down](std::string_view bytes, bool) {
			for (std::size_t i = 0; i != bytes.size(); ++i)
				down->mismatched += bytes[i] != byte_at(down->received + i);
			down->received += bytes.size();
		};
		h.done = [down](const std::optional<rpc::Failure>& f) { down->end(f); };
		p.client->call(2, rpc::Method::get, "key", 0, rpc::Clock::now() + 30s, h);
	});
	REQUIRE(down->wait(30s));
	CHECK(!down->failure);
	CHECK(down->reply.envelope == "blob");
	CHECK_EQ(down->received.load(), size);
	CHECK_EQ(down->mismatched.load(), 0u);
	// The pools are back to zero on both ends.
	CHECK(until([&] { return p.one.transport->calling_pool().used() == 0 && p.two.transport->serving_pool().used() == 0; }, 2s));
}

// Every Status reaches the caller with its text and HTTP status; not_leader carries its hint.
void test_statuses() {
	Pair p;
	p.service->serve(rpc::Method::info, [](std::shared_ptr<rpc::ServerCall> call) {
		const auto code = std::stoul(call->envelope());
		const auto status = rpc::Status(code);
		call->fail(status, status == rpc::Status::not_leader ? "3" : "because " + std::to_string(code));
	});
	p.start();
	struct Want {
		rpc::Status status;
		int http;
		bool safe;
	};
	const Want wants[] = {
		{rpc::Status::not_leader, 503, true},       {rpc::Status::unavailable, 503, true},
		{rpc::Status::unknown_group, 503, true},    {rpc::Status::not_found, 404, true},
		{rpc::Status::conflict, 409, true},         {rpc::Status::bad_request, 400, true},
		{rpc::Status::too_large, 413, true},        {rpc::Status::unknown_outcome, 504, false},
		{rpc::Status::internal, 500, false},
	};
	for (const auto& w : wants) {
		auto o = p.unary(rpc::Method::info, std::to_string(std::uint32_t(w.status)));
		REQUIRE(o->wait());
		REQUIRE(o->failure.has_value());
		CHECK(o->failure->fault == rpc::Fault::status);
		CHECK(o->failure->status == w.status);
		CHECK_EQ(o->failure->http_status(), w.http);
		CHECK_EQ(o->failure->retry_safe(), w.safe);
		if (w.status == rpc::Status::not_leader) {
			CHECK_EQ(o->failure->leader, NodeId{3});
		} else {
			CHECK(o->failure->text == "because " + std::to_string(std::uint32_t(w.status)));
		}
	}
}

// A handler that throws ends its own call handler_failed, with the exception's text.
void test_handler_throws() {
	Pair p;
	p.service->serve(rpc::Method::count, [](std::shared_ptr<rpc::ServerCall>) { throw std::runtime_error("boom"); });
	p.service->serve(rpc::Method::info, [](std::shared_ptr<rpc::ServerCall> call) { call->reply("fine"); });
	p.start();
	auto o = p.unary(rpc::Method::count, "x");
	REQUIRE(o->wait());
	REQUIRE(o->failure.has_value());
	CHECK(o->failure->fault == rpc::Fault::handler_failed);
	CHECK(o->failure->text == "boom");
	CHECK_EQ(o->failure->http_status(), 500);
	CHECK_FALSE(o->failure->retry_safe());
	// The connection goes on: the next call succeeds.
	auto next = p.unary(rpc::Method::info, "");
	REQUIRE(next->wait());
	CHECK(!next->failure);
	CHECK(next->reply.envelope == "fine");
}

// A caller outside the server's range is refused version_refused with the server's range.
void test_versions() {
	{
		Pair p;
		std::atomic<int> ran{0};
		p.service->serve(rpc::Method::keepalive, [&](std::shared_ptr<rpc::ServerCall> call) {
			++ran;
			call->reply("ok");
		});
		p.start();
		auto o = p.unary(rpc::Method::keepalive, "", "", 10s, 2);
		REQUIRE(o->wait());
		REQUIRE(o->failure.has_value());
		CHECK(o->failure->status == rpc::Status::version_refused);
		CHECK_EQ(o->failure->peer.min, 1);
		CHECK_EQ(o->failure->peer.max, 1);
		CHECK_EQ(o->failure->http_status(), 502);
		CHECK(o->failure->retry_safe());
		CHECK_EQ(ran.load(), 0);
		auto ok = p.unary(rpc::Method::keepalive, "");
		REQUIRE(ok->wait());
		CHECK(!ok->failure);
		CHECK_EQ(ran.load(), 1);
	}
	{
		Pair p({2, 3});  // a newer server: an N - 1 caller is refused by name
		p.service->serve(rpc::Method::keepalive, [](std::shared_ptr<rpc::ServerCall> call) { call->reply("ok"); });
		p.start();
		auto o = p.unary(rpc::Method::keepalive, "", "", 10s, 1);
		REQUIRE(o->wait());
		REQUIRE(o->failure.has_value());
		CHECK(o->failure->status == rpc::Status::version_refused);
		CHECK_EQ(o->failure->peer.min, 2);
		CHECK_EQ(o->failure->peer.max, 3);
		CHECK(o->failure->message() == "shard rpc: version_refused: 2 3");
	}
}

// A handler that never answers: the caller's deadline ends both ends, near the deadline.
void test_deadline() {
	Pair p;
	auto server = std::make_shared<Outcome>();
	std::shared_ptr<rpc::ServerCall> held;
	p.service->serve(rpc::Method::search_partial, [&, server](std::shared_ptr<rpc::ServerCall> call) {
		held = call;  // keeps it, never answers
		call->done = [server](const std::optional<rpc::Failure>& f) { server->end(f); };
	});
	p.start();
	auto o = p.unary(rpc::Method::search_partial, "q", "", 300ms);
	REQUIRE(o->wait(5s));
	REQUIRE(o->failure.has_value());
	CHECK(o->failure->fault == rpc::Fault::timed_out);
	CHECK_FALSE(o->failure->retry_safe());
	CHECK_EQ(o->failure->http_status(), 504);
	CHECK_GE(o->ms(), 250);
	CHECK_LT(o->ms(), 2000);
	REQUIRE(server->wait(5s));
	REQUIRE(server->failure.has_value());
	CHECK(server->failure->fault == rpc::Fault::timed_out || server->failure->fault == rpc::Fault::cancelled);
	p.two.at(1, [&] { held.reset(); });
}

// The caller cancels mid-stream: the server's call ends cancelled, the caller's too, once each.
void test_cancel() {
	Pair p;
	auto server = std::make_shared<Outcome>();
	std::atomic<int> server_ends{0};
	p.service->serve(rpc::Method::forward_write, [&, server](std::shared_ptr<rpc::ServerCall> call) {
		call->payload([server](std::string_view bytes, bool) { server->received += bytes.size(); });
		call->done = [&, server](const std::optional<rpc::Failure>& f) {
			++server_ends;
			server->end(f);
		};
	});
	p.start();
	auto o = std::make_shared<Outcome>();
	std::atomic<int> client_ends{0};
	std::shared_ptr<rpc::Call> call;
	p.one.at(2, [&] {
		rpc::Call::Handlers h;
		h.done = [&, o](const std::optional<rpc::Failure>& f) {
			++client_ends;
			o->end(f);
		};
		call = p.client->call(2, rpc::Method::forward_write, "w", 1u << 30, rpc::Clock::now() + 30s, h);
		call->push(bytes_at(0, 64 << 10));
	});
	REQUIRE(until([&] { return server->received.load() >= (64u << 10); }, 5s));
	p.one.at(2, [&] { call->cancel(); });
	REQUIRE(o->wait());
	REQUIRE(server->wait());
	REQUIRE(o->failure.has_value());
	CHECK(o->failure->fault == rpc::Fault::cancelled);
	REQUIRE(server->failure.has_value());
	CHECK(server->failure->fault == rpc::Fault::cancelled);
	std::this_thread::sleep_for(50ms);
	CHECK_EQ(client_ends.load(), 1);
	CHECK_EQ(server_ends.load(), 1);
	p.one.at(2, [&] { call.reset(); });
}

// Lane outcomes by name: a method nobody serves, a node with no address.
void test_lane_faults() {
	Pair p;
	p.start();
	auto o = p.unary(rpc::Method::stats, "s");
	REQUIRE(o->wait());
	REQUIRE(o->failure.has_value());
	CHECK(o->failure->fault == rpc::Fault::unknown_method);
	CHECK(o->failure->retry_safe());
	auto n = p.unary(rpc::Method::stats, "s", "", 1s, rpc::versions.max, 3);  // node 3 has no address
	REQUIRE(n->wait());
	REQUIRE(n->failure.has_value());
	CHECK(n->failure->fault == rpc::Fault::not_connected);
	CHECK(n->failure->retry_safe());
	CHECK_EQ(n->failure->http_status(), 503);
}

// A request header naming an envelope past envelope_bytes is refused too_large before any of it
// is buffered; a bad version word bad_request. Raw calls on the lane, past the client's checks.
void test_framing() {
	Pair p;
	std::atomic<int> ran{0};
	p.service->serve(rpc::Method::get, [&](std::shared_ptr<rpc::ServerCall> call) {
		++ran;
		call->reply("");
	});
	p.start();
	auto raw = [&](std::string header) {
		auto o = std::make_shared<Outcome>();
		p.one.at(2, [&] {
			kronuz::consensus::transport::rpc::Events e;
			e.done = [o](const kronuz::consensus::transport::rpc::Status& s) { o->end(rpc::failure_of(s)); };
			kronuz::consensus::transport::rpc::CallOptions options;
			options.deadline = rpc::Clock::now() + 5s;
			options.total = kronuz::consensus::transport::rpc::unknown_total;
			p.one.transport->call(2, rpc::method_id(rpc::Method::get), options, std::move(header), e);
		});
		return o;
	};
	auto big = raw(rpc::encode_request_header(1, rpc::envelope_bytes + 1));
	REQUIRE(big->wait());
	REQUIRE(big->failure.has_value());
	CHECK(big->failure->status == rpc::Status::too_large);
	auto word = raw(std::string("\x01\x00\x01\x00\x00\x00\x00\x00", 8));  // version word 0x10001
	REQUIRE(word->wait());
	REQUIRE(word->failure.has_value());
	CHECK(word->failure->status == rpc::Status::bad_request);
	CHECK_EQ(ran.load(), 0);
	CHECK_THROW(p.client->call(2, rpc::Method::get, std::string(rpc::envelope_bytes + 1, 'x'), 0, rpc::Clock::now() + 1s, {}),
				std::invalid_argument);
}

} // namespace

int main(int argc, char** argv) {
	struct Case {
		const char* name;
		void (*run)();
	};
	const Case cases[] = {
		{"unary", test_unary},       {"streams", test_streams},     {"statuses", test_statuses},
		{"handler-throws", test_handler_throws}, {"versions", test_versions}, {"deadline", test_deadline},
		{"cancel", test_cancel},     {"lane-faults", test_lane_faults}, {"framing", test_framing},
	};
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
