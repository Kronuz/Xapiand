/*
 * Copyright (c) 2026 Dubalu LLC
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

#include <stdexcept>               // for std::runtime_error
#include <string>                  // for std::string

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
	invalid,           // the caller passed something the engine rejects
	internal,          // anything else
};

constexpr const char* to_string(errc code) noexcept {
	switch (code) {
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
		case errc::invalid: return "invalid";
		case errc::internal: return "internal";
	}
	return "internal";
}

// Every failure an engine raises, whatever its implementation threw.
class Error : public std::runtime_error {
public:
	Error(errc code_, const std::string& message) : std::runtime_error(message), code(code_) { }

	errc code;
};

}  // namespace engine
