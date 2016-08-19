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

#include "schema.h"

#include "database.h"
#include "multivalue/generate_terms.h"
#include "log.h"
#include "manager.h"
#include "serialise.h"
#include "wkt_parser.h"


/*
 * Unordered Maps used for reading user data specification.
 */

static const std::unordered_map<std::string, unitTime> map_acc_date({
	{ "second",     unitTime::SECOND     }, { "minute",  unitTime::MINUTE  },
	{ "hour",       unitTime::HOUR       }, { "day",     unitTime::DAY     },
	{ "month",      unitTime::MONTH      }, { "year",    unitTime::YEAR    },
	{ "decade",     unitTime::DECADE     }, { "century", unitTime::CENTURY },
	{ "millennium", unitTime::MILLENNIUM }
});


static const std::unordered_map<std::string, Xapian::TermGenerator::stem_strategy> map_analyzer({
	{ "stem_none",  Xapian::TermGenerator::STEM_NONE   }, { "none",  Xapian::TermGenerator::STEM_NONE   },
	{ "stem_some",  Xapian::TermGenerator::STEM_SOME   }, { "some",  Xapian::TermGenerator::STEM_SOME   },
	{ "stem_all",   Xapian::TermGenerator::STEM_ALL    }, { "all",   Xapian::TermGenerator::STEM_ALL    },
	{ "stem_all_z", Xapian::TermGenerator::STEM_ALL_Z  }, { "all_z", Xapian::TermGenerator::STEM_ALL_Z  }
});


static const std::unordered_map<std::string, typeIndex> map_index({
	{ "none",          typeIndex::NONE          }, { "terms",        typeIndex::TERMS        },
	{ "values",        typeIndex::VALUES        }, { "all",          typeIndex::ALL          },
	{ "field_terms",   typeIndex::FIELD_TERMS   }, { "field_values", typeIndex::FIELD_VALUES },
	{ "field_all",     typeIndex::FIELD_ALL     }, { "global_terms", typeIndex::GLOBAL_TERMS },
	{ "global_values", typeIndex::GLOBAL_VALUES }, { "global_all",   typeIndex::GLOBAL_ALL   }
});


/*
 * Unordered Maps used for doing the schema readable.
 */

static const std::unordered_map<unitTime, std::string> map_acc_date_inv({
	{ unitTime::SECOND,     "second"     }, { unitTime::MINUTE,  "minute"  },
	{ unitTime::HOUR,       "hour"       }, { unitTime::DAY,     "day"     },
	{ unitTime::MONTH,      "month"      }, { unitTime::YEAR,    "year"    },
	{ unitTime::DECADE,     "decade"     }, { unitTime::CENTURY, "century" },
	{ unitTime::MILLENNIUM, "millennium" }
});


static const std::unordered_map<Xapian::TermGenerator::stem_strategy, std::string> map_analyzer_inv({
	{ Xapian::TermGenerator::STEM_NONE,  "stem_none"   },
	{ Xapian::TermGenerator::STEM_SOME,  "stem_some"   },
	{ Xapian::TermGenerator::STEM_ALL,   "stem_all"    },
	{ Xapian::TermGenerator::STEM_ALL_Z, "stem_all_z"  }
});


static const std::unordered_map<typeIndex, std::string> map_index_inv({
	{ typeIndex::NONE,          "none"          }, { typeIndex::TERMS,        "terms"        },
	{ typeIndex::VALUES,        "values"        }, { typeIndex::ALL,          "all"          },
	{ typeIndex::FIELD_TERMS,   "field_terms"   }, { typeIndex::FIELD_VALUES, "field_values" },
	{ typeIndex::FIELD_ALL,     "field_all"     }, { typeIndex::GLOBAL_TERMS, "global_terms" },
	{ typeIndex::GLOBAL_VALUES, "global_values" }, { typeIndex::GLOBAL_ALL,   "global_all"   }
});


/*
 * Default accuracies.
 */

static const MsgPack def_accuracy_geo  { 0, 5, 10, 15, 20, 25 };
static const MsgPack def_accuracy_num  { 100, 1000, 10000, 100000 };
static const MsgPack def_accuracy_date { "hour", "day", "month", "year", "decade", "century" };


/*
 * Acceptable values string used when there is a data inconsistency.
 */

static const std::string str_set_acc_date("{ second, minute, hour, day, month, year, decade, century, millennium }");
static const std::string str_set_analyzer("{ stem_none, stem_some, stem_all, stem_all_z, none, some, all, all_z }");
static const std::string str_set_index("{ none, terms, values, all, field_terms, field_values, field_all, global_terms, global_values, global_all }");


const specification_t default_spc;


const std::unordered_map<std::string, dispatch_reserved> map_dispatch_document({
	{ RESERVED_WEIGHT,       &Schema::process_weight      },
	{ RESERVED_POSITION,     &Schema::process_position    },
	{ RESERVED_LANGUAGE,     &Schema::process_language    },
	{ RESERVED_SPELLING,     &Schema::process_spelling    },
	{ RESERVED_POSITIONS,    &Schema::process_positions   },
	{ RESERVED_ACCURACY,     &Schema::process_accuracy    },
	{ RESERVED_ACC_PREFIX,   &Schema::process_acc_prefix  },
	{ RESERVED_ACC_GPREFIX,  &Schema::process_acc_gprefix },
	{ RESERVED_STORE,        &Schema::process_store       },
	{ RESERVED_TYPE,         &Schema::process_type        },
	{ RESERVED_ANALYZER,     &Schema::process_analyzer    },
	{ RESERVED_DYNAMIC,      &Schema::process_dynamic     },
	{ RESERVED_D_DETECTION,  &Schema::process_d_detection },
	{ RESERVED_N_DETECTION,  &Schema::process_n_detection },
	{ RESERVED_G_DETECTION,  &Schema::process_g_detection },
	{ RESERVED_B_DETECTION,  &Schema::process_b_detection },
	{ RESERVED_S_DETECTION,  &Schema::process_s_detection },
	{ RESERVED_BOOL_TERM,    &Schema::process_bool_term   },
	{ RESERVED_VALUE,        &Schema::process_value       },
	{ RESERVED_NAME,         &Schema::process_name        },
	{ RESERVED_SLOT,         &Schema::process_slot        },
	{ RESERVED_INDEX,        &Schema::process_index       },
	{ RESERVED_PREFIX,       &Schema::process_prefix      },
	{ RESERVED_PARTIALS,     &Schema::process_partials    },
	{ RESERVED_ERROR,        &Schema::process_error       },
	{ RESERVED_LATITUDE,     &Schema::process_latitude    },
	{ RESERVED_LONGITUDE,    &Schema::process_longitude   },
	{ RESERVED_RADIUS,       &Schema::process_radius      }
});


const std::unordered_map<std::string, dispatch_reserved> map_dispatch_properties({
	{ RESERVED_WEIGHT,       &Schema::update_weight       },
	{ RESERVED_POSITION,     &Schema::update_position     },
	{ RESERVED_LANGUAGE,     &Schema::update_language     },
	{ RESERVED_SPELLING,     &Schema::update_spelling     },
	{ RESERVED_POSITIONS,    &Schema::update_positions    },
	{ RESERVED_ACCURACY,     &Schema::update_accuracy     },
	{ RESERVED_ACC_PREFIX,   &Schema::update_acc_prefix   },
	{ RESERVED_ACC_GPREFIX,  &Schema::update_acc_gprefix  },
	{ RESERVED_STORE,        &Schema::update_store        },
	{ RESERVED_TYPE,         &Schema::update_type         },
	{ RESERVED_ANALYZER,     &Schema::update_analyzer     },
	{ RESERVED_DYNAMIC,      &Schema::update_dynamic      },
	{ RESERVED_D_DETECTION,  &Schema::update_d_detection  },
	{ RESERVED_N_DETECTION,  &Schema::update_n_detection  },
	{ RESERVED_G_DETECTION,  &Schema::update_g_detection  },
	{ RESERVED_B_DETECTION,  &Schema::update_b_detection  },
	{ RESERVED_S_DETECTION,  &Schema::update_s_detection  },
	{ RESERVED_BOOL_TERM,    &Schema::update_bool_term    },
	{ RESERVED_SLOT,         &Schema::update_slot         },
	{ RESERVED_INDEX,        &Schema::update_index        },
	{ RESERVED_PREFIX,       &Schema::update_prefix       },
	{ RESERVED_PARTIALS,     &Schema::update_partials     },
	{ RESERVED_ERROR,        &Schema::update_error        }
});


const std::unordered_map<std::string, dispatch_root> map_dispatch_root({
	{ RESERVED_DATA,           &Schema::process_data           },
	{ RESERVED_VALUES,         &Schema::process_values         },
	{ RESERVED_FIELD_VALUES,   &Schema::process_field_values   },
	{ RESERVED_GLOBAL_VALUES,  &Schema::process_global_values  },
	{ RESERVED_TERMS,          &Schema::process_terms          },
	{ RESERVED_FIELD_TERMS,    &Schema::process_field_terms    },
	{ RESERVED_GLOBAL_TERMS,   &Schema::process_global_terms   },
	{ RESERVED_FIELD_ALL,      &Schema::process_field_all      },
	{ RESERVED_GLOBAL_ALL,     &Schema::process_global_all     },
	{ RESERVED_NONE,           &Schema::process_none           }
});


const std::unordered_map<std::string, dispatch_readable> map_dispatch_readable({
	{ RESERVED_TYPE,         &Schema::readable_type     },
	{ RESERVED_ANALYZER,     &Schema::readable_analyzer },
	{ RESERVED_INDEX,        &Schema::readable_index    }
});


inline static constexpr auto getPos(size_t pos, size_t size) noexcept {
	return pos < size ? pos : size - 1;
};


specification_t::specification_t()
	: position({ 0 }),
	  weight({ 1 }),
	  language({ DEFAULT_LANGUAGE }),
	  spelling({ false }),
	  positions({ false }),
	  analyzer({ Xapian::TermGenerator::STEM_SOME }),
	  sep_types({ NO_TYPE, NO_TYPE, NO_TYPE }),
	  slot(Xapian::BAD_VALUENO),
	  index(typeIndex::ALL),
	  store(true),
	  dynamic(true),
	  date_detection(true),
	  numeric_detection(true),
	  geo_detection(true),
	  bool_detection(true),
	  string_detection(true),
	  bool_term(false),
	  found_field(true),
	  set_type(false),
	  set_bool_term(false),
	  fixed_index(false),
	  partials(GEO_DEF_PARTIALS),
	  error(GEO_DEF_ERROR) { }


specification_t::specification_t(const specification_t& o)
	: position(o.position),
	  weight(o.weight),
	  language(o.language),
	  spelling(o.spelling),
	  positions(o.positions),
	  analyzer(o.analyzer),
	  sep_types(o.sep_types),
	  prefix(o.prefix),
	  slot(o.slot),
	  index(o.index),
	  accuracy(o.accuracy),
	  acc_prefix(o.acc_prefix),
	  acc_gprefix(o.acc_gprefix),
	  store(o.store),
	  dynamic(o.dynamic),
	  date_detection(o.date_detection),
	  numeric_detection(o.numeric_detection),
	  geo_detection(o.geo_detection),
	  bool_detection(o.bool_detection),
	  string_detection(o.string_detection),
	  bool_term(o.bool_term),
	  full_name(o.full_name),
	  found_field(o.found_field),
	  set_type(o.set_type),
	  set_bool_term(o.set_bool_term),
	  fixed_index(o.fixed_index),
	  partials(o.partials),
	  error(o.error) { }


specification_t::specification_t(specification_t&& o) noexcept
	: position(std::move(o.position)),
	  weight(std::move(o.weight)),
	  language(std::move(o.language)),
	  spelling(std::move(o.spelling)),
	  positions(std::move(o.positions)),
	  analyzer(std::move(o.analyzer)),
	  sep_types(std::move(o.sep_types)),
	  prefix(std::move(o.prefix)),
	  slot(std::move(o.slot)),
	  index(std::move(o.index)),
	  accuracy(std::move(o.accuracy)),
	  acc_prefix(std::move(o.acc_prefix)),
	  acc_gprefix(std::move(o.acc_gprefix)),
	  store(std::move(o.store)),
	  dynamic(std::move(o.dynamic)),
	  date_detection(std::move(o.date_detection)),
	  numeric_detection(std::move(o.numeric_detection)),
	  geo_detection(std::move(o.geo_detection)),
	  bool_detection(std::move(o.bool_detection)),
	  string_detection(std::move(o.string_detection)),
	  bool_term(std::move(o.bool_term)),
	  full_name(std::move(o.full_name)),
	  found_field(std::move(o.found_field)),
	  set_type(std::move(o.set_type)),
	  set_bool_term(std::move(o.set_bool_term)),
	  fixed_index(std::move(o.fixed_index)),
	  partials(std::move(o.partials)),
	  error(std::move(o.error)) { }


specification_t&
specification_t::operator=(const specification_t& o)
{
	position = o.position;
	weight = o.weight;
	language = o.language;
	spelling = o.spelling;
	positions = o.positions;
	analyzer = o.analyzer;
	sep_types = o.sep_types;
	prefix = o.prefix;
	slot = o.slot;
	index = o.index;
	accuracy = o.accuracy;
	acc_prefix = o.acc_prefix;
	acc_gprefix = o.acc_gprefix;
	store = o.store;
	dynamic = o.dynamic;
	date_detection = o.date_detection;
	numeric_detection = o.numeric_detection;
	geo_detection = o.geo_detection;
	bool_detection = o.bool_detection;
	string_detection = o.string_detection;
	bool_term = o.bool_term;
	value.reset();
	value_rec.reset();
	doc_acc.reset();
	name = o.name;
	full_name = o.full_name;
	found_field = o.found_field;
	set_type = o.set_type;
	set_bool_term = o.set_bool_term;
	fixed_index = o.fixed_index;
	partials = o.partials;
	error = o.error;
	return *this;
}


specification_t&
specification_t::operator=(specification_t&& o) noexcept
{
	position = std::move(o.position);
	weight = std::move(o.weight);
	language = std::move(o.language);
	spelling = std::move(o.spelling);
	positions = std::move(o.positions);
	analyzer = std::move(o.analyzer);
	sep_types = std::move(o.sep_types);
	prefix = std::move(o.prefix);
	slot = std::move(o.slot);
	index = std::move(o.index);
	accuracy = std::move(o.accuracy);
	acc_prefix = std::move(o.acc_prefix);
	acc_gprefix = std::move(o.acc_gprefix);
	store = std::move(o.store);
	dynamic = std::move(o.dynamic);
	date_detection = std::move(o.date_detection);
	numeric_detection = std::move(o.numeric_detection);
	geo_detection = std::move(o.geo_detection);
	bool_detection = std::move(o.bool_detection);
	string_detection = std::move(o.string_detection);
	bool_term = std::move(o.bool_term);
	value.reset();
	value_rec.reset();
	doc_acc.reset();
	name = std::move(o.name);
	full_name = std::move(o.full_name);
	found_field = std::move(o.found_field);
	set_type = std::move(o.set_type);
	set_bool_term = std::move(o.set_bool_term);
	fixed_index = std::move(o.fixed_index);
	partials = std::move(o.partials);
	error = std::move(o.error);
	return *this;
}


std::string
specification_t::to_string() const
{
	std::stringstream str;
	str << "\n{\n";
	str << "\t" << RESERVED_NAME << ": " << full_name << "\n";
	str << "\t" << RESERVED_POSITION << ": [ ";
	for (const auto& _position : position) {
		str << _position << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_WEIGHT   << ": [ ";
	for (const auto& _w : weight) {
		str << _w << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_LANGUAGE << ": [ ";
	for (const auto& lan : language) {
		str << lan << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_SPELLING << ": [ ";
	for (const auto& spel : spelling) {
		str << (spel ? "true" : "false") << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_POSITIONS<< ": [ ";
	for (const auto& _positions : positions) {
		str << (_positions ? "true" : "false") << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_ANALYZER    << ": [ ";
	for (const auto& _analyzer : analyzer) {
		str << map_analyzer_inv.at(_analyzer) << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_ACCURACY << ": [ ";
	for (const auto& acc : accuracy) {
		str << acc << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_ACC_PREFIX  << ": [ ";
	for (const auto& acc_p : acc_prefix) {
		str << acc_p << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_SLOT        << ": " << slot                    << "\n";
	str << "\t" << RESERVED_TYPE        << ": " << str_type(sep_types)     << "\n";
	str << "\t" << RESERVED_PREFIX      << ": " << prefix                  << "\n";
	str << "\t" << RESERVED_INDEX       << ": " << map_index_inv.at(index) << "\n";
	str << "\t" << RESERVED_STORE       << ": " << (store             ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DYNAMIC     << ": " << (dynamic           ? "true" : "false") << "\n";
	str << "\t" << RESERVED_D_DETECTION << ": " << (date_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_N_DETECTION << ": " << (numeric_detection ? "true" : "false") << "\n";
	str << "\t" << RESERVED_G_DETECTION << ": " << (geo_detection     ? "true" : "false") << "\n";
	str << "\t" << RESERVED_B_DETECTION << ": " << (bool_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_S_DETECTION << ": " << (string_detection  ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_TERM   << ": " << (bool_term         ? "true" : "false") << "\n}\n";

	return str.str();
}


Schema::Schema(const std::shared_ptr<const MsgPack>& other)
	: schema(other)
{
	if (schema->is_null()) {
		MsgPack new_schema = {
			{ RESERVED_VERSION, DB_VERSION_SCHEMA },
			{ RESERVED_SCHEMA, nullptr },
		};
		new_schema.lock();
		schema = std::make_shared<const MsgPack>(std::move(new_schema));
	} else {
		try {
			const auto& version = schema->at(RESERVED_VERSION);
			if (version.as_f64() != DB_VERSION_SCHEMA) {
				throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
			}
		} catch (const std::out_of_range&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		}
	}
}


MsgPack&
Schema::get_mutable(const std::string& full_name)
{
	L_CALL(this, "Schema::get_mutable()");

	if (!mut_schema) {
		mut_schema = std::make_unique<MsgPack>(*schema);
	}

	MsgPack* prop = &mut_schema->at(RESERVED_SCHEMA);
	std::vector<std::string> field_names;
	stringTokenizer(full_name, DB_OFFSPRING_UNION, field_names);
	for (const auto& field_name : field_names) {
		prop = &(*prop)[field_name];
	}
	return *prop;
}


std::string
Schema::serialise_id(const MsgPack& properties, const std::string& value_id)
{
	L_CALL(this, "Schema::serialise_id()");

	specification.set_type = true;
	try {
		const auto& prop_id = properties.at(RESERVED_ID);
		update_specification(properties);
		return Serialise::serialise(static_cast<char>(prop_id.at(RESERVED_TYPE).at(2).as_u64()), value_id);
	} catch (const std::out_of_range&) {
		auto& prop_id = get_mutable(RESERVED_ID);
		specification.found_field = false;
		auto res_serialise = Serialise::serialise(value_id);
		prop_id[RESERVED_TYPE] = std::vector<unsigned>({ NO_TYPE, NO_TYPE, static_cast<unsigned>(res_serialise.first) });
		prop_id[RESERVED_PREFIX] = DOCUMENT_ID_TERM_PREFIX;
		prop_id[RESERVED_SLOT] = DB_SLOT_ID;
		prop_id[RESERVED_BOOL_TERM] = true;
		prop_id[RESERVED_INDEX] = typeIndex::ALL;
		return res_serialise.second;
	}
}


void
Schema::update_specification(const MsgPack& properties)
{
	L_CALL(this, "Schema::update_specification()");

	for (const auto& property : properties) {
		auto str_prop = property.as_string();
		try {
			auto func = map_dispatch_properties.at(str_prop);
			(this->*func)(properties.at(str_prop));
		} catch (const std::out_of_range&) { }
	}
}


void
Schema::restart_specification()
{
	L_CALL(this, "Schema::restart_specification()");

	specification.sep_types = default_spc.sep_types;
	specification.accuracy.clear();
	specification.acc_prefix.clear();
	specification.acc_gprefix.clear();
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;
	specification.bool_term = default_spc.bool_term;
	specification.set_type = default_spc.set_type;
	specification.name = default_spc.name;
	specification.partials = default_spc.partials;
	specification.error = default_spc.error;
}


const MsgPack&
Schema::get_subproperties(const MsgPack& properties)
{
	L_CALL(this, "Schema::get_subproperties()");

	std::vector<std::string> field_names;
	stringTokenizer(specification.name, DB_OFFSPRING_UNION, field_names);

	const MsgPack * subproperties = &properties;
	for (const auto& field_name : field_names) {
		if (!is_valid(field_name)) {
			throw MSG_ClientError("The field name: %s (%s) is not valid", specification.name.c_str(), field_name.c_str());
		}
		restart_specification();
		try {
			subproperties = &subproperties->at(field_name);
			specification.found_field = true;
			update_specification(*subproperties);
		} catch (const std::out_of_range&) {
			subproperties = &get_mutable(specification.full_name);
			specification.found_field = false;
		}
	}

	return *subproperties;
}


void
Schema::set_type(const MsgPack& item_doc)
{
	L_CALL(this, "Schema::set_type()");

	const auto& field = item_doc.is_array() ? item_doc.at(0) : item_doc;
	switch (field.type()) {
		case msgpack::type::POSITIVE_INTEGER:
			if (specification.numeric_detection) {
				specification.sep_types[2] = POSITIVE_TYPE;
				return;
			}
			break;
		case msgpack::type::NEGATIVE_INTEGER:
			if (specification.numeric_detection) {
				specification.sep_types[2] = INTEGER_TYPE;
				return;
			}
			break;
		case msgpack::type::FLOAT:
			if (specification.numeric_detection) {
				specification.sep_types[2] = FLOAT_TYPE;
				return;
			}
			break;
		case msgpack::type::BOOLEAN:
			if (specification.bool_detection) {
				specification.sep_types[2] = BOOLEAN_TYPE;
				return;
			}
			break;
		case msgpack::type::STR: {
			auto str_value(field.as_string());
			if (specification.date_detection && Datetime::isDate(str_value)) {
				specification.sep_types[2] = DATE_TYPE;
				return;
			} else if (specification.geo_detection && EWKT_Parser::isEWKT(str_value)) {
				specification.sep_types[2] = GEO_TYPE;
				return;
			} else if (specification.string_detection) {
				specification.sep_types[2] = STRING_TYPE;
				return;
			} else if (specification.bool_detection) {
				try {
					Serialise::boolean(str_value);
					specification.sep_types[2] = BOOLEAN_TYPE;
					return;
				} catch (const std::exception&) { }
			}
			break;
		}
		case msgpack::type::ARRAY:
			throw MSG_ClientError("%s can not be array of arrays", RESERVED_VALUE);
		case msgpack::type::MAP:
			throw MSG_ClientError("%s can not be object", RESERVED_VALUE);
		case msgpack::type::NIL:
			// Do not process this field.
			throw MSG_DummyException();
		default:
			break;
	}

	throw MSG_ClientError("%s: %s is ambiguous", RESERVED_VALUE, item_doc.to_string().c_str());
}


void
Schema::set_type_to_array()
{
	L_CALL(this, "Schema::set_type_to_array()");

	auto& _types = get_mutable(specification.full_name)[RESERVED_TYPE];
	if (_types.is_null()) {
		_types = MsgPack({ NO_TYPE, ARRAY_TYPE, NO_TYPE });
		specification.sep_types[1] = ARRAY_TYPE;
	} else {
		_types[1] = ARRAY_TYPE;
		specification.sep_types[1] = ARRAY_TYPE;
	}
}


void
Schema::set_type_to_object()
{
	L_CALL(this, "Schema::set_type_to_object()");

	auto& _types = get_mutable(specification.full_name)[RESERVED_TYPE];
	if (_types.is_null()) {
		_types = MsgPack({ OBJECT_TYPE, NO_TYPE, NO_TYPE });
		specification.sep_types[0] = OBJECT_TYPE;
	} else {
		_types[0] = OBJECT_TYPE;
		specification.sep_types[0] = OBJECT_TYPE;
	}
}


std::string
Schema::to_string(bool prettify) const
{
	L_CALL(this, "Schema::to_string()");

	return get_readable().to_string(prettify);
}


const MsgPack
Schema::get_readable() const
{
	L_CALL(this, "Schema::get_readable()");

	auto schema_readable = mut_schema ? *mut_schema : *schema;
	auto& properties = schema_readable.at(RESERVED_SCHEMA);
	if unlikely(properties.is_null()) {
		schema_readable.erase(RESERVED_SCHEMA);
	} else {
		readable(properties, true);
	}

	return schema_readable;
}


void
Schema::readable(MsgPack& item_schema, bool is_root)
{
	// Change this item of schema in readable form.
	for (const auto& item_key : item_schema) {
		auto str_key = item_key.as_string();
		try {
			auto func = map_dispatch_readable.at(str_key);
			(*func)(item_schema.at(str_key), item_schema);
		} catch (const std::out_of_range&) {
			if (is_valid(str_key) || (is_root && str_key == RESERVED_ID)) {
				auto& sub_item = item_schema.at(str_key);
				if unlikely(sub_item.is_null()) {
					item_schema.erase(str_key);
				} else {
					readable(sub_item);
				}
			}
		}
	}
}


void
Schema::readable_type(MsgPack& prop_type, MsgPack& properties)
{
	std::vector<unsigned> sep_types({
		static_cast<unsigned>(prop_type.at(0).as_u64()),
		static_cast<unsigned>(prop_type.at(1).as_u64()),
		static_cast<unsigned>(prop_type.at(2).as_u64())
	});
	prop_type = str_type(sep_types);

	// Readable accuracy.
	if (sep_types[2] == DATE_TYPE) {
		for (auto& _accuracy : properties.at(RESERVED_ACCURACY)) {
			_accuracy = map_acc_date_inv.at((unitTime)_accuracy.as_u64());
		}
	}
}


void
Schema::readable_analyzer(MsgPack& prop_analyzer, MsgPack&)
{
	for (auto& _analyzer : prop_analyzer) {
		_analyzer = map_analyzer_inv.at((Xapian::TermGenerator::stem_strategy)_analyzer.as_u64());
	}
}


void
Schema::readable_index(MsgPack& prop_index, MsgPack&)
{
	prop_index = map_index_inv.at((typeIndex)prop_index.as_u64());
}


void
Schema::process_position(const MsgPack& doc_position)
{
	// RESERVED_POSITION is heritable and can change between documents.
	try {
		specification.position.clear();
		if (doc_position.is_array()) {
			for (const auto& _position : doc_position) {
				specification.position.push_back(static_cast<unsigned>(_position.as_u64()));
			}
		} else {
			specification.position.push_back(static_cast<unsigned>(doc_position.as_u64()));
		}

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_POSITION] = specification.position;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_POSITION);
	}
}


void
Schema::process_weight(const MsgPack& doc_weight)
{
	// RESERVED_WEIGHT is heritable and can change between documents.
	try {
		specification.weight.clear();
		if (doc_weight.is_array()) {
			for (const auto& _weight : doc_weight) {
				specification.weight.push_back(static_cast<unsigned>(_weight.as_u64()));
			}
		} else {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.as_u64()));
		}

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_WEIGHT] = specification.weight;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_WEIGHT);
	}
}


void
Schema::process_language(const MsgPack& doc_language)
{
	// RESERVED_LANGUAGE is heritable and can change between documents.
	try {
		specification.language.clear();
		if (doc_language.is_array()) {
			for (const auto& _language : doc_language) {
				auto _str_language(_language.as_string());
				if (is_language(_str_language)) {
					specification.language.push_back(std::move(_str_language));
				} else {
					throw MSG_ClientError("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
				}
			}
		} else {
			auto _str_language(doc_language.as_string());
			if (is_language(_str_language)) {
				specification.language.push_back(std::move(_str_language));
			} else {
				throw MSG_ClientError("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
			}
		}

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_LANGUAGE] = specification.language;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string or array of strings", RESERVED_LANGUAGE);
	}
}


void
Schema::process_spelling(const MsgPack& doc_spelling)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	try {
		specification.spelling.clear();
		if (doc_spelling.is_array()) {
			for (const auto& _spelling : doc_spelling) {
				specification.spelling.push_back(_spelling.as_bool());
			}
		} else {
			specification.spelling.push_back(doc_spelling.as_bool());
		}

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_SPELLING] = specification.spelling;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean or array of booleans", RESERVED_SPELLING);
	}
}


void
Schema::process_positions(const MsgPack& doc_positions)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	try {
		specification.positions.clear();
		if (doc_positions.is_array()) {
			for (const auto& _positions : doc_positions) {
				specification.positions.push_back(_positions.as_bool());
			}
		} else {
			specification.positions.push_back(doc_positions.as_bool());
		}

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_POSITIONS] = specification.positions;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean or array of booleans", RESERVED_POSITIONS);
	}
}


void
Schema::process_analyzer(const MsgPack& doc_analyzer)
{
	// RESERVED_ANALYZER is heritable and can change between documents.
	try {
		specification.analyzer.clear();
		if (doc_analyzer.is_array()) {
			for (const auto& analyzer : doc_analyzer) {
				auto _analyzer(lower_string(analyzer.as_string()));
				try {
					specification.analyzer.push_back(map_analyzer.at(_analyzer));
				} catch (const std::out_of_range&) {
					throw MSG_ClientError("%s can be in %s (%s not supported)", RESERVED_ANALYZER, str_set_analyzer.c_str(), _analyzer.c_str());
				}
			}
		} else {
			auto _analyzer(lower_string(doc_analyzer.as_string()));
			try {
				specification.analyzer.push_back(map_analyzer.at(_analyzer));
			} catch (const std::out_of_range&) {
				throw MSG_ClientError("%s can be in %s (%s not supported)", RESERVED_ANALYZER, str_set_analyzer.c_str(), _analyzer.c_str());
			}
		}

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_ANALYZER] = specification.analyzer;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string or array of strings", RESERVED_ANALYZER);
	}
}


void
Schema::process_type(const MsgPack& doc_type)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		if (!set_types(lower_string(doc_type.as_string()), specification.sep_types)) {
			throw MSG_ClientError("%s can be [object/][array/]< %s | %s | %s | %s | %s >", RESERVED_TYPE, FLOAT_STR, INTEGER_STR, POSITIVE_STR, STRING_STR, DATE_STR, BOOLEAN_STR, GEO_STR);
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_TYPE);
	}
}


void
Schema::process_accuracy(const MsgPack& doc_accuracy)
{
	// RESERVED_ACCURACY isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	if (doc_accuracy.is_array()) {
		try {
			specification.doc_acc = std::make_unique<const MsgPack>(doc_accuracy);
		} catch (const msgpack::type_error&) {
			throw MSG_ClientError("Data inconsistency, %s must be array", RESERVED_ACCURACY);
		}
	} else {
		throw MSG_ClientError("Data inconsistency, %s must be array", RESERVED_ACCURACY);
	}
}


void
Schema::process_acc_prefix(const MsgPack& doc_acc_prefix)
{
	// RESERVED_ACC_PREFIX isn't heritable and can't change once fixed.
	// It is taken into account only if RESERVED_ACCURACY is defined.
	if likely(specification.set_type) {
		return;
	}

	if (doc_acc_prefix.is_array()) {
		std::unordered_set<std::string> uset_acc_prefix;
		uset_acc_prefix.reserve(doc_acc_prefix.size());
		specification.acc_prefix.reserve(doc_acc_prefix.size());
		try {
			for (const auto& _acc_prefix : doc_acc_prefix) {
				auto prefix = _acc_prefix.as_string();
				if (uset_acc_prefix.insert(prefix).second) {
					specification.acc_prefix.push_back(std::move(prefix));
				}
			}
		} catch (const msgpack::type_error&) {
			throw MSG_ClientError("Data inconsistency, %s must be an array of strings", RESERVED_ACC_PREFIX);
		}
	} else {
		throw MSG_ClientError("Data inconsistency, %s must be an array of strings", RESERVED_ACC_PREFIX);
	}
}


void
Schema::process_acc_gprefix(const MsgPack& doc_acc_gprefix)
{
	// RESERVED_ACC_GPREFIX isn't heritable and can't change once fixed.
	// It is taken into account only if RESERVED_ACCURACY is defined.
	if likely(specification.set_type) {
		return;
	}

	if (doc_acc_gprefix.is_array()) {
		std::unordered_set<std::string> uset_acc_gprefix;
		uset_acc_gprefix.reserve(doc_acc_gprefix.size());
		specification.acc_gprefix.reserve(doc_acc_gprefix.size());
		try {
			for (const auto& _acc_gprefix : doc_acc_gprefix) {
				auto gprefix = _acc_gprefix.as_string();
				if (uset_acc_gprefix.insert(gprefix).second) {
					specification.acc_gprefix.push_back(std::move(gprefix));
				}
			}
		} catch (const msgpack::type_error&) {
			throw MSG_ClientError("Data inconsistency, %s must be an array of strings", RESERVED_ACC_GPREFIX);
		}
	} else {
		throw MSG_ClientError("Data inconsistency, %s must be an array of strings", RESERVED_ACC_GPREFIX);
	}
}


void
Schema::process_prefix(const MsgPack& doc_prefix)
{
	// RESERVED_PREFIX isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.prefix = doc_prefix.as_string();
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_PREFIX);
	}
}


void
Schema::process_slot(const MsgPack& doc_slot)
{
	// RESERVED_SLOT isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.slot = static_cast<unsigned>(doc_slot.as_u64());
		if (specification.slot < DB_SLOT_RESERVED) {
			specification.slot += DB_SLOT_RESERVED;
		} else if (specification.slot == Xapian::BAD_VALUENO) {
			specification.slot = 0xfffffffe;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be a positive integer", RESERVED_SLOT);
	}
}


void
Schema::process_index(const MsgPack& doc_index)
{
	// RESERVED_INDEX is heritable and can change.
	try {
		auto str_index = lower_string(doc_index.as_string());
		try {
			specification.index = map_index.at(str_index);

			if unlikely(!specification.found_field) {
				get_mutable(specification.full_name)[RESERVED_INDEX] = specification.index;
			}
		} catch (const std::out_of_range&) {
			throw MSG_ClientError("%s must be in %s (%s not supported)", RESERVED_INDEX, str_set_index.c_str(), str_index.c_str());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_INDEX);
	}
}


void
Schema::process_store(const MsgPack& doc_store)
{
	/*
	 * RESERVED_STORE is heritable and can change, but once fixed in false
	 * it cannot change in its offsprings.
	 */
	try {
		specification.store = doc_store.as_bool() && specification.store;

		if unlikely(!specification.found_field) {
			get_mutable(specification.full_name)[RESERVED_STORE] = specification.store;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_STORE);
	} catch (const std::out_of_range&) { }
}


void
Schema::process_dynamic(const MsgPack& doc_dynamic)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.dynamic = doc_dynamic.as_bool();
		get_mutable(specification.full_name)[RESERVED_DYNAMIC] = specification.dynamic;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_DYNAMIC);
	} catch (const std::out_of_range&) { }
}


void
Schema::process_d_detection(const MsgPack& doc_d_detection)
{
	// RESERVED_D_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.date_detection = doc_d_detection.as_bool();
		get_mutable(specification.full_name)[RESERVED_D_DETECTION] = specification.date_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_D_DETECTION);
	}
}


void
Schema::process_n_detection(const MsgPack& doc_n_detection)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.numeric_detection = doc_n_detection.as_bool();
		get_mutable(specification.full_name)[RESERVED_N_DETECTION] = specification.numeric_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_N_DETECTION);
	}
}


void
Schema::process_g_detection(const MsgPack& doc_g_detection)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.geo_detection = doc_g_detection.as_bool();
		get_mutable(specification.full_name)[RESERVED_G_DETECTION] = specification.geo_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_G_DETECTION);
	}
}


void
Schema::process_b_detection(const MsgPack& doc_b_detection)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.bool_detection = doc_b_detection.as_bool();
		get_mutable(specification.full_name)[RESERVED_B_DETECTION] = specification.bool_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_B_DETECTION);
	}
}


void
Schema::process_s_detection(const MsgPack& doc_s_detection)
{
	// RESERVED_S_DETECTION isn't heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.string_detection = doc_s_detection.as_bool();
		get_mutable(specification.full_name)[RESERVED_S_DETECTION] = specification.string_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_S_DETECTION);
	}
}


void
Schema::process_bool_term(const MsgPack& doc_bool_term)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.bool_term = doc_bool_term.as_bool();
		specification.set_bool_term = true;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be a boolean", RESERVED_BOOL_TERM);
	}
}


void
Schema::process_partials(const MsgPack& doc_partials)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.partials = doc_partials.as_bool();
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_PARTIALS);
	}
}


void
Schema::process_error(const MsgPack& doc_error)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.error = doc_error.as_f64();
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be a double", RESERVED_ERROR);
	}
}


void
Schema::process_latitude(const MsgPack& doc_latitude)
{
	// RESERVED_LATITUDE isn't heritable and is not saved in schema.
	if (!specification.value_rec) {
		specification.value_rec = std::make_unique<MsgPack>();
	}
	(*specification.value_rec)[RESERVED_LATITUDE] = doc_latitude;
}


void
Schema::process_longitude(const MsgPack& doc_longitude)
{
	// RESERVED_LONGITUDE isn't heritable and is not saved in schema.
	if (!specification.value_rec) {
		specification.value_rec = std::make_unique<MsgPack>();
	}
	(*specification.value_rec)[RESERVED_LONGITUDE] = doc_longitude;
}


void
Schema::process_radius(const MsgPack& doc_radius)
{
	// RESERVED_RADIUS isn't heritable and is not saved in schema.
	if (!specification.value_rec) {
		specification.value_rec = std::make_unique<MsgPack>();
	}
	(*specification.value_rec)[RESERVED_RADIUS] = doc_radius;
}


void
Schema::process_value(const MsgPack& doc_value)
{
	// RESERVED_VALUE isn't heritable and is not saved in schema.
	specification.value = std::make_unique<const MsgPack>(doc_value);
}


void
Schema::process_name(const MsgPack& doc_name)
{
	// RESERVED_NAME isn't heritable and is not saved in schema.
	try {
		specification.name.assign(doc_name.as_string());
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_NAME);
	}
}


MsgPack
Schema::index(const MsgPack& properties, const MsgPack& object, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index()");

	try {
		MsgPack data;
		TaskVector tasks;
		tasks.reserve(object.size());
		for (const auto& item_key : object) {
			const auto str_key = item_key.as_string();
			try {
				auto func = map_dispatch_document.at(str_key);
				(this->*func)(object.at(str_key));
			} catch (const std::out_of_range&) {
				if (is_valid(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data), std::ref(doc), std::move(str_key)));
				} else {
					try {
						auto func = map_dispatch_root.at(str_key);
						tasks.push_back(std::async(std::launch::deferred, func, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data), std::ref(doc)));
					} catch (const std::out_of_range&) { }
				}
			}
		}

		restart_specification();
		const specification_t spc_start = std::move(specification);
		for (auto& task : tasks) {
			specification = spc_start;
			task.get();
		}

		for (const auto& elem : map_values) {
			auto val_ser = elem.second.serialise();
			doc.add_value(elem.first, val_ser);
			L_INDEX(this, "Slot: %zu  Values: %s", elem.first, repr(val_ser).c_str());
		}

		return data;
	} catch (...) {
		mut_schema.reset();
		throw;
	}
}


void
Schema::process_data(const MsgPack&, const MsgPack& doc_data, MsgPack& data, Xapian::Document&)
{
	L_CALL(this, "Schema::process_data()");

	data[RESERVED_DATA] = doc_data;
}


void
Schema::process_values(const MsgPack& properties, const MsgPack& doc_values, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_values()");

	specification.index = typeIndex::VALUES;
	fixed_index(properties, doc_values, specification.store ? data[RESERVED_VALUES] : data, doc, RESERVED_VALUES);
}


void
Schema::process_field_values(const MsgPack& properties, const MsgPack& doc_values, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_field_values()");

	specification.index = typeIndex::FIELD_VALUES;
	fixed_index(properties, doc_values, specification.store ? data[RESERVED_FIELD_VALUES] : data, doc, RESERVED_FIELD_VALUES);
}


void
Schema::process_global_values(const MsgPack& properties, const MsgPack& doc_values, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_global_values()");

	specification.index = typeIndex::GLOBAL_VALUES;
	fixed_index(properties, doc_values, specification.store ? data[RESERVED_GLOBAL_VALUES] : data, doc, RESERVED_GLOBAL_VALUES);
}


void
Schema::process_terms(const MsgPack& properties, const MsgPack& doc_terms, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_terms()");

	specification.index = typeIndex::TERMS;
	fixed_index(properties, doc_terms, specification.store ? data[RESERVED_TERMS] : data, doc, RESERVED_TERMS);
}


void
Schema::process_field_terms(const MsgPack& properties, const MsgPack& doc_terms, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_field_terms()");

	specification.index = typeIndex::FIELD_TERMS;
	fixed_index(properties, doc_terms, specification.store ? data[RESERVED_FIELD_TERMS] : data, doc, RESERVED_FIELD_TERMS);
}


void
Schema::process_global_terms(const MsgPack& properties, const MsgPack& doc_terms, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_global_terms()");

	specification.index = typeIndex::GLOBAL_TERMS;
	fixed_index(properties, doc_terms, specification.store ? data[RESERVED_GLOBAL_TERMS] : data, doc, RESERVED_GLOBAL_TERMS);
}


void
Schema::process_field_all(const MsgPack& properties, const MsgPack& doc_values, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_field_all()");

	specification.index = typeIndex::FIELD_ALL;
	fixed_index(properties, doc_values, specification.store ? data[RESERVED_FIELD_ALL] : data, doc, RESERVED_FIELD_ALL);
}


void
Schema::process_global_all(const MsgPack& properties, const MsgPack& doc_values, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_global_all()");

	specification.index = typeIndex::GLOBAL_ALL;
	fixed_index(properties, doc_values, specification.store ? data[RESERVED_GLOBAL_ALL] : data, doc, RESERVED_GLOBAL_ALL);
}


void
Schema::process_none(const MsgPack& properties, const MsgPack& doc_values, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_none()");

	specification.index = typeIndex::NONE;
	fixed_index(properties, doc_values, specification.store ? data[RESERVED_NONE] : data, doc, RESERVED_NONE);
}


void
Schema::fixed_index(const MsgPack& properties, const MsgPack& object, MsgPack& data, Xapian::Document& doc, const char* reserved_word)
{
	specification.fixed_index = true;
	switch (object.type()) {
		case msgpack::type::MAP:
			return index_object(properties, object, data, doc);
		case msgpack::type::ARRAY:
			return index_array(properties, object, data, doc);
		default:
			throw MSG_ClientError("%s must be an object or an array of objects", reserved_word);
	}
}


void
Schema::index_object(const MsgPack& parent_properties, const MsgPack& object, MsgPack& parent_data, Xapian::Document& doc, const std::string& name)
{
	L_CALL(this, "Schema::index_object()");

	const auto spc_start = specification;
	const MsgPack* properties = nullptr;
	MsgPack* data = nullptr;
	if (name.empty()) {
		properties = &parent_properties;
		data = &parent_data;
		specification.found_field = true;
	} else {
		if (specification.full_name.empty()) {
			specification.full_name.assign(name);
		} else {
			specification.full_name.append(DB_OFFSPRING_UNION).append(name);
		}
		specification.name.assign(name);
		properties = &get_subproperties(parent_properties);
		data = &parent_data[name];
	}

	switch (object.type()) {
		case msgpack::type::MAP: {
			bool offsprings = false;
			TaskVector tasks;
			tasks.reserve(object.size());
			for (const auto& item_key : object) {
				const auto str_key = item_key.as_string();
				try {
					auto func = map_dispatch_document.at(str_key);
					(this->*func)(object.at(str_key));
				} catch (const std::out_of_range&) {
					if (is_valid(str_key)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(*properties), std::ref(object.at(str_key)), std::ref(*data), std::ref(doc), std::move(str_key)));
						offsprings = true;
					}
				}
			}

			const auto spc_object = specification;

			if unlikely(!specification.found_field && specification.sep_types[2] != NO_TYPE) {
				validate_required_data(specification.value.get());
			}

			if (specification.name.empty()) {
				if (specification.value) {
					index_item(doc, *specification.value, *data);
				}
				if (specification.value_rec) {
					index_item(doc, *specification.value_rec, *data, 0);
				}
			} else {
				if (specification.full_name.empty()) {
					specification.full_name.assign(specification.name);
				} else {
					specification.full_name.append(DB_OFFSPRING_UNION).append(specification.name);
				}
				if (specification.value) {
					// Update specifications.
					get_subproperties(*properties);
					index_item(doc, *specification.value, *data);
				}
				if (specification.value_rec) {
					// Update specifications.
					get_subproperties(*properties);
					index_item(doc, *specification.value_rec, *data, 0);
				}
			}

			for (auto& task : tasks) {
				specification = spc_object;
				task.get();
			}

			if unlikely(offsprings && specification.sep_types[0] == NO_TYPE) {
				set_type_to_object();
			}
			break;
		}
		case msgpack::type::ARRAY:
			if unlikely(specification.sep_types[1] == NO_TYPE) {
				set_type_to_array();
			}
			index_array(*properties, object, *data, doc);
			break;
		default:
			index_item(doc, object, *data);
			break;
	}

	if (data->is_null()) {
		parent_data.erase(name);
	}

	specification = std::move(spc_start);
}


void
Schema::index_array(const MsgPack& properties, const MsgPack& array, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_array()");

	const auto spc_start = specification;
	bool offsprings = false;
	size_t pos = 0;
	for (const auto& item : array) {
		switch (item.type()) {
			case msgpack::type::MAP: {
				TaskVector tasks;
				tasks.reserve(item.size());
				specification.value = nullptr;
				specification.value_rec = nullptr;
				auto& data_pos = data[pos];

				for (const auto& property : item) {
					auto str_prop = property.as_string();
					try {
						auto func = map_dispatch_document.at(str_prop);
						(this->*func)(item.at(str_prop));
					} catch (const std::out_of_range&) {
						if (is_valid(str_prop)) {
							tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(item.at(str_prop)), std::ref(data_pos), std::ref(doc), std::move(str_prop)));
							offsprings = true;
						}
					}
				}

				const auto spc_item = specification;

				if (specification.name.empty()) {
					specification.found_field = true;
					if (specification.value) {
						index_item(doc, *specification.value, data_pos);
					}
					if (specification.value_rec) {
						index_item(doc, *specification.value_rec, data_pos, pos);
					}
				} else {
					if (specification.full_name.empty()) {
						specification.full_name.assign(specification.name);
					} else {
						specification.full_name.append(DB_OFFSPRING_UNION).append(specification.name);
					}
					if (specification.value) {
						// Update specification.
						get_subproperties(properties);
						index_item(doc, *specification.value, data_pos);
					}
					if (specification.value_rec) {
						// Update specification.
						get_subproperties(properties);
						index_item(doc, *specification.value_rec, data_pos, pos);
					}
				}

				for (auto& task : tasks) {
					specification = spc_item;
					task.get();
				}

				specification = spc_start;
				break;
			}
			case msgpack::type::ARRAY:
				index_item(doc, item, data[pos]);
				break;
			default:
				index_item(doc, item, data[pos], pos);
				break;
		}
		++pos;
	}

	if unlikely(offsprings && specification.sep_types[0] == NO_TYPE) {
		set_type_to_object();
	}
}


void
Schema::index_item(Xapian::Document& doc, const MsgPack& value, MsgPack& data, size_t pos)
{
	L_CALL(this, "Schema::index_item(1)");

	try {
		if unlikely(!specification.set_type) {
			validate_required_data(&value);
		}

		if (!specification.found_field && !specification.dynamic) {
			throw MSG_ClientError("%s is not dynamic", specification.full_name.c_str());
		}

		if (specification.prefix.empty()) {
			switch (specification.index) {
				case typeIndex::NONE:
					return;
				case typeIndex::TERMS:
				case typeIndex::FIELD_TERMS:
				case typeIndex::GLOBAL_TERMS:
					index_global_term(doc, Serialise::serialise(specification.sep_types[2], value), pos);
					break;
				case typeIndex::VALUES:
				case typeIndex::FIELD_VALUES:
				case typeIndex::GLOBAL_VALUES: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					index_value(doc, value, s_g, specification.acc_gprefix, pos);
					break;
				}
				case typeIndex::ALL:
				case typeIndex::FIELD_ALL:
				case typeIndex::GLOBAL_ALL: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					index_value(doc, value, s_g, specification.acc_gprefix, pos, &Schema::index_global_term);
					break;
				}
			}
		} else {
			switch (specification.index) {
				case typeIndex::NONE:
					return;
				case typeIndex::TERMS:
					index_all_term(doc, Serialise::serialise(specification.sep_types[2], value), pos);
					break;
				case typeIndex::FIELD_TERMS:
					index_field_term(doc, Serialise::serialise(specification.sep_types[2], value), pos);
					break;
				case typeIndex::GLOBAL_TERMS:
					index_global_term(doc, Serialise::serialise(specification.sep_types[2], value), pos);
					break;
				case typeIndex::VALUES: {
					StringSet& s_f = map_values[specification.slot];
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					index_all_value(doc, value, s_f, s_g, pos);
					break;
				}
				case typeIndex::FIELD_VALUES: {
					StringSet& s_f = map_values[specification.slot];
					index_value(doc, value, s_f, specification.acc_prefix, pos);
					break;
				}
				case typeIndex::GLOBAL_VALUES: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					index_value(doc, value, s_g, specification.acc_gprefix, pos);
					break;
				}
				case typeIndex::ALL: {
					StringSet& s_f = map_values[specification.slot];
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					index_all_value(doc, value, s_f, s_g, pos, true);
					break;
				}
				case typeIndex::FIELD_ALL: {
					StringSet& s_f = map_values[specification.slot];
					index_value(doc, value, s_f, specification.acc_prefix, pos, &Schema::index_field_term);
					break;
				}
				case typeIndex::GLOBAL_ALL: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					index_value(doc, value, s_g, specification.acc_gprefix, pos, &Schema::index_global_term);
					break;
				}
			}
		}

		if (specification.store) {
			// Add value to data.
			auto& data_value = data[RESERVED_VALUE];
			switch (data_value.type()) {
				case msgpack::type::NIL:
					data_value = value;
					break;
				case msgpack::type::ARRAY:
					data_value.push_back(value);
					break;
				default:
					data_value = MsgPack({ data_value, value });
			}
		}
	} catch (const DummyException&) { }
}


void
Schema::index_item(Xapian::Document& doc, const MsgPack& values, MsgPack& data)
{
	L_CALL(this, "Schema::index_item()");

	try {
		if unlikely(!specification.set_type) {
			validate_required_data(&values);
		}

		if (!specification.found_field && !specification.dynamic) {
			throw MSG_ClientError("%s is not dynamic", specification.full_name.c_str());
		}

		if (specification.prefix.empty()) {
			switch (specification.index) {
				case typeIndex::NONE:
					return;
				case typeIndex::TERMS:
				case typeIndex::FIELD_TERMS:
				case typeIndex::GLOBAL_TERMS: {
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_global_term(doc, Serialise::serialise(specification.sep_types[2], value), pos++);
						}
					} else {
						index_global_term(doc, Serialise::serialise(specification.sep_types[2], values), 0);
					}
					break;
				}
				case typeIndex::VALUES:
				case typeIndex::FIELD_VALUES:
				case typeIndex::GLOBAL_VALUES: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_value(doc, value, s_g, specification.acc_gprefix, pos++);
						}
					} else {
						index_value(doc, values, s_g, specification.acc_gprefix, 0);
					}
					break;
				}
				case typeIndex::ALL:
				case typeIndex::FIELD_ALL:
				case typeIndex::GLOBAL_ALL: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_value(doc, value, s_g, specification.acc_gprefix, pos++, &Schema::index_global_term);
						}
					} else {
						index_value(doc, values, s_g, specification.acc_gprefix, 0, &Schema::index_global_term);
					}
					break;
				}
			}
		} else {
			switch (specification.index) {
				case typeIndex::NONE:
					return;
				case typeIndex::TERMS: {
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_all_term(doc, Serialise::serialise(specification.sep_types[2], value), pos++);
						}
					} else {
						index_all_term(doc, Serialise::serialise(specification.sep_types[2], values), 0);
					}
					break;
				}
				case typeIndex::FIELD_TERMS: {
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_field_term(doc, Serialise::serialise(specification.sep_types[2], value), pos++);
						}
					} else {
						index_field_term(doc, Serialise::serialise(specification.sep_types[2], values), 0);
					}
					break;
				}
				case typeIndex::GLOBAL_TERMS: {
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_global_term(doc, Serialise::serialise(specification.sep_types[2], value), pos++);
						}
					} else {
						index_global_term(doc, Serialise::serialise(specification.sep_types[2], values), 0);
					}
					break;
				}
				case typeIndex::VALUES: {
					StringSet& s_f = map_values[specification.slot];
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_all_value(doc, value, s_f, s_g, pos++);
						}
					} else {
						index_all_value(doc, values, s_f, s_g, 0);
					}
					break;
				}
				case typeIndex::FIELD_VALUES: {
					StringSet& s_f = map_values[specification.slot];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_value(doc, value, s_f, specification.acc_prefix, pos++);
						}
					} else {
						index_value(doc, values, s_f, specification.acc_prefix, 0);
					}
					break;
				}
				case typeIndex::GLOBAL_VALUES: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_value(doc, value, s_g, specification.acc_gprefix, pos++);
						}
					} else {
						index_value(doc, values, s_g, specification.acc_gprefix, 0);
					}
					break;
				}
				case typeIndex::ALL: {
					StringSet& s_f = map_values[specification.slot];
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_all_value(doc, value, s_f, s_g, pos++, true);
						}
					} else {
						index_all_value(doc, values, s_f, s_g, 0, true);
					}
					break;
				}
				case typeIndex::FIELD_ALL: {
					StringSet& s_f = map_values[specification.slot];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_value(doc, value, s_f, specification.acc_prefix, pos++, &Schema::index_field_term);
						}
					} else {
						index_value(doc, values, s_f, specification.acc_prefix, 0, &Schema::index_field_term);
					}
					break;
				}
				case typeIndex::GLOBAL_ALL: {
					StringSet& s_g = map_values[get_global_slot(specification.sep_types[2])];
					if (values.is_array()) {
						if unlikely(specification.sep_types[1] == NO_TYPE) {
							set_type_to_array();
						}
						size_t pos = 0;
						for (const auto& value : values) {
							index_value(doc, value, s_g, specification.acc_gprefix, pos++, &Schema::index_global_term);
						}
					} else {
						index_value(doc, values, s_g, specification.acc_gprefix, 0, &Schema::index_global_term);
					}
					break;
				}
			}
		}

		if (specification.store) {
			// Add value to data.
			auto& data_value = data[RESERVED_VALUE];
			switch (data_value.type()) {
				case msgpack::type::NIL:
					data_value = values;
					break;
				case msgpack::type::ARRAY:
					if (values.is_array()) {
						for (const auto& value : values) {
							data_value.push_back(value);
						}
					} else {
						data_value.push_back(values);
					}
					break;
				default:
					if (values.is_array()) {
						data_value = MsgPack({ data_value });
						for (const auto& value : values) {
							data_value.push_back(value);
						}
					} else {
						data_value = MsgPack({ data_value, values });
					}
					break;
			}
		}
	} catch (const DummyException&) { }
}


void
Schema::validate_required_data(const MsgPack* value)
{
	L_CALL(this, "Schema::validate_required_data()");

	if (specification.sep_types[2] == NO_TYPE) {
		if (value) {
			if (XapiandManager::manager->type_required) {
				throw MSG_MissingTypeError("Type of field [%s] is missing", specification.full_name.c_str());
			}
			set_type(*value);
		} else if (specification.value_rec) {
			if (XapiandManager::manager->type_required) {
				throw MSG_MissingTypeError("Type of field [%s] is missing", specification.full_name.c_str());
			}
			set_type(*specification.value_rec);
		}
	}

	if (!specification.full_name.empty()) {
		auto& properties = get_mutable(specification.full_name);

		// Process RESERVED_ACCURACY, RESERVED_ACC_PREFIX, RESERVED_ACC_GPREFIX
		std::set<uint64_t> set_acc;
		switch (specification.sep_types[2]) {
			case GEO_TYPE: {
				if (!specification.doc_acc) {
					specification.doc_acc = std::make_unique<const MsgPack>(def_accuracy_geo);
				}

				// Set partials and error.
				properties[RESERVED_PARTIALS] = specification.partials;

				// Set partials and error.
				if (specification.error < HTM_MIN_ERROR) {
					specification.error = HTM_MIN_ERROR;
				} else if (specification.error > HTM_MAX_ERROR) {
					specification.error = HTM_MAX_ERROR;
				}
				properties[RESERVED_ERROR] = specification.error;

				try {
					for (const auto& _accuracy : *specification.doc_acc) {
						auto val_acc = _accuracy.as_u64();
						if (val_acc <= HTM_MAX_LEVEL) {
							set_acc.insert(val_acc);
						} else {
							throw MSG_ClientError("Data inconsistency, level value in %s: %s must be a positive number between 0 and %d (%llu not supported)", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL, val_acc);
						}
					}
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, level value in %s: %s must be a positive number between 0 and %d", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
				}
				break;
			}
			case DATE_TYPE: {
				if (!specification.doc_acc) {
					specification.doc_acc = std::make_unique<const MsgPack>(def_accuracy_date);
				}
				try {
					for (const auto& _accuracy : *specification.doc_acc) {
						auto str_accuracy(lower_string(_accuracy.as_string()));
						try {
							set_acc.insert(toUType(map_acc_date.at(str_accuracy)));
						} catch (const std::out_of_range&) {
							throw MSG_ClientError("Data inconsistency, %s: %s must be a subset of %s (%s not supported)", RESERVED_ACCURACY, DATE_STR, str_set_acc_date.c_str(), str_accuracy.c_str());
						}
					}
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, %s in %s must be a subset of %s", RESERVED_ACCURACY, DATE_STR, str_set_acc_date.c_str());
				}
				break;
			}
			case INTEGER_TYPE:
			case POSITIVE_TYPE:
			case FLOAT_TYPE: {
				if (!specification.doc_acc) {
					specification.doc_acc = std::make_unique<const MsgPack>(def_accuracy_num);
				}
				try {
					for (const auto& _accuracy : *specification.doc_acc) {
						set_acc.insert(_accuracy.as_u64());
					}
					break;
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, %s: %s must be an array of positive numbers", RESERVED_ACCURACY, specification.sep_types[2]);
				}
			}
			case NO_TYPE:
				throw MSG_ClientError("%s must be defined for validate data to index", RESERVED_TYPE);
		}

		auto size_acc = set_acc.size();
		if (size_acc) {
			if (specification.acc_prefix.empty()) {
				for (const auto& acc : set_acc) {
					specification.acc_prefix.push_back(get_prefix(specification.full_name + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, specification.sep_types[2]));
				}
			} else if (specification.acc_prefix.size() != size_acc) {
				throw MSG_ClientError("Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
			}

			if (specification.acc_gprefix.empty()) {
				std::string name(1, '_');
				name.append(Serialise::type(specification.sep_types[2]));
				for (const auto& acc : set_acc) {
					specification.acc_gprefix.push_back(get_prefix(name + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, specification.sep_types[2]));
				}
			} else if (specification.acc_gprefix.size() != size_acc) {
				throw MSG_ClientError("Data inconsistency, there must be a global prefix for each unique value in %s", RESERVED_ACCURACY);
			}

			specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
			properties[RESERVED_ACCURACY] = specification.accuracy;
			properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
			properties[RESERVED_ACC_GPREFIX] = specification.acc_gprefix;
		}

		// Process RESERVED_TYPE
		properties[RESERVED_TYPE] = specification.sep_types;

		// Process RESERVED_PREFIX
		if (specification.prefix.empty()) {
			specification.prefix = get_prefix(specification.full_name, DOCUMENT_CUSTOM_TERM_PREFIX, specification.sep_types[2]);
		}
		properties[RESERVED_PREFIX] = specification.prefix;

		// Process RESERVED_SLOT
		if (!specification.slot) {
			specification.slot = get_slot(specification.full_name);
		}
		properties[RESERVED_SLOT] = specification.slot;

		// Process RESERVED_BOOL_TERM
		if (!specification.set_bool_term) {
			// By default, if field name has upper characters then it is consider bool term.
			specification.bool_term = strhasupper(specification.full_name);
		}
		properties[RESERVED_BOOL_TERM] = specification.bool_term;

		specification.set_type = true;
	}
}


void
Schema::index_field_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_field_term()");

	if (serialise_val.empty()) {
		return;
	}

	if (specification.sep_types[2] == TEXT_TYPE) {
		Xapian::TermGenerator term_generator;
		term_generator.set_document(doc);
		term_generator.set_stemmer(Xapian::Stem(specification.language[getPos(pos, specification.language.size())]));
		term_generator.set_stemming_strategy(specification.analyzer[getPos(pos, specification.analyzer.size())]);
		// Xapian::WritableDatabase *wdb = nullptr;
		// if (specification.spelling[getPos(pos, specification.spelling.size())]) {
		// 	wdb = static_cast<Xapian::WritableDatabase *>(database->db.get());
		// 	term_generator.set_database(*wdb);
		// 	term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		// }
		auto positions = specification.positions[getPos(pos, specification.positions.size())];
		if (positions) {
			term_generator.index_text(serialise_val, specification.weight[getPos(pos, specification.weight.size())], specification.prefix);
		} else {
			term_generator.index_text_without_positions(serialise_val, specification.weight[getPos(pos, specification.weight.size())], specification.prefix);
		}
		L_INDEX(this, "Text to Index [%d] => %s: %s [with positions: %d]", pos, specification.prefix.c_str(), serialise_val.c_str(), positions);
	} else {
		auto nameterm = prefixed(serialise_val, specification.prefix);
		auto position = specification.position[getPos(pos, specification.position.size())];
		if (position) {
			if (specification.bool_term) {
				doc.add_posting(nameterm, position, 0);
			} else {
				doc.add_posting(nameterm, position, specification.weight[getPos(pos, specification.weight.size())]);
			}
		} else {
			if (specification.bool_term) {
				doc.add_boolean_term(nameterm);
			} else {
				doc.add_term(nameterm, specification.weight[getPos(pos, specification.weight.size())]);
			}
		}
		L_INDEX(this, "Term [%d] -> %s  Bool: %d  Posting: %d", pos, repr(nameterm).c_str(), specification.bool_term, position);
	}
}


void
Schema::index_global_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_global_term()");

	if (serialise_val.empty()) {
		return;
	}

	if (specification.sep_types[2] == TEXT_TYPE) {
		Xapian::TermGenerator term_generator;
		term_generator.set_document(doc);
		term_generator.set_stemmer(Xapian::Stem(specification.language[getPos(pos, specification.language.size())]));
		term_generator.set_stemming_strategy(specification.analyzer[getPos(pos, specification.analyzer.size())]);
		// Xapian::WritableDatabase *wdb = nullptr;
		// if (specification.spelling[getPos(pos, specification.spelling.size())]) {
		// 	wdb = static_cast<Xapian::WritableDatabase *>(database->db.get());
		// 	term_generator.set_database(*wdb);
		// 	term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		// }
		auto positions = specification.positions[getPos(pos, specification.positions.size())];
		if (positions) {
			term_generator.index_text(serialise_val, specification.weight[getPos(pos, specification.weight.size())]);
		} else {
			term_generator.index_text_without_positions(serialise_val, specification.weight[getPos(pos, specification.weight.size())]);
		}
		L_INDEX(this, "Text to Index [%d] => %s [with positions: %d]", pos, serialise_val.c_str(), positions);
	} else {
		auto position = specification.position[getPos(pos, specification.position.size())];
		if (position) {
			if (specification.bool_term) {
				doc.add_posting(serialise_val, position, 0);
			} else {
				doc.add_posting(serialise_val, position, specification.weight[getPos(pos, specification.weight.size())]);
			}
		} else {
			if (specification.bool_term) {
				doc.add_boolean_term(serialise_val);
			} else {
				doc.add_term(serialise_val, specification.weight[getPos(pos, specification.weight.size())]);
			}
		}
		L_INDEX(this, "Term [%d] -> %s  Bool: %d  Posting: %d", pos, repr(serialise_val).c_str(), specification.bool_term, position);
	}
}


void
Schema::index_all_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_all_term()");

	if (serialise_val.empty()) {
		return;
	}

	if (specification.sep_types[2] == TEXT_TYPE) {
		Xapian::TermGenerator term_generator;
		term_generator.set_document(doc);
		term_generator.set_stemmer(Xapian::Stem(specification.language[getPos(pos, specification.language.size())]));
		term_generator.set_stemming_strategy(specification.analyzer[getPos(pos, specification.analyzer.size())]);
		// Xapian::WritableDatabase *wdb = nullptr;
		// if (specification.spelling[getPos(pos, specification.spelling.size())]) {
		// 	wdb = static_cast<Xapian::WritableDatabase *>(database->db.get());
		// 	term_generator.set_database(*wdb);
		// 	term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		// }
		auto positions = specification.positions[getPos(pos, specification.positions.size())];
		if (positions) {
			auto wdfinc = specification.weight[getPos(pos, specification.weight.size())];
			term_generator.index_text(serialise_val, wdfinc, specification.prefix);
			term_generator.index_text(serialise_val, wdfinc);
		} else {
			auto wdfinc = specification.weight[getPos(pos, specification.weight.size())];
			term_generator.index_text_without_positions(serialise_val, wdfinc, specification.prefix);
			term_generator.index_text_without_positions(serialise_val, wdfinc);
		}
		L_INDEX(this, "Text to Index [%d] => { %s: %s, %s } [with positions: %s]", pos, specification.prefix.c_str(), serialise_val.c_str(), serialise_val.c_str(), positions);
	} else {
		auto nameterm = prefixed(serialise_val, specification.prefix);
		auto position = specification.position[getPos(pos, specification.position.size())];
		if (position) {
			if (specification.bool_term) {
				doc.add_posting(nameterm, position, 0);
				doc.add_posting(serialise_val, position, 0);
			} else {
				auto wdfinc = specification.weight[getPos(pos, specification.weight.size())];
				doc.add_posting(nameterm, position, wdfinc);
				doc.add_posting(serialise_val, position, wdfinc);
			}
		} else {
			if (specification.bool_term) {
				doc.add_boolean_term(nameterm);
				doc.add_boolean_term(serialise_val);
			} else {
				auto wdfinc = specification.weight[getPos(pos, specification.weight.size())];
				doc.add_term(nameterm, wdfinc);
				doc.add_term(serialise_val, wdfinc);
			}
		}
		L_INDEX(this, "Term [%d] -> { %s, %s }  Bool: %d  Posting: %d", pos, repr(nameterm).c_str(), repr(serialise_val).c_str(), specification.bool_term, position);
	}
}


void
Schema::index_value(Xapian::Document& doc, const MsgPack& value, StringSet& s, const std::vector<std::string>& acc_prefix, size_t pos, index_term fun) const
{
	L_CALL(this, "Schema::index_value()");

	std::string value_v;

	switch (specification.sep_types[2]) {
		case FLOAT_TYPE: {
			try {
				auto f_val = value.as_f64();
				value_v.assign(Serialise::_float(f_val));
				s.insert(value_v);
				GenerateTerms::integer(doc, specification.accuracy, acc_prefix, static_cast<int64_t>(f_val));
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for float type: %s", value.to_string().c_str());
			}
		}
		case INTEGER_TYPE: {
			try {
				auto i_val = value.as_i64();
				value_v.assign(Serialise::integer(i_val));
				s.insert(value_v);
				GenerateTerms::integer(doc, specification.accuracy, acc_prefix, i_val);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for integer type: %s", value.to_string().c_str());
			}
		}
		case POSITIVE_TYPE: {
			try {
				auto u_val = value.as_u64();
				value_v.assign(Serialise::positive(u_val));
				s.insert(value_v);
				GenerateTerms::positive(doc, specification.accuracy, acc_prefix, u_val);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for positive type: %s", value.to_string().c_str());
			}
		}
		case DATE_TYPE: {
			try {
				Datetime::tm_t tm;
				value_v.assign(Serialise::date(value, tm));
				s.insert(value_v);
				GenerateTerms::date(doc, specification.accuracy, acc_prefix, tm);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for date type: %s", value.to_string().c_str());
			}
		}
		case GEO_TYPE: {
			try {
				auto str_ewkt = value.as_string();
				if (fun) {
					value_v.assign(Serialise::ewkt(str_ewkt));
				}
				auto geo = EWKT_Parser::getGeoSpatial(str_ewkt, specification.partials, specification.error);
				s.insert(Serialise::geo(geo.ranges, geo.centroids));
				GenerateTerms::geo(doc, specification.accuracy, acc_prefix, geo.ranges);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for geo type: %s", value.to_string().c_str());
			}
		}
		case STRING_TYPE:
		case TEXT_TYPE:
			try {
				value_v.assign(value.as_string());
				s.insert(value_v);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for %s type: %s", Serialise::type(specification.sep_types[2]).c_str(), value.to_string().c_str());
			}
		case BOOLEAN_TYPE:
			try {
				value_v.assign(Serialise::serialise(BOOLEAN_TYPE, value));
				s.insert(value_v);
				break;
			} catch (const SerialisationError&) {
				throw MSG_ClientError("Format invalid for boolean type: %s", value.to_string().c_str());
			}
		default:
			throw MSG_ClientError("Type: '%c' is an unknown type", specification.sep_types[2]);
	}

	// Index like a term.
	if (fun) {
		(this->*fun)(doc, std::move(value_v), pos);
	}
}


void
Schema::index_all_value(Xapian::Document& doc, const MsgPack& value, StringSet& s_f, StringSet& s_g, size_t pos, bool is_term) const
{
	L_CALL(this, "Schema::index_all_value()");

	std::string value_v;

	switch (specification.sep_types[2]) {
		case FLOAT_TYPE: {
			try {
				auto f_val = value.as_f64();
				value_v.assign(Serialise::_float(f_val));
				s_f.insert(value_v);
				s_g.insert(value_v);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix,
					specification.acc_gprefix, static_cast<int64_t>(f_val));
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for float type: %s", value.to_string().c_str());
			}
		}
		case INTEGER_TYPE: {
			try {
				auto i_val = value.as_i64();
				value_v.assign(Serialise::integer(i_val));
				s_f.insert(value_v);
				s_g.insert(value_v);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix,
					specification.acc_gprefix, i_val);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for integer type: %s", value.to_string().c_str());
			}
		}
		case POSITIVE_TYPE: {
			try {
				auto u_val = value.as_u64();
				value_v.assign(Serialise::positive(u_val));
				s_f.insert(value_v);
				s_g.insert(value_v);
				GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix,
					specification.acc_gprefix, u_val);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for positive type: %s", value.to_string().c_str());
			}
		}
		case DATE_TYPE: {
			try {
				Datetime::tm_t tm;
				value_v.assign(Serialise::date(value, tm));
				s_f.insert(value_v);
				s_g.insert(value_v);
				GenerateTerms::date(doc, specification.accuracy, specification.acc_prefix,
					specification.acc_gprefix, tm);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for date type: %s", value.to_string().c_str());
			}
		}
		case GEO_TYPE: {
			try {
				auto str_ewkt = value.as_string();
				if (is_term) {
					value_v.assign(Serialise::ewkt(str_ewkt));
				}
				auto geo = EWKT_Parser::getGeoSpatial(str_ewkt, specification.partials, specification.error);
				auto val_ser = Serialise::geo(geo.ranges, geo.centroids);
				s_f.insert(val_ser);
				s_g.insert(val_ser);
				GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix,
					specification.acc_gprefix, geo.ranges);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for geo type: %s", value.to_string().c_str());
			}
		}
		case STRING_TYPE:
		case TEXT_TYPE:
			try {
				value_v.assign(value.as_string());
				s_f.insert(value_v);
				s_g.insert(value_v);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for %s type: %s", Serialise::type(specification.sep_types[2]).c_str(), value.to_string().c_str());
			}
		case BOOLEAN_TYPE:
			try {
				value_v.assign(Serialise::serialise(BOOLEAN_TYPE, value));
				s_f.insert(value_v);
				s_g.insert(value_v);
				break;
			} catch (const SerialisationError&) {
				throw MSG_ClientError("Format invalid for boolean type: %s", value.to_string().c_str());
			}
		default:
			throw MSG_ClientError("Type: '%c' is an unknown type", specification.sep_types[2]);
	}

	// Index like a term.
	if (is_term) {
		index_all_term(doc, std::move(value_v), pos);
	}
}


data_field_t
Schema::get_data_field(const std::string& field_name) const
{
	L_CALL(this, "Schema::get_data_field()");

	data_field_t res = {
		default_spc.slot, default_spc.prefix, default_spc.sep_types[2],
		default_spc.accuracy, default_spc.acc_prefix, default_spc.acc_gprefix,
		default_spc.bool_term, default_spc.partials, default_spc.error
	};

	if (field_name.empty()) {
		return res;
	}

	std::vector<std::string> fields;
	stringTokenizer(field_name, DB_OFFSPRING_UNION, fields);
	try {
		const auto& properties = schema->at(RESERVED_SCHEMA).path(fields);

		res.type = static_cast<unsigned>(properties.at(RESERVED_TYPE).at(2).as_u64());
		if (res.type == NO_TYPE) {
			return res;
		}

		res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
		res.prefix = properties.at(RESERVED_PREFIX).as_string();
		res.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();

		// Get accuracy, acc_prefix and acc_gprefix.
		switch (res.type) {
			case GEO_TYPE:
				res.partials = properties.at(RESERVED_PARTIALS).as_bool();
				res.error = properties.at(RESERVED_ERROR).as_f64();
			case FLOAT_TYPE:
			case INTEGER_TYPE:
			case POSITIVE_TYPE:
			case DATE_TYPE: {
				for (const auto& acc : properties.at(RESERVED_ACCURACY)) {
					res.accuracy.push_back(acc.as_u64());
				}
				for (const auto& acc_p : properties.at(RESERVED_ACC_PREFIX)) {
					res.acc_prefix.push_back(acc_p.as_string());
				}
				for (const auto& acc_gp : properties.at(RESERVED_ACC_GPREFIX)) {
					res.acc_gprefix.push_back(acc_gp.as_string());
				}
				break;
			}
			default:
				break;
		}
	} catch (const std::exception&) { }

	return res;
}


data_field_t
Schema::get_slot_field(const std::string& field_name) const
{
	L_CALL(this, "Schema::get_slot_field()");

	data_field_t res = {
		default_spc.slot, default_spc.prefix, default_spc.sep_types[2],
		default_spc.accuracy, default_spc.acc_prefix, default_spc.acc_gprefix,
		default_spc.bool_term, default_spc.partials, default_spc.error
	};

	if (field_name.empty()) {
		return res;
	}

	std::vector<std::string> fields;
	stringTokenizer(field_name, DB_OFFSPRING_UNION, fields);
	try {
		const auto& properties = schema->at(RESERVED_SCHEMA).path(fields);
		res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
		res.type = static_cast<unsigned>(properties.at(RESERVED_TYPE).at(2).as_u64());
		// Get partials and error if type is GEO.
		if (res.type == GEO_TYPE) {
			res.partials = properties.at(RESERVED_PARTIALS).as_bool();
			res.error = properties.at(RESERVED_ERROR).as_bool();
		}
	} catch (const std::exception&) { }

	return res;
}
