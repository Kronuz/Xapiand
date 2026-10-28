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

// Unit tests for the engine census: the counters at the seam, the key prefix check (T5), and the
// per-request JSON lines. Built without ENGINE_CENSUS the hooks must vanish, which the test checks
// by compiling them.

#include "test_harness.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/census.h"
#include "engine/document.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/sync.h"
#include "engine/xapian/shard.h"

#ifdef ENGINE_CENSUS

namespace {

using engine::Op;
using engine::Query;
namespace census = engine::census;

std::string path_of(const char* name) {
	return (std::filesystem::current_path() / name).string();
}

std::uint64_t get(const std::map<std::string, std::uint64_t>& counts, const std::string& key) {
	const auto it = counts.find(key);
	return it == counts.end() ? 0 : it->second;
}

std::string read_file(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

class Source final : public engine::PostingSource {
public:
	engine::valueno slot() const override { return 3; }
	bool accept(std::string_view) const override { return true; }
	double weight(std::string_view) const override { return 1; }
	double max_weight() const override { return 2; }
	std::unique_ptr<engine::PostingSource> clone() const override { return std::make_unique<Source>(); }
	std::string name() const override { return "TestSource"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::PostingSource> unserialise(std::string_view) const override { return std::make_unique<Source>(); }
	std::string description() const override { return "TestSource"; }
};

class SlotKey final : public engine::KeyMaker {
public:
	std::string key(const engine::DocView& doc) const override { return doc.value(1); }
	std::unique_ptr<engine::KeyMaker> clone() const override { return std::make_unique<SlotKey>(); }
	std::string name() const override { return "SlotKey"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::KeyMaker> unserialise(std::string_view) const override { return std::make_unique<SlotKey>(); }
};

class Spy final : public engine::MatchSpy {
public:
	void see(const engine::DocView& doc, double) override { (void)doc.did(); (void)doc.data(); }
	std::unique_ptr<engine::MatchSpy> clone() const override { return std::make_unique<Spy>(); }
	std::string name() const override { return "Spy"; }
	std::string serialise() const override { return ""; }
	std::unique_ptr<engine::MatchSpy> unserialise(std::string_view) const override { return std::make_unique<Spy>(); }
	std::string serialise_results() const override { return ""; }
	void merge_results(std::string_view) override { }
	std::string description() const override { return "Spy"; }
};

void test_query_nodes() {
	census::reset("");
	engine::TextConfig config;
	config.prefix = "title.";
	config.flags = engine::text::PHRASE | engine::text::PARTIAL;
	config.stem = engine::Stem::all;
	config.stop_language = "en";
	const auto query = Query::combine(Op::AND, {
		Query::term("a"),
		Query::term("b", 2, 4),
		Query::match_all(),
		Query::combine(Op::OR, Query::term("c"), Query::scale(Query::term("d"), 2.0)),
		Query::wildcard("ab*", 10, engine::Expansion::first, engine::PATTERN_MULTI),
		Query::edit_distance("abc", 5, engine::Expansion::error, Op::OR, 2, 1),
		Query::source(std::make_unique<Source>()),
		Query::text("hello world", config),
	});
	census::query(query);
	const auto counts = census::snapshot();
	CHECK_EQ(get(counts, "query.prepared"), 1u);
	CHECK_EQ(get(counts, "query.nodes"), 12u);
	CHECK_EQ(get(counts, "query.kind.term"), 4u);
	CHECK_EQ(get(counts, "query.kind.match_all"), 1u);
	CHECK_EQ(get(counts, "query.kind.combine"), 2u);
	CHECK_EQ(get(counts, "query.kind.scale"), 1u);
	CHECK_EQ(get(counts, "query.op.AND"), 1u);
	CHECK_EQ(get(counts, "query.op.OR"), 1u);
	CHECK_EQ(get(counts, "query.combine.binary"), 1u);
	CHECK_EQ(get(counts, "query.combine.nary"), 1u);
	CHECK_EQ(get(counts, "query.term.pos"), 1u);
	CHECK_EQ(get(counts, "query.term.wqf"), 1u);
	CHECK_EQ(get(counts, "query.wildcard.pattern.multi"), 1u);
	CHECK_EQ(get(counts, "query.wildcard.limit.first"), 1u);
	CHECK_EQ(get(counts, "query.edit_distance.distance.2"), 1u);
	CHECK_EQ(get(counts, "source.TestSource"), 1u);
	CHECK_EQ(get(counts, "source.max_weight"), 1u);
	CHECK_EQ(get(counts, "query.text.flag.PHRASE"), 1u);
	CHECK_EQ(get(counts, "query.text.flag.PARTIAL"), 1u);
	CHECK_EQ(get(counts, "query.text.flag.FUZZY"), 0u);
	CHECK_EQ(get(counts, "query.text.stem.all"), 1u);
	CHECK_EQ(get(counts, "query.text.stop"), 1u);
}

void test_key_prefix_check() {
	census::reset("");
	auto doc = [](std::vector<std::string> terms) {
		engine::Document d;
		for (auto& t : terms) { d.add_boolean_term(std::move(t)); }
		return d;
	};
	census::document(doc({"_id.1", "title.word"}), "");
	census::document(doc({"_id.2", "title.word"}), "_id.2");
	census::document(doc({"title.word"}), "");
	auto counts = census::snapshot();
	CHECK_EQ(get(counts, "t5[_id.].documents"), 3u);
	CHECK_EQ(get(counts, "t5[_id.].one_key_term.unnamed"), 1u);
	CHECK_EQ(get(counts, "t5[_id.].one_key_term.named"), 1u);
	CHECK_EQ(get(counts, "t5[_id.].no_key_term"), 1u);
	CHECK_EQ(get(counts, "t5[_id.].violation.multiple"), 0u);
	CHECK_EQ(get(counts, "t5[_id.].violation.not_key_term"), 0u);
	CHECK_EQ(get(counts, "write.terms"), 5u);
	CHECK_EQ(get(counts, "write.terms.boolean"), 5u);

	census::document(doc({"_id.3", "_id.3.exists"}), "");
	census::document(doc({"_id.4", "_id.4x"}), "_id.4");
	census::document(doc({"_id.5"}), "other.5");
	counts = census::snapshot();
	CHECK_EQ(get(counts, "t5[_id.].violation.multiple"), 2u);
	CHECK_EQ(get(counts, "t5[_id.].violation.not_key_term"), 2u);
	CHECK_EQ(get(counts, "t5[_id.].violation.key_without_prefix"), 1u);
	CHECK_EQ(get(counts, "t5[Q].documents"), 6u);
	CHECK_EQ(get(counts, "t5[Q].no_key_term"), 6u);

	// the id term prefix Q: a second Q-leading term (a field named Quote) breaks the premise
	census::reset("");
	census::document(doc({"QKabc", "title.q"}), "QKabc");
	census::document(doc({"QKabc", "Quote.x"}), "QKabc");
	census::document(doc({"Quote.x"}), "QKabc");
	counts = census::snapshot();
	CHECK_EQ(get(counts, "t5[Q].documents"), 3u);
	CHECK_EQ(get(counts, "t5[Q].one_key_term.named"), 1u);
	CHECK_EQ(get(counts, "t5[Q].violation.multiple"), 1u);
	CHECK_EQ(get(counts, "t5[Q].violation.not_key_term"), 2u);
	CHECK_EQ(get(counts, "t5[_id.].no_key_term"), 3u);
}

void test_document_shape() {
	census::reset("");
	engine::Document d;
	d.add_boolean_term("b");
	d.add_term("w", 2);
	d.add_posting("p", 3, 1);
	d.add_posting("p", 7, 1);
	d.add_value(1, "x");
	d.add_value(9, "y");
	d.set_data("data");
	census::document(d, "");
	auto counts = census::snapshot();
	CHECK_EQ(get(counts, "write.terms"), 3u);
	CHECK_EQ(get(counts, "write.terms.boolean"), 1u);
	CHECK_EQ(get(counts, "write.terms.positioned"), 1u);
	CHECK_EQ(get(counts, "write.values"), 2u);
	CHECK_EQ(get(counts, "write.data"), 1u);
}

void test_shard_search_and_views() {
	census::reset("");
	const auto path = path_of("engine_census.db");
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::xapian::open_local(path, true, true));
	auto& shard = *opened.shard;
	for (int i = 1; i <= 3; ++i) {
		engine::Document doc;
		doc.add_boolean_term("_id." + std::to_string(i));
		doc.add_term("word");
		doc.add_value(1, "k" + std::to_string(4 - i));
		doc.set_data("d");
		engine::WriteArgs args;
		engine::sync_wait(shard.replace_by_key("_id." + std::to_string(i), doc, args));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));

	const auto query = Query::term("word");
	auto prepared = engine::prepare(query);
	SlotKey sorter;
	Spy spy;
	engine::SearchRequest request;
	request.query = prepared.get();
	request.sorter = &sorter;
	request.spy = &spy;
	request.maxitems = 10;
	request.check_at_least = 10;
	std::vector<engine::Shard*> shards{&shard};
	const auto result = engine::sync_wait(engine::search(shards, request));
	CHECK_EQ(result.hits.size(), 3u);
	const auto view = engine::sync_wait(shard.get(1));
	(void)view->data();

	const auto counts = census::snapshot();
	CHECK_EQ(get(counts, "shard.replace_by_key"), 3u);
	CHECK_EQ(get(counts, "shard.commit"), 1u);
	CHECK_EQ(get(counts, "shard.get"), 1u);
	CHECK_EQ(get(counts, "write.documents"), 3u);
	CHECK_EQ(get(counts, "t5[_id.].one_key_term.named"), 3u);
	CHECK_EQ(get(counts, "t5[_id.].documents"), 3u);
	CHECK_EQ(get(counts, "t5[Q].no_key_term"), 3u);
	CHECK_EQ(get(counts, "search.calls"), 1u);
	CHECK_EQ(get(counts, "search.sorter.SlotKey"), 1u);
	CHECK_EQ(get(counts, "search.spy.Spy"), 1u);
	CHECK_EQ(get(counts, "search.shards.1"), 1u);
	CHECK_EQ(get(counts, "callback.spy:Spy"), 3u);
	CHECK_EQ(get(counts, "docview.did@spy:Spy"), 3u);
	CHECK_EQ(get(counts, "docview.data@spy:Spy"), 3u);
	CHECK_EQ(get(counts, "docview.data@host"), 1u);
	CHECK_GE(get(counts, "callback.keymaker:SlotKey"), 3u);
	CHECK_GE(get(counts, "docview.value@keymaker:SlotKey"), 3u);
	CHECK_EQ(get(counts, "docview.value"), get(counts, "docview.value@keymaker:SlotKey"));
	std::filesystem::remove_all(path);
}

void test_request_lines() {
	const auto file = path_of("engine_census.jsonl");
	std::filesystem::remove(file);
	census::reset(file);
	census::count("outside.before");
	{
		census::RequestScope request("GET", "/a\"b\x01\xff");
		census::count("x", 2);
		census::count("x");
		census::count("y.z");
		request.set_status(200);
	}
	try {
		census::RequestScope request("PUT", "/t");
		census::count("w");
		throw std::runtime_error("view");
	} catch (const std::exception&) { }
	{
		census::RequestScope one("GET", "/one");
		census::RequestScope two("GET", "/two");
		census::count("shared");
	}
	const auto text = read_file(file);
	std::vector<std::string> lines;
	for (size_t at = 0; at < text.size();) {
		const auto end = text.find('\n', at);
		lines.push_back(text.substr(at, end - at));
		at = end + 1;
	}
	REQUIRE_EQ(lines.size(), 6u);
	CHECK_EQ(lines[0], R"({"seq":0,"kind":"unattributed","counts":{"outside.before":1},"samples":[]})");
	CHECK_EQ(lines[1], std::string("{\"seq\":1,\"method\":\"GET\",\"target\":\"/a\\\"b\\u0001\\u00ff\",\"status\":200,\"threw\":false,\"tainted\":false,\"counts\":{\"x\":3,\"y.z\":1},\"samples\":[]}"));
	CHECK_EQ(lines[2], R"({"seq":2,"method":"PUT","target":"/t","status":0,"threw":true,"tainted":false,"counts":{"w":1},"samples":[]})");
	// two requests in flight: the count is not theirs, it is reported apart
	CHECK_EQ(lines[3], R"({"seq":0,"kind":"unattributed","counts":{"shared":1},"samples":[]})");
	CHECK_TRUE(lines[4].find("\"target\":\"/two\"") != std::string::npos);
	CHECK_TRUE(lines[4].find("\"tainted\":true") != std::string::npos);
	CHECK_TRUE(lines[5].find("\"target\":\"/one\"") != std::string::npos);
	census::reset("");
	std::filesystem::remove(file);
}

}  // namespace

TEST_MAIN(test_query_nodes, test_key_prefix_check, test_document_shape, test_shard_search_and_views, test_request_lines)

#else

// Off: every hook is an empty statement, so the arguments are not even evaluated.
static void off() {
	ENGINE_CENSUS_COUNT(undeclared_name);
	ENGINE_CENSUS_QUERY(undeclared_name);
	ENGINE_CENSUS_SEARCH(undeclared_name, 0);
	ENGINE_CENSUS_DOCUMENT(undeclared_name, "");
	ENGINE_CENSUS_DOCVIEW("did");
	ENGINE_CENSUS_CALLBACK(scope, "spy", undeclared_name);
	ENGINE_CENSUS_REQUEST(request, undeclared_name, undeclared_name);
	ENGINE_CENSUS_STATUS(request, 200);
}

int main() {
	off();
	std::printf("OK: census is off\n");
	return 0;
}

#endif
