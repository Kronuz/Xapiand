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

// Schema::update and Schema::write: the walks of a schema object that update or write the
// schema's properties.

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
