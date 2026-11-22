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
#include "database/schema/dispatch.h"


using namespace schema_detail;


#define L_SCHEMA L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_INDEX
// #define L_INDEX L_STACKED_CHOCOLATE


const std::string NAMESPACE_PREFIX_ID_FIELD_NAME = get_prefix(ID_FIELD_NAME);


/*
 * index() algorithm outline:
 * 1. Try reading schema from the metadata, if there is already a schema jump to 3.
 * 2. Write properties and feed specification_t using write_*, this step could
 *    use some process_* (for some properties). Jump to 5.
 * 3. Feed specification_t with the read schema using feed_*;
 *    sets field_found for all found fields.
 * 4. Complement specification_t with the object sent by the user using process_*,
 *    except those that are already fixed because are reserved to be and
 *    they already exist in the metadata, those are simply checked with consistency_*.
 * 5. If the field in the schema is normal and still has no RESERVED_TYPE (concrete)
 *    and a value is received for the field, call validate_required_data() to
 *    initialize the specification with validated data sent by the user.
 * 6. If the field is namespace or has partial paths call validate_required_namespace_data() to
 *    initialize the specification with default specifications and sent by the user.
 * 7. If there are values sent by user, fills the document to be indexed via
 *    index_item_value()
 * 8. If the path has uuid field name the values are indexed according to index_uuid_field.
 * 9. index_new_object() does step 2 to 8 and for each field it calls index_new_object(...).
 * 10. index() does steps 2 to 4 and for each field it calls index_new_object(...)
 *
 * write_schema() algorithm outline:
 * 1. Try reading schema from the metadata.
 * 2. If there is already a schema, feed specification_t with the read schema
 *    using feed_*; sets field_found for all found fields.
 * 3. Write properties and feed specification_t using write_*, this step could
 *    use some process_* (for some properties).
 * 4. write_new_object() does step 2 to 3 and for each field it calls update_schema(...).
 */


/*
 * Unordered Maps used for reading user data specification.
 */


namespace std {
	template<typename T, size_t N>
	struct hash<const array<T, N>> {
		size_t operator()(const array<T, N>& a) const {
			size_t h = 0;
			for (auto e : a) {
				h ^= hash<T>{}(e) + 0x9e3779b9 + (h << 6) + (h >> 2);
			}
			return h;
		}
	};
}


inline UnitTime
_get_accuracy_date(std::string_view str_accuracy_date)
{
	constexpr static auto _ = phf::make_phf({
		hhl("day"),
		hhl("month"),
		hhl("year"),
		hhl("decade"),
		hhl("century"),
		hhl("millennium"),
	});

	switch (_.fhhl(str_accuracy_date)) {
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


UnitTime
get_accuracy_date(std::string_view str_accuracy_date)
{
	return _get_accuracy_date(str_accuracy_date);
}


UnitTime
get_accuracy_datetime(std::string_view str_accuracy_datetime)
{
	return _get_accuracy_datetime(str_accuracy_datetime);
}


UnitTime
get_accuracy_time(std::string_view str_accuracy_time)
{
	return _get_accuracy_time(str_accuracy_time);
}


/*
 *  Function to generate a prefix given an field accuracy.
 */

static inline std::pair<std::string, FieldType>
_get_acc_data(std::string_view field_acc)
{
	auto accuracy_date = _get_accuracy_datetime(field_acc.substr(1));
	if (accuracy_date != UnitTime::INVALID) {
		return std::make_pair(get_prefix(toUType(accuracy_date)), FieldType::datetime);
	}
	try {
		switch (field_acc[1]) {
			case 'g':
				if (field_acc[2] == 'e' && field_acc[3] == 'o') {
					return std::make_pair(get_prefix(strict_stoull(field_acc.substr(4))), FieldType::geo);
				}
				break;
			case 't':
				if (field_acc[2] == 'd') {
					return std::make_pair(get_prefix(toUType(_get_accuracy_time(field_acc.substr(3)))), FieldType::timedelta);
				}
				return std::make_pair(get_prefix(toUType(_get_accuracy_time(field_acc.substr(2)))), FieldType::time);
			default:
				return std::make_pair(get_prefix(strict_stoull(field_acc.substr(1))), FieldType::integer);
		}
	} catch (const std::invalid_argument&) {
	} catch (const std::out_of_range&) { }

	THROW(ClientError, "The field name: {} is not valid", repr(field_acc));
}


Schema::Schema(std::shared_ptr<const MsgPack> s, std::unique_ptr<MsgPack> m, std::string o)
	: schema(std::move(s)),
	  mut_schema(std::move(m)),
	  origin(std::move(o))
{
	auto checked = check<Error>(*schema, "Schema is corrupt: ", true);
	if (checked.first != nullptr) {
		schema = get_initial_schema();
	}
}


std::shared_ptr<const MsgPack>
Schema::get_initial_schema()
{
	L_CALL("Schema::get_initial_schema()");

	static const MsgPack initial_schema_tpl(MsgPack::MAP());
	auto initial_schema = std::make_shared<const MsgPack>(initial_schema_tpl);
	initial_schema->lock();
	return initial_schema;
}


const MsgPack&
Schema::get_properties(std::string_view full_meta_name)
{
	L_CALL("Schema::get_properties({})", repr(full_meta_name));

	const MsgPack* prop = &get_properties();
	Split<std::string_view> field_names(full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : field_names) {
		prop = &prop->at(field_name);
	}
	return *prop;
}


MsgPack&
Schema::get_mutable_properties(std::string_view full_meta_name)
{
	L_CALL("Schema::get_mutable_properties({})", repr(full_meta_name));

	MsgPack* prop = &get_mutable_properties();
	Split<std::string_view> field_names(full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : field_names) {
		prop = &prop->get(field_name);
	}
	return *prop;
}


const MsgPack&
Schema::get_newest_properties(std::string_view full_meta_name)
{
	L_CALL("Schema::get_newest_properties({})", repr(full_meta_name));

	const MsgPack* prop = &get_newest_properties();
	Split<std::string_view> field_names(full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : field_names) {
		prop = &prop->at(field_name);
	}
	return *prop;
}


MsgPack&
Schema::clear()
{
	L_CALL("Schema::clear()");

	auto& prop = get_mutable_properties();
	prop.clear();
	return prop;
}


void
Schema::index_item_value(engine::Document& doc, MsgPack*& data, const MsgPack& item_value, size_t pos)
{
	L_CALL("Schema::index_item_value(<doc>, {}, {}, {})", data->to_string(), item_value.to_string(), pos);

	if (!specification.flags.complete) {
		complete_specification(item_value);
	}

	if (specification.partial_index_spcs.empty()) {
		index_item(doc, item_value, pos);
		if (specification.flags.store) {
			store_item(item_value, *data);
		}
	} else {
		index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot, std::move(specification.accuracy), std::move(specification.acc_prefix));
		for (const auto& index_spc : specification.partial_index_spcs) {
			specification.update(index_spc);
			index_item(doc, item_value, pos);
		}
		if (specification.flags.store) {
			store_item(item_value, *data);
		}
		specification.update(std::move(start_index_spc));
	}

	if (specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
		if (!specification.flags.static_foreign) {
			data->get(RESERVED_FOREIGN) = specification.endpoint;
		}
	}
}


/*  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 *      _   _           _       _
 *     | | | |_ __   __| | __ _| |_ ___
 *     | | | | '_ \ / _` |/ _` | __/ _ \
 *     | |_| | |_) | (_| | (_| | ||  __/
 *      \___/| .__/ \__,_|\__,_|\__\___|
 *           |_|
 *  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 */

bool
Schema::update(const MsgPack& object)
{
	L_CALL("Schema::update({})", object.to_string());

	L_INDEX(">>> Update schema from: " + DIM_GREY + "{}", object.to_string());

	try {
		map_values.clear();
		specification = default_spc;
		specification.slot = DB_SLOT_ROOT;  // Set default RESERVED_SLOT for root

		std::pair<const MsgPack*, const MsgPack*> checked;
		checked = check<ClientError>(object, "Invalid schema: ", true);

		if (checked.first != nullptr) {
			mut_schema = std::make_unique<MsgPack>(MsgPack({
				{ RESERVED_TYPE, "foreign/object" },
				{ RESERVED_FOREIGN, *checked.first },
			}));

			return checked.second != nullptr ? checked.second->size() != 2 : false;
		}

		if (checked.second != nullptr) {
			const auto& schema_obj = *checked.second;

			auto properties = &get_newest_properties();

			Fields fields;

			if (properties->empty()) {  // new schemas have empty properties
				specification.flags.field_found = false;
				auto mut_properties = &get_mutable_properties();
				dispatch_write_properties(*mut_properties, schema_obj, fields);
				properties = &*mut_properties;
			} else {
				dispatch_feed_properties(*properties);
				dispatch_process_properties(schema_obj, fields);
			}

			update_fields(properties, fields);
		}

		// L_INDEX("Updated schema: " + DIM_GREY + "{}", get_newest_properties().to_string());

		return false;
	} catch (...) {
		L_DEBUG("ERROR: {}", object.to_string());
		mut_schema.reset();
		throw;
	}
}


const MsgPack&
Schema::update_subproperties(const MsgPack*& properties, std::string_view name, const MsgPack& object, Fields& fields)
{
	L_CALL("Schema::update_subproperties({}, {}, {}, <fields>)", properties->to_string(), repr(name), object.to_string());

	Split<std::string_view> field_names(name, DB_OFFSPRING_UNION);

	auto it = field_names.begin();
	assert(it != field_names.end());

	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			detect_dynamic(field_name);
			update_prefixes();
		}
		const auto& field_name = *it;
		dispatch_process_properties(object, fields);
		detect_dynamic(field_name);
		update_prefixes();
		specification.flags.inside_namespace = true;
	} else {
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
				THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			restart_specification();
			if (feed_subproperties(properties, field_name)) {
				update_prefixes();
			} else {
				detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					if (feed_subproperties(properties, specification.meta_name)) {
						update_prefixes();
						continue;
					}
				}

				auto mut_properties = &get_mutable_properties(specification.full_meta_name);
				add_field(mut_properties);

				for (++it; !it.last(); ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name)) {
						THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
					}
				}
				const auto& n_field_name = *it;
				if (!is_valid(n_field_name)) {
					THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				} else {
					detect_dynamic(n_field_name);
					add_field(mut_properties, object, fields);
				}
				return *mut_properties;
			}
		}

		const auto& field_name = *it;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
			THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		restart_specification();
		if (feed_subproperties(properties, field_name)) {
			dispatch_process_properties(object, fields);
			update_prefixes();
		} else {
			detect_dynamic(field_name);
			if (specification.flags.uuid_field) {
				if (feed_subproperties(properties, specification.meta_name)) {
					dispatch_process_properties(object, fields);
					update_prefixes();
					return *properties;
				}
			}

			auto mut_properties = &get_mutable_properties(specification.full_meta_name);
			add_field(mut_properties, object, fields);
			return *mut_properties;
		}
	}

	return *properties;
}


const MsgPack&
Schema::update_subproperties(const MsgPack*& properties, const std::string& name)
{
	L_CALL("Schema::update_subproperties({}, {})", properties->to_string(), repr(name));

	Split<std::string_view> field_names(name, DB_OFFSPRING_UNION);

	auto it = field_names.begin();
	assert(it != field_names.end());

	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			detect_dynamic(field_name);
			update_prefixes();
		}
		const auto& field_name = *it;
		detect_dynamic(field_name);
		update_prefixes();
		specification.flags.inside_namespace = true;
	} else {
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
				THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			restart_specification();
			if (feed_subproperties(properties, field_name)) {
				update_prefixes();
			} else {
				detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					if (feed_subproperties(properties, specification.meta_name)) {
						update_prefixes();
						continue;
					}
				}

				auto mut_properties = &get_mutable_properties(specification.full_meta_name);
				add_field(mut_properties);

				for (++it; !it.last(); ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name)) {
						THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
					}
				}
				const auto& n_field_name = *it;
				if (!is_valid(n_field_name)) {
					THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				} else {
					detect_dynamic(n_field_name);
					add_field(mut_properties);
				}
				return *mut_properties;
			}
		}

		const auto& field_name = *it;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
			THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		restart_specification();
		if (feed_subproperties(properties, field_name)) {
			update_prefixes();
		} else {
			detect_dynamic(field_name);
			if (specification.flags.uuid_field) {
				if (feed_subproperties(properties, specification.meta_name)) {
					update_prefixes();
					return *properties;
				}
			}

			auto mut_properties = &get_mutable_properties(specification.full_meta_name);
			add_field(mut_properties);
			return *mut_properties;
		}

	}

	return *properties;
}


void
Schema::update_new_object(const MsgPack*& parent_properties, const MsgPack& object, const std::string& name)
{
	L_CALL("Schema::update_new_object({}, {}, {})", parent_properties->to_string(), object.to_string(), repr(name));

	if (is_comment(name)) {
		return;  // skip comments (empty fields or fields starting with '#')
	}

	if (is_valid(name) && (!specification.flags.recurse || specification.ignored.find(name) != specification.ignored.end())) {
		return;
	}

	auto spc_start = specification;

	switch (object.get_type()) {
		case MsgPack::Type::MAP: {
			auto properties = &*parent_properties;
			Fields fields;
			properties = &update_subproperties(properties, name, object, fields);
			update_fields(properties, fields);
			auto value = specification.value ? specification.value.get() : specification.value_rec.get();
			if (value) {
				THROW(ClientError, "Schema objects cannot receive '{}'", RESERVED_VALUE);
			}
			if (specification.flags.inside_namespace && !spc_start.flags.concrete) {
				// Bubble the namespaced type up
				spc_start.sep_types[SPC_CONCRETE_TYPE] = specification.sep_types[SPC_CONCRETE_TYPE];
			}
			specification = std::move(spc_start);
			return;
		}

		default: {
			auto properties = &*parent_properties;
			properties = &update_subproperties(properties, name);
			update_object(properties, object, name);
			if (specification.flags.inside_namespace && !spc_start.flags.concrete) {
				// Bubble the namespaced type up
				spc_start.sep_types[SPC_CONCRETE_TYPE] = specification.sep_types[SPC_CONCRETE_TYPE];
			}
			specification = std::move(spc_start);
			return;
		}
	}
}


void
Schema::update_object(const MsgPack*& parent_properties, const MsgPack& object, const std::string& name)
{
	L_CALL("Schema::update_object({}, {}, {})", parent_properties->to_string(), object.to_string(), repr(name));

	switch (object.get_type()) {
		case MsgPack::Type::MAP: {
			update_inner_object(parent_properties, object);
			break;
		}

		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED: {
			if (!specification.flags.concrete) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
					if (specification.flags.inside_namespace) {
						validate_required_namespace_data();
					} else {
						validate_required_data(get_mutable_properties(specification.full_meta_name));
					}
				}
			}
			break;
		}

		case MsgPack::Type::ARRAY: {
			update_array(parent_properties, object, name);
			break;
		}

		default: {
			update_item_value(object);
			break;
		}
	}
}


void
Schema::update_array(const MsgPack*& parent_properties, const MsgPack& array, const std::string& name)
{
	L_CALL("Schema::update_array({}, {}, {})", parent_properties->to_string(), array.to_string(), repr(name));

	set_type_to_array();  // this has to be done first

	if (array.empty()) {
		return;
	}

	size_t pos = 0;
	for (const auto& object : array) {
		switch (object.get_type()) {
			case MsgPack::Type::MAP: {
				update_inner_object(parent_properties, object);
				break;
			}

			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED: {
				if (!specification.flags.concrete) {
					if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
						if (specification.flags.inside_namespace) {
							validate_required_namespace_data();
						} else {
							validate_required_data(get_mutable_properties(specification.full_meta_name));
						}
					}
				}
				break;
			}

			case MsgPack::Type::ARRAY: {
				update_array(parent_properties, object, name);
				break;
			}

			default: {
				update_item_value(object);
				break;
			}
		}
		++pos;
	}
}


void
Schema::update_item_value([[maybe_unused]] const MsgPack& item_value)
{
	L_CALL("Schema::update_item_value({})", item_value.to_string());

	if (!specification.flags.concrete) {
		bool foreign_type = specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign;
		if (!foreign_type && !specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		bool concrete_type = specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty;
		if (!concrete_type && !foreign_type) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
		if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(get_mutable_properties(specification.full_meta_name));
			}
		}
	}

	if (!specification.partial_index_spcs.empty()) {
		index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot, std::move(specification.accuracy), std::move(specification.acc_prefix));
		for (const auto& index_spc : specification.partial_index_spcs) {
			specification.update(index_spc);
		}
		specification.update(std::move(start_index_spc));
	}
}


inline void
Schema::update_fields(const MsgPack*& properties, const Fields& fields)
{
	L_CALL("Schema::update_fields(<const MsgPack*>, <Fields>)");

	for (const auto& field : fields) {
		update_new_object(properties, *field.second, field.first);
	}

	if (!specification.flags.concrete) {
		bool foreign_type = specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign;
		if (!foreign_type && !specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		if (specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
			THROW(ClientError, "{} is a foreign type and as such it cannot have extra fields", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(get_mutable_properties(specification.full_meta_name));
			}
		}
	}

	if (!fields.empty()) {
		set_type_to_object();  // this has to be done last
	}
}


inline void
Schema::update_inner_object(const MsgPack*& properties, const MsgPack& object)
{
	L_CALL("Schema::update_inner_object(<const MsgPack*>, <object>)");

	for (auto& key : object) {
		update_new_object(properties, object.at(key), key.str());
	}

	if (!specification.flags.concrete) {
		bool foreign_type = specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign;
		if (!foreign_type && !specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		if (specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
			THROW(ClientError, "{} is a foreign type and as such it cannot have extra fields", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(get_mutable_properties(specification.full_meta_name));
			}
		}
	}

	if (!object.empty()) {
		set_type_to_object();  // this has to be done last
	}
}

/*  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 *     __        __    _ _
 *     \ \      / / __(_) |_ ___
 *      \ \ /\ / / '__| | __/ _ \
 *       \ V  V /| |  | | ||  __/
 *        \_/\_/ |_|  |_|\__\___|
 *  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 */

bool
Schema::write(const MsgPack& object)
{
	L_CALL("Schema::write({})", object.to_string());

	L_INDEX(">>> Write schema from: " + DIM_GREY + "{}", object.to_string());

	try {
		map_values.clear();
		specification = default_spc;
		specification.slot = DB_SLOT_ROOT;  // Set default RESERVED_SLOT for root

		std::pair<const MsgPack*, const MsgPack*> checked;
		checked = check<ClientError>(object, "Invalid schema: ", true);

		if (checked.first != nullptr) {
			mut_schema = std::make_unique<MsgPack>(MsgPack({
				{ RESERVED_TYPE, "foreign/object" },
				{ RESERVED_FOREIGN, *checked.first },
			}));

			return checked.second != nullptr ? checked.second->size() != 2 : false;
		}

		if (checked.second != nullptr) {
			const auto& schema_obj = *checked.second;

			auto mut_properties = &get_mutable_properties();
			mut_properties->clear();

			Fields fields;

			specification.flags.field_found = false;
			dispatch_write_properties(*mut_properties, schema_obj, fields);

			write_fields(mut_properties, fields);
		}

		// L_INDEX("Written schema: " + DIM_GREY + "{}", get_newest_properties().to_string());

		return false;
	} catch (...) {
		L_DEBUG("ERROR: {}", object.to_string());
		mut_schema.reset();
		throw;
	}
}


MsgPack&
Schema::write_subproperties(MsgPack*& mut_properties, std::string_view name, const MsgPack& object, Fields& fields)
{
	L_CALL("Schema::write_subproperties({}, {}, {}, <fields>)", mut_properties->to_string(), repr(name), object.to_string());

	Split<std::string_view> field_names(name, DB_OFFSPRING_UNION);

	auto it = field_names.begin();
	assert(it != field_names.end());

	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			detect_dynamic(field_name);
			update_prefixes();
		}
		const auto& field_name = *it;
		dispatch_write_properties(*mut_properties, object, fields);
		detect_dynamic(field_name);
		update_prefixes();
		specification.flags.inside_namespace = true;
	} else {
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
				THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			restart_specification();
			if (feed_subproperties(mut_properties, field_name)) {
				update_prefixes();
			} else {
				detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					if (feed_subproperties(mut_properties, specification.meta_name)) {
						update_prefixes();
						continue;
					}
				}

				add_field(mut_properties);

				for (++it; !it.last(); ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name)) {
						THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
					}
				}
				const auto& n_field_name = *it;
				if (!is_valid(n_field_name)) {
					THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				} else {
					detect_dynamic(n_field_name);
					add_field(mut_properties, object, fields);
				}
				return *mut_properties;
			}
		}

		const auto& field_name = *it;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
			THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		restart_specification();
		if (feed_subproperties(mut_properties, field_name)) {
			dispatch_write_properties(*mut_properties, object, fields);
			update_prefixes();
		} else {
			detect_dynamic(field_name);
			if (specification.flags.uuid_field) {
				if (feed_subproperties(mut_properties, specification.meta_name)) {
					dispatch_write_properties(*mut_properties, object, fields);
					update_prefixes();
					return *mut_properties;
				}
			}

			add_field(mut_properties, object, fields);
			return *mut_properties;
		}
	}

	return *mut_properties;
}


MsgPack&
Schema::write_subproperties(MsgPack*& mut_properties, const std::string& name)
{
	L_CALL("Schema::write_subproperties({}, {})", mut_properties->to_string(), repr(name));

	Split<std::string_view> field_names(name, DB_OFFSPRING_UNION);

	auto it = field_names.begin();
	assert(it != field_names.end());

	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			detect_dynamic(field_name);
			update_prefixes();
		}
		const auto& field_name = *it;
		detect_dynamic(field_name);
		update_prefixes();
		specification.flags.inside_namespace = true;
	} else {
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
				THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			restart_specification();
			if (feed_subproperties(mut_properties, field_name)) {
				update_prefixes();
			} else {
				detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					if (feed_subproperties(mut_properties, specification.meta_name)) {
						update_prefixes();
						continue;
					}
				}

				add_field(mut_properties);

				for (++it; !it.last(); ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name)) {
						THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
					}
				}
				const auto& n_field_name = *it;
				if (!is_valid(n_field_name)) {
					THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				} else {
					detect_dynamic(n_field_name);
					add_field(mut_properties);
				}
				return *mut_properties;
			}
		}

		const auto& field_name = *it;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
			THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		restart_specification();
		if (feed_subproperties(mut_properties, field_name)) {
			update_prefixes();
		} else {
			detect_dynamic(field_name);
			if (specification.flags.uuid_field) {
				if (feed_subproperties(mut_properties, specification.meta_name)) {
					update_prefixes();
					return *mut_properties;
				}
			}

			add_field(mut_properties);
			return *mut_properties;
		}

	}

	return *mut_properties;
}


void
Schema::write_new_object(MsgPack*& mut_parent_properties, const MsgPack& object, const std::string& name)
{
	L_CALL("Schema::write_new_object({}, {}, {})", mut_parent_properties->to_string(), object.to_string(), repr(name));

	if (is_comment(name)) {
		return;  // skip comments (empty fields or fields starting with '#')
	}

	if (is_valid(name) && (!specification.flags.recurse || specification.ignored.find(name) != specification.ignored.end())) {
		return;
	}

	auto spc_start = specification;

	switch (object.get_type()) {
		case MsgPack::Type::MAP: {
			auto properties = &*mut_parent_properties;
			Fields fields;
			properties = &write_subproperties(properties, name, object, fields);
			write_fields(properties, fields);
			auto value = specification.value ? specification.value.get() : specification.value_rec.get();
			if (value) {
				THROW(ClientError, "Schema objects cannot receive '{}'", RESERVED_VALUE);
			}
			if (specification.flags.inside_namespace && !spc_start.flags.concrete) {
				// Bubble the namespaced type up
				spc_start.sep_types[SPC_CONCRETE_TYPE] = specification.sep_types[SPC_CONCRETE_TYPE];
			}
			specification = std::move(spc_start);
			return;
		}

		default: {
			auto properties = &*mut_parent_properties;
			properties = &write_subproperties(properties, name);
			write_object(properties, object, name);
			if (specification.flags.inside_namespace && !spc_start.flags.concrete) {
				// Bubble the namespaced type up
				spc_start.sep_types[SPC_CONCRETE_TYPE] = specification.sep_types[SPC_CONCRETE_TYPE];
			}
			specification = std::move(spc_start);
			return;
		}
	}
}


void
Schema::write_object(MsgPack*& mut_parent_properties, const MsgPack& object, const std::string& name)
{
	L_CALL("Schema::write_object({}, {}, {})", mut_parent_properties->to_string(), object.to_string(), repr(name));

	switch (object.get_type()) {
		case MsgPack::Type::MAP: {
			write_inner_object(mut_parent_properties, object);
			break;
		}

		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED: {
			if (!specification.flags.concrete) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
					if (specification.flags.inside_namespace) {
						validate_required_namespace_data();
					} else {
						validate_required_data(get_mutable_properties(specification.full_meta_name));
					}
				}
			}
			break;
		}

		case MsgPack::Type::ARRAY: {
			write_array(mut_parent_properties, object, name);
			break;
		}

		default: {
			write_item_value(mut_parent_properties, object);
			break;
		}
	}
}


void
Schema::write_array(MsgPack*& mut_parent_properties, const MsgPack& array, const std::string& name)
{
	L_CALL("Schema::write_array({}, {}, {})", mut_parent_properties->to_string(), array.to_string(), repr(name));

	set_type_to_array();  // this has to be done first

	if (array.empty()) {
		return;
	}

	size_t pos = 0;
	for (const auto& object : array) {
		switch (object.get_type()) {
			case MsgPack::Type::MAP: {
				write_inner_object(mut_parent_properties, object);
				break;
			}

			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED: {
				if (!specification.flags.concrete) {
					if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
						if (specification.flags.inside_namespace) {
							validate_required_namespace_data();
						} else {
							validate_required_data(get_mutable_properties(specification.full_meta_name));
						}
					}
				}
				break;
			}

			case MsgPack::Type::ARRAY: {
				write_array(mut_parent_properties, object, name);
				break;
			}

			default: {
				write_item_value(mut_parent_properties, object);
				break;
			}
		}
		++pos;
	}
}


void
Schema::write_item_value(MsgPack*& mut_properties, [[maybe_unused]] const MsgPack& item_value)
{
	L_CALL("Schema::write_item_value({})", item_value.to_string());

	if (!specification.flags.concrete) {
		bool foreign_type = specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign;
		if (!foreign_type && !specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		bool concrete_type = specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty;
		if (!concrete_type && !foreign_type) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}
		if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(*mut_properties);
			}
		}
	}

	if (!specification.partial_index_spcs.empty()) {
		index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot, std::move(specification.accuracy), std::move(specification.acc_prefix));
		for (const auto& index_spc : specification.partial_index_spcs) {
			specification.update(index_spc);
		}
		specification.update(std::move(start_index_spc));
	}
}


inline void
Schema::write_fields(MsgPack*& mut_properties, const Fields& fields)
{
	L_CALL("Schema::write_fields(<const MsgPack*>, <Fields>)");

	for (const auto& field : fields) {
		write_new_object(mut_properties, *field.second, field.first);
	}

	if (!specification.flags.concrete) {
		bool foreign_type = specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign;
		if (!foreign_type && !specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		if (specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
			THROW(ClientError, "{} is a foreign type and as such it cannot have extra fields", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(*mut_properties);
			}
		}
	}

	if (!fields.empty()) {
		set_type_to_object();  // this has to be done last
	}
}


void
Schema::write_inner_object(MsgPack*& mut_properties, const MsgPack& object)
{
	L_CALL("Schema::write_fields(<const MsgPack*>, <object>)");

	for (auto& key : object) {
		write_new_object(mut_properties, object.at(key), key.str());
	}

	if (!specification.flags.concrete) {
		bool foreign_type = specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign;
		if (!foreign_type && !specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		if (specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
			THROW(ClientError, "{} is a foreign type and as such it cannot have extra fields", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
		}
		if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(*mut_properties);
			}
		}
	}

	if (!object.empty()) {
		set_type_to_object();  // this has to be done last
	}
}


/*  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 */

std::unordered_set<std::pair<std::string, bool>>
Schema::get_partial_paths()
{
	L_CALL("Schema::get_partial_paths()");

	if (specification.partial_prefixes.size() > LIMIT_PARTIAL_PATHS_DEPTH) {
		THROW(ClientError, "Partial paths limit depth is {}, and partial paths provided has a depth of {}", LIMIT_PARTIAL_PATHS_DEPTH, specification.partial_prefixes.size());
	}

	std::vector<std::pair<std::string, bool>> paths;
	bool do_get_uuid_slot = toUType(specification.index & TypeIndex::VALUES) != 0u;
	bool do_get_slot = do_get_uuid_slot && (specification.flags.has_uuid_prefix || specification.flags.inside_namespace);

	if (specification.partial_prefixes.size() >= 2) {
		auto it = specification.partial_prefixes.begin();

		if (!it->uuid.empty() && it->field != it->uuid) {
			paths.emplace_back(it->uuid, do_get_uuid_slot);
		}
		if (!it->field.empty()) {
			paths.emplace_back(it->field, do_get_slot);
		}

		const auto it_last = specification.partial_prefixes.end() - 1;
		for (++it; it != it_last; ++it) {
			const auto size = paths.size();
			for (size_t i = 0; i < size; ++i) {
				if (!it->uuid.empty() && it->field != it->uuid) {
					paths.emplace_back(paths[i].first + it->uuid, paths[i].second || do_get_uuid_slot);
				}
				if (!it->field.empty()) {
					paths.emplace_back(paths[i].first + it->field, paths[i].second || do_get_slot);
				}
			}
		}

		const auto size = paths.size();
		for (size_t i = 0; i < size; ++i) {
			if (!it->uuid.empty() && it->field != it->uuid) {
				paths.emplace_back(paths[i].first + it->uuid, paths[i].second || do_get_uuid_slot);
			}
			if (!it->field.empty()) {
				paths[i].first.append(it->field);
			}
		}
	} else {
		if (!specification.prefix.uuid.empty() && specification.prefix.field != specification.prefix.uuid) {
			paths.emplace_back(specification.prefix.uuid, do_get_uuid_slot);
		}
		if (!specification.prefix.field.empty()) {
			paths.emplace_back(specification.prefix.field, do_get_slot);
		}
	}

	return std::unordered_set<std::pair<std::string, bool>>(std::make_move_iterator(paths.begin()), std::make_move_iterator(paths.end()));
}


void
Schema::complete_specification(const MsgPack& item_value)
{
	L_CALL("Schema::complete_specification({})", item_value.to_string());

	if (!specification.flags.concrete) {
		if (!specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		if (specification.sep_types[SPC_FOREIGN_TYPE] != FieldType::foreign) {
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
				if (specification.flags.strict) {
					THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				specification.sep_types[SPC_CONCRETE_TYPE] = guess_concrete_type(item_value);
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					THROW(MissingTypeError, "Type of field {} cannot be guessed", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
			}

			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(get_mutable_properties(specification.full_meta_name));
			}
		}
	}

	auto paths = get_partial_paths();
	specification.partial_index_spcs.reserve(paths.size());

	for (auto& path : paths) {
		index_spc_t spc(
			specification.sep_types[SPC_CONCRETE_TYPE],
			std::move(path.first),
			path.second ? get_slot(path.first, specification.get_ctype()) : specification.slot,
			specification.accuracy,
			specification.acc_prefix);

		for (auto& acc_prefix : spc.acc_prefix) {
			acc_prefix.insert(0, acc_term_prefix(spc.prefix));
		}

		specification.partial_index_spcs.push_back(std::move(spc));
	}

	specification.flags.complete = true;
}


void
Schema::validate_required_namespace_data()
{
	L_CALL("Schema::validate_required_namespace_data() {{type:{}}}", _get_str_type(specification.sep_types));

	// This should be the exact same as validate_required_data(),
	// except this doesn't write to the mut_properties.

	std::set<uint64_t> set_acc;

	auto type = specification.sep_types[SPC_CONCRETE_TYPE];
	switch (type) {
		case FieldType::object:
			specification.flags.concrete = true;
			break;

		case FieldType::geo:
			// Set partials and error.
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								const auto val_acc = _accuracy.u64();
								if (val_acc <= HTM_MAX_LEVEL) {
									set_acc.insert(val_acc);
								} else {
									THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {} ({} not supported)", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL, val_acc);
								}
							} else {
								THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
					}
				} else {
					set_acc.insert(def_accuracy_geo.begin(), def_accuracy_geo.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::date:
		case FieldType::datetime:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							uint64_t accuracy;
							if (_accuracy.is_string()) {
								auto accuracy_date = _get_accuracy_datetime(_accuracy.str_view());
								if (accuracy_date != UnitTime::INVALID) {
									accuracy = toUType(accuracy_date);
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
								}
							} else if (_accuracy.is_number()) {
								accuracy = _accuracy.u64();
								if (!validate_acc_date(static_cast<UnitTime>(accuracy))) {
									THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
							}
							set_acc.insert(accuracy);
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
					}
				} else {
					set_acc.insert(def_accuracy_datetime.begin(), def_accuracy_datetime.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::time:
		case FieldType::timedelta:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_string()) {
								auto accuracy_time = _get_accuracy_time(_accuracy.str_view());
								if (accuracy_time != UnitTime::INVALID) {
									set_acc.insert(toUType(accuracy_time));
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time));
					}
				} else {
					set_acc.insert(def_accuracy_time.begin(), def_accuracy_time.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								set_acc.insert(_accuracy.u64());
							} else {
								THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
					}
				} else {
					set_acc.insert(def_accuracy_num.begin(), def_accuracy_num.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::text:
			// Language could be needed, for soundex.
			if (specification.aux_language.empty() && !specification.aux_stem_language.empty()) {
				specification.language = specification.aux_stem_language;
			}
			if (specification.aux_stem_language.empty() && !specification.aux_language.empty()) {
				specification.stem_language = specification.aux_language;
			}

			specification.flags.concrete = true;
			break;

		case FieldType::keyword:
			// Process RESERVED_BOOL_TERM
			if (!specification.flags.has_bool_term) {
				// By default, if normalized name has upper characters then it is consider bool term.
				specification.flags.bool_term = strings::hasupper(specification.meta_name);
				specification.flags.has_bool_term = true;
			}

			specification.flags.concrete = true;
			break;

		case FieldType::script:
			if (!specification.flags.has_index) {
				specification.index = TypeIndex::NONE;  // Fallback to index anything.
				specification.flags.has_index = true;
			}
			specification.flags.concrete = true;
			break;

		case FieldType::boolean:
		case FieldType::uuid:
			specification.flags.concrete = true;
			break;

		case FieldType::empty:
			specification.flags.concrete = false;
			break;

		default:
			THROW(ClientError, "{}: '{}' is not supported", RESERVED_TYPE, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
	}

	if (specification.flags.concrete) {
		if (toUType(specification.index & TypeIndex::VALUES) != 0u) {
			// Write RESERVED_SLOT in properties (if it has values).
			if (!specification.flags.is_namespace) {
				if (specification.slot == engine::BAD_VALUENO) {
					specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
				}
			}
		}
		if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
			// Write RESERVED_ACCURACY and RESERVED_ACC_PREFIX in properties.
			if (!set_acc.empty()) {
				specification.acc_prefix.clear();
				for (const auto& acc : set_acc) {
					specification.acc_prefix.push_back(get_prefix(acc));
				}
				specification.accuracy.assign(set_acc.begin(), set_acc.end());
			}
		}
	}
}


void
Schema::validate_required_data(MsgPack& mut_properties)
{
	L_CALL("Schema::validate_required_data({}) {{type:{}}}", mut_properties.to_string(), _get_str_type(specification.sep_types));

	dispatch_set_default_spc(mut_properties);

	std::set<uint64_t> set_acc;

	auto type = specification.sep_types[SPC_CONCRETE_TYPE];
	switch (type) {
		case FieldType::object:
			specification.flags.concrete = true;
			break;

		case FieldType::geo:
			// Set partials and error.
			mut_properties[RESERVED_PARTIALS] = static_cast<bool>(specification.flags.partials);
			mut_properties[RESERVED_ERROR] = specification.error;
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								const auto val_acc = _accuracy.u64();
								if (val_acc <= HTM_MAX_LEVEL) {
									set_acc.insert(val_acc);
								} else {
									THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {} ({} not supported)", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL, val_acc);
								}
							} else {
								THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
					}
				} else {
					set_acc.insert(def_accuracy_geo.begin(), def_accuracy_geo.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::date:
		case FieldType::datetime:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							uint64_t accuracy;
							if (_accuracy.is_string()) {
								auto accuracy_date = _get_accuracy_datetime(_accuracy.str_view());
								if (accuracy_date != UnitTime::INVALID) {
									accuracy = toUType(accuracy_date);
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
								}
							} else if (_accuracy.is_number()) {
								accuracy = _accuracy.u64();
								if (!validate_acc_date(static_cast<UnitTime>(accuracy))) {
									THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
							}
							set_acc.insert(accuracy);
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
					}
				} else {
					set_acc.insert(def_accuracy_datetime.begin(), def_accuracy_datetime.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::time:
		case FieldType::timedelta:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_string()) {
								auto accuracy_time = _get_accuracy_time(_accuracy.str_view());
								if (accuracy_time != UnitTime::INVALID) {
									set_acc.insert(toUType(accuracy_time));
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time));
					}
				} else {
					set_acc.insert(def_accuracy_time.begin(), def_accuracy_time.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								set_acc.insert(_accuracy.u64());
							} else {
								THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
					}
				} else {
					set_acc.insert(def_accuracy_num.begin(), def_accuracy_num.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::text:
			mut_properties[RESERVED_NGRAM] = static_cast<bool>(specification.flags.ngram);
			mut_properties[RESERVED_CJK_NGRAM] = static_cast<bool>(specification.flags.cjk_ngram);
			mut_properties[RESERVED_CJK_WORDS] = static_cast<bool>(specification.flags.cjk_words);

			// Language could be needed, for soundex.
			if (specification.aux_language.empty() && !specification.aux_stem_language.empty()) {
				specification.language = specification.aux_stem_language;
			}
			if (!specification.language.empty()) {
				mut_properties[RESERVED_LANGUAGE] = specification.language;
				try {
					mut_properties[RESERVED_STOP_FINGERPRINT] = stopWordsFingerprint(specification.language);
				} catch (const UnknownStopWordsError& exc) {
					THROW(ClientError, "{}: {}", repr(RESERVED_LANGUAGE), exc.what());
				}
				mut_properties[RESERVED_STOP_STRATEGY] = enum_name(specification.stop_strategy);
			}
			if (specification.aux_stem_language.empty() && !specification.aux_language.empty()) {
				specification.stem_language = specification.aux_language;
			}
			if (!specification.stem_language.empty()) {
				mut_properties[RESERVED_STEM_LANGUAGE] = specification.stem_language;
				mut_properties[RESERVED_STEM_STRATEGY] = enum_name(specification.stem_strategy);
			}

			specification.flags.concrete = true;
			break;

		case FieldType::keyword:
			// Process RESERVED_BOOL_TERM
			if (!specification.flags.has_bool_term) {
				// By default, if normalized name has upper characters then it is consider bool term.
				const auto bool_term = strings::hasupper(specification.meta_name);
				if (specification.flags.bool_term != bool_term) {
					specification.flags.bool_term = bool_term;
					mut_properties[RESERVED_BOOL_TERM] = static_cast<bool>(specification.flags.bool_term);
				}
				specification.flags.has_bool_term = true;
			}

			specification.flags.concrete = true;
			break;

		case FieldType::script:
			if (!specification.flags.has_index) {
				const auto index = TypeIndex::NONE; // Fallback to index anything.
				if (specification.index != index) {
					specification.index = index;
					mut_properties[RESERVED_INDEX] = _get_str_index(index);
				}
				specification.flags.has_index = true;
			}
			specification.flags.concrete = true;
			break;

		case FieldType::boolean:
		case FieldType::uuid:
			specification.flags.concrete = true;
			break;

		case FieldType::empty:
			specification.flags.concrete = false;
			break;

		default:
			THROW(ClientError, "{}: '{}' is not supported", RESERVED_TYPE, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
	}

	if (specification.flags.concrete) {
		if (toUType(specification.index & TypeIndex::VALUES) != 0u) {
			// Write RESERVED_SLOT in properties (if it has values).
			if (!specification.flags.is_namespace) {
				if (specification.slot == engine::BAD_VALUENO) {
					specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
				}
				mut_properties[RESERVED_SLOT] = specification.slot;
			}
		}
		if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
			// Write RESERVED_ACCURACY and RESERVED_ACC_PREFIX in properties.
			if (!set_acc.empty()) {
				specification.acc_prefix.clear();
				for (const auto& acc : set_acc) {
					specification.acc_prefix.push_back(get_prefix(acc));
				}
				specification.accuracy.assign(set_acc.begin(), set_acc.end());
				switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
					case FieldType::date:
					case FieldType::datetime:
					case FieldType::time:
					case FieldType::timedelta:
						mut_properties[RESERVED_ACCURACY] = MsgPack::ARRAY();
						for (auto& acc : specification.accuracy) {
							mut_properties[RESERVED_ACCURACY].push_back(_get_str_acc_date(static_cast<UnitTime>(acc)));
						}
						break;
					default:
						mut_properties[RESERVED_ACCURACY] = specification.accuracy;
						break;
				}
				mut_properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
			}
		}
	}

	// Process RESERVED_TYPE
	mut_properties[RESERVED_TYPE] = _get_str_type(specification.sep_types);

	// L_DEBUG("\nspecification = {}\nmut_properties = {}", specification.to_string(4), mut_properties.to_string(true));
}


// Whether the integer `value` is in the range of the integer type `type`, whatever its encoding.
static bool
fits_integer_type(FieldType type, const MsgPack& value)
{
	switch (value.get_type()) {
		case MsgPack::Type::NEGATIVE_INTEGER:
			return type == FieldType::integer || (type == FieldType::positive && value.i64() >= 0);
		case MsgPack::Type::POSITIVE_INTEGER:
			return type == FieldType::positive || (type == FieldType::integer && value.u64() <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
		default:
			return false;
	}
}


FieldType
Schema::guess_concrete_type(const MsgPack& item_doc)
{
	L_CALL("Schema::guess_concrete_type({}) {{complete:{}, concrete:{}}}", item_doc.to_string(), !!specification.flags.complete, !!specification.flags.concrete);

	if (specification.flags.complete || specification.flags.concrete) {
		return specification.sep_types[SPC_CONCRETE_TYPE];
	}

	switch (item_doc.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
			if (specification.flags.numeric_detection) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return Serialize::guess_type(item_doc);
				} else if (!fits_integer_type(specification.sep_types[SPC_CONCRETE_TYPE], item_doc)) {
					THROW(ClientError, "Type mismatch '{}' -> '{}'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), enum_name(Serialize::guess_type(item_doc)));
				}
				return specification.sep_types[SPC_CONCRETE_TYPE];
			}
			break;

		case MsgPack::Type::FLOAT:
			if (specification.flags.numeric_detection) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::floating;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::floating) {
					THROW(ClientError, "Type mismatch '{}' -> 'floating'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::floating;
			}
			break;

		case MsgPack::Type::BOOLEAN:
			if (specification.flags.bool_detection) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::boolean;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::boolean) {
					THROW(ClientError, "Type mismatch '{}' -> 'boolean'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::boolean;
			}
			break;

		case MsgPack::Type::STR: {
			const auto str_value = item_doc.str_view();
			if (specification.flags.uuid_detection && Serialize::isUUID(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::uuid;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::uuid) {
					THROW(ClientError, "Type mismatch '{}' -> 'uuid'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::uuid;
			}
			if (specification.flags.date_detection && Datetime::isDate(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::date;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::date) {
					THROW(ClientError, "Type mismatch '{}' -> 'date'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::date;
			}
			if (specification.flags.datetime_detection && Datetime::isDatetime(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::datetime;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::datetime) {
					THROW(ClientError, "Type mismatch '{}' -> 'datetime'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::datetime;
			}
			if (specification.flags.time_detection && Datetime::isTime(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::time;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::time) {
					THROW(ClientError, "Type mismatch '{}' -> 'time'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::time;
			}
			if (specification.flags.timedelta_detection && Datetime::isTimedelta(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::timedelta;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::timedelta) {
					THROW(ClientError, "Type mismatch '{}' -> 'timedelta'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::timedelta;
			}
			if (specification.flags.geo_detection && EWKT::isEWKT(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::geo;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::geo) {
					THROW(ClientError, "Type mismatch '{}' -> 'geo'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::geo;
			}
			if (specification.flags.bool_detection) {
				if (str_value == "true" || str_value == "false") {
					if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
						return FieldType::boolean;
					} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::boolean) {
						THROW(ClientError, "Type mismatch '{}' -> 'boolean'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
					}
					return FieldType::boolean;
				}
			}
			if (specification.flags.text_detection && !specification.flags.bool_term) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::text;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::text) {
					THROW(ClientError, "Type mismatch '{}' -> 'text'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::text;
			}
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
				return FieldType::keyword;
			} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::keyword) {
				THROW(ClientError, "Type mismatch '{}' -> 'keyword'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
			}
			return FieldType::keyword;
		}

		case MsgPack::Type::MAP:
			for (auto it = item_doc.begin(); it != item_doc.end(); ++it) {
				auto str_key = it->str_view();
				if (is_reserved(str_key)) {
					auto field_type = Cast::get_field_type(str_key);
					if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
						return field_type;
					} else if (specification.sep_types[SPC_CONCRETE_TYPE] != field_type) {
						THROW(ClientError, "Type mismatch '{}' -> '{}'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), enum_name(field_type));
					}
					return field_type;
				} else if (!is_comment(str_key)) {
					break;
				}
			}
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
				return FieldType::object;
			} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::object) {
				THROW(ClientError, "Type mismatch '{}' -> 'object'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
			}
			return FieldType::object;

		case MsgPack::Type::ARRAY:
			if (specification.flags.geo_detection) {
				auto items = item_doc.size();
				if ((items == 2 && item_doc[0].is_number() && item_doc[1].is_number()) ||
					(items == 3 && item_doc[0].is_number() && item_doc[1].is_number() && item_doc[2].is_number())) {
					// GeoJSON requires longitude first, latutude second
					auto longitude = item_doc[0].f64();
					auto latitude = item_doc[1].f64();
					if (longitude >= -180.0 && longitude <= 180.0 && latitude >= -90.0 && latitude <= 90.0) {
						if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
							return FieldType::geo;
						} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::geo) {
							THROW(ClientError, "Type mismatch '{}' -> 'geo'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
						}
						return FieldType::geo;
					}
				}
			}
			break;

		default:
			break;
	}

	return FieldType::empty;
}


void
Schema::index_partial_paths(engine::Document& doc)
{
	L_CALL("Schema::index_partial_paths(<engine::Document>)");

	if (specification.flags.partial_paths) {
		if (toUType(specification.index & TypeIndex::FIELD_TERMS) != 0u) {
			const auto paths = get_partial_paths();
			for (const auto& path : paths) {
				doc.add_boolean_term(doc.field(path.first, engine::FieldKind::boolean), std::string_view());
			}
		}
	}
}


inline void
Schema::index_term(engine::Document& doc, std::string_view field, std::string_view term, const specification_t& field_spc, size_t pos)
{
	L_CALL("Schema::index_term(<doc>, {}, {}, <field_spc>, {})", repr(field), repr(term), pos);

	const bool is_key = field_spc.prefix.field == DOCUMENT_ID_TERM_PREFIX && field == field_name(field_spc.prefix.field, field_spc.get_ctype());

	// The limit is on the whole term, the field's name included.
	if (field.size() + term.size() > 245) {
		if (field_spc.sep_types[SPC_CONCRETE_TYPE] == FieldType::keyword) {
			THROW(ClientError, "Keyword too long");
		}
		return;
	}

	// The id's terms are the key field's: the id's type character and its bytes.
	std::string key;
	if (is_key) {
		key = document_key(term, field_spc.get_ctype());
		field = engine::KEY_FIELD;
		term = key;
	}

	auto weight = field_spc.flags.bool_term ? 0 : field_spc.weight[getPos(pos, field_spc.weight.size())];
	auto position = field_spc.position[getPos(pos, field_spc.position.size())];
	const auto kind = position != 0u ? engine::FieldKind::text : field_spc.flags.bool_term ? engine::FieldKind::boolean : engine::FieldKind::terms;
	const auto doc_field = doc.field(field, kind);
	if (position != 0u) {
		doc.add_posting(doc_field, term, position, weight);
	} else {
		doc.add_term(doc_field, term, weight);
	}
	L_INDEX("Field Term [{}] -> {} {}  Bool: {}  Posting: {}", pos, repr(field), repr(term), field_spc.flags.bool_term, position);
}


inline void
Schema::index_item(engine::Document& doc, const MsgPack& item, size_t pos)
{
	L_CALL("Schema::index_item(<doc>, <item>, {})", pos);

	assert(specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty);
	assert(specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::object);

	if (item.is_undefined()) {
		return;
	}

	bool field_terms = toUType(specification.index & TypeIndex::FIELD_TERMS) != 0u;

	if (item.is_null()) {
		if (field_terms) {
			index_term(doc, specification.prefix.field, std::string_view(), specification, pos);
		}
		return;
	}

	bool field_values = toUType(specification.index & TypeIndex::FIELD_VALUES) != 0u;

	bool global_terms = toUType(specification.index & TypeIndex::GLOBAL_TERMS) != 0u;
	bool global_values = toUType(specification.index & TypeIndex::GLOBAL_VALUES) != 0u;

	const auto& g_specification = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);

	// A number meets its field by the one rule (D92, Serialize::exact); a cast object converts any other value.
	const auto concrete_type = specification.sep_types[SPC_CONCRETE_TYPE];
	const bool numeric = concrete_type == FieldType::integer || concrete_type == FieldType::positive || concrete_type == FieldType::floating;
	MsgPack value_holder;
	const MsgPack& value = numeric ? value_holder = Serialize::exact(specification.full_meta_name, concrete_type, item)
		: item.is_map() ? value_holder = Cast::cast(item) : item;

	switch (concrete_type) {
		case FieldType::floating: {
			// An integer keeps its exact bytes and terms; a float's terms are its truncation (design D6).
			std::string ser_value;
			int64_t term_val;
			switch (value.get_type()) {
				case MsgPack::Type::NEGATIVE_INTEGER:
					term_val = value.i64();
					ser_value = Serialize::integer(term_val);
					break;
				case MsgPack::Type::POSITIVE_INTEGER:
					term_val = static_cast<int64_t>(std::min(value.u64(), static_cast<uint64_t>(std::numeric_limits<int64_t>::max())));
					ser_value = Serialize::positive(value.u64());
					break;
				default:
					term_val = GenerateTerms::truncate<int64_t>(value.f64());
					ser_value = Serialize::floating(FieldType::floating, value.f64());
					break;
			}
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, term_val);
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, term_val);
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, term_val);
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, term_val);
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, term_val);
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::integer: {
			const auto i_val = value.i64();
			auto ser_value = Serialize::integer(i_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(i_val));
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(i_val));
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(i_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(i_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(i_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::positive: {
			const auto u_val = value.u64();
			auto ser_value = Serialize::positive(u_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(u_val));
				} else {
					GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(u_val));
					GenerateTerms::positive(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(u_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::positive(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(u_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(u_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::date:
		case FieldType::datetime: {
			Datetime::tm_t tm;
			auto ser_value = Serialize::datetime(value, tm);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::datetime(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, tm);
				} else {
					GenerateTerms::datetime(doc, specification.accuracy, specification.acc_prefix, tm);
					GenerateTerms::datetime(doc, g_specification.accuracy, g_specification.acc_prefix, tm);
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::datetime(doc, g_specification.accuracy, g_specification.acc_prefix, tm);
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::datetime(doc, specification.accuracy, specification.acc_prefix, tm);
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::time: {
			double t_val = 0.0;
			auto ser_value = Serialize::time(value, t_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::timedelta: {
			double t_val = 0.0;
			auto ser_value = Serialize::timedelta(value, t_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::geo: {
			GeoSpatial geo(value);
			const auto& geometry = geo.getGeometry();
			if ((global_terms || global_values) && (field_terms || field_values)) {
				if (specification.flags.partials == g_specification.flags.partials && specification.error == g_specification.error) {
					auto ranges = geometry->getRanges(specification.flags.partials, specification.error);
					if (!ranges.empty()) {
						if (global_terms && field_terms) {
							auto ser_value = Serialize::ranges_hash(ranges);
							index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
							index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
							if (specification.accuracy == g_specification.accuracy) {
								GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, ranges);
							} else {
								GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, ranges);
								GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, ranges);
							}
						} else if (global_terms) {
							auto ser_value = Serialize::ranges_hash(ranges);
							index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
							GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, ranges);
						} else if (field_terms) {
							auto ser_value = Serialize::ranges_hash(ranges);
							index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
							GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, ranges);
						}
						if (global_values) {
							merge_geospatial_values(map_values[g_specification.slot], ranges, geometry->getCentroids());
						}
						if (field_values) {
							merge_geospatial_values(map_values[specification.slot], std::move(ranges), geometry->getCentroids());
						}
					}
				} else {
					auto global_ranges = geometry->getRanges(g_specification.flags.partials, g_specification.error);
					if (!global_ranges.empty()) {
						if (global_terms) {
							auto ser_value = Serialize::ranges_hash(global_ranges);
							index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
							GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, global_ranges);
						}
						if (global_values) {
							merge_geospatial_values(map_values[g_specification.slot], std::move(global_ranges), geometry->getCentroids());
						}
					}
					auto field_ranges = geometry->getRanges(specification.flags.partials, specification.error);
					if (!field_ranges.empty()) {
						if (field_terms) {
							auto ser_value = Serialize::ranges_hash(field_ranges);
							index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
							GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, field_ranges);
						}
						if (field_values) {
							merge_geospatial_values(map_values[specification.slot], std::move(field_ranges), geometry->getCentroids());
						}
					}
				}
			} else if (global_terms || global_values) {
				auto ranges = geometry->getRanges(g_specification.flags.partials, g_specification.error);
				if (!ranges.empty()) {
					if (global_terms) {
						auto ser_value = Serialize::ranges_hash(ranges);
						index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
						GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, ranges);
					}
					if (global_values) {
						merge_geospatial_values(map_values[g_specification.slot], std::move(ranges), geometry->getCentroids());
					}
				}
			} else if (field_terms || field_values) {
				auto ranges = geometry->getRanges(specification.flags.partials, specification.error);
				if (!ranges.empty()) {
					if (field_terms) {
						auto ser_value = Serialize::ranges_hash(ranges);
						index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
						GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, ranges);
					}
					if (field_values) {
						merge_geospatial_values(map_values[specification.slot], std::move(ranges), geometry->getCentroids());
					}
				}
			}
			return;
		}

		case FieldType::keyword: {
			if (value.is_string()) {
				auto ser_value = value.str();
				if (global_terms) {
					GenerateTerms::text(doc,
						g_specification.prefix.field + g_specification.get_ctype(), ser_value,
						g_specification.positions[getPos(pos, g_specification.positions.size())],
						g_specification.flags.bool_term ? 0 : g_specification.weight[getPos(pos, g_specification.weight.size())],
						g_specification.flags.cjk_ngram,
						g_specification.language, g_specification.stem_language,
						getGeneratorStopStrategy(g_specification.stop_strategy),
						getGeneratorStemStrategy(g_specification.stem_strategy),
						g_specification.flags.cjk_words);
				}
				if (field_terms) {
					if (specification.flags.bool_term) {
						index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
					} else {
						index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), strings::lower(ser_value), specification, pos);
					}
				}
				if (global_values) {
					map_values[g_specification.slot].insert(ser_value);
				}
				if (field_values) {
					map_values[specification.slot].insert(std::move(ser_value));
				}
				return;
			} else {
				THROW(ClientError, "Format invalid for {} type: {}", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), value.to_string());
			}
		}

		case FieldType::text: {
			if (value.is_string()) {
				auto ser_value = value.str();
				if (global_terms) {
					GenerateTerms::text(doc,
						g_specification.prefix.field + g_specification.get_ctype(), ser_value,
						g_specification.positions[getPos(pos, g_specification.positions.size())],
						g_specification.flags.bool_term ? 0 : g_specification.weight[getPos(pos, g_specification.weight.size())],
						g_specification.flags.cjk_ngram,
						g_specification.language, g_specification.stem_language,
						getGeneratorStopStrategy(g_specification.stop_strategy),
						getGeneratorStemStrategy(g_specification.stem_strategy),
						g_specification.flags.cjk_words);
				}
				if (field_terms) {
					GenerateTerms::text(doc,
						specification.prefix.field + specification.get_ctype(), ser_value,
						specification.positions[getPos(pos, specification.positions.size())],
						specification.flags.bool_term ? 0 : specification.weight[getPos(pos, specification.weight.size())],
						specification.flags.cjk_ngram,
						specification.language, specification.stem_language,
						getGeneratorStopStrategy(specification.stop_strategy),
						getGeneratorStemStrategy(specification.stem_strategy),
						specification.flags.cjk_words);
				}
				if (ser_value.size() <= 100) {
					// For text and string, only add relatively short values
					if (global_values) {
						map_values[g_specification.slot].insert(ser_value);
					}
					if (field_values) {
						map_values[specification.slot].insert(std::move(ser_value));
					}
				}
				return;
			} else {
				THROW(ClientError, "Format invalid for {} type: {}", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), value.to_string());
			}
		}

		case FieldType::boolean: {
			auto ser_value = Serialize::MsgPack(specification, value);
			if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
			}
			if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::uuid: {
			if (value.is_string()) {
				auto ser_value = Serialize::uuid(value.str_view());
				if (global_terms) {
					index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				}
				if (field_terms) {
					index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				}
				if (global_values) {
					map_values[g_specification.slot].insert(ser_value);
				}
				if (field_values) {
					map_values[specification.slot].insert(std::move(ser_value));
				}
				return;
			} else {
				THROW(ClientError, "Format invalid for uuid type: {}", value.to_string());
			}
		}

		case FieldType::script:
			if (value.is_string()) {
				return;
			}
			THROW(ClientError, "Format invalid for {} type: {}", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), value.to_string());

		case FieldType::object:
			THROW(ClientError, "Type: '{}' is an invalid value type", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));

		default:
			THROW(ClientError, "Type: {:#04x} is an unknown value type", toUType(specification.sep_types[SPC_CONCRETE_TYPE]));
	}
}


void
Schema::store_item(const MsgPack& value, MsgPack& data)
{
	L_CALL("Schema::store_item({}, {})", value.to_string(), data.to_string());

	// Add value to data.
	auto& data_value = data[RESERVED_VALUE];

	switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::uuid:
			switch (data_value.get_type()) {
				case MsgPack::Type::UNDEFINED:
					data_value = normalize_uuid(value);
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(normalize_uuid(value));
					break;
				default:
					data_value = MsgPack({ data_value, normalize_uuid(value) });
					break;
			}
			break;

		case FieldType::date:
		case FieldType::datetime: {
			// A cast object (`{"_date": ...}`) is read through its cast, as index_item reads it.
			const auto iso = Datetime::iso8601(Datetime::DatetimeParser(value.is_map() ? Cast::cast(value) : value));
			switch (data_value.get_type()) {
				case MsgPack::Type::UNDEFINED:
					data_value = iso;
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(iso);
					break;
				default:
					data_value = MsgPack({ data_value, iso });
					break;
			}
			break;
		}

		default:
			switch (data_value.get_type()) {
				case MsgPack::Type::UNDEFINED:
					data_value = value;
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(value);
					break;
				default:
					data_value = MsgPack({ data_value, value });
					break;
			}
			break;
	}
}


void
Schema::merge_geospatial_values(std::set<std::string>& s, std::vector<range_t> ranges, std::vector<Cartesian> centroids)
{
	L_CALL("Schema::merge_geospatial_values(...)");

	if (s.empty()) {
		s.insert(Serialize::ranges_centroids(ranges, centroids));
	} else {
		auto prev_value = Unserialize::ranges_centroids(*s.begin());
		s.clear();
		ranges = HTM::range_union(std::move(ranges), std::vector<range_t>(std::make_move_iterator(prev_value.first.begin()), std::make_move_iterator(prev_value.first.end())));
		const auto& prev_centroids = prev_value.second;
		if (!prev_centroids.empty()) {
			std::vector<Cartesian> missing;
			auto centroids_begin = centroids.begin();
			auto centroids_end = centroids.end();
			for (const auto& _centroid : prev_centroids) {
				if (std::find(centroids_begin, centroids_end, _centroid) == centroids_end) {
					missing.push_back(_centroid);
				}
			}
			centroids.insert(centroids.end(), missing.begin(), missing.end());
		}
		s.insert(Serialize::ranges_centroids(ranges, centroids));
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


const MsgPack
Schema::get_full(bool readable) const
{
	L_CALL("Schema::get_full({})", readable);

	auto full_schema = get_schema();
	if (readable) {
		dispatch_readable(full_schema, true);
	}
	if (!origin.empty()) {
		full_schema[RESERVED_TYPE] = "foreign/object";
		full_schema[RESERVED_FOREIGN] = origin;
	}
	return full_schema;
}


void
Schema::dispatch_readable(MsgPack& item_schema, bool at_root)
{
	L_CALL("Schema::dispatch_readable({}, {})", repr(item_schema.to_string()), at_root);

	constexpr static auto _ = phf::make_phf({
		hh(RESERVED_TYPE),
		hh(RESERVED_PREFIX),
		hh(RESERVED_SLOT),
		hh(RESERVED_STEM_LANGUAGE),
		hh(RESERVED_ACC_PREFIX),
		hh(RESERVED_SCRIPT),
	});

	// Change this item of schema in readable form.
	for (auto it = item_schema.begin(); it != item_schema.end(); ) {
		auto str_key = it->str_view();
		auto& value = it.value();
		if (str_key == RESERVED_STOP_FINGERPRINT) {
			// Derived from the stop words list on disk, not a setting: a schema read back and sent again
			// must not carry a value that no longer matches its list.
			it = item_schema.erase(it);
			continue;
		}
		if (is_reserved(str_key)) {
			auto key = hh(str_key);
			switch (key) {
				case _.fhh(RESERVED_PREFIX):
					if (!Schema::readable_prefix(value, item_schema)) {
						it = item_schema.erase(it);
						continue;
					}
					break;
				case _.fhh(RESERVED_SLOT):
					if (!Schema::readable_slot(value, item_schema)) {
						it = item_schema.erase(it);
						continue;
					}
					break;
				case _.fhh(RESERVED_STEM_LANGUAGE):
					if (!Schema::readable_stem_language(value, item_schema)) {
						it = item_schema.erase(it);
						continue;
					}
					break;
				case _.fhh(RESERVED_ACC_PREFIX):
					if (!Schema::readable_acc_prefix(value, item_schema)) {
						it = item_schema.erase(it);
						continue;
					}
					break;
				case _.fhh(RESERVED_SCRIPT):
					if (!Schema::readable_script(value, item_schema)) {
						it = item_schema.erase(it);
						continue;
					}
					break;
				default:
					if (!at_root && has_dispatch_set_default_spc(key)) {
						it = item_schema.erase(it);
						continue;
					}
					if (value.is_map()) {
						dispatch_readable(value, false);
					}
					break;
			}
		} else if (is_valid(str_key)) {
			if (value.is_map()) {
				dispatch_readable(value, false);
			}
		} else {
			it = item_schema.erase(it);
			continue;
		}
		++it;
	}
}


inline bool
Schema::readable_prefix(MsgPack& /*unused*/, MsgPack& /*unused*/)
{
	L_CALL("Schema::readable_prefix(...)");

	return false;
}


inline bool
Schema::readable_slot(MsgPack& /*unused*/, MsgPack& /*unused*/)
{
	L_CALL("Schema::readable_slot(...)");

	return false;
}


inline bool
Schema::readable_stem_language(MsgPack& prop_obj, MsgPack& properties)
{
	L_CALL("Schema::readable_stem_language({})", repr(prop_obj.to_string()));

	const auto language = properties[RESERVED_LANGUAGE].str_view();
	const auto stem_language = prop_obj.str_view();

	return (language != stem_language);
}


inline bool
Schema::readable_acc_prefix(MsgPack& /*unused*/, MsgPack& /*unused*/)
{
	L_CALL("Schema::readable_acc_prefix(...)");

	return false;
}


inline bool
Schema::readable_script(MsgPack& prop_obj, MsgPack& /*unused*/)
{
	L_CALL("Schema::readable_script({})", repr(prop_obj.to_string()));

	dispatch_readable(prop_obj, false);
	return true;
}


namespace {

struct UnrecordedStopWords {
	std::vector<std::string> path;
	std::string language;
};

// Checks every field below `properties` that has a language against the fingerprint it recorded; the ones that
// recorded none (indexed before the fingerprints) are collected in `unrecorded`.
void
check_field_stop_words(const MsgPack& properties, std::vector<std::string>& path, std::string_view index, std::vector<UnrecordedStopWords>& unrecorded)
{
	const auto it_e = properties.end();
	const auto language_it = properties.find(RESERVED_LANGUAGE);
	if (language_it != it_e && language_it.value().is_string() && !language_it.value().str_view().empty()) {
		const auto language = language_it.value().str_view();
		const auto recorded_it = properties.find(RESERVED_STOP_FINGERPRINT);
		if (recorded_it == it_e) {
			unrecorded.push_back({path, std::string(language)});
		} else {
			checkStopWords(index, language, recorded_it.value().str_view());
		}
	}
	for (auto it = properties.begin(); it != it_e; ++it) {
		const auto str_key = it->str_view();
		if (!is_reserved(str_key) && is_valid(str_key) && it.value().is_map()) {
			path.emplace_back(str_key);
			check_field_stop_words(it.value(), path, index, unrecorded);
			path.pop_back();
		}
	}
}

// The schemas this thread verified with nothing left to adopt, by identity: a request that gets the same schema
// again pays a lookup, not a walk. The weak pointer keeps a freed schema's address from matching a new one.
class VerifiedSchemas {
	static constexpr std::size_t limit = 4096;
	std::unordered_map<const MsgPack*, std::weak_ptr<const MsgPack>> verified;

public:
	bool has(const std::shared_ptr<const MsgPack>& schema) const {
		const auto it = verified.find(schema.get());
		return it != verified.end() && it->second.lock() == schema;
	}

	void add(const std::shared_ptr<const MsgPack>& schema) {
		if (verified.size() >= limit) {
			std::erase_if(verified, [](const auto& entry) { return entry.second.expired(); });
			if (verified.size() >= limit) {
				verified.clear();
			}
		}
		verified[schema.get()] = schema;
	}
};

thread_local VerifiedSchemas verified_schemas;

}  // namespace


void
Schema::check_cached_stop_words(std::string_view index, const std::shared_ptr<const MsgPack>& schema)
{
	L_CALL("Schema::check_cached_stop_words({})", repr(index));

	if (!schema->is_map() || verified_schemas.has(schema)) {
		return;  // a foreign schema holds no fields of its own
	}
	std::vector<UnrecordedStopWords> unrecorded;
	std::vector<std::string> path;
	check_field_stop_words(*schema, path, index, unrecorded);
	if (unrecorded.empty()) {
		verified_schemas.add(schema);
	}
}


void
Schema::check_stop_words(std::string_view index, bool adopt)
{
	L_CALL("Schema::check_stop_words({}, {})", repr(index), adopt);

	const auto& root = get_newest_properties();
	if (!root.is_map()) {
		return;  // a foreign schema holds no fields of its own
	}
	if (!mut_schema && verified_schemas.has(schema)) {
		return;
	}
	std::vector<UnrecordedStopWords> unrecorded;
	std::vector<std::string> path;
	check_field_stop_words(root, path, index, unrecorded);
	if (unrecorded.empty() && !mut_schema) {
		verified_schemas.add(schema);
	}
	if (!adopt) {
		return;
	}
	static std::mutex adopted_mtx;
	static std::set<std::string> adopted;
	for (const auto& field : unrecorded) {
		auto* properties = &get_mutable_properties();
		for (const auto& name : field.path) {
			properties = &properties->get(name);
		}
		const auto fingerprint = stopWordsFingerprint(field.language);
		(*properties)[RESERVED_STOP_FINGERPRINT] = fingerprint;
		std::lock_guard<std::mutex> lk(adopted_mtx);
		if (adopted.insert(std::string(index) + '\0' + field.language + '\0' + fingerprint).second) {
			L_INFO("Index {} adopts stop words list {} (fingerprint {}): it recorded none", repr(index), repr(field.language), fingerprint);
		}
	}
}


std::shared_ptr<const MsgPack>
Schema::get_modified_schema()
{
	L_CALL("Schema::get_modified_schema()");

	if (!mut_schema) {
		return std::shared_ptr<const MsgPack>();
	}
	auto m_schema = std::shared_ptr<const MsgPack>(mut_schema.release());
	m_schema->lock();
	return m_schema;
}


std::shared_ptr<const MsgPack>
Schema::get_const_schema() const
{
	L_CALL("Schema::get_const_schema()");

	return schema;
}


std::string
Schema::to_string(bool prettify) const
{
	L_CALL("Schema::to_string({})", prettify);

	return get_full(true).to_string(static_cast<int>(prettify));
}


required_spc_t
_get_data_id(required_spc_t& spc_id, const MsgPack& id_properties)
{
	auto id_it_e = id_properties.end();

	auto id_type_it = id_properties.find(RESERVED_TYPE);
	if (id_type_it != id_it_e) {
		spc_id.sep_types[SPC_CONCRETE_TYPE] = required_spc_t::get_types(id_type_it.value().str_view())[SPC_CONCRETE_TYPE];
	}
	auto id_slot_it = id_properties.find(RESERVED_SLOT);
	if (id_slot_it != id_it_e) {
		spc_id.slot = static_cast<engine::valueno>(id_slot_it.value().u64());
	}
	auto id_prefix_it = id_properties.find(RESERVED_PREFIX);
	if (id_slot_it != id_it_e) {
		spc_id.prefix.field.assign(id_prefix_it.value().str_view());
	}

	// Get required specification.
	switch (spc_id.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::geo: {
			auto id_partials_it = id_properties.find(RESERVED_PARTIALS);
			if (id_partials_it != id_it_e) {
				spc_id.flags.partials = id_partials_it.value().boolean();
			}
			auto id_error_it = id_properties.find(RESERVED_ERROR);
			if (id_error_it != id_it_e) {
				spc_id.error = id_error_it.value().f64();
			}
			break;
		}
		case FieldType::keyword: {
			auto id_bool_term_it = id_properties.find(RESERVED_BOOL_TERM);
			if (id_bool_term_it != id_it_e) {
				spc_id.flags.bool_term = id_bool_term_it.value().boolean();
			}
			break;
		}
		default:
			break;
	}

	return spc_id;
}


required_spc_t
Schema::get_data_id() const
{
	L_CALL("Schema::get_data_id()");

	required_spc_t spc_id;

	// Set default prefix
	spc_id.prefix.field = DOCUMENT_ID_TERM_PREFIX;

	// Set default RESERVED_SLOT
	spc_id.slot = DB_SLOT_ID;

	const auto& properties = get_newest_properties();
	auto it = properties.find(ID_FIELD_NAME);
	if (it == properties.end()) {
		return spc_id;
	}

	const auto& id_properties = it.value();
	if (!id_properties.is_map()) {
		return spc_id;
	}
	return _get_data_id(spc_id, id_properties);
}


void
Schema::set_data_id(const required_spc_t& spc_id)
{
	L_CALL("Schema::set_data_id(<spc_id>)");

	auto& mut_properties = get_mutable_properties(ID_FIELD_NAME);

	mut_properties[RESERVED_TYPE] = spc_id.get_str_type();
	mut_properties[RESERVED_SLOT] = spc_id.slot;
	mut_properties[RESERVED_PREFIX] = spc_id.prefix.field;

	switch (spc_id.get_type()) {
		case FieldType::geo: {
			mut_properties[RESERVED_PARTIALS] = spc_id.flags.partials;
			mut_properties[RESERVED_ERROR] = spc_id.error;
			break;
		}
		case FieldType::keyword: {
			mut_properties[RESERVED_BOOL_TERM] = spc_id.flags.bool_term;
			break;
		}
		default:
			break;
	}
}


MsgPack
Schema::get_data_script() const
{
	L_CALL("Schema::get_data_script()");

	const auto& properties = get_newest_properties();
	auto it_e = properties.end();
	auto it = properties.find(RESERVED_SCRIPT);
	if (it != it_e) {
		return it.value();
	}
	return MsgPack();
}


std::pair<required_spc_t, std::string>
Schema::get_data_field(std::string_view field_name, bool is_range) const
{
	L_CALL("Schema::get_data_field({}, {})", repr(field_name), is_range);

	required_spc_t res;

	if (field_name.empty()) {
		return std::make_pair(std::move(res), "");
	}

	auto spc = get_dynamic_subproperties(get_properties(), field_name);
	res.flags.inside_namespace = spc.inside_namespace;
	res.prefix.field = std::move(spc.prefix);

	if (!spc.acc_field.empty()) {
		res.sep_types[SPC_CONCRETE_TYPE] = spc.acc_field_type;
		return std::make_pair(res, std::move(spc.acc_field));
	}

	const auto& properties = *spc.properties;
	auto it_e = properties.end();

	auto type_it = properties.find(RESERVED_TYPE);
	if (type_it != it_e) {
		res.sep_types[SPC_CONCRETE_TYPE] = required_spc_t::get_types(type_it.value().str_view())[SPC_CONCRETE_TYPE];
	}
	if (res.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
		return std::make_pair(std::move(res), "");
	}

	if (spc.has_uuid_prefix || spc.inside_namespace) {
		res.slot = get_slot(res.prefix.field, res.get_ctype());
	} else {
		auto slot_it = properties.find(RESERVED_SLOT);
		if (slot_it != it_e) {
			res.slot = static_cast<engine::valueno>(slot_it.value().u64());
		}
	}

	if (is_range) {
		if (spc.has_uuid_prefix) {
			res.slot = get_slot(res.prefix.field, res.get_ctype());
		} else {
			auto slot_it = properties.find(RESERVED_SLOT);
			if (slot_it != it_e) {
				res.slot = static_cast<engine::valueno>(slot_it.value().u64());
			}
		}

		// Get required specification.
		switch (res.sep_types[SPC_CONCRETE_TYPE]) {
			case FieldType::geo: {
				auto partials_it = properties.find(RESERVED_PARTIALS);
				if (partials_it != it_e) {
					res.flags.partials = partials_it.value().boolean();
				}
				auto error_it = properties.find(RESERVED_ERROR);
				if (error_it != it_e) {
					res.error = error_it.value().f64();
				}
			}
			[[fallthrough]];
			case FieldType::floating:
			case FieldType::integer:
			case FieldType::positive:
			case FieldType::date:
			case FieldType::datetime:
			case FieldType::time:
			case FieldType::timedelta: {
				auto accuracy_it = properties.find(RESERVED_ACCURACY);
				if (accuracy_it != it_e) {
					for (const auto& acc : accuracy_it.value()) {
						uint64_t accuracy;
						if (acc.is_string()) {
							auto accuracy_date = _get_accuracy_datetime(acc.str_view());
							if (accuracy_date != UnitTime::INVALID) {
								accuracy = toUType(accuracy_date);
							} else {
								THROW(Error, "Schema is corrupt: '{}' in {} is not valid.", RESERVED_ACCURACY, specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
							}
						} else {
							accuracy = acc.u64();
						}
						res.accuracy.push_back(accuracy);
					}
				}
				auto acc_prefix_it = properties.find(RESERVED_ACC_PREFIX);
				if (acc_prefix_it != it_e) {
					for (const auto& acc_p : acc_prefix_it.value()) {
						res.acc_prefix.push_back(std::string(acc_term_prefix(res.prefix.field)).append(acc_p.str_view()));
					}
				}
				break;
			}
			case FieldType::text: {
				auto ngram_it = properties.find(RESERVED_NGRAM);
				if (ngram_it != it_e) {
					res.flags.ngram = ngram_it.value().boolean();
				}
				auto cjk_ngram_it = properties.find(RESERVED_CJK_NGRAM);
				if (cjk_ngram_it != it_e) {
					res.flags.cjk_ngram = cjk_ngram_it.value().boolean();
				}
				auto cjk_words_it = properties.find(RESERVED_CJK_WORDS);
				if (cjk_words_it != it_e) {
					res.flags.cjk_words = cjk_words_it.value().boolean();
				}
				auto language_it = properties.find(RESERVED_LANGUAGE);
				if (language_it != it_e) {
					res.language = language_it.value().str();
				}
				if (!res.language.empty()) {
					auto stop_strategy_it = properties.find(RESERVED_STOP_STRATEGY);
					if (stop_strategy_it != it_e) {
						res.stop_strategy = _get_stop_strategy(stop_strategy_it.value().str_view());
					}
				}
				auto stem_language_it = properties.find(RESERVED_STEM_LANGUAGE);
				if (stem_language_it != it_e) {
					res.stem_language = _get_stemmer_name(stem_language_it.value().str_view());
				}
				if (!res.stem_language.empty()) {
					auto stem_strategy_it = properties.find(RESERVED_STEM_STRATEGY);
					if (stem_strategy_it != it_e) {
						res.stem_strategy = enum_type<StemStrategy>(stem_strategy_it.value().str_view());
					}
				}
				break;
			}
			case FieldType::keyword: {
				auto bool_term_it = properties.find(RESERVED_BOOL_TERM);
				if (bool_term_it != it_e) {
					res.flags.bool_term = bool_term_it.value().boolean();
				}
				break;
			}
			default:
				break;
		}
	} else {
		// Get required specification.
		switch (res.sep_types[SPC_CONCRETE_TYPE]) {
			case FieldType::geo: {
				auto partials_it = properties.find(RESERVED_PARTIALS);
				if (partials_it != it_e) {
					res.flags.partials = partials_it.value().boolean();
				}
				auto error_it = properties.find(RESERVED_ERROR);
				if (error_it != it_e) {
					res.error = error_it.value().f64();
				}
				break;
			}
			case FieldType::text: {
				auto ngram_it = properties.find(RESERVED_NGRAM);
				if (ngram_it != it_e) {
					res.flags.ngram = ngram_it.value().boolean();
				}
				auto cjk_ngram_it = properties.find(RESERVED_CJK_NGRAM);
				if (cjk_ngram_it != it_e) {
					res.flags.cjk_ngram = cjk_ngram_it.value().boolean();
				}
				auto cjk_words_it = properties.find(RESERVED_CJK_WORDS);
				if (cjk_words_it != it_e) {
					res.flags.cjk_words = cjk_words_it.value().boolean();
				}
				auto language_it = properties.find(RESERVED_LANGUAGE);
				if (language_it != it_e) {
					res.language = language_it.value().str();
				}
				if (!res.language.empty()) {
					auto stop_strategy_it = properties.find(RESERVED_STOP_STRATEGY);
					if (stop_strategy_it != it_e) {
						res.stop_strategy = _get_stop_strategy(stop_strategy_it.value().str_view());
					}
				}
				auto stem_language_it = properties.find(RESERVED_STEM_LANGUAGE);
				if (stem_language_it != it_e) {
					res.stem_language = _get_stemmer_name(stem_language_it.value().str_view());
				}
				if (!res.stem_language.empty()) {
					auto stem_strategy_it = properties.find(RESERVED_STEM_STRATEGY);
					if (stem_strategy_it != it_e) {
						res.stem_strategy = enum_type<StemStrategy>(stem_strategy_it.value().str_view());
					}
				}
				break;
			}
			case FieldType::keyword: {
				auto bool_term_it = properties.find(RESERVED_BOOL_TERM);
				if (bool_term_it != it_e) {
					res.flags.bool_term = bool_term_it.value().boolean();
				}
				break;
			}
			default:
				break;
		}
	}

	return std::make_pair(std::move(res), "");
}


required_spc_t
Schema::get_slot_field(std::string_view field_name) const
{
	L_CALL("Schema::get_slot_field({})", repr(field_name));

	required_spc_t res;

	if (field_name.empty()) {
		return res;
	}

	auto spc = get_dynamic_subproperties(get_properties(), field_name);
	res.flags.inside_namespace = spc.inside_namespace;
	res.prefix.field = std::move(spc.prefix);

	if (!spc.acc_field.empty()) {
		THROW(ClientError, "Field {} is an accuracy, therefore does not have slot", repr(field_name));
	}

	const auto& properties = *spc.properties;
	auto it_e = properties.end();

	auto type_it = properties.find(RESERVED_TYPE);
	if (type_it != it_e) {
		res.sep_types[SPC_CONCRETE_TYPE] = required_spc_t::get_types(type_it.value().str_view())[SPC_CONCRETE_TYPE];
	}

	if (spc.has_uuid_prefix || spc.inside_namespace) {
		res.slot = get_slot(res.prefix.field, res.get_ctype());
	} else {
		auto slot_it = properties.find(RESERVED_SLOT);
		if (slot_it != it_e) {
			res.slot = static_cast<engine::valueno>(slot_it.value().u64());
		}
	}

	if (const auto index_it = properties.find(RESERVED_INDEX); index_it != it_e && index_it.value().is_string()) {
		res.field_values = toUType(_get_index(index_it.value().str_view()) & TypeIndex::FIELD_VALUES) != 0u;
	}

	// Get required specification.
	switch (res.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::geo: {
			auto partials_it = properties.find(RESERVED_PARTIALS);
			if (partials_it != it_e) {
				res.flags.partials = partials_it.value().boolean();
			}
			auto error_it = properties.find(RESERVED_ERROR);
			if (error_it != it_e) {
				res.error = error_it.value().f64();
			}
			break;
		}
		case FieldType::text: {
			auto ngram_it = properties.find(RESERVED_NGRAM);
			if (ngram_it != it_e) {
				res.flags.ngram = ngram_it.value().boolean();
			}
			auto cjk_ngram_it = properties.find(RESERVED_CJK_NGRAM);
			if (cjk_ngram_it != it_e) {
				res.flags.cjk_ngram = cjk_ngram_it.value().boolean();
			}
			auto cjk_words_it = properties.find(RESERVED_CJK_WORDS);
			if (cjk_words_it != it_e) {
				res.flags.cjk_words = cjk_words_it.value().boolean();
			}
			auto language_it = properties.find(RESERVED_LANGUAGE);
			if (language_it != it_e) {
				res.language = language_it.value().str();
			}
			if (!res.language.empty()) {
				auto stop_strategy_it = properties.find(RESERVED_STOP_STRATEGY);
				if (stop_strategy_it != it_e) {
					res.stop_strategy = _get_stop_strategy(stop_strategy_it.value().str_view());
				}
			}
			auto stem_language_it = properties.find(RESERVED_STEM_LANGUAGE);
			if (stem_language_it != it_e) {
				res.stem_language = _get_stemmer_name(stem_language_it.value().str_view());
			}
			if (!res.stem_language.empty()) {
				auto stem_strategy_it = properties.find(RESERVED_STEM_STRATEGY);
				if (stem_strategy_it != it_e) {
					res.stem_strategy = enum_type<StemStrategy>(stem_strategy_it.value().str_view());
				}
			}
			break;
		}
		case FieldType::keyword: {
			auto bool_term_it = properties.find(RESERVED_BOOL_TERM);
			if (bool_term_it != it_e) {
				res.flags.bool_term = bool_term_it.value().boolean();
			}
			break;
		}
		default:
			break;
	}

	return res;
}


Schema::dynamic_spc_t
Schema::get_dynamic_subproperties(const MsgPack& properties, std::string_view full_name) const
{
	L_CALL("Schema::get_dynamic_subproperties({}, {})", properties.to_string(), repr(full_name));

	Split<std::string_view> field_names(full_name, DB_OFFSPRING_UNION);

	dynamic_spc_t spc(&properties);

	bool is_namespace = false;

	const auto it_e = field_names.end();
	const auto it_b = field_names.begin();
	for (auto it = it_b; it != it_e; ++it) {
		std::string default_prefix;
		auto field_name = *it;
		if (!is_valid(field_name)) {
			// Check if the field_name is accuracy.
			if (it == it_b) {
				auto key = hh(field_name);
				if (has_dispatch_set_default_spc(key)) {
					constexpr static auto _ = phf::make_phf({
						hh(ID_FIELD_NAME),
						hh(VERSION_FIELD_NAME),
					});
					switch (_.find(key)) {
						case _.fhh(ID_FIELD_NAME):
							default_prefix = DOCUMENT_ID_TERM_PREFIX;
							break;
						case _.fhh(VERSION_FIELD_NAME):
							default_prefix = DOCUMENT_VERSION_TERM_PREFIX;
							break;
					}
				} else {
					if (++it == it_e) {
						auto acc_data = _get_acc_data(field_name);
						spc.prefix.append(acc_data.first);
						spc.acc_field.assign(std::move(field_name));
						spc.acc_field_type = acc_data.second;
						return spc;
					}
					THROW(ClientError, "The field name: {} in {} is not valid", repr_field(full_name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
			} else if (++it == it_e) {
				auto acc_data = _get_acc_data(field_name);
				spc.prefix.append(acc_data.first);
				spc.acc_field.assign(std::move(field_name));
				spc.acc_field_type = acc_data.second;
				return spc;
			} else {
				THROW(ClientError, "Field {} in {} is not valid", repr_field(full_name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
		}

		auto field_it = spc.properties->find(field_name);
		if (field_it != spc.properties->end()) {
			spc.properties = &field_it.value();
			// Get configured prefix:
			auto prefix_it = spc.properties->find(RESERVED_PREFIX);
			if (prefix_it != spc.properties->end()) {
				auto& prop_obj = prefix_it.value();
				if (prop_obj.is_string()) {
					spc.prefix.append(prop_obj.str_view());
				} else {
					THROW(Error, "Schema is corrupt: '{}' is not valid.", RESERVED_PREFIX);
				}
			} else {
				spc.prefix.append(default_prefix.empty() ? get_prefix(field_name) : default_prefix);
			}
			// Get namespace:
			auto namespace_it = spc.properties->find(RESERVED_NAMESPACE);
			if (namespace_it != spc.properties->end()) {
				auto& prop_obj = namespace_it.value();
				if (prop_obj.is_boolean()) {
					is_namespace = prop_obj.boolean();
				} else {
					THROW(Error, "Schema is corrupt: '{}' is not valid.", RESERVED_NAMESPACE);
				}
			}
		} else {
			if (Serialize::possiblyUUID(field_name)) {
				try {
					const auto prefix_uuid = Serialize::uuid(field_name);
					spc.has_uuid_prefix = true;
					field_it = spc.properties->find(UUID_FIELD_NAME);
					if (field_it != spc.properties->end()) {
						spc.properties = &field_it.value();
					}
					spc.prefix.append(prefix_uuid);
				} catch (const SerializationError&) {
					spc.prefix.append(default_prefix.empty() ? get_prefix(field_name) : default_prefix);
				}
			} else {
				spc.prefix.append(default_prefix.empty() ? get_prefix(field_name) : default_prefix);
			}

			// It is a search using partial prefix.
			size_t depth_partials = std::distance(it, it_e);
			if (depth_partials > LIMIT_PARTIAL_PATHS_DEPTH) {
				THROW(ClientError, "Partial paths limit depth is {}, and partial paths provided has a depth of {}", LIMIT_PARTIAL_PATHS_DEPTH, depth_partials);
			}
			spc.inside_namespace = is_namespace;
			for (++it; it != it_e; ++it) {
				auto partial_field = *it;
				if (is_valid(partial_field)) {
					if (Serialize::possiblyUUID(field_name)) {
						try {
							spc.prefix.append(Serialize::uuid(partial_field));
							spc.has_uuid_prefix = true;
						} catch (const SerializationError&) {
							spc.prefix.append(default_prefix.empty() ? get_prefix(partial_field) : default_prefix);
						}
					} else {
						spc.prefix.append(default_prefix.empty() ? get_prefix(partial_field) : default_prefix);
					}
				} else if (++it == it_e) {
					auto acc_data = _get_acc_data(partial_field);
					spc.prefix.append(acc_data.first);
					spc.acc_field.assign(std::move(partial_field));
					spc.acc_field_type = acc_data.second;
					return spc;
				} else {
					THROW(ClientError, "Field {} in {} is not valid", repr_field(full_name, partial_field), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
			}
			return spc;
		}
	}

	return spc;
}


required_spc_t
Schema::get_prefixed_global(FieldType namespace_type, const std::string& prefix_namespace)
{
	L_CALL("Schema::get_prefixed_global('{}', {})", toUType(namespace_type), repr(prefix_namespace));

	auto spc = specification_t::get_global(namespace_type);

	if (!prefix_namespace.empty()) {
		spc.prefix.field = prefix_namespace;

		// If the namespace field is ID_FIELD_NAME, restart its default values.
		if (prefix_namespace == NAMESPACE_PREFIX_ID_FIELD_NAME) {
			// ID_FIELD_NAME cannot be text or string.
			if (spc.sep_types[SPC_CONCRETE_TYPE] == FieldType::text) {
				spc.sep_types[SPC_CONCRETE_TYPE] = FieldType::keyword;
			}
		}

		spc.slot = get_slot(spc.prefix.field, spc.get_ctype());

		switch (spc.get_type()) {
			case FieldType::integer:
			case FieldType::positive:
			case FieldType::floating:
			case FieldType::date:
			case FieldType::datetime:
			case FieldType::time:
			case FieldType::timedelta:
			case FieldType::geo:
				for (auto& acc_prefix : spc.acc_prefix) {
					acc_prefix.insert(0, acc_term_prefix(spc.prefix.field));
				}
				break;
			default:
				break;
		}
	}

	return std::move(spc);
}
