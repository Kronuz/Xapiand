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

// The engine census: with ENGINE_CENSUS defined, the seam counts the facilities a request uses
// (query nodes, posting sources, spies, key makers, DocView methods, write operations) and writes
// one JSON line per HTTP request. Without it every hook below expands to nothing.

#ifdef ENGINE_CENSUS

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <map>                     // for std::map
#include <string>                  // for std::string
#include <string_view>             // for std::string_view
#include <vector>                  // for std::vector


namespace engine {

class Document;
class Query;
struct SearchRequest;

namespace census {

// Adds n to the counter. It belongs to the one request in flight, or to the background when none is.
void count(std::string_view key, std::uint64_t n = 1);

// Counts the nodes of a query and what each one asks of the engine.
void query(const Query& query);

// Counts the fields a search request uses.
void search(const SearchRequest& request, std::size_t shards);

// Counts a document about to be written and, for each of KEY_PREFIXES, checks that at most one term
// leads with it, and that it is the key term (when the write names one): counters t5[prefix].*.
void document(const Document& doc, std::string_view key);

// The prefixes the id term might lead with: Xapiand's id term prefix, and the id field's name prefix.
inline constexpr std::string_view KEY_PREFIXES[] = {"Q", "_id."};

// One HTTP request: counters between construction and destruction are written as one JSON line.
class RequestScope {
public:
	RequestScope(std::string_view method, std::string_view target);
	~RequestScope();
	RequestScope(const RequestScope&) = delete;
	RequestScope& operator=(const RequestScope&) = delete;

	void set_status(int status) noexcept {
		status_ = status;
	}

private:
	std::uint64_t id_;
	int status_ = 0;
	int uncaught_;
};

// While alive, DocView counts are also kept under this host callback (a spy or a key maker).
class CallbackScope {
public:
	CallbackScope(std::string_view kind, std::string_view name);
	~CallbackScope();
	CallbackScope(const CallbackScope&) = delete;
	CallbackScope& operator=(const CallbackScope&) = delete;

private:
	std::string previous_;
};

// Counts a DocView method, under the host callback running it, if any.
void docview(std::string_view method);

// The counters of the request in flight (tests).
std::map<std::string, std::uint64_t> snapshot();

// Forgets every counter and request, and sets the output file ("" for none); tests.
void reset(const std::string& file);

}  // namespace census
}  // namespace engine

#define ENGINE_CENSUS_REQUEST(var, method, target) ::engine::census::RequestScope var(method, target)
#define ENGINE_CENSUS_STATUS(var, status) (var).set_status(static_cast<int>(status))
#define ENGINE_CENSUS_COUNT(key) ::engine::census::count(key)
#define ENGINE_CENSUS_QUERY(q) ::engine::census::query(q)
#define ENGINE_CENSUS_SEARCH(request, shards) ::engine::census::search(request, shards)
#define ENGINE_CENSUS_DOCUMENT(doc, key) ::engine::census::document(doc, key)
#define ENGINE_CENSUS_DOCVIEW(method) ::engine::census::docview(method)
#define ENGINE_CENSUS_CALLBACK(var, kind, name) ::engine::census::CallbackScope var(kind, name)

#else

#define ENGINE_CENSUS_REQUEST(var, method, target) ((void)0)
#define ENGINE_CENSUS_STATUS(var, status) ((void)0)
#define ENGINE_CENSUS_COUNT(key) ((void)0)
#define ENGINE_CENSUS_QUERY(q) ((void)0)
#define ENGINE_CENSUS_SEARCH(request, shards) ((void)0)
#define ENGINE_CENSUS_DOCUMENT(doc, key) ((void)0)
#define ENGINE_CENSUS_DOCVIEW(method) ((void)0)
#define ENGINE_CENSUS_CALLBACK(var, kind, name) ((void)0)

#endif
