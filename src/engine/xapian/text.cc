/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "engine/xapian/text.h"

#include "config.h"                 // for USE_ICU

#ifdef USE_ICU

#include "engine/xapian/convert.h"         // for from_xapian
#include "engine/xapian/snowball_stem.h"   // for makeStem
#include "engine/xapian/stopper.h"         // for getStopper
#include "engine/xapian/xapian.h"          // for Xapian::TermGenerator


namespace engine::xapian {

namespace {

Xapian::TermGenerator::stop_strategy
xapian_stop_strategy(analysis::stop_strategy strategy)
{
	switch (strategy) {
		case analysis::stop_strategy::none: return Xapian::TermGenerator::STOP_NONE;
		case analysis::stop_strategy::all: return Xapian::TermGenerator::STOP_ALL;
		case analysis::stop_strategy::stemmed: return Xapian::TermGenerator::STOP_STEMMED;
	}
	return Xapian::TermGenerator::STOP_STEMMED;
}


Xapian::TermGenerator::stem_strategy
xapian_stem_strategy(analysis::stem_strategy strategy)
{
	switch (strategy) {
		case analysis::stem_strategy::none: return Xapian::TermGenerator::STEM_NONE;
		case analysis::stem_strategy::some: return Xapian::TermGenerator::STEM_SOME;
		case analysis::stem_strategy::all: return Xapian::TermGenerator::STEM_ALL;
		case analysis::stem_strategy::all_z: return Xapian::TermGenerator::STEM_ALL_Z;
	}
	return Xapian::TermGenerator::STEM_SOME;
}

}  // namespace


void
index_text_word_breaks(engine::Document& doc,
	const std::string& prefix, const std::string& value,
	bool positions, engine::termcount weight, bool cjk_ngram,
	const std::string& language, const std::string& stem_language,
	analysis::stop_strategy stop_strategy, analysis::stem_strategy stem_strategy)
{
	Xapian::Document xdoc;
	Xapian::TermGenerator term_generator;
	Xapian::TermGenerator::flags flags = Xapian::TermGenerator::FLAG_WORD_BREAKS;
	if (cjk_ngram) {
		flags |= Xapian::TermGenerator::FLAG_CJK_NGRAM;
	}
	term_generator.set_flags(flags);
	term_generator.set_document(xdoc);
	if (!language.empty()) {
		term_generator.set_stopper(getStopper(language).get());
		term_generator.set_stopper_strategy(xapian_stop_strategy(stop_strategy));
	}
	if (!stem_language.empty()) {
		term_generator.set_stemmer(makeStem(stem_language));
		term_generator.set_stemming_strategy(xapian_stem_strategy(stem_strategy));
	}
	if (positions) {
		term_generator.index_text(value, weight, prefix);
	} else {
		term_generator.index_text_without_positions(value, weight, prefix);
	}
	const auto terms_end = xdoc.termlist_end();
	for (auto it = xdoc.termlist_begin(); it != terms_end; ++it) {
		const auto name = *it;
		doc.add_term(name, it.get_wdf());
		const auto positions_end = it.positionlist_end();
		for (auto pit = it.positionlist_begin(); pit != positions_end; ++pit) {
			doc.add_posting(name, *pit, 0);
		}
	}
}

}  // namespace engine::xapian

#endif  // USE_ICU
