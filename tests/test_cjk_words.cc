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
#include <vector>

#include "analysis.hh"
#include "engine/document.h"
#include "engine/text.h"
#include "multivalue/generate_terms.h"

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

}  // namespace

TEST_MAIN(test_words_are_the_segmenters, test_indexing_splits_words, test_queries_split_words)
