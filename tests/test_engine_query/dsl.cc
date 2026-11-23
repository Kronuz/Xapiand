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

// The query DSL: not-is-none-of and keyword values that look like fuzzy distances.

#include "support.h"

namespace detail {

namespace {

// A schema of keyword fields "g" and "e", and the (field, term) a query on them finds in a document.
std::shared_ptr<Schema> keyword_schema() {
	MsgPack props = MsgPack::MAP();
	props["g"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1101 } });
	props["e"] = MsgPack({ { "_type", "keyword" }, { "_slot", 1102 } });
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

std::pair<std::string, std::string> term_of(QueryDSL& dsl, const char* field, const char* value) {
	const auto query = dsl.get_query(MsgPack({ { field, value } }));
	// The DSL appends a clause onto the empty query; the term is the appended side.
	auto node = query.node();
	while (node != nullptr && node->kind == engine::QueryKind::append) {
		node = node->subqueries.back().node();
	}
	if (node == nullptr || node->kind != engine::QueryKind::term) {
		++testh::failures;
		std::fprintf(stderr, "  FAIL: %s=%s is not a term query (kind %d)\n", field, value, query.node() ? static_cast<int>(query.node()->kind) : -1);
		return {};
	}
	return { node->field, node->str };
}

struct Person {
	const char* g;
	const char* e;  // nullptr: the document has no "e"
};

constexpr Person people[] = {
	{ "female", "brown" }, { "female", "blue" }, { "female", "green" },
	{ "male", "brown" }, { "male", "blue" }, { "male", "green" },
	{ "female", nullptr },
};

}  // namespace

// `_not` matches the documents that match none of its subqueries, however many there are, and a
// single subquery may be given as an object. Checked by ids against a filter over the same documents.
void test_dsl_not_is_none_of() {
	auto schema = keyword_schema();
	QueryDSL dsl(schema);
	auto shard = testsupport::scratch_shard("engine_query_dsl_not");
	for (std::size_t i = 0; i < std::size(people); ++i) {
		engine::Document doc;
		auto [gfield, gterm] = term_of(dsl, "g", people[i].g);
		doc.add_term(doc.field(gfield), gterm);
		if (people[i].e != nullptr) {
			auto [efield, eterm] = term_of(dsl, "e", people[i].e);
			doc.add_term(doc.field(efield), eterm);
		}
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));

	const std::pair<const char*, const char*> subqueries[] = { { "g", "female" }, { "e", "brown" }, { "e", "blue" }, { "g", "male" } };
	const auto has = [](const Person& person, const std::pair<const char*, const char*>& sub) {
		const char* value = std::string_view(sub.first) == "g" ? person.g : person.e;
		return value != nullptr && std::string_view(value) == sub.second;
	};
	const auto found = [&](const MsgPack& query) {
		return run(*shard, dsl.get_query(query));
	};

	// Every non-empty subset of the subqueries, in order: N = 1 to 4.
	std::size_t strict = 0;
	for (unsigned mask = 1; mask < (1u << std::size(subqueries)); ++mask) {
		MsgPack list = MsgPack::ARRAY();
		for (std::size_t k = 0; k < std::size(subqueries); ++k) {
			if (mask & (1u << k)) {
				list.push_back(MsgPack({ { subqueries[k].first, subqueries[k].second } }));
			}
		}
		std::set<engine::docid> expected;
		for (std::size_t i = 0; i < std::size(people); ++i) {
			bool any = false;
			for (std::size_t k = 0; k < std::size(subqueries); ++k) {
				any = any || ((mask & (1u << k)) && has(people[i], subqueries[k]));
			}
			if (!any) {
				expected.insert(static_cast<engine::docid>(i + 1));
			}
		}
		strict += !expected.empty() && expected.size() < std::size(people);
		CHECK_TRUE(found(MsgPack({ { "_not", list } })) == expected);
	}
	CHECK_GT(strict, std::size_t(5));

	// The cases of the audit, by value: ids are 1 female/brown, 2 female/blue, 3 female/green, 4 male/brown,
	// 5 male/blue, 6 male/green, 7 female without "e".
	const auto sub = [](const char* field, const char* value) { return MsgPack({ { field, value } }); };
	const auto list_of = [](std::initializer_list<MsgPack> items) {
		MsgPack list = MsgPack::ARRAY();
		for (const auto& item : items) {
			list.push_back(item);
		}
		return list;
	};
	using Ids = std::set<engine::docid>;
	CHECK_TRUE(found(MsgPack({ { "_not", list_of({ sub("g", "female"), sub("e", "brown") }) } })) == (Ids{ 5, 6 }));
	CHECK_TRUE(found(MsgPack({ { "_not", list_of({ sub("g", "female"), sub("e", "brown"), sub("e", "blue") }) } })) == (Ids{ 6 }));
	CHECK_TRUE(found(MsgPack({ { "_not", list_of({ sub("g", "female"), sub("g", "male") }) } })).empty());

	// A bare object is the list of one.
	CHECK_TRUE(found(MsgPack({ { "_not", sub("g", "female") } })) == (Ids{ 4, 5, 6 }));
	CHECK_TRUE(found(MsgPack({ { "_not", sub("e", "brown") } })) == (Ids{ 2, 3, 5, 6, 7 }));

	// Inside another clause.
	CHECK_TRUE(found(MsgPack({ { "_and", list_of({ MsgPack({ { "_not", list_of({ sub("e", "brown"), sub("e", "blue") }) } }), sub("g", "male") }) } })) == (Ids{ 6 }));
}

// A keyword value of one character is a term of itself: the check for a trailing "~N" fuzzy distance reads
// the second-to-last character only when there is one.
void test_dsl_one_character_keyword_is_a_term() {
	auto schema = keyword_schema();
	QueryDSL dsl(schema);
	for (const char* value : { "a", "0", "7" }) {
		CHECK_EQ(term_of(dsl, "g", value).second, std::string(value));
	}
}

}  // namespace detail
