/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors
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


template <int N, int rem, char... a>
struct explode : explode<N + 1, rem / 10, ('0' + rem % 10), a...> { };

template <int N, char... a>
struct explode<N, 0, a...> {
	constexpr static const char value[N + 1]{a..., 0};
};

template <unsigned num>
struct to_string : explode<0, num> { };

template <>
struct to_string<0> : explode<1, 0, '0'> { };


enum class Coloring : uint8_t {
	TrueColor,
	Palette,
	Standard256,
	Standard16,
	None,
};


// Ansi colors:
template <int red, int green, int blue, bool bold = false>
class ansi_color {
	static constexpr uint8_t r = red < 0 ? 0 : red > 255 ? 255 : red;
	static constexpr uint8_t g = green < 0 ? 0 : green > 255 ? 255 : green;
	static constexpr uint8_t b = blue < 0 ? 0 : blue > 255 ? 255 : blue;
	static constexpr auto esc = static_str::literal("\033[");

	static constexpr auto noColor() {
		return esc + "0m";
	}

	static constexpr auto trueColor() {
		return (
			esc +
			to_string<bold>::value +
			";38;2;" +
			to_string<r>::value + ";" +
			to_string<g>::value + ";" +
			to_string<b>::value +
			"m"
		);
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
		return (
			esc +
			to_string<bold>::value +
			";38;5;" +
			to_string<color>::value +
			"m"
		);
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
		return (
			esc +
			to_string<bold>::value +
			";38;5;" +
			to_string<color>::value +
			"m"
		);
	}

	static Coloring _detectColoring() {
		std::string colorterm;
		const char *env_colorterm = getenv("COLORTERM");
		if (env_colorterm) {
			colorterm = env_colorterm;
		}
		std::string term;
		const char* env_term = getenv("TERM");
		if (env_term) {
			term = env_term;
		}
		if (colorterm.find("truecolor") != std::string::npos || term.find("24bit") != std::string::npos) {
			return Coloring::TrueColor;
		} else if (term.find("256color") != std::string::npos) {
			return Coloring::Standard256;
		} else if (term.find("ansi") != std::string::npos || term.find("16color") != std::string::npos) {
			return Coloring::Standard16;
		} else {
			return Coloring::Standard16;
		}
	}

	static const std::string _col() {
		switch (ansi_color<0, 0, 0>::detectColoring()) {
			case Coloring::TrueColor: {
				constexpr const auto _ = trueColor();
				return _;
			}
			case Coloring::Palette:
			case Coloring::Standard256: {
				constexpr const auto _ = standard256();
				return _;
			}
			case Coloring::Standard16: {
				constexpr const auto _ = standard16();
				return _;
			}
			case Coloring::None: {
				return "";
			}
		};
	}

	static const std::string _no_col() {
		constexpr const auto _ = noColor();
		return _;
	}

public:
	static Coloring detectColoring() {
		static Coloring coloring = _detectColoring();
		return coloring;
	}

	static const std::string& col() {
		static auto col = _col();
		return col;
	}

	static const std::string& no_col() {
		static auto no_col = _no_col();
		return no_col;
	}
};

#define rgb(r, g, b)      ansi_color<static_cast<int>(r), static_cast<int>(g), static_cast<int>(b)>::col()
#define rgba(r, g, b, a)  ansi_color<static_cast<int>(r * a + 0.5f), static_cast<int>(g * a + 0.5f), static_cast<int>(b * a + 0.5f)>::col()
#define brgb(r, g, b)     ansi_color<static_cast<int>(r), static_cast<int>(g), static_cast<int>(b), true>::col()
#define brgba(r, g, b, a) ansi_color<static_cast<int>(r * a + 0.5f), static_cast<int>(g * a + 0.5f), static_cast<int>(b * a + 0.5f), true>::col()
#define no_col()          ansi_color<0, 0, 0>::no_col()
