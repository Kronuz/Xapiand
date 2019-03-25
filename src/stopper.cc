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
#include <mutex>                                  // for std::mutex, std::lock_guard
#include <fstream>                                // for std::ifstream

#include "config.h"                               // for STOPWORDS_PATH
#include "log.h"                                  // for L_WARNING_ONCE


const std::unique_ptr<Xapian::Stopper>&
getStopper(std::string_view language)
{
	static std::mutex mtx;
	static std::string stopwords_path(getenv("XAPIAN_STOPWORDS_PATH") != nullptr ? getenv("XAPIAN_STOPWORDS_PATH") : STOPWORDS_PATH);
	static std::unordered_map<uint32_t, std::unique_ptr<Xapian::Stopper>> stoppers;
	auto language_hash = hh(language);
	std::lock_guard<std::mutex> lk(mtx);
	auto it = stoppers.find(language_hash);
	if (it != stoppers.end()) {
		return it->second;
	}
	auto& stopper = stoppers[language_hash];
	auto path = stopwords_path + "/" + std::string(language) + ".txt";
	std::ifstream words(path);
	if (words.is_open()) {
		stopper = std::make_unique<SimpleStopper<>>(std::istream_iterator<std::string>(words), std::istream_iterator<std::string>());
	} else {
		L_WARNING_ONCE("Cannot open stop words file: {}", path);
	}
	return stopper;
}
