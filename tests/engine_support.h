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

// Helpers the engine tests share: a scratch strata shard in the working directory, documents with
// their own keys, a search that returns the ids in rank order, and a DocView over values in memory.

#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "engine/callbacks.h"
#include "engine/doc_view.h"
#include "engine/document.h"
#include "engine/open.h"
#include "engine/query.h"
#include "engine/search.h"
#include "engine/sync.h"

namespace testsupport {

// A shard at a fresh path in the working directory; the files stay until the next call with that name.
inline std::unique_ptr<engine::Shard> scratch_shard(const std::string& name) {
	const auto path = (std::filesystem::current_path() / (name + ".db")).string();
	std::filesystem::remove_all(path);
	return engine::sync_wait(engine::open_local(path, true, true)).shard;
}

// Adds the document under a key of its own ("K" and a running number per shard path is the caller's).
inline engine::docid add_keyed(engine::Shard& shard, engine::Document& doc, const std::string& key) {
	doc.set_key(key);
	return engine::sync_wait(shard.add(doc, {})).did;
}

// A search window and what rides along with it.
struct Find {
	engine::doccount first = 0;
	engine::doccount maxitems = 1000;
	const engine::KeyMaker* sorter = nullptr;
	engine::MatchSpy* spy = nullptr;
	engine::DocidOrder order = engine::DocidOrder::ascending;
	engine::doccount check_at_least = 1000;
};

// The result of a search over the shards.
inline engine::SearchResult search(std::vector<engine::Shard*> shards, const engine::Query& query, const Find& find = {}) {
	auto prepared = engine::prepare(query);
	engine::SearchRequest request;
	request.query = prepared.get();
	request.first = find.first;
	request.maxitems = find.maxitems;
	request.check_at_least = find.check_at_least;
	request.sorter = find.sorter;
	request.spy = find.spy;
	request.order = find.order;
	return engine::sync_wait(engine::search(shards, request));
}

// The ids of the hits, in rank order.
inline std::vector<engine::docid> hit_ids(engine::Shard& shard, const engine::Query& query, const Find& find = {}) {
	std::vector<engine::docid> ids;
	for (const auto& hit : search({&shard}, query, find).hits) {
		ids.push_back(hit.did);
	}
	return ids;
}

// A stored document held in memory, for a callback that reads one.
class MapView final : public engine::DocView {
public:
	explicit MapView(std::map<engine::valueno, std::string> values = {}, std::string data = {}, engine::docid did = 1)
		: values_(std::move(values)), data_(std::move(data)), did_(did) { }

	engine::docid did() const override { return did_; }
	std::string data() const override { return data_; }
	std::string value(engine::valueno slot) const override {
		const auto it = values_.find(slot);
		return it == values_.end() ? std::string() : it->second;
	}
	engine::rev version() const override { return 0; }
	void values(const engine::ValueSink& sink) const override {
		for (const auto& [slot, value] : values_) {
			sink(slot, value);
		}
	}
	std::string serialise() const override { return {}; }

private:
	std::map<engine::valueno, std::string> values_;
	std::string data_;
	engine::docid did_;
};

}  // namespace testsupport
