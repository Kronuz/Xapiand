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

#pragma once

#include <cmath>             // for std::fmod

#include "ansi_color.hh"
#include "string.hh"


inline void
hsv2rgb(
	// input:
	double hue,        // angle in degrees between 0 and 360
	double saturation, // a fraction between 0 and 1
	double value,      // a fraction between 0 and 1
	// output:
	double& red,       // a fraction between 0 and 1
	double& green,     // a fraction between 0 and 1
	double& blue       // a fraction between 0 and 1
) {
	if (saturation <= 0.0) {
		red = value;
		green = value;
		blue = value;
		return;
	}

	if (hue >= 360.0) {
		hue = std::fmod(hue, 360.0);
	}

	hue /= 60.0;
	auto i = static_cast<long>(hue);
	auto f = hue - i;
	auto p = value * (1.0 - saturation);
	auto q = value * (1.0 - (saturation * f));
	auto t = value * (1.0 - (saturation * (1.0 - f)));

	switch (i) {
		case 0:
			red = value;
			green = t;
			blue = p;
			break;
		case 1:
			red = q;
			green = value;
			blue = p;
			break;
		case 2:
			red = p;
			green = value;
			blue = t;
			break;
		case 3:
			red = p;
			green = q;
			blue = value;
			break;
		case 4:
			red = t;
			green = p;
			blue = value;
			break;
		case 5:
		default:
			red = value;
			green = p;
			blue = q;
			break;
	}
	return;
}


class color {
	// non-constexpr version of ansi_color

	uint8_t r;
	uint8_t g;
	uint8_t b;

	auto trueColor(bool bold) {
		auto trueColor = string::format(ESC "[{};38;2;{};{};{}m", bold ? 1 : 0, r, g, b);
		return trueColor;
	}

	auto standard256(bool bold) {
		uint8_t color = static_cast<uint8_t>((r == g && g == b) ? (
			r < 6 ? 16 :
			r > 249 ? 231 :
			231 + static_cast<int>((r * 25.0f / 255.0f) + 0.5f)
		) : (
			16 +
			(static_cast<int>(r / 255.0f * 5.0f + 0.5f) * 36) +
			(static_cast<int>(g / 255.0f * 5.0f + 0.5f) * 6) +
			(static_cast<int>(b / 255.0f * 5.0f + 0.5f))
		));
		auto standard256 = string::format(ESC "[{};38;5;{}m", bold ? 1 : 0, color);
		return standard256;
	}

	auto standard16(bool bold) {
		auto _min = r < g ? r : g;
		auto min = _min < b ? _min : b;
		auto _max = r > g ? r : g;
		auto max = _max > b ? _max : b;
		uint8_t color = static_cast<uint8_t>((r == g && g == b) ? (
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
		auto standard16 = string::format(ESC "[{};38;5;{}m", bold ? 1 : 0, color);
		return standard16;
	}

public:
	color(uint8_t red, uint8_t green, uint8_t blue) :
		r(red), g(green), b(blue) {}

	auto red() {
		return r;
	}

	auto green() {
		return g;
	}

	auto blue() {
		return b;
	}

	auto ansi(bool bold = false) {
		auto ansi = (
			trueColor(bold) +
			standard256(bold) +
			standard16(bold)
		);
		return ansi;
	}
};
