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

// Schema::index: the walk of a document against the schema, field by field.

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



required_spc_t _get_data_id(required_spc_t& spc_id, const MsgPack& id_properties);

/*  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 *      ___           _
 *     |_ _|_ __   __| | _____  __
 *      | || '_ \ / _` |/ _ \ \/ /
 *      | || | | | (_| |  __/>  <
 *     |___|_| |_|\__,_|\___/_/\_\
 *  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 */

task::task<std::tuple<std::string, engine::Document, MsgPack>>
Schema::index(const MsgPack& object, MsgPack document_id, DatabaseHandler& db_handler, const Data& data, size_t seq)
{
	L_CALL("Schema::index({}, {}, <db_handler>)", object.to_string(), document_id.to_string());

	// Mint ids with the v6 codec by default; --legacy-ids pins pure v1. Encode through the
	// version-nibble dispatch so a v6 id gets its compact wire (UUID::serialize() alone would
	// fall back to the full form for a version-6 value).
	const auto mint = [](bool compact) {
		return cuuid::serialize(cuuid::generate(opts.legacy_ids ? cuuid::Codec::v1 : cuuid::Codec::v6, compact));
	};

	L_INDEX(">>> Index schema {} from: " + DIM_GREY + "{}", document_id.to_string(), object.to_string());

	try {
		map_values.clear();
		specification = default_spc;
		specification.slot = DB_SLOT_ROOT;  // Set default RESERVED_SLOT for root

		Fields fields;
		Field* id_field = nullptr;
		Field* version_field = nullptr;
		auto properties = &get_newest_properties();

		if (object.empty()) {
			dispatch_feed_properties(*properties);
		} else if (properties->empty()) {  // new schemas have empty properties
			specification.flags.field_found = false;
			auto mut_properties = &get_mutable_properties();
			dispatch_write_properties(*mut_properties, object, fields, &id_field, &version_field);
			properties = &*mut_properties;
		} else {
			dispatch_feed_properties(*properties);
			dispatch_process_properties(object, fields, &id_field, &version_field);
		}

		auto spc_id = get_data_id();
		if (id_field != nullptr && id_field->second != nullptr && id_field->second->is_map()) {
			_get_data_id(spc_id, *id_field->second);
		}
		auto id_type = spc_id.get_type();

		std::string unprefixed_term_id;
		std::string term_id;

		if (!has_document_id(document_id)) {
			switch (id_type) {
				case FieldType::empty:
					id_type = FieldType::uuid;
					spc_id.set_type(id_type);
					set_data_id(spc_id);
					properties = &get_mutable_properties();
				[[fallthrough]];
				case FieldType::uuid: {
					// Try getting a new ID which can currently be indexed (active node)
					size_t n_shards = db_handler.endpoints.size();
					size_t shard_num;
					if (seq) {
						shard_num = seq % n_shards;
					} else {
						shard_num = random_int(0, n_shards - 1);
						// Get the least used shard:
						auto min_doccount = std::numeric_limits<engine::doccount>::max();
						for (size_t n = 0; n < n_shards; ++n) {
							auto& endpoint = db_handler.endpoints[n];
							if (endpoint.is_active()) {
								try {
									lock_shard lk_shard(endpoint, db_handler.flags, db_handler.turn_token());
									co_await lk_shard.pin();
									auto doccount = (co_await lk_shard->engine_for_read())->doc_count();
									if (min_doccount > doccount) {
										min_doccount = doccount;
										shard_num = n;
									}
								} catch (...) {}
							}
						}
					}
					// Figure out a term which goes into the least used shard:
					for (int t = 100; t >= 0; --t) {
						auto tmp_unprefixed_term_id = mint(opts.uuid_compact);
						auto tmp_term_id = document_key(tmp_unprefixed_term_id, spc_id.get_ctype());
						auto tmp_shard_num = id_shard(tmp_term_id, n_shards);
						if (db_handler.endpoints[tmp_shard_num].is_active()) {
							if (shard_num == tmp_shard_num || !db_handler.endpoints[shard_num].is_active()) {
								unprefixed_term_id = std::move(tmp_unprefixed_term_id);
								term_id = std::move(tmp_term_id);
								break;
							}
						}
					}
					if (unprefixed_term_id.empty()) {
						// The loop above is a shard-placement optimization; if no candidate
						// landed on an active target shard (shards can be momentarily inactive
						// while being created, a race that surfaced as an intermittent
						// "Invalid UUID format: ''"), fall back to a valid generated UUID
						// rather than leaving the id empty.
						unprefixed_term_id = mint(opts.uuid_compact);
						term_id = document_key(unprefixed_term_id, spc_id.get_ctype());
					}
					document_id = Unserialize::uuid(unprefixed_term_id, static_cast<UUIDRepr>(opts.uuid_repr));
					break;
				}
				// A new numeric id: the shard issues it when it is written; 0 holds its place until then.
				case FieldType::integer:
					document_id = MsgPack(0).as_i64();
					term_id = NEW_NUMERIC_KEY;
					break;
				case FieldType::positive:
					document_id = MsgPack(0).as_u64();
					term_id = NEW_NUMERIC_KEY;
					break;
				case FieldType::floating:
					document_id = MsgPack(0).as_f64();
					term_id = NEW_NUMERIC_KEY;
					break;
				case FieldType::text:
				case FieldType::keyword: {
					// Try getting a new ID which can currently be indexed (active node)
					size_t n_shards = db_handler.endpoints.size();
					size_t shard_num;
					if (seq) {
						shard_num = seq % n_shards;
					} else {
						shard_num = random_int(0, n_shards - 1);
						// Get the least used shard:
						auto min_doccount = std::numeric_limits<engine::doccount>::max();
						for (size_t n = 0; n < n_shards; ++n) {
							auto& endpoint = db_handler.endpoints[n];
							if (endpoint.is_active()) {
								try {
									lock_shard lk_shard(endpoint, db_handler.flags, db_handler.turn_token());
									co_await lk_shard.pin();
									auto doccount = (co_await lk_shard->engine_for_read())->doc_count();
									if (min_doccount > doccount) {
										min_doccount = doccount;
										shard_num = n;
									}
								} catch (...) {}
							}
						}
					}
					// Figure out a term which goes into the least used shard:
					for (int t = 100; t >= 0; --t) {
						auto tmp_document_id = Base64::rfc4648url_unpadded().encode(mint(true));
						auto tmp_unprefixed_term_id = Serialize::serialize(spc_id, tmp_document_id);
						auto tmp_term_id = document_key(tmp_unprefixed_term_id, spc_id.get_ctype());
						auto tmp_shard_num = id_shard(tmp_term_id, n_shards);
						if (db_handler.endpoints[tmp_shard_num].is_active()) {
							if (shard_num == tmp_shard_num || !db_handler.endpoints[shard_num].is_active()) {
								document_id = std::move(tmp_document_id);
								unprefixed_term_id = std::move(tmp_unprefixed_term_id);
								term_id = std::move(tmp_term_id);
								break;
							}
						}
					}
					if (unprefixed_term_id.empty()) {
						// Same shard-placement fallback as the uuid branch: never leave the
						// id empty when the loop cannot find a candidate on an active target
						// shard (shards can be momentarily inactive while being created).
						document_id = Base64::rfc4648url_unpadded().encode(mint(true));
						unprefixed_term_id = Serialize::serialize(spc_id, document_id);
						term_id = document_key(unprefixed_term_id, spc_id.get_ctype());
					}
					break;
				}
				default:
					THROW(ClientError, "Invalid datatype for '{}'", ID_FIELD_NAME);
			}
		} else {
			// Get early term ID when possible
			switch (id_type) {
				case FieldType::empty: {
					const auto type_ser = Serialize::guess_id(document_id);
					id_type = type_ser.first;
					if (id_type == FieldType::text) {
						id_type = FieldType::keyword;
					}
					spc_id.set_type(id_type);
					spc_id.flags.bool_term = true;
					set_data_id(spc_id);
					properties = &get_mutable_properties();
					unprefixed_term_id = type_ser.second;
					document_id = Serialize::exact(ID_FIELD_NAME, id_type, document_id);
					break;
				}
				case FieldType::uuid:
				case FieldType::integer:
				case FieldType::positive:
				case FieldType::floating:
				case FieldType::text:
				case FieldType::keyword:
					document_id = Serialize::exact(ID_FIELD_NAME, id_type, document_id);
					unprefixed_term_id = Serialize::id(spc_id, document_id);
					break;
				default:
					THROW(ClientError, "Invalid datatype for '{}'", ID_FIELD_NAME);
			}
			term_id = document_key(unprefixed_term_id, spc_id.get_ctype());
		}

#ifdef XAPIAND_LUA
		std::unique_ptr<MsgPack> mut_object;
		if (specification.script) {
			mut_object = co_await db_handler.call_script(object, term_id, *specification.script, data);
			if (mut_object != nullptr) {
				if (!mut_object->is_map()) {
					THROW(ClientError, "Script must return an object, it returned {}", enum_name(mut_object->get_type()));
				}
				// Rebuild fields with new values.
				fields.clear();
				id_field = nullptr;
				version_field = nullptr;
				const auto it_e = mut_object->end();
				for (auto it = mut_object->begin(); it != it_e; ++it) {
					auto str_key = it->str_view();
					if (is_reserved(str_key)) {
						auto key = hh(str_key);
						if (!has_dispatch_process_properties(key)) {
							if (!has_dispatch_process_concrete_properties(key)) {
								fields.emplace_back(str_key, &it.value());
								if (key == hh(ID_FIELD_NAME)) {
									id_field = &fields.back();
								} else if (key == hh(VERSION_FIELD_NAME)) {
									version_field = &fields.back();
								}
							}
						}
					} else {
						fields.emplace_back(str_key, &it.value());
					}
				}
			}
		}
#endif

		// Add ID field.
		MsgPack id_field_obj;
		if (id_field != nullptr && id_field->second != nullptr) {
			if (id_field->second->is_map()) {
				id_field_obj = *id_field->second;
				id_field_obj[RESERVED_VALUE] = document_id;
				id_field->second = &id_field_obj;
			} else {
				id_field->second = &document_id;
			}
		} else {
			fields.emplace_back(ID_FIELD_NAME, &document_id);
			id_field = &fields.back();
		}

		MsgPack version_field_obj = MsgPack::NIL();
		if (version_field != nullptr && version_field->second != nullptr) {
			if (version_field->second->is_map()) {
				version_field_obj = *version_field->second;
				version_field_obj[RESERVED_VALUE] = MsgPack::NIL();
				version_field->second = &version_field_obj;
			}
		} else {
			fields.emplace_back(VERSION_FIELD_NAME, &version_field_obj);
			version_field = &fields.back();
		}

		engine::Document doc;
		MsgPack data_obj;

		auto data_ptr = &data_obj;
		index_fields(properties, doc, data_ptr, fields);

		for (const auto& elem : map_values) {
			const auto val_ser = StringList::serialize(elem.second.begin(), elem.second.end());
			doc.add_value(elem.first, val_ser);
			L_INDEX("Slot: {}  Values: {}", elem.first, repr(val_ser));
		}

		if (term_id != NEW_NUMERIC_KEY) {
			doc.set_key(term_id);  // a new numeric id's key is set when the shard issues it
		}

		// L_INDEX("Indexed schema: " + DIM_GREY + "{}", get_newest_properties().to_string());

		co_return std::make_tuple(std::move(term_id), std::move(doc), std::move(data_obj));
	} catch (...) {
		L_DEBUG("ERROR IN {}: {}", document_id.to_string(), object.to_string());
		mut_schema.reset();
		throw;
	}
}


const MsgPack&
Schema::index_subproperties(const MsgPack*& properties, MsgPack*& data, std::string_view name, const MsgPack* object, Fields* fields)
{
	L_CALL("Schema::index_subproperties({}, {}, {}, {}, {})", properties->to_string(), data->to_string(), repr(name), object ? repr(object->to_string()) : "null", fields ? "<fields>" : "null");

	Split<std::string_view> field_names(name, DB_OFFSPRING_UNION);

	auto it = field_names.begin();
	assert(it != field_names.end());

	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			detect_dynamic(field_name);
			update_prefixes();
			if (specification.flags.store) {
				auto inserted = data->insert(specification.flags.uuid_field ? normalize_uuid(field_name) : field_name);
				data = &inserted.first.value();
			}
		}
		const auto& field_name = *it;
		if (object) {
			assert(fields);
			dispatch_process_properties(*object, *fields);
		}
		detect_dynamic(field_name);
		update_prefixes();
		specification.flags.inside_namespace = true;
		if (specification.flags.store) {
			auto inserted = data->insert(specification.flags.uuid_field ? normalize_uuid(field_name) : field_name);
			if (!inserted.second) {
				THROW(ClientError, "Field {} in {} is duplicated", repr_field(name, inserted.first->as_str()), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			data = &inserted.first.value();
		}
	} else {
		for (; !it.last(); ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && has_dispatch_set_default_spc(hh(field_name)))) {
				THROW(ClientError, "Field {} in {} is not valid", repr_field(name, field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			restart_specification();
			if (feed_subproperties(properties, field_name)) {
				update_prefixes();
				if (specification.flags.store) {
					auto inserted = data->insert(field_name);
					data = &inserted.first.value();
				}
			} else {
				detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					if (feed_subproperties(properties, specification.meta_name)) {
						update_prefixes();
						if (specification.flags.store) {
							auto inserted = data->insert(normalize_uuid(field_name));
							data = &inserted.first.value();
						}
						continue;
					}
				}

				auto mut_properties = &get_mutable_properties(specification.full_meta_name);
				add_field(mut_properties);
				if (specification.flags.store) {
					auto inserted = data->insert(specification.flags.uuid_field ? normalize_uuid(field_name) : field_name);
					data = &inserted.first.value();
				}

				for (++it; !it.last(); ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name)) {
						THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
						if (specification.flags.store) {
							auto inserted = data->insert(specification.flags.uuid_field ? normalize_uuid(n_field_name) : n_field_name);
							data = &inserted.first.value();
						}
					}
				}
				const auto& n_field_name = *it;
				if (!is_valid(n_field_name)) {
					THROW(ClientError, "Field {} in {} is not valid", repr_field(name, n_field_name), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				} else {
					detect_dynamic(n_field_name);
					if (object) {
						assert(fields);
						add_field(mut_properties, *object, *fields);
					} else {
						add_field(mut_properties);
					}
					if (specification.flags.store) {
						auto inserted = data->insert(specification.flags.uuid_field ? normalize_uuid(n_field_name) : n_field_name);
						if (!inserted.second) {
							THROW(ClientError, "Field {} in {} is duplicated", repr_field(name, inserted.first->as_str()), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
						}
						data = &inserted.first.value();
					}
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
			if (object) {
				assert(fields);
				dispatch_process_properties(*object, *fields);
			}
			update_prefixes();
			if (specification.flags.store) {
				auto inserted = data->insert(field_name);
				if (!inserted.second) {
					THROW(ClientError, "Field {} in {} is duplicated", repr_field(name, inserted.first->as_str()), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				data = &inserted.first.value();
			}
		} else {
			detect_dynamic(field_name);
			if (specification.flags.uuid_field) {
				if (feed_subproperties(properties, specification.meta_name)) {
					if (object) {
						assert(fields);
						dispatch_process_properties(*object, *fields);
					}
					update_prefixes();
					if (specification.flags.store) {
						auto inserted = data->insert(normalize_uuid(field_name));
						if (!inserted.second) {
							THROW(ClientError, "Field {} in {} is duplicated", repr_field(name, inserted.first->as_str()), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
						}
						data = &inserted.first.value();
					}
					return *properties;
				}
			}

			auto mut_properties = &get_mutable_properties(specification.full_meta_name);
			if (object) {
				assert(fields);
				add_field(mut_properties, *object, *fields);
			} else {
				add_field(mut_properties);
			}
			if (specification.flags.store) {
				auto inserted = data->insert(specification.flags.uuid_field ? normalize_uuid(field_name) : field_name);
				if (!inserted.second) {
					THROW(ClientError, "Field {} in {} is duplicated", repr_field(name, inserted.first->as_str()), specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				data = &inserted.first.value();
			}
			return *mut_properties;
		}
	}

	return *properties;
}


void
Schema::index_new_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, engine::Document& doc, const std::string& name)
{
	L_CALL("Schema::index_new_object({}, {}, {}, <engine::Document>, {})", parent_properties->to_string(), object.to_string(), repr(parent_data->to_string()), repr(name));

	if (is_comment(name)) {
		return;  // skip comments (empty fields or fields starting with '#')
	}

	if (is_valid(name) && (!specification.flags.recurse || specification.ignored.find(name) != specification.ignored.end())) {
		if (specification.flags.store) {
			parent_data->get(name) = object;
		}
		return;
	}

	auto spc_start = specification;

	switch (object.get_type()) {
		case MsgPack::Type::MAP: {
			auto properties = &*parent_properties;
			auto data = parent_data;
			Fields fields;
			properties = &index_subproperties(properties, data, name, &object, &fields);
			index_fields(properties, doc, data, fields);
			auto value = specification.value ? specification.value.get() : specification.value_rec.get();
			if (value) {
				// Save a copy of the value and reset (this could be recursive)
				auto value_object = std::move(*value);
				specification.value.reset();
				specification.value_rec.reset();
				index_object(properties, value_object, data, doc, name);
			} else {
				if (specification.flags.store) {
					if (data->is_undefined()) {
						parent_data->erase(name);
					}
				}
			}
			if (specification.flags.inside_namespace && !spc_start.flags.concrete) {
				// Bubble the namespaced type up
				spc_start.sep_types[SPC_CONCRETE_TYPE] = specification.sep_types[SPC_CONCRETE_TYPE];
			}
			specification = std::move(spc_start);
			break;
		}

		default: {
			auto properties = &*parent_properties;
			auto data = parent_data;
			properties = &index_subproperties(properties, data, name);
			index_object(properties, object, data, doc, name);
			if (specification.flags.inside_namespace && !spc_start.flags.concrete) {
				// Bubble the namespaced type up
				spc_start.sep_types[SPC_CONCRETE_TYPE] = specification.sep_types[SPC_CONCRETE_TYPE];
			}
			specification = std::move(spc_start);
			break;
		}
	}
}


void
Schema::index_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, engine::Document& doc, const std::string& name)
{
	L_CALL("Schema::index_object({}, {}, <MsgPack*>, <engine::Document>, {})", parent_properties->to_string(), object.to_string(), repr(name));

	switch (object.get_type()) {
		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED: {
			auto data = parent_data;
			if (!specification.flags.concrete) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
					if (specification.flags.inside_namespace) {
						validate_required_namespace_data();
					} else {
						validate_required_data(get_mutable_properties(specification.full_meta_name));
					}
				}
			}
			index_partial_paths(doc);
			if (specification.flags.store) {
				*data = object;
				if (data->is_map() && data->size() == 1) {
					auto it = data->find(RESERVED_VALUE);
					if (it != data->end()) {
						*data = it.value();
					}
				}
				if (data->is_undefined() || (data->is_map() && data->empty())) {
					parent_data->erase(name);
				}
			}
			break;
		}

		case MsgPack::Type::MAP: {
			auto data = parent_data;
			auto concrete_type = guess_concrete_type(object);
			if (concrete_type == FieldType::empty || concrete_type == FieldType::object) {
				index_inner_object(parent_properties, doc, data, object);
			} else {
				index_item_value(doc, data, object);
				if (specification.flags.store) {
					if (data->is_map() && data->size() == 1) {
						auto it = data->find(RESERVED_VALUE);
						if (it != data->end()) {
							*data = it.value();
						}
					}
					if (data->is_undefined() || (data->is_map() && data->empty())) {
						parent_data->erase(name);
					}
				}
			}
			break;
		}

		case MsgPack::Type::ARRAY: {
			auto data = parent_data;
			auto concrete_type = guess_concrete_type(object);
			if (concrete_type == FieldType::empty || specification.sep_types[SPC_ARRAY_TYPE] == FieldType::array || specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::object) {
				index_array(parent_properties, object, data, doc, name);
			} else {
				index_item_value(doc, data, object);
				if (specification.flags.store) {
					if (data->is_map() && data->size() == 1) {
						auto it = data->find(RESERVED_VALUE);
						if (it != data->end()) {
							*data = it.value();
						}
					}
					if (data->is_undefined() || (data->is_map() && data->empty())) {
						parent_data->erase(name);
					}
				}
			}
			break;
		}

		default: {
			auto data = parent_data;
			index_item_value(doc, data, object);
			if (specification.flags.store) {
				if (data->is_map() && data->size() == 1) {
					auto it = data->find(RESERVED_VALUE);
					if (it != data->end()) {
						*data = it.value();
					}
				}
				if (data->is_undefined() || (data->is_map() && data->empty())) {
					parent_data->erase(name);
				}
			}
			break;
		}
	}
}


void
Schema::index_array(const MsgPack*& parent_properties, const MsgPack& array, MsgPack*& parent_data, engine::Document& doc, const std::string& name)
{
	L_CALL("Schema::index_array({}, {}, <MsgPack*>, <engine::Document>, {})", parent_properties->to_string(), array.to_string(), repr(name));

	set_type_to_array();  // this has to be done first

	if (array.empty()) {
		if (specification.flags.store) {
			*parent_data = MsgPack::ARRAY();
		}
		return;
	}

	size_t pos = 0;
	for (const auto& object : array) {
		switch (object.get_type()) {
			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED: {
				auto data_array = parent_data;
				auto data = specification.flags.store ? &data_array->get(pos) : data_array;
				if (!specification.flags.concrete) {
					if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty) {
						if (specification.flags.inside_namespace) {
							validate_required_namespace_data();
						} else {
							validate_required_data(get_mutable_properties(specification.full_meta_name));
						}
					}
				}
				index_partial_paths(doc);
				if (specification.flags.store) {
					*data = object;
					if (data->is_map() && data->size() == 1) {
						auto it = data->find(RESERVED_VALUE);
						if (it != data->end()) {
							*data = it.value();
						}
					}
				}
				break;
			}

			case MsgPack::Type::MAP: {
				auto data_array = parent_data;
				auto data = specification.flags.store ? &data_array->get(pos) : data_array;
				auto concrete_type = guess_concrete_type(object);
				if (concrete_type == FieldType::empty || concrete_type == FieldType::object) {
					index_inner_object(parent_properties, doc, data, object);
				} else {
					index_item_value(doc, data, object, pos);
					if (specification.flags.store) {
						if (data->is_map() && data->size() == 1) {
							auto it = data->find(RESERVED_VALUE);
							if (it != data->end()) {
								*data = it.value();
							}
						}
					}
				}
				break;
			}

			case MsgPack::Type::ARRAY: {
				auto data_array = parent_data;
				auto data = specification.flags.store ? &data_array->get(pos) : data_array;
				auto concrete_type = guess_concrete_type(object);
				if (concrete_type == FieldType::empty || specification.sep_types[SPC_ARRAY_TYPE] == FieldType::array || specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::object) {
					index_array(parent_properties, object, data, doc, name);
				} else {
					index_item_value(doc, data, object, pos);
					if (specification.flags.store) {
						if (data->is_map() && data->size() == 1) {
							auto it = data->find(RESERVED_VALUE);
							if (it != data->end()) {
								*data = it.value();
							}
						}
					}
				}
				break;
			}

			default: {
				auto data_array = parent_data;
				auto data = specification.flags.store ? &data_array->get(pos) : data_array;
				index_item_value(doc, data, object, pos);
				if (specification.flags.store) {
					if (data->is_map() && data->size() == 1) {
						auto it = data->find(RESERVED_VALUE);
						if (it != data->end()) {
							*data = it.value();
						}
					}
				}
				break;
			}
		}
		++pos;
	}
}


void
Schema::index_fields(const MsgPack*& properties, engine::Document& doc, MsgPack*& data, const Fields& fields)
{
	L_CALL("Schema::index_fields({}, <doc>, {}, <Fields>)", properties->to_string(), data->to_string());

	for (const auto& field : fields) {
		index_new_object(properties, *field.second, data, doc, field.first);
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
	} else {
		index_partial_paths(doc);
		if (specification.flags.store && specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::object) {
			*data = MsgPack::MAP();
		}

	}
}


void
Schema::index_inner_object(const MsgPack*& properties, engine::Document& doc, MsgPack*& data, const MsgPack& object)
{
	L_CALL("Schema::index_inner_object({}, <doc>, {}, <object>)", properties->to_string(), data->to_string());

	for (auto& key : object) {
		index_new_object(properties, object.at(key), data, doc, key.str());
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
	} else {
		if (specification.flags.store) {
			*data = MsgPack::MAP();
		}
	}

}
