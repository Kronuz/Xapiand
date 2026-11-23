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

// The inline Schema members every walk (index, update, write) shares: restarting and
// feeding a field's specification, marking objects and arrays, prefixes and dynamic fields,
// the _id and _version defaults and which fields get them, and Schema::check (a template
// the constructor and the update and write walks instantiate).

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


template <typename ErrorType>
std::pair<const MsgPack*, const MsgPack*>
Schema::check(const MsgPack& schema, const char* prefix, bool allow_foreign)
{
	L_CALL("Schema::check({}, <prefix>, allow_foreign:{})", schema.to_string(), allow_foreign);

	if (schema.is_undefined()) {
		return std::make_pair(nullptr, &schema);
	}

	// Check foreign:
	if (allow_foreign) {
		if (schema.is_string()) {
			return std::make_pair(&schema, nullptr);
		}
		if (!schema.is_map()) {
			THROW(ErrorType, "{}Schema must be a map", prefix);
		}
		auto it_end = schema.end();
		auto type_it = schema.find(RESERVED_TYPE);
		if (type_it != it_end) {
			auto& type = type_it.value();
			if (!type.is_string()) {
				THROW(ErrorType, "{}Schema field '{}' must be a string", prefix, RESERVED_TYPE);
			}
			auto type_name = type.str_view();
			const auto& sep_types = required_spc_t::get_types(type_name);
			if (sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
				auto foreign_it = schema.find(RESERVED_FOREIGN);
				if (foreign_it == it_end) {
					THROW(ErrorType, "{}Schema field '{}' does not exist", prefix, RESERVED_FOREIGN);
				}
				auto& foreign = foreign_it.value();
				if (!foreign.is_string()) {
					THROW(ErrorType, "{}Schema field '{}' must be a string", prefix, RESERVED_FOREIGN);
				}
				return std::make_pair(&foreign, &schema);
			}
		}
	} else {
		if (!schema.is_map()) {
			THROW(ErrorType, "{}Schema must be a map", prefix);
		}
	}

	auto schema_it_end = schema.end();
	auto type_it = schema.find(RESERVED_TYPE);
	if (type_it != schema_it_end) {
		auto& type = type_it.value();
		if (!type.is_string()) {
			THROW(ErrorType, "{}Schema field '{}' must be a string", prefix, RESERVED_TYPE);
		}
		auto type_name = type.str_view();
		const auto& sep_types = required_spc_t::get_types(type_name);
		if (sep_types[SPC_CONCRETE_TYPE] != FieldType::object) {
			THROW(ErrorType, "{}Schema has an unsupported type: {}", prefix, type_name);
		}
	}

	// Prevent schemas from having a '_schema' field inside:
	auto reserved_schema_it = schema.find(RESERVED_SCHEMA);
	if (reserved_schema_it != schema_it_end) {
		THROW(ErrorType, "{}Schema field '{}' is not valid", prefix, RESERVED_SCHEMA);
	}

	return std::make_pair(nullptr, &schema);
}


inline void
Schema::restart_specification()
{
	L_CALL("Schema::restart_specification()");

	specification.flags.partials             = default_spc.flags.partials;
	specification.error                      = default_spc.error;

	specification.flags.ngram                = default_spc.flags.ngram;
	specification.flags.cjk_ngram            = default_spc.flags.cjk_ngram;
	specification.flags.cjk_words            = default_spc.flags.cjk_words;
	specification.language                   = default_spc.language;
	specification.stop_strategy              = default_spc.stop_strategy;
	specification.stem_strategy              = default_spc.stem_strategy;
	specification.stem_language              = default_spc.stem_language;

	specification.flags.bool_term            = default_spc.flags.bool_term;
	specification.flags.has_bool_term        = default_spc.flags.has_bool_term;
	specification.flags.has_index            = default_spc.flags.has_index;
	specification.flags.has_namespace        = default_spc.flags.has_namespace;
	specification.flags.static_foreign       = default_spc.flags.static_foreign;

	specification.flags.concrete             = default_spc.flags.concrete;
	specification.flags.complete             = default_spc.flags.complete;
	specification.flags.uuid_field           = default_spc.flags.uuid_field;

	specification.sep_types                  = default_spc.sep_types;
	specification.endpoint                   = default_spc.endpoint;
	specification.local_prefix               = default_spc.local_prefix;
	specification.slot                       = default_spc.slot;
	specification.accuracy                   = default_spc.accuracy;
	specification.acc_prefix                 = default_spc.acc_prefix;
	specification.aux_stem_language          = default_spc.aux_stem_language;
	specification.aux_language               = default_spc.aux_language;

	specification.ignored                    = default_spc.ignored;

	specification.partial_index_spcs         = default_spc.partial_index_spcs;
}


inline void
Schema::restart_namespace_specification()
{
	L_CALL("Schema::restart_namespace_specification()");

	specification.flags.bool_term            = default_spc.flags.bool_term;
	specification.flags.has_bool_term        = default_spc.flags.has_bool_term;
	specification.flags.static_foreign       = default_spc.flags.static_foreign;

	specification.flags.concrete             = default_spc.flags.concrete;
	specification.flags.complete             = default_spc.flags.complete;
	specification.flags.uuid_field           = default_spc.flags.uuid_field;

	specification.endpoint                   = default_spc.endpoint;
	specification.aux_stem_language          = default_spc.aux_stem_language;
	specification.aux_language               = default_spc.aux_language;

	specification.partial_index_spcs         = default_spc.partial_index_spcs;
}


struct FedSpecification : MsgPack::Data {
	specification_t specification;

	FedSpecification(specification_t  specification) : specification(std::move(specification)) { }
};


template <typename T>
inline bool
Schema::feed_subproperties(T& properties, std::string_view meta_name)
{
	L_CALL("Schema::feed_subproperties({}, {})", properties->to_string(), repr(meta_name));

	auto it = properties->find(meta_name);
	if (it == properties->end()) {
		return false;
	}

	properties = &it.value();
	const auto data = std::static_pointer_cast<const FedSpecification>(properties->get_data());
	if (data) {
		// This is the feed cache
		auto local_prefix_uuid = specification.local_prefix.uuid;
		auto prefix = specification.prefix;
		specification = data->specification;
		specification.prefix = prefix;
		specification.local_prefix.uuid = local_prefix_uuid;
		return true;
	}

	specification.flags.field_found = true;

	const auto& stem = _get_stem_language(meta_name);
	if (stem.first && stem.second != "unknown") {
		specification.language = stem.second;
		specification.aux_language = stem.second;
	}

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(meta_name);
	} else {
		specification.full_meta_name.append(1, DB_OFFSPRING_UNION).append(meta_name);
	}

	dispatch_feed_properties(*properties);

	properties->set_data(std::make_shared<const FedSpecification>(specification));

	return true;
}


inline void
Schema::set_type_to_object()
{
	L_CALL("Schema::set_type_to_object()");

	if (!specification.flags.is_namespace) {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
			specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::object;
			auto& mut_properties = get_mutable_properties(specification.full_meta_name);
			mut_properties[RESERVED_TYPE] = _get_str_type(specification.sep_types);
		} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::object) {
			THROW(ClientError, "Type mismatch '{}' -> 'object'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
		}
	}
}


inline void
Schema::set_type_to_array()
{
	L_CALL("Schema::set_type_to_array()");

	if (!specification.flags.is_namespace) {
		if (specification.sep_types[SPC_ARRAY_TYPE] == FieldType::empty) {
			specification.sep_types[SPC_ARRAY_TYPE] = FieldType::array;
			auto& mut_properties = get_mutable_properties(specification.full_meta_name);
			mut_properties[RESERVED_TYPE] = _get_str_type(specification.sep_types);
		} else if (specification.sep_types[SPC_ARRAY_TYPE] != FieldType::array) {
			THROW(ClientError, "Type mismatch '{}' -> 'array'", enum_name(specification.sep_types[SPC_ARRAY_TYPE]));
		}
	}
}


inline void
Schema::update_prefixes()
{
	L_CALL("Schema::update_prefixes()");

	if (specification.flags.uuid_path) {
		if (specification.flags.uuid_field) {
			switch (specification.index_uuid_field) {
				case UUIDFieldIndex::uuid: {
					specification.flags.has_uuid_prefix = true;
					specification.local_prefix.field = std::move(specification.local_prefix.uuid);
					specification.prefix.field.append(specification.local_prefix.field);
					if (!specification.prefix.uuid.empty()) {
						specification.prefix.uuid.append(specification.local_prefix.field);
					}
					specification.local_prefix.uuid.clear();
					break;
				}
				case UUIDFieldIndex::uuid_field: {
					specification.prefix.field.append(specification.local_prefix.field);
					if (!specification.prefix.uuid.empty()) {
						specification.prefix.uuid.append(specification.local_prefix.field);
					}
					specification.local_prefix.uuid.clear();
					break;
				}
				case UUIDFieldIndex::both: {
					if (specification.prefix.uuid.empty()) {
						specification.prefix.uuid = specification.prefix.field;
					}
					specification.prefix.field.append(specification.local_prefix.field);
					specification.prefix.uuid.append(specification.local_prefix.uuid);
					break;
				}
				case UUIDFieldIndex::INVALID:
					break;
			}
		} else {
			specification.prefix.field.append(specification.local_prefix.field);
			if (!specification.prefix.uuid.empty()) {
				specification.prefix.uuid.append(specification.local_prefix.field);
			}
		}
	} else {
		specification.prefix.field.append(specification.local_prefix.field);
		if (!specification.prefix.uuid.empty()) {
			specification.prefix.uuid.append(specification.local_prefix.field);
		}
	}

	if (specification.flags.partial_paths) {
		if (!specification.flags.is_namespace) {
			THROW(ClientError, "Cannot use '{}' without '{}'.", RESERVED_PARTIAL_PATHS, RESERVED_NAMESPACE);
		}
		if (specification.partial_prefixes.empty()) {
			specification.partial_prefixes.push_back(specification.prefix);
		} else {
			specification.partial_prefixes.push_back(specification.local_prefix);
		}
	} else {
		specification.partial_prefixes.clear();
	}
}


inline void
Schema::detect_dynamic(std::string_view field_name)
{
	L_CALL("Schema::detect_dynamic({})", repr(field_name));

	if (field_name == UUID_FIELD_NAME) {
		specification.meta_name.assign(UUID_FIELD_NAME);
		specification.flags.uuid_field = true;
		specification.flags.uuid_path = true;
	} else if (Serialize::possiblyUUID(field_name)) {
		try {
			auto ser_uuid = Serialize::uuid(field_name);
			specification.local_prefix.uuid.assign(ser_uuid);
			static const auto uuid_field_prefix = get_prefix(UUID_FIELD_NAME);
			specification.local_prefix.field.assign(uuid_field_prefix);
			specification.meta_name.assign(UUID_FIELD_NAME);
			specification.flags.uuid_field = true;
			specification.flags.uuid_path = true;
		} catch (const SerializationError&) {
			specification.local_prefix.field.assign(get_prefix(field_name));
			specification.meta_name.assign(field_name);
			specification.flags.uuid_field = false;
		}
	} else {
		specification.local_prefix.field.assign(get_prefix(field_name));
		specification.meta_name.assign(field_name);
		specification.flags.uuid_field = false;
	}
}


inline bool
has_dispatch_set_default_spc(uint64_t key)
{
	constexpr static auto _ = phf::make_phf({
		hh(ID_FIELD_NAME),
		hh(VERSION_FIELD_NAME),
	});
	return _.count(key) != 0u;
}


inline void
Schema::dispatch_set_default_spc(MsgPack& mut_properties)
{
	L_CALL("Schema::dispatch_set_default_spc({})", mut_properties.to_string());

	auto key = hh(specification.full_meta_name);
	constexpr static auto _ = phf::make_phf({
		hh(ID_FIELD_NAME),
		hh(VERSION_FIELD_NAME),
	});
	switch (_.find(key)) {
		case _.fhh(ID_FIELD_NAME):
			set_default_spc_id(mut_properties);
			break;
		case _.fhh(VERSION_FIELD_NAME):
			set_default_spc_version(mut_properties);
			break;
	}
}
