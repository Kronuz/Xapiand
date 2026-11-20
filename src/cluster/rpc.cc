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

#include "cluster/rpc.h"

#include <algorithm>                // for std::min
#include <charconv>                 // for std::from_chars
#include <stdexcept>                // for std::invalid_argument, std::logic_error

#include <asio/post.hpp>            // for asio::post

namespace cluster::rpc {

namespace {

constexpr std::uint64_t unknown = lane::unknown_total;

std::uint32_t read32(std::string_view bytes) {
	return kronuz::journal::get32(bytes);
}

// A decimal at the start of `text`, and where it ends.
template <class T> bool parse(std::string_view& text, T& value) {
	auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (ec != std::errc{} || end == text.data())
		return false;
	text.remove_prefix(std::size_t(end - text.data()));
	return true;
}

Failure fault(Fault f, std::string text = {}) {
	Failure r;
	r.fault = f;
	r.text = std::move(text);
	return r;
}

} // namespace

std::string_view name(Method m) noexcept {
	switch (m) {
		case Method::forward_write: return "forward_write";
		case Method::stats: return "stats";
		case Method::search_partial: return "search_partial";
		case Method::get: return "get";
		case Method::count: return "count";
		case Method::info: return "info";
		case Method::keepalive: return "keepalive";
	}
	return "unknown";
}

std::string_view name(Status s) noexcept {
	switch (s) {
		case Status::version_refused: return "version_refused";
		case Status::not_leader: return "not_leader";
		case Status::unavailable: return "unavailable";
		case Status::unknown_group: return "unknown_group";
		case Status::not_found: return "not_found";
		case Status::conflict: return "conflict";
		case Status::bad_request: return "bad_request";
		case Status::too_large: return "too_large";
		case Status::unknown_outcome: return "unknown_outcome";
		case Status::internal: return "internal";
	}
	return "unknown";
}

std::string_view name(Fault f) noexcept {
	switch (f) {
		case Fault::status: return "status";
		case Fault::expired: return "expired";
		case Fault::timed_out: return "timed_out";
		case Fault::cancelled: return "cancelled";
		case Fault::not_connected: return "not_connected";
		case Fault::aborted: return "aborted";
		case Fault::not_processed: return "not_processed";
		case Fault::overloaded: return "overloaded";
		case Fault::unknown_method: return "unknown_method";
		case Fault::protocol: return "protocol";
		case Fault::message_size: return "message_size";
		case Fault::handler_failed: return "handler_failed";
	}
	return "unknown";
}

bool Failure::retry_safe() const noexcept {
	switch (fault) {
		case Fault::status:
			return status != Status::unknown_outcome && status != Status::internal;
		case Fault::expired:
		case Fault::not_connected:
		case Fault::not_processed:
		case Fault::overloaded:
		case Fault::unknown_method:
		case Fault::protocol:
			return true;
		default:
			return false;
	}
}

int Failure::http_status() const noexcept {
	if (fault == Fault::status) {
		switch (status) {
			case Status::version_refused: return 502;
			case Status::not_leader:
			case Status::unavailable:
			case Status::unknown_group: return 503;
			case Status::not_found: return 404;
			case Status::conflict: return 409;
			case Status::bad_request: return 400;
			case Status::too_large: return 413;
			case Status::unknown_outcome: return 504;
			case Status::internal: return 500;
		}
	}
	switch (fault) {
		case Fault::unknown_method:
		case Fault::protocol:
		case Fault::message_size: return 502;
		case Fault::handler_failed: return 500;
		default: return retry_safe() ? 503 : 504;
	}
}

std::string Failure::message() const {
	std::string m = "shard rpc: ";
	m += fault == Fault::status ? name(status) : name(fault);
	if (!text.empty()) {
		m += ": ";
		m += text;
	}
	return m;
}

std::optional<Failure> failure_of(const lane::Status& s) {
	using O = lane::Outcome;
	using C = lane::Code;
	switch (s.outcome) {
		case O::ok:
			return std::nullopt;
		case O::error:
			if (s.code >= status_base) {
				const auto code = s.code - status_base;
				if (!code || code > status_last)
					return fault(Fault::protocol, "status " + std::to_string(s.code));
				Failure f = fault(Fault::status, s.text);
				f.status = Status(code);
				std::string_view text = s.text;
				if (f.status == Status::not_leader && !text.empty() && !(parse(text, f.leader) && text.empty()))
					f.leader = 0;
				if (f.status == Status::version_refused) {
					Versions v{0, 0};
					if (parse(text, v.min) && text.size() > 1 && text[0] == ' ' && (text.remove_prefix(1), parse(text, v.max)))
						f.peer = v;
				}
				return f;
			}
			switch (C(s.code)) {
				case C::unknown_method: return fault(Fault::unknown_method, s.text);
				case C::overloaded: return fault(Fault::overloaded, s.text);
				case C::deadline_exceeded: return fault(Fault::timed_out, s.text);
				case C::cancelled: return fault(Fault::cancelled, s.text);
				case C::message_size: return fault(Fault::message_size, s.text);
				case C::handler_failed: return fault(Fault::handler_failed, s.text);
				default: return fault(Fault::protocol, s.text);
			}
		case O::cancel:
			switch (C(s.code)) {
				case C::deadline_exceeded: return fault(Fault::timed_out, s.text);
				case C::message_size: return fault(Fault::message_size, s.text);
				default: return fault(Fault::cancelled, s.text);
			}
		case O::not_processed: return fault(Fault::not_processed, s.text);
		case O::connection_aborted: return fault(Fault::aborted, s.text);
		case O::timed_out: return fault(Fault::expired, s.text);
		case O::not_connected: return fault(Fault::not_connected, s.text);
		case O::protocol_not_supported: return fault(Fault::protocol, "no shared lane version");
	}
	return fault(Fault::protocol, s.text);
}

std::string encode_request_header(std::uint16_t version, std::uint32_t envelope) {
	std::string h;
	h.reserve(request_header);
	kronuz::journal::put32(h, version);
	kronuz::journal::put32(h, envelope);
	return h;
}

std::string encode_response_header(std::uint32_t envelope) {
	std::string h;
	h.reserve(response_header);
	kronuz::journal::put32(h, envelope);
	return h;
}

// Sender

void Sender::push(std::string bytes, bool end) {
	if (end_ && (!bytes.empty() || !end))
		throw std::logic_error("shard rpc: push after the stream's end");
	if (total_ != unknown && sent_ + queued_ + bytes.size() > total_)
		throw std::logic_error("shard rpc: push past the stream's total");
	if (end && total_ != unknown && sent_ + queued_ + bytes.size() != total_)
		throw std::logic_error("shard rpc: end before the stream's total");
	queued_ += bytes.size();
	if (!bytes.empty())
		queue_.push_back(std::move(bytes));
	end_ = end_ || end || (total_ != unknown && sent_ + queued_ == total_);
	pump();
}

bool Sender::pump() {
	while (!written_) {
		if (queue_.empty()) {
			if (!end_)
				return false;
			if (total_ == unknown)
				transport_->write(call_, {}, true);  // an unknown total ends with an empty chunk
			written_ = true;
			break;
		}
		const auto n = transport_->sendable(call_);
		if (!n)
			return false;
		auto& front = queue_.front();
		std::string piece;
		if (front.size() <= n) {
			piece = std::move(front);
			queue_.pop_front();
		} else {
			piece = front.substr(0, n);
			front.erase(0, n);
		}
		sent_ += piece.size();
		queued_ -= piece.size();
		const bool last = total_ != unknown ? sent_ == total_ : end_ && queue_.empty();
		transport_->write(call_, std::move(piece), last);
		written_ = last;
	}
	return true;
}

// Prefix

std::size_t Prefix::take(std::string_view bytes) {
	std::size_t used = 0;
	if (!known_) {
		const auto n = std::min(header_ - head_.size(), bytes.size());
		head_.append(bytes.substr(0, n));
		used += n;
		bytes.remove_prefix(n);
		if (head_.size() < header_)
			return used;
		length_ = read32(std::string_view(head_).substr(header_ - 4));
		known_ = true;
		complete_ = length_ == 0;
	}
	if (complete_ || too_large())
		return used;
	const auto n = std::min<std::size_t>(length_ - envelope_.size(), bytes.size());
	envelope_.append(bytes.substr(0, n));
	used += n;
	complete_ = envelope_.size() == length_;
	return used;
}

// Call

void Call::push(std::string bytes, bool end) {
	if (ended_)
		return;
	if (!sender_) {
		early_ += bytes;
		early_end_ = early_end_ || end;
		return;
	}
	sender_->push(std::move(bytes), end);
}

void Call::cancel() {
	if (!ended_)
		transport_->stop(call_, lane::Code::cancelled);
}

void Call::received(std::string_view bytes, bool end) {
	const auto n = bytes.size();
	auto self = shared_from_this();  // a handler may drop the last reference
	if (!prefix_.complete()) {
		const auto used = prefix_.take(bytes);
		if (prefix_.too_large()) {
			transport_->stop(call_, lane::Code::message_size, "answer envelope past envelope_bytes");
			return;
		}
		bytes.remove_prefix(used);
		if (auto r = handlers_.reply; prefix_.complete() && r)
			r(prefix_.envelope());
	}
	if (auto p = handlers_.payload; prefix_.complete() && (!bytes.empty() || end) && p && !ended_)
		p(bytes, end);
	if (end && !prefix_.complete())
		local_ = fault(Fault::protocol, "answer ended inside its envelope");
	transport_->consume(call_, n);
}

void Call::finished(const lane::Status& status) {
	if (ended_)
		return;
	ended_ = true;
	auto f = failure_of(status);
	if (!f && local_)
		f = local_;
	if (!f && !prefix_.complete())
		f = fault(Fault::protocol, "answer without an envelope");
	auto done = std::move(handlers_.done);
	handlers_ = {};
	if (done)
		done(f);
}

// Client

std::shared_ptr<Call> Client::call(NodeId node, Method method, std::string envelope, std::uint64_t payload_total,
								   Clock::time_point deadline, Call::Handlers handlers, std::string inline_payload,
								   std::uint16_t version) {
	if (envelope.size() > envelope_bytes)
		throw std::invalid_argument("shard rpc: envelope past envelope_bytes");
	if (payload_total != unknown && inline_payload.size() > payload_total)
		throw std::invalid_argument("shard rpc: inline payload past its total");
	std::string stream = encode_request_header(version, std::uint32_t(envelope.size()));
	stream += envelope;
	stream += inline_payload;
	const std::uint64_t total = payload_total == unknown ? unknown : request_header + envelope.size() + payload_total;
	std::string rest;
	if (stream.size() > lane::chunk_bytes) {
		rest = stream.substr(lane::chunk_bytes);
		stream.resize(lane::chunk_bytes);
	}
	std::shared_ptr<Call> c(new Call(transport_, method, std::move(handlers)));
	lane::Events events;
	events.body = [c](std::string_view bytes, bool end) { c->received(bytes, end); };
	events.credit = [c] {
		if (c->sender_)
			c->sender_->pump();
		if (auto w = c->handlers_.writable; w && !c->ended_)
			w();  // a copy: the call may end inside it, which clears its handlers
	};
	events.done = [c](const lane::Status& status) { c->finished(status); };
	lane::CallOptions options;
	options.deadline = deadline;
	options.total = total;
	const auto sent = stream.size();
	c->call_ = transport_.call(node, method_id(method), options, std::move(stream), std::move(events));
	if (c->ended_)
		return c;
	c->sender_.emplace(transport_, c->call_, total, sent);
	if (!rest.empty())
		c->sender_->push(std::move(rest));
	if (!c->early_.empty() || c->early_end_)
		c->sender_->push(std::exchange(c->early_, {}), c->early_end_);
	return c;
}

void Client::unary(NodeId node, Method method, std::string envelope, std::string payload, Clock::time_point deadline,
				   std::function<void(std::optional<Failure>, Reply)> done, std::uint16_t version) {
	post(node, [this, node, method, envelope = std::move(envelope), payload = std::move(payload), deadline,
				done = std::move(done), version]() mutable {
		auto reply = std::make_shared<Reply>();
		Call::Handlers h;
		h.reply = [reply](std::string& e) { reply->envelope = std::move(e); };
		h.payload = [reply](std::string_view bytes, bool) { reply->payload.append(bytes); };
		h.done = [reply, done = std::move(done)](const std::optional<Failure>& f) { done(f, std::move(*reply)); };
		const auto total = payload.size();
		call(node, method, std::move(envelope), total, deadline, std::move(h), std::move(payload), version);
	});
}

void Client::post(NodeId node, std::function<void()> f) {
	asio::post(loops_.at(transport_.loop_of_node(node)), std::move(f));
}

// ServerCall

void ServerCall::payload(std::function<void(std::string_view, bool)> sink) {
	sink_ = std::move(sink);
	if (ended_ || !sink_ || (held_.empty() && !held_end_))
		return;
	const auto held = std::exchange(held_, {});
	auto copy = sink_;  // the call may end inside it, which clears sink_
	copy(held, held_end_);
	transport_->consume(call_, held.size());
}

void ServerCall::reply(std::string envelope, std::uint64_t payload_total, std::string payload) {
	if (ended_)
		return;
	if (sender_)
		throw std::logic_error("shard rpc: a second reply");
	if (envelope.size() > envelope_bytes)
		throw std::invalid_argument("shard rpc: envelope past envelope_bytes");
	const std::uint64_t total = payload_total == unknown ? unknown : response_header + envelope.size() + payload_total;
	transport_->total(call_, total);
	sender_.emplace(*transport_, call_, total, 0);
	std::string stream = encode_response_header(std::uint32_t(envelope.size()));
	stream += envelope;
	stream += payload;
	sender_->push(std::move(stream));
}

void ServerCall::push(std::string bytes, bool end) {
	if (ended_)
		return;
	if (!sender_)
		throw std::logic_error("shard rpc: payload before the reply");
	sender_->push(std::move(bytes), end);
}

void ServerCall::fail(Status status, std::string_view text) {
	if (!ended_)
		transport_->stop(call_, lane::Code(status_base + std::uint32_t(status)), text);
}

void ServerCall::post(std::function<void()> f) {
	asio::post(loop_, [self = shared_from_this(), f = std::move(f)] {
		if (!self->ended_)
			f();
	});
}

void ServerCall::received(std::string_view bytes, bool end) {
	if (ended_)
		return;
	if (!prefix_.complete()) {
		const auto used = prefix_.take(bytes);
		if (!version_ && prefix_.header().size() == request_header) {
			const auto word = read32(prefix_.header());
			if (!word || word > 0xffff)
				return fail(Status::bad_request, "shard rpc: version word");
			version_ = std::uint16_t(word);
			if (!range_.contains(version_))
				return fail(Status::version_refused, std::to_string(range_.min) + " " + std::to_string(range_.max));
		}
		if (prefix_.too_large())
			return fail(Status::too_large, "shard rpc: request envelope past envelope_bytes");
		bytes.remove_prefix(used);
		transport_->consume(call_, used);
		if (!prefix_.complete()) {
			if (end)
				fail(Status::bad_request, "shard rpc: request ended inside its envelope");
			return;
		}
		payload_total_ = total_ == lane::unknown_total ? lane::unknown_total
													   : total_ - request_header - prefix_.envelope().size();
		auto self = shared_from_this();
		(*handler_)(self);
		if (ended_)
			return;
	}
	if (!bytes.empty() || end)
		deliver(bytes, end);
}

void ServerCall::deliver(std::string_view bytes, bool end) {
	if (!sink_) {
		held_.append(bytes);  // unconsumed: it holds the caller's credit until a sink takes it
		held_end_ = held_end_ || end;
		return;
	}
	auto copy = sink_;  // the call may end inside it, which clears sink_
	copy(bytes, end);
	transport_->consume(call_, bytes.size());
}

void ServerCall::finished(const lane::Status& status) {
	if (ended_)
		return;
	ended_ = true;
	auto d = std::move(done);
	done = {};
	writable = {};
	sink_ = {};
	if (d)
		d(failure_of(status));
}

// Service

void Service::serve(Method method, Handler handler) {
	const auto index = std::uint32_t(method) - 1;
	if (index >= method_count)
		throw std::invalid_argument("shard rpc: method");
	handlers_[index] = std::make_unique<Handler>(std::move(handler));
	const Handler& h = *handlers_[index];
	transport_.handle(method_id(method), [this, method, &h](RpcCall c, const lane::Incoming& in) -> std::optional<lane::Events> {
		std::shared_ptr<ServerCall> s(
			new ServerCall(transport_, loops_.at(transport_.loop_of_node(c.node)), c, method, in.total, range_, h));
		lane::Events events;
		events.body = [s](std::string_view bytes, bool end) { s->received(bytes, end); };
		events.credit = [s] {
			if (s->sender_)
				s->sender_->pump();
			if (auto w = s->writable; w && !s->ended_)
				w();  // a copy: the call may end inside it, which clears writable
		};
		events.done = [s](const lane::Status& status) { s->finished(status); };
		return events;
	});
}

} // namespace cluster::rpc
