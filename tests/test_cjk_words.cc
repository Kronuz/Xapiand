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

// _cjk_words: CJK text is indexed and queried as the words of analysis's segmenter, the words
// ICU's word break iterator finds. The same split feeds the indexer and the query parser.

#include "test_harness.h"

#include <string>
#include <utility>
#include <vector>

#include "analysis.hh"
#include "database/schema.h"
#include "engine/document.h"
#include "engine/text.h"
#include "msgpack.h"
#include "multivalue/generate_terms.h"
#include "xapiand_exception.h"

namespace {

namespace text = engine::text;

// Samples, with the words the segmenter gives (its own ICU-checked vectors segment the same).
struct Sample {
	const char* text;
	const char* words;
};

const std::vector<Sample> samples = {
	{ "我喜欢吃苹果", "我 喜欢 吃 苹果" },                          // Chinese
	{ "東京都に住んでいます", "東京 都 に 住 んで い ます" },           // Japanese
	{ "한국어는 아름답다", "한국어는 아름답다" },                      // Korean: Hangul is not divided
	{ "abc中文def ok", "abc 中文 def ok" },                         // CJK meets other words
	{ "hello world 中文*", "hello world 中文*" },                  // no space before an operator
	{ "plain latin text", "plain latin text" },
	{ "", "" },
};

void
test_words_are_the_segmenters()
{
	for (const auto& s : samples) {
		CHECK_EQ(text::cjk_words(s.text), std::string(s.words));
	}
}

void
test_indexing_splits_words()
{
	for (const auto& s : samples) {
		engine::Document words, spaced;
		GenerateTerms::text(words, "t", s.text, true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none, true);
		GenerateTerms::text(spaced, "t", s.words, true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none);
		CHECK_EQ(words.serialize(), spaced.serialize());
	}
	// Without the flag a run of CJK stays one word.
	engine::Document plain, split;
	GenerateTerms::text(plain, "t", "我喜欢吃苹果", true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none);
	GenerateTerms::text(split, "t", "我喜欢吃苹果", true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none, true);
	CHECK_NE(plain.serialize(), split.serialize());
}

void
test_queries_split_words()
{
	engine::TextConfig c;
	c.field = "t";
	for (const auto& s : samples) {
		std::string words, spaced;
		c.flags = text::CJK_WORDS;
		engine::parse_text(s.text, c, &words);
		c.flags = 0;
		engine::parse_text(s.words, c, &spaced);
		CHECK_EQ(words, spaced);
	}
}

// The letter infixes the term generator joins words across, beside CJK words, in Chinese,
// Japanese and Korean: each reads as the same words as the text with a space in its place.
void
test_infix_splits_cjk_words()
{
	const std::vector<std::string> infixes = { "·", "‧", "&", "'", "’" };
	const std::vector<std::pair<std::string, std::string>> pairs = {
		{ "乔治", "华盛顿" },      // Chinese
		{ "中文", "英文" },        // Chinese
		{ "東京", "大阪" },        // Japanese kanji
		{ "ジョン", "スミス" },    // Japanese katakana
		{ "가나다", "라마" },      // Korean
		{ "中文", "abc" },         // CJK then Latin
		{ "abc", "中文" },         // Latin then CJK
	};
	engine::TextConfig c;
	c.field = "t";
	c.flags = text::CJK_WORDS;
	for (const auto& [left, right] : pairs) {
		const auto spaced = left + " " + right;
		engine::Document expected;
		GenerateTerms::text(expected, "t", spaced, true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none, true);
		std::string expected_query;
		engine::parse_text(spaced, c, &expected_query);
		for (const auto& infix : infixes) {
			const auto joined = left + infix + right;
			engine::Document doc;
			GenerateTerms::text(doc, "t", joined, true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none, true);
			CHECK_EQ(doc.serialize(), expected.serialize());
			std::string query;
			engine::parse_text(joined, c, &query);
			CHECK_EQ(query, expected_query);
		}
	}
	// The katakana middle dot (U+30FB) is not an infix and splits as before.
	CHECK_EQ(text::cjk_words("ジョン・スミス"), text::cjk_words("ジョン") + "・" + text::cjk_words("スミス"));
	engine::Document dot, space;
	GenerateTerms::text(dot, "t", "ジョン・スミス", true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none, true);
	GenerateTerms::text(space, "t", "ジョン スミス", true, 1, false, "", "", analysis::stop_strategy::none, analysis::stem_strategy::none, true);
	CHECK_EQ(dot.serialize(), space.serialize());
	// Between Latin letters an infix stays where it is and still joins.
	for (const char* latin : { "AT&T", "O'Neil", "O’Neil", "col·lecció", "AT&T 中文", "rock'n'roll" }) {
		CHECK_EQ(text::cjk_words(latin), std::string(latin));
	}
}

// The schema refuses _cjk_words with _cjk_ngram, and keeps the stored property.
void
test_schema_property()
{
	{
		Schema schema(Schema::get_initial_schema(), nullptr, "");
		bool refused = false;
		try {
			schema.write(MsgPack({{"body", MsgPack({{"_type", "text"}, {"_cjk_words", true}, {"_cjk_ngram", true}})}}));
		} catch (const ClientError&) {
			refused = true;
		}
		CHECK(refused);
	}
	Schema written(Schema::get_initial_schema(), nullptr, "");
	written.write(MsgPack({{"body", MsgPack({{"_type", "text"}, {"_cjk_words", true}})}}));
	CHECK(written.get_schema().at("body").at("_cjk_words").boolean());
	CHECK_EQ(written.get_schema().at("body").at("_cjk_ngram").boolean(), false);
	// An update of the stored schema (write replaces it whole) refuses to change the property.
	Schema stored(written.get_modified_schema(), nullptr, "");
	std::string refusal;
	try {
		stored.update(MsgPack({{"body", MsgPack({{"_cjk_words", false}})}}));
	} catch (const ClientError& exc) {
		refusal = exc.what();
	}
	CHECK(refusal.find("_cjk_words") != std::string::npos);
	CHECK(stored.get_schema().at("body").at("_cjk_words").boolean());
}

}  // namespace

TEST_MAIN(test_words_are_the_segmenters, test_indexing_splits_words, test_queries_split_words, test_infix_splits_cjk_words, test_schema_property)
