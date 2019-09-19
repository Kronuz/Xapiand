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

#include "strings.hh"

#include <cassert>            // for assert
#include <cmath>              // for std::log, std::pow
#include <vector>             // for std::vector

#include "colors.h"


class Humanize {
	long double base;
	long double div;
	std::vector<long double> scaling;
	std::vector<std::string_view> units;
	std::vector<std::string_view> colors;
	size_t needle;

public:
	Humanize(
		long double base_,
		std::vector<long double>&& scaling_,
		std::vector<std::string_view>&& units_,
		std::vector<std::string_view>&& colors_
	) :
		base(base_),
		div(std::log(base)),
		scaling(std::move(scaling_)),
		units(std::move(units_)),
		colors(std::move(colors_)),
		needle(std::distance(scaling.begin(), std::find(scaling.begin(), scaling.end(), 0)))
	{
		assert(base > 0);
		assert(units.size() > 0);
		assert(scaling.size() == units.size());
		assert(colors.size() == units.size() + 1);
		assert(needle >= 0);
		assert(needle < units.size());
		std::transform(scaling.begin(), scaling.end(), scaling.begin(), [&](long double s) {
			return std::pow(base, s);
		});
	}

	std::string operator()(long double delta, const char* prefix, bool colored, long double rounding) const {
		long double num = delta;

		if (delta < 0) {
			delta = -delta;
		}

		ssize_t last = units.size();
		ssize_t order = (delta == 0) ? last - 1 : -std::floor(std::log(delta) / div);
		order += needle;
		if (order >= last) {
			order = last - 1;
		}
		if (order < 0) {
			order = 0;
		}

		num = std::round(rounding * num / scaling[order]) / rounding;
		auto& unit = units[order];

		if (colored) {
			auto& color = colors[order];
			auto& reset = colors[last];
			return strings::format("{}{}{}{}{}", color, prefix, static_cast<double>(num), unit, reset);
		}

		return strings::format("{}{}{}", prefix, static_cast<double>(num), unit);
	}
};


// MEDIUM_SEA_GREEN  -> rgb(60, 179, 113)
// MEDIUM_SEA_GREEN  -> rgb(60, 179, 113)
// SEA_GREEN         -> rgb(46, 139, 87);
// OLIVE_DRAB        -> rgb(107, 142, 35)
// OLIVE             -> rgb(128, 128, 0)
// DARK_GOLDEN_ROD   -> rgb(184, 134, 11);
// PERU              -> rgb(205, 133, 63);
// SADDLE_BROWN      -> rgb(139, 69, 19);
// BROWN             -> rgb(165, 42, 42);


static inline std::string
_from_bytes(size_t bytes, const char* prefix, bool colored)
{
	static const Humanize humanize(
		1024,
		{ 8, 7, 6, 5, 4, 3, 2, 1, 0 },
		{ "YiB", "ZiB", "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" },
		{ BROWN, BROWN, BROWN, BROWN, BROWN, PERU, OLIVE, SEA_GREEN, MEDIUM_SEA_GREEN, CLEAR_COLOR }
	);
	return humanize(bytes, prefix, colored, 10.0L);
}


std::string
strings::from_bytes(size_t bytes, const char* prefix, bool colored)
{
	return _from_bytes(bytes, prefix, colored);
}


static inline std::string
_from_small_time(long double seconds, const char* prefix, bool colored)
{
	static const Humanize humanize(
		1000,
		{ 0, -1, -2, -3, -4 },
		{ "s", "ms", R"(µs)", "ns", "ps" },
		{ OLIVE, OLIVE_DRAB, SEA_GREEN, MEDIUM_SEA_GREEN, MEDIUM_SEA_GREEN, CLEAR_COLOR }
	);
	return humanize(seconds, prefix, colored, 1000.0L);
}


std::string
strings::from_small_time(long double seconds, const char* prefix, bool colored)
{
	return _from_small_time(seconds, prefix, colored);
}


static inline std::string
_from_time(long double seconds, const char* prefix, bool colored)
{
	static const Humanize humanize(
		60,
		{ 2, 1, 0 },
		{ "hrs", "min", "s" },
		{ SADDLE_BROWN, PERU, DARK_GOLDEN_ROD, CLEAR_COLOR }
	);
	return humanize(seconds, prefix, colored, 100.0L);
}


std::string
strings::from_time(long double seconds, const char* prefix, bool colored)
{
	return _from_time(seconds, prefix, colored);
}


static inline std::string
_from_delta(long double nanoseconds, const char* prefix, bool colored)
{
	long double seconds = nanoseconds / 1e9;  // convert nanoseconds to seconds (as a double)
	return (seconds < 1) ? _from_small_time(seconds, prefix, colored) : _from_time(seconds, prefix, colored);
}


std::string
strings::from_delta(long double nanoseconds, const char* prefix, bool colored)
{
	return _from_delta(nanoseconds, prefix, colored);
}
