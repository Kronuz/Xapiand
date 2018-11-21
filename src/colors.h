/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "ansi_color.hh"      // for ansi_color

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvariadic-macros"

static constexpr auto NO_COLOR = no_color();
static constexpr auto CLEAR_COLOR = clear_color();


static constexpr auto ALICE_BLUE = rgb(240, 248, 255);
#define L_UNINDENTED_ALICE_BLUE(args...) L_UNINDENTED(LOG_NOTICE, ALICE_BLUE, args)
#define L_ALICE_BLUE(args...) L(LOG_NOTICE, ALICE_BLUE, args)
#define L_STACKED_ALICE_BLUE(args...) L_STACKED(LOG_NOTICE, ALICE_BLUE, args)

static constexpr auto ANTIQUE_WHITE = rgb(250, 235, 215);
#define L_UNINDENTED_ANTIQUE_WHITE(args...) L_UNINDENTED(LOG_NOTICE, ANTIQUE_WHITE, args)
#define L_ANTIQUE_WHITE(args...) L(LOG_NOTICE, ANTIQUE_WHITE, args)
#define L_STACKED_ANTIQUE_WHITE(args...) L_STACKED(LOG_NOTICE, ANTIQUE_WHITE, args)

static constexpr auto AQUA = rgb(0, 255, 255);
#define L_UNINDENTED_AQUA(args...) L_UNINDENTED(LOG_NOTICE, AQUA, args)
#define L_AQUA(args...) L(LOG_NOTICE, AQUA, args)
#define L_STACKED_AQUA(args...) L_STACKED(LOG_NOTICE, AQUA, args)

static constexpr auto AQUA_MARINE = rgb(127, 255, 212);
#define L_UNINDENTED_AQUA_MARINE(args...) L_UNINDENTED(LOG_NOTICE, AQUA_MARINE, args)
#define L_AQUA_MARINE(args...) L(LOG_NOTICE, AQUA_MARINE, args)
#define L_STACKED_AQUA_MARINE(args...) L_STACKED(LOG_NOTICE, AQUA_MARINE, args)

static constexpr auto AZURE = rgb(240, 255, 255);
#define L_UNINDENTED_AZURE(args...) L_UNINDENTED(LOG_NOTICE, AZURE, args)
#define L_AZURE(args...) L(LOG_NOTICE, AZURE, args)
#define L_STACKED_AZURE(args...) L_STACKED(LOG_NOTICE, AZURE, args)

static constexpr auto BEIGE = rgb(245, 245, 220);
#define L_UNINDENTED_BEIGE(args...) L_UNINDENTED(LOG_NOTICE, BEIGE, args)
#define L_BEIGE(args...) L(LOG_NOTICE, BEIGE, args)
#define L_STACKED_BEIGE(args...) L_STACKED(LOG_NOTICE, BEIGE, args)

static constexpr auto BISQUE = rgb(255, 228, 196);
#define L_UNINDENTED_BISQUE(args...) L_UNINDENTED(LOG_NOTICE, BISQUE, args)
#define L_BISQUE(args...) L(LOG_NOTICE, BISQUE, args)
#define L_STACKED_BISQUE(args...) L_STACKED(LOG_NOTICE, BISQUE, args)

static constexpr auto BLACK = rgb(0, 0, 0);
#define L_UNINDENTED_BLACK(args...) L_UNINDENTED(LOG_NOTICE, BLACK, args)
#define L_BLACK(args...) L(LOG_NOTICE, BLACK, args)
#define L_STACKED_BLACK(args...) L_STACKED(LOG_NOTICE, BLACK, args)

static constexpr auto BLANCHED_ALMOND = rgb(255, 235, 205);
#define L_UNINDENTED_BLANCHED_ALMOND(args...) L_UNINDENTED(LOG_NOTICE, BLANCHED_ALMOND, args)
#define L_BLANCHED_ALMOND(args...) L(LOG_NOTICE, BLANCHED_ALMOND, args)
#define L_STACKED_BLANCHED_ALMOND(args...) L_STACKED(LOG_NOTICE, BLANCHED_ALMOND, args)

static constexpr auto BLUE = rgb(0, 0, 255);
#define L_UNINDENTED_BLUE(args...) L_UNINDENTED(LOG_NOTICE, BLUE, args)
#define L_BLUE(args...) L(LOG_NOTICE, BLUE, args)
#define L_STACKED_BLUE(args...) L_STACKED(LOG_NOTICE, BLUE, args)

static constexpr auto BLUE_VIOLET = rgb(138, 43, 226);
#define L_UNINDENTED_BLUE_VIOLET(args...) L_UNINDENTED(LOG_NOTICE, BLUE_VIOLET, args)
#define L_BLUE_VIOLET(args...) L(LOG_NOTICE, BLUE_VIOLET, args)
#define L_STACKED_BLUE_VIOLET(args...) L_STACKED(LOG_NOTICE, BLUE_VIOLET, args)

static constexpr auto BROWN = rgb(165, 42, 42);
#define L_UNINDENTED_BROWN(args...) L_UNINDENTED(LOG_NOTICE, BROWN, args)
#define L_BROWN(args...) L(LOG_NOTICE, BROWN, args)
#define L_STACKED_BROWN(args...) L_STACKED(LOG_NOTICE, BROWN, args)

static constexpr auto BURLY_WOOD = rgb(222, 184, 135);
#define L_UNINDENTED_BURLY_WOOD(args...) L_UNINDENTED(LOG_NOTICE, BURLY_WOOD, args)
#define L_BURLY_WOOD(args...) L(LOG_NOTICE, BURLY_WOOD, args)
#define L_STACKED_BURLY_WOOD(args...) L_STACKED(LOG_NOTICE, BURLY_WOOD, args)

static constexpr auto CADET_BLUE = rgb(95, 158, 160);
#define L_UNINDENTED_CADET_BLUE(args...) L_UNINDENTED(LOG_NOTICE, CADET_BLUE, args)
#define L_CADET_BLUE(args...) L(LOG_NOTICE, CADET_BLUE, args)
#define L_STACKED_CADET_BLUE(args...) L_STACKED(LOG_NOTICE, CADET_BLUE, args)

static constexpr auto CHARTREUSE = rgb(127, 255, 0);
#define L_UNINDENTED_CHARTREUSE(args...) L_UNINDENTED(LOG_NOTICE, CHARTREUSE, args)
#define L_CHARTREUSE(args...) L(LOG_NOTICE, CHARTREUSE, args)
#define L_STACKED_CHARTREUSE(args...) L_STACKED(LOG_NOTICE, CHARTREUSE, args)

static constexpr auto CHOCOLATE = rgb(210, 105, 30);
#define L_UNINDENTED_CHOCOLATE(args...) L_UNINDENTED(LOG_NOTICE, CHOCOLATE, args)
#define L_CHOCOLATE(args...) L(LOG_NOTICE, CHOCOLATE, args)
#define L_STACKED_CHOCOLATE(args...) L_STACKED(LOG_NOTICE, CHOCOLATE, args)

static constexpr auto CORAL = rgb(255, 127, 80);
#define L_UNINDENTED_CORAL(args...) L_UNINDENTED(LOG_NOTICE, CORAL, args)
#define L_CORAL(args...) L(LOG_NOTICE, CORAL, args)
#define L_STACKED_CORAL(args...) L_STACKED(LOG_NOTICE, CORAL, args)

static constexpr auto CORNFLOWER_BLUE = rgb(100, 149, 237);
#define L_UNINDENTED_CORNFLOWER_BLUE(args...) L_UNINDENTED(LOG_NOTICE, CORNFLOWER_BLUE, args)
#define L_CORNFLOWER_BLUE(args...) L(LOG_NOTICE, CORNFLOWER_BLUE, args)
#define L_STACKED_CORNFLOWER_BLUE(args...) L_STACKED(LOG_NOTICE, CORNFLOWER_BLUE, args)

static constexpr auto CORNSILK = rgb(255, 248, 220);
#define L_UNINDENTED_CORNSILK(args...) L_UNINDENTED(LOG_NOTICE, CORNSILK, args)
#define L_CORNSILK(args...) L(LOG_NOTICE, CORNSILK, args)
#define L_STACKED_CORNSILK(args...) L_STACKED(LOG_NOTICE, CORNSILK, args)

static constexpr auto CRIMSON = rgb(220, 20, 60);
#define L_UNINDENTED_CRIMSON(args...) L_UNINDENTED(LOG_NOTICE, CRIMSON, args)
#define L_CRIMSON(args...) L(LOG_NOTICE, CRIMSON, args)
#define L_STACKED_CRIMSON(args...) L_STACKED(LOG_NOTICE, CRIMSON, args)

static constexpr auto CYAN = rgb(0, 255, 255);
#define L_UNINDENTED_CYAN(args...) L_UNINDENTED(LOG_NOTICE, CYAN, args)
#define L_CYAN(args...) L(LOG_NOTICE, CYAN, args)
#define L_STACKED_CYAN(args...) L_STACKED(LOG_NOTICE, CYAN, args)

static constexpr auto DARK_BLUE = rgb(0, 0, 139);
#define L_UNINDENTED_DARK_BLUE(args...) L_UNINDENTED(LOG_NOTICE, DARK_BLUE, args)
#define L_DARK_BLUE(args...) L(LOG_NOTICE, DARK_BLUE, args)
#define L_STACKED_DARK_BLUE(args...) L_STACKED(LOG_NOTICE, DARK_BLUE, args)

static constexpr auto DARK_CYAN = rgb(0, 139, 139);
#define L_UNINDENTED_DARK_CYAN(args...) L_UNINDENTED(LOG_NOTICE, DARK_CYAN, args)
#define L_DARK_CYAN(args...) L(LOG_NOTICE, DARK_CYAN, args)
#define L_STACKED_DARK_CYAN(args...) L_STACKED(LOG_NOTICE, DARK_CYAN, args)

static constexpr auto DARK_GOLDEN_ROD = rgb(184, 134, 11);
#define L_UNINDENTED_DARK_GOLDEN_ROD(args...) L_UNINDENTED(LOG_NOTICE, DARK_GOLDEN_ROD, args)
#define L_DARK_GOLDEN_ROD(args...) L(LOG_NOTICE, DARK_GOLDEN_ROD, args)
#define L_STACKED_DARK_GOLDEN_ROD(args...) L_STACKED(LOG_NOTICE, DARK_GOLDEN_ROD, args)

static constexpr auto DARK_GRAY = rgb(169, 169, 169);
#define L_UNINDENTED_DARK_GRAY(args...) L_UNINDENTED(LOG_NOTICE, DARK_GRAY, args)
#define L_DARK_GRAY(args...) L(LOG_NOTICE, DARK_GRAY, args)
#define L_STACKED_DARK_GRAY(args...) L_STACKED(LOG_NOTICE, DARK_GRAY, args)

static constexpr auto DARK_GREEN = rgb(0, 100, 0);
#define L_UNINDENTED_DARK_GREEN(args...) L_UNINDENTED(LOG_NOTICE, DARK_GREEN, args)
#define L_DARK_GREEN(args...) L(LOG_NOTICE, DARK_GREEN, args)
#define L_STACKED_DARK_GREEN(args...) L_STACKED(LOG_NOTICE, DARK_GREEN, args)

static constexpr auto DARK_GREY = rgb(169, 169, 169);
#define L_UNINDENTED_DARK_GREY(args...) L_UNINDENTED(LOG_NOTICE, DARK_GREY, args)
#define L_DARK_GREY(args...) L(LOG_NOTICE, DARK_GREY, args)
#define L_STACKED_DARK_GREY(args...) L_STACKED(LOG_NOTICE, DARK_GREY, args)

static constexpr auto DARK_KHAKI = rgb(189, 183, 107);
#define L_UNINDENTED_DARK_KHAKI(args...) L_UNINDENTED(LOG_NOTICE, DARK_KHAKI, args)
#define L_DARK_KHAKI(args...) L(LOG_NOTICE, DARK_KHAKI, args)
#define L_STACKED_DARK_KHAKI(args...) L_STACKED(LOG_NOTICE, DARK_KHAKI, args)

static constexpr auto DARK_MAGENTA = rgb(139, 0, 139);
#define L_UNINDENTED_DARK_MAGENTA(args...) L_UNINDENTED(LOG_NOTICE, DARK_MAGENTA, args)
#define L_DARK_MAGENTA(args...) L(LOG_NOTICE, DARK_MAGENTA, args)
#define L_STACKED_DARK_MAGENTA(args...) L_STACKED(LOG_NOTICE, DARK_MAGENTA, args)

static constexpr auto DARK_OLIVE_GREEN = rgb(85, 107, 47);
#define L_UNINDENTED_DARK_OLIVE_GREEN(args...) L_UNINDENTED(LOG_NOTICE, DARK_OLIVE_GREEN, args)
#define L_DARK_OLIVE_GREEN(args...) L(LOG_NOTICE, DARK_OLIVE_GREEN, args)
#define L_STACKED_DARK_OLIVE_GREEN(args...) L_STACKED(LOG_NOTICE, DARK_OLIVE_GREEN, args)

static constexpr auto DARK_ORANGE = rgb(255, 140, 0);
#define L_UNINDENTED_DARK_ORANGE(args...) L_UNINDENTED(LOG_NOTICE, DARK_ORANGE, args)
#define L_DARK_ORANGE(args...) L(LOG_NOTICE, DARK_ORANGE, args)
#define L_STACKED_DARK_ORANGE(args...) L_STACKED(LOG_NOTICE, DARK_ORANGE, args)

static constexpr auto DARK_ORCHID = rgb(153, 50, 204);
#define L_UNINDENTED_DARK_ORCHID(args...) L_UNINDENTED(LOG_NOTICE, DARK_ORCHID, args)
#define L_DARK_ORCHID(args...) L(LOG_NOTICE, DARK_ORCHID, args)
#define L_STACKED_DARK_ORCHID(args...) L_STACKED(LOG_NOTICE, DARK_ORCHID, args)

static constexpr auto DARK_RED = rgb(139, 0, 0);
#define L_UNINDENTED_DARK_RED(args...) L_UNINDENTED(LOG_NOTICE, DARK_RED, args)
#define L_DARK_RED(args...) L(LOG_NOTICE, DARK_RED, args)
#define L_STACKED_DARK_RED(args...) L_STACKED(LOG_NOTICE, DARK_RED, args)

static constexpr auto DARK_SALMON = rgb(233, 150, 122);
#define L_UNINDENTED_DARK_SALMON(args...) L_UNINDENTED(LOG_NOTICE, DARK_SALMON, args)
#define L_DARK_SALMON(args...) L(LOG_NOTICE, DARK_SALMON, args)
#define L_STACKED_DARK_SALMON(args...) L_STACKED(LOG_NOTICE, DARK_SALMON, args)

static constexpr auto DARK_SEA_GREEN = rgb(143, 188, 143);
#define L_UNINDENTED_DARK_SEA_GREEN(args...) L_UNINDENTED(LOG_NOTICE, DARK_SEA_GREEN, args)
#define L_DARK_SEA_GREEN(args...) L(LOG_NOTICE, DARK_SEA_GREEN, args)
#define L_STACKED_DARK_SEA_GREEN(args...) L_STACKED(LOG_NOTICE, DARK_SEA_GREEN, args)

static constexpr auto DARK_SLATE_BLUE = rgb(72, 61, 139);
#define L_UNINDENTED_DARK_SLATE_BLUE(args...) L_UNINDENTED(LOG_NOTICE, DARK_SLATE_BLUE, args)
#define L_DARK_SLATE_BLUE(args...) L(LOG_NOTICE, DARK_SLATE_BLUE, args)
#define L_STACKED_DARK_SLATE_BLUE(args...) L_STACKED(LOG_NOTICE, DARK_SLATE_BLUE, args)

static constexpr auto DARK_SLATE_GRAY = rgb(47, 79, 79);
#define L_UNINDENTED_DARK_SLATE_GRAY(args...) L_UNINDENTED(LOG_NOTICE, DARK_SLATE_GRAY, args)
#define L_DARK_SLATE_GRAY(args...) L(LOG_NOTICE, DARK_SLATE_GRAY, args)
#define L_STACKED_DARK_SLATE_GRAY(args...) L_STACKED(LOG_NOTICE, DARK_SLATE_GRAY, args)

static constexpr auto DARK_SLATE_GREY = rgb(47, 79, 79);
#define L_UNINDENTED_DARK_SLATE_GREY(args...) L_UNINDENTED(LOG_NOTICE, DARK_SLATE_GREY, args)
#define L_DARK_SLATE_GREY(args...) L(LOG_NOTICE, DARK_SLATE_GREY, args)
#define L_STACKED_DARK_SLATE_GREY(args...) L_STACKED(LOG_NOTICE, DARK_SLATE_GREY, args)

static constexpr auto DARK_TURQUOISE = rgb(0, 206, 209);
#define L_UNINDENTED_DARK_TURQUOISE(args...) L_UNINDENTED(LOG_NOTICE, DARK_TURQUOISE, args)
#define L_DARK_TURQUOISE(args...) L(LOG_NOTICE, DARK_TURQUOISE, args)
#define L_STACKED_DARK_TURQUOISE(args...) L_STACKED(LOG_NOTICE, DARK_TURQUOISE, args)

static constexpr auto DARK_VIOLET = rgb(148, 0, 211);
#define L_UNINDENTED_DARK_VIOLET(args...) L_UNINDENTED(LOG_NOTICE, DARK_VIOLET, args)
#define L_DARK_VIOLET(args...) L(LOG_NOTICE, DARK_VIOLET, args)
#define L_STACKED_DARK_VIOLET(args...) L_STACKED(LOG_NOTICE, DARK_VIOLET, args)

static constexpr auto DEEP_PINK = rgb(255, 20, 147);
#define L_UNINDENTED_DEEP_PINK(args...) L_UNINDENTED(LOG_NOTICE, DEEP_PINK, args)
#define L_DEEP_PINK(args...) L(LOG_NOTICE, DEEP_PINK, args)
#define L_STACKED_DEEP_PINK(args...) L_STACKED(LOG_NOTICE, DEEP_PINK, args)

static constexpr auto DEEP_SKY_BLUE = rgb(0, 191, 255);
#define L_UNINDENTED_DEEP_SKY_BLUE(args...) L_UNINDENTED(LOG_NOTICE, DEEP_SKY_BLUE, args)
#define L_DEEP_SKY_BLUE(args...) L(LOG_NOTICE, DEEP_SKY_BLUE, args)
#define L_STACKED_DEEP_SKY_BLUE(args...) L_STACKED(LOG_NOTICE, DEEP_SKY_BLUE, args)

static constexpr auto DIM_GRAY = rgb(105, 105, 105);
#define L_UNINDENTED_DIM_GRAY(args...) L_UNINDENTED(LOG_NOTICE, DIM_GRAY, args)
#define L_DIM_GRAY(args...) L(LOG_NOTICE, DIM_GRAY, args)
#define L_STACKED_DIM_GRAY(args...) L_STACKED(LOG_NOTICE, DIM_GRAY, args)

static constexpr auto DIM_GREY = rgb(105, 105, 105);
#define L_UNINDENTED_DIM_GREY(args...) L_UNINDENTED(LOG_NOTICE, DIM_GREY, args)
#define L_DIM_GREY(args...) L(LOG_NOTICE, DIM_GREY, args)
#define L_STACKED_DIM_GREY(args...) L_STACKED(LOG_NOTICE, DIM_GREY, args)

static constexpr auto DODGER_BLUE = rgb(30, 144, 255);
#define L_UNINDENTED_DODGER_BLUE(args...) L_UNINDENTED(LOG_NOTICE, DODGER_BLUE, args)
#define L_DODGER_BLUE(args...) L(LOG_NOTICE, DODGER_BLUE, args)
#define L_STACKED_DODGER_BLUE(args...) L_STACKED(LOG_NOTICE, DODGER_BLUE, args)

static constexpr auto FIRE_BRICK = rgb(178, 34, 34);
#define L_UNINDENTED_FIRE_BRICK(args...) L_UNINDENTED(LOG_NOTICE, FIRE_BRICK, args)
#define L_FIRE_BRICK(args...) L(LOG_NOTICE, FIRE_BRICK, args)
#define L_STACKED_FIRE_BRICK(args...) L_STACKED(LOG_NOTICE, FIRE_BRICK, args)

static constexpr auto FLORAL_WHITE = rgb(255, 250, 240);
#define L_UNINDENTED_FLORAL_WHITE(args...) L_UNINDENTED(LOG_NOTICE, FLORAL_WHITE, args)
#define L_FLORAL_WHITE(args...) L(LOG_NOTICE, FLORAL_WHITE, args)
#define L_STACKED_FLORAL_WHITE(args...) L_STACKED(LOG_NOTICE, FLORAL_WHITE, args)

static constexpr auto FOREST_GREEN = rgb(34, 139, 34);
#define L_UNINDENTED_FOREST_GREEN(args...) L_UNINDENTED(LOG_NOTICE, FOREST_GREEN, args)
#define L_FOREST_GREEN(args...) L(LOG_NOTICE, FOREST_GREEN, args)
#define L_STACKED_FOREST_GREEN(args...) L_STACKED(LOG_NOTICE, FOREST_GREEN, args)

static constexpr auto FUCHSIA = rgb(255, 0, 255);
#define L_UNINDENTED_FUCHSIA(args...) L_UNINDENTED(LOG_NOTICE, FUCHSIA, args)
#define L_FUCHSIA(args...) L(LOG_NOTICE, FUCHSIA, args)
#define L_STACKED_FUCHSIA(args...) L_STACKED(LOG_NOTICE, FUCHSIA, args)

static constexpr auto GAINSBORO = rgb(220, 220, 220);
#define L_UNINDENTED_GAINSBORO(args...) L_UNINDENTED(LOG_NOTICE, GAINSBORO, args)
#define L_GAINSBORO(args...) L(LOG_NOTICE, GAINSBORO, args)
#define L_STACKED_GAINSBORO(args...) L_STACKED(LOG_NOTICE, GAINSBORO, args)

static constexpr auto GHOST_WHITE = rgb(248, 248, 255);
#define L_UNINDENTED_GHOST_WHITE(args...) L_UNINDENTED(LOG_NOTICE, GHOST_WHITE, args)
#define L_GHOST_WHITE(args...) L(LOG_NOTICE, GHOST_WHITE, args)
#define L_STACKED_GHOST_WHITE(args...) L_STACKED(LOG_NOTICE, GHOST_WHITE, args)

static constexpr auto GOLD = rgb(255, 215, 0);
#define L_UNINDENTED_GOLD(args...) L_UNINDENTED(LOG_NOTICE, GOLD, args)
#define L_GOLD(args...) L(LOG_NOTICE, GOLD, args)
#define L_STACKED_GOLD(args...) L_STACKED(LOG_NOTICE, GOLD, args)

static constexpr auto GOLDEN_ROD = rgb(218, 165, 32);
#define L_UNINDENTED_GOLDEN_ROD(args...) L_UNINDENTED(LOG_NOTICE, GOLDEN_ROD, args)
#define L_GOLDEN_ROD(args...) L(LOG_NOTICE, GOLDEN_ROD, args)
#define L_STACKED_GOLDEN_ROD(args...) L_STACKED(LOG_NOTICE, GOLDEN_ROD, args)

static constexpr auto GRAY = rgb(128, 128, 128);
#define L_UNINDENTED_GRAY(args...) L_UNINDENTED(LOG_NOTICE, GRAY, args)
#define L_GRAY(args...) L(LOG_NOTICE, GRAY, args)
#define L_STACKED_GRAY(args...) L_STACKED(LOG_NOTICE, GRAY, args)

static constexpr auto GREEN = rgb(0, 128, 0);
#define L_UNINDENTED_GREEN(args...) L_UNINDENTED(LOG_NOTICE, GREEN, args)
#define L_GREEN(args...) L(LOG_NOTICE, GREEN, args)
#define L_STACKED_GREEN(args...) L_STACKED(LOG_NOTICE, GREEN, args)

static constexpr auto GREEN_YELLOW = rgb(173, 255, 47);
#define L_UNINDENTED_GREEN_YELLOW(args...) L_UNINDENTED(LOG_NOTICE, GREEN_YELLOW, args)
#define L_GREEN_YELLOW(args...) L(LOG_NOTICE, GREEN_YELLOW, args)
#define L_STACKED_GREEN_YELLOW(args...) L_STACKED(LOG_NOTICE, GREEN_YELLOW, args)

static constexpr auto GREY = rgb(128, 128, 128);
#define L_UNINDENTED_GREY(args...) L_UNINDENTED(LOG_NOTICE, GREY, args)
#define L_GREY(args...) L(LOG_NOTICE, GREY, args)
#define L_STACKED_GREY(args...) L_STACKED(LOG_NOTICE, GREY, args)

static constexpr auto HONEY_DEW = rgb(240, 255, 240);
#define L_UNINDENTED_HONEY_DEW(args...) L_UNINDENTED(LOG_NOTICE, HONEY_DEW, args)
#define L_HONEY_DEW(args...) L(LOG_NOTICE, HONEY_DEW, args)
#define L_STACKED_HONEY_DEW(args...) L_STACKED(LOG_NOTICE, HONEY_DEW, args)

static constexpr auto HOT_PINK = rgb(255, 105, 180);
#define L_UNINDENTED_HOT_PINK(args...) L_UNINDENTED(LOG_NOTICE, HOT_PINK, args)
#define L_HOT_PINK(args...) L(LOG_NOTICE, HOT_PINK, args)
#define L_STACKED_HOT_PINK(args...) L_STACKED(LOG_NOTICE, HOT_PINK, args)

static constexpr auto INDIAN_RED = rgb(205, 92, 92);
#define L_UNINDENTED_INDIAN_RED(args...) L_UNINDENTED(LOG_NOTICE, INDIAN_RED, args)
#define L_INDIAN_RED(args...) L(LOG_NOTICE, INDIAN_RED, args)
#define L_STACKED_INDIAN_RED(args...) L_STACKED(LOG_NOTICE, INDIAN_RED, args)

static constexpr auto INDIGO = rgb(75, 0, 130);
#define L_UNINDENTED_INDIGO(args...) L_UNINDENTED(LOG_NOTICE, INDIGO, args)
#define L_INDIGO(args...) L(LOG_NOTICE, INDIGO, args)
#define L_STACKED_INDIGO(args...) L_STACKED(LOG_NOTICE, INDIGO, args)

static constexpr auto IVORY = rgb(255, 255, 240);
#define L_UNINDENTED_IVORY(args...) L_UNINDENTED(LOG_NOTICE, IVORY, args)
#define L_IVORY(args...) L(LOG_NOTICE, IVORY, args)
#define L_STACKED_IVORY(args...) L_STACKED(LOG_NOTICE, IVORY, args)

static constexpr auto KHAKI = rgb(240, 230, 140);
#define L_UNINDENTED_KHAKI(args...) L_UNINDENTED(LOG_NOTICE, KHAKI, args)
#define L_KHAKI(args...) L(LOG_NOTICE, KHAKI, args)
#define L_STACKED_KHAKI(args...) L_STACKED(LOG_NOTICE, KHAKI, args)

static constexpr auto LAVENDER = rgb(230, 230, 250);
#define L_UNINDENTED_LAVENDER(args...) L_UNINDENTED(LOG_NOTICE, LAVENDER, args)
#define L_LAVENDER(args...) L(LOG_NOTICE, LAVENDER, args)
#define L_STACKED_LAVENDER(args...) L_STACKED(LOG_NOTICE, LAVENDER, args)

static constexpr auto LAVENDER_BLUSH = rgb(255, 240, 245);
#define L_UNINDENTED_LAVENDER_BLUSH(args...) L_UNINDENTED(LOG_NOTICE, LAVENDER_BLUSH, args)
#define L_LAVENDER_BLUSH(args...) L(LOG_NOTICE, LAVENDER_BLUSH, args)
#define L_STACKED_LAVENDER_BLUSH(args...) L_STACKED(LOG_NOTICE, LAVENDER_BLUSH, args)

static constexpr auto LAWN_GREEN = rgb(124, 252, 0);
#define L_UNINDENTED_LAWN_GREEN(args...) L_UNINDENTED(LOG_NOTICE, LAWN_GREEN, args)
#define L_LAWN_GREEN(args...) L(LOG_NOTICE, LAWN_GREEN, args)
#define L_STACKED_LAWN_GREEN(args...) L_STACKED(LOG_NOTICE, LAWN_GREEN, args)

static constexpr auto LEMON_CHIFFON = rgb(255, 250, 205);
#define L_UNINDENTED_LEMON_CHIFFON(args...) L_UNINDENTED(LOG_NOTICE, LEMON_CHIFFON, args)
#define L_LEMON_CHIFFON(args...) L(LOG_NOTICE, LEMON_CHIFFON, args)
#define L_STACKED_LEMON_CHIFFON(args...) L_STACKED(LOG_NOTICE, LEMON_CHIFFON, args)

static constexpr auto LIGHT_BLUE = rgb(173, 216, 230);
#define L_UNINDENTED_LIGHT_BLUE(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_BLUE, args)
#define L_LIGHT_BLUE(args...) L(LOG_NOTICE, LIGHT_BLUE, args)
#define L_STACKED_LIGHT_BLUE(args...) L_STACKED(LOG_NOTICE, LIGHT_BLUE, args)

static constexpr auto LIGHT_CORAL = rgb(240, 128, 128);
#define L_UNINDENTED_LIGHT_CORAL(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_CORAL, args)
#define L_LIGHT_CORAL(args...) L(LOG_NOTICE, LIGHT_CORAL, args)
#define L_STACKED_LIGHT_CORAL(args...) L_STACKED(LOG_NOTICE, LIGHT_CORAL, args)

static constexpr auto LIGHT_CYAN = rgb(224, 255, 255);
#define L_UNINDENTED_LIGHT_CYAN(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_CYAN, args)
#define L_LIGHT_CYAN(args...) L(LOG_NOTICE, LIGHT_CYAN, args)
#define L_STACKED_LIGHT_CYAN(args...) L_STACKED(LOG_NOTICE, LIGHT_CYAN, args)

static constexpr auto LIGHT_GOLDEN_ROD_YELLOW = rgb(250, 250, 210);
#define L_UNINDENTED_LIGHT_GOLDEN_ROD_YELLOW(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_GOLDEN_ROD_YELLOW, args)
#define L_LIGHT_GOLDEN_ROD_YELLOW(args...) L(LOG_NOTICE, LIGHT_GOLDEN_ROD_YELLOW, args)
#define L_STACKED_LIGHT_GOLDEN_ROD_YELLOW(args...) L_STACKED(LOG_NOTICE, LIGHT_GOLDEN_ROD_YELLOW, args)

static constexpr auto LIGHT_GRAY = rgb(211, 211, 211);
#define L_UNINDENTED_LIGHT_GRAY(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_GRAY, args)
#define L_LIGHT_GRAY(args...) L(LOG_NOTICE, LIGHT_GRAY, args)
#define L_STACKED_LIGHT_GRAY(args...) L_STACKED(LOG_NOTICE, LIGHT_GRAY, args)

static constexpr auto LIGHT_GREEN = rgb(144, 238, 144);
#define L_UNINDENTED_LIGHT_GREEN(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_GREEN, args)
#define L_LIGHT_GREEN(args...) L(LOG_NOTICE, LIGHT_GREEN, args)
#define L_STACKED_LIGHT_GREEN(args...) L_STACKED(LOG_NOTICE, LIGHT_GREEN, args)

static constexpr auto LIGHT_GREY = rgb(211, 211, 211);
#define L_UNINDENTED_LIGHT_GREY(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_GREY, args)
#define L_LIGHT_GREY(args...) L(LOG_NOTICE, LIGHT_GREY, args)
#define L_STACKED_LIGHT_GREY(args...) L_STACKED(LOG_NOTICE, LIGHT_GREY, args)

static constexpr auto LIGHT_PINK = rgb(255, 182, 193);
#define L_UNINDENTED_LIGHT_PINK(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_PINK, args)
#define L_LIGHT_PINK(args...) L(LOG_NOTICE, LIGHT_PINK, args)
#define L_STACKED_LIGHT_PINK(args...) L_STACKED(LOG_NOTICE, LIGHT_PINK, args)

static constexpr auto LIGHT_PURPLE = rgb(232, 10, 180);
#define L_UNINDENTED_LIGHT_PURPLE(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_PURPLE, args)
#define L_LIGHT_PURPLE(args...) L(LOG_NOTICE, LIGHT_PURPLE, args)
#define L_STACKED_LIGHT_PURPLE(args...) L_STACKED(LOG_NOTICE, LIGHT_PURPLE, args)

static constexpr auto LIGHT_RED = rgb(232, 25, 10);
#define L_UNINDENTED_LIGHT_RED(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_RED, args)
#define L_LIGHT_RED(args...) L(LOG_NOTICE, LIGHT_RED, args)
#define L_STACKED_LIGHT_RED(args...) L_STACKED(LOG_NOTICE, LIGHT_RED, args)

static constexpr auto LIGHT_SALMON = rgb(255, 160, 122);
#define L_UNINDENTED_LIGHT_SALMON(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_SALMON, args)
#define L_LIGHT_SALMON(args...) L(LOG_NOTICE, LIGHT_SALMON, args)
#define L_STACKED_LIGHT_SALMON(args...) L_STACKED(LOG_NOTICE, LIGHT_SALMON, args)

static constexpr auto LIGHT_SEA_GREEN = rgb(32, 178, 170);
#define L_UNINDENTED_LIGHT_SEA_GREEN(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_SEA_GREEN, args)
#define L_LIGHT_SEA_GREEN(args...) L(LOG_NOTICE, LIGHT_SEA_GREEN, args)
#define L_STACKED_LIGHT_SEA_GREEN(args...) L_STACKED(LOG_NOTICE, LIGHT_SEA_GREEN, args)

static constexpr auto LIGHT_SKY_BLUE = rgb(135, 206, 250);
#define L_UNINDENTED_LIGHT_SKY_BLUE(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_SKY_BLUE, args)
#define L_LIGHT_SKY_BLUE(args...) L(LOG_NOTICE, LIGHT_SKY_BLUE, args)
#define L_STACKED_LIGHT_SKY_BLUE(args...) L_STACKED(LOG_NOTICE, LIGHT_SKY_BLUE, args)

static constexpr auto LIGHT_SLATE_GRAY = rgb(119, 136, 153);
#define L_UNINDENTED_LIGHT_SLATE_GRAY(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_SLATE_GRAY, args)
#define L_LIGHT_SLATE_GRAY(args...) L(LOG_NOTICE, LIGHT_SLATE_GRAY, args)
#define L_STACKED_LIGHT_SLATE_GRAY(args...) L_STACKED(LOG_NOTICE, LIGHT_SLATE_GRAY, args)

static constexpr auto LIGHT_SLATE_GREY = rgb(119, 136, 153);
#define L_UNINDENTED_LIGHT_SLATE_GREY(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_SLATE_GREY, args)
#define L_LIGHT_SLATE_GREY(args...) L(LOG_NOTICE, LIGHT_SLATE_GREY, args)
#define L_STACKED_LIGHT_SLATE_GREY(args...) L_STACKED(LOG_NOTICE, LIGHT_SLATE_GREY, args)

static constexpr auto LIGHT_STEEL_BLUE = rgb(176, 196, 222);
#define L_UNINDENTED_LIGHT_STEEL_BLUE(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_STEEL_BLUE, args)
#define L_LIGHT_STEEL_BLUE(args...) L(LOG_NOTICE, LIGHT_STEEL_BLUE, args)
#define L_STACKED_LIGHT_STEEL_BLUE(args...) L_STACKED(LOG_NOTICE, LIGHT_STEEL_BLUE, args)

static constexpr auto LIGHT_YELLOW = rgb(255, 255, 224);
#define L_UNINDENTED_LIGHT_YELLOW(args...) L_UNINDENTED(LOG_NOTICE, LIGHT_YELLOW, args)
#define L_LIGHT_YELLOW(args...) L(LOG_NOTICE, LIGHT_YELLOW, args)
#define L_STACKED_LIGHT_YELLOW(args...) L_STACKED(LOG_NOTICE, LIGHT_YELLOW, args)

static constexpr auto LIME = rgb(0, 255, 0);
#define L_UNINDENTED_LIME(args...) L_UNINDENTED(LOG_NOTICE, LIME, args)
#define L_LIME(args...) L(LOG_NOTICE, LIME, args)
#define L_STACKED_LIME(args...) L_STACKED(LOG_NOTICE, LIME, args)

static constexpr auto LIME_GREEN = rgb(50, 205, 50);
#define L_UNINDENTED_LIME_GREEN(args...) L_UNINDENTED(LOG_NOTICE, LIME_GREEN, args)
#define L_LIME_GREEN(args...) L(LOG_NOTICE, LIME_GREEN, args)
#define L_STACKED_LIME_GREEN(args...) L_STACKED(LOG_NOTICE, LIME_GREEN, args)

static constexpr auto LINEN = rgb(250, 240, 230);
#define L_UNINDENTED_LINEN(args...) L_UNINDENTED(LOG_NOTICE, LINEN, args)
#define L_LINEN(args...) L(LOG_NOTICE, LINEN, args)
#define L_STACKED_LINEN(args...) L_STACKED(LOG_NOTICE, LINEN, args)

static constexpr auto MAGENTA = rgb(255, 0, 255);
#define L_UNINDENTED_MAGENTA(args...) L_UNINDENTED(LOG_NOTICE, MAGENTA, args)
#define L_MAGENTA(args...) L(LOG_NOTICE, MAGENTA, args)
#define L_STACKED_MAGENTA(args...) L_STACKED(LOG_NOTICE, MAGENTA, args)

static constexpr auto MAROON = rgb(128, 0, 0);
#define L_UNINDENTED_MAROON(args...) L_UNINDENTED(LOG_NOTICE, MAROON, args)
#define L_MAROON(args...) L(LOG_NOTICE, MAROON, args)
#define L_STACKED_MAROON(args...) L_STACKED(LOG_NOTICE, MAROON, args)

static constexpr auto MEDIUM_AQUA_MARINE = rgb(102, 205, 170);
#define L_UNINDENTED_MEDIUM_AQUA_MARINE(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_AQUA_MARINE, args)
#define L_MEDIUM_AQUA_MARINE(args...) L(LOG_NOTICE, MEDIUM_AQUA_MARINE, args)
#define L_STACKED_MEDIUM_AQUA_MARINE(args...) L_STACKED(LOG_NOTICE, MEDIUM_AQUA_MARINE, args)

static constexpr auto MEDIUM_BLUE = rgb(0, 0, 205);
#define L_UNINDENTED_MEDIUM_BLUE(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_BLUE, args)
#define L_MEDIUM_BLUE(args...) L(LOG_NOTICE, MEDIUM_BLUE, args)
#define L_STACKED_MEDIUM_BLUE(args...) L_STACKED(LOG_NOTICE, MEDIUM_BLUE, args)

static constexpr auto MEDIUM_ORCHID = rgb(186, 85, 211);
#define L_UNINDENTED_MEDIUM_ORCHID(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_ORCHID, args)
#define L_MEDIUM_ORCHID(args...) L(LOG_NOTICE, MEDIUM_ORCHID, args)
#define L_STACKED_MEDIUM_ORCHID(args...) L_STACKED(LOG_NOTICE, MEDIUM_ORCHID, args)

static constexpr auto MEDIUM_PURPLE = rgb(147, 112, 216);
#define L_UNINDENTED_MEDIUM_PURPLE(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_PURPLE, args)
#define L_MEDIUM_PURPLE(args...) L(LOG_NOTICE, MEDIUM_PURPLE, args)
#define L_STACKED_MEDIUM_PURPLE(args...) L_STACKED(LOG_NOTICE, MEDIUM_PURPLE, args)

static constexpr auto MEDIUM_SEA_GREEN = rgb(60, 179, 113);
#define L_UNINDENTED_MEDIUM_SEA_GREEN(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_SEA_GREEN, args)
#define L_MEDIUM_SEA_GREEN(args...) L(LOG_NOTICE, MEDIUM_SEA_GREEN, args)
#define L_STACKED_MEDIUM_SEA_GREEN(args...) L_STACKED(LOG_NOTICE, MEDIUM_SEA_GREEN, args)

static constexpr auto MEDIUM_SLATE_BLUE = rgb(123, 104, 238);
#define L_UNINDENTED_MEDIUM_SLATE_BLUE(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_SLATE_BLUE, args)
#define L_MEDIUM_SLATE_BLUE(args...) L(LOG_NOTICE, MEDIUM_SLATE_BLUE, args)
#define L_STACKED_MEDIUM_SLATE_BLUE(args...) L_STACKED(LOG_NOTICE, MEDIUM_SLATE_BLUE, args)

static constexpr auto MEDIUM_SPRING_GREEN = rgb(0, 250, 154);
#define L_UNINDENTED_MEDIUM_SPRING_GREEN(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_SPRING_GREEN, args)
#define L_MEDIUM_SPRING_GREEN(args...) L(LOG_NOTICE, MEDIUM_SPRING_GREEN, args)
#define L_STACKED_MEDIUM_SPRING_GREEN(args...) L_STACKED(LOG_NOTICE, MEDIUM_SPRING_GREEN, args)

static constexpr auto MEDIUM_TURQUOISE = rgb(72, 209, 204);
#define L_UNINDENTED_MEDIUM_TURQUOISE(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_TURQUOISE, args)
#define L_MEDIUM_TURQUOISE(args...) L(LOG_NOTICE, MEDIUM_TURQUOISE, args)
#define L_STACKED_MEDIUM_TURQUOISE(args...) L_STACKED(LOG_NOTICE, MEDIUM_TURQUOISE, args)

static constexpr auto MEDIUM_VIOLET_RED = rgb(199, 21, 133);
#define L_UNINDENTED_MEDIUM_VIOLET_RED(args...) L_UNINDENTED(LOG_NOTICE, MEDIUM_VIOLET_RED, args)
#define L_MEDIUM_VIOLET_RED(args...) L(LOG_NOTICE, MEDIUM_VIOLET_RED, args)
#define L_STACKED_MEDIUM_VIOLET_RED(args...) L_STACKED(LOG_NOTICE, MEDIUM_VIOLET_RED, args)

static constexpr auto MIDNIGHT_BLUE = rgb(25, 25, 112);
#define L_UNINDENTED_MIDNIGHT_BLUE(args...) L_UNINDENTED(LOG_NOTICE, MIDNIGHT_BLUE, args)
#define L_MIDNIGHT_BLUE(args...) L(LOG_NOTICE, MIDNIGHT_BLUE, args)
#define L_STACKED_MIDNIGHT_BLUE(args...) L_STACKED(LOG_NOTICE, MIDNIGHT_BLUE, args)

static constexpr auto MINT_CREAM = rgb(245, 255, 250);
#define L_UNINDENTED_MINT_CREAM(args...) L_UNINDENTED(LOG_NOTICE, MINT_CREAM, args)
#define L_MINT_CREAM(args...) L(LOG_NOTICE, MINT_CREAM, args)
#define L_STACKED_MINT_CREAM(args...) L_STACKED(LOG_NOTICE, MINT_CREAM, args)

static constexpr auto MISTY_ROSE = rgb(255, 228, 225);
#define L_UNINDENTED_MISTY_ROSE(args...) L_UNINDENTED(LOG_NOTICE, MISTY_ROSE, args)
#define L_MISTY_ROSE(args...) L(LOG_NOTICE, MISTY_ROSE, args)
#define L_STACKED_MISTY_ROSE(args...) L_STACKED(LOG_NOTICE, MISTY_ROSE, args)

static constexpr auto MOCCASIN = rgb(255, 228, 181);
#define L_UNINDENTED_MOCCASIN(args...) L_UNINDENTED(LOG_NOTICE, MOCCASIN, args)
#define L_MOCCASIN(args...) L(LOG_NOTICE, MOCCASIN, args)
#define L_STACKED_MOCCASIN(args...) L_STACKED(LOG_NOTICE, MOCCASIN, args)

static constexpr auto NAVAJO_WHITE = rgb(255, 222, 173);
#define L_UNINDENTED_NAVAJO_WHITE(args...) L_UNINDENTED(LOG_NOTICE, NAVAJO_WHITE, args)
#define L_NAVAJO_WHITE(args...) L(LOG_NOTICE, NAVAJO_WHITE, args)
#define L_STACKED_NAVAJO_WHITE(args...) L_STACKED(LOG_NOTICE, NAVAJO_WHITE, args)

static constexpr auto NAVY = rgb(0, 0, 128);
#define L_UNINDENTED_NAVY(args...) L_UNINDENTED(LOG_NOTICE, NAVY, args)
#define L_NAVY(args...) L(LOG_NOTICE, NAVY, args)
#define L_STACKED_NAVY(args...) L_STACKED(LOG_NOTICE, NAVY, args)

static constexpr auto OLD_LACE = rgb(253, 245, 230);
#define L_UNINDENTED_OLD_LACE(args...) L_UNINDENTED(LOG_NOTICE, OLD_LACE, args)
#define L_OLD_LACE(args...) L(LOG_NOTICE, OLD_LACE, args)
#define L_STACKED_OLD_LACE(args...) L_STACKED(LOG_NOTICE, OLD_LACE, args)

static constexpr auto OLIVE = rgb(128, 128, 0);
#define L_UNINDENTED_OLIVE(args...) L_UNINDENTED(LOG_NOTICE, OLIVE, args)
#define L_OLIVE(args...) L(LOG_NOTICE, OLIVE, args)
#define L_STACKED_OLIVE(args...) L_STACKED(LOG_NOTICE, OLIVE, args)

static constexpr auto OLIVE_DRAB = rgb(107, 142, 35);
#define L_UNINDENTED_OLIVE_DRAB(args...) L_UNINDENTED(LOG_NOTICE, OLIVE_DRAB, args)
#define L_OLIVE_DRAB(args...) L(LOG_NOTICE, OLIVE_DRAB, args)
#define L_STACKED_OLIVE_DRAB(args...) L_STACKED(LOG_NOTICE, OLIVE_DRAB, args)

static constexpr auto ORANGE = rgb(255, 165, 0);
#define L_UNINDENTED_ORANGE(args...) L_UNINDENTED(LOG_NOTICE, ORANGE, args)
#define L_ORANGE(args...) L(LOG_NOTICE, ORANGE, args)
#define L_STACKED_ORANGE(args...) L_STACKED(LOG_NOTICE, ORANGE, args)

static constexpr auto ORANGE_RED = rgb(255, 69, 0);
#define L_UNINDENTED_ORANGE_RED(args...) L_UNINDENTED(LOG_NOTICE, ORANGE_RED, args)
#define L_ORANGE_RED(args...) L(LOG_NOTICE, ORANGE_RED, args)
#define L_STACKED_ORANGE_RED(args...) L_STACKED(LOG_NOTICE, ORANGE_RED, args)

static constexpr auto ORCHID = rgb(218, 112, 214);
#define L_UNINDENTED_ORCHID(args...) L_UNINDENTED(LOG_NOTICE, ORCHID, args)
#define L_ORCHID(args...) L(LOG_NOTICE, ORCHID, args)
#define L_STACKED_ORCHID(args...) L_STACKED(LOG_NOTICE, ORCHID, args)

static constexpr auto PALE_GOLDEN_ROD = rgb(238, 232, 170);
#define L_UNINDENTED_PALE_GOLDEN_ROD(args...) L_UNINDENTED(LOG_NOTICE, PALE_GOLDEN_ROD, args)
#define L_PALE_GOLDEN_ROD(args...) L(LOG_NOTICE, PALE_GOLDEN_ROD, args)
#define L_STACKED_PALE_GOLDEN_ROD(args...) L_STACKED(LOG_NOTICE, PALE_GOLDEN_ROD, args)

static constexpr auto PALE_GREEN = rgb(152, 251, 152);
#define L_UNINDENTED_PALE_GREEN(args...) L_UNINDENTED(LOG_NOTICE, PALE_GREEN, args)
#define L_PALE_GREEN(args...) L(LOG_NOTICE, PALE_GREEN, args)
#define L_STACKED_PALE_GREEN(args...) L_STACKED(LOG_NOTICE, PALE_GREEN, args)

static constexpr auto PALE_TURQUOISE = rgb(175, 238, 238);
#define L_UNINDENTED_PALE_TURQUOISE(args...) L_UNINDENTED(LOG_NOTICE, PALE_TURQUOISE, args)
#define L_PALE_TURQUOISE(args...) L(LOG_NOTICE, PALE_TURQUOISE, args)
#define L_STACKED_PALE_TURQUOISE(args...) L_STACKED(LOG_NOTICE, PALE_TURQUOISE, args)

static constexpr auto PALE_VIOLET_RED = rgb(216, 112, 147);
#define L_UNINDENTED_PALE_VIOLET_RED(args...) L_UNINDENTED(LOG_NOTICE, PALE_VIOLET_RED, args)
#define L_PALE_VIOLET_RED(args...) L(LOG_NOTICE, PALE_VIOLET_RED, args)
#define L_STACKED_PALE_VIOLET_RED(args...) L_STACKED(LOG_NOTICE, PALE_VIOLET_RED, args)

static constexpr auto PAPAYA_WHIP = rgb(255, 239, 213);
#define L_UNINDENTED_PAPAYA_WHIP(args...) L_UNINDENTED(LOG_NOTICE, PAPAYA_WHIP, args)
#define L_PAPAYA_WHIP(args...) L(LOG_NOTICE, PAPAYA_WHIP, args)
#define L_STACKED_PAPAYA_WHIP(args...) L_STACKED(LOG_NOTICE, PAPAYA_WHIP, args)

static constexpr auto PEACH_PUFF = rgb(255, 218, 185);
#define L_UNINDENTED_PEACH_PUFF(args...) L_UNINDENTED(LOG_NOTICE, PEACH_PUFF, args)
#define L_PEACH_PUFF(args...) L(LOG_NOTICE, PEACH_PUFF, args)
#define L_STACKED_PEACH_PUFF(args...) L_STACKED(LOG_NOTICE, PEACH_PUFF, args)

static constexpr auto PERU = rgb(205, 133, 63);
#define L_UNINDENTED_PERU(args...) L_UNINDENTED(LOG_NOTICE, PERU, args)
#define L_PERU(args...) L(LOG_NOTICE, PERU, args)
#define L_STACKED_PERU(args...) L_STACKED(LOG_NOTICE, PERU, args)

static constexpr auto PINK = rgb(255, 192, 203);
#define L_UNINDENTED_PINK(args...) L_UNINDENTED(LOG_NOTICE, PINK, args)
#define L_PINK(args...) L(LOG_NOTICE, PINK, args)
#define L_STACKED_PINK(args...) L_STACKED(LOG_NOTICE, PINK, args)

static constexpr auto PLUM = rgb(221, 160, 221);
#define L_UNINDENTED_PLUM(args...) L_UNINDENTED(LOG_NOTICE, PLUM, args)
#define L_PLUM(args...) L(LOG_NOTICE, PLUM, args)
#define L_STACKED_PLUM(args...) L_STACKED(LOG_NOTICE, PLUM, args)

static constexpr auto POWDER_BLUE = rgb(176, 224, 230);
#define L_UNINDENTED_POWDER_BLUE(args...) L_UNINDENTED(LOG_NOTICE, POWDER_BLUE, args)
#define L_POWDER_BLUE(args...) L(LOG_NOTICE, POWDER_BLUE, args)
#define L_STACKED_POWDER_BLUE(args...) L_STACKED(LOG_NOTICE, POWDER_BLUE, args)

static constexpr auto PURPLE = rgb(128, 0, 128);
#define L_UNINDENTED_PURPLE(args...) L_UNINDENTED(LOG_NOTICE, PURPLE, args)
#define L_PURPLE(args...) L(LOG_NOTICE, PURPLE, args)
#define L_STACKED_PURPLE(args...) L_STACKED(LOG_NOTICE, PURPLE, args)

static constexpr auto RED = rgb(255, 0, 0);
#define L_UNINDENTED_RED(args...) L_UNINDENTED(LOG_NOTICE, RED, args)
#define L_RED(args...) L(LOG_NOTICE, RED, args)
#define L_STACKED_RED(args...) L_STACKED(LOG_NOTICE, RED, args)

static constexpr auto ROSY_BROWN = rgb(188, 143, 143);
#define L_UNINDENTED_ROSY_BROWN(args...) L_UNINDENTED(LOG_NOTICE, ROSY_BROWN, args)
#define L_ROSY_BROWN(args...) L(LOG_NOTICE, ROSY_BROWN, args)
#define L_STACKED_ROSY_BROWN(args...) L_STACKED(LOG_NOTICE, ROSY_BROWN, args)

static constexpr auto ROYAL_BLUE = rgb(65, 105, 225);
#define L_UNINDENTED_ROYAL_BLUE(args...) L_UNINDENTED(LOG_NOTICE, ROYAL_BLUE, args)
#define L_ROYAL_BLUE(args...) L(LOG_NOTICE, ROYAL_BLUE, args)
#define L_STACKED_ROYAL_BLUE(args...) L_STACKED(LOG_NOTICE, ROYAL_BLUE, args)

static constexpr auto SADDLE_BROWN = rgb(139, 69, 19);
#define L_UNINDENTED_SADDLE_BROWN(args...) L_UNINDENTED(LOG_NOTICE, SADDLE_BROWN, args)
#define L_SADDLE_BROWN(args...) L(LOG_NOTICE, SADDLE_BROWN, args)
#define L_STACKED_SADDLE_BROWN(args...) L_STACKED(LOG_NOTICE, SADDLE_BROWN, args)

static constexpr auto SALMON = rgb(250, 128, 114);
#define L_UNINDENTED_SALMON(args...) L_UNINDENTED(LOG_NOTICE, SALMON, args)
#define L_SALMON(args...) L(LOG_NOTICE, SALMON, args)
#define L_STACKED_SALMON(args...) L_STACKED(LOG_NOTICE, SALMON, args)

static constexpr auto SANDY_BROWN = rgb(244, 164, 96);
#define L_UNINDENTED_SANDY_BROWN(args...) L_UNINDENTED(LOG_NOTICE, SANDY_BROWN, args)
#define L_SANDY_BROWN(args...) L(LOG_NOTICE, SANDY_BROWN, args)
#define L_STACKED_SANDY_BROWN(args...) L_STACKED(LOG_NOTICE, SANDY_BROWN, args)

static constexpr auto SEA_GREEN = rgb(46, 139, 87);
#define L_UNINDENTED_SEA_GREEN(args...) L_UNINDENTED(LOG_NOTICE, SEA_GREEN, args)
#define L_SEA_GREEN(args...) L(LOG_NOTICE, SEA_GREEN, args)
#define L_STACKED_SEA_GREEN(args...) L_STACKED(LOG_NOTICE, SEA_GREEN, args)

static constexpr auto SEA_SHELL = rgb(255, 245, 238);
#define L_UNINDENTED_SEA_SHELL(args...) L_UNINDENTED(LOG_NOTICE, SEA_SHELL, args)
#define L_SEA_SHELL(args...) L(LOG_NOTICE, SEA_SHELL, args)
#define L_STACKED_SEA_SHELL(args...) L_STACKED(LOG_NOTICE, SEA_SHELL, args)

static constexpr auto SIENNA = rgb(160, 82, 45);
#define L_UNINDENTED_SIENNA(args...) L_UNINDENTED(LOG_NOTICE, SIENNA, args)
#define L_SIENNA(args...) L(LOG_NOTICE, SIENNA, args)
#define L_STACKED_SIENNA(args...) L_STACKED(LOG_NOTICE, SIENNA, args)

static constexpr auto SILVER = rgb(192, 192, 192);
#define L_UNINDENTED_SILVER(args...) L_UNINDENTED(LOG_NOTICE, SILVER, args)
#define L_SILVER(args...) L(LOG_NOTICE, SILVER, args)
#define L_STACKED_SILVER(args...) L_STACKED(LOG_NOTICE, SILVER, args)

static constexpr auto SKY_BLUE = rgb(135, 206, 235);
#define L_UNINDENTED_SKY_BLUE(args...) L_UNINDENTED(LOG_NOTICE, SKY_BLUE, args)
#define L_SKY_BLUE(args...) L(LOG_NOTICE, SKY_BLUE, args)
#define L_STACKED_SKY_BLUE(args...) L_STACKED(LOG_NOTICE, SKY_BLUE, args)

static constexpr auto SLATE_BLUE = rgb(106, 90, 205);
#define L_UNINDENTED_SLATE_BLUE(args...) L_UNINDENTED(LOG_NOTICE, SLATE_BLUE, args)
#define L_SLATE_BLUE(args...) L(LOG_NOTICE, SLATE_BLUE, args)
#define L_STACKED_SLATE_BLUE(args...) L_STACKED(LOG_NOTICE, SLATE_BLUE, args)

static constexpr auto SLATE_GRAY = rgb(112, 128, 144);
#define L_UNINDENTED_SLATE_GRAY(args...) L_UNINDENTED(LOG_NOTICE, SLATE_GRAY, args)
#define L_SLATE_GRAY(args...) L(LOG_NOTICE, SLATE_GRAY, args)
#define L_STACKED_SLATE_GRAY(args...) L_STACKED(LOG_NOTICE, SLATE_GRAY, args)

static constexpr auto SLATE_GREY = rgb(112, 128, 144);
#define L_UNINDENTED_SLATE_GREY(args...) L_UNINDENTED(LOG_NOTICE, SLATE_GREY, args)
#define L_SLATE_GREY(args...) L(LOG_NOTICE, SLATE_GREY, args)
#define L_STACKED_SLATE_GREY(args...) L_STACKED(LOG_NOTICE, SLATE_GREY, args)

static constexpr auto SNOW = rgb(255, 250, 250);
#define L_UNINDENTED_SNOW(args...) L_UNINDENTED(LOG_NOTICE, SNOW, args)
#define L_SNOW(args...) L(LOG_NOTICE, SNOW, args)
#define L_STACKED_SNOW(args...) L_STACKED(LOG_NOTICE, SNOW, args)

static constexpr auto SPRING_GREEN = rgb(0, 255, 127);
#define L_UNINDENTED_SPRING_GREEN(args...) L_UNINDENTED(LOG_NOTICE, SPRING_GREEN, args)
#define L_SPRING_GREEN(args...) L(LOG_NOTICE, SPRING_GREEN, args)
#define L_STACKED_SPRING_GREEN(args...) L_STACKED(LOG_NOTICE, SPRING_GREEN, args)

static constexpr auto STEEL_BLUE = rgb(70, 130, 180);
#define L_UNINDENTED_STEEL_BLUE(args...) L_UNINDENTED(LOG_NOTICE, STEEL_BLUE, args)
#define L_STEEL_BLUE(args...) L(LOG_NOTICE, STEEL_BLUE, args)
#define L_STACKED_STEEL_BLUE(args...) L_STACKED(LOG_NOTICE, STEEL_BLUE, args)

static constexpr auto TAN = rgb(210, 180, 140);
#define L_UNINDENTED_TAN(args...) L_UNINDENTED(LOG_NOTICE, TAN, args)
#define L_TAN(args...) L(LOG_NOTICE, TAN, args)
#define L_STACKED_TAN(args...) L_STACKED(LOG_NOTICE, TAN, args)

static constexpr auto TEAL = rgb(0, 128, 128);
#define L_UNINDENTED_TEAL(args...) L_UNINDENTED(LOG_NOTICE, TEAL, args)
#define L_TEAL(args...) L(LOG_NOTICE, TEAL, args)
#define L_STACKED_TEAL(args...) L_STACKED(LOG_NOTICE, TEAL, args)

static constexpr auto THISTLE = rgb(216, 191, 216);
#define L_UNINDENTED_THISTLE(args...) L_UNINDENTED(LOG_NOTICE, THISTLE, args)
#define L_THISTLE(args...) L(LOG_NOTICE, THISTLE, args)
#define L_STACKED_THISTLE(args...) L_STACKED(LOG_NOTICE, THISTLE, args)

static constexpr auto TOMATO = rgb(255, 99, 71);
#define L_UNINDENTED_TOMATO(args...) L_UNINDENTED(LOG_NOTICE, TOMATO, args)
#define L_TOMATO(args...) L(LOG_NOTICE, TOMATO, args)
#define L_STACKED_TOMATO(args...) L_STACKED(LOG_NOTICE, TOMATO, args)

static constexpr auto TURQUOISE = rgb(64, 224, 208);
#define L_UNINDENTED_TURQUOISE(args...) L_UNINDENTED(LOG_NOTICE, TURQUOISE, args)
#define L_TURQUOISE(args...) L(LOG_NOTICE, TURQUOISE, args)
#define L_STACKED_TURQUOISE(args...) L_STACKED(LOG_NOTICE, TURQUOISE, args)

static constexpr auto VIOLET = rgb(238, 130, 238);
#define L_UNINDENTED_VIOLET(args...) L_UNINDENTED(LOG_NOTICE, VIOLET, args)
#define L_VIOLET(args...) L(LOG_NOTICE, VIOLET, args)
#define L_STACKED_VIOLET(args...) L_STACKED(LOG_NOTICE, VIOLET, args)

static constexpr auto WHEAT = rgb(245, 222, 179);
#define L_UNINDENTED_WHEAT(args...) L_UNINDENTED(LOG_NOTICE, WHEAT, args)
#define L_WHEAT(args...) L(LOG_NOTICE, WHEAT, args)
#define L_STACKED_WHEAT(args...) L_STACKED(LOG_NOTICE, WHEAT, args)

static constexpr auto WHITE = rgb(255, 255, 255);
#define L_UNINDENTED_WHITE(args...) L_UNINDENTED(LOG_NOTICE, WHITE, args)
#define L_WHITE(args...) L(LOG_NOTICE, WHITE, args)
#define L_STACKED_WHITE(args...) L_STACKED(LOG_NOTICE, WHITE, args)

static constexpr auto WHITE_SMOKE = rgb(245, 245, 245);
#define L_UNINDENTED_WHITE_SMOKE(args...) L_UNINDENTED(LOG_NOTICE, WHITE_SMOKE, args)
#define L_WHITE_SMOKE(args...) L(LOG_NOTICE, WHITE_SMOKE, args)
#define L_STACKED_WHITE_SMOKE(args...) L_STACKED(LOG_NOTICE, WHITE_SMOKE, args)

static constexpr auto YELLOW = rgb(255, 255, 0);
#define L_UNINDENTED_YELLOW(args...) L_UNINDENTED(LOG_NOTICE, YELLOW, args)
#define L_YELLOW(args...) L(LOG_NOTICE, YELLOW, args)
#define L_STACKED_YELLOW(args...) L_STACKED(LOG_NOTICE, YELLOW, args)

static constexpr auto YELLOW_GREEN = rgb(154, 205, 50);
#define L_UNINDENTED_YELLOW_GREEN(args...) L_UNINDENTED(LOG_NOTICE, YELLOW_GREEN, args)
#define L_YELLOW_GREEN(args...) L(LOG_NOTICE, YELLOW_GREEN, args)
#define L_STACKED_YELLOW_GREEN(args...) L_STACKED(LOG_NOTICE, YELLOW_GREEN, args)

#pragma GCC diagnostic pop
