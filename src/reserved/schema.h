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

constexpr const char RESERVED_META[]                        = RESERVED__ "meta";
constexpr const char RESERVED_WEIGHT[]                      = RESERVED__ "weight";
constexpr const char RESERVED_POSITION[]                    = RESERVED__ "position";
constexpr const char RESERVED_SPELLING[]                    = RESERVED__ "spelling";
constexpr const char RESERVED_POSITIONS[]                   = RESERVED__ "positions";
constexpr const char RESERVED_LANGUAGE[]                    = RESERVED__ "language";
constexpr const char RESERVED_NGRAM[]                       = RESERVED__ "ngram";
constexpr const char RESERVED_CJK_NGRAM[]                   = RESERVED__ "cjk_ngram";
constexpr const char RESERVED_CJK_WORDS[]                   = RESERVED__ "cjk_words";
constexpr const char RESERVED_ACCURACY[]                    = RESERVED__ "accuracy";
constexpr const char RESERVED_ACC_PREFIX[]                  = RESERVED__ "accuracy_prefix";
constexpr const char RESERVED_STORE[]                       = RESERVED__ "store";
constexpr const char RESERVED_TYPE[]                        = RESERVED__ "type";
constexpr const char RESERVED_DYNAMIC[]                     = RESERVED__ "dynamic";
constexpr const char RESERVED_STRICT[]                      = RESERVED__ "strict";
constexpr const char RESERVED_BOOL_TERM[]                   = RESERVED__ "bool_term";
constexpr const char RESERVED_VALUE[]                       = RESERVED__ "value";
constexpr const char RESERVED_SLOT[]                        = RESERVED__ "slot";
constexpr const char RESERVED_INDEX[]                       = RESERVED__ "index";
constexpr const char RESERVED_PREFIX[]                      = RESERVED__ "prefix";
constexpr const char RESERVED_CHAI[]                        = RESERVED__ "chai";
constexpr const char RESERVED_SCRIPT[]                      = RESERVED__ "script";
constexpr const char RESERVED_NAME[]                        = RESERVED__ "name";
constexpr const char RESERVED_BODY[]                        = RESERVED__ "body";
constexpr const char RESERVED_PARAMS[]                      = RESERVED__ "params";
constexpr const char RESERVED_RECURSE[]                     = RESERVED__ "recurse";
constexpr const char RESERVED_IGNORE[]                      = RESERVED__ "ignore";
constexpr const char RESERVED_NAMESPACE[]                   = RESERVED__ "namespace";
constexpr const char RESERVED_PARTIAL_PATHS[]               = RESERVED__ "partial_paths";
constexpr const char RESERVED_INDEX_UUID_FIELD[]            = RESERVED__ "index_uuid_field";
constexpr const char RESERVED_SCHEMA[]                      = RESERVED__ "schema";
constexpr const char RESERVED_SETTINGS[]                    = RESERVED__ "settings";
constexpr const char RESERVED_FOREIGN[]                     = RESERVED__ "foreign";

// Reserved for bulk operations
constexpr const char RESERVED_OP_TYPE[]                     = RESERVED__ "op_type";

// Reserved words for detecting types.
constexpr const char RESERVED_DATE_DETECTION[]              = RESERVED__ "date_detection";
constexpr const char RESERVED_DATETIME_DETECTION[]          = RESERVED__ "datetime_detection";
constexpr const char RESERVED_TIME_DETECTION[]              = RESERVED__ "time_detection";
constexpr const char RESERVED_TIMEDELTA_DETECTION[]         = RESERVED__ "timedelta_detection";
constexpr const char RESERVED_NUMERIC_DETECTION[]           = RESERVED__ "numeric_detection";
constexpr const char RESERVED_GEO_DETECTION[]               = RESERVED__ "geo_detection";
constexpr const char RESERVED_BOOL_DETECTION[]              = RESERVED__ "bool_detection";
constexpr const char RESERVED_TEXT_DETECTION[]              = RESERVED__ "text_detection";
constexpr const char RESERVED_TERM_DETECTION[]              = RESERVED__ "term_detection";
constexpr const char RESERVED_UUID_DETECTION[]              = RESERVED__ "uuid_detection";

// Reserved words used only in the root of the schema.
constexpr const char RESERVED_VALUES[]                      = RESERVED__ "values";
constexpr const char RESERVED_TERMS[]                       = RESERVED__ "terms";
constexpr const char RESERVED_DATA[]                        = RESERVED__ "data";
constexpr const char RESERVED_BLOB[]                        = RESERVED__ "blob";
constexpr const char RESERVED_CONTENT_TYPE[]                = RESERVED__ "content_type";

// Reserved words used in schema only for TEXT fields.
constexpr const char RESERVED_STOP_STRATEGY[]               = RESERVED__ "stop_strategy";
constexpr const char RESERVED_STEM_STRATEGY[]               = RESERVED__ "stem_strategy";
constexpr const char RESERVED_STEM_LANGUAGE[]               = RESERVED__ "stem_language";

// Reserved words used in schema only for GEO fields.
constexpr const char RESERVED_PARTIALS[]                    = RESERVED__ "partials";
constexpr const char RESERVED_ERROR[]                       = RESERVED__ "error";
