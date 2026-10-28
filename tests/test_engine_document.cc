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

// Unit tests for the engine seam's Document: it must build the same document as the Xapian
// calls it replaces (same serialised bytes, so old write-ahead log lines stay readable),
// and Xapian's exceptions must classify into the seam's error kinds.

#include "test_harness.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "config.h"
#include "engine/xapian/convert.h"
#include "engine/xapian/errors.h"
#include "multivalue/generate_terms.h"
#include "engine/xapian/xapian.h"

namespace {

bool same(const engine::Document& a, const engine::Document& b) {
	return a.serialise() == b.serialise();
}

// The within-document frequency and positions of a term in the document, by Xapian's own view of it.
engine::termcount wdf_of(const engine::Document& doc, const std::string& name) {
	const auto& xdoc = engine::xapian::xapian_document(doc);
	auto it = xdoc.termlist_begin();
	it.skip_to(name);
	return it != xdoc.termlist_end() && *it == name ? it.get_wdf() : 0;
}

std::size_t positions_of(const engine::Document& doc, const std::string& name) {
	const auto& xdoc = engine::xapian::xapian_document(doc);
	auto it = xdoc.termlist_begin();
	it.skip_to(name);
	if (it == xdoc.termlist_end() || *it != name) {
		return 0;
	}
	return it.positionlist_count();
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

	// Xapian agrees that an empty value removes the slot.
	Xapian::Document xdoc;
	xdoc.add_value(5, "x");
	xdoc.add_value(5, "");
	CHECK_EQ(xdoc.values_count(), 0u);
}

static void test_terms() {
	engine::Document doc;
	doc.add_term("a", 2);
	doc.add_term("a", 3);
	CHECK_EQ(wdf_of(doc, "a"), 5u);
	doc.add_boolean_term("a");  // keeps the frequency
	CHECK_EQ(wdf_of(doc, "a"), 5u);
	doc.add_boolean_term("b");
	CHECK_EQ(wdf_of(doc, "b"), 0u);
	CHECK_EQ(positions_of(doc, "b"), 0u);
	doc.add_posting("c", 7, 2);
	doc.add_posting("c", 7, 1);  // the same position once, the frequency twice
	doc.add_posting("c", 3, 0);
	CHECK_EQ(wdf_of(doc, "c"), 3u);
	CHECK_EQ(positions_of(doc, "c"), 2u);
}

static void test_matches_xapian() {
	for (std::uint64_t seed = 1; seed <= 200; ++seed) {
		Lcg rng{seed};
		engine::Document doc;
		Xapian::Document xdoc;
		const int ops = 1 + static_cast<int>(rng.next(40));
		for (int i = 0; i < ops; ++i) {
			const std::string term = "t" + std::to_string(rng.next(8));
			const auto n = rng.next(6);
			if (n == 0) {
				const auto wdf = rng.next(4);
				doc.add_term(term, wdf);
				xdoc.add_term(term, wdf);
			} else if (n == 1) {
				doc.add_boolean_term(term);
				xdoc.add_boolean_term(term);
			} else if (n == 2) {
				const auto pos = 1 + rng.next(6);
				const auto wdf = rng.next(3);
				doc.add_posting(term, pos, wdf);
				xdoc.add_posting(term, pos, wdf);
			} else if (n == 3) {
				const auto slot = rng.next(5);
				const std::string value = rng.next(3) == 0 ? std::string() : "v" + std::to_string(rng.next(100));
				doc.add_value(slot, value);
				xdoc.add_value(slot, value);
			} else if (n == 4) {
				const std::string data = "d" + std::to_string(rng.next(100));
				doc.set_data(data);
				xdoc.set_data(data);
			}
		}

		// Same stored bytes as the Xapian document built by the same calls.
		CHECK_EQ(doc.serialise(), xdoc.serialise());

		// And back: the stored form reads as the same document.
		CHECK_TRUE(same(engine::Document::unserialise(xdoc.serialise()), doc));
		CHECK_TRUE(same(engine::xapian::from_xapian(xdoc), doc));
	}
}

template<typename E>
static engine::errc classified(const E& error) {
	try {
		throw error;
	} catch (...) {
		return engine::xapian::classify_current();
	}
}

static void test_classify() {
	using engine::errc;
	CHECK_TRUE(classified(Xapian::DatabaseNotFoundError("x")) == errc::not_found);
	CHECK_TRUE(classified(Xapian::DatabaseNotAvailableError("x")) == errc::not_available);
	CHECK_TRUE(classified(Xapian::DatabaseModifiedError("x")) == errc::reopen);
	CHECK_TRUE(classified(Xapian::DatabaseCorruptError("x")) == errc::corrupt);
	CHECK_TRUE(classified(Xapian::DatabaseOpeningError("x")) == errc::opening);
	CHECK_TRUE(classified(Xapian::DatabaseClosedError("x")) == errc::closed);
	CHECK_TRUE(classified(Xapian::NetworkTimeoutError("x")) == errc::timeout);
	CHECK_TRUE(classified(Xapian::NetworkError("x")) == errc::network);
	CHECK_TRUE(classified(Xapian::DocNotFoundError("x")) == errc::doc_not_found);
	CHECK_TRUE(classified(Xapian::DocVersionConflictError("x")) == errc::version_conflict);
	CHECK_TRUE(classified(Xapian::InvalidArgumentError("x")) == errc::invalid);
	CHECK_TRUE(classified(Xapian::InvalidOperationError("x")) == errc::invalid);
	CHECK_TRUE(classified(Xapian::QueryParserError("x")) == errc::syntax);
	CHECK_TRUE(classified(Xapian::DatabaseError("x")) == errc::database);
	CHECK_TRUE(classified(Xapian::InternalError("x")) == errc::internal);
	CHECK_TRUE(classified(std::runtime_error("x")) == errc::internal);

	CHECK_TRUE(classified(Xapian::DatabaseLockError("x")) == errc::database);
}

template<typename E>
static void translate(const E& error) {
	try {
		throw error;
	} catch (...) {
		engine::xapian::rethrow_translated();
	}
}

static void test_translation() {
	using engine::errc;

	// A Xapian failure comes out as an engine error of its kind that is still the Xapian exception.
	bool as_engine = false;
	try {
		translate(Xapian::DocNotFoundError("no such doc"));
	} catch (const engine::Error& error) {
		as_engine = error.kind == errc::doc_not_found && error.reason().find("no such doc") != std::string::npos;
	}
	CHECK_TRUE(as_engine);

	bool as_xapian = false;
	try {
		translate(Xapian::DatabaseNotFoundError("gone"));
	} catch (const Xapian::DatabaseOpeningError&) {  // a not-found is an opening failure
		as_xapian = true;
	}
	CHECK_TRUE(as_xapian);

	// Any other database failure is a database error.
	errc kind = errc::internal;
	try {
		translate(Xapian::DatabaseLockError("locked"));
	} catch (const engine::Error& error) {
		kind = error.kind;
	}
	CHECK_TRUE(kind == errc::database);

	// An engine error passes through unchanged, and what is not Xapian's is not touched.
	bool unchanged = false;
	try {
		translate(engine::xapian::Network("net"));
	} catch (const engine::xapian::Network&) {
		unchanged = true;
	}
	CHECK_TRUE(unchanged);

	bool foreign = false;
	try {
		translate(std::runtime_error("other"));
	} catch (const std::runtime_error&) {
		foreign = true;
	}
	CHECK_TRUE(foreign);
}

static void test_view() {
	Xapian::Document xdoc;
	xdoc.set_data("payload");
	xdoc.add_value(3, "three");
	xdoc.add_term("Aone", 2);
	xdoc.add_term("Atwo", 1);
	xdoc.add_term("Bthree", 4);

	auto view = engine::xapian::make_view(xdoc);
	CHECK_EQ(view->data(), "payload");
	CHECK_EQ(view->value(3), "three");
	CHECK_EQ(view->value(4), "");
	CHECK_EQ(view->version(), 0u);

	std::vector<std::string> seen;
	view->terms("A", [&](std::string_view term, engine::termcount wdf) {
		seen.push_back(std::string(term) + ":" + std::to_string(wdf));
	});
	CHECK_EQ(seen.size(), 2u);
	CHECK_EQ(seen[0], "Aone:2");
	CHECK_EQ(seen[1], "Atwo:1");
}

#ifdef USE_ICU
// The word-break path runs Xapian's term generator on a Xapian document and merges the result.
static void test_word_breaks() {
	const std::string text = "Hello world, hello 我爱北京天安门 and 東京都";
	for (bool positions : {true, false}) {
		engine::Document doc;
		GenerateTerms::text(doc, "Zt", text, positions, 3, false, true, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none);

		Xapian::Document xdoc;
		Xapian::TermGenerator generator;
		generator.set_flags(Xapian::TermGenerator::FLAG_WORD_BREAKS);
		generator.set_document(xdoc);
		if (positions) {
			generator.index_text(text, 3, "Zt");
		} else {
			generator.index_text_without_positions(text, 3, "Zt");
		}

		CHECK_TRUE(engine::xapian::xapian_document(doc).termlist_count() > 0);
		CHECK_TRUE(same(doc, engine::xapian::from_xapian(xdoc)));
	}
}
#endif

#ifdef USE_ICU
TEST_MAIN(test_values, test_terms, test_matches_xapian, test_classify, test_translation, test_view, test_word_breaks)
#else
TEST_MAIN(test_values, test_terms, test_matches_xapian, test_classify, test_translation, test_view)
#endif
