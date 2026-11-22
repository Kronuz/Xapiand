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

// The reserved-word vocabulary the schema pieces share: type-name strings, the parsers and
// printers of reserved values, and the acceptable-value lists and default accuracies
// (specification.cc defines them).

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


constexpr static auto EMPTY      = static_string::string(EMPTY_CHAR);
constexpr static auto TIMEDELTA  = static_string::string(TIMEDELTA_CHAR);
constexpr static auto ARRAY      = static_string::string(ARRAY_CHAR);
constexpr static auto BOOLEAN    = static_string::string(BOOLEAN_CHAR);
constexpr static auto DATE       = static_string::string(DATE_CHAR);
constexpr static auto DATETIME   = static_string::string(DATETIME_CHAR);
constexpr static auto FOREIGN    = static_string::string(FOREIGN_CHAR);
constexpr static auto FLOATING   = static_string::string(FLOATING_CHAR);
constexpr static auto GEO        = static_string::string(GEO_CHAR);
constexpr static auto INTEGER    = static_string::string(INTEGER_CHAR);
constexpr static auto OBJECT     = static_string::string(OBJECT_CHAR);
constexpr static auto POSITIVE   = static_string::string(POSITIVE_CHAR);
constexpr static auto TEXT       = static_string::string(TEXT_CHAR);
constexpr static auto KEYWORD    = static_string::string(KEYWORD_CHAR);
constexpr static auto UUID       = static_string::string(UUID_CHAR);
constexpr static auto SCRIPT     = static_string::string(SCRIPT_CHAR);
constexpr static auto TIME       = static_string::string(TIME_CHAR);

namespace schema_detail {


// The tables specification.cc defines.
extern const std::vector<uint64_t> def_accuracy_num;
extern const std::vector<uint64_t> def_accuracy_datetime;
extern const std::vector<uint64_t> def_accuracy_time;
extern const std::vector<uint64_t> def_accuracy_geo;
extern const std::string str_set_acc_date;
extern const std::string str_set_acc_time;
extern const std::string str_set_stop_strategy;
extern const std::string str_set_stem_strategy;
extern const std::string str_set_index_uuid_field;
extern const std::string str_set_index;



inline bool
validate_acc_date(UnitTime unit) noexcept
{
	switch (unit) {
		case UnitTime::second:
		case UnitTime::minute:
		case UnitTime::hour:
		case UnitTime::day:
		case UnitTime::month:
		case UnitTime::year:
		case UnitTime::decade:
		case UnitTime::century:
		case UnitTime::millennium:
			return true;
		default:
			return false;
	}
}


/*
 * Helper functions to print readable form of enums
 */

inline std::string_view
_get_str_acc_date(UnitTime unit) noexcept
{
	switch (unit) {
		case UnitTime::second:
			return std::string_view("second");
		case UnitTime::minute:
			return std::string_view("minute");
		case UnitTime::hour:
			return std::string_view("hour");
		case UnitTime::day:
			return std::string_view("day");
		case UnitTime::month:
			return std::string_view("month");
		case UnitTime::year:
			return std::string_view("year");
		case UnitTime::decade:
			return std::string_view("decade");
		case UnitTime::century:
			return std::string_view("century");
		case UnitTime::millennium:
			return std::string_view("millennium");
		default:
			return std::string_view("unknown");
	}
}


inline std::string_view
_get_str_index(TypeIndex index) noexcept
{
	switch (index) {
		case TypeIndex::NONE:
			return std::string_view("none");
		case TypeIndex::FIELD_TERMS:
			return std::string_view("field_terms");
		case TypeIndex::FIELD_VALUES:
			return std::string_view("field_values");
		case TypeIndex::FIELD_ALL:
			return std::string_view("field");
		case TypeIndex::GLOBAL_TERMS:
			return std::string_view("global_terms");
		case TypeIndex::TERMS:
			return std::string_view("terms");
		case TypeIndex::GLOBAL_TERMS_FIELD_VALUES:
			return std::string_view("global_terms,field_values");
		case TypeIndex::GLOBAL_TERMS_FIELD_ALL:
			return std::string_view("global_terms,field");
		case TypeIndex::GLOBAL_VALUES:
			return std::string_view("global_values");
		case TypeIndex::GLOBAL_VALUES_FIELD_TERMS:
			return std::string_view("global_values,field_terms");
		case TypeIndex::VALUES:
			return std::string_view("values");
		case TypeIndex::GLOBAL_VALUES_FIELD_ALL:
			return std::string_view("global_values,field");
		case TypeIndex::GLOBAL_ALL:
			return std::string_view("global");
		case TypeIndex::GLOBAL_ALL_FIELD_TERMS:
			return std::string_view("global,field_terms");
		case TypeIndex::GLOBAL_ALL_FIELD_VALUES:
			return std::string_view("global,field_values");
		case TypeIndex::ALL:
			return std::string_view("all");
		default:
			return std::string_view("unknown");
	}
}


inline UnitTime
_get_accuracy_datetime(std::string_view str_accuracy_datetime)
{
	constexpr static auto _ = phf::make_phf({
		hhl("second"),
		hhl("minute"),
		hhl("hour"),
		hhl("day"),
		hhl("month"),
		hhl("year"),
		hhl("decade"),
		hhl("century"),
		hhl("millennium"),
	});

	switch (_.fhhl(str_accuracy_datetime)) {
		case _.fhhl("second"):
			return UnitTime::second;
		case _.fhhl("minute"):
			return UnitTime::minute;
		case _.fhhl("hour"):
			return UnitTime::hour;
		case _.fhhl("day"):
			return UnitTime::day;
		case _.fhhl("month"):
			return UnitTime::month;
		case _.fhhl("year"):
			return UnitTime::year;
		case _.fhhl("decade"):
			return UnitTime::decade;
		case _.fhhl("century"):
			return UnitTime::century;
		case _.fhhl("millennium"):
			return UnitTime::millennium;
		default:
			return UnitTime::INVALID;
	}
}


inline UnitTime
_get_accuracy_time(std::string_view str_accuracy_time)
{
	constexpr static auto _ = phf::make_phf({
		hhl("second"),
		hhl("minute"),
		hhl("hour"),
	});

	switch (_.fhhl(str_accuracy_time)) {
		case _.fhhl("second"):
			return UnitTime::second;
		case _.fhhl("minute"):
			return UnitTime::minute;
		case _.fhhl("hour"):
			return UnitTime::hour;
		default:
			return UnitTime::INVALID;
	}
}


inline StopStrategy
_get_stop_strategy(std::string_view str_stop_strategy)
{
	constexpr static auto _ = phf::make_phf({
		hhl("stop_none"),
		hhl("none"),
		hhl("stop_all"),
		hhl("all"),
		hhl("stop_stemmed"),
		hhl("stemmed"),
	});

	switch (_.fhhl(str_stop_strategy)) {
		case _.fhhl("stop_none"):
			return StopStrategy::stop_none;
		case _.fhhl("none"):
			return StopStrategy::stop_none;
		case _.fhhl("stop_all"):
			return StopStrategy::stop_all;
		case _.fhhl("all"):
			return StopStrategy::stop_all;
		case _.fhhl("stop_stemmed"):
			return StopStrategy::stop_stemmed;
		case _.fhhl("stemmed"):
			return StopStrategy::stop_stemmed;
		default:
			return StopStrategy::INVALID;
	}
}


inline UUIDFieldIndex
_get_index_uuid_field(std::string_view str_index_uuid_field)
{
	constexpr static auto _ = phf::make_phf({
		hhl("uuid"),
		hhl("uuid_field"),
		hhl("both"),
	});

	switch (_.fhhl(str_index_uuid_field)) {
		case _.fhhl("uuid"):
			return UUIDFieldIndex::uuid;
		case _.fhhl("uuid_field"):
			return UUIDFieldIndex::uuid_field;
		case _.fhhl("both"):
			return UUIDFieldIndex::both;
		default:
			return UUIDFieldIndex::INVALID;
	}
}



inline TypeIndex
_get_index(std::string_view str_index)
{
	constexpr static auto _ = phf::make_phf({
		hhl("none"),
		hhl("field_terms"),
		hhl("field_values"),
		hhl("field_terms,field_values"),
		hhl("field_values,field_terms"),
		hhl("field"),
		hhl("field_all"),
		hhl("global_terms"),
		hhl("field_terms,global_terms"),
		hhl("global_terms,field_terms"),
		hhl("terms"),
		hhl("global_terms,field_values"),
		hhl("field_values,global_terms"),
		hhl("global_terms,field"),
		hhl("global_terms,field_all"),
		hhl("field,global_terms"),
		hhl("field_all,global_terms"),
		hhl("global_values"),
		hhl("global_values,field_terms"),
		hhl("field_terms,global_values"),
		hhl("field_values,global_values"),
		hhl("global_values,field_values"),
		hhl("values"),
		hhl("global_values,field"),
		hhl("global_values,field_all"),
		hhl("field,global_values"),
		hhl("field_all,global_values"),
		hhl("global"),
		hhl("global_all"),
		hhl("global_values,global_terms"),
		hhl("global_terms,global_values"),
		hhl("global,field_terms"),
		hhl("global_all,field_terms"),
		hhl("field_terms,global"),
		hhl("field_terms,global_all"),
		hhl("global_all,field_values"),
		hhl("global,field_values"),
		hhl("field_values,global"),
		hhl("field_values,global_all"),
		hhl("field_all,global_all"),
		hhl("global_all,field_all"),
		hhl("all"),
	});

	switch(_.fhhl(str_index)) {
		case _.fhhl("none"):
			return TypeIndex::NONE;
		case _.fhhl("field_terms"):
			return TypeIndex::FIELD_TERMS;
		case _.fhhl("field_values"):
			return TypeIndex::FIELD_VALUES;
		case _.fhhl("field_terms,field_values"):
			return TypeIndex::FIELD_ALL;
		case _.fhhl("field_values,field_terms"):
			return TypeIndex::FIELD_ALL;
		case _.fhhl("field"):
			return TypeIndex::FIELD_ALL;
		case _.fhhl("field_all"):
			return TypeIndex::FIELD_ALL;
		case _.fhhl("global_terms"):
			return TypeIndex::GLOBAL_TERMS;
		case _.fhhl("field_terms,global_terms"):
			return TypeIndex::TERMS;
		case _.fhhl("global_terms,field_terms"):
			return TypeIndex::TERMS;
		case _.fhhl("terms"):
			return TypeIndex::TERMS;
		case _.fhhl("global_terms,field_values"):
			return TypeIndex::GLOBAL_TERMS_FIELD_VALUES;
		case _.fhhl("field_values,global_terms"):
			return TypeIndex::GLOBAL_TERMS_FIELD_VALUES;
		case _.fhhl("global_terms,field"):
			return TypeIndex::GLOBAL_TERMS_FIELD_ALL;
		case _.fhhl("global_terms,field_all"):
			return TypeIndex::GLOBAL_TERMS_FIELD_ALL;
		case _.fhhl("field,global_terms"):
			return TypeIndex::GLOBAL_TERMS_FIELD_ALL;
		case _.fhhl("field_all,global_terms"):
			return TypeIndex::GLOBAL_TERMS_FIELD_ALL;
		case _.fhhl("global_values"):
			return TypeIndex::GLOBAL_VALUES;
		case _.fhhl("global_values,field_terms"):
			return TypeIndex::GLOBAL_VALUES_FIELD_TERMS;
		case _.fhhl("field_terms,global_values"):
			return TypeIndex::GLOBAL_VALUES_FIELD_TERMS;
		case _.fhhl("field_values,global_values"):
			return TypeIndex::VALUES;
		case _.fhhl("global_values,field_values"):
			return TypeIndex::VALUES;
		case _.fhhl("values"):
			return TypeIndex::VALUES;
		case _.fhhl("global_values,field"):
			return TypeIndex::GLOBAL_VALUES_FIELD_ALL;
		case _.fhhl("global_values,field_all"):
			return TypeIndex::GLOBAL_VALUES_FIELD_ALL;
		case _.fhhl("field,global_values"):
			return TypeIndex::GLOBAL_VALUES_FIELD_ALL;
		case _.fhhl("field_all,global_values"):
			return TypeIndex::GLOBAL_VALUES_FIELD_ALL;
		case _.fhhl("global"):
			return TypeIndex::GLOBAL_ALL;
		case _.fhhl("global_all"):
			return TypeIndex::GLOBAL_ALL;
		case _.fhhl("global_values,global_terms"):
			return TypeIndex::GLOBAL_ALL;
		case _.fhhl("global_terms,global_values"):
			return TypeIndex::GLOBAL_ALL;
		case _.fhhl("global,field_terms"):
			return TypeIndex::GLOBAL_ALL_FIELD_TERMS;
		case _.fhhl("global_all,field_terms"):
			return TypeIndex::GLOBAL_ALL_FIELD_TERMS;
		case _.fhhl("field_terms,global"):
			return TypeIndex::GLOBAL_ALL_FIELD_TERMS;
		case _.fhhl("field_terms,global_all"):
			return TypeIndex::GLOBAL_ALL_FIELD_TERMS;
		case _.fhhl("global_all,field_values"):
			return TypeIndex::GLOBAL_ALL_FIELD_VALUES;
		case _.fhhl("global,field_values"):
			return TypeIndex::GLOBAL_ALL_FIELD_VALUES;
		case _.fhhl("field_values,global"):
			return TypeIndex::GLOBAL_ALL_FIELD_VALUES;
		case _.fhhl("field_values,global_all"):
			return TypeIndex::GLOBAL_ALL_FIELD_VALUES;
		case _.fhhl("field_all,global_all"):
			return TypeIndex::ALL;
		case _.fhhl("global_all,field_all"):
			return TypeIndex::ALL;
		case _.fhhl("all"):
			return TypeIndex::ALL;
		default:
			return TypeIndex::INVALID;
	}
}


inline std::string_view
_get_str_index_uuid_field(UUIDFieldIndex index_uuid_field) noexcept
{
	switch (index_uuid_field) {
		case UUIDFieldIndex::uuid: {
			return std::string_view("uuid");
		}
		case UUIDFieldIndex::uuid_field: {
			return std::string_view("uuid_field");
		}
		case UUIDFieldIndex::both: {
			return std::string_view("both");
		}
		default: {
			return std::string_view("unknown");
		}
	}
}


inline std::string_view
_get_str_type(const std::array<FieldType, SPC_TOTAL_TYPES>& sep_types)
{
	constexpr static auto _ = phf::make_phf({
		hh(EMPTY + EMPTY + EMPTY),
		hh(EMPTY + EMPTY + OBJECT),
		hh(EMPTY + EMPTY + BOOLEAN),
		hh(EMPTY + EMPTY + DATE),
		hh(EMPTY + EMPTY + DATETIME),
		hh(EMPTY + EMPTY + FLOATING),
		hh(EMPTY + EMPTY + GEO),
		hh(EMPTY + EMPTY + INTEGER),
		hh(EMPTY + EMPTY + POSITIVE),
		hh(EMPTY + EMPTY + SCRIPT),
		hh(EMPTY + EMPTY + KEYWORD),
		hh(EMPTY + EMPTY + TEXT),
		hh(EMPTY + EMPTY + TIME),
		hh(EMPTY + EMPTY + TIMEDELTA),
		hh(EMPTY + EMPTY + UUID),

		hh(EMPTY + ARRAY + EMPTY),
		hh(EMPTY + ARRAY + OBJECT),
		hh(EMPTY + ARRAY + BOOLEAN),
		hh(EMPTY + ARRAY + DATE),
		hh(EMPTY + ARRAY + DATETIME),
		hh(EMPTY + ARRAY + FLOATING),
		hh(EMPTY + ARRAY + GEO),
		hh(EMPTY + ARRAY + INTEGER),
		hh(EMPTY + ARRAY + POSITIVE),
		hh(EMPTY + ARRAY + SCRIPT),
		hh(EMPTY + ARRAY + KEYWORD),
		hh(EMPTY + ARRAY + TEXT),
		hh(EMPTY + ARRAY + TIME),
		hh(EMPTY + ARRAY + TIMEDELTA),
		hh(EMPTY + ARRAY + UUID),

		hh(FOREIGN + EMPTY + EMPTY),
		hh(FOREIGN + EMPTY + OBJECT),
		hh(FOREIGN + EMPTY + BOOLEAN),
		hh(FOREIGN + EMPTY + DATE),
		hh(FOREIGN + EMPTY + DATETIME),
		hh(FOREIGN + EMPTY + FLOATING),
		hh(FOREIGN + EMPTY + GEO),
		hh(FOREIGN + EMPTY + INTEGER),
		hh(FOREIGN + EMPTY + POSITIVE),
		hh(FOREIGN + EMPTY + SCRIPT),
		hh(FOREIGN + EMPTY + KEYWORD),
		hh(FOREIGN + EMPTY + TEXT),
		hh(FOREIGN + EMPTY + TIME),
		hh(FOREIGN + EMPTY + TIMEDELTA),
		hh(FOREIGN + EMPTY + UUID),

		hh(FOREIGN + ARRAY + EMPTY),
		hh(FOREIGN + ARRAY + OBJECT),
		hh(FOREIGN + ARRAY + BOOLEAN),
		hh(FOREIGN + ARRAY + DATE),
		hh(FOREIGN + ARRAY + DATETIME),
		hh(FOREIGN + ARRAY + FLOATING),
		hh(FOREIGN + ARRAY + GEO),
		hh(FOREIGN + ARRAY + INTEGER),
		hh(FOREIGN + ARRAY + POSITIVE),
		hh(FOREIGN + ARRAY + SCRIPT),
		hh(FOREIGN + ARRAY + KEYWORD),
		hh(FOREIGN + ARRAY + TEXT),
		hh(FOREIGN + ARRAY + TIME),
		hh(FOREIGN + ARRAY + TIMEDELTA),
		hh(FOREIGN + ARRAY + UUID),
	});

	switch (_.fhh(std::string_view(reinterpret_cast<const char*>(sep_types.data()), SPC_TOTAL_TYPES))) {
		case _.fhh(EMPTY + EMPTY + EMPTY): {
			return std::string_view("undefined");
		}
		case _.fhh(EMPTY + EMPTY + OBJECT): {
			return std::string_view("object");
		}
		case _.fhh(EMPTY + EMPTY + BOOLEAN): {
			return std::string_view("boolean");
		}
		case _.fhh(EMPTY + EMPTY + DATE): {
			return std::string_view("date");
		}
		case _.fhh(EMPTY + EMPTY + DATETIME): {
			return std::string_view("datetime");
		}
		case _.fhh(EMPTY + EMPTY + FLOATING): {
			return std::string_view("floating");
		}
		case _.fhh(EMPTY + EMPTY + GEO): {
			return std::string_view("geo");
		}
		case _.fhh(EMPTY + EMPTY + INTEGER): {
			return std::string_view("integer");
		}
		case _.fhh(EMPTY + EMPTY + POSITIVE): {
			return std::string_view("positive");
		}
		case _.fhh(EMPTY + EMPTY + SCRIPT): {
			return std::string_view("script");
		}
		case _.fhh(EMPTY + EMPTY + KEYWORD): {
			return std::string_view("keyword");
		}
		case _.fhh(EMPTY + EMPTY + TEXT): {
			return std::string_view("text");
		}
		case _.fhh(EMPTY + EMPTY + TIME): {
			return std::string_view("time");
		}
		case _.fhh(EMPTY + EMPTY + TIMEDELTA): {
			return std::string_view("timedelta");
		}
		case _.fhh(EMPTY + EMPTY + UUID): {
			return std::string_view("uuid");
		}
		case _.fhh(EMPTY + ARRAY + EMPTY): {
			return std::string_view("array");
		}
		case _.fhh(EMPTY + ARRAY + OBJECT): {
			return std::string_view("array/object");
		}
		case _.fhh(EMPTY + ARRAY + BOOLEAN): {
			return std::string_view("array/boolean");
		}
		case _.fhh(EMPTY + ARRAY + DATE): {
			return std::string_view("array/date");
		}
		case _.fhh(EMPTY + ARRAY + DATETIME): {
			return std::string_view("array/datetime");
		}
		case _.fhh(EMPTY + ARRAY + FLOATING): {
			return std::string_view("array/floating");
		}
		case _.fhh(EMPTY + ARRAY + GEO): {
			return std::string_view("array/geo");
		}
		case _.fhh(EMPTY + ARRAY + INTEGER): {
			return std::string_view("array/integer");
		}
		case _.fhh(EMPTY + ARRAY + POSITIVE): {
			return std::string_view("array/positive");
		}
		case _.fhh(EMPTY + ARRAY + SCRIPT): {
			return std::string_view("array/script");
		}
		case _.fhh(EMPTY + ARRAY + KEYWORD): {
			return std::string_view("array/keyword");
		}
		case _.fhh(EMPTY + ARRAY + TEXT): {
			return std::string_view("array/text");
		}
		case _.fhh(EMPTY + ARRAY + TIME): {
			return std::string_view("array/time");
		}
		case _.fhh(EMPTY + ARRAY + TIMEDELTA): {
			return std::string_view("array/timedelta");
		}
		case _.fhh(EMPTY + ARRAY + UUID): {
			return std::string_view("array/uuid");
		}
		case _.fhh(FOREIGN + EMPTY + EMPTY): {
			return std::string_view("foreign");
		}
		case _.fhh(FOREIGN + EMPTY + OBJECT): {
			return std::string_view("foreign/object");
		}
		case _.fhh(FOREIGN + EMPTY + BOOLEAN): {
			return std::string_view("foreign/boolean");
		}
		case _.fhh(FOREIGN + EMPTY + DATE): {
			return std::string_view("foreign/date");
		}
		case _.fhh(FOREIGN + EMPTY + DATETIME): {
			return std::string_view("foreign/datetime");
		}
		case _.fhh(FOREIGN + EMPTY + FLOATING): {
			return std::string_view("foreign/floating");
		}
		case _.fhh(FOREIGN + EMPTY + GEO): {
			return std::string_view("foreign/geo");
		}
		case _.fhh(FOREIGN + EMPTY + INTEGER): {
			return std::string_view("foreign/integer");
		}
		case _.fhh(FOREIGN + EMPTY + POSITIVE): {
			return std::string_view("foreign/positive");
		}
		case _.fhh(FOREIGN + EMPTY + SCRIPT): {
			return std::string_view("foreign/script");
		}
		case _.fhh(FOREIGN + EMPTY + KEYWORD): {
			return std::string_view("foreign/keyword");
		}
		case _.fhh(FOREIGN + EMPTY + TEXT): {
			return std::string_view("foreign/text");
		}
		case _.fhh(FOREIGN + EMPTY + TIME): {
			return std::string_view("foreign/time");
		}
		case _.fhh(FOREIGN + EMPTY + TIMEDELTA): {
			return std::string_view("foreign/timedelta");
		}
		case _.fhh(FOREIGN + EMPTY + UUID): {
			return std::string_view("foreign/uuid");
		}
		case _.fhh(FOREIGN + ARRAY + EMPTY): {
			return std::string_view("foreign/array");
		}
		case _.fhh(FOREIGN + ARRAY + OBJECT): {
			return std::string_view("foreign/array/object");
		}
		case _.fhh(FOREIGN + ARRAY + BOOLEAN): {
			return std::string_view("foreign/array/boolean");
		}
		case _.fhh(FOREIGN + ARRAY + DATE): {
			return std::string_view("foreign/array/date");
		}
		case _.fhh(FOREIGN + ARRAY + DATETIME): {
			return std::string_view("foreign/array/datetime");
		}
		case _.fhh(FOREIGN + ARRAY + FLOATING): {
			return std::string_view("foreign/array/floating");
		}
		case _.fhh(FOREIGN + ARRAY + GEO): {
			return std::string_view("foreign/array/geo");
		}
		case _.fhh(FOREIGN + ARRAY + INTEGER): {
			return std::string_view("foreign/array/integer");
		}
		case _.fhh(FOREIGN + ARRAY + POSITIVE): {
			return std::string_view("foreign/array/positive");
		}
		case _.fhh(FOREIGN + ARRAY + SCRIPT): {
			return std::string_view("foreign/array/script");
		}
		case _.fhh(FOREIGN + ARRAY + KEYWORD): {
			return std::string_view("foreign/array/keyword");
		}
		case _.fhh(FOREIGN + ARRAY + TEXT): {
			return std::string_view("foreign/array/text");
		}
		case _.fhh(FOREIGN + ARRAY + TIME): {
			return std::string_view("foreign/array/time");
		}
		case _.fhh(FOREIGN + ARRAY + TIMEDELTA): {
			return std::string_view("foreign/array/timedelta");
		}
		case _.fhh(FOREIGN + ARRAY + UUID): {
			return std::string_view("foreign/array/uuid");
		}
		default: {
			std::string result;
			if (sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
				result += enum_name(sep_types[SPC_FOREIGN_TYPE]);
			}
			if (sep_types[SPC_ARRAY_TYPE] == FieldType::array) {
				if (!result.empty()) { result += "/"; }
				result += enum_name(sep_types[SPC_ARRAY_TYPE]);
			}
			if (sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
				if (!result.empty()) { result += "/"; }
				result += enum_name(sep_types[SPC_CONCRETE_TYPE]);
			}
			THROW(ClientError, "{} not supported.", repr(result), RESERVED_TYPE);
		}
	}
}

// The stem_language names _get_stem_language accepts, as a refusal names them.
static constexpr std::string_view stem_language_names =
	"armenian, hy, basque, eu, catalan, ca, danish, da, dutch, nl, kraaij_pohlmann, english, en, "
	"lovins, porter, finnish, fi, french, fr, german, de, "
	"german2, hungarian, hu, italian, it, norwegian, nb, nn, no, portuguese, pt, romanian, ro, russian, "
	"ru, spanish, es, swedish, sv, turkish, tr, none";


inline const std::pair<bool, const std::string>&
_get_stem_language(std::string_view str_stem_language)
{
	constexpr static auto _ = phf::make_phf({
		hhl("armenian"),
		hhl("hy"),
		hhl("basque"),
		hhl("eu"),
		hhl("catalan"),
		hhl("ca"),
		hhl("danish"),
		hhl("da"),
		hhl("dutch"),
		hhl("nl"),
		hhl("kraaij_pohlmann"),
		hhl("english"),
		hhl("en"),
		hhl("lovins"),
		hhl("porter"),
		hhl("finnish"),
		hhl("fi"),
		hhl("french"),
		hhl("fr"),
		hhl("german"),
		hhl("de"),
		hhl("german2"),
		hhl("hungarian"),
		hhl("hu"),
		hhl("italian"),
		hhl("it"),
		hhl("norwegian"),
		hhl("nb"),
		hhl("nn"),
		hhl("no"),
		hhl("portuguese"),
		hhl("pt"),
		hhl("romanian"),
		hhl("ro"),
		hhl("russian"),
		hhl("ru"),
		hhl("spanish"),
		hhl("es"),
		hhl("swedish"),
		hhl("sv"),
		hhl("turkish"),
		hhl("tr"),
		hhl("none"),
		hhl(""),
	});

	switch(_.fhhl(str_stem_language)) {
		case _.fhhl("armenian"): {
			static const std::pair<bool, const std::string> hy{ true,  "hy" };
			return hy;
		}
		case _.fhhl("hy"): {
			static const std::pair<bool, const std::string> hy{ true,  "hy" };
			return hy;
		}
		case _.fhhl("basque"): {
			static const std::pair<bool, const std::string> eu{ true,  "eu" };
			return eu;
		}
		case _.fhhl("eu"): {
			static const std::pair<bool, const std::string> eu{ true,  "eu" };
			return eu;
		}
		case _.fhhl("catalan"): {
			static const std::pair<bool, const std::string> ca{ true,  "ca" };
			return ca;
		}
		case _.fhhl("ca"): {
			static const std::pair<bool, const std::string> ca{ true,  "ca" };
			return ca;
		}
		case _.fhhl("danish"): {
			static const std::pair<bool, const std::string> da{ true,  "da" };
			return da;
		}
		case _.fhhl("da"): {
			static const std::pair<bool, const std::string> da{ true,  "da" };
			return da;
		}
		case _.fhhl("dutch"): {
			static const std::pair<bool, const std::string> nl{ true,  "nl" };
			return nl;
		}
		case _.fhhl("nl"): {
			static const std::pair<bool, const std::string> nl{ true,  "nl" };
			return nl;
		}
		case _.fhhl("kraaij_pohlmann"): {
			static const std::pair<bool, const std::string> nl{ false, "nl" };
			return nl;
		}
		case _.fhhl("english"): {
			static const std::pair<bool, const std::string> en{ true,  "en" };
			return en;
		}
		case _.fhhl("en"): {
			static const std::pair<bool, const std::string> en{ true,  "en" };
			return en;
		}
		case _.fhhl("lovins"): {
			static const std::pair<bool, const std::string> en{ false, "en" };
			return en;
		}
		case _.fhhl("porter"): {
			static const std::pair<bool, const std::string> en{ false, "en" };
			return en;
		}
		case _.fhhl("finnish"): {
			static const std::pair<bool, const std::string> fi{ true,  "fi" };
			return fi;
		}
		case _.fhhl("fi"): {
			static const std::pair<bool, const std::string> fi{ true,  "fi" };
			return fi;
		}
		case _.fhhl("french"): {
			static const std::pair<bool, const std::string> fr{ true,  "fr" };
			return fr;
		}
		case _.fhhl("fr"): {
			static const std::pair<bool, const std::string> fr{ true,  "fr" };
			return fr;
		}
		case _.fhhl("german"): {
			static const std::pair<bool, const std::string> de{ true,  "de" };
			return de;
		}
		case _.fhhl("de"): {
			static const std::pair<bool, const std::string> de{ true,  "de" };
			return de;
		}
		case _.fhhl("german2"): {
			static const std::pair<bool, const std::string> de{ false, "de" };
			return de;
		}
		case _.fhhl("hungarian"): {
			static const std::pair<bool, const std::string> hu{ true,  "hu" };
			return hu;
		}
		case _.fhhl("hu"): {
			static const std::pair<bool, const std::string> hu{ true,  "hu" };
			return hu;
		}
		case _.fhhl("italian"): {
			static const std::pair<bool, const std::string> it{ true,  "it" };
			return it;
		}
		case _.fhhl("it"): {
			static const std::pair<bool, const std::string> it{ true,  "it" };
			return it;
		}
		case _.fhhl("norwegian"): {
			static const std::pair<bool, const std::string> no{ true,  "no" };
			return no;
		}
		case _.fhhl("nb"): {
			static const std::pair<bool, const std::string> no{ false, "no" };
			return no;
		}
		case _.fhhl("nn"): {
			static const std::pair<bool, const std::string> no{ false, "no" };
			return no;
		}
		case _.fhhl("no"): {
			static const std::pair<bool, const std::string> no{ true,  "no" };
			return no;
		}
		case _.fhhl("portuguese"): {
			static const std::pair<bool, const std::string> pt{ true,  "pt" };
			return pt;
		}
		case _.fhhl("pt"): {
			static const std::pair<bool, const std::string> pt{ true,  "pt" };
			return pt;
		}
		case _.fhhl("romanian"): {
			static const std::pair<bool, const std::string> ro{ true,  "ro" };
			return ro;
		}
		case _.fhhl("ro"): {
			static const std::pair<bool, const std::string> ro{ true,  "ro" };
			return ro;
		}
		case _.fhhl("russian"): {
			static const std::pair<bool, const std::string> ru{ true,  "ru" };
			return ru;
		}
		case _.fhhl("ru"): {
			static const std::pair<bool, const std::string> ru{ true,  "ru" };
			return ru;
		}
		case _.fhhl("spanish"): {
			static const std::pair<bool, const std::string> es{ true,  "es" };
			return es;
		}
		case _.fhhl("es"): {
			static const std::pair<bool, const std::string> es{ true,  "es" };
			return es;
		}
		case _.fhhl("swedish"): {
			static const std::pair<bool, const std::string> sv{ true,  "sv" };
			return sv;
		}
		case _.fhhl("sv"): {
			static const std::pair<bool, const std::string> sv{ true,  "sv" };
			return sv;
		}
		case _.fhhl("turkish"): {
			static const std::pair<bool, const std::string> tr{ true,  "tr" };
			return tr;
		}
		case _.fhhl("tr"): {
			static const std::pair<bool, const std::string> tr{ true,  "tr" };
			return tr;
		}
		case _.fhhl("none"):
		case _.fhhl(""): {
			static const std::pair<bool, const std::string> _{ true, "" };
			return _;
		}
		default: {
			static const std::pair<bool, const std::string> _{ false, "unknown" };
			return _;
		}
	}
}


// The name snowball's stemmer takes for a stem_language: nb and nn stem as norwegian, and german2
// as german, word for word with Xapian 2.1.0 over snowball-data's vocabularies.
inline std::string
_get_stemmer_name(std::string_view stem_language)
{
	auto name = strings::lower(stem_language);
	if (name == "nb" || name == "nn") {
		return "norwegian";
	}
	if (name == "german2") {
		return "german";
	}
	return name;
}


inline std::string
repr_field(std::string_view name, std::string_view field_name)
{
	return name == field_name ? repr(name) : strings::format("{} ({})", repr(name), repr(field_name));
}

}  // namespace schema_detail
