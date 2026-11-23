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


