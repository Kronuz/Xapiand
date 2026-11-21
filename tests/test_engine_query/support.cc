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

// The helpers the query tests share: ranked and unranked runs of a query on a shard, and a shard of terms.

#include "support.h"

namespace detail {

// The ids a query finds in a shard (at most 20), in rank order.
std::vector<engine::docid> ranked(engine::Shard& shard, const Query& query, const engine::KeyMaker* sorter, engine::MatchSpy* spy) {
	testsupport::Find find;
	find.maxitems = 20;
	find.check_at_least = 20;
	find.sorter = sorter;
	find.spy = spy;
	return testsupport::hit_ids(shard, query, find);
}

std::set<engine::docid> run(engine::Shard& shard, const Query& query, const engine::KeyMaker* sorter, engine::MatchSpy* spy) {
	const auto dids = ranked(shard, query, sorter, spy);
	return std::set<engine::docid>(dids.begin(), dids.end());
}

// One document per term, in the field "f".
std::unique_ptr<engine::Shard> terms_db(const char* name, std::initializer_list<const char*> terms) {
	auto shard = testsupport::scratch_shard(name);
	int n = 0;
	for (auto term : terms) {
		engine::Document doc;
		doc.add_term(doc.field("f"), term);
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(++n));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	return shard;
}

}  // namespace detail
