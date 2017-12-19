/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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
template <uint8_t red, uint8_t green, uint8_t blue, bool bold = false>
class ansi_color {
  static constexpr auto esc = static_str::literal("\033[");

  static constexpr auto trueColor() {
    return (
      esc +
      to_string<bold>::value +
      ";38;2;" +
      to_string<red>::value + ";" +
      to_string<green>::value + ";" +
      to_string<blue>::value +
      "m"
    );
  }

  static constexpr auto standard256() {
    constexpr uint8_t color = (red == green && green == blue) ? (
      red <= 8 ? 16 :
      red >= 238 ? 231 :
      (232 + static_cast<int>(((red - 8) * 23.0f / 230.0f) + 0.5f))
    ) : (
      16 +
      (static_cast<int>(red / 255.0f * 5.0f + 0.5f) * 36) +
      (static_cast<int>(green / 255.0f * 5.0f + 0.5f) * 6) +
      (static_cast<int>(blue / 255.0f * 5.0f + 0.5f))
    );
    return (
      esc +
      to_string<bold>::value +
      ";38;5;" +
      to_string<color>::value +
      "m"
    );
  }

  static constexpr auto standard16() {
    constexpr auto _min = red < green ? red : green;
    constexpr auto min = _min < blue ? _min : blue;
    constexpr auto _max = red > green ? red : green;
    constexpr auto max = _max > blue ? _max : blue;
    constexpr uint8_t color = (red == green && green == blue) ? (
      red > 192 ? 15 :
      red > 128 ? 7 :
      red > 32 ? 8 :
      0
    ) : (
      (max <= 32) ? (
        0
      ) : (
        (
          ((static_cast<int>((blue - min) * 255.0f / (max - min) + 0.5f) > 128 ? 1 : 0) << 2) |
          ((static_cast<int>((green - min) * 255.0f / (max - min) + 0.5f) > 128 ? 1 : 0) << 1) |
          ((static_cast<int>((red - min) * 255.0f / (max - min) + 0.5f) > 128 ? 1 : 0))
        ) + (max > 192 ? 8 : 0)
      )
    );
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
        constexpr const auto trueColor = ansi_color<red, green, blue, bold>::trueColor();
        return std::string(trueColor);
      }
      case Coloring::Palette:
      case Coloring::Standard256: {
        constexpr const auto standard256 = ansi_color<red, green, blue, bold>::standard256();
        return std::string(standard256);
      }
      case Coloring::Standard16: {
        constexpr const auto standard16 = ansi_color<red, green, blue, bold>::standard16();
        return std::string(standard16);
      }
      case Coloring::None: {
        return "";
      }
    };
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
};

#define rgb(r, g, b)      ansi_color<static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), false>::col()
#define rgba(r, g, b, a)  ansi_color<static_cast<uint8_t>(r * a + 0.5f), static_cast<uint8_t>(g * a + 0.5f), static_cast<uint8_t>(b * a + 0.5f), false>::col()
#define brgb(r, g, b)     ansi_color<static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), true>::col()
#define brgba(r, g, b, a) ansi_color<static_cast<uint8_t>(r * a + 0.5f), static_cast<uint8_t>(g * a + 0.5f), static_cast<uint8_t>(b * a + 0.5f), true>::col()
