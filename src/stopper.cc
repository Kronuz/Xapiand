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

#include "stopper.h"

#include <cstdlib>                                // for getenv
#include <filesystem>                             // for std::filesystem
#include <fstream>                                // for std::ifstream
#include <iterator>                               // for std::istream_iterator
#include <string>                                 // for std::string
#include <system_error>                           // for std::error_code
#include <unordered_map>                          // for std::unordered_map

#include "config.h"                               // for XAPIAND_DATA_PATH
#include "log.h"                                  // for L_WARNING_ONCE


namespace {

using StopWordLists = std::unordered_map<std::string, analysis::stopper>;

// Every <data path>/stopwords/<language>.txt, read once. The table never changes after it is built, so a
// lookup is a read of an immutable map.
const StopWordLists&
stop_word_lists()
{
	static const StopWordLists lists = [] {
		StopWordLists loaded;
		const std::string data_path(getenv("XAPIAND_DATA_PATH") != nullptr ? getenv("XAPIAND_DATA_PATH") : XAPIAND_DATA_PATH);
		const auto directory = std::filesystem::path(data_path) / "stopwords";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
			if (it->path().extension() != ".txt") {
				continue;
			}
			std::ifstream file(it->path());
			if (!file.is_open()) {
				L_WARNING_ONCE("Cannot open stop words file: {}", it->path().string());
				continue;
			}
			auto& words = loaded[it->path().stem().string()];
			for (std::istream_iterator<std::string> word(file), last; word != last; ++word) {
				words.add(*word);
			}
		}
		if (loaded.empty()) {
			L_WARNING_ONCE("No stop words found in {}", directory.string());
		}
		return loaded;
	}();
	return lists;
}

}  // namespace


void
preloadStopWords()
{
	(void)stop_word_lists();
}


const analysis::stopper*
getStopWords(std::string_view language)
{
	const auto& lists = stop_word_lists();
	auto it = lists.find(std::string(language));
	return it != lists.end() ? &it->second : nullptr;
}
