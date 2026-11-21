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

// The corpus: random documents written to a set of strata shards and, as a whole, to one more shard.

#include "support.h"

namespace detail {

// --- the corpus ---

namespace {

const std::vector<std::string> words = {"apple", "apricot", "apply", "banana", "band", "bandana", "bond", "cat", "cart", "cast", "coat", "caf\xc3\xa9", "cafe", "caf\xc3\xa9s", "na\xc3\xafve"};

std::string pad(unsigned n, int width) {
	char buffer[32];
	std::snprintf(buffer, sizeof buffer, "%0*u", width, n);
	return buffer;
}

std::string path_of(const char* name, int shard) {
	const auto path = (std::filesystem::current_path() / (std::string("engine_callbacks_") + name + "_" + std::to_string(shard) + ".db")).string();
	std::filesystem::remove_all(path);
	return path;
}

}  // namespace

namespace {

Spec random_spec(std::mt19937& rng, int shard, int i) {
	Spec spec;
	const int k = 1 + static_cast<int>(rng() % 4);
	for (int n = 0; n < k; ++n) {
		spec.terms[words[rng() % words.size()]] += 1 + rng() % 3;
	}
	spec.terms["common"] = 1;
	spec.terms["tag" + std::to_string(i % 5)] = 1 + i % 3;
	if (i % 2 == 0) {
		spec.terms["even"] = 2;
	}
	spec.key = pad(rng() % 1000000, 6) + "-" + std::to_string(shard) + "-" + std::to_string(i);
	spec.group = "g" + std::to_string(rng() % 4);
	spec.has_number = i % 7 != 0;
	spec.number = pad(rng() % 1000, 5);
	spec.data = "d" + std::to_string(shard) + "_" + std::to_string(i);
	return spec;
}

}  // namespace

std::string key_of(int shard, int i) {
	return "Q" + std::to_string(shard) + "_" + std::to_string(i);
}

engine::Document make(int shard, int i, const Spec& spec) {
	engine::Document doc;
	doc.set_key(key_of(shard, i));
	const auto flat = doc.field("f");   // the terms are flat strings; no field name is joined to them
	for (const auto& [term, wdf] : spec.terms) {
		doc.add_term(flat, term, wdf);
	}
	doc.add_boolean_term(flat, "B" + spec.group);   // a term without frequency, as the fields of a schema make
	doc.add_value(KEY, spec.key);
	doc.add_value(GROUP, spec.group);
	if (spec.has_number) {
		doc.add_value(NUMBER, spec.number);
	}
	doc.set_data(spec.data);
	return doc;
}

// The documents on `shard_count` shards, `commits` commits per shard (several segments), and all of them in one more shard.
void build(const char* name, int shard_count, int count, int commits, Corpus& corpus) {
	std::mt19937 rng(4321);
	corpus.specs.assign(static_cast<std::size_t>(shard_count), {});
	corpus.strata.owned.clear();
	for (int s = 0; s < shard_count; ++s) {
		corpus.strata.owned.push_back(engine::sync_wait(engine::open_local(path_of(name, s), true, true)).shard);
	}
	corpus.merged.owned.clear();
	corpus.merged.owned.push_back(engine::sync_wait(engine::open_local(path_of(name, 99), true, true)).shard);
	for (int s = 0; s < shard_count; ++s) {
		auto& list = corpus.specs[static_cast<std::size_t>(s)];
		for (int c = 0; c < commits; ++c) {
			for (int n = 0; n < count / commits; ++n) {
				const int i = static_cast<int>(list.size()) + 1;
				list.push_back(random_spec(rng, s, i));
				// Shard 0 also holds a few terms of known, distinct frequencies.
				if (s == 0) {
					for (const auto& [term, upto] : {std::pair{"mf_a", 5}, {"mf_b", 3}, {"mf_c", 8}, {"mf_d", 1}}) {
						if (i <= upto) {
							list.back().terms[term] = 1;
						}
					}
				}
				for (auto* index : {&corpus.strata, &corpus.merged}) {
					auto doc = make(s, i, list.back());
					engine::sync_wait(index->owned[index == &corpus.merged ? 0 : static_cast<std::size_t>(s)]->add(doc, {}));
				}
			}
			engine::sync_wait(corpus.strata.owned[static_cast<std::size_t>(s)]->commit(engine::Transaction::none));
		}
	}
	engine::sync_wait(corpus.merged.owned[0]->commit(engine::Transaction::none));
}

}  // namespace detail
