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

#include "stopper.h"

#include <cstdlib>                                // for getenv
#include <fstream>                                // for std::ifstream
#include <iterator>                               // for std::istream_iterator
#include <mutex>                                  // for std::mutex, std::lock_guard
#include <string>                                 // for std::string
#include <unordered_map>                          // for std::unordered_map

#include "config.h"                               // for XAPIAND_DATA_PATH
#include "log.h"                                  // for L_WARNING_ONCE


namespace {

class WordsStopper : public Xapian::Stopper {
	const analysis::stopper& words;

public:
	explicit WordsStopper(const analysis::stopper& words_) : words(words_) { }

	bool operator()(const std::string& term) const override {
		return words(term);
	}
};

struct StopWords {
	bool loaded = false;
	analysis::stopper words;
	std::unique_ptr<Xapian::Stopper> stopper;
};

static StopWords&
getStopWordsEntry(std::string_view language)
{
	static std::mutex mtx;
	static std::string data_path(getenv("XAPIAND_DATA_PATH") != nullptr ? getenv("XAPIAND_DATA_PATH") : XAPIAND_DATA_PATH);
	static std::unordered_map<std::string, StopWords> entries;
	std::lock_guard<std::mutex> lk(mtx);
	auto it = entries.find(std::string(language));
	if (it != entries.end()) {
		return it->second;
	}
	auto& entry = entries[std::string(language)];
	auto path = data_path + "/stopwords/" + std::string(language) + ".txt";
	std::ifstream file(path);
	if (file.is_open()) {
		for (std::istream_iterator<std::string> word(file), end; word != end; ++word) {
			entry.words.add(*word);
		}
		entry.stopper = std::make_unique<WordsStopper>(entry.words);
		entry.loaded = true;
	} else {
		L_WARNING_ONCE("Cannot open stop words file: {}", path);
	}
	return entry;
}

}  // namespace


const analysis::stopper*
getStopWords(std::string_view language)
{
	auto& entry = getStopWordsEntry(language);
	return entry.loaded ? &entry.words : nullptr;
}


const std::unique_ptr<Xapian::Stopper>&
getStopper(std::string_view language)
{
	return getStopWordsEntry(language).stopper;
}
