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

// Unit tests for the engine seam's Document: the calls build the terms, values and data they
// describe (checked against a model), and the serialised form reads back as the same document.

#include "test_harness.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "engine/document.h"
#include "engine/strata/document.h"

namespace {

using engine::strata::DocTerm;
using engine::strata::StrataDocument;

const StrataDocument& strata_of(const engine::Document& doc) {
	return static_cast<const StrataDocument&>(doc.native());
}

bool same(const engine::Document& a, const engine::Document& b) {
	return a.serialise() == b.serialise();
}

// The within-document frequency of a term of the field "f", 0 when the document has no such term.
engine::termcount wdf_of(const engine::Document& doc, const std::string& name) {
	const auto* term = strata_of(doc).find_term("f", name);
	return term ? term->wdf : 0;
}

std::size_t positions_of(const engine::Document& doc, const std::string& name) {
	const auto& native = strata_of(doc);
	const auto* term = native.find_term("f", name);
	return term ? native.positions_of(*term).size() : 0;
}

// A small deterministic generator, so a failure reproduces.
struct Lcg {
	std::uint64_t state;
	std::uint32_t next(std::uint32_t bound) {
		state = state * 6364136223846793005ULL + 1442695040888963407ULL;
		return static_cast<std::uint32_t>((state >> 33) % bound);
	}
};

}  // namespace

static void test_values() {
	engine::Document doc;
	CHECK_EQ(doc.get_value(5), "");
	doc.add_value(5, "x");
	CHECK_EQ(doc.get_value(5), "x");
	doc.add_value(5, "y");  // replaces
	CHECK_EQ(doc.get_value(5), "y");
	doc.add_value(5, "");  // removes
	CHECK_EQ(doc.get_value(5), "");
	CHECK_TRUE(doc.serialise() == engine::Document().serialise());
}

static void test_terms() {
	engine::Document doc;
	doc.add_term(doc.field("f"), "a", 2);
	doc.add_term(doc.field("f"), "a", 3);
	CHECK_EQ(wdf_of(doc, "a"), 5u);
	doc.add_boolean_term(doc.field("f"), "a");  // keeps the frequency
	CHECK_EQ(wdf_of(doc, "a"), 5u);
	doc.add_boolean_term(doc.field("f"), "b");
	CHECK_EQ(wdf_of(doc, "b"), 0u);
	CHECK_EQ(positions_of(doc, "b"), 0u);
	CHECK_TRUE(strata_of(doc).find_term("f", "b") != nullptr);
	CHECK_TRUE(strata_of(doc).find_term("f", "absent") == nullptr);
	doc.add_posting(doc.field("f"), "c", 7, 2);
	doc.add_posting(doc.field("f"), "c", 7, 1);  // the same position once, the frequency twice
	doc.add_posting(doc.field("f"), "c", 3, 0);
	CHECK_EQ(wdf_of(doc, "c"), 3u);
	CHECK_EQ(positions_of(doc, "c"), 2u);
	const auto positions = strata_of(doc).positions_of(*strata_of(doc).find_term("f", "c"));
	CHECK_TRUE((positions == std::vector<engine::termpos>{3, 7}));
}

// Random calls against a model of what they mean; then the serialised form reads back the same.
static void test_matches_model() {
	struct Term {
		engine::termcount wdf = 0;
		std::set<engine::termpos> positions;
	};
	for (std::uint64_t seed = 1; seed <= 200; ++seed) {
		Lcg rng{seed};
		engine::Document doc;
		std::map<std::string, Term> terms;
		std::map<engine::valueno, std::string> values;
		std::string data;
		const int ops = 1 + static_cast<int>(rng.next(40));
		for (int i = 0; i < ops; ++i) {
			const std::string term = "t" + std::to_string(rng.next(8));
			const auto n = rng.next(6);
			if (n == 0) {
				const auto wdf = rng.next(4);
				doc.add_term(doc.field("f"), term, wdf);
				terms[term].wdf += wdf;
			} else if (n == 1) {
				doc.add_boolean_term(doc.field("f"), term);
				terms[term];
			} else if (n == 2) {
				const auto pos = 1 + rng.next(6);
				const auto wdf = rng.next(3);
				doc.add_posting(doc.field("f"), term, pos, wdf);
				terms[term].wdf += wdf;
				terms[term].positions.insert(pos);
			} else if (n == 3) {
				const auto slot = rng.next(5);
				const std::string value = rng.next(3) == 0 ? std::string() : "v" + std::to_string(rng.next(100));
				doc.add_value(slot, value);
				if (value.empty()) {
					values.erase(slot);
				} else {
					values[slot] = value;
				}
			} else if (n == 4) {
				data = "d" + std::to_string(rng.next(100));
				doc.set_data(data);
			}
		}

		const auto check = [&](const engine::Document& d) {
			const auto& native = strata_of(d);
			CHECK_EQ(native.terms().size(), terms.size());
			for (const auto& [name, term] : terms) {
				CHECK_EQ(wdf_of(d, name), term.wdf);
				const auto* found = native.find_term("f", name);
				CHECK_TRUE(found != nullptr);
				if (found) {
					const auto positions = native.positions_of(*found);
					CHECK_TRUE((std::set<engine::termpos>(positions.begin(), positions.end()) == term.positions));
				}
			}
			CHECK_TRUE(native.values() == values);
			CHECK_EQ(d.get_data(), data);
		};
		check(doc);

		// And back: the stored form reads as the same document.
		const auto bytes = doc.serialise();
		const auto back = engine::Document::unserialise(bytes);
		check(back);
		CHECK_EQ(back.serialise(), bytes);
		CHECK_TRUE(same(back, doc));
	}
}

TEST_MAIN(test_values, test_terms, test_matches_model)
