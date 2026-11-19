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

#pragma once

#include <stdexcept>               // for std::invalid_argument, std::runtime_error
#include <string>                  // for std::string
#include <string_view>             // for std::string_view

#include "analysis.hh"             // for analysis::stopper


// No list has the language's name: not shipped, and no <data path>/stopwords/<language>.txt.
class UnknownStopWordsError : public std::invalid_argument {
public:
	explicit UnknownStopWordsError(std::string_view language);
};

// An index was built with a list that is now missing or has other words.
class StopWordsMismatchError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

// Reads every <data path>/stopwords/<language>.txt, each one replacing the shipped list of its language (or
// adding a list of its own name). The server calls it at startup, before its listeners open, so that no
// request reads a file (docs/design/phase7c.md D1); a process that does not (a test) reads them at the first
// lookup. The shipped lists are compiled in: with no such directory nothing is read.
void preloadStopWords();

// The stop words of a language. Throws UnknownStopWordsError when there is no such list: a missing list never
// means "stop nothing".
const analysis::stopper* getStopWords(std::string_view language);

// The fingerprint of the language's list: 16 hex digits of the hash of its sorted words (stopwords/builtin.h).
// An index records it for every list it stops with. Throws UnknownStopWordsError.
std::string stopWordsFingerprint(std::string_view language);

// Refuses to go on with `index` when the list `recorded` was taken from is missing or has other words now:
// throws StopWordsMismatchError naming the index, the list and both fingerprints.
void checkStopWords(std::string_view index, std::string_view language, std::string_view recorded);
