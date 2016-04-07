/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include "database_utils.h"
#include "msgpack.h"
#include "stl_serialise.h"
#include "utils.h"

#include <future>


enum class unitTime {
	SECOND,
	MINUTE,
	HOUR,
	DAY,
	MONTH,
	YEAR,
};


enum class Index {
	ALL,
	TERM,
	VALUE,
	TEXT
};


struct specification_t {
	// Reserved values.
	std::vector<unsigned> position;
	std::vector<unsigned> weight;
	std::vector<std::string> language;
	std::vector<bool> spelling;
	std::vector<bool> positions;
	std::vector<unsigned> analyzer;
	std::vector<unsigned> sep_types;
	std::vector<double> accuracy;
	std::vector<std::string> acc_prefix;
	std::string prefix;
	unsigned slot;
	Index index;
	bool store;
	bool dynamic;
	bool date_detection;
	bool numeric_detection;
	bool geo_detection;
	bool bool_detection;
	bool string_detection;
	bool bool_term;
	std::unique_ptr<const MsgPack> value;
	std::string name;

	// Auxiliar variables.
	bool found_field;
	bool set_type;
	bool set_bool_term;
	bool fixed_index;
	std::unique_ptr<const MsgPack> doc_acc;
	std::string full_name;

	specification_t();
	specification_t(const specification_t& o);

	specification_t& operator=(const specification_t& o);

	std::string to_string() const;
};


extern const specification_t default_spc;


class Database;
class Schema;


using dispatch_reserved = void (Schema::*)(MsgPack&, const MsgPack&, specification_t&);
using dispatch_root     = void (Schema::*)(MsgPack&, const MsgPack, specification_t&, Xapian::Document&);
using dispatch_index    = void (Schema::*)(MsgPack&, const MsgPack&, const specification_t&, Xapian::Document&);
using dispatch_property = void (*)(MsgPack&&, specification_t&);
using dispatch_readable = void (*)(MsgPack&&, const MsgPack&);


extern const std::unordered_map<std::string, dispatch_reserved> map_dispatch_reserved;
extern const std::unordered_map<std::string, dispatch_root> map_dispatch_root;
extern const std::unordered_map<std::string, dispatch_property> map_dispatch_properties;
extern const std::unordered_map<std::string, dispatch_readable> map_dispatch_readable;


using TaskVector = std::vector<std::future<void>>;

class Schema {
	Database* database;

	MsgPack schema;
	std::atomic_bool exist;
	std::atomic_bool to_store;

	/*
	 * specification is updated with the properties.
	 */
	static void update_specification(const MsgPack& properties, specification_t& specification);

	/*
	 * Sets type to array in properties.
	 */
	void set_type_to_array(MsgPack& properties);

	/*
	 * Set type to object in properties.
	 */
	void set_type_to_object(MsgPack& properties);

	/*
	 * Sets in specification the item_doc's type
	 */
	static void set_type(const MsgPack& item_doc, specification_t& specification);

	/*
	 * Recursively transforms item_schema into a readable form.
	 */
	static void readable(MsgPack&& item_schema, bool is_root=false);

	/*
	 * Validates data when RESERVED_TYPE has not been save in schema.
	 * Insert into properties all required data.
	 */
	void validate_required_data(MsgPack& properties, const MsgPack* value, specification_t& specification);


public:
	std::unordered_map<Xapian::valueno, StringSet> map_values;

	Schema() = default;

	Schema(Schema&& schema) = delete;
	Schema(const Schema& schema) = delete;

	Schema& operator=(Schema&& schema) = delete;
	Schema& operator=(const Schema& schema) = delete;

	~Schema() = default;

	void set_database(Database* _database);

	inline std::string to_string() const {
		return schema.to_string();
	}

	inline bool get_store() const {
		return to_store;
	}

	template<typename... Args>
	inline void set_schema(Args&&... args) {
		schema = MsgPack(std::forward<Args>(args)...);
	}

	inline void set_store(bool _to_store) {
		to_store = _to_store;
	}

	/*
	 * specification is updated with the properties of schema.
	 * Returns the properties of schema.
	 */
	inline MsgPack getPropertiesSchema() {
		map_values.clear();
		return schema.at(RESERVED_SCHEMA);
	}

	/*
	 * Returns serialise value of value_id.
	 */
	std::string serialise_id(MsgPack& schema_properties, specification_t& specification, const std::string& value_id);

	/*
	 * Restarting reserved words than are not inherited.
	 */
	void restart_specification(specification_t& specification);

	/*
	 * Gets the properties of item_key and specification is updated.
	 * Returns the properties of schema.
	 */
	MsgPack get_subproperties(MsgPack& properties, const std::string& item_key, specification_t& specification);

	/*
	 * Sets properties and update specification with the properties in item_doc.
	 */
	void set_properties(MsgPack& properties, const MsgPack& item_doc, specification_t& specification);


	/*
	 * Stores schema only if needed.
	 */
	void store();

	/*
	 * Transforms schema into json string.
	 */
	std::string to_json_string(bool prettify=false);


	/*
	 * Tranforms reserved words into a readable form.
	 */

	static void readable_type(MsgPack&& prop_type, const MsgPack& properties);
	static void readable_analyzer(MsgPack&& prop_analyzer, const MsgPack& properties);
	static void readable_index(MsgPack&& prop_index, const MsgPack& properties);


	/*
	 * Functions for indexing elements in a Xapian::document.
	 */

	void process_weight(MsgPack& properties, const MsgPack& doc_weight, specification_t& specification);
	void process_position(MsgPack& properties, const MsgPack& doc_position, specification_t& specification);
	void process_language(MsgPack& properties, const MsgPack& doc_language, specification_t& specification);
	void process_spelling(MsgPack& properties, const MsgPack& doc_spelling, specification_t& specification);
	void process_positions(MsgPack& properties, const MsgPack& doc_positions, specification_t& specification);
	void process_analyzer(MsgPack& properties, const MsgPack& doc_analyzer, specification_t& specification);
	void process_type(MsgPack& properties, const MsgPack& doc_type, specification_t& specification);
	void process_accuracy(MsgPack& properties, const MsgPack& doc_accuracy, specification_t& specification);
	void process_acc_prefix(MsgPack& properties, const MsgPack& doc_acc_prefix, specification_t& specification);
	void process_prefix(MsgPack& properties, const MsgPack& doc_prefix, specification_t& specification);
	void process_slot(MsgPack& properties, const MsgPack& doc_slot, specification_t& specification);
	void process_index(MsgPack& properties, const MsgPack& doc_index, specification_t& specification);
	void process_store(MsgPack& properties, const MsgPack& doc_store, specification_t& specification);
	void process_dynamic(MsgPack& properties, const MsgPack& doc_dynamic, specification_t& specification);
	void process_d_detection(MsgPack& properties, const MsgPack& doc_d_detection, specification_t& specification);
	void process_n_detection(MsgPack& properties, const MsgPack& doc_n_detection, specification_t& specification);
	void process_g_detection(MsgPack& properties, const MsgPack& doc_g_detection, specification_t& specification);
	void process_b_detection(MsgPack& properties, const MsgPack& doc_b_detection, specification_t& specification);
	void process_s_detection(MsgPack& properties, const MsgPack& doc_s_detection, specification_t& specification);
	void process_bool_term(MsgPack& properties, const MsgPack& doc_bool_term, specification_t& specification);
	void process_value(MsgPack& properties, const MsgPack& doc_value, specification_t& specification);
	void process_name(MsgPack& properties, const MsgPack& doc_name, specification_t& specification);


	/*
	 * Functions for reserved words that are only in json's root.
	 */

	inline void process_values(MsgPack& properties, const MsgPack doc_values, specification_t& specification, Xapian::Document& doc);
	inline void process_texts(MsgPack& properties, const MsgPack doc_texts, specification_t& specification, Xapian::Document& doc);
	inline void process_terms(MsgPack& properties, const MsgPack doc_terms, specification_t& specification, Xapian::Document& doc);


	/*
	 * Functions for adding fields to index in FieldMap.
	 */

	inline void fixed_index(MsgPack& properties, const MsgPack& object, specification_t& specifications, Xapian::Document& doc);
	void index_object(MsgPack& global_properties, const MsgPack object, specification_t& specification, Xapian::Document& doc, const std::string name=std::string());
	void index_array(MsgPack& properties, const MsgPack& array, specification_t& specification, Xapian::Document& doc);
	inline void index_item(MsgPack& properties, const MsgPack& value, specification_t& specification, Xapian::Document& doc);
	void index_texts(MsgPack& properties, const MsgPack& texts, const specification_t& specification, Xapian::Document& doc);
	void index_text(const specification_t& specification, Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;
	void index_terms(MsgPack& properties, const MsgPack& terms, const specification_t& specification, Xapian::Document& doc);
	void index_term(const specification_t& specification, Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;
	void index_values(MsgPack& properties, const MsgPack& values, const specification_t& specification, Xapian::Document& doc, bool is_term=false);
	void index_value(const MsgPack& value, const specification_t& specification, Xapian::Document& doc, StringSet& s, size_t& pos, bool is_term) const;


	/*
	 * Functions for updating specification using the properties in schema.
	 */

	static inline void process_weight(MsgPack&& prop_weight, specification_t& specification) {
		specification.weight.clear();
		for (const auto _weight : prop_weight) {
			specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
		}
	}

	static inline void process_position(MsgPack&& prop_position, specification_t& specification) {
		specification.position.clear();
		for (const auto _position : prop_position) {
			specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
		}
	}

	static inline void process_language(MsgPack&& prop_language, specification_t& specification) {
		specification.language.clear();
		for (const auto _language : prop_language) {
			specification.language.push_back(_language.get_str());
		}
	}

	static inline void process_spelling(MsgPack&& prop_spelling, specification_t& specification) {
		specification.spelling.clear();
		for (const auto _spelling : prop_spelling) {
			specification.spelling.push_back(_spelling.get_bool());
		}
	}

	static inline void process_positions(MsgPack&& prop_positions, specification_t& specification) {
		specification.positions.clear();
		for (const auto _positions : prop_positions) {
			specification.positions.push_back(_positions.get_bool());
		}
	}

	static inline void process_analyzer(MsgPack&& prop_analyzer, specification_t& specification) {
		specification.analyzer.clear();
		for (const auto _analyzer : prop_analyzer) {
			specification.analyzer.push_back(static_cast<unsigned>(_analyzer.get_u64()));
		}
	}

	static inline void process_type(MsgPack&& prop_type, specification_t& specification) {
		specification.sep_types[0] = static_cast<unsigned>(prop_type.at(0).get_u64());
		specification.sep_types[1] = static_cast<unsigned>(prop_type.at(1).get_u64());
		specification.sep_types[2] = static_cast<unsigned>(prop_type.at(2).get_u64());
		specification.set_type = true;
	}

	static inline void process_accuracy(MsgPack&& prop_accuracy, specification_t& specification) {
		for (const auto acc : prop_accuracy) {
			specification.accuracy.push_back(acc.get_f64());
		}
	}

	static inline void process_acc_prefix(MsgPack&& prop_acc_prefix, specification_t& specification) {
		for (const auto acc_p : prop_acc_prefix) {
			specification.acc_prefix.push_back(acc_p.get_str());
		}
	}

	static inline void process_prefix(MsgPack&& prop_prefix, specification_t& specification) {
		specification.prefix = prop_prefix.get_str();
	}

	static inline void process_slot(MsgPack&& prop_slot, specification_t& specification) {
		specification.slot = static_cast<unsigned>(prop_slot.get_u64());
	}

	static inline void process_index(MsgPack&& prop_index, specification_t& specification) {
		specification.index = (Index)prop_index.get_u64();
	}

	static inline void process_store(MsgPack&& prop_store, specification_t& specification) {
		specification.store = prop_store.get_bool();
	}

	static inline void process_dynamic(MsgPack&& prop_dynamic, specification_t& specification) {
		specification.dynamic = prop_dynamic.get_bool();
	}

	static inline void process_d_detection(MsgPack&& prop_d_detection, specification_t& specification) {
		specification.date_detection = prop_d_detection.get_bool();
	}

	static inline void process_n_detection(MsgPack&& prop_n_detection, specification_t& specification) {
		specification.numeric_detection = prop_n_detection.get_bool();
	}

	static inline void process_g_detection(MsgPack&& prop_g_detection, specification_t& specification) {
		specification.geo_detection = prop_g_detection.get_bool();
	}

	static inline void process_b_detection(MsgPack&& prop_b_detection, specification_t& specification) {
		specification.bool_detection = prop_b_detection.get_bool();
	}

	static inline void process_s_detection(MsgPack&& prop_s_detection, specification_t& specification) {
		specification.string_detection = prop_s_detection.get_bool();
	}

	static inline void process_bool_term(MsgPack&& prop_bool_term, specification_t& specification) {
		specification.bool_term = prop_bool_term.get_bool();
	}
};
