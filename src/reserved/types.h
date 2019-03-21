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

#include "reserved/reserved.h"

constexpr const char RESERVED_FLOAT[]                       = RESERVED__ "float";
constexpr const char RESERVED_POSITIVE[]                    = RESERVED__ "positive";
constexpr const char RESERVED_INTEGER[]                     = RESERVED__ "integer";
constexpr const char RESERVED_BOOLEAN[]                     = RESERVED__ "boolean";
constexpr const char RESERVED_TERM[]                        = RESERVED__ "term";  // FIXME: remove legacy term
constexpr const char RESERVED_KEYWORD[]                     = RESERVED__ "keyword";
constexpr const char RESERVED_TEXT[]                        = RESERVED__ "text";
constexpr const char RESERVED_STRING[]                      = RESERVED__ "string";  // FIXME: remove legacy string
constexpr const char RESERVED_DATE[]                        = RESERVED__ "date";
constexpr const char RESERVED_DATETIME[]                    = RESERVED__ "datetime";
constexpr const char RESERVED_TIME[]                        = RESERVED__ "time";
constexpr const char RESERVED_TIMEDELTA[]                   = RESERVED__ "timedelta";
constexpr const char RESERVED_UUID[]                        = RESERVED__ "uuid";
constexpr const char RESERVED_EWKT[]                        = RESERVED__ "ewkt";
constexpr const char RESERVED_POINT[]                       = RESERVED__ "point";
constexpr const char RESERVED_CIRCLE[]                      = RESERVED__ "circle";
constexpr const char RESERVED_CONVEX[]                      = RESERVED__ "convex";
constexpr const char RESERVED_POLYGON[]                     = RESERVED__ "polygon";
constexpr const char RESERVED_CHULL[]                       = RESERVED__ "chull";
constexpr const char RESERVED_MULTIPOINT[]                  = RESERVED__ "multipoint";
constexpr const char RESERVED_MULTICIRCLE[]                 = RESERVED__ "multicircle";
constexpr const char RESERVED_MULTICONVEX[]                 = RESERVED__ "multiconvex";
constexpr const char RESERVED_MULTIPOLYGON[]                = RESERVED__ "multipolygon";
constexpr const char RESERVED_MULTICHULL[]                  = RESERVED__ "multichull";
constexpr const char RESERVED_GEO_COLLECTION[]              = RESERVED__ "geometrycollection";
constexpr const char RESERVED_GEO_INTERSECTION[]            = RESERVED__ "geometryintersection";
