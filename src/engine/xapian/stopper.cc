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

#include "engine/xapian/stopper.h"

#include <mutex>                   // for std::mutex, std::lock_guard
#include <string>                  // for std::string
#include <unordered_map>           // for std::unordered_map

#include <stopper.h>               // for getStopWords (src/stopper.h, not this directory's)


namespace {

class WordsStopper : public Xapian::Stopper {
	const analysis::stopper& words;

public:
	explicit WordsStopper(const analysis::stopper& words_) : words(words_) { }

	bool operator()(const std::string& term) const override {
		return words(term);
	}
};

}  // namespace


const std::unique_ptr<Xapian::Stopper>&
getStopper(std::string_view language)
{
	static std::mutex mtx;
	static std::unordered_map<std::string, std::unique_ptr<Xapian::Stopper>> stoppers;
	std::lock_guard<std::mutex> lk(mtx);
	auto& stopper = stoppers[std::string(language)];
	if (!stopper) {
		if (const auto* words = getStopWords(language)) {
			stopper = std::make_unique<WordsStopper>(*words);
		}
	}
	return stopper;
}
