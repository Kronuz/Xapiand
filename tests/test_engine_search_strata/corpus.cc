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

// The model corpus: random documents, the shards they are written to, and the index that holds them.

#include "support.h"

namespace detail {

std::string path_of(const char* name, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_search_strata_") + name + "_" + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

engine::Document make(int shard, int i, const Model::Doc& model) {
	engine::Document doc;
	const auto key = "Q" + std::to_string(shard) + "_" + std::to_string(i);
	doc.set_key(key);
	// Slot 2 groups the documents (every seventh has no group), slot 3 orders them.
	if (i % 7 != 0) {
		doc.add_value(2, "g" + std::to_string((shard * 3 + i) % 4));
	}
	doc.add_value(3, std::string(1, static_cast<char>('a' + (shard * 5 + i * 3) % 11)));
	if (model.key_wdf != 0) {
		// The id term's frequency counts in the document's length on both engines (D5).
		doc.add_term(doc.field(engine::KEY_FIELD), key, model.key_wdf);
	}
	for (const auto& [term, positions] : model.positions) {
		for (const auto position : positions) {
			doc.add_posting(doc.field("f"), term, position);
		}
	}
	for (const auto& term : model.terms) {
		if (!model.positions.contains(term)) {
			if (model.wdf.at(term) == 0) {
				doc.add_boolean_term(doc.field("f"), term);
			} else {
				doc.add_term(doc.field("f"), term, model.wdf.at(term));
			}
		}
	}
	doc.set_data("d" + std::to_string(i));
	return doc;
}

Model::Doc random_doc(std::mt19937& rng, int i) {
	Model::Doc doc;
	doc.live = true;
	const int length = 3 + static_cast<int>(rng() % 8);
	for (int p = 1; p <= length; ++p) {
		doc.positions["w" + std::to_string(rng() % vocabulary)].push_back(static_cast<std::uint32_t>(p));
	}
	for (const auto& [term, positions] : doc.positions) {
		doc.terms.insert(term);
	}
	doc.terms.insert("common");
	doc.terms.insert("tag" + std::to_string(rng() % 5));
	if (rng() % 3 == 0) {
		doc.terms.insert("tag" + std::to_string(rng() % 5));
	}
	if (i % 2 == 0) {
		doc.terms.insert("even");
	}
	doc.terms.insert("bool" + std::to_string(i % 3));   // a boolean term: present, weighing nothing
	doc.terms.insert("bool" + std::to_string(3 + rng() % 2));
	for (const auto& term : doc.terms) {
		const auto it = doc.positions.find(term);
		doc.wdf[term] = it != doc.positions.end() ? static_cast<std::uint32_t>(it->second.size()) : term.starts_with("bool") ? 0 : 1 + static_cast<std::uint32_t>(term.size() % 3);
		doc.length += doc.wdf[term];
	}
	doc.key_wdf = 1;
	doc.length += doc.key_wdf;
	return doc;
}

namespace {

// Writes document numbers [from, to) of the shard's model to the index.
void write_docs(Index& index, int shard, const Model& model, std::size_t from, std::size_t to) {
	const auto& list = model.shards[static_cast<std::size_t>(shard)];
	for (std::size_t i = from; i < to; ++i) {
		auto doc = make(shard, static_cast<int>(i + 1), list[i]);
		engine::sync_wait(index.owned[static_cast<std::size_t>(shard)]->add(doc, {}));
	}
}

}  // namespace

// An index holding the model's documents; `commits` splits each shard's load into that many commits,
// which leaves several segments.
// `host` opens the shards in host mode: their writes take turns (the caller's), a read sees what the last
// refresh flushed, and nothing a reader holds blocks a write.
void build(const char* name, int shard_count, int count, int commits, Index& strata, Model& model, bool host) {
	std::mt19937 rng(1234);
	model.shards.assign(static_cast<std::size_t>(shard_count), {});
	strata.owned.clear();
	engine::OpenOptions options;
	options.host = host;
	for (int s = 0; s < shard_count; ++s) {
		auto opened = engine::sync_wait(engine::open_local(path_of(name, s), true, true, options));
		strata.owned.push_back(std::move(opened.shard));
	}
	for (int s = 0; s < shard_count; ++s) {
		auto& list = model.shards[static_cast<std::size_t>(s)];
		for (int c = 0; c < commits; ++c) {
			const std::size_t from = list.size();
			for (int n = 0; n < count / commits; ++n) {
				list.push_back(random_doc(rng, static_cast<int>(list.size()) + 1));
			}
			write_docs(strata, s, model, from, list.size());
			engine::sync_wait(strata.owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
		}
	}
}

}  // namespace detail
