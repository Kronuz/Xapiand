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

// The phonetic sort metrics Xapiand serialises between nodes: a comparand that arrives as a
// view (a stored value) is encoded by each of the four language encoders, and a metric that
// is serialised and read back scores the same.

#include "test_harness.h"

#include <string>
#include <string_view>

#include "english_soundex.h"
#include "french_soundex.h"
#include "german_soundex.h"
#include "spanish_soundex.h"
#include "metric_serialise.h"

template <typename Encoder>
static void check_phonetic() {
	using Metric = SerialisablePhoneticMetric<Encoder, LCSubsequence>;
	Metric metric("Robert");
	CHECK_EQ(metric.distance(std::string_view("Robert")), 0.0);
	CHECK(metric.distance(std::string_view("Xylophone")) > 0.0);

	const std::string bytes = metric.serialise();
	Metric copy;
	const char* p = bytes.data();
	copy.unserialise(&p, bytes.data() + bytes.size());
	CHECK(p == bytes.data() + bytes.size());
	CHECK_EQ(copy.distance(std::string_view("Robert")), 0.0);
	CHECK_EQ(copy.distance(std::string_view("Xylophone")), metric.distance(std::string_view("Xylophone")));
}

static void test_phonetic_english() { check_phonetic<SoundexEnglish>(); }
static void test_phonetic_french() { check_phonetic<SoundexFrench>(); }
static void test_phonetic_german() { check_phonetic<SoundexGerman>(); }
static void test_phonetic_spanish() { check_phonetic<SoundexSpanish>(); }

TEST_MAIN(test_phonetic_english, test_phonetic_french, test_phonetic_german, test_phonetic_spanish)
