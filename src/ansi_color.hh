/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <string>             // for string
#include <stdlib.h>           // for getenv

#include "static_str.hh"

#define ESC "\033"

template <int N, int num, char... a>
struct explode : explode<N + 1, num / 10, ('0' + num % 10), a...> { };

template <int N, char... a>
struct explode<N, 0, a...> {
	constexpr static const char value[N + 1]{a..., 0};
};

template <unsigned num>
struct to_string : explode<0, num> { };

template <>
struct to_string<0> : explode<1, 0, '0'> { };


// Ansi colors:
template <int red, int green, int blue, bool bold = false>
class ansi_color {
	static constexpr uint8_t r = red < 0 ? 0 : red > 255 ? 255 : red;
	static constexpr uint8_t g = green < 0 ? 0 : green > 255 ? 255 : green;
	static constexpr uint8_t b = blue < 0 ? 0 : blue > 255 ? 255 : blue;

	static constexpr auto noColor() {
		constexpr auto noColor = static_str::literal(ESC "[0m");
		return noColor;
	}

	static constexpr auto trueColor() {
		constexpr auto trueColor = (
			static_str::literal(ESC "[") +
			to_string<bold>::value +
			";38;2;" +
			to_string<r>::value + ";" +
			to_string<g>::value + ";" +
			to_string<b>::value +
			"m"
		);
		return trueColor;
	}

	static constexpr auto standard256() {
		constexpr uint8_t color = static_cast<uint8_t>((r == g && g == b) ? (
			r < 6 ? 16 :
			r > 249 ? 231 :
			231 + static_cast<int>((r * 25.0f / 255.0f) + 0.5f)
		) : (
			16 +
			(static_cast<int>(r / 255.0f * 5.0f + 0.5f) * 36) +
			(static_cast<int>(g / 255.0f * 5.0f + 0.5f) * 6) +
			(static_cast<int>(b / 255.0f * 5.0f + 0.5f))
		));
		constexpr auto standard256 = (
			static_str::literal(ESC "[") +
			to_string<bold>::value +
			";38;5;" +
			to_string<color>::value +
			"m"
		);
		return standard256;
	}

	static constexpr auto standard16() {
		constexpr auto _min = r < g ? r : g;
		constexpr auto min = _min < b ? _min : b;
		constexpr auto _max = r > g ? r : g;
		constexpr auto max = _max > b ? _max : b;
		constexpr uint8_t color = static_cast<uint8_t>((r == g && g == b) ? (
			r > 192 ? 15 :
			r > 128 ? 7 :
			r > 32 ? 8 :
			0
		) : (
			(max <= 32) ? (
				0
			) : (
				(
					((static_cast<int>((b - min) * 255.0f / (max - min) + 0.5f) > 128 ? 1 : 0) << 2) |
					((static_cast<int>((g - min) * 255.0f / (max - min) + 0.5f) > 128 ? 1 : 0) << 1) |
					((static_cast<int>((r - min) * 255.0f / (max - min) + 0.5f) > 128 ? 1 : 0))
				) + (max > 192 ? 8 : 0)
			)
		));
		constexpr auto standard16 = (
			static_str::literal(ESC "[") +
			to_string<bold>::value +
			";38;5;" +
			to_string<color>::value +
			"m"
		);
		return standard16;
	}

public:
	static auto& col() {
		constexpr auto col = (
			trueColor() +
			standard256() +
			standard16()
		);
		static std::string _ = col;
		return _;
	}

	static auto& no_col() {
		constexpr auto no_col = (
			noColor() +
			noColor() +
			noColor()
		);
		static std::string _ = no_col;
		return _;
	}
};


#define rgb(r, g, b)      ansi_color<static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)>::col()
#define rgba(r, g, b, a)  ansi_color<static_cast<int>(r * a + 0.5f), static_cast<int>(g * a + 0.5f), static_cast<int>(b * a + 0.5f)>::col()
#define brgb(r, g, b)     ansi_color<static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), true>::col()
#define brgba(r, g, b, a) ansi_color<static_cast<int>(r * a + 0.5f), static_cast<int>(g * a + 0.5f), static_cast<int>(b * a + 0.5f), true>::col()
#define no_col()          ansi_color<0, 0, 0>::no_col()
