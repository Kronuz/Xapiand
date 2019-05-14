/*
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

#pragma once

#include "config.h"                               // for XAPIAND_CHAISCRIPT

#include <array>                                  // for std::array
#include <cstdint>                                // for uint8_t
#include <deque>                                  // for std::deque
#include <memory>                                 // for std::shared_ptr
#include <string>                                 // for std::string
#include <string_view>                            // for std::string_view
#include <tuple>                                  // for std::tuple
#include <unordered_map>                          // for std::unordered_map
#include <unordered_set>                          // for std::unordered_set
#include <utility>                                // for std::pair
#include <vector>                                 // for std::vector

#include "database/utils.h"
#include "enum.h"                                 // for ENUM_CLASS
#include "geospatial/htm.h"                       // for range_t, GeoSpatial
#include "log.h"                                  // for L_CALL
#include "msgpack.h"                              // for MsgPack
#include "repr.hh"                                // for repr
#include "reserved/fields.h"                      // for SCHEMA_FIELD_NAME
#include "utype.hh"                               // for toUType
#include "xapian.h"                               // for Xapian::QueryParser


ENUM_CLASS(TypeIndex, uint8_t,
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
	INVALID                   = static_cast<uint8_t>(-1)
)


ENUM_CLASS(UUIDFieldIndex, uint8_t,
	uuid        = 0b0001,  // Indexin using the field name.
	uuid_field  = 0b0010,  // Indexing using the meta name.
	both        = 0b0011,  // Indexing using field_uuid and uuid.
	INVALID     = static_cast<uint8_t>(-1)
)


ENUM_CLASS(StopStrategy, uint8_t,
	stop_none,
	stop_all,
	stop_stemmed,
	INVALID    = static_cast<uint8_t>(-1)
)


ENUM_CLASS(StemStrategy, uint8_t,
	stem_none,
	stem_some,
	stem_all,
	stem_all_z,
	INVALID    = static_cast<uint8_t>(-1)
)


ENUM_CLASS(UnitTime, uint64_t,
	second     = 1,                     // 1                  60
	minute     = second * 60,           // 60                 60
	hour       = minute * 60,           // 3600               24
	day        = hour * 24,             // 86400              30
	month      = day * 30,              // 2592000            12
	year       = day * 365,             // 31536000           10
	decade     = year * 10,             // 315360000          10
	century    = year * 100,            // 3153600000         10
	millennium = year * 1000,           // 31536000000        8
	INVALID    = static_cast<uint64_t>(-1)
)


constexpr StopStrategy DEFAULT_STOP_STRATEGY      = StopStrategy::stop_stemmed;
constexpr StemStrategy DEFAULT_STEM_STRATEGY      = StemStrategy::stem_some;
constexpr bool DEFAULT_GEO_PARTIALS               = true;
constexpr double DEFAULT_GEO_ERROR                = 0.3;
constexpr bool DEFAULT_POSITIONS                  = true;
constexpr bool DEFAULT_SPELLING                   = false;
constexpr bool DEFAULT_BOOL_TERM                  = false;
constexpr TypeIndex DEFAULT_INDEX                 = TypeIndex::FIELD_ALL;
constexpr UUIDFieldIndex DEFAULT_INDEX_UUID_FIELD = UUIDFieldIndex::uuid;
constexpr size_t LIMIT_PARTIAL_PATHS_DEPTH        = 10; // 2^(n - 2) => 2^8 => 256 namespace terms


constexpr size_t SPC_FOREIGN_TYPE  = 0;
constexpr size_t SPC_ARRAY_TYPE    = 1;
constexpr size_t SPC_CONCRETE_TYPE = 2;
constexpr size_t SPC_TOTAL_TYPES   = 3;


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


constexpr uint8_t EMPTY_CHAR         = ' ';
constexpr uint8_t STRING_CHAR        = 's';
constexpr uint8_t TIMEDELTA_CHAR     = 'z';
constexpr uint8_t ARRAY_CHAR         = 'A';
constexpr uint8_t BOOLEAN_CHAR       = 'B';
constexpr uint8_t DATE_CHAR          = 'd';
constexpr uint8_t DATETIME_CHAR      = 'D';
constexpr uint8_t FOREIGN_CHAR       = 'E';
constexpr uint8_t FLOATING_CHAR      = 'F';
constexpr uint8_t GEO_CHAR           = 'G';
constexpr uint8_t INTEGER_CHAR       = 'I';
constexpr uint8_t OBJECT_CHAR        = 'O';
constexpr uint8_t POSITIVE_CHAR      = 'P';
constexpr uint8_t TEXT_CHAR          = 'S';
constexpr uint8_t KEYWORD_CHAR       = 'K';
constexpr uint8_t UUID_CHAR          = 'U';
constexpr uint8_t SCRIPT_CHAR        = 'X';
constexpr uint8_t TIME_CHAR          = 'Z';

ENUM_CLASS(FieldType, uint8_t,
	empty         = EMPTY_CHAR,

	foreign       = FOREIGN_CHAR,
	array         = ARRAY_CHAR,

	object        = OBJECT_CHAR,
	boolean       = BOOLEAN_CHAR,
	date          = DATE_CHAR,
	datetime      = DATETIME_CHAR,
	floating      = FLOATING_CHAR,
	geo           = GEO_CHAR,
	integer       = INTEGER_CHAR,
	keyword       = KEYWORD_CHAR,
	positive      = POSITIVE_CHAR,
	script        = SCRIPT_CHAR,
	string        = STRING_CHAR,
	text          = TEXT_CHAR,
	time          = TIME_CHAR,
	timedelta     = TIMEDELTA_CHAR,
	uuid          = UUID_CHAR
)


inline constexpr Xapian::TermGenerator::stop_strategy getGeneratorStopStrategy(StopStrategy stop_strategy) {
	switch (stop_strategy) {
		case StopStrategy::stop_none:
			return Xapian::TermGenerator::STOP_NONE;
		case StopStrategy::stop_all:
			return Xapian::TermGenerator::STOP_ALL;
		case StopStrategy::stop_stemmed:
			return Xapian::TermGenerator::STOP_STEMMED;
		default:
			THROW(Error, "Schema is corrupt: invalid stop strategy");
	}
}


inline constexpr Xapian::TermGenerator::stem_strategy getGeneratorStemStrategy(StemStrategy stem_strategy) {
	switch (stem_strategy) {
		case StemStrategy::stem_none:
			return Xapian::TermGenerator::STEM_NONE;
		case StemStrategy::stem_some:
			return Xapian::TermGenerator::STEM_SOME;
		case StemStrategy::stem_all:
			return Xapian::TermGenerator::STEM_ALL;
		case StemStrategy::stem_all_z:
			return Xapian::TermGenerator::STEM_ALL_Z;
		default:
			THROW(Error, "Schema is corrupt: invalid stem strategy");
	}
}


// Waiting for QueryParser::stop_strategy()
// https://trac.xapian.org/ticket/750
// inline constexpr Xapian::QueryParser::stop_strategy getGeneratorStopStrategy(StopStrategy stop_strategy) {
// 	switch (stop_strategy) {
// 		case StopStrategy::stop_none:
// 			return Xapian::QueryParser::STOP_NONE;
// 		case StopStrategy::stop_all:
// 			return Xapian::QueryParser::STOP_ALL;
// 		case StopStrategy::stop_stemmed:
// 			return Xapian::QueryParser::STOP_STEMMED;
// 		default:
// 			THROW(Error, "Schema is corrupt: invalid stop strategy");
// 	}
// }


inline constexpr Xapian::QueryParser::stem_strategy getQueryParserStemStrategy(StemStrategy stem_strategy) {
	switch (stem_strategy) {
		case StemStrategy::stem_none:
			return Xapian::QueryParser::STEM_NONE;
		case StemStrategy::stem_some:
			return Xapian::QueryParser::STEM_SOME;
		case StemStrategy::stem_all:
			return Xapian::QueryParser::STEM_ALL;
		case StemStrategy::stem_all_z:
			return Xapian::QueryParser::STEM_ALL_Z;
		default:
			THROW(Error, "Schema is corrupt: invalid stem strategy");
	}
}


inline constexpr size_t getPos(size_t pos, size_t size) noexcept {
	if (pos < size) {
		return pos;
	} else {
		return size - 1;
	}
}


extern UnitTime get_accuracy_time(std::string_view str_accuracy_time);
extern UnitTime get_accuracy_datetime(std::string_view str_accuracy_datetime);


MSGPACK_ADD_ENUM(UnitTime)
MSGPACK_ADD_ENUM(TypeIndex)
MSGPACK_ADD_ENUM(UUIDFieldIndex)
MSGPACK_ADD_ENUM(StopStrategy)
MSGPACK_ADD_ENUM(StemStrategy)
MSGPACK_ADD_ENUM(FieldType)


struct required_spc_t {
	struct flags_t {
		bool bool_term:1;
		bool partials:1;

		bool store:1;
		bool parent_store:1;
		bool recurse:1;
		bool dynamic:1;
		bool strict:1;
		bool date_detection:1;
		bool datetime_detection:1;
		bool time_detection:1;
		bool timedelta_detection:1;
		bool numeric_detection:1;
		bool geo_detection:1;
		bool bool_detection:1;
		bool text_detection:1;
		bool uuid_detection:1;

		bool partial_paths:1;
		bool is_namespace:1;
		bool ngram:1;
		bool cjk_ngram:1;
		bool cjk_words:1;

		// Auxiliar variables.
		bool field_found:1;          // Flag if the property is already in the schema saved in the metadata
		bool concrete:1;             // Reserved properties that shouldn't change once set, are flagged as fixed
		bool complete:1;             // Flag if the specification for a field is complete
		bool uuid_field:1;           // Flag if the field is uuid
		bool uuid_path:1;            // Flag if the paths has uuid fields.
		bool inside_namespace:1;     // Flag if the field is inside a namespace
#ifdef XAPIAND_CHAISCRIPT
		bool normalized_script:1;    // Flag if the script is normalized.
#endif

		bool has_uuid_prefix:1;      // Flag if prefix.field has uuid prefix.
		bool has_bool_term:1;        // Either RESERVED_BOOL_TERM is in the schema or the user sent it
		bool has_index:1;            // Either RESERVED_INDEX is in the schema or the user sent it
		bool has_namespace:1;        // Either RESERVED_NAMESPACE is in the schema or the user sent it
		bool has_partial_paths:1;    // Either RESERVED_PARTIAL_PATHS is in the schema or the user sent it

		bool static_endpoint:1;      // RESERVED_ENDPOINT is from the schema

		flags_t();
	};

	struct prefix_t {
		std::string field;
		std::string uuid;

		prefix_t() = default;

		template <typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value>>
		explicit prefix_t(S&& _field, S&& _uuid=std::string())
			: field(std::forward<S>(_field)),
			  uuid(std::forward<S>(_uuid)) { }

		prefix_t(prefix_t&&) = default;
		prefix_t(const prefix_t&) = default;
		prefix_t& operator=(prefix_t&&) = default;
		prefix_t& operator=(const prefix_t&) = default;

		std::string to_string() const;
		std::string operator()() const noexcept;
	};

	std::array<FieldType, SPC_TOTAL_TYPES> sep_types; // foreign/object/array/index_type
	prefix_t prefix;
	Xapian::valueno slot;
	flags_t flags;

	// For GEO, DATE, DATETIME, TIME, TIMEDELTA and Numeric types.
	std::vector<uint64_t> accuracy;
	std::vector<std::string> acc_prefix;

	std::unordered_set<std::string> ignored;

	// For STRING and TEXT type.
	std::string language;
	// Variables for TEXT type.
	StopStrategy stop_strategy;
	StemStrategy stem_strategy;
	std::string stem_language;

	// Variables for GEO type.
	double error;

	////////////////////////////////////////////////////////////////////
	// Methods:

	required_spc_t();
	required_spc_t(Xapian::valueno slot, FieldType type, std::vector<uint64_t> accuracy, std::vector<std::string> acc_prefix);
	required_spc_t(const required_spc_t& o);
	required_spc_t(required_spc_t&& o) noexcept;

	required_spc_t& operator=(const required_spc_t& o);
	required_spc_t& operator=(required_spc_t&& o) noexcept;

	MsgPack to_obj() const;
	std::string to_string(int indent=-1) const;

	FieldType get_type() const noexcept {
		return sep_types[SPC_CONCRETE_TYPE];
	}

	std::string_view get_str_type() const {
		return get_str_type(sep_types);
	}

	void set_type(FieldType type) {
		sep_types[SPC_CONCRETE_TYPE] = type;
	}

	static char get_ctype(FieldType type) noexcept {
		switch (type) {
			case FieldType::uuid:
				return 'U';

			case FieldType::keyword:
				return 'K';

			case FieldType::script:
			case FieldType::string:
			case FieldType::text:
				return 'S';

			case FieldType::positive:
			case FieldType::integer:
			case FieldType::floating:
				return 'N';

			case FieldType::boolean:
				return 'B';

			case FieldType::date:
			case FieldType::datetime:
				return 'D';

			case FieldType::timedelta:
			case FieldType::time:
				return 'T';

			case FieldType::geo:
				return 'G';

			case FieldType::array:
			case FieldType::object:
			case FieldType::foreign:
			case FieldType::empty:
			default:
				return '\x00';
		}
	}

	char get_ctype() const noexcept {
		return get_ctype(sep_types[SPC_CONCRETE_TYPE]);
	}

	static const std::array<FieldType, SPC_TOTAL_TYPES>& get_types(std::string_view str_type);
	static std::string_view get_str_type(const std::array<FieldType, SPC_TOTAL_TYPES>& sep_types);

	void set_types(std::string_view str_type);
};


struct index_spc_t {
	FieldType type;
	std::string prefix;
	Xapian::valueno slot;
	std::vector<uint64_t> accuracy;
	std::vector<std::string> acc_prefix;

	template <typename S=std::string, typename Uint64Vector=std::vector<uint64_t>, typename StringVector=std::vector<std::string>, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value &&
		std::is_same<std::vector<uint64_t>, std::decay_t<Uint64Vector>>::value && std::is_same<std::vector<std::string>, std::decay_t<StringVector>>::value>>
	explicit index_spc_t(FieldType type, S&& prefix=S{}, Xapian::valueno slot=Xapian::BAD_VALUENO, Uint64Vector&& accuracy=Uint64Vector(), StringVector&& acc_prefix=StringVector())
		: type(type),
		  prefix(std::forward<S>(prefix)),
		  slot(slot),
		  accuracy(std::forward<Uint64Vector>(accuracy)),
		  acc_prefix(std::forward<StringVector>(acc_prefix)) { }

	explicit index_spc_t(required_spc_t&& spc);
	explicit index_spc_t(const required_spc_t& spc);
};


struct specification_t : required_spc_t {
	// Reserved values.
	prefix_t local_prefix;
	std::vector<Xapian::termpos> position;
	std::vector<Xapian::termcount> weight;
	std::vector<bool> spelling;
	std::vector<bool> positions;

	TypeIndex index;

	UUIDFieldIndex index_uuid_field;  // Used to save how to index uuid fields.

	// Value recovered from the item.
	std::unique_ptr<const MsgPack> value_rec;
	std::unique_ptr<const MsgPack> value;
	std::unique_ptr<const MsgPack> doc_acc;
#ifdef XAPIAND_CHAISCRIPT
	std::unique_ptr<const MsgPack> script;
#endif

	std::string endpoint;

	// Used to save the last meta name.
	std::string meta_name;

	// Used to get mutable properties anytime.
	std::string full_meta_name;

	// Auxiliar variables, only need specify _stem_language or _language.
	std::string aux_stem_language;
	std::string aux_language;

	// Auxiliar variables for saving partial prefixes.
	std::vector<prefix_t> partial_prefixes;
	std::vector<index_spc_t> partial_index_spcs;

	////////////////////////////////////////////////////////////////////
	// Methods:

	specification_t();
	specification_t(Xapian::valueno slot, FieldType type, const std::vector<uint64_t>& accuracy, const std::vector<std::string>& acc_prefix);
	specification_t(const specification_t& o);
	specification_t(specification_t&& o) noexcept;

	specification_t& operator=(const specification_t& o);
	specification_t& operator=(specification_t&& o) noexcept;

	void update(index_spc_t&& spc);
	void update(const index_spc_t& spc);

	MsgPack to_obj() const;
	std::string to_string(int indent=-1) const;

	static FieldType global_type(FieldType field_type);
	static const specification_t& get_global(FieldType field_type);
};


extern specification_t default_spc;
extern const std::string NAMESPACE_PREFIX_ID_FIELD_NAME;


using dispatch_index = void (*)(Xapian::Document&, std::string&&, const specification_t&, size_t);


class DatabaseHandler;


class Schema {
	using dispatcher_set_default_spc     = void (Schema::*)(MsgPack&);
	using dispatcher_write_properties    = void (Schema::*)(MsgPack&, std::string_view, const MsgPack&);
	using dispatcher_process_properties  = void (Schema::*)(std::string_view, const MsgPack&);
	using dispatcher_feed_properties     = void (Schema::*)(const MsgPack&);
	using dispatcher_readable            = bool (*)(MsgPack&, MsgPack&);

	using Field = std::pair<std::string, const MsgPack*>;
	using Fields = std::deque<Field>;

	bool _dispatch_write_properties(uint32_t key, MsgPack& mut_properties, std::string_view prop_name, const MsgPack& value);
	bool _dispatch_feed_properties(uint32_t key, const MsgPack& value);
	bool _dispatch_process_properties(uint32_t key, std::string_view prop_name, const MsgPack& value);
	bool _dispatch_process_concrete_properties(uint32_t key, std::string_view prop_name, const MsgPack& value);
	static bool _dispatch_readable(uint32_t key, MsgPack& value, MsgPack& properties);

	std::shared_ptr<const MsgPack> schema;
	std::unique_ptr<MsgPack> mut_schema;
	std::string origin;

	std::unordered_map<Xapian::valueno, std::set<std::string>> map_values;
	specification_t specification;

	/*
	 * Returns root properties of schema.
	 */
	const MsgPack& get_properties() const {
		return schema->at(SCHEMA_FIELD_NAME);
	}

	/*
	 * Returns root properties of mut_schema.
	 */
	MsgPack& get_mutable_properties() {
		if (!mut_schema) {
			mut_schema = std::make_unique<MsgPack>(*schema);
		}
		return mut_schema->at(SCHEMA_FIELD_NAME);
	}

	/*
	 * Returns newest root properties.
	 */
	const MsgPack& get_newest_properties() const {
		if (mut_schema) {
			return mut_schema->at(SCHEMA_FIELD_NAME);
		} else {
			return schema->at(SCHEMA_FIELD_NAME);
		}
	}

	/*
	 * Returns full_meta_name properties of schema.
	 */
	const MsgPack& get_properties(std::string_view full_meta_name);

	/*
	 * Returns mutable full_meta_name properties of mut_schema.
	 */
	MsgPack& get_mutable_properties(std::string_view full_meta_name);

	/*
	 * Returns newest full_meta_name properties.
	 */
	const MsgPack& get_newest_properties(std::string_view full_meta_name);

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
	 * Get the properties of meta name of schema.
	 */

	template <typename T>
	bool feed_subproperties(T& properties, std::string_view meta_name);

	/*
	 * Main functions to index objects and arrays
	 */

	const MsgPack& index_subproperties(const MsgPack*& properties, MsgPack*& data, std::string_view name, const MsgPack* object = nullptr, Fields* fields = nullptr);

	void index_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name);
	void index_array(const MsgPack*& parent_properties, const MsgPack& array, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name);

	void index_item_value(Xapian::Document& doc, MsgPack& data, const MsgPack& item_value, size_t pos = 0);

	void index_fields(const MsgPack*& properties, Xapian::Document& doc, MsgPack*& data, const Fields& fields);

	/*
	 * Main functions to update objects and arrays
	 */

	const MsgPack& update_subproperties(const MsgPack*& properties, std::string_view name, const MsgPack& object, Fields& fields);
	const MsgPack& update_subproperties(const MsgPack*& properties, const std::string& name);

	void update_object(const MsgPack*& parent_properties, const MsgPack& object, const std::string& name);
	void update_array(const MsgPack*& parent_properties, const MsgPack& array, const std::string& name);

	void update_item_value();
	void update_item_value(const MsgPack*& properties, const Fields& fields);

	/*
	 * Main functions to write objects and arrays
	 */

	MsgPack& write_subproperties(MsgPack*& mut_properties, std::string_view name, const MsgPack& object, Fields& fields);
	MsgPack& write_subproperties(MsgPack*& mut_properties, const std::string& name);

	void write_object(MsgPack*& mut_parent_properties, const MsgPack& object, const std::string& name);
	void write_array(MsgPack*& mut_parent_properties, const MsgPack& array, const std::string& name);

	void write_item_value(MsgPack*& mut_properties);
	void write_item_value(MsgPack*& mut_properties, const Fields& fields);


	/*
	 * Get the prefixes for a namespace.
	 */
	static std::unordered_set<std::string> get_partial_paths(const std::vector<required_spc_t::prefix_t>& partial_prefixes, bool uuid_path);

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
	void set_type_to_object();

	/*
	 * Sets type to array in properties.
	 */
	void set_type_to_array();

	/*
	 * Validates data when RESERVED_TYPE has not been save in schema.
	 * Insert into properties all required data.
	 */

	void validate_required_namespace_data();
	void validate_required_data(MsgPack& mut_properties);

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
	void _index_items(Xapian::Document& doc, T&& values, size_t pos);
	void _store_items(const MsgPack& values, MsgPack& data, bool add_values = true);
	void _store_item(const MsgPack& value, MsgPack& data, bool add_value = true);
	void index_item(Xapian::Document& doc, const MsgPack& value, MsgPack& data, size_t pos, bool add_value = true);


	static void index_simple_term(Xapian::Document& doc, std::string_view term, const specification_t& field_spc, size_t pos);
	static void index_term(Xapian::Document& doc, std::string serialise_val, const specification_t& field_spc, size_t pos);
	static void index_all_term(Xapian::Document& doc, const MsgPack& value, const specification_t& field_spc, const specification_t& global_spc, size_t pos);
	static void merge_geospatial_values(std::set<std::string>& s, std::vector<range_t> ranges, std::vector<Cartesian> centroids);
	static void index_value(Xapian::Document& doc, const MsgPack& value, std::set<std::string>& s, const specification_t& spc, size_t pos, const specification_t* field_spc=nullptr, const specification_t* global_spc=nullptr);
	static void index_all_value(Xapian::Document& doc, const MsgPack& value, std::set<std::string>& s_f, std::set<std::string>& s_g, const specification_t& field_spc, const specification_t& global_spc, size_t pos);

	/*
	 * Add partial prefix in specification.partials_prefixes or clear it.
	 */
	void update_prefixes();

	/*
	 * Verify if field_name is the meta field used for dynamic types.
	 */
	void verify_dynamic(std::string_view field_name);

	/*
	 * Detect if field_name is dynamic type.
	 */
	void detect_dynamic(std::string_view field_name);

	/*
	 * Update specification using object's properties.
	 */

	void dispatch_process_concrete_properties(const MsgPack& object, Fields& fields, Field** id_field = nullptr, Field** version_field = nullptr);
	void dispatch_process_all_properties(const MsgPack& object, Fields& fields, Field** id_field = nullptr, Field** version_field = nullptr);
	void dispatch_process_properties(const MsgPack& object, Fields& fields, Field** id_field = nullptr, Field** version_field = nullptr);
	void dispatch_write_concrete_properties(MsgPack& mut_properties, const MsgPack& object, Fields& fields, Field** id_field = nullptr, Field** version_field = nullptr);
	void dispatch_write_all_properties(MsgPack& mut_properties, const MsgPack& object, Fields& fields, Field** id_field = nullptr, Field** version_field = nullptr);
	void dispatch_write_properties(MsgPack& mut_properties, const MsgPack& object, Fields& fields, Field** id_field = nullptr, Field** version_field = nullptr);
	void dispatch_set_default_spc(MsgPack& mut_properties);


	/*
	 * Add new field to properties.
	 */

	void add_field(MsgPack*& mut_properties, const MsgPack& object, Fields& fields);
	void add_field(MsgPack*& mut_properties);


	/*
	 * Specification is fed with the properties.
	 */
	void dispatch_feed_properties(const MsgPack& properties);

	/*
	 * Functions for feeding specification using the properties in schema.
	 */

	void feed_position(const MsgPack& prop_obj);
	void feed_weight(const MsgPack& prop_obj);
	void feed_spelling(const MsgPack& prop_obj);
	void feed_positions(const MsgPack& prop_obj);
	void feed_ngram(const MsgPack& prop_obj);
	void feed_cjk_ngram(const MsgPack& prop_obj);
	void feed_cjk_words(const MsgPack& prop_obj);
	void feed_language(const MsgPack& prop_obj);
	void feed_stop_strategy(const MsgPack& prop_obj);
	void feed_stem_strategy(const MsgPack& prop_obj);
	void feed_stem_language(const MsgPack& prop_obj);
	void feed_type(const MsgPack& prop_obj);
	void feed_accuracy(const MsgPack& prop_obj);
	void feed_acc_prefix(const MsgPack& prop_obj);
	void feed_prefix(const MsgPack& prop_obj);
	void feed_slot(const MsgPack& prop_obj);
	void feed_index(const MsgPack& prop_obj);
	void feed_store(const MsgPack& prop_obj);
	void feed_recurse(const MsgPack& prop_obj);
	void feed_ignore(const MsgPack& prop_obj);
	void feed_dynamic(const MsgPack& prop_obj);
	void feed_strict(const MsgPack& prop_obj);
	void feed_date_detection(const MsgPack& prop_obj);
	void feed_datetime_detection(const MsgPack& prop_obj);
	void feed_time_detection(const MsgPack& prop_obj);
	void feed_timedelta_detection(const MsgPack& prop_obj);
	void feed_numeric_detection(const MsgPack& prop_obj);
	void feed_geo_detection(const MsgPack& prop_obj);
	void feed_bool_detection(const MsgPack& prop_obj);
	void feed_text_detection(const MsgPack& prop_obj);
	void feed_uuid_detection(const MsgPack& prop_obj);
	void feed_bool_term(const MsgPack& prop_obj);
	void feed_partials(const MsgPack& prop_obj);
	void feed_error(const MsgPack& prop_obj);
	void feed_namespace(const MsgPack& prop_obj);
	void feed_partial_paths(const MsgPack& prop_obj);
	void feed_index_uuid_field(const MsgPack& prop_obj);
	void feed_script(const MsgPack& prop_obj);
	void feed_endpoint(const MsgPack& prop_obj);


	/*
	 * Functions for reserved words that are in document and need to be written in schema properties.
	 */

	void write_weight(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_position(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_spelling(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_positions(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_index(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_store(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_recurse(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_ignore(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_dynamic(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_strict(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_date_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_datetime_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_time_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_timedelta_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_numeric_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_geo_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_bool_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_text_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_uuid_detection(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_bool_term(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_namespace(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_partial_paths(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_index_uuid_field(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_schema(MsgPack& properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_settings(MsgPack& properties, std::string_view prop_name, const MsgPack& prop_obj);
	void write_endpoint(MsgPack& mut_properties, std::string_view prop_name, const MsgPack& prop_obj);


	/*
	 * Functions for reserved words that are in the document.
	 */

	void process_data(std::string_view prop_name, const MsgPack& prop_obj);
	void process_weight(std::string_view prop_name, const MsgPack& prop_obj);
	void process_position(std::string_view prop_name, const MsgPack& prop_obj);
	void process_spelling(std::string_view prop_name, const MsgPack& prop_obj);
	void process_positions(std::string_view prop_name, const MsgPack& prop_obj);
	void process_ngram(std::string_view prop_name, const MsgPack& prop_obj);
	void process_cjk_ngram(std::string_view prop_name, const MsgPack& prop_obj);
	void process_cjk_words(std::string_view prop_name, const MsgPack& prop_obj);
	void process_language(std::string_view prop_name, const MsgPack& prop_obj);
	void process_prefix(std::string_view prop_name, const MsgPack& prop_obj);
	void process_slot(std::string_view prop_name, const MsgPack& prop_obj);
	void process_stop_strategy(std::string_view prop_name, const MsgPack& prop_obj);
	void process_stem_strategy(std::string_view prop_name, const MsgPack& prop_obj);
	void process_stem_language(std::string_view prop_name, const MsgPack& prop_obj);
	void process_type(std::string_view prop_name, const MsgPack& prop_obj);
	void process_accuracy(std::string_view prop_name, const MsgPack& prop_obj);
	void process_acc_prefix(std::string_view prop_name, const MsgPack& prop_obj);
	void process_index(std::string_view prop_name, const MsgPack& prop_obj);
	void process_store(std::string_view prop_name, const MsgPack& prop_obj);
	void process_recurse(std::string_view prop_name, const MsgPack& prop_obj);
	void process_ignore(std::string_view prop_name, const MsgPack& prop_obj);
	void process_partial_paths(std::string_view prop_name, const MsgPack& prop_obj);
	void process_index_uuid_field(std::string_view prop_name, const MsgPack& prop_obj);
	void process_bool_term(std::string_view prop_name, const MsgPack& prop_obj);
	void process_partials(std::string_view prop_name, const MsgPack& prop_obj);
	void process_error(std::string_view prop_name, const MsgPack& prop_obj);
	void process_value(std::string_view prop_name, const MsgPack& prop_obj);
	void process_endpoint(std::string_view prop_name, const MsgPack& prop_obj);
	void process_cast_object(std::string_view prop_name, const MsgPack& prop_obj);
	void process_script(std::string_view prop_name, const MsgPack& prop_obj);
	// Next functions only check the consistency of user provided data.
	void consistency_slot(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_ngram(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_cjk_ngram(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_cjk_words(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_language(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_stop_strategy(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_stem_strategy(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_stem_language(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_type(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_bool_term(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_accuracy(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_partials(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_error(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_dynamic(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_strict(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_date_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_datetime_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_time_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_timedelta_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_numeric_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_geo_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_bool_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_text_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_uuid_detection(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_namespace(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_chai(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_ecma(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_script(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_schema(std::string_view prop_name, const MsgPack& prop_obj);
	void consistency_settings(std::string_view prop_name, const MsgPack& prop_obj);


#ifdef XAPIAND_CHAISCRIPT
	/*
	 * Auxiliar functions for RESERVED_SCRIPT.
	 */

	void write_script(MsgPack& mut_properties);
	void normalize_script();
#endif


	/*
	 * Functions to update default specification for fields.
	 */

	void set_default_spc_id(MsgPack& mut_properties);
	void set_default_spc_version(MsgPack& mut_properties);


	/*
	 * Recursively transforms item_schema into a readable form.
	 */
	static void dispatch_readable(MsgPack& item_schema, bool at_root);

	/*
	 * Tranforms reserved words into a readable form.
	 */

	static bool readable_prefix(MsgPack& prop_prefix, MsgPack& properties);
	static bool readable_slot(MsgPack& prop_prefix, MsgPack& properties);
	static bool readable_stem_language(MsgPack& prop_stem_language, MsgPack& properties);
	static bool readable_acc_prefix(MsgPack& prop_acc_prefix, MsgPack& properties);
	static bool readable_script(MsgPack& prop_script, MsgPack& properties);


	struct dynamic_spc_t {
		const MsgPack* properties;  // Field's properties.
		bool inside_namespace;      // If field is inside a namespace
		std::string prefix;         // Field's prefix.
		bool has_uuid_prefix;       // If prefix has uuid fields.
		std::string acc_field;      // The accuracy field name.
		FieldType acc_field_type;   // The type for accuracy field name.

		dynamic_spc_t(const MsgPack* _properties)
			: properties(_properties),
			  inside_namespace(false),
			  has_uuid_prefix(false),
			  acc_field_type(FieldType::empty) { }
	};


	/*
	 * Returns dynamic_spc_t of full_name.
	 * If full_name is not valid field name throw a ClientError exception.
	 */

	dynamic_spc_t get_dynamic_subproperties(const MsgPack& properties, std::string_view full_name) const;

public:
	Schema(std::shared_ptr<const MsgPack> s, std::unique_ptr<MsgPack> m, std::string o);

	Schema() = delete;
	Schema(Schema&& schema) = delete;
	Schema(const Schema& schema) = delete;
	Schema& operator=(Schema&& schema) = delete;
	Schema& operator=(const Schema& schema) = delete;

	static std::shared_ptr<const MsgPack> get_initial_schema();

	std::shared_ptr<const MsgPack> get_modified_schema();

	std::shared_ptr<const MsgPack> get_const_schema() const;

	void swap(std::unique_ptr<MsgPack>& other) noexcept {
		mut_schema.swap(other);
	}

	template <typename ErrorType>
	static std::pair<const MsgPack*, const MsgPack*> check(const MsgPack& object, const char* prefix, bool allow_foreign, bool allow_root);

	/*
	 * Transforms schema into json string.
	 */
	std::string to_string(bool prettify=false) const;


	/*
	 * Function to index object in doc.
	 */
	std::tuple<std::string, Xapian::Document, MsgPack> index(const MsgPack& object, MsgPack document_id, DatabaseHandler& db_handler, const Data& data);

	/*
	 * Function to update the schema according to obj_schema.
	 */
	bool update(const MsgPack& object);
	bool write(const MsgPack& object, bool replace);

	/*
	 * Returns underlying msgpack schema.
	 */
	const MsgPack& get_schema() const {
		if (mut_schema) {
			return *mut_schema;
		} else {
			return *schema;
		}
	}

	std::string_view get_origin() const {
		return origin;
	}

	/*
	 * Returns full schema.
	 */
	const MsgPack get_full(bool readable = false) const;

	/*
	 * Set default specification in namespace FIELD_ID_NAME
	 */
	static void set_namespace_spc_id(required_spc_t& spc);

	/*
	 * Update namespace specification according to prefix_namespace.
	 */
	template <typename S, typename = std::enable_if_t<std::is_same<std::string, std::decay_t<S>>::value>>
	static required_spc_t get_namespace_specification(FieldType namespace_type, S&& prefix_namespace) {
		L_CALL("Schema::get_namespace_specification('{}', {})", toUType(namespace_type), repr(prefix_namespace));

		auto spc = specification_t::get_global(namespace_type);

		// If the namespace field is ID_FIELD_NAME, restart its default values.
		if (prefix_namespace == NAMESPACE_PREFIX_ID_FIELD_NAME) {
			set_namespace_spc_id(spc);
		} else {
			spc.prefix.field = std::forward<S>(prefix_namespace);
			spc.slot = get_slot(spc.prefix.field, spc.get_ctype());
		}

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
					acc_prefix.insert(0, spc.prefix.field);
				}
				[[fallthrough]];
			default:
				return std::move(spc);
		}
	}

	/*
	 * Returns type, slot and prefix of ID_FIELD_NAME
	 */
	required_spc_t get_data_id() const;

	void set_data_id(const required_spc_t& spc_id);

	/*
	 * Returns data of RESERVED_SCRIPT
	 */
	MsgPack get_data_script() const;

	/*
	 * Functions used for searching, return a field properties.
	 */

	std::pair<required_spc_t, std::string> get_data_field(std::string_view field_name, bool is_range=true) const;
	required_spc_t get_slot_field(std::string_view field_name) const;
};
