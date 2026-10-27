/*
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

#pragma once

#include <string>                  // for std::string

#include "config.h"               // for USE_ICU
#include "analysis.hh"             // for analysis::stop_strategy, stem_strategy
#include "engine/document.h"       // for engine::Document
#include "engine/types.h"          // for engine::termcount


#ifdef USE_ICU

namespace engine::xapian {

// Word-break segmentation (cjk_words) is Xapian's only; analysis has no equivalent. Indexes
// `value` with Xapian's term generator and adds its terms and postings to `doc`.
void index_text_word_breaks(engine::Document& doc,
	const std::string& prefix, const std::string& value,
	bool positions, engine::termcount weight, bool cjk_ngram,
	const std::string& language, const std::string& stem_language,
	analysis::stop_strategy stop_strategy, analysis::stem_strategy stem_strategy);

}  // namespace engine::xapian

#endif  // USE_ICU
