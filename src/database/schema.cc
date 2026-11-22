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
