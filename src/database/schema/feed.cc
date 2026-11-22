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

// Feeding a field's specification from the reserved properties a stored schema holds.

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


void
Schema::dispatch_feed_properties(const MsgPack& properties)
{
	L_CALL("Schema::dispatch_feed_properties({})", properties.to_string());

	const auto it_e = properties.end();
	for (auto it = properties.begin(); it != it_e; ++it) {
		auto str_key = it->str_view();
		if (is_reserved(str_key)) {
			auto& value = it.value();
			auto key = hh(str_key);
			_dispatch_feed_properties(key, value);
		}
	}
}


void
Schema::feed_weight(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_weight({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.weight.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_number()) {
				specification.weight.push_back(static_cast<engine::termpos>(prop_item_obj.u64()));
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_WEIGHT, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else if (prop_obj.is_number()) {
		specification.weight.clear();
		specification.weight.push_back(static_cast<engine::termpos>(prop_obj.u64()));
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_WEIGHT, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_position(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_position({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.position.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_number()) {
				specification.position.push_back(static_cast<engine::termpos>(prop_item_obj.u64()));
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_WEIGHT, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else if (prop_obj.is_number()) {
		specification.position.clear();
		specification.position.push_back(static_cast<engine::termpos>(prop_obj.u64()));
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_POSITION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_spelling(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_spelling({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.spelling.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_boolean()) {
				specification.spelling.push_back(prop_item_obj.boolean());
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_SPELLING, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else if (prop_obj.is_boolean()) {
		specification.spelling.clear();
		specification.spelling.push_back(prop_obj.boolean());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_SPELLING, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_positions(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_positions({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.positions.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_boolean()) {
				specification.positions.push_back(prop_item_obj.boolean());
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_POSITIONS, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else if (prop_obj.is_boolean()) {
		specification.positions.clear();
		specification.positions.push_back(prop_obj.boolean());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_POSITIONS, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_ngram(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_ngram({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.ngram = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_NGRAM, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_cjk_ngram(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_cjk_ngram({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.cjk_ngram = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_CJK_NGRAM, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_cjk_words(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_cjk_words({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.cjk_words = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_CJK_WORDS, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_language(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_language({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.language = prop_obj.str();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_LANGUAGE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_stop_strategy(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_stop_strategy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.stop_strategy = _get_stop_strategy(prop_obj.str_view());
		if (specification.stop_strategy == StopStrategy::INVALID) {
			THROW(Error, "Schema is corrupt: '{}' in {} must be one of {}.", RESERVED_STOP_STRATEGY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name), str_set_stop_strategy);
		}
	} else if (prop_obj.is_number()) {
		specification.stop_strategy = static_cast<StopStrategy>(prop_obj.u64());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_STOP_STRATEGY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_stem_strategy(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_stem_strategy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.stem_strategy = enum_type<StemStrategy>(prop_obj.str_view());
		if (specification.stem_strategy == StemStrategy::INVALID) {
			THROW(Error, "Schema is corrupt: '{}' in {} must be one of {}.", RESERVED_STEM_STRATEGY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name), str_set_stem_strategy);
		}
	} else if (prop_obj.is_number()) {
		specification.stem_strategy = static_cast<StemStrategy>(prop_obj.u64());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_STEM_STRATEGY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_stem_language(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_stem_language({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.stem_language = _get_stemmer_name(prop_obj.str_view());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_STEM_LANGUAGE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_type(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_type({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.set_types(prop_obj.str_view());
		specification.flags.concrete = specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty;
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_TYPE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_accuracy(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_accuracy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.accuracy.clear();
		specification.accuracy.reserve(prop_obj.size());
		for (const auto& prop_item_obj : prop_obj) {
			uint64_t accuracy;
			if (prop_item_obj.is_string()) {
				auto accuracy_date = _get_accuracy_datetime(prop_item_obj.str_view());
				if (accuracy_date != UnitTime::INVALID) {
					accuracy = toUType(accuracy_date);
				} else {
					THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ACCURACY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
			} else if (prop_item_obj.is_number()) {
				accuracy = prop_item_obj.u64();
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ACCURACY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.accuracy.push_back(accuracy);
		}
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ACC_PREFIX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_acc_prefix(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_acc_prefix({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.acc_prefix.clear();
		specification.acc_prefix.reserve(prop_obj.size());
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_string()) {
				specification.acc_prefix.push_back(prop_item_obj.str());
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ACC_PREFIX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ACC_PREFIX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_prefix(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_prefix({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.local_prefix.field.assign(prop_obj.str_view());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_PREFIX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_slot(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_slot({})", repr(prop_obj.to_string()));

	if (prop_obj.is_number()) {
		specification.slot = static_cast<engine::valueno>(prop_obj.u64());
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_SLOT, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_index(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_index({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.index = _get_index(prop_obj.str_view());
		if (specification.index == TypeIndex::INVALID) {
			THROW(Error, "Schema is corrupt: '{}' in {} must be one of {}.", RESERVED_INDEX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name), str_set_index);
		}
		specification.flags.has_index = true;
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_INDEX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_store(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_store({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.parent_store = specification.flags.store;
		specification.flags.store = prop_obj.boolean() && specification.flags.parent_store;
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_STORE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_recurse(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_recurse({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.recurse = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_RECURSE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_ignore(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_ignore({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.ignored.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_string()) {
				auto ignored = prop_item_obj.str();
				if (ignored == "*") {
					specification.flags.recurse = false;
				}
				specification.ignored.insert(ignored);
			} else {
				THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_INDEX, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else if (prop_obj.is_string()) {
		auto ignored = prop_obj.str();
		if (ignored == "*") {
			specification.flags.recurse = false;
		}
		specification.ignored.clear();
		specification.ignored.insert(ignored);
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_IGNORE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_dynamic(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_dynamic({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.dynamic = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_DYNAMIC, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_strict(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_strict({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.strict = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_STRICT, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_date_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_date_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.date_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_DATE_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_datetime_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_datetime_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.datetime_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_DATETIME_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_time_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_time_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.time_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_TIME_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_timedelta_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_timedelta_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.timedelta_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_TIMEDELTA_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_numeric_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_numeric_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.numeric_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_NUMERIC_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_geo_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_geo_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.geo_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_GEO_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_bool_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_bool_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.bool_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_BOOL_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_text_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_text_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.text_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_TEXT_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_uuid_detection(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_uuid_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.uuid_detection = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_UUID_DETECTION, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_bool_term(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_bool_term({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.bool_term = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_BOOL_TERM, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_partials(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_partials({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.partials = prop_obj.boolean();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_PARTIALS, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_error(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_error({})", repr(prop_obj.to_string()));

	if (prop_obj.is_number()) {
		specification.error = prop_obj.f64();
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ERROR, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_namespace(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_namespace({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.is_namespace = prop_obj.boolean();
		if (specification.flags.is_namespace && !specification.flags.has_partial_paths) {
			specification.flags.partial_paths = true;  // namespaces with partial paths ON by default
		}
		specification.flags.has_namespace = true;
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_NAMESPACE, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_partial_paths(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_partial_paths({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.partial_paths = prop_obj.boolean();
		specification.flags.has_partial_paths = true;
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_PARTIAL_PATHS, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_index_uuid_field(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_index_uuid_field({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.index_uuid_field = _get_index_uuid_field(prop_obj.str_view());
		if (specification.index_uuid_field == UUIDFieldIndex::INVALID) {
			THROW(Error, "Schema is corrupt: '{}' in {} must be one of {}.", RESERVED_INDEX_UUID_FIELD, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name), str_set_index_uuid_field);
		}
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_INDEX_UUID_FIELD, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}


void
Schema::feed_script([[maybe_unused]] const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_script({})", repr(prop_obj.to_string()));

#ifdef XAPIAND_LUA
	specification.script = std::make_unique<const MsgPack>(prop_obj);
	specification.flags.normalized_script = true;
#else
	THROW(ClientError, "{} only is allowed when Lua scripting is active", RESERVED_SCRIPT);
#endif
}


void
Schema::feed_foreign(const MsgPack& prop_obj)
{
	L_CALL("Schema::feed_foreign({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.endpoint.assign(prop_obj.str_view());
		specification.flags.static_foreign = true;
	} else {
		THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_FOREIGN, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
	}
}
