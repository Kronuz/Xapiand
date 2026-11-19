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

#include <algorithm>                              // for std::lower_bound, std::sort, std::unique
#include <array>                                  // for std::array
#include <cstdio>                                 // for std::snprintf
#include <cstdlib>                                // for getenv
#include <cstring>                                // for std::strlen
#include <filesystem>                             // for std::filesystem
#include <fstream>                                // for std::ifstream
#include <iterator>                               // for std::istream_iterator, std::size
#include <map>                                    // for std::map
#include <mutex>                                  // for std::call_once
#include <string>                                 // for std::string
#include <system_error>                           // for std::error_code
#include <vector>                                 // for std::vector

#include "config.h"                               // for XAPIAND_DATA_PATH
#include "stopwords/builtin.h"                    // for stopwords::builtin


namespace {

struct List {
	analysis::stopper words;
	std::string fingerprint;
};

std::string
hex(std::uint64_t hash)
{
	char text[17];
	std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
	return text;
}

using Overrides = std::map<std::string, List, std::less<>>;

// Every <data path>/stopwords/<language>.txt, read once. The table never changes after it is built, so a
// lookup is a read of an immutable map. A file that cannot be read throws: the shipped list must not stand in
// for a list the operator wrote.
const Overrides&
overrides()
{
	static const Overrides lists = [] {
		Overrides loaded;
		const std::string data_path(getenv("XAPIAND_DATA_PATH") != nullptr ? getenv("XAPIAND_DATA_PATH") : XAPIAND_DATA_PATH);
		const auto directory = std::filesystem::path(data_path) / "stopwords";
		std::error_code ec;
		for (std::filesystem::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
			if (it->path().extension() != ".txt") {
				continue;
			}
			std::error_code status_ec;
			std::ifstream file;
			if (std::filesystem::is_regular_file(it->path(), status_ec)) {
				file.open(it->path());
			}
			if (!file.is_open()) {
				throw std::runtime_error("Cannot open stop words file: " + it->path().string());
			}
			std::vector<std::string> words((std::istream_iterator<std::string>(file)), std::istream_iterator<std::string>());
			if (!words.empty() && words.front().starts_with("\xEF\xBB\xBF")) {
				words.front().erase(0, 3);   // a UTF-8 byte order mark is not part of the first word
				if (words.front().empty()) {
					words.erase(words.begin());
				}
			}
			std::sort(words.begin(), words.end());
			words.erase(std::unique(words.begin(), words.end()), words.end());
			auto& list = loaded[it->path().stem().string()];
			auto hash = stopwords::fnv_offset;
			for (const auto& word : words) {
				list.words.add(word);
				hash = stopwords::fnv1a(std::string_view("\0", 1), stopwords::fnv1a(word, hash));
			}
			list.fingerprint = hex(hash);
		}
		return loaded;
	}();
	return lists;
}

// The shipped list of a language, as an analysis::stopper built the first time it is asked for.
const List*
builtin(std::string_view language)
{
	static const auto* const begin = std::begin(stopwords::builtin);
	static const auto* const end = std::end(stopwords::builtin);
	static std::array<std::once_flag, std::size(stopwords::builtin)> once;
	static std::array<List, std::size(stopwords::builtin)> lists;
	const auto* it = std::lower_bound(begin, end, language, [](const stopwords::Builtin& entry, std::string_view name) {
		return entry.language < name;
	});
	if (it == end || it->language != language) {
		return nullptr;
	}
	const auto index = static_cast<std::size_t>(it - begin);
	std::call_once(once[index], [&] {
		auto& list = lists[index];
		for (const char* word = it->blob.data(); word != it->blob.data() + it->blob.size(); word += std::strlen(word) + 1) {
			list.words.add(word);
		}
		list.fingerprint = hex(it->fingerprint);
	});
	return &lists[index];
}

const List*
find(std::string_view language)
{
	const auto& files = overrides();
	if (auto it = files.find(language); it != files.end()) {
		return &it->second;
	}
	return builtin(language);
}

const List&
get(std::string_view language)
{
	const auto* list = find(language);
	if (list == nullptr) {
		throw UnknownStopWordsError(language);
	}
	return *list;
}

}  // namespace


UnknownStopWordsError::UnknownStopWordsError(std::string_view language)
	: std::invalid_argument("Stop words list " + std::string(language) + " unknown")
{
}


void
preloadStopWords()
{
	(void)overrides();
}


const analysis::stopper*
getStopWords(std::string_view language)
{
	return &get(language).words;
}


std::string
stopWordsFingerprint(std::string_view language)
{
	return get(language).fingerprint;
}


void
checkStopWords(std::string_view index, std::string_view language, std::string_view recorded)
{
	const auto* list = find(language);
	if (list == nullptr) {
		throw StopWordsMismatchError("Index " + std::string(index) + " was built with stop words list " + std::string(language) +
			" (fingerprint " + std::string(recorded) + "), which is missing now");
	}
	if (list->fingerprint != recorded) {
		throw StopWordsMismatchError("Index " + std::string(index) + " was built with stop words list " + std::string(language) +
			" (fingerprint " + std::string(recorded) + "), which has fingerprint " + list->fingerprint + " now");
	}
}
