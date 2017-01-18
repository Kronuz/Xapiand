/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#include <array>                           // for array
#include <future>                          // for future
#include <memory>                          // for shared_ptr
#include <stddef.h>                        // for size_t
#include <string>                          // for string
#include <sys/types.h>                     // for uint8_t
#include <tuple>                           // for tuple
#include <unordered_map>                   // for unordered_map
#include <utility>                         // for pair
#include <vector>                          // for vector
#include <xapian.h>                        // for QueryParser

#include "database_utils.h"
#include "geo/htm.h"                       // for HTM_MIN_ERROR
#include "msgpack.h"                       // for MsgPack, MS...
#include "stl_serialise.h"                 // for StringSet
#include "utils.h"                         // for repr, toUType, lower_string


#define LIMIT_PARTIAL_PATHS_DEPTH  10    // 2^(n - 2) => 2^8 => 256 namespace terms.


enum class TypeIndex : uint8_t {
	NONE                      = 0,                              // 0000  Bits for  "none"
	FIELD_TERMS               = 0b0001,                         // 0001  Bit for   "field_terms"
	FIELD_VALUES              = 0b0010,                         // 0010  Bit for   "field_values"
	FIELD_ALL                 = FIELD_TERMS   | FIELD_VALUES,   // 0011  Bits for  "field_all"
	GLOBAL_TERMS              = 0b0100,                         // 0100  Bit for   "global_terms"
	TERMS                     = GLOBAL_TERMS  | FIELD_TERMS,    // 0101  Bits for  "terms"
	GLOBAL_TERMS_FIELD_VALUES = GLOBAL_TERMS  | FIELD_VALUES,   // 0110  Bits for  "global_terms,field_values" *
	GLOBAL_TERMS_FIELD_ALL    = GLOBAL_TERMS  | FIELD_ALL,      // 0111  Bits for  "global_terms,field_all" *
	GLOBAL_VALUES             = 0b1000,                         // 1000  Bit for   "global_values"
	GLOBAL_VALUES_FIELD_TERMS = GLOBAL_VALUES | FIELD_TERMS,    // 1001  Bits for  "global_values,field_terms" *
	VALUES                    = GLOBAL_VALUES | FIELD_VALUES,   // 1010  Bits for  "values"
	GLOBAL_VALUES_FIELD_ALL   = GLOBAL_VALUES | FIELD_ALL,      // 1011  Bits for  "global_values,field_all" *
	GLOBAL_ALL                = GLOBAL_VALUES | GLOBAL_TERMS,   // 1100  Bits for  "global_all"
	GLOBAL_ALL_FIELD_TERMS    = GLOBAL_ALL    | FIELD_TERMS,    // 1101  Bits for  "global_all,field_terms" *
	GLOBAL_ALL_FIELD_VALUES   = GLOBAL_ALL    | FIELD_VALUES,   // 1110  Bits for  "global_all,field_values" *
	ALL                       = GLOBAL_ALL    | FIELD_ALL,      // 1111  Bits for  "all"
};


enum class StopStrategy : uint8_t {
	STOP_NONE,
	STOP_ALL,
	STOP_STEMMED,
};


enum class StemStrategy : uint8_t {
	STEM_NONE,
	STEM_SOME,
	STEM_ALL,
	STEM_ALL_Z,
};


enum class UnitTime : uint8_t {
	SECOND,
	MINUTE,
	HOUR,
	DAY,
	MONTH,
	YEAR,
	DECADE,
	CENTURY,
	MILLENNIUM,
};


inline constexpr TypeIndex operator|(const uint8_t& a, const TypeIndex& b) {
	return static_cast<TypeIndex>(a | static_cast<uint8_t>(b));
}


inline constexpr TypeIndex operator|(const TypeIndex& a, const uint8_t& b) {
	return static_cast<TypeIndex>(static_cast<uint8_t>(a) | b);
}


inline constexpr TypeIndex operator~(const TypeIndex& o) {
	return static_cast<TypeIndex>(~static_cast<uint8_t>(o));
}


inline constexpr TypeIndex operator&(const TypeIndex& a, const TypeIndex& b) {
	return static_cast<TypeIndex>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}


inline constexpr void operator&=(TypeIndex& a, const TypeIndex& b) {
	a = a & b;
}


inline constexpr TypeIndex operator|(const TypeIndex& a, const TypeIndex& b) {
	return static_cast<TypeIndex>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}


inline constexpr void operator|=(TypeIndex& a, const TypeIndex& b) {
	a = a | b;
}


inline constexpr TypeIndex operator^(const TypeIndex& a, const TypeIndex& b) {
	return static_cast<TypeIndex>(static_cast<uint8_t>(a) ^ static_cast<uint8_t>(b));
}


inline constexpr void operator^=(TypeIndex& a, const TypeIndex& b) {
	a = a ^ b;
}


enum class FieldType : uint8_t {
	EMPTY         =  ' ',
	STRING        =  's',
	ARRAY         =  'A',
	BOOLEAN       =  'B',
	DATE          =  'D',
	FLOAT         =  'F',
	GEO           =  'G',
	INTEGER       =  'I',
	OBJECT        =  'O',
	POSITIVE      =  'P',
	TERM          =  'T',
	TEXT          =  'S',
	UUID          =  'U',
};


const std::unique_ptr<Xapian::SimpleStopper>& getStopper(const std::string& language);


inline static Xapian::TermGenerator::stop_strategy getGeneratorStopStrategy(StopStrategy stop_strategy) noexcept {
	switch (stop_strategy) {
		case StopStrategy::STOP_NONE:
			return Xapian::TermGenerator::STOP_NONE;
		case StopStrategy::STOP_ALL:
			return Xapian::TermGenerator::STOP_ALL;
		case StopStrategy::STOP_STEMMED:
			return Xapian::TermGenerator::STOP_STEMMED;
	}
}


inline static Xapian::TermGenerator::stem_strategy getGeneratorStemStrategy(StemStrategy stem_strategy) noexcept {
	switch (stem_strategy) {
		case StemStrategy::STEM_NONE:
			return Xapian::TermGenerator::STEM_NONE;
		case StemStrategy::STEM_SOME:
			return Xapian::TermGenerator::STEM_SOME;
		case StemStrategy::STEM_ALL:
			return Xapian::TermGenerator::STEM_ALL;
		case StemStrategy::STEM_ALL_Z:
			return Xapian::TermGenerator::STEM_ALL_Z;
	}
}


inline static Xapian::QueryParser::stem_strategy getQueryParserStemStrategy(StemStrategy stem_strategy) noexcept {
	switch (stem_strategy) {
		case StemStrategy::STEM_NONE:
			return Xapian::QueryParser::STEM_NONE;
		case StemStrategy::STEM_SOME:
			return Xapian::QueryParser::STEM_SOME;
		case StemStrategy::STEM_ALL:
			return Xapian::QueryParser::STEM_ALL;
		case StemStrategy::STEM_ALL_Z:
			return Xapian::QueryParser::STEM_ALL_Z;
	}
}


inline static constexpr size_t getPos(size_t pos, size_t size) noexcept {
	if (pos < size) {
		return pos;
	} else {
		return size - 1;
	}
};


/*
 * Unordered Maps used for reading user data specification.
 */

extern const std::unordered_map<std::string, UnitTime> map_acc_date;
extern const std::unordered_map<std::string, StopStrategy> map_stop_strategy;
extern const std::unordered_map<std::string, StemStrategy> map_stem_strategy;
extern const std::unordered_map<std::string, TypeIndex> map_index;
extern const std::unordered_map<std::string, FieldType> map_type;


MSGPACK_ADD_ENUM(UnitTime);
MSGPACK_ADD_ENUM(TypeIndex);
MSGPACK_ADD_ENUM(StopStrategy);
MSGPACK_ADD_ENUM(StemStrategy);
MSGPACK_ADD_ENUM(FieldType);


struct required_spc_t {
	std::array<FieldType, 3> sep_types;
	std::string prefix;
	Xapian::valueno slot;

	struct flags_t {
		bool bool_term:1;
		bool partials:1;

		bool store:1;
		bool parent_store:1;
		bool is_recursive:1;
		bool dynamic:1;
		bool strict:1;
		bool date_detection:1;
		bool numeric_detection:1;
		bool geo_detection:1;
		bool bool_detection:1;
		bool string_detection:1;
		bool text_detection:1;
		bool term_detection:1;
		bool uuid_detection:1;
		bool partial_paths:1;
		bool is_namespace:1;
		bool optimal:1;

		// Auxiliar variables.
		bool field_found:1;          // Flag if the property is already in the schema saved in the metadata
		bool field_with_type:1;      // Reserved properties that shouldn't change once set, are flagged as fixed
		bool complete:1;             // Flag if the specification for a field is complete
		bool dynamic_type:1;         // Flag if the field is dynamic type
		bool inside_namespace:1;     // Flag if the field is inside a namespace
		bool dynamic_type_path:1;    // Flag if the path has a dynamic type field

		bool has_bool_term:1;        // Either RESERVED_BOOL_TERM is in the schema or the user sent it
		bool has_index:1;            // Either RESERVED_INDEX is in the schema or the user sent it
		bool has_namespace:1;        // Either RESERVED_NAMESPACE is in the schema or the user sent it
		bool has_partial_paths:1;    // Either RESERVED_PARTIAL_PATHS is in the schema or the user sent it

		flags_t();
	} flags;

	// For GEO, DATE and Numeric types.
	std::vector<uint64_t> accuracy;
	std::vector<std::string> acc_prefix;

	// For STRING and TEXT type.
	std::string language;
	// Variables for TEXT type.
	StopStrategy stop_strategy;
	StemStrategy stem_strategy;
	std::string stem_language;

	// Variables for GEO type.
	double error;

	required_spc_t();
	required_spc_t(Xapian::valueno _slot, FieldType type, const std::vector<uint64_t>& acc, const std::vector<std::string>& _acc_prefix);
	required_spc_t(const required_spc_t& o);
	required_spc_t(required_spc_t&& o) noexcept;

	required_spc_t& operator=(const required_spc_t& o);
	required_spc_t& operator=(required_spc_t&& o) noexcept;

	FieldType get_type() const noexcept {
		return sep_types[2];
	}

	char get_ctype() const noexcept {
		return toupper(toUType(sep_types[2]));
	}
};


struct specification_t : required_spc_t {
	// Reserved values.
	std::string local_prefix;
	std::vector<Xapian::termpos> position;
	std::vector<Xapian::termcount> weight;
	std::vector<bool> spelling;
	std::vector<bool> positions;
	TypeIndex index;

	// Value recovered from the item.
	std::unique_ptr<const MsgPack> value;
	std::unique_ptr<MsgPack> value_rec;
	std::unique_ptr<const MsgPack> doc_acc;

	// Script for the object.
	std::shared_ptr<const MsgPack> script;

	std::string name;
	std::string meta_name;
	std::string full_meta_name;

	std::string aux_stem_lan;
	std::string aux_lan;

	// Auxiliar variables for saving partial prefixes.
	std::vector<std::string> partial_prefixes;
	std::vector<required_spc_t> partial_spcs;

	specification_t();
	specification_t(Xapian::valueno _slot, FieldType type, const std::vector<uint64_t>& acc, const std::vector<std::string>& _acc_prefix);
	specification_t(const specification_t& o);
	specification_t(specification_t&& o) noexcept;

	specification_t& operator=(const specification_t& o);
	specification_t& operator=(specification_t&& o) noexcept;

	std::string to_string() const;

	static FieldType global_type(FieldType field_type);
	static const specification_t& get_global(FieldType field_type);
};


extern specification_t default_spc;


using TaskVector = std::vector<std::future<void>>;


using dispatch_index = void (*)(Xapian::Document&, std::string&&, const specification_t&, size_t);


class Schema {
	using dispatch_set_default_spc   = void (Schema::*)(MsgPack&);
	using dispatch_write_reserved    = void (Schema::*)(MsgPack&, const std::string&, const MsgPack&, bool);
	using dispatch_process_reserved  = void (Schema::*)(const std::string&, const MsgPack&);
	using dispatch_update_reserved   = void (Schema::*)(const MsgPack&);
	using dispatch_readable          = bool (*)(MsgPack&, MsgPack&);

	static const std::unordered_map<std::string, dispatch_set_default_spc> map_dispatch_set_default_spc;
	static const std::unordered_map<std::string, dispatch_write_reserved> map_dispatch_write_properties;
	static const std::unordered_map<std::string, dispatch_process_reserved> map_dispatch_without_type;
	static const std::unordered_map<std::string, dispatch_process_reserved> map_dispatch_document;
	static const std::unordered_map<std::string, dispatch_update_reserved> map_dispatch_properties;
	static const std::unordered_map<std::string, dispatch_readable> map_dispatch_readable;

	std::shared_ptr<const MsgPack> schema;
	std::unique_ptr<MsgPack> mut_schema;

	std::unordered_map<Xapian::valueno, StringSet> map_values;
	specification_t specification;

	/*
	 * Returns a reference to a mutable schema (a copy of the one stored in the metadata)
	 */
	MsgPack& get_mutable();

	/*
	 * Deletes the schema from the metadata and returns a reference to the mutable empty schema.
	 */
	MsgPack& clear();

	/*
	 * Restarting reserved words than are not inherited.
	 */
	void restart_specification();

	/*
	 * Restarting reserved words than are not inherited in namespace.
	 */
	void restart_namespace_specification();

	/*
	 * Main functions to index objects and arrays
	 */

	void index_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name=std::string());
	void index_array(const MsgPack*& properties, const MsgPack& array, MsgPack*& data, Xapian::Document& doc);

	void process_item_value(Xapian::Document& doc, MsgPack& data, const MsgPack& item_value, size_t pos);
	void process_item_value(Xapian::Document& doc, MsgPack*& data, const MsgPack& item_value);
	void process_item_value(Xapian::Document& doc, MsgPack*& data, size_t offsprings);


	/*
	 * Get the prefixes for a namespace.
	 */
	static std::vector<std::string> get_partial_paths(const std::vector<std::string>& partial_prefixes);

	/*
	 * Complete specification of a namespace.
	 */
	void complete_namespace_specification(const MsgPack& item_value);

	/*
	 * Complete specification of a normal field.
	 */
	void complete_specification(const MsgPack& item_value);

	/*
	 * Set type to object in properties.
	 */
	void set_type_to_object(size_t offsprings);

	/*
	 * Sets type to array in properties.
	 */
	void set_type_to_array();

	/*
	 * Validates data when RESERVED_TYPE has not been save in schema.
	 * Insert into properties all required data.
	 */

	void _validate_required_data(MsgPack& properties);
	void validate_required_namespace_data(const MsgPack& value);
	void validate_required_data(const MsgPack& value);


	/*
	 * Sets in specification the item_doc's type
	 */
	void guess_field_type(const MsgPack& item_doc);

	/*
	 * Function to index paths namespace in doc.
	 */
	void index_partial_paths(Xapian::Document& doc);

	/*
	 * Auxiliar functions for index fields in doc.
	 */

	template <typename T>
	void _index_item(Xapian::Document& doc, T&& values, size_t pos);
	void index_item(Xapian::Document& doc, const MsgPack& value, MsgPack& data, bool add_value=true);
	void index_item(Xapian::Document& doc, const MsgPack& values, MsgPack& data, size_t pos, bool add_values=true);


	static void index_term(Xapian::Document& doc, std::string serialise_val, const specification_t& field_spc, size_t pos);
	static void index_all_term(Xapian::Document& doc, const MsgPack& value, const specification_t& field_spc, const specification_t& global_spc, size_t pos);
	static void index_value(Xapian::Document& doc, const MsgPack& value, StringSet& s, const specification_t& spc, size_t pos, const specification_t* field_spc=nullptr, const specification_t* global_spc=nullptr);
	static void index_all_value(Xapian::Document& doc, const MsgPack& value, StringSet& s_f, StringSet& s_g, const specification_t& field_spc, const specification_t& global_spc, size_t pos);


	/*
	 * Auxiliar function for update schema.
	 */
	void update_schema(MsgPack*& parent_properties, const MsgPack& obj_schema, const std::string& name);

	/*
	 * Get the properties of meta name of schema.
	 */
	void get_subproperties(const MsgPack*& properties, const std::string& meta_name);
	void get_subproperties(MsgPack*& properties, const std::string& meta_name);

	/*
	 * Add partial prefix in specification.partials_prefixes or clear it.
	 */
	void update_partial_prefixes();

	/*
	 * Gets the properties stored in the schema as well as those sent by the user.
	 */

	const MsgPack& get_subproperties(const MsgPack*& properties, const MsgPack& object, MsgPack*& data, Xapian::Document& doc, TaskVector& tasks);
	const MsgPack& get_subproperties(const MsgPack*& properties);
	MsgPack& get_subproperties(MsgPack*& properties, const MsgPack& object, TaskVector& tasks);


	/*
	 * Detect and set dynamic type.
	 */
	void detect_dynamic(const std::string& field_name);

	/*
	 * Update specification using object's properties.
	 */

	void process_properties_document(const MsgPack*& properties, const MsgPack& object, MsgPack*& data, Xapian::Document& doc, TaskVector& tasks);
	void process_properties_document(MsgPack*& properties, const MsgPack& object, TaskVector& tasks);


	/*
	 * Add new field to properties.
	 */

	void add_field(MsgPack*& mut_properties, const MsgPack*& properties, const MsgPack& object, MsgPack*& data, Xapian::Document& doc, TaskVector& tasks);
	void add_field(MsgPack*& mut_properties, const MsgPack& object, TaskVector& tasks);
	void add_field(MsgPack*& mut_properties);


	/*
	 * Specification is updated with the properties.
	 */
	void update_specification(const MsgPack& properties);

	/*
	 * Functions for updating specification using the properties in schema.
	 */

	void update_position(const MsgPack& prop_position);
	void update_weight(const MsgPack& prop_weight);
	void update_spelling(const MsgPack& prop_spelling);
	void update_positions(const MsgPack& prop_positions);
	void update_language(const MsgPack& prop_language);
	void update_stop_strategy(const MsgPack& prop_stop_strategy);
	void update_stem_strategy(const MsgPack& prop_stem_strategy);
	void update_stem_language(const MsgPack& prop_stem_language);
	void update_type(const MsgPack& prop_type);
	void update_accuracy(const MsgPack& prop_accuracy);
	void update_acc_prefix(const MsgPack& prop_acc_prefix);
	void update_prefix(const MsgPack& prop_prefix);
	void update_slot(const MsgPack& prop_slot);
	void update_index(const MsgPack& prop_index);
	void update_store(const MsgPack& prop_store);
	void update_recursive(const MsgPack& prop_namespace);
	void update_dynamic(const MsgPack& prop_dynamic);
	void update_strict(const MsgPack& prop_strict);
	void update_d_detection(const MsgPack& prop_d_detection);
	void update_n_detection(const MsgPack& prop_n_detection);
	void update_g_detection(const MsgPack& prop_g_detection);
	void update_b_detection(const MsgPack& prop_b_detection);
	void update_s_detection(const MsgPack& prop_s_detection);
	void update_t_detection(const MsgPack& prop_t_detection);
	void update_tm_detection(const MsgPack& prop_tm_detection);
	void update_u_detection(const MsgPack& prop_u_detection);
	void update_bool_term(const MsgPack& prop_bool_term);
	void update_partials(const MsgPack& prop_partials);
	void update_error(const MsgPack& prop_error);
	void update_namespace(const MsgPack& prop_namespace);
	void update_partial_paths(const MsgPack& prop_partial_paths);


	/*
	 * Functions for reserved words that are in document and need to be written in schema properties.
	 */

	void write_weight(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_weight, bool is_root);
	void write_position(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_position, bool is_root);
	void write_spelling(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_spelling, bool is_root);
	void write_positions(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_positions, bool is_root);
	void write_index(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_index, bool is_root);
	void write_store(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_store, bool is_root);
	void write_recursive(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_recursive, bool is_root);
	void write_dynamic(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_dynamic, bool is_root);
	void write_strict(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_strict, bool is_root);
	void write_d_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_d_detection, bool is_root);
	void write_n_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_n_detection, bool is_root);
	void write_g_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_g_detection, bool is_root);
	void write_b_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_b_detection, bool is_root);
	void write_s_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_s_detection, bool is_root);
	void write_t_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_t_detection, bool is_root);
	void write_tm_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_tm_detection, bool is_root);
	void write_u_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_u_detection, bool is_root);
	void write_namespace(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_namespace, bool is_root);
	void write_partial_paths(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_partial_paths, bool is_root);


	/*
	 * Functions for reserved words that are in the document.
	 */

	void process_weight(const std::string& prop_name, const MsgPack& doc_weight);
	void process_position(const std::string& prop_name, const MsgPack& doc_position);
	void process_spelling(const std::string& prop_name, const MsgPack& doc_spelling);
	void process_positions(const std::string& prop_name, const MsgPack& doc_positions);
	void process_language(const std::string& prop_name, const MsgPack& doc_language);
	void process_stop_strategy(const std::string& prop_name, const MsgPack& doc_stop_strategy);
	void process_stem_strategy(const std::string& prop_name, const MsgPack& doc_stem_strategy);
	void process_stem_language(const std::string& prop_name, const MsgPack& doc_stem_language);
	void process_type(const std::string& prop_name, const MsgPack& doc_type);
	void process_accuracy(const std::string& prop_name, const MsgPack& doc_accuracy);
	void process_index(const std::string& prop_name, const MsgPack& doc_index);
	void process_store(const std::string& prop_name, const MsgPack& doc_store);
	void process_recursive(const std::string& prop_name, const MsgPack& doc_recursive);
	void process_partial_paths(const std::string& prop_name, const MsgPack& doc_partial_paths);
	void process_bool_term(const std::string& prop_name, const MsgPack& doc_bool_term);
	void process_partials(const std::string& prop_name, const MsgPack& doc_partials);
	void process_error(const std::string& prop_name, const MsgPack& doc_error);
	void process_value(const std::string& prop_name, const MsgPack& doc_value);
	void process_script(const std::string& prop_name, const MsgPack& doc_script);
	void process_cast_object(const std::string& prop_name, const MsgPack& doc_cast_object);


	/*
	 * Functions to update default specification for fields.
	 */

	void set_default_spc_id(MsgPack& properties);
	void set_default_spc_ct(MsgPack& properties);


	/*
	 * Recursively transforms item_schema into a readable form.
	 */
	static void readable(MsgPack& item_schema, bool is_root);

	/*
	 * Tranforms reserved words into a readable form.
	 */

	static bool readable_type(MsgPack& prop_type, MsgPack& properties);
	static bool readable_prefix(MsgPack& prop_prefix, MsgPack& properties);
	static bool readable_stop_strategy(MsgPack& prop_stop_strategy, MsgPack& properties);
	static bool readable_stem_strategy(MsgPack& prop_stem_strategy, MsgPack& properties);
	static bool readable_stem_language(MsgPack& prop_stem_language, MsgPack& properties);
	static bool readable_index(MsgPack& prop_index, MsgPack& properties);
	static bool readable_acc_prefix(MsgPack& prop_acc_prefix, MsgPack& properties);


	/*
	 * Returns:
	 *   - The propierties of full name
	 *   - If full name is dynamic
	 *   - If full name is namespace
	 *   - The prefix
	 *   - The accuracy field name
	 *   - The type for accuracy field name
	 * if the path does not exist or is not valid field name throw a ClientError exception.
	 */

	std::tuple<const MsgPack&, bool, bool, std::string, std::string, FieldType> get_dynamic_subproperties(const MsgPack& properties, const std::string& full_name) const;

public:
	Schema(const std::shared_ptr<const MsgPack>& schema);

	Schema() = delete;
	Schema(Schema&& schema) = delete;
	Schema(const Schema& schema) = delete;
	Schema& operator=(Schema&& schema) = delete;
	Schema& operator=(const Schema& schema) = delete;

	~Schema() = default;

	static std::shared_ptr<const MsgPack> get_initial_schema();

	std::shared_ptr<const MsgPack> get_modified_schema();

	std::shared_ptr<const MsgPack> get_const_schema() const;

	/*
	 * Transforms schema into json string.
	 */
	std::string to_string(bool prettify=false) const;

	/*
	 * Function to index object in doc.
	 */
	MsgPack index(const MsgPack& object, Xapian::Document& doc);

	/*
	 * Returns readable schema.
	 */
	const MsgPack get_readable() const;

	/*
	 * Function to update the schema according to obj_schema.
	 */
	void write_schema(const MsgPack& obj_schema, bool replace);

	/*
	 * Update namespace specification according to prefix_namespace.
	 */
	static required_spc_t get_namespace_specification(FieldType namespace_type, const std::string& prefix_namespace);

	/*
	 * Returns type, slot and prefix of ID_FIELD_NAME
	 */
	required_spc_t get_data_id() const;

	/*
	 * Functions used for searching, return a field properties.
	 */

	std::pair<required_spc_t, std::string> get_data_field(const std::string& field_name, bool is_range=true) const;
	required_spc_t get_slot_field(const std::string& field_name) const;
	static const required_spc_t& get_data_global(FieldType field_type);
};
