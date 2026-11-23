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

// Xapiand's value sources, the bytes they read, and a stored value that does not decode.

#include "support.h"

namespace detail {

// Xapiand's own value sources find the documents their range names.
void test_registered_sources() {
	auto shard = testsupport::scratch_shard("engine_query_sources");
	for (int i = 10; i <= 30; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		doc.add_value(5, "0" + std::to_string(i));
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));

	// The sources are built from their serialized form: their constructors are private to range.cc.
	auto serialized = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialize(data.begin(), data.end());
	};
	auto range = [&](std::string from, std::string to) { return MultipleValueRange{}.unserialize(serialized({ serialize_length(5), from, to })); };
	auto ge = [&](std::string from) { return MultipleValueGE{}.unserialize(serialized({ serialize_length(5), from })); };
	auto le = [&](std::string to) { return MultipleValueLE{}.unserialize(serialized({ serialize_length(5), to })); };
	const std::vector<std::pair<Query, std::set<engine::docid>>> cases = {
		{ Query::source(range("015", "018")), { 6, 7, 8, 9 } },
		{ Query::source(ge("028")), { 19, 20, 21 } },
		{ Query::source(le("012")), { 1, 2, 3 } },
		{ Query::combine(Op::FILTER, Query::source(range("015", "025")), Query::term("f", "t")), { 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 } },
	};
	for (const auto& [query, expected] : cases) {
		CHECK_TRUE(run(*shard, query) == expected);
	}
}

// Bytes a source never wrote: random, and valid serializations with a few bytes flipped, cut or
// extended. Each source must either rebuild or raise the engine's invalid error, and never
// anything else.
void test_unserialize_garbage() {
	auto serialized = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialize(data.begin(), data.end());
	};
	const std::vector<std::pair<const engine::PostingSource*, std::string>> seeds = {
		{ new MultipleValueRange{}, serialized({ serialize_length(5), "015", "018" }) },
		{ new MultipleValueGE{}, serialized({ serialize_length(5), "015" }) },
		{ new MultipleValueLE{}, serialized({ serialize_length(5), "015" }) },
		{ new GeoSpatialRange{}, serialized({ serialize_length(7), Serialize::ranges_centroids({ range_t(1, 9), range_t(20, 30) }, { Cartesian(1, 0, 0) }) }) },
	};
	Lcg rng{777};
	std::size_t rebuilt = 0, rejected = 0;
	for (const auto& [source, seed] : seeds) {
		for (int i = 0; i < 4000; ++i) {
			std::string bytes;
			switch (rng.next(4)) {
				case 0:
					for (auto n = rng.next(40); n > 0; --n) {
						bytes.push_back(static_cast<char>(rng.next(256)));
					}
					break;
				case 1:
					bytes = seed;
					for (auto n = rng.next(4) + 1; n > 0 && !bytes.empty(); --n) {
						bytes[rng.next(static_cast<std::uint32_t>(bytes.size()))] = static_cast<char>(rng.next(256));
					}
					break;
				case 2:
					bytes = seed.substr(0, rng.next(static_cast<std::uint32_t>(seed.size() + 1)));
					break;
				default:
					bytes = seed;
					for (auto n = rng.next(8); n > 0; --n) {
						bytes.push_back(static_cast<char>(rng.next(256)));
					}
					break;
			}
			try {
				auto rebuilt_source = source->unserialize(bytes);
				CHECK_TRUE(rebuilt_source != nullptr);
				// What rebuilt is usable: it describes and serializes without raising.
				(void)rebuilt_source->description();
				(void)rebuilt_source->serialize();
				++rebuilt;
			} catch (const engine::Error& exc) {
				CHECK_TRUE(exc.kind == engine::errc::invalid);
				++rejected;
			} catch (...) {
				CHECK_TRUE(false);
			}
		}
		delete source;
	}
	CHECK_TRUE(rebuilt > 100 && rejected > 100);
}

// A stored value that does not decode is not accepted; it must never stop the process.
void test_corrupt_stored_value() {
	auto serialized = [](std::initializer_list<std::string> parts) {
		std::vector<std::string> data(parts);
		return StringList::serialize(data.begin(), data.end());
	};
	std::vector<std::unique_ptr<engine::PostingSource>> sources;
	sources.push_back(MultipleValueRange{}.unserialize(serialized({ serialize_length(5), "a", "z" })));
	sources.push_back(MultipleValueGE{}.unserialize(serialized({ serialize_length(5), "a" })));
	sources.push_back(MultipleValueLE{}.unserialize(serialized({ serialize_length(5), "z" })));
	sources.push_back(GeoSpatialRange{}.unserialize(serialized({ serialize_length(5), Serialize::ranges_centroids({ range_t(1, 9), range_t(20, 30) }, { Cartesian(1, 0, 0) }) })));

	const std::string garbage_values[] = { "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff", "\x7f", std::string("\x05" "ab", 3), std::string("\x01\x00", 2) };
	for (const auto& source : sources) {
		for (const auto& value : garbage_values) {
			(void)source->accept(value);
			(void)source->weight(value);
		}
		Lcg rng{4242};
		for (int i = 0; i < 20000; ++i) {
			std::string bytes;
			for (auto n = rng.next(14) + 1; n > 0; --n) {
				bytes.push_back(static_cast<char>(rng.next(256)));
			}
			(void)source->accept(bytes);
		}
	}
	CHECK_FALSE(sources[0]->accept(garbage_values[2]));
	CHECK_FALSE(sources[1]->accept(garbage_values[2]));
	CHECK_FALSE(sources[3]->accept(garbage_values[2]));

	// A search over a slot with a corrupt value skips that document and finds the others.
	auto shard = testsupport::scratch_shard("engine_query_corrupt");
	for (int i = 0; i < 5; ++i) {
		engine::Document doc;
		doc.add_term(doc.field("f"), "t");
		std::vector<std::string> one = { std::string(1, static_cast<char>('b' + i)) };
		doc.add_value(5, i == 2 ? garbage_values[2] : StringList::serialize(one.begin(), one.end()));
		testsupport::add_keyed(*shard, doc, "K" + std::to_string(i));
	}
	engine::sync_wait(shard->commit(engine::Transaction::none));
	auto found = run(*shard, Query::source(MultipleValueGE{}.unserialize(serialized({ serialize_length(5), "a" }))));
	CHECK_TRUE((found == std::set<engine::docid>{ 1, 2, 4, 5 }));
}

}  // namespace detail
