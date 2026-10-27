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

#ifdef ENGINE_CENSUS

#include "engine/census.h"

#include <algorithm>                // for std::lower_bound
#include <cstdio>                  // for std::FILE, std::fopen, std::fwrite
#include <cstdlib>                 // for std::getenv
#include <exception>               // for std::uncaught_exceptions
#include <mutex>                   // for std::mutex, std::lock_guard
#include <utility>                 // for std::move

#include "engine/callbacks.h"      // for PostingSource
#include "engine/document.h"       // for Document
#include "engine/query.h"          // for Query, QueryNode
#include "engine/search.h"         // for SearchRequest


namespace engine::census {

namespace {

constexpr std::size_t MAX_SAMPLES = 8;

struct Counters {
	std::map<std::string, std::uint64_t> counts;
	std::vector<std::string> samples;
};

struct Request : Counters {
	std::uint64_t id = 0;
	std::string method;
	std::string target;
	bool tainted = false;
};

struct State {
	std::mutex mutex;
	std::vector<Request*> active;
	Counters outside;                  // counted while no request, or several, were in flight
	std::uint64_t seq = 0;
	std::string file;
	bool file_read = false;
	std::FILE* out = nullptr;
};

State&
state()
{
	static State* instance = new State;  // never destroyed: requests can finish during exit
	return *instance;
}

thread_local std::string current_callback;

// Valid JSON for any bytes: everything but printable ASCII is escaped.
void
json_string(std::string& out, std::string_view text)
{
	out += '"';
	for (const unsigned char c : text) {
		if (c == '"' || c == '\\') {
			out += '\\';
			out += static_cast<char>(c);
		} else if (c < 0x20 || c >= 0x7f) {
			char buffer[8];
			std::snprintf(buffer, sizeof buffer, "\\u%04x", c);
			out += buffer;
		} else {
			out += static_cast<char>(c);
		}
	}
	out += '"';
}

void
json_counters(std::string& out, const Counters& counters)
{
	out += "\"counts\":{";
	bool first = true;
	for (const auto& [key, value] : counters.counts) {
		if (!first) {
			out += ',';
		}
		first = false;
		json_string(out, key);
		out += ':';
		out += std::to_string(value);
	}
	out += "},\"samples\":[";
	first = true;
	for (const auto& sample : counters.samples) {
		if (!first) {
			out += ',';
		}
		first = false;
		json_string(out, sample);
	}
	out += ']';
}

// Appends a line to the output file, if there is one. The state's mutex is held.
void
write_line(State& s, const std::string& line)
{
	if (!s.file_read) {
		s.file_read = true;
		if (s.file.empty()) {
			if (const char* env = std::getenv("XAPIAND_ENGINE_CENSUS")) {
				s.file = env;
			}
		}
	}
	if (s.file.empty()) {
		return;
	}
	if (s.out == nullptr) {
		s.out = std::fopen(s.file.c_str(), "ab");
		if (s.out == nullptr) {
			s.file.clear();
			return;
		}
	}
	std::fwrite(line.data(), 1, line.size(), s.out);
	std::fputc('\n', s.out);
	std::fflush(s.out);
}

Counters&
target_of(State& s)
{
	return s.active.size() == 1 ? static_cast<Counters&>(*s.active.front()) : s.outside;
}

void
add(State& s, std::string_view key, std::uint64_t n)
{
	target_of(s).counts[std::string(key)] += n;
}

void
add_sample(State& s, std::string text)
{
	auto& samples = target_of(s).samples;
	if (samples.size() < MAX_SAMPLES) {
		samples.push_back(std::move(text));
	}
}

constexpr struct { unsigned bit; const char* name; } TEXT_FLAGS[] = {
	{text::BOOLEAN, "BOOLEAN"}, {text::PHRASE, "PHRASE"}, {text::LOVEHATE, "LOVEHATE"},
	{text::WILDCARD, "WILDCARD"}, {text::WILDCARD_SINGLE, "WILDCARD_SINGLE"},
	{text::WILDCARD_MULTI, "WILDCARD_MULTI"}, {text::PARTIAL, "PARTIAL"}, {text::FUZZY, "FUZZY"},
	{text::CJK_NGRAM, "CJK_NGRAM"}, {text::WORD_BREAKS, "WORD_BREAKS"},
};

const char*
name_of(Expansion limit) noexcept
{
	switch (limit) {
		case Expansion::error: return "error";
		case Expansion::first: return "first";
		case Expansion::most_frequent: return "most_frequent";
	}
	return "?";
}

const char*
name_of(Stem stem) noexcept
{
	switch (stem) {
		case Stem::none: return "none";
		case Stem::some: return "some";
		case Stem::all: return "all";
		case Stem::all_z: return "all_z";
	}
	return "?";
}

const char*
name_of(DocidOrder order) noexcept
{
	switch (order) {
		case DocidOrder::ascending: return "ascending";
		case DocidOrder::descending: return "descending";
		case DocidOrder::dont_care: return "dont_care";
	}
	return "?";
}

const char*
name_of(QueryKind kind) noexcept
{
	switch (kind) {
		case QueryKind::term: return "term";
		case QueryKind::combine: return "combine";
		case QueryKind::append: return "append";
		case QueryKind::scale: return "scale";
		case QueryKind::wildcard: return "wildcard";
		case QueryKind::edit_distance: return "edit_distance";
		case QueryKind::source: return "source";
		case QueryKind::text: return "text";
	}
	return "?";
}

std::string
join(std::string_view a, std::string_view b)
{
	std::string out(a);
	out += b;
	return out;
}

}  // namespace


void
count(std::string_view key, std::uint64_t n)
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	add(s, key, n);
}


void
query(const Query& query)
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	add(s, "query.prepared", 1);
	std::vector<const QueryNode*> pending;
	if (query.node() != nullptr) {
		pending.push_back(query.node());
	}
	while (!pending.empty()) {
		const auto& node = *pending.back();
		pending.pop_back();
		add(s, "query.nodes", 1);
		const auto kind = join("query.kind.", name_of(node.kind));
		add(s, node.kind == QueryKind::term && node.str.empty() ? "query.kind.match_all" : kind, 1);
		switch (node.kind) {
			case QueryKind::term:
				if (node.wqf != 1) { add(s, "query.term.wqf", 1); }
				if (node.pos != 0) { add(s, "query.term.pos", 1); }
				break;
			case QueryKind::combine:
				add(s, join("query.op.", to_string(node.op)), 1);
				add(s, node.binary ? "query.combine.binary" : "query.combine.nary", 1);
				if (node.parameter != 0) { add(s, "query.combine.parameter", 1); }
				break;
			case QueryKind::append:
				add(s, join("query.op.", to_string(node.op)), 1);
				break;
			case QueryKind::wildcard:
				add(s, join("query.wildcard.combiner.", to_string(node.op)), 1);
				add(s, join("query.wildcard.limit.", name_of(node.limit)), 1);
				if (node.parameter != 0) { add(s, "query.wildcard.max_expansion", 1); }
				if ((node.pattern_flags & PATTERN_SINGLE) != 0) { add(s, "query.wildcard.pattern.single", 1); }
				if ((node.pattern_flags & PATTERN_MULTI) != 0) { add(s, "query.wildcard.pattern.multi", 1); }
				break;
			case QueryKind::edit_distance:
				add(s, join("query.edit_distance.combiner.", to_string(node.op)), 1);
				add(s, join("query.edit_distance.limit.", name_of(node.limit)), 1);
				add(s, join("query.edit_distance.distance.", std::to_string(node.distance)), 1);
				if (node.prefix_length != 0) { add(s, "query.edit_distance.prefix_length", 1); }
				break;
			case QueryKind::source:
				if (node.src) {
					add(s, join("source.", node.src->name()), 1);
					if (node.src->max_weight() > 0) { add(s, "source.max_weight", 1); }
				}
				break;
			case QueryKind::text: {
				const auto& config = node.config;
				for (const auto& flag : TEXT_FLAGS) {
					if ((config.flags & flag.bit) != 0) { add(s, join("query.text.flag.", flag.name), 1); }
				}
				if (config.default_op != Op::INVALID) { add(s, join("query.text.default_op.", to_string(config.default_op)), 1); }
				add(s, join("query.text.stem.", name_of(config.stem)), 1);
				if (!config.stop_language.empty()) { add(s, "query.text.stop", 1); }
				if (!config.stem_language.empty()) { add(s, "query.text.stem_language", 1); }
				if (config.partial_max_expansion != 0) { add(s, "query.text.partial_max_expansion", 1); }
				if (config.fuzzy_max_expansion != 0) { add(s, "query.text.fuzzy_max_expansion", 1); }
				break;
			}
		}
		for (const auto& sub : node.subqueries) {
			if (sub.node() != nullptr) {
				pending.push_back(sub.node());
			}
		}
	}
}


void
search(const SearchRequest& request, std::size_t shards)
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	add(s, "search.calls", 1);
	add(s, join("search.shards.", std::to_string(shards)), 1);
	add(s, join("search.order.", name_of(request.order)), 1);
	if (request.first != 0) { add(s, "search.first", 1); }
	if (request.maxitems == 0) { add(s, "search.maxitems_zero", 1); }
	if (request.check_at_least != 0) { add(s, "search.check_at_least", 1); }
	if (request.sorter != nullptr) { add(s, join("search.sorter.", request.sorter->name()), 1); }
	if (request.spy != nullptr) { add(s, join("search.spy.", request.spy->name()), 1); }
	if (request.collapse_key != BAD_VALUENO) { add(s, "search.collapse", 1); }
	if (request.percent_threshold != 0) { add(s, "search.percent_threshold", 1); }
	if (request.weight_threshold != 0) { add(s, "search.weight_threshold", 1); }
	if (request.fuzzy) { add(s, "search.fuzzy", 1); }
	if (request.nearest) { add(s, "search.nearest", 1); }
}


void
document(const Document& doc, std::string_view key)
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	add(s, "write.documents", 1);
	std::uint64_t boolean = 0, positioned = 0, terms = 0;
	std::vector<std::string> all;
	doc.native().census_terms([&](std::string_view term, termcount wdf, std::size_t positions) {
		++terms;
		if (positions != 0) {
			++positioned;
		} else if (wdf == 0) {
			++boolean;
		}
		all.emplace_back(term);
	});
	add(s, "write.terms", terms);
	add(s, "write.terms.boolean", boolean);
	add(s, "write.terms.positioned", positioned);
	add(s, "write.values", doc.native().census_values());
	if (!doc.get_data().empty()) { add(s, "write.data", 1); }
	if (!key.empty()) { add(s, "write.named_key", 1); }

	// Terms leading with each candidate key prefix (the terms are sorted, so they are contiguous).
	for (const std::string_view prefix : KEY_PREFIXES) {
		const auto tag = join("t5[", join(prefix, "]."));
		std::uint64_t leading = 0;
		bool other = false;
		std::string listed;
		for (auto it = std::lower_bound(all.begin(), all.end(), prefix); it != all.end() && it->starts_with(prefix); ++it) {
			++leading;
			if (leading <= 4) {
				listed += listed.empty() ? "" : " ";
				listed += it->substr(0, 40);
			}
			if (key.empty()) {
				if (leading == 1) { add_sample(s, join(tag, join("key_term:", *it))); }
			} else if (*it != key) {
				other = true;
				add(s, join(tag, "violation.not_key_term"), 1);
				add_sample(s, join(tag, join("not_key_term:", *it)));
			}
		}
		add(s, join(tag, "documents"), 1);
		if (leading == 0) {
			add(s, join(tag, "no_key_term"), 1);
		} else if (leading == 1) {
			if (!other) {
				add(s, join(tag, key.empty() ? "one_key_term.unnamed" : "one_key_term.named"), 1);
			}
		} else {
			add(s, join(tag, "violation.multiple"), 1);
			add_sample(s, join(tag, join("multiple:", join(std::to_string(leading), join(" ", listed)))));
		}
		if (!key.empty() && !key.starts_with(prefix)) {
			add(s, join(tag, "violation.key_without_prefix"), 1);
			add_sample(s, join(tag, join("key_without_prefix:", key)));
		}
	}
}


void
docview(std::string_view method)
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	const auto base = join("docview.", method);
	add(s, base, 1);
	add(s, current_callback.empty() ? base + "@host" : base + "@" + current_callback, 1);
}


CallbackScope::CallbackScope(std::string_view kind, std::string_view name) : previous_(std::move(current_callback))
{
	current_callback = join(kind, ":");
	current_callback += name;
	count(join("callback.", current_callback));
}


CallbackScope::~CallbackScope()
{
	current_callback = std::move(previous_);
}


RequestScope::RequestScope(std::string_view method, std::string_view target) : uncaught_(std::uncaught_exceptions())
{
	auto& s = state();
	auto* request = new Request;
	request->method = method;
	request->target = target;
	std::lock_guard<std::mutex> lock(s.mutex);
	request->id = id_ = ++s.seq;
	if (!s.active.empty()) {
		request->tainted = true;
		for (auto* other : s.active) {
			other->tainted = true;
		}
	}
	s.active.push_back(request);
}


RequestScope::~RequestScope()
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	Request* request = nullptr;
	for (auto it = s.active.begin(); it != s.active.end(); ++it) {
		if ((*it)->id == id_) {
			request = *it;
			s.active.erase(it);
			break;
		}
	}
	if (request == nullptr) {
		return;
	}
	if (!s.outside.counts.empty() || !s.outside.samples.empty()) {
		std::string line = "{\"seq\":0,\"kind\":\"unattributed\",";
		json_counters(line, s.outside);
		line += '}';
		write_line(s, line);
		s.outside = Counters{};
	}
	std::string line = "{\"seq\":" + std::to_string(request->id) + ",\"method\":";
	json_string(line, request->method);
	line += ",\"target\":";
	json_string(line, request->target);
	line += ",\"status\":" + std::to_string(status_);
	line += std::uncaught_exceptions() > uncaught_ ? ",\"threw\":true" : ",\"threw\":false";
	line += request->tainted ? ",\"tainted\":true," : ",\"tainted\":false,";
	json_counters(line, *request);
	line += '}';
	write_line(s, line);
	delete request;
}


std::map<std::string, std::uint64_t>
snapshot()
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	return target_of(s).counts;
}


void
reset(const std::string& file)
{
	auto& s = state();
	std::lock_guard<std::mutex> lock(s.mutex);
	for (auto* request : s.active) {
		delete request;
	}
	s.active.clear();
	s.outside = Counters{};
	s.seq = 0;
	if (s.out != nullptr) {
		std::fclose(s.out);
		s.out = nullptr;
	}
	s.file = file;
	s.file_read = true;
}

}  // namespace engine::census

#endif  // ENGINE_CENSUS
