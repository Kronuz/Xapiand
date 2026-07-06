/*
 * Copyright (c) 2015-2026 Dubalu LLC
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

// Unit tests for StringList, the serialised list of strings used by the
// multi-value serialiser. Revived and modernized from the retired oldtests
// serialise_list suite. (CartesianList / RangeList follow the same shape but
// need the geospatial / range types; StringList is the self-contained case.)
//
// Note serialise() takes an input range whose elements are std::string (a single
// element is stored verbatim; two or more get a magic byte + length prefixes),
// while the list itself yields std::string_view over the serialised buffer.

#include "gtest/gtest.h"

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "serialise_list.h"

TEST(SerialiseList, StringListRoundTrip) {
	const std::vector<std::string> values = {
		"alpha", "beta", "gamma delta", "x", "a longer value with spaces"
	};

	const std::string serialised = StringList::serialise(values.begin(), values.end());

	// Construct a StringList over the serialised buffer and iterate it.
	StringList list(serialised);
	ASSERT_EQ(list.size(), values.size());
	size_t i = 0;
	for (const auto& sv : list) {
		ASSERT_LT(i, values.size());
		EXPECT_EQ(sv, values[i]);
		++i;
	}
	EXPECT_EQ(i, values.size());

	// unserialise into a fresh container and compare element by element.
	std::vector<std::string_view> restored;
	StringList::unserialise(serialised, std::back_inserter(restored));
	ASSERT_EQ(restored.size(), values.size());
	for (size_t j = 0; j < restored.size(); ++j) {
		EXPECT_EQ(restored[j], values[j]);
	}
}

TEST(SerialiseList, StringListEmpty) {
	const std::vector<std::string> values;
	const std::string serialised = StringList::serialise(values.begin(), values.end());

	StringList list(serialised);
	EXPECT_EQ(list.size(), 0u);
	EXPECT_TRUE(list.begin() == list.end());

	std::vector<std::string_view> restored;
	StringList::unserialise(serialised, std::back_inserter(restored));
	EXPECT_TRUE(restored.empty());
}

TEST(SerialiseList, StringListSingleWithEmbeddedNul) {
	const std::vector<std::string> values = { std::string("with\0nul and spaces", 19) };

	const std::string serialised = StringList::serialise(values.begin(), values.end());
	StringList list(serialised);
	ASSERT_EQ(list.size(), 1u);
	EXPECT_EQ(*list.begin(), values[0]);      // survives the embedded NUL
	EXPECT_EQ((*list.begin()).size(), 19u);
}
