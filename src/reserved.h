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


#define RESERVED__ "_"
constexpr const char reserved__ = RESERVED__[0];


// Reserved field names.
constexpr const char ID_FIELD_NAME[]                        = RESERVED__ "id";
constexpr const char UUID_FIELD_NAME[]                      = "<uuid_field>";
constexpr const char SCHEMA_FIELD_NAME[]                    = "schema";
constexpr const char SCRIPT_FIELD_NAME[]                    = "script";

constexpr const char RESERVED_DOCID[]                       = RESERVED__ "docid";

// Reserved words used in schema.
constexpr const char RESERVED_VERSION[]                     = RESERVED__ "version";
constexpr const char RESERVED_WEIGHT[]                      = RESERVED__ "weight";
constexpr const char RESERVED_POSITION[]                    = RESERVED__ "position";
constexpr const char RESERVED_SPELLING[]                    = RESERVED__ "spelling";
constexpr const char RESERVED_POSITIONS[]                   = RESERVED__ "positions";
constexpr const char RESERVED_LANGUAGE[]                    = RESERVED__ "language";
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
constexpr const char RESERVED_NAMESPACE[]                   = RESERVED__ "namespace";
constexpr const char RESERVED_PARTIAL_PATHS[]               = RESERVED__ "partial_paths";
constexpr const char RESERVED_INDEX_UUID_FIELD[]            = RESERVED__ "index_uuid_field";
constexpr const char RESERVED_SCHEMA[]                      = RESERVED__ "schema";
constexpr const char RESERVED_ENDPOINT[]                    = RESERVED__ "endpoint";

// Reserved words for detecting types.
constexpr const char RESERVED_DATE_DETECTION[]              = RESERVED__ "date_detection";
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

// Reserved words used for doing explicit cast convertions
constexpr const char RESERVED_FLOAT[]                       = RESERVED__ "float";
constexpr const char RESERVED_POSITIVE[]                    = RESERVED__ "positive";
constexpr const char RESERVED_INTEGER[]                     = RESERVED__ "integer";
constexpr const char RESERVED_BOOLEAN[]                     = RESERVED__ "boolean";
constexpr const char RESERVED_TERM[]                        = RESERVED__ "term";  // FIXME: remove legacy term
constexpr const char RESERVED_KEYWORD[]                     = RESERVED__ "keyword";
constexpr const char RESERVED_TEXT[]                        = RESERVED__ "text";
constexpr const char RESERVED_STRING[]                      = RESERVED__ "string";  // FIXME: remove legacy string
constexpr const char RESERVED_DATE[]                        = RESERVED__ "date";
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

// Reserved for QueryDSL
constexpr const char RESERVED_QUERYDSL_FROM[]               = RESERVED__ "from";
constexpr const char RESERVED_QUERYDSL_IN[]                 = RESERVED__ "in";
constexpr const char RESERVED_QUERYDSL_QUERY[]              = RESERVED__ "query";
constexpr const char RESERVED_QUERYDSL_RANGE[]              = RESERVED__ "range";
constexpr const char RESERVED_QUERYDSL_TO[]                 = RESERVED__ "to";
constexpr const char RESERVED_QUERYDSL_LIMIT[]              = RESERVED__ "limit";
constexpr const char RESERVED_QUERYDSL_CHECK_AT_LEAST[]     = RESERVED__ "check_at_least";
constexpr const char RESERVED_QUERYDSL_OFFSET[]             = RESERVED__ "offset";
constexpr const char RESERVED_QUERYDSL_SORT[]               = RESERVED__ "sort";
constexpr const char RESERVED_QUERYDSL_SELECTOR[]           = RESERVED__ "selector";
constexpr const char RESERVED_QUERYDSL_ORDER[]              = RESERVED__ "order";
constexpr const char RESERVED_QUERYDSL_METRIC[]             = RESERVED__ "metric";
constexpr const char RESERVED_QUERYDSL_PARTIAL[]            = RESERVED__ "partial";

constexpr const char RESERVED_QUERYDSL_AND[]                = RESERVED__ "and";
constexpr const char RESERVED_QUERYDSL_OR[]                 = RESERVED__ "or";
constexpr const char RESERVED_QUERYDSL_NOT[]                = RESERVED__ "not";
constexpr const char RESERVED_QUERYDSL_AND_NOT[]            = RESERVED__ "and_not";
constexpr const char RESERVED_QUERYDSL_XOR[]                = RESERVED__ "xor";
constexpr const char RESERVED_QUERYDSL_AND_MAYBE[]          = RESERVED__ "and_maybe";
constexpr const char RESERVED_QUERYDSL_FILTER[]             = RESERVED__ "filter";
constexpr const char RESERVED_QUERYDSL_NEAR[]               = RESERVED__ "near";
constexpr const char RESERVED_QUERYDSL_PHRASE[]             = RESERVED__ "phrase";
constexpr const char RESERVED_QUERYDSL_SCALE_WEIGHT[]       = RESERVED__ "scale_weight";
constexpr const char RESERVED_QUERYDSL_ELITE_SET[]          = RESERVED__ "elite_set";
constexpr const char RESERVED_QUERYDSL_SYNONYM[]            = RESERVED__ "synonym";
constexpr const char RESERVED_QUERYDSL_MAX[]                = RESERVED__ "max";
constexpr const char RESERVED_QUERYDSL_WILDCARD[]           = RESERVED__ "wildcard";

constexpr const char RESERVED_QUERYDSL_MATCH_ALL[]          = RESERVED__ "match_all";
constexpr const char RESERVED_QUERYDSL_MATCH_NONE[]         = RESERVED__ "match_none";

// Reserved for Geospatial
constexpr const char RESERVED_GEO_LATITUDE[]                = RESERVED__ "latitude";
constexpr const char RESERVED_GEO_LAT[]                     = RESERVED__ "lat";
constexpr const char RESERVED_GEO_LONGITUDE[]               = RESERVED__ "longitude";
constexpr const char RESERVED_GEO_LNG[]                     = RESERVED__ "lng";
constexpr const char RESERVED_GEO_HEIGHT[]                  = RESERVED__ "height";
constexpr const char RESERVED_GEO_RADIUS[]                  = RESERVED__ "radius";
constexpr const char RESERVED_GEO_UNITS[]                   = RESERVED__ "units";
constexpr const char RESERVED_GEO_SRID[]                    = RESERVED__ "srid";

// Reserved for Aggregations
constexpr const char RESERVED_AGGS_AGGS[]                   = RESERVED__ "aggs";
constexpr const char RESERVED_AGGS_AGGREGATIONS[]           = RESERVED__ "aggregations";
constexpr const char RESERVED_AGGS_DOC_COUNT[]              = RESERVED__ "doc_count";
constexpr const char RESERVED_AGGS_FIELD[]                  = RESERVED__ "field";
constexpr const char RESERVED_AGGS_FROM[]                   = RESERVED__ "from";
constexpr const char RESERVED_AGGS_INTERVAL[]               = RESERVED__ "interval";
constexpr const char RESERVED_AGGS_SHIFT[]                  = RESERVED__ "shift";
constexpr const char RESERVED_AGGS_KEY[]                    = RESERVED__ "key";
constexpr const char RESERVED_AGGS_RANGES[]                 = RESERVED__ "ranges";
constexpr const char RESERVED_AGGS_SUM_OF_SQ[]              = RESERVED__ "sum_of_squares";
constexpr const char RESERVED_AGGS_TO[]                     = RESERVED__ "to";

constexpr const char RESERVED_AGGS_AVG[]                    = RESERVED__ "avg";
constexpr const char RESERVED_AGGS_CARDINALITY[]            = RESERVED__ "cardinality";
constexpr const char RESERVED_AGGS_COUNT[]                  = RESERVED__ "count";
constexpr const char RESERVED_AGGS_EXT_STATS[]              = RESERVED__ "extended_stats";
constexpr const char RESERVED_AGGS_GEO_BOUNDS[]             = RESERVED__ "geo_bounds";
constexpr const char RESERVED_AGGS_GEO_CENTROID[]           = RESERVED__ "geo_centroid";
constexpr const char RESERVED_AGGS_MAX[]                    = RESERVED__ "max";
constexpr const char RESERVED_AGGS_MEDIAN[]                 = RESERVED__ "median";
constexpr const char RESERVED_AGGS_MIN[]                    = RESERVED__ "min";
constexpr const char RESERVED_AGGS_MODE[]                   = RESERVED__ "mode";
constexpr const char RESERVED_AGGS_PERCENTILES[]            = RESERVED__ "percentiles";
constexpr const char RESERVED_AGGS_PERCENTILES_RANK[]       = RESERVED__ "percentiles_rank";
constexpr const char RESERVED_AGGS_SCRIPTED_METRIC[]        = RESERVED__ "scripted_metric";
constexpr const char RESERVED_AGGS_STATS[]                  = RESERVED__ "stats";
constexpr const char RESERVED_AGGS_STD[]                    = RESERVED__ "std_deviation";
constexpr const char RESERVED_AGGS_STD_BOUNDS[]             = RESERVED__ "std_deviation_bounds";
constexpr const char RESERVED_AGGS_SUM[]                    = RESERVED__ "sum";
constexpr const char RESERVED_AGGS_VARIANCE[]               = RESERVED__ "variance";

constexpr const char RESERVED_AGGS_DATE_HISTOGRAM[]         = RESERVED__ "date_histogram";
constexpr const char RESERVED_AGGS_DATE_RANGE[]             = RESERVED__ "date_range";
constexpr const char RESERVED_AGGS_FILTER[]                 = RESERVED__ "filter";
constexpr const char RESERVED_AGGS_GEO_DISTANCE[]           = RESERVED__ "geo_distance";
constexpr const char RESERVED_AGGS_GEO_IP[]                 = RESERVED__ "geo_ip";
constexpr const char RESERVED_AGGS_GEO_TRIXELS[]            = RESERVED__ "geo_trixels";
constexpr const char RESERVED_AGGS_HISTOGRAM[]              = RESERVED__ "histogram";
constexpr const char RESERVED_AGGS_IP_RANGE[]               = RESERVED__ "ip_range";
constexpr const char RESERVED_AGGS_MISSING[]                = RESERVED__ "missing";
constexpr const char RESERVED_AGGS_RANGE[]                  = RESERVED__ "range";
constexpr const char RESERVED_AGGS_VALUES[]                 = RESERVED__ "values";
constexpr const char RESERVED_AGGS_TERMS[]                  = RESERVED__ "terms";

constexpr const char RESERVED_AGGS_UPPER[]                  = RESERVED__ "upper";
constexpr const char RESERVED_AGGS_LOWER[]                  = RESERVED__ "lower";
constexpr const char RESERVED_AGGS_SIGMA[]                  = RESERVED__ "sigma";

constexpr const char RESERVED_AGGS_VALUE[]                  = RESERVED__ "_value";
constexpr const char RESERVED_AGGS_TERM[]                   = RESERVED__ "_term";
constexpr const char RESERVED_AGGS_SORT[]                   = RESERVED__ "_sort";
constexpr const char RESERVED_AGGS_ORDER[]                  = RESERVED__ "_order";
constexpr const char RESERVED_AGGS_MIN_DOC_COUNT[]          = RESERVED__ "_min_doc_count";
constexpr const char RESERVED_AGGS_LIMIT[]                  = RESERVED__ "_limit";
constexpr const char RESERVED_AGGS_KEYED[]                  = RESERVED__ "_keyed";

// Reserved for Datetime
constexpr const char RESERVED_YEAR[]                        = RESERVED__ "year";
constexpr const char RESERVED_MONTH[]                       = RESERVED__ "month";
constexpr const char RESERVED_DAY[]                         = RESERVED__ "day";
