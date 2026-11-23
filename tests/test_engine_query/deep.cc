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

// Queries as deep and as long as their caps: chains of appends, trees, and phrase groups.

#include "support.h"

namespace detail {

// A chain of appends (one per key of a flat query object) costs no stack and keeps its meaning.
namespace {

struct ChainJob {
	std::size_t length = 0;
	bool done = false;
	std::string description;
};

void* chain_run(void* arg) {
	auto* job = static_cast<ChainJob*>(arg);
	{
		Query chain;
		for (std::size_t i = 0; i < job->length; ++i) {
			// Every 7th operand matches nothing, as a stop-word-only text does.
			chain = Query::append(Op::AND, chain, i % 7 == 6 ? Query() : Query::term("", "t" + std::to_string(i)));
		}
		job->description = chain.description().substr(0, 40);
		job->done = true;
	}
	return nullptr;
}

}  // namespace

void test_append_chain() {
	// 600 operands on a 256 KB stack: a frame per level would not fit.
	ChainJob job;
	job.length = 600;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 256 * 1024);
	pthread_t thread;
	CHECK_EQ(pthread_create(&thread, &attr, chain_run, &job), 0);
	pthread_join(thread, nullptr);
	pthread_attr_destroy(&attr);
	CHECK_TRUE(job.done);

	// The same chain, short, is the left fold of its operands: an operand that matches nothing leaves an
	// AND with a query that matches nothing, from which the next operand starts again.
	auto shard = testsupport::scratch_shard("engine_query_fold");
	for (int k = 0; k < 12; ++k) {
		// Each of twelve documents has every term but its own.
		engine::Document doc;
		for (int j = 0; j < 12; ++j) {
			if (j != k) {
				doc.add_term(doc.field("f"), "t" + std::to_string(j));
			}
		}
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(k));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	for (const Op op : { Op::AND, Op::OR }) {
		Query chain;
		Query expected;
		for (int i = 0; i < 12; ++i) {
			auto operand = i % 5 == 4 ? Query() : Query::term("f", "t" + std::to_string(i));
			chain = Query::append(op, chain, operand);
			expected = expected.empty() ? operand : Query::combine(op, expected, operand);
		}
		CHECK_EQ(chain.empty(), expected.empty());
		CHECK_TRUE(run(*shard, chain) == run(*shard, expected));
		// AND: the empty operands 4 and 9 cut the chain, so only t10 AND t11 remain; OR skips the empty ones.
		CHECK_EQ(run(*shard, chain).size(), op == Op::AND ? 10u : 12u);
	}
}

// A tree as deep as the cap converts and describes itself on a small stack, and one level more is a 400.
namespace {

struct DepthJob {
	bool done = false;
	std::string description;
	bool nonempty = false;
};

void* depth_run(void* arg) {
	auto* job = static_cast<DepthJob*>(arg);
	Query tree = Query::term("", "a");
	for (unsigned level = 1; level < engine::MAX_QUERY_DEPTH; ++level) {
		// Alternating operators and a scale: nothing is flattened into a wide node.
		tree = level % 5 == 0 ? Query::scale(tree, 2.0) : Query::combine(level % 2 ? Op::AND : Op::OR, tree, Query::term("", "b"));
	}
	job->description = tree.description();
	job->nonempty = !tree.empty();
	job->done = true;
	return nullptr;
}

}  // namespace

void test_depth_cap() {
	// 40 KB of stack: a frame per level of a tree at the cap would not fit.
	DepthJob job;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 40 * 1024);
	pthread_t thread;
	CHECK_EQ(pthread_create(&thread, &attr, depth_run, &job), 0);
	pthread_join(thread, nullptr);
	pthread_attr_destroy(&attr);
	CHECK_TRUE(job.done);
	CHECK_TRUE(job.description.size() > engine::MAX_QUERY_DEPTH);
	CHECK_TRUE(job.nonempty);

	// Exactly the cap builds; one more level of any operator throws a client error.
	Query tree = Query::term("", "a");
	for (unsigned level = 1; level < engine::MAX_QUERY_DEPTH; ++level) {
		tree = Query::combine(Op::AND, tree, Query::term("", "b"));
	}
	CHECK_EQ(tree.depth(), engine::MAX_QUERY_DEPTH);
	const auto refused = [](auto&& build, const char* message = "nested too deeply") {
		try {
			build();
		} catch (const QueryDepthError& exc) {
			return std::string(exc.what()).find(message) != std::string::npos && dynamic_cast<const ClientError*>(&exc) != nullptr;
		}
		return false;
	};
	CHECK_TRUE(refused([&] { Query::combine(Op::OR, tree, Query::term("", "b")); }));
	CHECK_TRUE(refused([&] { Query::combine(Op::OR, std::vector<Query>{ tree }, 0); }));
	CHECK_TRUE(refused([&] { Query::scale(tree, 2.0); }));

	// An append adds no level: a flat object with many keys is not nested.
	Query flat;
	for (unsigned i = 0; i < 4 * engine::MAX_QUERY_DEPTH; ++i) {
		flat = Query::append(Op::AND, flat, Query::term("", "t" + std::to_string(i)));
	}
	CHECK_EQ(flat.depth(), 1U);
	CHECK_EQ(Query::append(Op::AND, tree, Query::term("", "c")).depth(), engine::MAX_QUERY_DEPTH);

	// The clause cap: a node per term and one for the operator, appends and shared subtrees included.
	std::vector<Query> wide(engine::MAX_QUERY_CLAUSES - 1, Query::term("", "t"));
	CHECK_EQ(Query::combine(Op::OR, wide).size(), engine::MAX_QUERY_CLAUSES);
	wide.push_back(Query::term("", "t"));
	CHECK_TRUE(refused([&] { Query::combine(Op::OR, wide); }, "too large"));
	Query chain;
	for (unsigned i = 0; i < engine::MAX_QUERY_CLAUSES; ++i) {
		chain = Query::append(Op::AND, chain, Query::term("", "t"));
	}
	CHECK_EQ(chain.size(), engine::MAX_QUERY_CLAUSES);
	CHECK_TRUE(refused([&] { Query::append(Op::AND, chain, Query::term("", "t")); }, "too large"));
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, chain, chain); }, "too large"));
	CHECK_FALSE(flat.empty());
}

// The matcher recurses once per phrase or near group an AND joins, so the query counts the
// links it would chain: AND and FILTER add their operands' up, every other operator takes the deepest.
void test_chain_cap() {
	const auto phrase = [](const char* a, const char* b, Op op = Op::PHRASE) { return Query::combine(op, std::vector<Query>{ Query::term("", a), Query::term("", b) }, 2); };
	const auto many = [&](unsigned n, Op op = Op::PHRASE) {
		std::vector<Query> list;
		for (unsigned i = 0; i < n; ++i) {
			list.push_back(phrase("a", "b", op));
		}
		return list;
	};
	const auto refused = [](auto&& build) {
		try {
			build();
		} catch (const QueryDepthError& exc) {
			return std::string(exc.what()).find("too many phrases") != std::string::npos && dynamic_cast<const ClientError*>(&exc) != nullptr;
		}
		return false;
	};
	const unsigned cap = engine::MAX_QUERY_CHAIN;
	CHECK_EQ(Query::term("", "a").chain(), 0U);
	CHECK_EQ(phrase("a", "b").chain(), 1U);
	CHECK_EQ(phrase("a", "b", Op::NEAR).chain(), 1U);
	CHECK_EQ(Query::combine(Op::AND, many(cap)).chain(), cap);
	CHECK_EQ(Query::combine(Op::FILTER, many(cap)).chain(), cap);
	for (const auto op : { Op::AND, Op::FILTER }) {
		CHECK_TRUE(refused([&] { Query::combine(op, many(cap + 1)); }));
		CHECK_TRUE(refused([&] { Query::combine(op, many(cap + 1, Op::NEAR)); }));
	}
	// Near groups and phrases are counted together; terms and groups of terms are not links.
	auto mixed = many(cap / 2);
	for (auto& near : many(cap / 2, Op::NEAR)) {
		mixed.push_back(std::move(near));
	}
	CHECK_EQ(Query::combine(Op::AND, mixed).chain(), cap);
	mixed.push_back(phrase("c", "d"));
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, mixed); }));
	std::vector<Query> terms(1000, Query::term("", "t"));
	terms.push_back(Query::combine(Op::OR, terms));
	CHECK_EQ(Query::combine(Op::AND, terms).chain(), 0U);

	// Other operators run their operands apart: an OR of thousands of phrases is no deeper than one.
	for (const auto op : { Op::OR, Op::XOR, Op::AND_MAYBE, Op::AND_NOT, Op::MAX }) {
		CHECK_EQ(Query::combine(op, many(1000), 5).chain(), 1U);
	}
	CHECK_EQ(Query::combine(Op::AND_NOT, Query::combine(Op::AND, many(cap)), Query::combine(Op::AND, many(cap))).chain(), cap);

	// Operands pass their links up through AND, however they are wrapped.
	const auto wrapped = [&](unsigned n) {
		std::vector<Query> list;
		for (unsigned i = 0; i < n; ++i) {
			list.push_back(i % 3 == 0 ? Query::combine(Op::AND, phrase("a", "b"), Query::term("", "c"))
				: i % 3 == 1 ? Query::combine(Op::AND_NOT, phrase("a", "b"), Query::term("", "z"))
				: Query::combine(Op::OR, std::vector<Query>{ phrase("a", "b") }, 0));
		}
		return list;
	};
	CHECK_EQ(Query::combine(Op::AND, wrapped(cap)).chain(), cap);
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, wrapped(cap + 1)); }));
	// An OR between ANDs does not hide the links below it, and sibling ORs do not add up.
	const auto half = Query::combine(Op::AND, many(cap / 2));
	CHECK_EQ(Query::combine(Op::AND, Query::combine(Op::OR, half, Query::term("", "c")), half).chain(), cap);
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, Query::combine(Op::OR, half, Query::term("", "c")), Query::combine(Op::AND, many(cap / 2 + 1))); }));
	CHECK_EQ(Query::combine(Op::OR, half, half).chain(), cap / 2);
	// A chain of appends (the text path) adds up as an AND does.
	Query chain;
	for (unsigned i = 0; i < cap; ++i) {
		chain = Query::append(Op::AND, chain, phrase("a", "b"));
	}
	CHECK_EQ(chain.chain(), cap);
	CHECK_TRUE(refused([&] { Query::append(Op::AND, chain, phrase("a", "b")); }));
	CHECK_EQ(Query::append(Op::OR, chain, phrase("a", "b")).chain(), cap);
	// A scale passes its operand's links through.
	CHECK_EQ(Query::scale(Query::combine(Op::AND, many(cap)), 2.0).chain(), cap);
	// Shared subtrees count each time, as they do for the clause cap.
	CHECK_TRUE(refused([&] { Query::combine(Op::AND, Query::combine(Op::AND, many(cap)), phrase("a", "b")); }));
	// The matcher runs a query at the cap.
	auto shard = testsupport::scratch_shard("engine_query_chain");
	engine::Document doc;
	doc.add_posting(doc.field("f"), "a", 1);
	doc.add_posting(doc.field("f"), "b", 2);
	testsupport::add_keyed(*shard, doc, "K1");
	engine::sync_wait(shard->commit(engine::Transaction::none));
	const auto fphrase = [](Op op) {
		return Query::combine(op, std::vector<Query>{ Query::term("f", "a"), Query::term("f", "b") }, 2);
	};
	std::vector<Query> at_cap(cap, fphrase(Op::PHRASE));
	CHECK_EQ(run(*shard, Query::combine(Op::AND, at_cap)).size(), std::size_t(1));
}

}  // namespace detail
