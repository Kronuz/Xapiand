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

constexpr const char RESPONSE_xRANK[]                       = "#rank";
constexpr const char RESPONSE_xWEIGHT[]                     = "#weight";
constexpr const char RESPONSE_xPERCENT[]                    = "#percent";
constexpr const char RESPONSE_xENDPOINT[]                   = "#endpoint";

constexpr const char RESPONSE_xMESSAGE[]                    = "#message";
constexpr const char RESPONSE_xSTATUS[]                     = "#status";

// Reserved words only used in the responses to the user.
constexpr const char RESPONSE_COUNT[]                       = "count";
constexpr const char RESPONSE_DOC_COUNT[]                   = "doc_count";
constexpr const char RESPONSE_MATCHES_ESTIMATED[]           = "matches_estimated";
constexpr const char RESPONSE_AGGREGATIONS[]                = "aggregations";
constexpr const char RESPONSE_HITS[]                        = "hits";

constexpr const char RESPONSE_DOCUMENT_INFO[]               = "document_info";
constexpr const char RESPONSE_DATABASE_INFO[]               = "database_info";

constexpr const char RESPONSE_ENDPOINT[]                    = "endpoint";
constexpr const char RESPONSE_PROCESSED[]                   = "processed";
constexpr const char RESPONSE_INDEXED[]                     = "indexed";
constexpr const char RESPONSE_TOTAL[]                       = "total";
constexpr const char RESPONSE_ITEMS[]                       = "items";

constexpr const char RESPONSE_TOOK[]                        = "took";

// Reserved words only used in the responses to the user.
constexpr const char RESPONSE_UUID[]                        = "uuid";
constexpr const char RESPONSE_REVISION[]                    = "revision";
constexpr const char RESPONSE_LAST_ID[]                     = "last_id";
constexpr const char RESPONSE_DOC_DEL[]                     = "doc_del";
constexpr const char RESPONSE_AV_LENGTH[]                   = "av_length";
constexpr const char RESPONSE_DOC_LEN_LOWER[]               = "doc_len_lower";
constexpr const char RESPONSE_DOC_LEN_UPPER[]               = "doc_len_upper";
constexpr const char RESPONSE_HAS_POSITIONS[]               = "has_positions";

constexpr const char RESPONSE_WDF[]                         = "wdf";
constexpr const char RESPONSE_TERM_FREQ[]                   = "term_freq";
constexpr const char RESPONSE_POS[]                         = "pos";

constexpr const char RESPONSE_DOCID[]                       = "docid";
constexpr const char RESPONSE_DATA[]                        = "data";
constexpr const char RESPONSE_RAW_DATA[]                    = "raw_data";
constexpr const char RESPONSE_TERMS[]                       = "terms";
constexpr const char RESPONSE_VALUES[]                      = "values";

constexpr const char RESPONSE_TYPE[]                        = "type";
constexpr const char RESPONSE_CONTENT_TYPE[]                = "content_type";
constexpr const char RESPONSE_VOLUME[]                      = "volume";
constexpr const char RESPONSE_OFFSET[]                      = "offset";
constexpr const char RESPONSE_SIZE[]                        = "size";

constexpr const char RESPONSE_CLUSTER_NAME[]                = "cluster_name";
constexpr const char RESPONSE_NODES[]                       = "nodes";
constexpr const char RESPONSE_SERVER[]                      = "server";
constexpr const char RESPONSE_URL[]                         = "url";
constexpr const char RESPONSE_VERSIONS[]                    = "versions";
