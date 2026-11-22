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

// The reserved-property handlers the dispatch tables call out of line: the write_* ones,
// process_* for fields not concrete yet, the _id and _version defaults, and add_field.

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
#include "database/schema/walk.h"
#include "database/schema/handlers.h"
#include "database/schema/dispatch.h"


using namespace schema_detail;


void
Schema::dispatch_write_all_properties(MsgPack& mut_properties, const MsgPack& object, Fields& fields, Field** id_field, Field** version_field)
{
	L_CALL("Schema::dispatch_write_all_properties({}, {}, <fields>)", mut_properties.to_string(), object.to_string());

	auto it_e = object.end();
	for (auto it = object.begin(); it != it_e; ++it) {
		auto str_key = it->str_view();
		auto& value = it.value();
		if (is_reserved(str_key)) {
			auto key = hh(str_key);
			if (!_dispatch_write_properties(key, mut_properties, str_key, value)) {
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
			}
		} else {
			fields.emplace_back(str_key, &value);
		}
	}

#ifdef XAPIAND_LUA
	write_script(mut_properties);
#endif
}


void
Schema::add_field(MsgPack*& mut_properties, const MsgPack& object, Fields& fields)
{
	L_CALL("Schema::add_field({}, {}, <fields>)", mut_properties->to_string(), object.to_string());

	specification.flags.field_found = false;

	mut_properties = &mut_properties->get(specification.meta_name);

	const auto& stem = _get_stem_language(specification.meta_name);
	if (stem.first && stem.second != "unknown") {
		specification.language = stem.second;
		specification.aux_language = stem.second;
	}

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(specification.meta_name);
	} else {
		specification.full_meta_name.append(1, DB_OFFSPRING_UNION).append(specification.meta_name);
	}

	// Write obj specifications.
	dispatch_write_all_properties(*mut_properties, object, fields);

	// Load default specifications.
	dispatch_set_default_spc(*mut_properties);

	// Write prefix in properties.
	mut_properties->get(RESERVED_PREFIX) = specification.local_prefix.field;

	update_prefixes();
}


void
Schema::add_field(MsgPack*& mut_properties)
{
	L_CALL("Schema::add_field({})", mut_properties->to_string());

	mut_properties = &mut_properties->get(specification.meta_name);

	const auto& stem = _get_stem_language(specification.meta_name);
	if (stem.first && stem.second != "unknown") {
		specification.language = stem.second;
		specification.aux_language = stem.second;
	}

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(specification.meta_name);
	} else {
		specification.full_meta_name.append(1, DB_OFFSPRING_UNION).append(specification.meta_name);
	}

	// Load default specifications.
	dispatch_set_default_spc(*mut_properties);

	// Write prefix in properties.
	mut_properties->get(RESERVED_PREFIX) = specification.local_prefix.field;

	update_prefixes();
}


void
Schema::write_meta(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_META property is heritable and can change between documents.
	L_CALL("Schema::write_meta({})", repr(prop_obj.to_string()));

	process_meta(prop_name, prop_obj);
	mut_properties[prop_name] = prop_obj;
}


void
Schema::write_position(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_POSITION is heritable and can change between documents.
	L_CALL("Schema::write_position({})", repr(prop_obj.to_string()));

	process_position(prop_name, prop_obj);
	mut_properties[prop_name] = specification.position;
}


void
Schema::write_weight(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_WEIGHT property is heritable and can change between documents.
	L_CALL("Schema::write_weight({})", repr(prop_obj.to_string()));

	process_weight(prop_name, prop_obj);
	mut_properties[prop_name] = specification.weight;
}


void
Schema::write_spelling(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	L_CALL("Schema::write_spelling({})", repr(prop_obj.to_string()));

	process_spelling(prop_name, prop_obj);
	mut_properties[prop_name] = specification.spelling;
}


void
Schema::write_positions(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	L_CALL("Schema::write_positions({})", repr(prop_obj.to_string()));

	process_positions(prop_name, prop_obj);
	mut_properties[prop_name] = specification.positions;
}


void
Schema::write_index(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_INDEX is heritable and can change.
	L_CALL("Schema::write_index({})", repr(prop_obj.to_string()));

	process_index(prop_name, prop_obj);
	mut_properties[prop_name] = _get_str_index(specification.index);
}


void
Schema::write_store(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_store({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_STORE is heritable and can change, but once fixed in false
	 * it cannot change in its offsprings.
	 */

	process_store(prop_name, prop_obj);
	mut_properties[prop_name] = prop_obj.boolean();
}


void
Schema::write_recurse(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_recurse({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_RECURSE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */

	process_recurse(prop_name, prop_obj);
	mut_properties[prop_name] = static_cast<bool>(specification.flags.recurse);
}


void
Schema::write_ignore(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_ignore({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_IGNORE is not heritable and can change, but once fixed it ignores
	 * its specified children fields.
	 */

	process_ignore(prop_name, prop_obj);
	auto ignored = specification.ignored.size();
	if (ignored == 1) {
		mut_properties[prop_name] = *specification.ignored.begin();
	} else if (ignored > 1) {
		mut_properties[prop_name] = MsgPack::ARRAY();
		for (const auto& item : specification.ignored) {
			mut_properties[prop_name].append(item);
		}
	}
}


void
Schema::write_dynamic(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	L_CALL("Schema::write_dynamic({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.dynamic = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.dynamic);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_strict(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STRICT is heritable but can't change.
	L_CALL("Schema::write_strict({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.strict = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.strict);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_date_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_DATE_DETECTION is heritable and can't change.
	L_CALL("Schema::write_date_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.date_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.date_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_datetime_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_DATETIME_DETECTION is heritable and can't change.
	L_CALL("Schema::write_datetime_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.datetime_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.datetime_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_time_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TIME_DETECTION is heritable and can't change.
	L_CALL("Schema::write_time_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.time_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.time_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_timedelta_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TD_DETECTION is heritable and can't change.
	L_CALL("Schema::write_timedelta_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.timedelta_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.timedelta_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_numeric_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	L_CALL("Schema::write_numeric_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.numeric_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.numeric_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_geo_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	L_CALL("Schema::write_geo_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.geo_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.geo_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_bool_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	L_CALL("Schema::write_bool_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.bool_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.bool_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_text_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_T_DETECTION is heritable and can't change.
	L_CALL("Schema::write_text_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.text_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.text_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_uuid_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_U_DETECTION is heritable and can't change.
	L_CALL("Schema::write_uuid_detection({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.uuid_detection = prop_obj.boolean();
		mut_properties[prop_name] = static_cast<bool>(specification.flags.uuid_detection);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_bool_term(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	L_CALL("Schema::write_bool_term({})", repr(prop_obj.to_string()));

	process_bool_term(prop_name, prop_obj);
	mut_properties[prop_name] = static_cast<bool>(specification.flags.bool_term);
}


void
Schema::write_namespace(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_NAMESPACE isn't heritable and can't change once fixed.
	L_CALL("Schema::write_namespace({})", repr(prop_obj.to_string()));

	if (specification.flags.field_found) {
		return consistency_namespace(prop_name, prop_obj);
	}

	if (prop_obj.is_boolean()) {
		// Only save in Schema if RESERVED_NAMESPACE is true.
		specification.flags.is_namespace = prop_obj.boolean();
		if (specification.flags.is_namespace && !specification.flags.has_partial_paths) {
			specification.flags.partial_paths = true;  // namespaces with partial paths ON by default
		}
		specification.flags.has_namespace = true;
		mut_properties[prop_name] = static_cast<bool>(specification.flags.is_namespace);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::write_partial_paths(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_partial_paths({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_PARTIAL_PATHS is heritable and can change.
	 */

	process_partial_paths(prop_name, prop_obj);
	mut_properties[prop_name] = static_cast<bool>(specification.flags.partial_paths);
}


void
Schema::write_index_uuid_field(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_index_uuid_field({})", repr(prop_obj.to_string()));

	/*
	 * RESERVED_INDEX_UUID_FIELD is heritable and can change.
	 */

	process_index_uuid_field(prop_name, prop_obj);
	mut_properties[prop_name] = _get_str_index_uuid_field(specification.index_uuid_field);
}


void
Schema::write_schema(MsgPack& /*unused*/, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_schema({})", repr(prop_obj.to_string()));

	consistency_schema(prop_name, prop_obj);
}


void
Schema::write_settings(MsgPack& /*unused*/, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_settings({})", repr(prop_obj.to_string()));

	consistency_settings(prop_name, prop_obj);
}


void
Schema::write_foreign(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj)
{
	L_CALL("Schema::write_foreign({})", repr(prop_obj.to_string()));

	process_foreign(prop_name, prop_obj);
	specification.flags.static_foreign = true;
	mut_properties[prop_name] = specification.endpoint;
}


void
Schema::process_ngram(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::process_ngram({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.ngram = prop_obj.boolean();
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_cjk_ngram(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::process_cjk_ngram({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.cjk_ngram = prop_obj.boolean();
		if (specification.flags.cjk_words && specification.flags.cjk_ngram) {
			THROW(ClientError, "{} and {} cannot both be true in {}", repr(RESERVED_CJK_WORDS), repr(RESERVED_CJK_NGRAM), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_cjk_words(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::process_cjk_words({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.cjk_words = prop_obj.boolean();
		if (specification.flags.cjk_words && specification.flags.cjk_ngram) {
			THROW(ClientError, "{} and {} cannot both be true in {}", repr(RESERVED_CJK_WORDS), repr(RESERVED_CJK_NGRAM), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::process_language(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::process_language({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		const auto str_language = prop_obj.str_view();
		const auto& stem = _get_stem_language(str_language);
		if (stem.first && stem.second != "unknown") {
			specification.language = stem.second;
			specification.aux_language = stem.second;
		} else {
			THROW(ClientError, "{}: {} is not supported", repr(prop_name), repr(str_language));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_prefix(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_prefix isn't heritable and can't change once fixed.
	L_CALL("Schema::process_prefix({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.local_prefix.field.assign(prop_obj.str_view());
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_slot(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_SLOT isn't heritable and can't change once fixed.
	L_CALL("Schema::process_slot({})", repr(prop_obj.to_string()));

	if (prop_obj.is_number()) {
		auto slot = static_cast<engine::valueno>(prop_obj.u64());
		if (slot == engine::BAD_VALUENO) {
			THROW(ClientError, "{} invalid slot ({} not supported)", repr(prop_name), slot);
		}
		specification.slot = slot;
	} else {
		THROW(ClientError, "Data inconsistency, {} must be integer", repr(prop_name));
	}
}


void
Schema::process_stop_strategy(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STOP_STRATEGY isn't heritable and can't change once fixed.
	L_CALL("Schema::process_stop_strategy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		auto str_stop_strategy = prop_obj.str_view();
		specification.stop_strategy = _get_stop_strategy(str_stop_strategy);
		if (specification.stop_strategy == StopStrategy::INVALID) {
			THROW(ClientError, "{} can be in {} ({} not supported)", repr(prop_name), str_set_stop_strategy, repr(str_stop_strategy));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_stem_strategy(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STEM_STRATEGY isn't heritable and can't change once fixed.
	L_CALL("Schema::process_stem_strategy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		auto str_stem_strategy = prop_obj.str_view();
		specification.stem_strategy = enum_type<StemStrategy>(str_stem_strategy);
		if (specification.stem_strategy == StemStrategy::INVALID) {
			THROW(ClientError, "{} can be in {} ({} not supported)", repr(prop_name), str_set_stem_strategy, repr(str_stem_strategy));
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_stem_language(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_STEM_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL("Schema::process_stem_language({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		auto str_stem_language = prop_obj.str_view();
		const auto& stem = _get_stem_language(str_stem_language);
		if (stem.second != "unknown") {
			specification.stem_language = stem.second.empty() ? stem.second : _get_stemmer_name(str_stem_language);
			specification.aux_stem_language = stem.second;
		} else {
			THROW(ClientError, "{}: {} is not supported; the accepted languages are: {}", repr(prop_name), repr(str_stem_language), stem_language_names);
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
}


void
Schema::process_type(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	L_CALL("Schema::process_type({})", repr(prop_obj.to_string()));

	if (prop_obj.is_string()) {
		specification.set_types(prop_obj.str_view());
	} else {
		THROW(ClientError, "Data inconsistency, {} must be string", repr(prop_name));
	}
	if (!specification.endpoint.empty()) {
		if (specification.sep_types[SPC_FOREIGN_TYPE] != FieldType::foreign) {
			THROW(ClientError, "Data inconsistency, {} must be foreign", repr(prop_name));
		}
	}
}


void
Schema::process_accuracy(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_ACCURACY isn't heritable and can't change once fixed.
	L_CALL("Schema::process_accuracy({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.doc_acc = std::make_unique<const MsgPack>(prop_obj);
	} else {
		THROW(ClientError, "Data inconsistency, {} must be array", repr(prop_name));
	}
}


void
Schema::process_acc_prefix(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_ACC_PREFIX isn't heritable and can't change once fixed.
	L_CALL("Schema::process_acc_prefix({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		specification.acc_prefix.clear();
		specification.acc_prefix.reserve(prop_obj.size());
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_string()) {
				specification.acc_prefix.push_back(prop_item_obj.str());
			} else {
				THROW(ClientError, "Data inconsistency, {} must be an array of strings", repr(prop_name));
			}
		}
	} else {
		THROW(ClientError, "Data inconsistency, {} must be an array of strings", repr(prop_name));
	}
}


void
Schema::process_bool_term(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	L_CALL("Schema::process_bool_term({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.bool_term = prop_obj.boolean();
		specification.flags.has_bool_term = true;
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a boolean", repr(prop_name));
	}
}


void
Schema::process_partials(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL("Schema::process_partials({})", repr(prop_obj.to_string()));

	if (prop_obj.is_boolean()) {
		specification.flags.partials = prop_obj.boolean();
	} else {
		THROW(ClientError, "Data inconsistency, {} must be boolean", repr(prop_name));
	}
}


void
Schema::process_error(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL("Schema::process_error({})", repr(prop_obj.to_string()));

	if (prop_obj.is_number()) {
		specification.error = prop_obj.f64();
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a double", repr(prop_name));
	}
}


void
Schema::process_meta(std::string_view, const MsgPack& prop_obj)
{
	// RESERVED_POSITION is heritable and can change between documents.
	L_CALL("Schema::process_meta({})", repr(prop_obj.to_string()));

	specification.meta = std::make_unique<const MsgPack>(prop_obj);
}


void
Schema::process_position(std::string_view prop_name, const MsgPack& prop_obj)
{
	// RESERVED_POSITION is heritable and can change between documents.
	L_CALL("Schema::process_position({})", repr(prop_obj.to_string()));

	if (prop_obj.is_array()) {
		if (prop_obj.empty()) {
			THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
		}
		specification.position.clear();
		for (const auto& prop_item_obj : prop_obj) {
			if (prop_item_obj.is_number()) {
				specification.position.push_back(static_cast<unsigned>(prop_item_obj.u64()));
			} else {
				THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
			}
		}
	} else if (prop_obj.is_number()) {
		specification.position.clear();
		specification.position.push_back(static_cast<unsigned>(prop_obj.u64()));
	} else {
		THROW(ClientError, "Data inconsistency, {} must be a positive integer or a not-empty array of positive integers", repr(prop_name));
	}
}



void
Schema::set_default_spc_id(MsgPack& mut_properties)
{
	L_CALL("Schema::set_default_spc_id({})", mut_properties.to_string());

	specification.flags.bool_term = true;
	specification.flags.has_bool_term = true;
	mut_properties[RESERVED_BOOL_TERM] = true;  // force bool term

	if (!specification.flags.has_index) {
		const auto index = specification.index | TypeIndex::FIELD_ALL;  // force field_all
		if (specification.index != index) {
			specification.index = index;
			mut_properties[RESERVED_INDEX] = _get_str_index(index);
		}
		specification.flags.has_index = true;
	}

	// ID_FIELD_NAME cannot be TEXT.
	if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::text) {
		specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::keyword;
		L_DEBUG("{} cannot be type string or text, it's type was changed to keyword", ID_FIELD_NAME);
	}

	// Set default prefix
	specification.local_prefix.field = DOCUMENT_ID_TERM_PREFIX;

	// Set default RESERVED_SLOT
	specification.slot = DB_SLOT_ID;
}

void
Schema::set_default_spc_version([[maybe_unused]] MsgPack& mut_properties)
{
	L_CALL("Schema::set_default_spc_version({})", mut_properties.to_string());

	specification.index = TypeIndex::FIELD_VALUES;
	specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::positive;
	specification.local_prefix.field = DOCUMENT_VERSION_TERM_PREFIX;
	specification.slot = DB_SLOT_VERSION;
}
