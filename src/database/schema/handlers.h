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

// The handlers the dispatch tables inline, run for a concrete field's reserved properties on
// every document: process_* for the concrete ones, consistency_* for those fixed once, and
// the script's.

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


using namespace schema_detail;


inline void
Schema::process_data(std::string_view /*unused*/, [[maybe_unused]] const MsgPack& prop_obj)
{
	// RESERVED_DATA is ignored by the schema.
	L_CALL("Schema::process_data({})", repr(prop_obj.to_string()));
}


inline void
Schema::process_weight(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_WEIGHT property is heritable and can change between documents.
	L_CALL("Schema::process_weight({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		if (prop_obj.empty()) {
			THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
		}
		specification.weight.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_number()) {
				specification.weight.push_back(static_cast<unsigned>(prop_item_obj.u64()));
			} else {
				THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
			}
		}
	} else if (prop_obj.is_number()) {
		specification.weight.clear();
		specification.weight.push_back(static_cast<unsigned>(prop_obj.u64()));
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
	}
}


inline void
Schema::process_spelling(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	L_CALL("Schema::process_spelling({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		if (prop_obj.empty()) {
			THROW(ClientError, "Data inconsistency, {} must be a boolean or a not-empty array of booleans", repr(prop_name));
		}
		specification.spelling.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_boolean()) {
				specification.spelling.push_back(prop_item_obj.boolean());
			} else {
				THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
			}
		}
	} else if (prop_obj.is_boolean()) {
		specification.spelling.clear();
		specification.spelling.push_back(prop_obj.boolean());
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a boolean or a not-empty array of booleans", repr(prop_name));
	}
}


inline void
Schema::process_positions(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	L_CALL("Schema::process_positions({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		if (prop_obj.empty()) {
			THROW(ClientError, "Data inconsistency, {} must be a boolean or a not-empty array of booleans", repr(prop_name));
		}
		specification.positions.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_boolean()) {
				specification.positions.push_back(prop_item_obj.boolean());
			} else {
				THROW(ClientError, "Data inconsistency, {} must be a boolean or a not-empty array of booleans", repr(prop_name));
			}
		}
	} else if (prop_obj.is_boolean()) {
		specification.positions.clear();
		specification.positions.push_back(prop_obj.boolean());
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a boolean or a not-empty array of booleans", repr(prop_name));
	}
}


inline void
Schema::process_index(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_INDEX is heritable and can change.
	L_CALL("Schema::process_index({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		auto str_index = prop_obj.str_view();
		specification.index = _get_index(str_index);
		if (specification.index == TypeIndex::INVALID) {
			THROW(ClientError, "{} not supported, {} must be one of {}", repr(str_index), repr(prop_name), str_set_index);
		}
		specification.flags.has_index = true;
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::process_store(std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::process_store({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_STORE is heritable and can change, but once fixed in false
	 * it cannot change in its offsprings.
	 */

	if (prop_obj.is_boolean()) {
		specification.flags.store = specification.flags.parent_store && prop_obj.boolean();
		specification.flags.parent_store = specification.flags.store;
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::process_recurse(std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::process_recurse({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_RECURSE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */

	if (prop_obj.is_boolean()) {
		specification.flags.recurse = prop_obj.boolean();
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::process_ignore(std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::process_ignore({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_IGNORE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */

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
				THROW(ClientError, "Data inconsistency, {} must be an array of strings", repr(prop_name));
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
		THROW(ClientError, "Data inconsistency, {} must be an array of strings", repr(prop_name));
	}
}


inline void
Schema::process_partial_paths(std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::process_partial_paths({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_PARTIAL_PATHS is heritable and can change.
	 */

	if (prop_obj.is_boolean()) {
		specification.flags.partial_paths = prop_obj.boolean();
		specification.flags.has_partial_paths = true;
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::process_index_uuid_field(std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::process_index_uuid_field({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_INDEX_UUID_FIELD is heritable and can change.
	 */

	if (prop_obj.is_string()) {
		auto str_index_uuid_field = prop_obj.str_view();
		specification.index_uuid_field = _get_index_uuid_field(str_index_uuid_field);
		if (specification.index_uuid_field == UUIDFieldIndex::INVALID) {
			THROW(ClientError, "{} not supported, {} must be one of {}", repr(str_index_uuid_field), repr(prop_name), str_set_index_uuid_field);
		}
	} else {
		THROW(ClientError, "Type not supported, {} must be one of {}", repr(prop_name), str_set_index_uuid_field);
	}
}


inline void
Schema::process_value(std::string_view /*unused*/, const MsgPack& prop_obj)
{
	// RESERVED_VALUE isn't heritable and is not saved in schema.
	L_CALL("Schema::process_value({})", repr(prop_obj.to_string()));

	if (specification.value || specification.value_rec) {
		THROW(ClientError, "Object already has a value");
	} else {
		specification.value = std::make_unique<const MsgPack>(prop_obj);
	}
}


inline void
Schema::process_script(std::string_view /*unused*/, [[maybe_unused]] const MsgPack& prop_obj)
{
	// RESERVED_SCRIPT isn't heritable.
	L_CALL("Schema::process_script({})", repr(prop_obj.to_string()));

#ifdef XAPIAND_LUA
	specification.script = std::make_unique<const MsgPack>(prop_obj);
	specification.flags.normalized_script = false;
#else
	THROW(ClientError, "'{}' only is allowed when Lua scripting is active", RESERVED_SCRIPT);
#endif
}


inline void
Schema::process_foreign(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_FOREIGN isn't heritable.
	L_CALL("Schema::process_foreign({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		const auto _foreign = prop_obj.str_view();
		if (_foreign.empty()) {
			THROW(ClientError, "Data inconsistency, {} must be a valid endpoint", repr(prop_name));
		}
		std::string_view _path, _id;
		split_path_id(_foreign, _path, _id);
		if (_path.empty() || _id.empty()) {
			THROW(ClientError, "Data inconsistency, {} must be a valid endpoint", repr(prop_name));
		}
		if (specification.endpoint != _foreign) {
			if (
				specification.sep_types[SPC_FOREIGN_TYPE] != FieldType::foreign && (
					specification.sep_types[SPC_ARRAY_TYPE] != FieldType::empty ||
					specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty
				)
			) {
				THROW(ClientError, "Data inconsistency, {} cannot be used in non-foreign fields", repr(prop_name));
			}
			specification.flags.static_foreign = false;
			specification.endpoint.assign(_foreign);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::process_strict(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STRICT is heritable and can change.
	L_CALL("Schema::process_strict({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.strict = prop_obj.boolean();
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::process_cast_object(std::string_view prop_name, const MsgPack& prop_obj)
{
	// This property isn't heritable and is not saved in schema.
	L_CALL("Schema::process_cast_object({})", repr(prop_obj.to_string()));

	if (specification.value || specification.value_rec) {
		THROW(ClientError, "Object already has a value");
	} else {
		specification.value_rec = std::make_unique<const MsgPack>(MsgPack({
			{ prop_name, prop_obj },
		}));
	}
}


inline void
Schema::consistency_slot(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_SLOT isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_slot({})", repr(prop_obj.to_string()));

	if (prop_obj.is_number()) {
		auto slot = static_cast<engine::valueno>(prop_obj.u64());
		if (specification.slot != slot) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), specification.slot, slot, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be integer", repr(prop_name));
	}
}


inline void
Schema::consistency_ngram(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_ngram({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto ngram = prop_obj.boolean();
		if (specification.flags.ngram != ngram) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), bool(specification.flags.ngram), ngram, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::consistency_cjk_ngram(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_cjk_ngram({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto cjk_ngram = prop_obj.boolean();
		if (specification.flags.cjk_ngram != cjk_ngram) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), bool(specification.flags.cjk_ngram), cjk_ngram, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::consistency_cjk_words(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_cjk_words({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto cjk_words = prop_obj.boolean();
		if (specification.flags.cjk_words != cjk_words) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), bool(specification.flags.cjk_words), cjk_words, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}



inline void
Schema::consistency_language(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_language({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		const auto str_language = prop_obj.str_view();
		if (specification.language != str_language) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), specification.language, repr(str_language), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::consistency_stop_strategy(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STOP_STRATEGY isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_stop_strategy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::text) {
			const auto _stop_strategy = strings::lower(prop_obj.str_view());
			const auto stop_strategy = enum_name(specification.stop_strategy);
			if (stop_strategy != _stop_strategy) {
				THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), stop_strategy, _stop_strategy, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		} else {
			THROW(ClientError, "{} only is allowed in text type fields", repr(prop_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::consistency_stem_strategy(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STEM_STRATEGY isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_stem_strategy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::text) {
			const auto _stem_strategy = strings::lower(prop_obj.str_view());
			const auto stem_strategy = enum_name(specification.stem_strategy);
			if (stem_strategy != _stem_strategy) {
				THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), repr(stem_strategy), repr(_stem_strategy), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		} else {
			THROW(ClientError, "{} only is allowed in text type fields", repr(prop_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::consistency_stem_language(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STEM_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_stem_language({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::text) {
			const auto _stem_language = _get_stemmer_name(prop_obj.str_view());
			if (specification.stem_language != _stem_language) {
				THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), repr(specification.stem_language), repr(_stem_language), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		} else {
			THROW(ClientError, "{} only is allowed in text type fields", repr(prop_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


inline void
Schema::consistency_type(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_type({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		const auto _str_type = prop_obj.str_view();
		auto init_pos = _str_type.rfind('/');
		if (init_pos == std::string::npos) {
			init_pos = 0;
		} else {
			++init_pos;
		}
		const auto str_type = enum_name(specification.sep_types[SPC_CONCRETE_TYPE]);
		if (_str_type.compare(init_pos, std::string::npos, str_type) != 0) {
			auto str_concrete_type = _str_type.substr(init_pos);
			if (((str_concrete_type != "string" || str_type != "text") && (str_concrete_type != "text" || str_type != "string")) &&
				((str_concrete_type != "term" || str_type != "keyword") && (str_concrete_type != "keyword" || str_type != "term")) &&
				((str_concrete_type != "float" || str_type != "floating") && (str_concrete_type != "floating" || str_type != "float")) &&
				((str_concrete_type != "geo" || str_type != "geospatial") && (str_concrete_type != "geospatial" || str_type != "geo"))) {
				// FIXME: remove legacy types
				THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), repr(str_type), repr(str_concrete_type), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}

	if (!specification.endpoint.empty()) {
		if (specification.sep_types[SPC_FOREIGN_TYPE] != FieldType::foreign) {
			THROW(ClientError, "Data inconsistency, {} must be foreign", repr(prop_name));
		}
	}
}


inline void
Schema::consistency_accuracy(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_ACCURACY isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_accuracy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		std::set<uint64_t> set_acc;
		switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
			case FieldType::geo: {
				if (prop_obj.is_array()) {
					for (const auto& prop_item_obj : prop_obj) {
						if (prop_item_obj.is_number()) {
							set_acc.insert(prop_item_obj.u64());
						} else {
							THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
						}
					}
				} else {
					THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::vector<std::string> str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.push_back(strings::format("{}", acc));
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.push_back(strings::format("{}", acc));
					}
					THROW(ClientError, "It is not allowed to change {} [({})  ->  ({})] in {}", repr(prop_name), strings::join(str_accuracy, ", "), strings::join(_str_accuracy, ", "), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				return;
			}
			case FieldType::date:
			case FieldType::datetime: {
				if (prop_obj.is_array()) {
					for (const auto& prop_item_obj : prop_obj) {
						uint64_t accuracy;
						if (prop_item_obj.is_string()) {
							auto accuracy_date = _get_accuracy_datetime(prop_item_obj.str_view());
							if (accuracy_date != UnitTime::INVALID) {
								accuracy = toUType(accuracy_date);
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date), repr(prop_item_obj.str_view()));
							}
						} else if (prop_item_obj.is_number()) {
							accuracy = prop_item_obj.u64();
							if (!validate_acc_date(static_cast<UnitTime>(accuracy))) {
								THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date));
							}
						} else {
							THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date));
						}
						set_acc.insert(accuracy);
					}
				} else {
					THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date));
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::vector<std::string> str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.push_back(repr(_get_str_acc_date(static_cast<UnitTime>(acc))));
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.push_back(repr(_get_str_acc_date(static_cast<UnitTime>(acc))));
					}
					THROW(ClientError, "It is not allowed to change {} [({})  ->  ({})] in {}", repr(prop_name), strings::join(str_accuracy, ", "), strings::join(_str_accuracy, ", "), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				return;
			}
			case FieldType::time:
			case FieldType::timedelta: {
				if (prop_obj.is_array()) {
					for (const auto& prop_item_obj : prop_obj) {
						if (prop_item_obj.is_string()) {
							auto accuracy_time = _get_accuracy_time(prop_item_obj.str_view());
							if (accuracy_time != UnitTime::INVALID) {
								set_acc.insert(toUType(accuracy_time));
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date), repr(prop_item_obj.str_view()));
							}
						} else {
							THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(prop_item_obj.str_view()));
						}
					}
				} else {
					THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time));
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::vector<std::string> str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.push_back(repr(_get_str_acc_date(static_cast<UnitTime>(acc))));
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.push_back(repr(_get_str_acc_date(static_cast<UnitTime>(acc))));
					}
					THROW(ClientError, "It is not allowed to change {} [({})  ->  ({})] in {}", repr(prop_name), strings::join(str_accuracy, ", "), strings::join(_str_accuracy, ", "), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				return;
			}
			case FieldType::integer:
			case FieldType::positive:
			case FieldType::floating: {
				if (prop_obj.is_array()) {
					for (const auto& prop_item_obj : prop_obj) {
						if (prop_item_obj.is_number()) {
							set_acc.insert(prop_item_obj.u64());
						} else {
							THROW(ClientError, "Data inconsistency, {} in {} must be an array of positive numbers in {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
						}
					}
				} else {
					THROW(ClientError, "Data inconsistency, {} in {} must be an array of positive numbers in {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::vector<std::string> str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.push_back(strings::format("{}", acc));
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.push_back(strings::format("{}", acc));
					}
					THROW(ClientError, "It is not allowed to change {} [({})  ->  ({})] in {}", repr(prop_name), strings::join(str_accuracy, ", "), strings::join(_str_accuracy, ", "), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				return;
			}
			default:
				THROW(ClientError, "{} is not allowed in {} type fields", repr(prop_name), enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be array", repr(prop_name));
	}
}


inline void
Schema::consistency_bool_term(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	L_CALL("Schema::consistency_bool_term({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::keyword) {
			const auto _bool_term = prop_obj.boolean();
			if (specification.flags.bool_term != _bool_term) {
				THROW(ClientError, "It is not allowed to change {} [{}  ->  {}] in {}", repr(prop_name), bool(specification.flags.bool_term), _bool_term, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		} else {
			THROW(ClientError, "{} only is allowed in keyword type fields", repr(prop_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_partials(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_partials({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::geo) {
			const auto _partials = prop_obj.boolean();
			if (specification.flags.partials != _partials) {
				THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.partials), _partials);
			}
		} else {
			THROW(ClientError, "{} only is allowed in geospatial type fields", repr(prop_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_error(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_error({})", repr(prop_obj.to_string()));

	if (prop_obj.is_number()) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::geo) {
			const auto _error = prop_obj.f64();
			if (specification.error != _error) {
				THROW(ClientError, "It is not allowed to change {} [{:.2}  ->  {:.2}]", repr(prop_name), specification.error, _error);
			}
		} else {
			THROW(ClientError, "{} only is allowed in geospatial type fields", repr(prop_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a double", repr(prop_name));
	}
}


inline void
Schema::consistency_dynamic(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	L_CALL("Schema::consistency_dynamic({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _dynamic = prop_obj.boolean();
		if (specification.flags.dynamic != _dynamic) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.dynamic), _dynamic);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_date_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_DATE_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_date_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _date_detection = prop_obj.boolean();
		if (specification.flags.date_detection != _date_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.date_detection), _date_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_datetime_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_DATETIME_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_datetime_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _datetime_detection = prop_obj.boolean();
		if (specification.flags.datetime_detection != _datetime_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.datetime_detection), _datetime_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_time_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TIME_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_time_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _time_detection = prop_obj.boolean();
		if (specification.flags.time_detection != _time_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.time_detection), _time_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_timedelta_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TIMEDELTA_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_timedelta_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _timedelta_detection = prop_obj.boolean();
		if (specification.flags.timedelta_detection != _timedelta_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.timedelta_detection), _timedelta_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_numeric_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_NUMERIC_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_numeric_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _numeric_detection = prop_obj.boolean();
		if (specification.flags.numeric_detection != _numeric_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.numeric_detection), _numeric_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_geo_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_GEO_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_geo_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _geo_detection = prop_obj.boolean();
		if (specification.flags.geo_detection != _geo_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.geo_detection), _geo_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_bool_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_BOOL_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_bool_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _bool_detection = prop_obj.boolean();
		if (specification.flags.bool_detection != _bool_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.bool_detection), _bool_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_text_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TEXT_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_text_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _text_detection = prop_obj.boolean();
		if (specification.flags.text_detection != _text_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.text_detection), _text_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_uuid_detection(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_UUID_DETECTION is heritable and can't change.
	L_CALL("Schema::consistency_uuid_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _uuid_detection = prop_obj.boolean();
		if (specification.flags.uuid_detection != _uuid_detection) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.uuid_detection), _uuid_detection);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_namespace(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_NAMESPACE isn't heritable and can't change once fixed.
	L_CALL("Schema::consistency_namespace({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		const auto _is_namespace = prop_obj.boolean();
		if (specification.flags.is_namespace != _is_namespace) {
			THROW(ClientError, "It is not allowed to change {} [{}  ->  {}]", repr(prop_name), bool(specification.flags.is_namespace), _is_namespace);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


inline void
Schema::consistency_schema(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_SCHEMA isn't heritable and is only allowed in root object.
	L_CALL("Schema::consistency_schema({})", repr(prop_obj.to_string()));

	if (specification.full_meta_name.empty()) {
		if (!prop_obj.is_string() && !prop_obj.is_map()) {
			THROW(ClientError, "{} must be string or map", repr(prop_name));
		}
	} else {
		THROW(ClientError, "{} is only allowed in root object", repr(prop_name));
	}
}


inline void
Schema::consistency_settings(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_SETTINGS isn't heritable and is only allowed in root object.
	L_CALL("Schema::consistency_settings({})", repr(prop_obj.to_string()));

	if (specification.full_meta_name.empty()) {
		if (!prop_obj.is_map()) {
			THROW(ClientError, "{} must be string or map", repr(prop_name));
		}
	} else {
		THROW(ClientError, "{} is only allowed in root object", repr(prop_name));
	}
}


#ifdef XAPIAND_LUA
inline void
Schema::write_script(MsgPack& mut_properties)
{
	// RESERVED_SCRIPT isn't heritable and can't change once fixed.
	L_CALL("Schema::write_script({})", mut_properties.to_string());

	if (specification.script) {
		Script script(*specification.script);
		specification.script = std::make_unique<const MsgPack>(script.process_script(specification.flags.strict));
		mut_properties[RESERVED_SCRIPT] = *specification.script;
		specification.flags.normalized_script = true;
	}
}


inline void
Schema::normalize_script()
{
	// RESERVED_SCRIPT isn't heritable.
	L_CALL("Schema::normalize_script()");

	if (specification.script && !specification.flags.normalized_script) {
		Script script(*specification.script);
		specification.script = std::make_unique<const MsgPack>(script.process_script(specification.flags.strict));
		specification.flags.normalized_script = true;
	}
}
#endif
