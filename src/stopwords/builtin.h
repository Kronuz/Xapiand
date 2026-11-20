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

#pragma once

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <string_view>             // for std::string_view


// The shipped stop-word lists, compiled in. A list is a blob: its words sorted by bytes without repeats, each
// followed by a NUL. Its fingerprint is the FNV-1a 64-bit hash of the blob, so a list read from a file hashes
// the same when it holds the same words.
namespace stopwords {

constexpr std::uint64_t fnv_offset = 14695981039346656037ull;
constexpr std::uint64_t fnv_prime = 1099511628211ull;

constexpr std::uint64_t
fnv1a(std::string_view bytes, std::uint64_t hash = fnv_offset) noexcept
{
	for (const char byte : bytes) {
		hash = (hash ^ static_cast<unsigned char>(byte)) * fnv_prime;
	}
	return hash;
}

struct Builtin {
	std::string_view language;
	std::string_view blob;
	std::size_t count;
	std::uint64_t fingerprint;
};

template <std::size_t N>
constexpr Builtin
make_builtin(std::string_view language, const char (&words)[N]) noexcept
{
	const std::string_view blob(words, N - 1);
	std::size_t count = 0;
	for (const char byte : blob) {
		count += byte == '\0';
	}
	return {language, blob, count, fnv1a(blob)};
}

#include "stopwords/builtin.inc"

constexpr bool
sorted() noexcept
{
	for (std::size_t i = 1; i < sizeof(builtin) / sizeof(builtin[0]); ++i) {
		if (!(builtin[i - 1].language < builtin[i].language)) {
			return false;
		}
	}
	return true;
}
static_assert(sorted(), "stopwords::builtin is searched by language");

}  // namespace stopwords
