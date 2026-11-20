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
 * The shard RPC (docs/mechanisms/shard-rpc.md): Xapiand's node-to-node calls on consensus's
 * request lane, one connection per node pair (cluster-release D9). A call's stream is a header,
 * an envelope (the method's message) and a payload streamed under the lane's credit; a failure
 * is the lane's Error frame with a Status. Everything here runs on the call's loop
 * (HostTransport::loop_of_node) unless it says otherwise.
 */

#pragma once

#include <cstdint>                                  // for std::uint16_t, std::uint32_t, std::uint64_t
#include <deque>                                    // for std::deque
#include <functional>                               // for std::function
#include <memory>                                   // for std::shared_ptr
#include <optional>                                 // for std::optional
#include <string>                                   // for std::string
#include <string_view>                              // for std::string_view
#include <vector>                                   // for std::vector

#include <consensus/transport/host_transport.h>    // for HostTransport, RpcCall, rpc::

namespace cluster::rpc {

namespace lane = kronuz::consensus::transport::rpc;
using kronuz::consensus::NodeId;
using kronuz::consensus::transport::HostTransport;
using kronuz::consensus::transport::RpcCall;
using Clock = lane::Clock;
using Executors = std::vector<asio::any_io_executor>;  // one per transport loop, in its order

// The service's methods (contract 11).
enum class Method : std::uint32_t { forward_write = 1, stats, search_partial, get, count, info, keepalive };
inline constexpr std::uint32_t method_count = 7;
inline constexpr std::uint32_t method_base = 0x58530000;  // "XS": apart from other lane users' ids
inline constexpr lane::MethodId method_id(Method m) noexcept { return method_base + std::uint32_t(m); }
std::string_view name(Method m) noexcept;

// The versions of this service a build speaks; every call carries the caller's.
struct Versions {
	std::uint16_t min = 1, max = 1;
	bool contains(std::uint16_t v) const noexcept { return v >= min && v <= max; }
};
inline constexpr Versions versions{1, 1};

// The bytes before a stream's envelope: the request's version and envelope length, the
// response's envelope length.
inline constexpr std::size_t request_header = 8, response_header = 4;
// An envelope is buffered whole; this is the largest buffered body Xapiand accepts
// (search_service.h's 16 MiB). Anything larger is a payload, streamed.
inline constexpr std::uint32_t envelope_bytes = 16u << 20;

// A server's answer other than success, carried as the lane's Error code status_base + Status.
enum class Status : std::uint32_t {
	version_refused = 1,  // text: the server's range, "min max"
	not_leader,           // text: the leader hint's node id, empty when none
	unavailable,          // refused before proposing: no quorum, or a group closing
	unknown_group,        // no such group, or another incarnation, on this node
	not_found,
	conflict,
	bad_request,
	too_large,
	unknown_outcome,      // proposed, then cancelled or timed out: it may apply
	internal,
};
inline constexpr std::uint32_t status_base = 256;
inline constexpr std::uint32_t status_last = std::uint32_t(Status::internal);
std::string_view name(Status s) noexcept;

// How a call ended at its caller, when it did not succeed.
enum class Fault : std::uint8_t {
	status,          // the server answered a Status
	expired,         // the deadline passed before the call was sent
	timed_out,       // the deadline passed after it was sent
	cancelled,       // this end stopped it
	not_connected,   // no connection, or this host is stopping
	aborted,         // the connection ended with the call live
	not_processed,   // the peer drained before it started
	overloaded,      // the peer's serving bound or pool was full
	unknown_method,  // the peer serves no such method
	protocol,        // no shared lane version, or a stream that breaks this service's framing
	message_size,    // an envelope past envelope_bytes
	handler_failed,  // the peer's handler threw
};
std::string_view name(Fault f) noexcept;

struct Failure {
	Fault fault = Fault::protocol;
	Status status{};       // fault == status
	std::string text;
	NodeId leader = 0;     // not_leader's hint
	Versions peer{0, 0};   // version_refused's server range
	// The server never ran it, so any call may be sent again.
	bool retry_safe() const noexcept;
	// The HTTP status a receiving node answers when it gives up: retry-safe failures 503, a
	// failure after the server may have run 504, statuses their own.
	int http_status() const noexcept;
	std::string message() const;
};
// A call's end at the caller as the lane reports it; nullopt for success.
std::optional<Failure> failure_of(const lane::Status& status);

std::string encode_request_header(std::uint16_t version, std::uint32_t envelope);
std::string encode_response_header(std::uint32_t envelope);

// One end's outgoing stream of a call: bytes queued by its producer, written within the peer's
// credit. `total` is the stream's length, unknown_total when the producer ends it.
class Sender {
  public:
	Sender(HostTransport& transport, RpcCall call, std::uint64_t total, std::uint64_t sent)
		: transport_(&transport), call_(call), total_(total), sent_(sent),
		  end_(total != lane::unknown_total && sent == total), written_(end_) {}
	// Queues bytes; `end` ends the stream (implied when a known total is reached).
	void push(std::string bytes, bool end = false);
	// Writes what the credit allows; true once every byte is written.
	bool pump();
	std::uint64_t queued() const noexcept { return queued_; }
	bool ended() const noexcept { return end_; }
	std::uint64_t total() const noexcept { return total_; }

  private:
	HostTransport* transport_;
	RpcCall call_;
	std::uint64_t total_, sent_, queued_ = 0;
	std::deque<std::string> queue_;
	bool end_, written_;
};

// A stream's prefix, header then envelope, as the peer sends it.
class Prefix {
  public:
	explicit Prefix(std::size_t header) : header_(header) {}
	// Takes bytes of the stream from its start; returns how many belong to the prefix.
	std::size_t take(std::string_view bytes);
	bool complete() const noexcept { return complete_; }
	std::string_view header() const noexcept { return head_; }
	std::string& envelope() noexcept { return envelope_; }
	// The envelope length the header names; the caller checks it before take() goes on.
	std::uint32_t envelope_length() const noexcept { return length_; }
	bool too_large() const noexcept { return length_ > envelope_bytes; }

  private:
	std::size_t header_;
	std::string head_, envelope_;
	std::uint32_t length_ = 0;
	bool known_ = false, complete_ = false;
};

// A successful call's answer at the caller.
struct Reply {
	std::string envelope, payload;
};

// A call this host makes.
class Call : public std::enable_shared_from_this<Call> {
  public:
	struct Handlers {
		std::function<void(std::string& envelope)> reply;             // the answer's envelope, once
		std::function<void(std::string_view bytes, bool end)> payload;  // the answer's payload, in order
		std::function<void()> writable;                               // credit for more request payload
		std::function<void(const std::optional<Failure>&)> done;      // once, last: nullopt on success
	};
	Method method() const noexcept { return method_; }
	NodeId node() const noexcept { return call_.node; }
	// The request payload, within payload_total when known.
	void push(std::string bytes, bool end = false);
	// Request bytes pushed and not yet written: a producer keeps it near a chunk or two.
	std::uint64_t queued() const noexcept { return sender_ ? sender_->queued() : early_.size(); }
	// Cancels the call: the server's handler sees it cancelled.
	void cancel();
	bool ended() const noexcept { return ended_; }

  private:
	friend class Client;
	Call(HostTransport& transport, Method method, Handlers handlers)
		: transport_(&transport), method_(method), handlers_(std::move(handlers)), prefix_(response_header) {}
	void received(std::string_view bytes, bool end);
	void finished(const lane::Status& status);

	HostTransport* transport_;
	Method method_;
	Handlers handlers_;
	RpcCall call_{};
	std::optional<Sender> sender_;
	std::string early_;  // request bytes pushed before the call had a handle
	bool early_end_ = false;
	Prefix prefix_;
	std::optional<Failure> local_;  // this end's own reason for stopping, before the lane's
	bool ended_ = false;
};

// Makes calls. call() runs on the node's loop; unary() from any thread.
class Client {
  public:
	Client(HostTransport& transport, Executors loops) : transport_(transport), loops_(std::move(loops)) {}
	// Starts a call whose request payload is `payload_total` bytes (unknown_total: pushed to an
	// end) after `envelope`. `inline_payload` goes with the Call frame.
	std::shared_ptr<Call> call(NodeId node, Method method, std::string envelope, std::uint64_t payload_total,
							   Clock::time_point deadline, Call::Handlers handlers, std::string inline_payload = {},
							   std::uint16_t version = versions.max);
	// A buffered call: the whole request and answer in memory; `done` runs on the node's loop.
	void unary(NodeId node, Method method, std::string envelope, std::string payload, Clock::time_point deadline,
			   std::function<void(std::optional<Failure>, Reply)> done, std::uint16_t version = versions.max);
	// Runs `f` on the loop of calls to `node`.
	void post(NodeId node, std::function<void()> f);
	HostTransport& transport() noexcept { return transport_; }

  private:
	HostTransport& transport_;
	Executors loops_;
};

// A call this host serves, from its handler on.
class ServerCall : public std::enable_shared_from_this<ServerCall> {
  public:
	Method method() const noexcept { return method_; }
	NodeId node() const noexcept { return call_.node; }  // the caller, as its certificate authenticated it
	std::uint16_t version() const noexcept { return version_; }
	std::string& envelope() noexcept { return prefix_.envelope(); }
	// The request payload's length, unknown_total when the caller streams it to an end.
	std::uint64_t payload_total() const noexcept { return payload_total_; }
	// Where the request payload goes; bytes that came before it wait, holding the caller's credit.
	void payload(std::function<void(std::string_view bytes, bool end)> sink);
	// Starts the answer: its envelope, then `payload_total` bytes of push() (unknown_total: to an end).
	void reply(std::string envelope, std::uint64_t payload_total = 0, std::string payload = {});
	void push(std::string bytes, bool end = false);
	std::uint64_t queued() const noexcept { return sender_ ? sender_->queued() : 0; }
	std::function<void()> writable;  // credit for more answer payload
	// Ends the call with a Status; `text` at most lane::text_bytes.
	void fail(Status status, std::string_view text = {});
	// Once, when the call ends here: nullopt when the answer was queued whole.
	std::function<void(const std::optional<Failure>&)> done;
	bool ended() const noexcept { return ended_; }
	// From any thread: runs `f` on this call's loop unless it ended.
	void post(std::function<void()> f);

  private:
	friend class Service;
	using Handler = std::function<void(std::shared_ptr<ServerCall>)>;
	ServerCall(HostTransport& transport, asio::any_io_executor loop, RpcCall call, Method method, std::uint64_t total,
			   Versions range, const Handler& handler)
		: transport_(&transport), loop_(std::move(loop)), call_(call), method_(method), total_(total), range_(range),
		  handler_(&handler), prefix_(request_header) {}
	void received(std::string_view bytes, bool end);
	void deliver(std::string_view bytes, bool end);
	void finished(const lane::Status& status);

	HostTransport* transport_;
	asio::any_io_executor loop_;
	RpcCall call_;
	Method method_;
	Versions range_;
	const Handler* handler_;  // the Service's, which outlives its calls
	std::uint64_t total_, payload_total_ = 0;
	std::uint16_t version_ = 0;
	Prefix prefix_;
	std::function<void(std::string_view, bool)> sink_;
	std::string held_;      // payload bytes before the sink, not consumed
	bool held_end_ = false;
	std::optional<Sender> sender_;
	bool ended_ = false;
};

// Serves methods on a transport; serve() before the transport's serve().
class Service {
  public:
	using Handler = std::function<void(std::shared_ptr<ServerCall>)>;
	Service(HostTransport& transport, Executors loops, Versions range = versions)
		: transport_(transport), loops_(std::move(loops)), range_(range) {}
	// The handler runs once the envelope is in, on the call's loop.
	void serve(Method method, Handler handler);

  private:
	HostTransport& transport_;
	Executors loops_;
	Versions range_;
	std::unique_ptr<Handler> handlers_[method_count];
};

} // namespace cluster::rpc
