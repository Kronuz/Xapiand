/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// The reserved-property dispatch the walks inline: the entry points and the perfect-hash
// tables routing a reserved word to its process, write or feed handler.

#pragma once

#include "database/schema.h"

#include <algorithm>                              // for std::move
#include <cassert>                                // for assert
#include <cmath>                                  // for std::pow
#include <cstdint>                                // for uint64_t
#include <cstring>                                // for size_t, strlen
#include <cctype>                                 // for tolower
#include <functional>                             // for ref, reference_wrapper
#include <limits>                                 // for std::numeric_limits
#include <mutex>                                  // for std::mutex
#include <ostream>                                // for operator<<, basic_ostream
#include <memory>                                 // for std::weak_ptr
#include <set>                                    // for std::set
#include <stdexcept>                              // for std::out_of_range
#include <type_traits>                            // for remove_reference<>::type
#include <unordered_map>                          // for std::unordered_map
#include <utility>

#include "base_x.hh"                              // for Base64
#include "cast.h"                                 // for Cast
#include "cuuid.hh"                                // for UUIDGenerator, cuuid::generate/serialize
#include "database/handler.h"                     // for DatabaseHandler
#include "database/lock.h"                        // for lock_shard
#include "database/shard.h"                       // for Shard
#include "datetime.h"                             // for isDate, isDatetime, tm_t
#include "datetime_msgpack.h"          // for the Datetime MsgPack overloads
#include "exception_engine.h"                     // for ClientError, SerializationError
#include "geospatial/geospatial.h"                // for GeoSpatial
#include "manager.h"                              // for XapiandManager, XapiandMan...
#include "multivalue/generate_terms.h"            // for integer, geo, datetime, positive
#include "opts.h"                                 // for opts::*
#include "random.hh"                              // for random_int
#include "reserved/schema.h"                      // for RESERVED_
#include "script.h"                               // for Script
#include "serialize_list.h"                       // for StringList
#include "split.h"                                // for Split
#include "stopper.h"                              // for stopWordsFingerprint, checkStopWords
#include "static_string.hh"                       // for static_string
#include "xapiand_strings.hh"                             // for strings::format, strings::inplace_lower
#include "database/schema/reserved.h"
#include "database/schema/handlers.h"


using namespace schema_detail;


bool has_dispatch_set_default_spc(uint64_t key);
bool has_dispatch_process_properties(uint64_t key);
bool has_dispatch_process_concrete_properties(uint64_t key);

inline void
Schema::dispatch_process_concrete_properties(const MsgPack& object, Fields& fields, Field** id_field, Field** version_field)
{
	L_CALL("Schema::dispatch_process_concrete_properties({}, <fields>)", object.to_string());

	const auto it_e = object.end();
	for (auto it = object.begin(); it != it_e; ++it) {
		auto str_key = it->str_view();
		auto& value = it.value();
		if (is_reserved(str_key)) {
			auto key = hh(str_key);
			if (!_dispatch_process_concrete_properties(key, str_key, value)) {
				fields.emplace_back(str_key, &value);
				if (id_field != nullptr && key == hh(ID_FIELD_NAME)) {
					*id_field = &fields.back();
				} else if (version_field != nullptr && key == hh(VERSION_FIELD_NAME)) {
					*version_field = &fields.back();
				}
			}
		} else {
			fields.emplace_back(str_key, &value);
		}
	}

#ifdef XAPIAND_LUA
	normalize_script();
#endif
}


inline void
Schema::dispatch_process_all_properties(const MsgPack& object, Fields& fields, Field** id_field, Field** version_field)
{
	L_CALL("Schema::dispatch_process_all_properties({}, <fields>)", object.to_string());

	const auto it_e = object.end();
	for (auto it = object.begin(); it != it_e; ++it) {
		auto str_key = it->str_view();
		auto& value = it.value();
		if (is_reserved(str_key)) {
			auto key = hh(str_key);
			if (!_dispatch_process_properties(key, str_key, value)) {
				if (!_dispatch_process_concrete_properties(key, str_key, value)) {
					fields.emplace_back(str_key, &value);
					if (id_field != nullptr && key == hh(ID_FIELD_NAME)) {
						*id_field = &fields.back();
					} else if (version_field != nullptr && key == hh(VERSION_FIELD_NAME)) {
						*version_field = &fields.back();
					}
				}
			}
		} else {
			fields.emplace_back(str_key, &value);
		}
	}

#ifdef XAPIAND_LUA
	normalize_script();
#endif
}


inline void
Schema::dispatch_process_properties(const MsgPack& object, Fields& fields, Field** id_field, Field** version_field)
{
	if (specification.flags.concrete) {
		dispatch_process_concrete_properties(object, fields, id_field, version_field);
	} else {
		dispatch_process_all_properties(object, fields, id_field, version_field);
	}
}


inline void
Schema::dispatch_write_concrete_properties(MsgPack& mut_properties, const MsgPack& object, Fields& fields, Field** id_field, Field** version_field)
{
	L_CALL("Schema::dispatch_write_concrete_properties({}, {}, <fields>)", mut_properties.to_string(), object.to_string());

	const auto it_e = object.end();
	for (auto it = object.begin(); it != it_e; ++it) {
		auto str_key = it->str_view();
		auto& value = it.value();
		if (is_reserved(str_key)) {
			auto key = hh(str_key);
			if (!_dispatch_write_properties(key, mut_properties, str_key, value)) {
				if (!_dispatch_process_concrete_properties(key, str_key, value)) {
					fields.emplace_back(str_key, &value);
					if (id_field != nullptr && key == hh(ID_FIELD_NAME)) {
						*id_field = &fields.back();
					} else if (version_field != nullptr && key == hh(VERSION_FIELD_NAME)) {
						*version_field = &fields.back();
					}
				}
			}
		} else {
			fields.emplace_back(str_key, &value);
		}
	}

#ifdef XAPIAND_LUA
	write_script(mut_properties);
#endif
}


inline bool
Schema::_dispatch_write_properties(uint64_t key, MsgPack& mut_properties, std::string_view prop_name, const MsgPack& value)
{
	L_CALL("Schema::_dispatch_write_properties({})", mut_properties.to_string());

	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_META),
		hh(RESERVED_WEIGHT),
		hh(RESERVED_POSITION),
		hh(RESERVED_SPELLING),
		hh(RESERVED_POSITIONS),
		hh(RESERVED_INDEX),
		hh(RESERVED_STORE),
		hh(RESERVED_RECURSE),
		hh(RESERVED_IGNORE),
		hh(RESERVED_DYNAMIC),
		hh(RESERVED_STRICT),
		hh(RESERVED_DATE_DETECTION),
		hh(RESERVED_DATETIME_DETECTION),
		hh(RESERVED_TIME_DETECTION),
		hh(RESERVED_TIMEDELTA_DETECTION),
		hh(RESERVED_NUMERIC_DETECTION),
		hh(RESERVED_GEO_DETECTION),
		hh(RESERVED_BOOL_DETECTION),
		hh(RESERVED_TEXT_DETECTION),
		hh(RESERVED_UUID_DETECTION),
		hh(RESERVED_BOOL_TERM),
		hh(RESERVED_NAMESPACE),
		hh(RESERVED_PARTIAL_PATHS),
		hh(RESERVED_INDEX_UUID_FIELD),
		hh(RESERVED_SCHEMA),
		hh(RESERVED_SETTINGS),
	});

	switch (_.find(key)) {
		case _.fhh(RESERVED_META):
			write_meta(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_WEIGHT):
			write_weight(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_POSITION):
			write_position(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_SPELLING):
			write_spelling(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_POSITIONS):
			write_positions(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_INDEX):
			write_index(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_STORE):
			write_store(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_RECURSE):
			write_recurse(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_IGNORE):
			write_ignore(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_DYNAMIC):
			write_dynamic(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_STRICT):
			write_strict(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_DATE_DETECTION):
			write_date_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_DATETIME_DETECTION):
			write_datetime_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_TIME_DETECTION):
			write_time_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_TIMEDELTA_DETECTION):
			write_timedelta_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_NUMERIC_DETECTION):
			write_numeric_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_GEO_DETECTION):
			write_geo_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_BOOL_DETECTION):
			write_bool_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_TEXT_DETECTION):
			write_text_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_UUID_DETECTION):
			write_uuid_detection(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_BOOL_TERM):
			write_bool_term(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_NAMESPACE):
			write_namespace(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_PARTIAL_PATHS):
			write_partial_paths(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_INDEX_UUID_FIELD):
			write_index_uuid_field(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_SCHEMA):
			write_schema(mut_properties, prop_name, value);
			return true;
		case _.fhh(RESERVED_SETTINGS):
			write_settings(mut_properties, prop_name, value);
			return true;
		default:
			return false;
	}
}


inline bool
Schema::_dispatch_feed_properties(uint64_t key, const MsgPack& value)
{
	L_CALL("Schema::_dispatch_feed_properties({})", value.to_string());

	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_META),
		hh(RESERVED_WEIGHT),
		hh(RESERVED_POSITION),
		hh(RESERVED_SPELLING),
		hh(RESERVED_POSITIONS),
		hh(RESERVED_TYPE),
		hh(RESERVED_PREFIX),
		hh(RESERVED_SLOT),
		hh(RESERVED_INDEX),
		hh(RESERVED_STORE),
		hh(RESERVED_RECURSE),
		hh(RESERVED_IGNORE),
		hh(RESERVED_DYNAMIC),
		hh(RESERVED_STRICT),
		hh(RESERVED_DATE_DETECTION),
		hh(RESERVED_DATETIME_DETECTION),
		hh(RESERVED_TIME_DETECTION),
		hh(RESERVED_TIMEDELTA_DETECTION),
		hh(RESERVED_NUMERIC_DETECTION),
		hh(RESERVED_GEO_DETECTION),
		hh(RESERVED_BOOL_DETECTION),
		hh(RESERVED_TEXT_DETECTION),
		hh(RESERVED_UUID_DETECTION),
		hh(RESERVED_BOOL_TERM),
		hh(RESERVED_ACCURACY),
		hh(RESERVED_ACC_PREFIX),
		hh(RESERVED_NGRAM),
		hh(RESERVED_CJK_NGRAM),
		hh(RESERVED_CJK_WORDS),
		hh(RESERVED_LANGUAGE),
		hh(RESERVED_STOP_STRATEGY),
		hh(RESERVED_STEM_STRATEGY),
		hh(RESERVED_STEM_LANGUAGE),
		hh(RESERVED_PARTIALS),
		hh(RESERVED_ERROR),
		hh(RESERVED_NAMESPACE),
		hh(RESERVED_PARTIAL_PATHS),
		hh(RESERVED_INDEX_UUID_FIELD),
		hh(RESERVED_SCRIPT),
		hh(RESERVED_FOREIGN),
	});

	switch (_.find(key)) {
		case _.fhh(RESERVED_META):
			return true;
		case _.fhh(RESERVED_WEIGHT):
			Schema::feed_weight(value);
			return true;
		case _.fhh(RESERVED_POSITION):
			Schema::feed_position(value);
			return true;
		case _.fhh(RESERVED_SPELLING):
			Schema::feed_spelling(value);
			return true;
		case _.fhh(RESERVED_POSITIONS):
			Schema::feed_positions(value);
			return true;
		case _.fhh(RESERVED_TYPE):
			Schema::feed_type(value);
			return true;
		case _.fhh(RESERVED_PREFIX):
			Schema::feed_prefix(value);
			return true;
		case _.fhh(RESERVED_SLOT):
			Schema::feed_slot(value);
			return true;
		case _.fhh(RESERVED_INDEX):
			Schema::feed_index(value);
			return true;
		case _.fhh(RESERVED_STORE):
			Schema::feed_store(value);
			return true;
		case _.fhh(RESERVED_RECURSE):
			Schema::feed_recurse(value);
			return true;
		case _.fhh(RESERVED_IGNORE):
			Schema::feed_ignore(value);
			return true;
		case _.fhh(RESERVED_DYNAMIC):
			Schema::feed_dynamic(value);
			return true;
		case _.fhh(RESERVED_STRICT):
			Schema::feed_strict(value);
			return true;
		case _.fhh(RESERVED_DATE_DETECTION):
			Schema::feed_date_detection(value);
			return true;
		case _.fhh(RESERVED_DATETIME_DETECTION):
			Schema::feed_datetime_detection(value);
			return true;
		case _.fhh(RESERVED_TIME_DETECTION):
			Schema::feed_time_detection(value);
			return true;
		case _.fhh(RESERVED_TIMEDELTA_DETECTION):
			Schema::feed_timedelta_detection(value);
			return true;
		case _.fhh(RESERVED_NUMERIC_DETECTION):
			Schema::feed_numeric_detection(value);
			return true;
		case _.fhh(RESERVED_GEO_DETECTION):
			Schema::feed_geo_detection(value);
			return true;
		case _.fhh(RESERVED_BOOL_DETECTION):
			Schema::feed_bool_detection(value);
			return true;
		case _.fhh(RESERVED_TEXT_DETECTION):
			Schema::feed_text_detection(value);
			return true;
		case _.fhh(RESERVED_UUID_DETECTION):
			Schema::feed_uuid_detection(value);
			return true;
		case _.fhh(RESERVED_BOOL_TERM):
			Schema::feed_bool_term(value);
			return true;
		case _.fhh(RESERVED_ACCURACY):
			Schema::feed_accuracy(value);
			return true;
		case _.fhh(RESERVED_ACC_PREFIX):
			Schema::feed_acc_prefix(value);
			return true;
		case _.fhh(RESERVED_NGRAM):
			Schema::feed_ngram(value);
			return true;
		case _.fhh(RESERVED_CJK_NGRAM):
			Schema::feed_cjk_ngram(value);
			return true;
		case _.fhh(RESERVED_CJK_WORDS):
			Schema::feed_cjk_words(value);
			return true;
		case _.fhh(RESERVED_LANGUAGE):
			Schema::feed_language(value);
			return true;
		case _.fhh(RESERVED_STOP_STRATEGY):
			Schema::feed_stop_strategy(value);
			return true;
		case _.fhh(RESERVED_STEM_STRATEGY):
			Schema::feed_stem_strategy(value);
			return true;
		case _.fhh(RESERVED_STEM_LANGUAGE):
			Schema::feed_stem_language(value);
			return true;
		case _.fhh(RESERVED_PARTIALS):
			Schema::feed_partials(value);
			return true;
		case _.fhh(RESERVED_ERROR):
			Schema::feed_error(value);
			return true;
		case _.fhh(RESERVED_NAMESPACE):
			Schema::feed_namespace(value);
			return true;
		case _.fhh(RESERVED_PARTIAL_PATHS):
			Schema::feed_partial_paths(value);
			return true;
		case _.fhh(RESERVED_INDEX_UUID_FIELD):
			Schema::feed_index_uuid_field(value);
			return true;
		case _.fhh(RESERVED_SCRIPT):
			Schema::feed_script(value);
			return true;
		case _.fhh(RESERVED_FOREIGN):
			Schema::feed_foreign(value);
			return true;
		default:
			return false;
	}
}


inline bool
has_dispatch_process_properties(uint64_t key)
{
	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_NGRAM),
		hh(RESERVED_CJK_NGRAM),
		hh(RESERVED_CJK_WORDS),
		hh(RESERVED_LANGUAGE),
		hh(RESERVED_PREFIX),
		hh(RESERVED_SLOT),
		hh(RESERVED_STOP_STRATEGY),
		hh(RESERVED_STEM_STRATEGY),
		hh(RESERVED_STEM_LANGUAGE),
		hh(RESERVED_TYPE),
		hh(RESERVED_BOOL_TERM),
		hh(RESERVED_ACCURACY),
		hh(RESERVED_ACC_PREFIX),
		hh(RESERVED_PARTIALS),
		hh(RESERVED_ERROR),
	});

	return _.count(key) != 0u;
}

inline bool
Schema::_dispatch_process_properties(uint64_t key, std::string_view prop_name, const MsgPack& value)
{
	L_CALL("Schema::_dispatch_process_properties({})", repr(prop_name));

	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_NGRAM),
		hh(RESERVED_CJK_NGRAM),
		hh(RESERVED_CJK_WORDS),
		hh(RESERVED_LANGUAGE),
		hh(RESERVED_PREFIX),
		hh(RESERVED_SLOT),
		hh(RESERVED_STOP_STRATEGY),
		hh(RESERVED_STEM_STRATEGY),
		hh(RESERVED_STEM_LANGUAGE),
		hh(RESERVED_TYPE),
		hh(RESERVED_BOOL_TERM),
		hh(RESERVED_ACCURACY),
		hh(RESERVED_ACC_PREFIX),
		hh(RESERVED_PARTIALS),
		hh(RESERVED_ERROR),
	});

	switch (_.find(key)) {
		case _.fhh(RESERVED_NGRAM):
			Schema::process_ngram(prop_name, value);
			return true;
		case _.fhh(RESERVED_CJK_NGRAM):
			Schema::process_cjk_ngram(prop_name, value);
			return true;
		case _.fhh(RESERVED_CJK_WORDS):
			Schema::process_cjk_words(prop_name, value);
			return true;
		case _.fhh(RESERVED_LANGUAGE):
			Schema::process_language(prop_name, value);
			return true;
		case _.fhh(RESERVED_PREFIX):
			Schema::process_prefix(prop_name, value);
			return true;
		case _.fhh(RESERVED_SLOT):
			Schema::process_slot(prop_name, value);
			return true;
		case _.fhh(RESERVED_STOP_STRATEGY):
			Schema::process_stop_strategy(prop_name, value);
			return true;
		case _.fhh(RESERVED_STEM_STRATEGY):
			Schema::process_stem_strategy(prop_name, value);
			return true;
		case _.fhh(RESERVED_STEM_LANGUAGE):
			Schema::process_stem_language(prop_name, value);
			return true;
		case _.fhh(RESERVED_TYPE):
			Schema::process_type(prop_name, value);
			return true;
		case _.fhh(RESERVED_BOOL_TERM):
			Schema::process_bool_term(prop_name, value);
			return true;
		case _.fhh(RESERVED_ACCURACY):
			Schema::process_accuracy(prop_name, value);
			return true;
		case _.fhh(RESERVED_ACC_PREFIX):
			Schema::process_acc_prefix(prop_name, value);
			return true;
		case _.fhh(RESERVED_PARTIALS):
			Schema::process_partials(prop_name, value);
			return true;
		case _.fhh(RESERVED_ERROR):
			Schema::process_error(prop_name, value);
			return true;
		default:
			return false;
	}
}

inline bool
has_dispatch_process_concrete_properties(uint64_t key)
{
	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_META),
		hh(RESERVED_DATA),
		hh(RESERVED_WEIGHT),
		hh(RESERVED_POSITION),
		hh(RESERVED_SPELLING),
		hh(RESERVED_POSITIONS),
		hh(RESERVED_INDEX),
		hh(RESERVED_STORE),
		hh(RESERVED_RECURSE),
		hh(RESERVED_IGNORE),
		hh(RESERVED_PARTIAL_PATHS),
		hh(RESERVED_INDEX_UUID_FIELD),
		hh(RESERVED_VALUE),
		hh(RESERVED_FOREIGN),
		hh(RESERVED_SCRIPT),
		hh(RESERVED_FLOAT),
		hh(RESERVED_POSITIVE),
		hh(RESERVED_INTEGER),
		hh(RESERVED_BOOLEAN),
		hh(RESERVED_TERM),  // FIXME: remove legacy term
		hh(RESERVED_KEYWORD),
		hh(RESERVED_TEXT),
		hh(RESERVED_STRING),
		hh(RESERVED_DATE),
		hh(RESERVED_DATETIME),
		hh(RESERVED_UUID),
		hh(RESERVED_EWKT),
		hh(RESERVED_POINT),
		hh(RESERVED_CIRCLE),
		hh(RESERVED_CONVEX),
		hh(RESERVED_POLYGON),
		hh(RESERVED_CHULL),
		hh(RESERVED_MULTIPOINT),
		hh(RESERVED_MULTICIRCLE),
		hh(RESERVED_MULTICONVEX),
		hh(RESERVED_MULTIPOLYGON),
		hh(RESERVED_MULTICHULL),
		hh(RESERVED_GEO_COLLECTION),
		hh(RESERVED_GEO_INTERSECTION),
		hh(RESERVED_LUA),
		// Next functions only check the consistency of user provided data.
		hh(RESERVED_SLOT),
		hh(RESERVED_NGRAM),
		hh(RESERVED_CJK_NGRAM),
		hh(RESERVED_CJK_WORDS),
		hh(RESERVED_LANGUAGE),
		hh(RESERVED_STOP_STRATEGY),
		hh(RESERVED_STEM_STRATEGY),
		hh(RESERVED_STEM_LANGUAGE),
		hh(RESERVED_TYPE),
		hh(RESERVED_BOOL_TERM),
		hh(RESERVED_ACCURACY),
		hh(RESERVED_PARTIALS),
		hh(RESERVED_ERROR),
		hh(RESERVED_DYNAMIC),
		hh(RESERVED_STRICT),
		hh(RESERVED_DATE_DETECTION),
		hh(RESERVED_DATETIME_DETECTION),
		hh(RESERVED_TIME_DETECTION),
		hh(RESERVED_TIMEDELTA_DETECTION),
		hh(RESERVED_NUMERIC_DETECTION),
		hh(RESERVED_GEO_DETECTION),
		hh(RESERVED_BOOL_DETECTION),
		hh(RESERVED_TEXT_DETECTION),
		hh(RESERVED_UUID_DETECTION),
		hh(RESERVED_NAMESPACE),
		hh(RESERVED_SCHEMA),
		hh(RESERVED_SETTINGS),
	});
	return _.count(key) != 0u;
}

inline bool
Schema::_dispatch_process_concrete_properties(uint64_t key, std::string_view prop_name, const MsgPack& value)
{
	L_CALL("Schema::_dispatch_process_concrete_properties({})", repr(prop_name));

	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_META),
		hh(RESERVED_DATA),
		hh(RESERVED_WEIGHT),
		hh(RESERVED_POSITION),
		hh(RESERVED_SPELLING),
		hh(RESERVED_POSITIONS),
		hh(RESERVED_INDEX),
		hh(RESERVED_STORE),
		hh(RESERVED_RECURSE),
		hh(RESERVED_IGNORE),
		hh(RESERVED_PARTIAL_PATHS),
		hh(RESERVED_INDEX_UUID_FIELD),
		hh(RESERVED_VALUE),
		hh(RESERVED_FOREIGN),
		hh(RESERVED_SCRIPT),
		hh(RESERVED_FLOAT),
		hh(RESERVED_POSITIVE),
		hh(RESERVED_INTEGER),
		hh(RESERVED_BOOLEAN),
		hh(RESERVED_TERM),  // FIXME: remove legacy term
		hh(RESERVED_KEYWORD),
		hh(RESERVED_TEXT),
		hh(RESERVED_STRING),
		hh(RESERVED_DATE),
		hh(RESERVED_DATETIME),
		hh(RESERVED_UUID),
		hh(RESERVED_EWKT),
		hh(RESERVED_POINT),
		hh(RESERVED_CIRCLE),
		hh(RESERVED_CONVEX),
		hh(RESERVED_POLYGON),
		hh(RESERVED_CHULL),
		hh(RESERVED_MULTIPOINT),
		hh(RESERVED_MULTICIRCLE),
		hh(RESERVED_MULTICONVEX),
		hh(RESERVED_MULTIPOLYGON),
		hh(RESERVED_MULTICHULL),
		hh(RESERVED_GEO_COLLECTION),
		hh(RESERVED_GEO_INTERSECTION),
		hh(RESERVED_LUA),
		// Next functions only check the consistency of user provided data.
		hh(RESERVED_SLOT),
		hh(RESERVED_NGRAM),
		hh(RESERVED_CJK_NGRAM),
		hh(RESERVED_CJK_WORDS),
		hh(RESERVED_LANGUAGE),
		hh(RESERVED_STOP_STRATEGY),
		hh(RESERVED_STEM_STRATEGY),
		hh(RESERVED_STEM_LANGUAGE),
		hh(RESERVED_TYPE),
		hh(RESERVED_BOOL_TERM),
		hh(RESERVED_ACCURACY),
		hh(RESERVED_PARTIALS),
		hh(RESERVED_ERROR),
		hh(RESERVED_DYNAMIC),
		hh(RESERVED_STRICT),
		hh(RESERVED_DATE_DETECTION),
		hh(RESERVED_DATETIME_DETECTION),
		hh(RESERVED_TIME_DETECTION),
		hh(RESERVED_TIMEDELTA_DETECTION),
		hh(RESERVED_NUMERIC_DETECTION),
		hh(RESERVED_GEO_DETECTION),
		hh(RESERVED_BOOL_DETECTION),
		hh(RESERVED_TEXT_DETECTION),
		hh(RESERVED_UUID_DETECTION),
		hh(RESERVED_NAMESPACE),
		hh(RESERVED_SCHEMA),
		hh(RESERVED_SETTINGS),
	});

	switch (_.find(key)) {
		case _.fhh(RESERVED_META):
			Schema::process_meta(prop_name, value);
			return true;
		case _.fhh(RESERVED_DATA):
			Schema::process_data(prop_name, value);
			return true;
		case _.fhh(RESERVED_WEIGHT):
			Schema::process_weight(prop_name, value);
			return true;
		case _.fhh(RESERVED_POSITION):
			Schema::process_position(prop_name, value);
			return true;
		case _.fhh(RESERVED_SPELLING):
			Schema::process_spelling(prop_name, value);
			return true;
		case _.fhh(RESERVED_POSITIONS):
			Schema::process_positions(prop_name, value);
			return true;
		case _.fhh(RESERVED_INDEX):
			Schema::process_index(prop_name, value);
			return true;
		case _.fhh(RESERVED_STORE):
			Schema::process_store(prop_name, value);
			return true;
		case _.fhh(RESERVED_RECURSE):
			Schema::process_recurse(prop_name, value);
			return true;
		case _.fhh(RESERVED_IGNORE):
			Schema::process_ignore(prop_name, value);
			return true;
		case _.fhh(RESERVED_PARTIAL_PATHS):
			Schema::process_partial_paths(prop_name, value);
			return true;
		case _.fhh(RESERVED_INDEX_UUID_FIELD):
			Schema::process_index_uuid_field(prop_name, value);
			return true;
		case _.fhh(RESERVED_VALUE):
			Schema::process_value(prop_name, value);
			return true;
		case _.fhh(RESERVED_FOREIGN):
			Schema::process_foreign(prop_name, value);
			return true;
		case _.fhh(RESERVED_STRICT):
			Schema::process_strict(prop_name, value);
			return true;
		case _.fhh(RESERVED_SCRIPT):
			Schema::process_script(prop_name, value);
			return true;
		case _.fhh(RESERVED_FLOAT):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_POSITIVE):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_INTEGER):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_BOOLEAN):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_TERM):  // FIXME: remove legacy term
		case _.fhh(RESERVED_KEYWORD):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_TEXT):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_STRING):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_DATE):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_DATETIME):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_UUID):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_EWKT):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_POINT):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_CIRCLE):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_CONVEX):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_POLYGON):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_CHULL):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_MULTIPOINT):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_MULTICIRCLE):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_MULTICONVEX):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_MULTIPOLYGON):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_MULTICHULL):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_GEO_COLLECTION):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_GEO_INTERSECTION):
			Schema::process_cast_object(prop_name, value);
			return true;
		case _.fhh(RESERVED_LUA):
			Schema::process_cast_object(prop_name, value);
			return true;
		// Next functions only check the consistency of user provided data.
		case _.fhh(RESERVED_SLOT):
			Schema::consistency_slot(prop_name, value);
			return true;
		case _.fhh(RESERVED_NGRAM):
			Schema::consistency_ngram(prop_name, value);
			return true;
		case _.fhh(RESERVED_CJK_NGRAM):
			Schema::consistency_cjk_ngram(prop_name, value);
			return true;
		case _.fhh(RESERVED_CJK_WORDS):
			Schema::consistency_cjk_words(prop_name, value);
			return true;
		case _.fhh(RESERVED_LANGUAGE):
			Schema::consistency_language(prop_name, value);
			return true;
		case _.fhh(RESERVED_STOP_STRATEGY):
			Schema::consistency_stop_strategy(prop_name, value);
			return true;
		case _.fhh(RESERVED_STEM_STRATEGY):
			Schema::consistency_stem_strategy(prop_name, value);
			return true;
		case _.fhh(RESERVED_STEM_LANGUAGE):
			Schema::consistency_stem_language(prop_name, value);
			return true;
		case _.fhh(RESERVED_TYPE):
			Schema::consistency_type(prop_name, value);
			return true;
		case _.fhh(RESERVED_BOOL_TERM):
			Schema::consistency_bool_term(prop_name, value);
			return true;
		case _.fhh(RESERVED_ACCURACY):
			Schema::consistency_accuracy(prop_name, value);
			return true;
		case _.fhh(RESERVED_PARTIALS):
			Schema::consistency_partials(prop_name, value);
			return true;
		case _.fhh(RESERVED_ERROR):
			Schema::consistency_error(prop_name, value);
			return true;
		case _.fhh(RESERVED_DYNAMIC):
			Schema::consistency_dynamic(prop_name, value);
			return true;
		case _.fhh(RESERVED_DATE_DETECTION):
			Schema::consistency_date_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_DATETIME_DETECTION):
			Schema::consistency_datetime_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_TIME_DETECTION):
			Schema::consistency_time_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_TIMEDELTA_DETECTION):
			Schema::consistency_timedelta_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_NUMERIC_DETECTION):
			Schema::consistency_numeric_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_GEO_DETECTION):
			Schema::consistency_geo_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_BOOL_DETECTION):
			Schema::consistency_bool_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_TEXT_DETECTION):
			Schema::consistency_text_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_UUID_DETECTION):
			Schema::consistency_uuid_detection(prop_name, value);
			return true;
		case _.fhh(RESERVED_NAMESPACE):
			Schema::consistency_namespace(prop_name, value);
			return true;
		case _.fhh(RESERVED_SCHEMA):
			Schema::consistency_schema(prop_name, value);
			return true;
		case _.fhh(RESERVED_SETTINGS):
			Schema::consistency_settings(prop_name, value);
			return true;
		default:
			return false;
	}
}


inline void
Schema::dispatch_write_properties(MsgPack& mut_properties, const MsgPack& object, Fields& fields, Field** id_field, Field** version_field)
{
	L_CALL("Schema::dispatch_write_properties({}, <object>, <fields>)", mut_properties.to_string());

	if (specification.flags.concrete) {
		dispatch_write_concrete_properties(mut_properties, object, fields, id_field, version_field);
	} else {
		dispatch_write_all_properties(mut_properties, object, fields, id_field, version_field);
	}
}
