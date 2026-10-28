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

#pragma once

#include <string>                  // for std::string
#include <utility>                 // for std::move

namespace engine {

// What went wrong, in terms callers act on (retry, reopen, give up, report to the client).
enum class errc {
	reopen,            // the snapshot is stale: reopen and retry
	opening,           // the shard could not be opened (yet)
	closed,            // the shard was closed under the operation
	not_found,         // the shard does not exist
	not_available,     // the shard exists but cannot serve now (no quorum, remote down)
	network,           // a transport failure
	timeout,           // a transport timeout
	doc_not_found,     // no such document
	version_conflict,  // an expected document version did not match
	corrupt,           // the stored data failed its integrity checks
	database,          // any other failure of the shard itself: close it
	invalid,           // the caller passed something the engine rejects
	syntax,            // query or serialised input the engine cannot parse: a client error
	internal,          // anything else
};

constexpr const char* to_string(errc kind) noexcept {
	switch (kind) {
		case errc::reopen: return "reopen";
		case errc::opening: return "opening";
		case errc::closed: return "closed";
		case errc::not_found: return "not_found";
		case errc::not_available: return "not_available";
		case errc::network: return "network";
		case errc::timeout: return "timeout";
		case errc::doc_not_found: return "doc_not_found";
		case errc::version_conflict: return "version_conflict";
		case errc::corrupt: return "corrupt";
		case errc::database: return "database";
		case errc::invalid: return "invalid";
		case errc::syntax: return "syntax";
		case errc::internal: return "internal";
	}
	return "internal";
}

// A missing document or shard is often data, not a failure.
constexpr bool is_missing(errc kind) noexcept {
	return kind == errc::doc_not_found || kind == errc::not_found;
}

// What a read retry ladder does with a failure on try `t` (counting down to 0).
enum class fate {
	retry_close,  // reopen and retry; on the last try close the shard and fail
	close_retry,  // close the shard, then reopen and retry; fail on the last try
	close_fail,   // close the shard and fail
	fail,         // not a failure of the shard: fail as is
};

constexpr fate read_fate(errc kind) noexcept {
	switch (kind) {
		case errc::reopen:
		case errc::opening:
		case errc::not_found:
		case errc::not_available:
		case errc::timeout:
		case errc::network:
			return fate::retry_close;
		case errc::closed:
			return fate::close_retry;
		case errc::corrupt:
		case errc::database:
			return fate::close_fail;
		default:
			return fate::fail;
	}
}

// Every failure an engine raises, whatever its implementation threw. It is deliberately not a
// std::exception: it is a mixin that Xapiand's own exception types also derive from (alongside
// ClientError, which is one), and a second std::exception base would make a catch of
// std::exception ambiguous and silently miss them.
class Error {
public:
	Error(errc kind_, std::string reason) : kind(kind_), reason_(std::move(reason)) { }

	// `description` is the engine's full text for logs and 500 bodies; `detail` the system or
	// transport string behind it (empty when none).
	Error(errc kind_, std::string reason, std::string description, std::string detail = {})
		: kind(kind_), reason_(std::move(reason)), description_(std::move(description)), detail_(std::move(detail)) { }

	virtual ~Error() = default;

	const std::string& reason() const noexcept {
		return reason_;
	}

	const std::string& description() const noexcept {
		return description_.empty() ? reason_ : description_;
	}

	const std::string& detail() const noexcept {
		return detail_;
	}

	errc kind;

protected:
	void set_texts(std::string description, std::string detail) {
		description_ = std::move(description);
		detail_ = std::move(detail);
	}

private:
	std::string reason_;
	std::string description_;
	std::string detail_;
};

}  // namespace engine
