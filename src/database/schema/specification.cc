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

// The field specification: required_spc_t, index_spc_t and specification_t, the default
// specification, and the tables they are built from.

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


using namespace schema_detail;


namespace schema_detail {

/*
 * Default accuracies.
 */

/* Python script to generate the def_accuracy_num:
 * deltas = [8, 7, 7, 7, 7, 7, 5, 5, 2, 1]; w = [(0, 64)]; print('\n'.join(reversed(['\t(1ULL << {}),                       // {} (max_terms: {}, delta: {})'.format(y, 1 << y, int((1 << x) / (1 << y)), x - y) for x, y in ((lambda delta: w.append((w[-1][1], w[-1][1] - delta)))(delta) or w[-1] for delta in reversed(deltas))])))
 */
const std::vector<uint64_t> def_accuracy_num({
	100ULL,
	1000ULL,
	10000ULL,
	100000ULL,
	1000000ULL,
	100000000ULL,
 });


static const std::vector<uint64_t> def_accuracy_date({
	toUType(UnitTime::day),             // 86400 s
	toUType(UnitTime::month),           // 2592000 s
	toUType(UnitTime::year),            // 31536000 s
	toUType(UnitTime::decade),          // 315360000 s
	toUType(UnitTime::century),         // 3153600000 s
});


const std::vector<uint64_t> def_accuracy_datetime({
	toUType(UnitTime::hour),            // 3600 s
	toUType(UnitTime::day),             // 86400 s
	toUType(UnitTime::month),           // 2592000 s
	toUType(UnitTime::year),            // 31536000 s
	toUType(UnitTime::decade),          // 315360000 s
	toUType(UnitTime::century),         // 3153600000 s
});


const std::vector<uint64_t> def_accuracy_time({
	toUType(UnitTime::minute),          // 60 s
	toUType(UnitTime::hour),            // 3600 s
});


/* HTM terms (Hierarchical Triangular Mesh)
 * HTM terms (Hierarchical Triangular Mesh)
 * Any integer value in the range 0-25 can be used to specify an HTM level
 * An approximation of the accuracy obtained by a level can be estimated as:
 *    0.30 * 2 ** (25 - level)
 *
 * Python script to generate the def_accuracy_geo:
 * levels = [3, 5, 8, 10, 12, 15]; print('\n'.join('\t{},                                 // ~ {} m'.format(level, 0.30 * 2 ** (25 - level)) for level in levels))
 */
const std::vector<uint64_t> def_accuracy_geo({
	3,                                  //  ~ 1,258,291.2 m
	5,                                  //    ~ 314,572.8 m
	8,                                  //     ~ 39,321.6 m
	10,                                 //      ~ 9,830.4 m
	12,                                 //      ~ 2,457.6 m
	15,                                 //        ~ 307.2 m
});

const std::string str_set_acc_date(strings::join<std::string>({
	"second",
	"minute",
	"hour",
	"day",
	"month",
	"year",
	"decade",
	"century",
	"millennium",
}, ", ", " or "));

const std::string str_set_acc_time(strings::join<std::string>({
	"second",
	"minute",
	"hour",
}, ", ", " or "));
const std::string str_set_stop_strategy(strings::join<std::string>({
	"stop_none",
	"none",
	"stop_all",
	"all",
	"stop_stemmed",
	"stemmed",
}, ", ", " or "));
const std::string str_set_stem_strategy(strings::join<std::string>({
	"stem_none",
	"none",
	"stem_some",
	"some",
	"stem_all",
	"all",
	"stem_all_z",
	"all_z",
}, ", ", " or "));


const std::string str_set_index_uuid_field(strings::join<std::string>({
	"uuid",
	"uuid_field",
	"both",
}, ", ", " or "));
const std::string str_set_index(strings::join<std::string>({
	"none",
	"field_terms",
	"field_values",
	"field_terms,field_values",
	"field_values,field_terms",
	"field",
	"field_all",
	"global_terms",
	"field_terms,global_terms",
	"global_terms,field_terms",
	"terms",
	"global_terms,field_values",
	"field_values,global_terms",
	"global_terms,field",
	"global_terms,field_all",
	"field,global_terms",
	"field_all,global_terms",
	"global_values",
	"global_values,field_terms",
	"field_terms,global_values",
	"field_values,global_values",
	"global_values,field_values",
	"values",
	"global_values,field",
	"global_values,field_all",
	"field,global_values",
	"field_all,global_values",
	"global",
	"global_all",
	"global_values,global_terms",
	"global_terms,global_values",
	"global,field_terms",
	"global_all,field_terms",
	"field_terms,global",
	"field_terms,global_all",
	"global_all,field_values",
	"global,field_values",
	"field_values,global",
	"field_values,global_all",
	"field_all,global_all",
	"global_all,field_all",
	"all",
}, ", ", " or "));
}  // namespace schema_detail


static inline const std::array<FieldType, SPC_TOTAL_TYPES>&
_get_type(std::string_view str_type)
{
	constexpr static auto _ = phf::make_phf({
		hhl("undefined"),
		hhl("object"),
		hhl("boolean"),
		hhl("date"),
		hhl("datetime"),
		hhl("float"),
		hhl("floating"),
		hhl("geo"),
		hhl("geospatial"),
		hhl("integer"),
		hhl("positive"),
		hhl("script"),
		hhl("term"),
		hhl("keyword"),
		hhl("string"),
		hhl("text"),
		hhl("time"),
		hhl("timedelta"),
		hhl("uuid"),
		hhl("array"),
		hhl("array/object"),
		hhl("object/array"),
		hhl("array/boolean"),
		hhl("boolean/array"),
		hhl("array/date"),
		hhl("date/array"),
		hhl("array/datetime"),
		hhl("datetime/array"),
		hhl("array/float"),
		hhl("float/array"),
		hhl("array/floating"),
		hhl("floating/array"),
		hhl("array/geo"),
		hhl("geo/array"),
		hhl("array/geospatial"),
		hhl("geospatial/array"),
		hhl("array/integer"),
		hhl("integer/array"),
		hhl("array/positive"),
		hhl("positive/array"),
		hhl("array/script"),
		hhl("script/array"),
		hhl("array/term"),
		hhl("term/array"),
		hhl("array/keyword"),
		hhl("keyword/array"),
		hhl("array/string"),
		hhl("string/array"),
		hhl("array/text"),
		hhl("text/array"),
		hhl("array/time"),
		hhl("time/array"),
		hhl("array/timedelta"),
		hhl("timedelta/array"),
		hhl("array/uuid"),
		hhl("uuid/array"),
		hhl("foreign"),
		hhl("foreign/object"),
		hhl("object/foreign"),
		hhl("foreign/boolean"),
		hhl("boolean/foreign"),
		hhl("foreign/date"),
		hhl("date/foreign"),
		hhl("foreign/datetime"),
		hhl("datetime/foreign"),
		hhl("foreign/float"),
		hhl("float/foreign"),
		hhl("foreign/floating"),
		hhl("floating/foreign"),
		hhl("foreign/geo"),
		hhl("geo/foreign"),
		hhl("foreign/geospatial"),
		hhl("geospatial/foreign"),
		hhl("foreign/integer"),
		hhl("integer/foreign"),
		hhl("foreign/positive"),
		hhl("positive/foreign"),
		hhl("foreign/script"),
		hhl("script/foreign"),
		hhl("foreign/term"),
		hhl("term/foreign"),
		hhl("foreign/keyword"),
		hhl("keyword/foreign"),
		hhl("foreign/string"),
		hhl("string/foreign"),
		hhl("foreign/text"),
		hhl("text/foreign"),
		hhl("foreign/time"),
		hhl("time/foreign"),
		hhl("foreign/timedelta"),
		hhl("timedelta/foreign"),
		hhl("foreign/uuid"),
		hhl("uuid/foreign"),
		hhl("foreign/array"),
		hhl("array/foreign"),
		hhl("foreign/array/object"),
		hhl("foreign/object/array"),
		hhl("array/foreign/object"),
		hhl("array/object/foreign"),
		hhl("object/foreign/array"),
		hhl("object/array/foreign"),
		hhl("foreign/array/boolean"),
		hhl("foreign/boolean/array"),
		hhl("array/foreign/boolean"),
		hhl("array/boolean/foreign"),
		hhl("boolean/foreign/array"),
		hhl("boolean/array/foreign"),
		hhl("foreign/array/date"),
		hhl("foreign/date/array"),
		hhl("array/foreign/date"),
		hhl("array/date/foreign"),
		hhl("date/foreign/array"),
		hhl("date/array/foreign"),
		hhl("foreign/array/datetime"),
		hhl("foreign/datetime/array"),
		hhl("array/foreign/datetime"),
		hhl("array/datetime/foreign"),
		hhl("datetime/foreign/array"),
		hhl("datetime/array/foreign"),
		hhl("foreign/array/float"),
		hhl("foreign/float/array"),
		hhl("array/foreign/float"),
		hhl("array/float/foreign"),
		hhl("float/foreign/array"),
		hhl("float/array/foreign"),
		hhl("foreign/array/floating"),
		hhl("foreign/floating/array"),
		hhl("array/foreign/floating"),
		hhl("array/floating/foreign"),
		hhl("floating/foreign/array"),
		hhl("floating/array/foreign"),
		hhl("foreign/array/geo"),
		hhl("foreign/geo/array"),
		hhl("array/foreign/geo"),
		hhl("array/geo/foreign"),
		hhl("geo/foreign/array"),
		hhl("geo/array/foreign"),
		hhl("foreign/array/geospatial"),
		hhl("foreign/geospatial/array"),
		hhl("array/foreign/geospatial"),
		hhl("array/geospatial/foreign"),
		hhl("geospatial/foreign/array"),
		hhl("geospatial/array/foreign"),
		hhl("foreign/array/integer"),
		hhl("foreign/integer/array"),
		hhl("array/foreign/integer"),
		hhl("array/integer/foreign"),
		hhl("integer/foreign/array"),
		hhl("integer/array/foreign"),
		hhl("foreign/array/positive"),
		hhl("foreign/positive/array"),
		hhl("array/foreign/positive"),
		hhl("array/positive/foreign"),
		hhl("positive/foreign/array"),
		hhl("positive/array/foreign"),
		hhl("foreign/array/script"),
		hhl("foreign/script/array"),
		hhl("array/foreign/script"),
		hhl("array/script/foreign"),
		hhl("script/foreign/array"),
		hhl("script/array/foreign"),
		hhl("foreign/array/term"),
		hhl("foreign/term/array"),
		hhl("array/foreign/term"),
		hhl("array/term/foreign"),
		hhl("term/foreign/array"),
		hhl("term/array/foreign"),
		hhl("foreign/array/keyword"),
		hhl("foreign/keyword/array"),
		hhl("array/foreign/keyword"),
		hhl("array/keyword/foreign"),
		hhl("keyword/foreign/array"),
		hhl("keyword/array/foreign"),
		hhl("foreign/array/string"),
		hhl("foreign/string/array"),
		hhl("array/foreign/string"),
		hhl("array/string/foreign"),
		hhl("string/foreign/array"),
		hhl("string/array/foreign"),
		hhl("foreign/array/text"),
		hhl("foreign/text/array"),
		hhl("array/foreign/text"),
		hhl("array/text/foreign"),
		hhl("text/foreign/array"),
		hhl("text/array/foreign"),
		hhl("foreign/array/time"),
		hhl("foreign/time/array"),
		hhl("array/foreign/time"),
		hhl("array/time/foreign"),
		hhl("time/foreign/array"),
		hhl("time/array/foreign"),
		hhl("foreign/array/timedelta"),
		hhl("foreign/timedelta/array"),
		hhl("array/foreign/timedelta"),
		hhl("array/timedelta/foreign"),
		hhl("timedelta/foreign/array"),
		hhl("timedelta/array/foreign"),
		hhl("foreign/array/uuid"),
		hhl("foreign/uuid/array"),
		hhl("array/foreign/uuid"),
		hhl("array/uuid/foreign"),
		hhl("uuid/foreign/array"),
		hhl("uuid/array/foreign"),
	});

	switch(_.fhhl(str_type)) {
		default:
		case _.fhhl("undefined"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::empty }};
			return _;
		}
		case _.fhhl("object"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::object }};
			return _;
		}
		case _.fhhl("boolean"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::boolean }};
			return _;
		}
		case _.fhhl("date"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::date }};
			return _;
		}
		case _.fhhl("datetime"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::datetime }};
			return _;
		}
		case _.fhhl("float"):
		case _.fhhl("floating"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::floating }};
			return _;
		}
		case _.fhhl("geospatial"):
		case _.fhhl("geo"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::geo }};
			return _;
		}
		case _.fhhl("integer"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::integer }};
			return _;
		}
		case _.fhhl("positive"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::positive }};
			return _;
		}
		case _.fhhl("script"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::script }};
			return _;
		}
		case _.fhhl("term"):
		case _.fhhl("keyword"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::keyword }};
			return _;
		}
		case _.fhhl("string"):
		case _.fhhl("text"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::text }};
			return _;
		}
		case _.fhhl("time"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::time }};
			return _;
		}
		case _.fhhl("timedelta"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::timedelta }};
			return _;
		}
		case _.fhhl("uuid"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::empty, FieldType::uuid }};
			return _;
		}
		case _.fhhl("array"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::empty }};
			return _;
		}
		case _.fhhl("object/array"):
		case _.fhhl("array/object"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::object }};
			return _;
		}
		case _.fhhl("boolean/array"):
		case _.fhhl("array/boolean"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::boolean }};
			return _;
		}
		case _.fhhl("date/array"):
		case _.fhhl("array/date"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::date }};
			return _;
		}
		case _.fhhl("datetime/array"):
		case _.fhhl("array/datetime"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::datetime }};
			return _;
		}
		case _.fhhl("float/array"):
		case _.fhhl("array/float"):
		case _.fhhl("floating/array"):
		case _.fhhl("array/floating"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::floating }};
			return _;
		}
		case _.fhhl("geospatial/array"):
		case _.fhhl("array/geospatial"):
		case _.fhhl("geo/array"):
		case _.fhhl("array/geo"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::geo }};
			return _;
		}
		case _.fhhl("integer/array"):
		case _.fhhl("array/integer"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::integer }};
			return _;
		}
		case _.fhhl("positive/array"):
		case _.fhhl("array/positive"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::positive }};
			return _;
		}
		case _.fhhl("script/array"):
		case _.fhhl("array/script"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::script }};
			return _;
		}
		case _.fhhl("term/array"):
		case _.fhhl("array/term"):
		case _.fhhl("keyword/array"):
		case _.fhhl("array/keyword"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::keyword }};
			return _;
		}
		case _.fhhl("string/array"):
		case _.fhhl("array/string"):
		case _.fhhl("text/array"):
		case _.fhhl("array/text"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::text }};
			return _;
		}
		case _.fhhl("time/array"):
		case _.fhhl("array/time"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::time }};
			return _;
		}
		case _.fhhl("timedelta/array"):
		case _.fhhl("array/timedelta"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::timedelta }};
			return _;
		}
		case _.fhhl("uuid/array"):
		case _.fhhl("array/uuid"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::empty, FieldType::array, FieldType::uuid }};
			return _;
		}
		case _.fhhl("foreign"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::empty }};
			return _;
		}
		case _.fhhl("object/foreign"):
		case _.fhhl("foreign/object"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::object }};
			return _;
		}
		case _.fhhl("boolean/foreign"):
		case _.fhhl("foreign/boolean"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::boolean }};
			return _;
		}
		case _.fhhl("date/foreign"):
		case _.fhhl("foreign/date"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::date }};
			return _;
		}
		case _.fhhl("datetime/foreign"):
		case _.fhhl("foreign/datetime"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::datetime }};
			return _;
		}
		case _.fhhl("float/foreign"):
		case _.fhhl("foreign/float"):
		case _.fhhl("floating/foreign"):
		case _.fhhl("foreign/floating"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::floating }};
			return _;
		}
		case _.fhhl("geospatial/foreign"):
		case _.fhhl("foreign/geospatial"):
		case _.fhhl("geo/foreign"):
		case _.fhhl("foreign/geo"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::geo }};
			return _;
		}
		case _.fhhl("integer/foreign"):
		case _.fhhl("foreign/integer"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::integer }};
			return _;
		}
		case _.fhhl("positive/foreign"):
		case _.fhhl("foreign/positive"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::positive }};
			return _;
		}
		case _.fhhl("script/foreign"):
		case _.fhhl("foreign/script"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::script }};
			return _;
		}
		case _.fhhl("term/foreign"):
		case _.fhhl("foreign/term"):
		case _.fhhl("keyword/foreign"):
		case _.fhhl("foreign/keyword"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::keyword }};
			return _;
		}
		case _.fhhl("string/foreign"):
		case _.fhhl("foreign/string"):
		case _.fhhl("text/foreign"):
		case _.fhhl("foreign/text"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::text }};
			return _;
		}
		case _.fhhl("time/foreign"):
		case _.fhhl("foreign/time"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::time }};
			return _;
		}
		case _.fhhl("timedelta/foreign"):
		case _.fhhl("foreign/timedelta"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::timedelta }};
			return _;
		}
		case _.fhhl("uuid/foreign"):
		case _.fhhl("foreign/uuid"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::empty, FieldType::uuid }};
			return _;
		}
		case _.fhhl("array/foreign"):
		case _.fhhl("foreign/array"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::empty }};
			return _;
		}
		case _.fhhl("object/array/foreign"):
		case _.fhhl("object/foreign/array"):
		case _.fhhl("array/object/foreign"):
		case _.fhhl("array/foreign/object"):
		case _.fhhl("foreign/object/array"):
		case _.fhhl("foreign/array/object"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::object }};
			return _;
		}
		case _.fhhl("boolean/array/foreign"):
		case _.fhhl("boolean/foreign/array"):
		case _.fhhl("array/boolean/foreign"):
		case _.fhhl("array/foreign/boolean"):
		case _.fhhl("foreign/boolean/array"):
		case _.fhhl("foreign/array/boolean"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::boolean }};
			return _;
		}
		case _.fhhl("date/array/foreign"):
		case _.fhhl("date/foreign/array"):
		case _.fhhl("array/date/foreign"):
		case _.fhhl("array/foreign/date"):
		case _.fhhl("foreign/date/array"):
		case _.fhhl("foreign/array/date"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::date }};
			return _;
		}
		case _.fhhl("datetime/array/foreign"):
		case _.fhhl("datetime/foreign/array"):
		case _.fhhl("array/datetime/foreign"):
		case _.fhhl("array/foreign/datetime"):
		case _.fhhl("foreign/datetime/array"):
		case _.fhhl("foreign/array/datetime"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::datetime }};
			return _;
		}
		case _.fhhl("float/array/foreign"):
		case _.fhhl("float/foreign/array"):
		case _.fhhl("array/float/foreign"):
		case _.fhhl("array/foreign/float"):
		case _.fhhl("foreign/float/array"):
		case _.fhhl("foreign/array/float"):
		case _.fhhl("floating/array/foreign"):
		case _.fhhl("floating/foreign/array"):
		case _.fhhl("array/floating/foreign"):
		case _.fhhl("array/foreign/floating"):
		case _.fhhl("foreign/floating/array"):
		case _.fhhl("foreign/array/floating"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::floating }};
			return _;
		}
		case _.fhhl("geospatial/array/foreign"):
		case _.fhhl("geospatial/foreign/array"):
		case _.fhhl("array/geospatial/foreign"):
		case _.fhhl("array/foreign/geospatial"):
		case _.fhhl("foreign/geospatial/array"):
		case _.fhhl("foreign/array/geospatial"):
		case _.fhhl("geo/array/foreign"):
		case _.fhhl("geo/foreign/array"):
		case _.fhhl("array/geo/foreign"):
		case _.fhhl("array/foreign/geo"):
		case _.fhhl("foreign/geo/array"):
		case _.fhhl("foreign/array/geo"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::geo }};
			return _;
		}
		case _.fhhl("integer/array/foreign"):
		case _.fhhl("integer/foreign/array"):
		case _.fhhl("array/integer/foreign"):
		case _.fhhl("array/foreign/integer"):
		case _.fhhl("foreign/integer/array"):
		case _.fhhl("foreign/array/integer"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::integer }};
			return _;
		}
		case _.fhhl("positive/array/foreign"):
		case _.fhhl("positive/foreign/array"):
		case _.fhhl("array/positive/foreign"):
		case _.fhhl("array/foreign/positive"):
		case _.fhhl("foreign/positive/array"):
		case _.fhhl("foreign/array/positive"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::positive }};
			return _;
		}
		case _.fhhl("script/array/foreign"):
		case _.fhhl("script/foreign/array"):
		case _.fhhl("array/script/foreign"):
		case _.fhhl("array/foreign/script"):
		case _.fhhl("foreign/script/array"):
		case _.fhhl("foreign/array/script"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::script }};
			return _;
		}
		case _.fhhl("term/array/foreign"):
		case _.fhhl("term/foreign/array"):
		case _.fhhl("array/term/foreign"):
		case _.fhhl("array/foreign/term"):
		case _.fhhl("foreign/term/array"):
		case _.fhhl("foreign/array/term"):
		case _.fhhl("keyword/array/foreign"):
		case _.fhhl("keyword/foreign/array"):
		case _.fhhl("array/keyword/foreign"):
		case _.fhhl("array/foreign/keyword"):
		case _.fhhl("foreign/keyword/array"):
		case _.fhhl("foreign/array/keyword"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::keyword }};
			return _;
		}
		case _.fhhl("string/array/foreign"):
		case _.fhhl("string/foreign/array"):
		case _.fhhl("array/string/foreign"):
		case _.fhhl("array/foreign/string"):
		case _.fhhl("foreign/string/array"):
		case _.fhhl("foreign/array/string"):
		case _.fhhl("text/array/foreign"):
		case _.fhhl("text/foreign/array"):
		case _.fhhl("array/text/foreign"):
		case _.fhhl("array/foreign/text"):
		case _.fhhl("foreign/text/array"):
		case _.fhhl("foreign/array/text"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::text }};
			return _;
		}
		case _.fhhl("time/array/foreign"):
		case _.fhhl("time/foreign/array"):
		case _.fhhl("array/time/foreign"):
		case _.fhhl("array/foreign/time"):
		case _.fhhl("foreign/time/array"):
		case _.fhhl("foreign/array/time"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::time }};
			return _;
		}
		case _.fhhl("timedelta/array/foreign"):
		case _.fhhl("timedelta/foreign/array"):
		case _.fhhl("array/timedelta/foreign"):
		case _.fhhl("array/foreign/timedelta"):
		case _.fhhl("foreign/timedelta/array"):
		case _.fhhl("foreign/array/timedelta"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::timedelta }};
			return _;
		}
		case _.fhhl("uuid/array/foreign"):
		case _.fhhl("uuid/foreign/array"):
		case _.fhhl("array/uuid/foreign"):
		case _.fhhl("array/foreign/uuid"):
		case _.fhhl("foreign/uuid/array"):
		case _.fhhl("foreign/array/uuid"): {
			static const std::array<FieldType, SPC_TOTAL_TYPES> _{{ FieldType::foreign, FieldType::array, FieldType::uuid }};
			return _;
		}
	}
}

/*
 * Default acc_prefixes for global values.
 */

static std::vector<std::string>
get_acc_prefix(const std::vector<uint64_t> accuracy)
{
	std::vector<std::string> res;
	res.reserve(accuracy.size());
	for (const auto& acc : accuracy) {
		res.push_back(get_prefix(acc));
	}
	return res;
}

static const std::vector<std::string> global_acc_prefix_num(get_acc_prefix(def_accuracy_num));
static const std::vector<std::string> global_acc_prefix_date(get_acc_prefix(def_accuracy_datetime));
static const std::vector<std::string> global_acc_prefix_time(get_acc_prefix(def_accuracy_time));
static const std::vector<std::string> global_acc_prefix_geo(get_acc_prefix(def_accuracy_geo));


/*
 * Acceptable values string used when there is a data inconsistency.
 */


specification_t default_spc;


required_spc_t::flags_t::flags_t()
	: bool_term(DEFAULT_BOOL_TERM),
	  partials(DEFAULT_GEO_PARTIALS),
	  store(true),
	  parent_store(true),
	  recurse(true),
	  dynamic(true),
	  strict(false),
	  date_detection(true),
	  datetime_detection(true),
	  time_detection(true),
	  timedelta_detection(true),
	  numeric_detection(true),
	  geo_detection(true),
	  bool_detection(true),
	  text_detection(true),
	  uuid_detection(true),
	  partial_paths(false),
	  is_namespace(false),
	  ngram(false),
	  cjk_ngram(false),
	  cjk_words(false),
	  field_found(true),
	  concrete(false),
	  complete(false),
	  uuid_field(false),
	  uuid_path(false),
	  inside_namespace(false),
#ifdef XAPIAND_LUA
	  normalized_script(false),
#endif
	  has_uuid_prefix(false),
	  has_bool_term(false),
	  has_index(false),
	  has_namespace(false),
	  has_partial_paths(false),
	  static_foreign(false) { }


std::string
required_spc_t::prefix_t::to_string() const
{
	auto res = repr(field);
	if (uuid.empty()) {
		return res;
	}
	res.insert(0, 1, '(').append(", ").append(repr(uuid)).push_back(')');
	return res;
}


std::string
required_spc_t::prefix_t::operator()() const noexcept
{
	return field;
}


required_spc_t::required_spc_t()
	: sep_types({{ FieldType::empty, FieldType::empty, FieldType::empty }}),
	  slot(engine::BAD_VALUENO),
	  stop_strategy(DEFAULT_STOP_STRATEGY),
	  stem_strategy(DEFAULT_STEM_STRATEGY),
	  error(DEFAULT_GEO_ERROR) { }


required_spc_t::required_spc_t(engine::valueno slot, FieldType type, std::vector<uint64_t> accuracy, std::vector<std::string> acc_prefix)
	: sep_types({{ FieldType::empty, FieldType::empty, type }}),
	  slot(slot),
	  accuracy(std::move(accuracy)),
	  acc_prefix(std::move(acc_prefix)),
	  stop_strategy(DEFAULT_STOP_STRATEGY),
	  stem_strategy(DEFAULT_STEM_STRATEGY),
	  error(DEFAULT_GEO_ERROR) { }


required_spc_t::required_spc_t(const required_spc_t& o) = default;


required_spc_t::required_spc_t(required_spc_t&& o) noexcept
	: sep_types(std::move(o.sep_types)),
	  prefix(std::move(o.prefix)),
	  slot(std::move(o.slot)),
	  flags(std::move(o.flags)),
	  accuracy(std::move(o.accuracy)),
	  acc_prefix(std::move(o.acc_prefix)),
	  ignored(std::move(o.ignored)),
	  language(std::move(o.language)),
	  stop_strategy(std::move(o.stop_strategy)),
	  stem_strategy(std::move(o.stem_strategy)),
	  stem_language(std::move(o.stem_language)),
	  error(std::move(o.error)) { }


required_spc_t&
required_spc_t::operator=(const required_spc_t& o) = default;


required_spc_t&
required_spc_t::operator=(required_spc_t&& o) noexcept
{
	sep_types = std::move(o.sep_types);
	prefix = std::move(o.prefix);
	slot = std::move(o.slot);
	flags = std::move(o.flags);
	accuracy = std::move(o.accuracy);
	acc_prefix = std::move(o.acc_prefix);
	ignored = std::move(o.ignored);
	language = std::move(o.language);
	stop_strategy = std::move(o.stop_strategy);
	stem_strategy = std::move(o.stem_strategy);
	stem_language = std::move(o.stem_language);
	error = std::move(o.error);
	return *this;
}


const std::array<FieldType, SPC_TOTAL_TYPES>&
required_spc_t::get_types(std::string_view str_type)
{
	L_CALL("required_spc_t::get_types({})", repr(str_type));

	const auto& type = _get_type(str_type);
	if (std::string_view(reinterpret_cast<const char*>(type.data()), SPC_TOTAL_TYPES) == (EMPTY + EMPTY + EMPTY)) {
		THROW(ClientError, "{} not supported, '{}' must be one of {{ 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', 'keyword', 'string', 'text', 'time', 'timedelta', 'uuid' }} or any of their {{ 'array/<type>', 'foreign/<type>', 'foreign/array/<type>' }} variants.", repr(str_type), RESERVED_TYPE);
	}
	return type;
}


std::string_view
required_spc_t::get_str_type(const std::array<FieldType, SPC_TOTAL_TYPES>& sep_types)
{
	L_CALL("required_spc_t::get_str_type({{ {}, {}, {} }})", toUType(sep_types[SPC_FOREIGN_TYPE]), toUType(sep_types[SPC_ARRAY_TYPE]), toUType(sep_types[SPC_CONCRETE_TYPE]));

	return _get_str_type(sep_types);
}


void
required_spc_t::set_types(std::string_view str_type)
{
	L_CALL("required_spc_t::set_types({})", repr(str_type));

	sep_types = get_types(str_type);
}


MsgPack
required_spc_t::to_obj() const
{
	MsgPack obj;

	// required_spc_t

	obj["type"] = _get_str_type(sep_types);
	obj["prefix"] = prefix.to_string();
	obj["slot"] = slot;

	auto& obj_flags = obj["flags"] = MsgPack::MAP();
	obj_flags["bool_term"] = flags.bool_term;
	obj_flags["partials"] = flags.partials;

	obj_flags["store"] = flags.store;
	obj_flags["parent_store"] = flags.parent_store;
	obj_flags["recurse"] = flags.recurse;
	obj_flags["dynamic"] = flags.dynamic;
	obj_flags["strict"] = flags.strict;
	obj_flags["date_detection"] = flags.date_detection;
	obj_flags["datetime_detection"] = flags.datetime_detection;
	obj_flags["time_detection"] = flags.time_detection;
	obj_flags["timedelta_detection"] = flags.timedelta_detection;
	obj_flags["numeric_detection"] = flags.numeric_detection;
	obj_flags["geo_detection"] = flags.geo_detection;
	obj_flags["bool_detection"] = flags.bool_detection;
	obj_flags["text_detection"] = flags.text_detection;
	obj_flags["uuid_detection"] = flags.uuid_detection;

	obj_flags["partial_paths"] = flags.partial_paths;
	obj_flags["is_namespace"] = flags.is_namespace;

	obj_flags["field_found"] = flags.field_found;
	obj_flags["concrete"] = flags.concrete;
	obj_flags["complete"] = flags.complete;
	obj_flags["uuid_field"] = flags.uuid_field;
	obj_flags["uuid_path"] = flags.uuid_path;
	obj_flags["inside_namespace"] = flags.inside_namespace;
#ifdef XAPIAND_LUA
	obj_flags["normalized_script"] = flags.normalized_script;
#endif
	obj_flags["has_uuid_prefix"] = flags.has_uuid_prefix;
	obj_flags["has_bool_term"] = flags.has_bool_term;
	obj_flags["has_index"] = flags.has_index;
	obj_flags["has_namespace"] = flags.has_namespace;
	obj_flags["has_partial_paths"] = flags.has_partial_paths;
	obj_flags["static_foreign"] = flags.static_foreign;
	obj_flags["ngram"] = flags.ngram;
	obj_flags["cjk_ngram"] = flags.cjk_ngram;
	obj_flags["cjk_words"] = flags.cjk_words;

	auto& obj_accuracy = obj["accuracy"] = MsgPack::ARRAY();
	for (const auto& a : accuracy) {
		obj_accuracy.append(a);
	}

	auto& obj_acc_prefix = obj["acc_prefix"] = MsgPack::ARRAY();
	for (const auto& a : acc_prefix) {
		obj_acc_prefix.append(a);
	}

	auto& obj_ignore = obj["ignored"] = MsgPack::ARRAY();
	for (const auto& a : ignored) {
		obj_ignore.append(a);
	}

	obj["language"] = language;
	obj["stop_strategy"] = enum_name(stop_strategy);
	obj["stem_strategy"] = enum_name(stem_strategy);
	obj["stem_language"] = stem_language;

	obj["error"] = error;

	return obj;
}


std::string
required_spc_t::to_string(int indent) const
{
	return to_obj().to_string(indent);
}


index_spc_t::index_spc_t(required_spc_t&& spc)
	: type(std::move(spc.sep_types[SPC_CONCRETE_TYPE])),
	  prefix(std::move(spc.prefix.field)),
	  slot(std::move(spc.slot)),
	  accuracy(std::move(spc.accuracy)),
	  acc_prefix(std::move(spc.acc_prefix)) { }


index_spc_t::index_spc_t(const required_spc_t& spc)
	: type(spc.sep_types[SPC_CONCRETE_TYPE]),
	  prefix(spc.prefix.field),
	  slot(spc.slot),
	  accuracy(spc.accuracy),
	  acc_prefix(spc.acc_prefix) { }


specification_t::specification_t()
	: position({ 0 }),
	  weight({ 1 }),
	  spelling({ DEFAULT_SPELLING }),
	  positions({ DEFAULT_POSITIONS }),
	  index(DEFAULT_INDEX),
	  index_uuid_field(DEFAULT_INDEX_UUID_FIELD) { }


specification_t::specification_t(engine::valueno slot, FieldType type, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix)
	: required_spc_t(slot, type, accuracy, acc_prefix),
	  position({ 0 }),
	  weight({ 1 }),
	  spelling({ DEFAULT_SPELLING }),
	  positions({ DEFAULT_POSITIONS }),
	  index(DEFAULT_INDEX),
	  index_uuid_field(DEFAULT_INDEX_UUID_FIELD) { }


specification_t::specification_t(const specification_t& o)
	: required_spc_t(o),
	  local_prefix(o.local_prefix),
	  position(o.position),
	  weight(o.weight),
	  spelling(o.spelling),
	  positions(o.positions),
	  index(o.index),
	  index_uuid_field(o.index_uuid_field),
	  value_rec(o.value_rec ? std::make_unique<const MsgPack>(*o.value_rec) : nullptr),
	  value(o.value ? std::make_unique<const MsgPack>(*o.value) : nullptr),
	  doc_acc(o.doc_acc ? std::make_unique<const MsgPack>(*o.doc_acc) : nullptr),
#ifdef XAPIAND_LUA
	  script(o.script ? std::make_unique<const MsgPack>(*o.script) : nullptr),
#endif
	  endpoint(o.endpoint),
	  meta_name(o.meta_name),
	  full_meta_name(o.full_meta_name),
	  aux_stem_language(o.aux_stem_language),
	  aux_language(o.aux_language),
	  partial_prefixes(o.partial_prefixes),
	  partial_index_spcs(o.partial_index_spcs) { }


specification_t::specification_t(specification_t&& o) noexcept
	: required_spc_t(std::move(o)),
	  local_prefix(std::move(o.local_prefix)),
	  position(std::move(o.position)),
	  weight(std::move(o.weight)),
	  spelling(std::move(o.spelling)),
	  positions(std::move(o.positions)),
	  index(std::move(o.index)),
	  index_uuid_field(std::move(o.index_uuid_field)),
	  value_rec(std::move(o.value_rec)),
	  value(std::move(o.value)),
	  doc_acc(std::move(o.doc_acc)),
#ifdef XAPIAND_LUA
	  script(std::move(o.script)),
#endif
	  endpoint(std::move(o.endpoint)),
	  meta_name(std::move(o.meta_name)),
	  full_meta_name(std::move(o.full_meta_name)),
	  aux_stem_language(std::move(o.aux_stem_language)),
	  aux_language(std::move(o.aux_language)),
	  partial_prefixes(std::move(o.partial_prefixes)),
	  partial_index_spcs(std::move(o.partial_index_spcs)) { }


specification_t&
specification_t::operator=(const specification_t& o)
{
	local_prefix = o.local_prefix;
	position = o.position;
	weight = o.weight;
	spelling = o.spelling;
	positions = o.positions;
	index = o.index;
	index_uuid_field = o.index_uuid_field;
	value_rec = o.value_rec ? std::make_unique<const MsgPack>(*o.value_rec) : nullptr;
	value = o.value ? std::make_unique<const MsgPack>(*o.value) : nullptr;
	doc_acc = o.doc_acc ? std::make_unique<const MsgPack>(*o.doc_acc) : nullptr;
#ifdef XAPIAND_LUA
	script = o.script ? std::make_unique<const MsgPack>(*o.script) : nullptr;
#endif
	endpoint = o.endpoint;
	meta_name = o.meta_name;
	full_meta_name = o.full_meta_name;
	aux_stem_language = o.aux_stem_language;
	aux_language = o.aux_language;
	partial_prefixes = o.partial_prefixes;
	partial_index_spcs = o.partial_index_spcs;
	required_spc_t::operator=(o);
	return *this;
}


specification_t&
specification_t::operator=(specification_t&& o) noexcept
{
	local_prefix = std::move(o.local_prefix);
	position = std::move(o.position);
	weight = std::move(o.weight);
	spelling = std::move(o.spelling);
	positions = std::move(o.positions);
	index = std::move(o.index);
	index_uuid_field = std::move(o.index_uuid_field);
	value_rec = std::move(o.value_rec);
	value = std::move(o.value);
	doc_acc = std::move(o.doc_acc);
#ifdef XAPIAND_LUA
	script = std::move(o.script);
#endif
	endpoint = std::move(o.endpoint);
	meta_name = std::move(o.meta_name);
	full_meta_name = std::move(o.full_meta_name);
	aux_stem_language = std::move(o.aux_stem_language);
	aux_language = std::move(o.aux_language);
	partial_prefixes = std::move(o.partial_prefixes);
	partial_index_spcs = std::move(o.partial_index_spcs);
	required_spc_t::operator=(std::move(o));
	return *this;
}


FieldType
specification_t::global_type(FieldType field_type)
{
	switch (field_type) {
		case FieldType::object:
		case FieldType::floating:
		case FieldType::integer:
		case FieldType::positive:
		case FieldType::boolean:
		case FieldType::date:
		case FieldType::datetime:
		case FieldType::time:
		case FieldType::timedelta:
		case FieldType::geo:
		case FieldType::uuid:
		case FieldType::keyword:
			return field_type;

		case FieldType::text:
			return FieldType::text;

		default:
			THROW(ClientError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}
}


const specification_t&
specification_t::get_global(FieldType field_type)
{
	switch (field_type) {
		case FieldType::object: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::object, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::floating: {
			static const specification_t spc(DB_SLOT_NUMERIC, FieldType::floating, def_accuracy_num, global_acc_prefix_num);
			return spc;
		}
		case FieldType::integer: {
			static const specification_t spc(DB_SLOT_NUMERIC, FieldType::integer, def_accuracy_num, global_acc_prefix_num);
			return spc;
		}
		case FieldType::positive: {
			static const specification_t spc(DB_SLOT_NUMERIC, FieldType::positive, def_accuracy_num, global_acc_prefix_num);
			return spc;
		}
		case FieldType::boolean: {
			static const specification_t spc(DB_SLOT_BOOLEAN, FieldType::boolean, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::date: {
			static const specification_t spc(DB_SLOT_DATE, FieldType::date, def_accuracy_datetime, global_acc_prefix_date);
			return spc;
		}
		case FieldType::datetime: {
			static const specification_t spc(DB_SLOT_DATE, FieldType::datetime, def_accuracy_datetime, global_acc_prefix_date);
			return spc;
		}
		case FieldType::time: {
			static const specification_t spc(DB_SLOT_TIME, FieldType::time, def_accuracy_time, global_acc_prefix_time);
			return spc;
		}
		case FieldType::timedelta: {
			static const specification_t spc(DB_SLOT_TIMEDELTA, FieldType::timedelta, def_accuracy_time, global_acc_prefix_time);
			return spc;
		}
		case FieldType::geo: {
			static const specification_t spc(DB_SLOT_GEO, FieldType::geo, def_accuracy_geo, global_acc_prefix_geo);
			return spc;
		}
		case FieldType::uuid: {
			static const specification_t spc(DB_SLOT_UUID, FieldType::uuid, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::keyword:
		case FieldType::text: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::text, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		default:
			THROW(ClientError, "Type: {:#04x} is an unknown type", toUType(field_type));
	}
}


void
specification_t::update(index_spc_t&& spc)
{
	sep_types[SPC_CONCRETE_TYPE] = std::move(spc.type);
	prefix.field = std::move(spc.prefix);
	slot = std::move(spc.slot);
	accuracy = std::move(spc.accuracy);
	acc_prefix = std::move(spc.acc_prefix);
}


void
specification_t::update(const index_spc_t& spc)
{
	sep_types[SPC_CONCRETE_TYPE] = spc.type;
	prefix.field = spc.prefix;
	slot = spc.slot;
	accuracy = spc.accuracy;
	acc_prefix = spc.acc_prefix;
}


MsgPack
specification_t::to_obj() const
{
	MsgPack obj = required_spc_t::to_obj();

	// specification_t

	obj["local_prefix"] = local_prefix.to_string();

	auto& obj_position = obj["position"] = MsgPack::ARRAY();
	for (const auto& p : position) {
		obj_position.append(p);
	}

	auto& obj_weight = obj["weight"] = MsgPack::ARRAY();
	for (const auto& w : weight) {
		obj_weight.append(w);
	}

	auto& obj_spelling = obj["spelling"] = MsgPack::ARRAY();
	for (const auto& s : spelling) {
		obj_spelling.append(static_cast<bool>(s));
	}

	auto& obj_positions = obj["positions"] = MsgPack::ARRAY();
	for (const auto& p : positions) {
		obj_positions.append(static_cast<bool>(p));
	}

	obj["index"] = _get_str_index(index);

	obj["index_uuid_field"] = _get_str_index_uuid_field(index_uuid_field);

	obj["value_rec"] = value_rec ? value_rec->to_string() : MsgPack::NIL();
	obj["value"] = value ? value->to_string() : MsgPack::NIL();
	obj["doc_acc"] = doc_acc ? doc_acc->to_string() : MsgPack::NIL();
#ifdef XAPIAND_LUA
	obj["script"] = script ? script->to_string() : MsgPack::NIL();
#endif

	obj["endpoint"] = endpoint;

	obj["meta_name"] = meta_name;
	obj["full_meta_name"] = full_meta_name;

	obj["aux_stem_language"] = aux_stem_language;
	obj["aux_language"] = aux_language;

	auto& obj_partial_prefixes = obj["partial_prefixes"] = MsgPack::ARRAY();
	for (const auto& p : partial_prefixes) {
		obj_partial_prefixes.append(p.to_string());
	}

	auto& obj_partial_index_spcs = obj["partial_index_spcs"] = MsgPack::ARRAY();
	for (const auto& s : partial_index_spcs) {
		obj_partial_index_spcs.append(MsgPack({
			{ "prefix", repr(s.prefix) },
			{ "slot", s.slot },
		}));
	}

	return obj;
}


std::string
specification_t::to_string(int indent) const
{
	return to_obj().to_string(indent);
}
