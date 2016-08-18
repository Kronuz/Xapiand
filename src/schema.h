/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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


enum class unitTime : uint8_t {
	SECOND,
	MINUTE,
	HOUR,
	DAY,
	MONTH,
	YEAR,
	DECADE,
	CENTURY,
	MILLENNIUM
};


enum class typeIndex : uint8_t {
	NONE,           // Not index
	TERMS,          // Index the field value like FIELD_TERMS and GLOBAL_TERMS.
	VALUES,         // Index the field value like FIELD_VALUES and GLOBAL_VALUES.
	ALL,            // Index the field value like FIELD_ALL and GLOBAL_ALL.

	FIELD_TERMS,    // Index the field value like terms with prefix.
	FIELD_VALUES,   // Index the field value like values with prefix.
	FIELD_ALL,      // Index the field value like FIELD_TERMS and FIELD_VALUES.

	GLOBAL_TERMS,   // Index the field value like terms without prefix.
	GLOBAL_VALUES,  // Index the field value like values without prefix.
	GLOBAL_ALL      // Index the field value like GLOBAL_TERMS and GLOBAL_VALUES.
};


MSGPACK_ADD_ENUM(unitTime);
MSGPACK_ADD_ENUM(typeIndex);
MSGPACK_ADD_ENUM(Xapian::TermGenerator::stem_strategy);


struct specification_t {
	// Reserved values.
	std::vector<Xapian::termpos> position;
	std::vector<Xapian::termcount> weight;
	std::vector<std::string> language;
	std::vector<bool> spelling;
	std::vector<bool> positions;
	std::vector<Xapian::TermGenerator::stem_strategy> analyzer;
	std::vector<unsigned> sep_types;
	std::string prefix;
	Xapian::valueno slot;
	typeIndex index;

	std::vector<uint64_t> accuracy;
	std::vector<std::string> acc_prefix;
	std::vector<std::string> acc_gprefix;

	bool store;
	bool dynamic;
	bool date_detection;
	bool numeric_detection;
	bool geo_detection;
	bool bool_detection;
	bool string_detection;
	bool bool_term;

	std::unique_ptr<const MsgPack> value;
	std::unique_ptr<MsgPack> value_rec; // Value recovered from the item.
	std::unique_ptr<const MsgPack> doc_acc;
	std::string name;
	std::string full_name;

	// Auxiliar variables.
	bool found_field;
	bool set_type;
	bool set_bool_term;
	bool fixed_index;

	// Variables for geospatial types.
	bool partials;
	double error;


	specification_t();
	specification_t(const specification_t& o);

	specification_t& operator=(const specification_t& o);

	std::string to_string() const;
};


extern const specification_t default_spc;


using TaskVector = std::vector<std::future<void>>;


class Schema;


using index_term = void (Schema::*)(Xapian::Document&, std::string&&, size_t) const;



class Schema {
	std::shared_ptr<const MsgPack> schema;
	std::unique_ptr<MsgPack> mut_schema;

	std::unordered_map<Xapian::valueno, StringSet> map_values;
	specification_t specification;

	MsgPack& get_mutable(const std::string& full_name);

	/*
	 * specification is updated with the properties.
	 */
	void update_specification(const MsgPack& properties);

	/*
	 * Restarting reserved words than are not inherited.
	 */
	void restart_specification();

	/*
	 * Gets the properties of item_key and specification is updated.
	 * Returns the properties of schema.
	 */
	const MsgPack& get_subproperties(const MsgPack& properties);

	/*
	 * Sets type to array in properties.
	 */
	void set_type_to_array();

	/*
	 * Set type to object in properties.
	 */
	void set_type_to_object();

	/*
	 * Sets in specification the item_doc's type
	 */
	void set_type(const MsgPack& item_doc);

	/*
	 * Recursively transforms item_schema into a readable form.
	 */
	static void readable(MsgPack& item_schema, bool is_root=false);


	/*
	 * Auxiliar functions for index fields in doc.
	 */

	inline void fixed_index(const MsgPack& properties, const MsgPack& object, Xapian::Document& doc);
	void index_object(const MsgPack& parent_properties, const MsgPack& object, Xapian::Document& doc, const std::string& name=std::string());
	void index_array(const MsgPack& properties, const MsgPack& array, Xapian::Document& doc);
	void index_item(Xapian::Document& doc, const MsgPack& value, size_t pos);
	void index_item(Xapian::Document& doc, const MsgPack& values);

	void index_field_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;
	void index_global_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;
	void index_all_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const;

	void index_value(Xapian::Document& doc, const MsgPack& value, StringSet& s, const std::vector<std::string>& acc_prefix, size_t pos, index_term fun=nullptr) const;
	void index_all_value(Xapian::Document& doc, const MsgPack& value, StringSet& s_f, StringSet& s_g, size_t pos, bool is_term=false) const;

	/*
	 * Validates data when RESERVED_TYPE has not been save in schema.
	 * Insert into properties all required data.
	 */
	void validate_required_data(const MsgPack* value);

public:
	Schema(const std::shared_ptr<const MsgPack>& schema);

	Schema() = delete;
	Schema(Schema&& schema) = delete;
	Schema(const Schema& schema) = delete;
	Schema& operator=(Schema&& schema) = delete;
	Schema& operator=(const Schema& schema) = delete;

	~Schema() = default;

	auto get_modified_schema() {
		if (mut_schema) {
			auto schema = std::shared_ptr<const MsgPack>(mut_schema.release());
			schema->lock();
			return schema;
		} else {
			return std::shared_ptr<const MsgPack>();
		}
	}

	/*
	 * specification is updated with the properties of schema.
	 * Returns the properties of schema.
	 */
	const MsgPack& getProperties() {
		return schema->at(RESERVED_SCHEMA);
	}

	/*
	 * Returns serialise value of value_id.
	 */
	std::string serialise_id(const MsgPack& schema_properties, const std::string& value_id);

	/*
	 * Transforms schema into json string.
	 */
	std::string to_string(bool prettify=false) const;

	/*
	 * Returns readable schema.
	 */
	const MsgPack get_readable() const;

	/*
	 * Function to index object in doc.
	 */
	void index(const MsgPack& properties, const MsgPack& object, Xapian::Document& doc);


	/*
	 * Tranforms reserved words into a readable form.
	 */

	static void readable_type(MsgPack& prop_type, MsgPack& properties);
	static void readable_analyzer(MsgPack& prop_analyzer, MsgPack& properties);
	static void readable_index(MsgPack& prop_index, MsgPack& properties);


	/*
	 * Functions for reserved words that are in the document.
	 */

	void process_weight(const MsgPack& doc_weight);
	void process_position(const MsgPack& doc_position);
	void process_language(const MsgPack& doc_language);
	void process_spelling(const MsgPack& doc_spelling);
	void process_positions(const MsgPack& doc_positions);
	void process_analyzer(const MsgPack& doc_analyzer);
	void process_type(const MsgPack& doc_type);
	void process_accuracy(const MsgPack& doc_accuracy);
	void process_acc_prefix(const MsgPack& doc_acc_prefix);
	void process_acc_gprefix(const MsgPack& doc_acc_gprefix);
	void process_prefix(const MsgPack& doc_prefix);
	void process_slot(const MsgPack& doc_slot);
	void process_index(const MsgPack& doc_index);
	void process_store(const MsgPack& doc_store);
	void process_dynamic(const MsgPack& doc_dynamic);
	void process_d_detection(const MsgPack& doc_d_detection);
	void process_n_detection(const MsgPack& doc_n_detection);
	void process_g_detection(const MsgPack& doc_g_detection);
	void process_b_detection(const MsgPack& doc_b_detection);
	void process_s_detection(const MsgPack& doc_s_detection);
	void process_bool_term(const MsgPack& doc_bool_term);
	void process_partials(const MsgPack& doc_partials);
	void process_error(const MsgPack& doc_error);
	void process_latitude(const MsgPack& doc_latitude);
	void process_longitude(const MsgPack& doc_longitude);
	void process_radius(const MsgPack& doc_radius);
	void process_value(const MsgPack& doc_value);
	void process_name(const MsgPack& doc_name);


	/*
	 * Functions for reserved words that are only in document's root.
	 */

	inline void process_values(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc);
	inline void process_field_values(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc);
	inline void process_global_values(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc);
	inline void process_terms(const MsgPack& properties, const MsgPack& doc_terms, Xapian::Document& doc);
	inline void process_field_terms(const MsgPack& properties, const MsgPack& doc_terms, Xapian::Document& doc);
	inline void process_global_terms(const MsgPack& properties, const MsgPack& doc_terms, Xapian::Document& doc);
	inline void process_field_all(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc);
	inline void process_global_all(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc);
	inline void process_none(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc);


	/*
	 * Functions used for searching, return a field properties.
	 */

	data_field_t get_data_field(const std::string& field_name) const;
	data_field_t get_slot_field(const std::string& field_name) const;


	/*
	 * Functions for updating specification using the properties in schema.
	 */

	void update_position(const MsgPack& prop_position) {
		specification.position.clear();
		for (const auto& _position : prop_position) {
			specification.position.push_back(static_cast<Xapian::termpos>(_position.as_u64()));
		}
	}

	void update_weight(const MsgPack& prop_weight) {
		specification.weight.clear();
		for (const auto& _weight : prop_weight) {
			specification.weight.push_back(static_cast<Xapian::termpos>(_weight.as_u64()));
		}
	}

	void update_language(const MsgPack& prop_language) {
		specification.language.clear();
		for (const auto& _language : prop_language) {
			specification.language.push_back(lower_string(_language.as_string()));
		}
	}

	void update_spelling(const MsgPack& prop_spelling) {
		specification.spelling.clear();
		for (const auto& _spelling : prop_spelling) {
			specification.spelling.push_back(_spelling.as_bool());
		}
	}

	void update_positions(const MsgPack& prop_positions) {
		specification.positions.clear();
		for (const auto& _positions : prop_positions) {
			specification.positions.push_back(_positions.as_bool());
		}
	}

	void update_analyzer(const MsgPack& prop_analyzer) {
		specification.analyzer.clear();
		for (const auto& _analyzer : prop_analyzer) {
			specification.analyzer.push_back(static_cast<unsigned>(_analyzer.as_u64()));
		}
	}

	void update_type(const MsgPack& prop_type) {
		specification.sep_types[0] = static_cast<unsigned>(prop_type.at(0).as_u64());
		specification.sep_types[1] = static_cast<unsigned>(prop_type.at(1).as_u64());
		specification.sep_types[2] = static_cast<unsigned>(prop_type.at(2).as_u64());
		specification.set_type = true;
	}

	void update_accuracy(const MsgPack& prop_accuracy) {
		for (const auto& acc : prop_accuracy) {
			specification.accuracy.push_back(acc.as_f64());
		}
	}

	void update_acc_prefix(const MsgPack& prop_acc_prefix) {
		for (const auto& acc_p : prop_acc_prefix) {
			specification.acc_prefix.push_back(acc_p.as_string());
		}
	}

	void update_acc_gprefix(const MsgPack& prop_acc_gprefix) {
		for (const auto& acc_gp : prop_acc_gprefix) {
			specification.acc_gprefix.push_back(acc_gp.as_string());
		}
	}

	void update_prefix(const MsgPack& prop_prefix) {
		specification.prefix = prop_prefix.as_string();
	}

	void update_slot(const MsgPack& prop_slot) {
		specification.slot = static_cast<unsigned>(prop_slot.as_u64());
	}

	void update_index(const MsgPack& prop_index) {
		// If not fixed_index update index type.
		if likely(!specification.fixed_index) {
			specification.index = static_cast<typeIndex>(prop_index.as_u64());
		}
	}

	void update_store(const MsgPack& prop_store) {
		specification.store = prop_store.as_bool();
	}

	void update_dynamic(const MsgPack& prop_dynamic) {
		specification.dynamic = prop_dynamic.as_bool();
	}

	void update_d_detection(const MsgPack& prop_d_detection) {
		specification.date_detection = prop_d_detection.as_bool();
	}

	void update_n_detection(const MsgPack& prop_n_detection) {
		specification.numeric_detection = prop_n_detection.as_bool();
	}

	void update_g_detection(const MsgPack& prop_g_detection) {
		specification.geo_detection = prop_g_detection.as_bool();
	}

	void update_b_detection(const MsgPack& prop_b_detection) {
		specification.bool_detection = prop_b_detection.as_bool();
	}

	void update_s_detection(const MsgPack& prop_s_detection) {
		specification.string_detection = prop_s_detection.as_bool();
	}

	void update_bool_term(const MsgPack& prop_bool_term) {
		specification.bool_term = prop_bool_term.as_bool();
	}

	void update_partials(const MsgPack& prop_partials) {
		specification.partials = prop_partials.as_bool();
	}

	void update_error(const MsgPack& prop_error) {
		specification.error = prop_error.as_f64();
	}
};


using dispatch_reserved = void (Schema::*)(const MsgPack&);
using dispatch_root     = void (Schema::*)(const MsgPack&, const MsgPack&, Xapian::Document&);
using dispatch_readable = void (*)(MsgPack&, MsgPack&);


extern const std::unordered_map<std::string, dispatch_reserved> map_dispatch_document;
extern const std::unordered_map<std::string, dispatch_reserved> map_dispatch_properties;
extern const std::unordered_map<std::string, dispatch_root> map_dispatch_root;
extern const std::unordered_map<std::string, dispatch_readable> map_dispatch_readable;
