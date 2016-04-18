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
#include "log.h"
#include "serialise.h"
#include "wkt_parser.h"


static const std::vector<std::string> str_time     { "second", "minute", "hour", "day", "month", "year" };
static const std::vector<std::string> str_analyzer { "STEM_NONE", "STEM_SOME", "STEM_ALL", "STEM_ALL_Z" };
static const std::vector<std::string> str_index    { "ALL", "TERM", "VALUE", "TEXT" };


static const MsgPack def_accuracy_geo(to_json("[true, 0.2, 0, 5, 10, 15, 20, 25]"));
static const MsgPack def_accuracy_num(to_json("[100, 1000, 10000, 100000]"));
static const MsgPack def_accuracy_date(to_json("[\"hour\", \"day\", \"month\", \"year\"]"));


const specification_t default_spc;


const std::unordered_map<std::string, dispatch_reserved> map_dispatch_reserved({
	{ RESERVED_WEIGHT,       &Schema::process_weight      },
	{ RESERVED_POSITION,     &Schema::process_position    },
	{ RESERVED_LANGUAGE,     &Schema::process_language    },
	{ RESERVED_SPELLING,     &Schema::process_spelling    },
	{ RESERVED_POSITIONS,    &Schema::process_positions   },
	{ RESERVED_ACCURACY,     &Schema::process_accuracy    },
	{ RESERVED_ACC_PREFIX,   &Schema::process_acc_prefix  },
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
});


const std::unordered_map<std::string, dispatch_root> map_dispatch_root({
	{ RESERVED_TEXTS,        &Schema::process_texts             },
	{ RESERVED_VALUES,       &Schema::process_values            },
	{ RESERVED_TERMS,        &Schema::process_terms             }
});


const std::unordered_map<std::string, dispatch_property> map_dispatch_properties({
	{ RESERVED_WEIGHT,       &Schema::process_weight            },
	{ RESERVED_POSITION,     &Schema::process_position          },
	{ RESERVED_LANGUAGE,     &Schema::process_language          },
	{ RESERVED_SPELLING,     &Schema::process_spelling          },
	{ RESERVED_POSITIONS,    &Schema::process_positions         },
	{ RESERVED_ACCURACY,     &Schema::process_accuracy          },
	{ RESERVED_ACC_PREFIX,   &Schema::process_acc_prefix        },
	{ RESERVED_STORE,        &Schema::process_store             },
	{ RESERVED_TYPE,         &Schema::process_type              },
	{ RESERVED_ANALYZER,     &Schema::process_analyzer          },
	{ RESERVED_DYNAMIC,      &Schema::process_dynamic           },
	{ RESERVED_D_DETECTION,  &Schema::process_d_detection       },
	{ RESERVED_N_DETECTION,  &Schema::process_n_detection       },
	{ RESERVED_G_DETECTION,  &Schema::process_g_detection       },
	{ RESERVED_B_DETECTION,  &Schema::process_b_detection       },
	{ RESERVED_S_DETECTION,  &Schema::process_s_detection       },
	{ RESERVED_BOOL_TERM,    &Schema::process_bool_term         },
	{ RESERVED_SLOT,         &Schema::process_slot              },
	{ RESERVED_INDEX,        &Schema::process_index             },
	{ RESERVED_PREFIX,       &Schema::process_prefix            },
});


const std::unordered_map<std::string, dispatch_readable> map_dispatch_readable({
	{ RESERVED_TYPE,         &Schema::readable_type     },
	{ RESERVED_ANALYZER,     &Schema::readable_analyzer },
	{ RESERVED_INDEX,        &Schema::readable_index    },
});


static auto getPos = [](size_t pos, size_t size) noexcept {
	return pos < size ? pos : size - 1;
};


specification_t::specification_t()
	: position({ 0 }),
	  weight({ 1 }),
	  language({ "en" }),
	  spelling({ false }),
	  positions({ false }),
	  analyzer({ Xapian::TermGenerator::STEM_SOME }),
	  sep_types({ NO_TYPE, NO_TYPE, NO_TYPE }),
	  slot(0),
	  index(Index::ALL),
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
	  fixed_index(false) { }


specification_t::specification_t(const specification_t& o)
	: position(o.position),
	  weight(o.weight),
	  language(o.language),
	  spelling(o.spelling),
	  positions(o.positions),
	  analyzer(o.analyzer),
	  sep_types(o.sep_types),
	  accuracy(o.accuracy),
	  acc_prefix(o.acc_prefix),
	  prefix(o.prefix),
	  slot(o.slot),
	  index(o.index),
	  store(o.store),
	  dynamic(o.dynamic),
	  date_detection(o.date_detection),
	  numeric_detection(o.numeric_detection),
	  geo_detection(o.geo_detection),
	  bool_detection(o.bool_detection),
	  string_detection(o.string_detection),
	  bool_term(o.bool_term),
	  found_field(o.found_field),
	  set_type(o.set_type),
	  set_bool_term(o.bool_term),
	  fixed_index(o.fixed_index),
	  full_name(o.full_name) { }


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
	accuracy = o.accuracy;
	acc_prefix = o.acc_prefix;
	prefix = o.prefix;
	slot = o.slot;
	index = o.index;
	store = o.store;
	dynamic = o.dynamic;
	date_detection = o.date_detection;
	numeric_detection = o.numeric_detection;
	geo_detection = o.geo_detection;
	bool_detection = o.bool_detection;
	string_detection = o.string_detection;
	bool_term = o.bool_term;
	value.reset();
	name = o.name;
	found_field = o.found_field;
	set_type = o.set_type;
	set_bool_term = o.bool_term;
	fixed_index = o.fixed_index;
	doc_acc.reset();
	full_name = o.full_name;
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
		str << str_analyzer[_analyzer] << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_ACCURACY << ": [ ";
	if (sep_types[2] == DATE_TYPE) {
		for (const auto& acc : accuracy) {
			str << str_time[acc] << " ";
		}
	} else {
		for (const auto& acc : accuracy) {
			str << acc << " ";
		}
	}
	str << "]\n";

	str << "\t" << RESERVED_ACC_PREFIX  << ": [ ";
	for (const auto& acc_p : acc_prefix) {
		str << acc_p << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_SLOT        << ": " << slot                 << "\n";
	str << "\t" << RESERVED_TYPE        << ": " << str_type(sep_types)  << "\n";
	str << "\t" << RESERVED_PREFIX      << ": " << prefix               << "\n";
	str << "\t" << RESERVED_INDEX       << ": " << str_index[toUType(index)]     << "\n";
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


Schema::Schema(const Schema& other)
	: schema(other.schema.clone()),
	  exist(other.exist.load()),
	  to_store(other.to_store.load()) { }


void
Schema::set_database(Database* _database)
{
	L_CALL(this, "Schema::set_database()");

	database = _database;

	// Reload schema.
	std::string s_schema = database->get_metadata(RESERVED_SCHEMA);

	if (s_schema.empty()) {
		schema[RESERVED_VERSION] = DB_VERSION_SCHEMA;
		schema[RESERVED_SCHEMA];
	} else {
		schema = MsgPack(s_schema);
		try {
			auto version = schema.at(RESERVED_VERSION);
			if (version.get_f64() != DB_VERSION_SCHEMA) {
				throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
			}
			to_store = false;
		} catch (const std::out_of_range&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		}
	}
}


void
Schema::build(const std::string& s_schema)
{
	L_CALL(this, "Schema::build()");

	if (s_schema.empty()) {
		schema[RESERVED_VERSION] = DB_VERSION_SCHEMA;
		schema[RESERVED_SCHEMA];
	} else {
		schema = MsgPack(s_schema);
		try {
			auto version = schema.at(RESERVED_VERSION);
			if (version.get_f64() != DB_VERSION_SCHEMA) {
				throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
			}
			to_store = false;
		} catch (const std::out_of_range&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		}
	}
}


std::string
Schema::serialise_id(MsgPack& properties, specification_t& specification, const std::string& value_id)
{
	L_CALL(this, "Schema::serialise_id()");

	auto prop_id = properties[RESERVED_ID];
	specification.set_type = true;
	if (prop_id) {
		update_specification(properties, specification);
		return Serialise::serialise(static_cast<char>(prop_id.at(RESERVED_TYPE).at(2).get_u64()), value_id);
	} else {
		to_store.store(true);
		specification.found_field = false;
		std::pair<char, std::string> res_serialise = Serialise::serialise(value_id);
		prop_id[RESERVED_TYPE] = std::vector<unsigned>({ NO_TYPE, NO_TYPE, static_cast<unsigned>(res_serialise.first) });
		prop_id[RESERVED_PREFIX] = DOCUMENT_ID_TERM_PREFIX;
		prop_id[RESERVED_SLOT] = DB_SLOT_ID;
		prop_id[RESERVED_BOOL_TERM] = true;
		prop_id[RESERVED_INDEX] = static_cast<unsigned>(Index::ALL);
		return res_serialise.second;
	}
}


void
Schema::update_specification(const MsgPack& properties, specification_t& specification)
{
	L_CALL(nullptr, "Schema::update_specification()");

	for (const auto property : properties) {
		auto str_prop = property.get_str();
		try {
			auto func = map_dispatch_properties.at(str_prop);
			(*func)(properties.at(str_prop), specification);
		} catch (const std::out_of_range&) { }
	}
}


void
Schema::restart_specification(specification_t& specification)
{
	specification.sep_types = default_spc.sep_types;
	specification.accuracy.clear();
	specification.acc_prefix.clear();
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;
	specification.bool_term = default_spc.bool_term;
	specification.set_type = default_spc.set_type;
	specification.name = default_spc.name;
}


MsgPack
Schema::get_subproperties(MsgPack& properties, specification_t& specification)
{
	L_CALL(this, "Schema::get_subproperties()");

	std::vector<std::string> field_names;
	stringTokenizer(specification.name, DB_OFFSPRING_UNION, field_names);

	MsgPack subproperties;
	for (const auto& field_name : field_names) {
		subproperties.reset(properties[field_name]);
		restart_specification(specification);
		if (subproperties) {
			specification.found_field = true;
			update_specification(subproperties, specification);
		} else {
			to_store = true;
			specification.found_field = false;
		}
	}

	return subproperties;
}


void
Schema::store()
{
	L_CALL(this, "Schema::store()");

	if (to_store) {
		database->set_metadata(RESERVED_SCHEMA, schema.to_string());
		to_store = false;
	}
}


void
Schema::set_type(const MsgPack& item_doc, specification_t& specification)
{
	L_CALL(nullptr, "Schema::set_type()");

	MsgPack field = item_doc.get_type() == msgpack::type::ARRAY ? item_doc.at(0) : item_doc;
	switch (field.get_type()) {
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
			std::string str_value(field.get_str());
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
			throw MSG_DummyException();
		default:
			break;
	}

	throw MSG_ClientError("%s: %s is ambiguous", RESERVED_VALUE, item_doc.to_json_string().c_str());
}


void
Schema::set_type_to_array(MsgPack& properties)
{
	try {
		auto _type = properties.at(RESERVED_TYPE).at(1);
		if (_type.get_u64() == NO_TYPE) {
			_type = ARRAY_TYPE;
			to_store = true;
		}
	} catch (const std::out_of_range&) { }
}


void
Schema::set_type_to_object(MsgPack& properties)
{
	try {
		auto _type = properties.at(RESERVED_TYPE).at(0);
		if (_type.get_u64() == NO_TYPE) {
			_type = OBJECT_TYPE;
			to_store = true;
		}
	} catch (const std::out_of_range&) { }
}


std::string
Schema::to_json_string(bool prettify) const
{
	MsgPack schema_readable = schema.clone();
	auto properties = schema_readable.at(RESERVED_SCHEMA);
	if likely(properties) {
		readable(std::move(properties), true);
	} else {
		schema_readable.erase(RESERVED_SCHEMA);
	}

	return schema_readable.to_json_string(prettify);
}


void
Schema::readable(MsgPack&& item_schema, bool is_root)
{
	// Change this item of schema in readable form.
	for (const auto item_key : item_schema) {
		auto str_key = item_key.get_str();
		try {
			auto func = map_dispatch_readable.at(str_key);
			(*func)(item_schema.at(str_key), item_schema);
		} catch (const std::out_of_range&) {
			if (is_valid(str_key) || (is_root && str_key == RESERVED_ID)) {
				auto sub_item = item_schema.at(str_key);
				if likely(sub_item) {
					readable(std::move(sub_item));
				} else {
					item_schema.erase(str_key);
				}
			}
		}
	}
}


void
Schema::readable_type(MsgPack&& prop_type, const MsgPack& properties)
{
	std::vector<unsigned> sep_types({
		static_cast<unsigned>(prop_type.at(0).get_u64()),
		static_cast<unsigned>(prop_type.at(1).get_u64()),
		static_cast<unsigned>(prop_type.at(2).get_u64())
	});
	prop_type = str_type(sep_types);

	// Readable accuracy.
	try {
		if (sep_types[2] == DATE_TYPE) {
			for (auto _accuracy : properties.at(RESERVED_ACCURACY)) {
				_accuracy = str_time[_accuracy.get_f64()];
			}
		} else if (sep_types[2] == GEO_TYPE) {
			auto _partials = properties.at(RESERVED_ACCURACY).at(0);
			_partials = _partials.get_f64() ? true : false;
		}
	} catch (const std::out_of_range&) { }
}


void
Schema::readable_analyzer(MsgPack&& prop_analyzer, const MsgPack&)
{
	for (auto _analyzer : prop_analyzer) {
		_analyzer = str_analyzer[_analyzer.get_u64()];
	}
}


void
Schema::readable_index(MsgPack&& prop_index, const MsgPack&)
{
	prop_index = str_index[prop_index.get_u64()];
}


void
Schema::process_weight(MsgPack& properties, const MsgPack& doc_weight, specification_t& specification)
{
	// RESERVED_WEIGHT is heritable and can change between documents.
	try {
		specification.weight.clear();
		if (doc_weight.get_type() == msgpack::type::ARRAY) {
			for (const auto _weight : doc_weight) {
				specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
			}
		} else {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.get_u64()));
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_WEIGHT] = specification.weight;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_WEIGHT);
	}
}


void
Schema::process_position(MsgPack& properties, const MsgPack& doc_position, specification_t& specification)
{
	// RESERVED_POSITION is heritable and can change between documents.
	try {
		specification.position.clear();
		if (doc_position.get_type() == msgpack::type::ARRAY) {
			for (const auto _position : doc_position) {
				specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
			}
		} else {
			specification.position.push_back(static_cast<unsigned>(doc_position.get_u64()));
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_POSITION] = specification.position;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_POSITION);
	}
}


void
Schema::process_language(MsgPack& properties, const MsgPack& doc_language, specification_t& specification)
{
	// RESERVED_LANGUAGE is heritable and can change between documents.
	try {
		specification.language.clear();
		if (doc_language.get_type() == msgpack::type::ARRAY) {
			for (const auto _language : doc_language) {
				std::string _str_language(_language.get_str());
				if (is_language(_str_language)) {
					specification.language.push_back(std::move(_str_language));
				} else {
					throw MSG_ClientError("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
				}
			}
		} else {
			std::string _str_language(doc_language.get_str());
			if (is_language(_str_language)) {
				specification.language.push_back(std::move(_str_language));
			} else {
				throw MSG_ClientError("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
			}
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_LANGUAGE] = specification.language;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string or array of strings", RESERVED_LANGUAGE);
	}
}


void
Schema::process_spelling(MsgPack& properties, const MsgPack& doc_spelling, specification_t& specification)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	try {
		specification.spelling.clear();
		if (doc_spelling.get_type() == msgpack::type::ARRAY) {
			for (const auto _spelling : doc_spelling) {
				specification.spelling.push_back(_spelling.get_bool());
			}
		} else {
			specification.spelling.push_back(doc_spelling.get_bool());
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_SPELLING] = specification.spelling;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean or array of booleans", RESERVED_SPELLING);
	}
}


void
Schema::process_positions(MsgPack& properties, const MsgPack& doc_positions, specification_t& specification)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	try {
		specification.positions.clear();
		if (doc_positions.get_type() == msgpack::type::ARRAY) {
			for (const auto _positions : doc_positions) {
				specification.positions.push_back(_positions.get_bool());
			}
		} else {
			specification.positions.push_back(doc_positions.get_bool());
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_POSITIONS] = specification.positions;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean or array of booleans", RESERVED_POSITIONS);
	}
}


void
Schema::process_analyzer(MsgPack& properties, const MsgPack& doc_analyzer, specification_t& specification)
{
	// RESERVED_ANALYZER is heritable and can change between documents.
	try {
		specification.analyzer.clear();
		if (doc_analyzer.get_type() == msgpack::type::ARRAY) {
			for (const auto analyzer : doc_analyzer) {
				std::string _analyzer(upper_string(analyzer.get_str()));
				if (_analyzer == str_analyzer[0]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
				} else if (_analyzer == str_analyzer[1]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
				} else if (_analyzer == str_analyzer[2]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
				} else if (_analyzer == str_analyzer[3]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
				} else {
					throw MSG_ClientError("%s can be  {%s, %s, %s, %s} (%s not supported)", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str(), _analyzer.c_str());
				}
			}
		} else {
			std::string _analyzer(upper_string(doc_analyzer.get_str()));
			if (_analyzer == str_analyzer[0]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
			} else if (_analyzer == str_analyzer[1]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
			} else if (_analyzer == str_analyzer[2]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
			} else if (_analyzer == str_analyzer[3]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
			} else {
				throw MSG_ClientError("%s can be  {%s, %s, %s, %s} (%s not supported)", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str(), _analyzer.c_str());
			}
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_ANALYZER] = specification.analyzer;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string or array of strings", RESERVED_ANALYZER);
	}
}


void
Schema::process_type(MsgPack&, const MsgPack& doc_type, specification_t& specification)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		if (!set_types(lower_string(doc_type.get_str()), specification.sep_types)) {
			throw MSG_ClientError("%s can be [object/][array/]< %s | %s | %s | %s | %s >", RESERVED_TYPE, FLOAT_STR, INTEGER_STR, POSITIVE_STR, STRING_STR, DATE_STR, BOOLEAN_STR, GEO_STR);
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_TYPE);
	}
}


void
Schema::process_accuracy(MsgPack&, const MsgPack& doc_accuracy, specification_t& specification)
{
	// RESERVED_ACCURACY isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	if (doc_accuracy.get_type() == msgpack::type::ARRAY) {
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
Schema::process_acc_prefix(MsgPack&, const MsgPack& doc_acc_prefix, specification_t& specification)
{
	// RESERVED_ACC_PREFIX isn't heritable and can't change once fixed.
	// It is taken into account only if RESERVED_ACCURACY is defined.
	if likely(specification.set_type) {
		return;
	}

	if (doc_acc_prefix.get_type() == msgpack::type::ARRAY) {
		std::set<std::string> set_acc_prefix;
		try {
			for (const auto _acc_prefix : doc_acc_prefix) {
				set_acc_prefix.insert(_acc_prefix.get_str());
			}
			specification.acc_prefix.insert(specification.acc_prefix.end(), set_acc_prefix.begin(), set_acc_prefix.end());
		} catch (const msgpack::type_error&) {
			throw MSG_ClientError("Data inconsistency, %s must be an array of strings", RESERVED_ACC_PREFIX);
		}
	} else {
		throw MSG_ClientError("Data inconsistency, %s must be an array of strings", RESERVED_ACC_PREFIX);
	}
}


void
Schema::process_prefix(MsgPack&, const MsgPack& doc_prefix, specification_t& specification)
{
	// RESERVED_PREFIX isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.prefix = doc_prefix.get_str();
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_PREFIX);
	}
}


void
Schema::process_slot(MsgPack&, const MsgPack& doc_slot, specification_t& specification)
{
	// RESERVED_SLOT isn't heritable and can't change once fixed.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.slot = static_cast<unsigned>(doc_slot.get_u64());
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
Schema::process_index(MsgPack& properties, const MsgPack& doc_index, specification_t& specification)
{
	// RESERVED_INDEX is heritable and can change if fixed_index is false.
	if unlikely(specification.fixed_index) {
		return;
	}

	try {
		std::string _index(upper_string(doc_index.get_str()));
		if (_index == str_index[0]) {
			specification.index = Index::ALL;
		} else if (_index == str_index[1]) {
			specification.index = Index::TERM;
		} else if (_index == str_index[2]) {
			specification.index = Index::VALUE;
		} else if (_index == str_index[3]) {
			specification.index = Index::TEXT;
		} else {
			throw MSG_ClientError("%s can be in {%s, %s, %s, %s} (%s not supported)", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str(), str_index[3].c_str(), _index.c_str());
		}

		if unlikely(!specification.found_field) {
			properties[RESERVED_INDEX] = (unsigned)specification.index;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_INDEX);
	}
}


void
Schema::process_store(MsgPack& properties, const MsgPack& doc_store, specification_t& specification)
{
	// RESERVED_STORE is heritable and can change.
	try {
		specification.store = doc_store.get_bool();

		if unlikely(!specification.found_field) {
			properties[RESERVED_STORE] = specification.store;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_STORE);
	} catch (const std::out_of_range&) { }
}


void
Schema::process_dynamic(MsgPack& properties, const MsgPack& doc_dynamic, specification_t& specification)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.dynamic = doc_dynamic.get_bool();
		properties[RESERVED_DYNAMIC] = specification.dynamic;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_DYNAMIC);
	} catch (const std::out_of_range&) { }
}


void
Schema::process_d_detection(MsgPack& properties, const MsgPack& doc_d_detection, specification_t& specification)
{
	// RESERVED_D_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.date_detection = doc_d_detection.get_bool();
		properties[RESERVED_D_DETECTION] = specification.date_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_D_DETECTION);
	}
}


void
Schema::process_n_detection(MsgPack& properties, const MsgPack& doc_n_detection, specification_t& specification)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.numeric_detection = doc_n_detection.get_bool();
		properties[RESERVED_N_DETECTION] = specification.numeric_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_N_DETECTION);
	}
}


void
Schema::process_g_detection(MsgPack& properties, const MsgPack& doc_g_detection, specification_t& specification)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.geo_detection = doc_g_detection.get_bool();
		properties[RESERVED_G_DETECTION] = specification.geo_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_G_DETECTION);
	}
}


void
Schema::process_b_detection(MsgPack& properties, const MsgPack& doc_b_detection, specification_t& specification)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.bool_detection = doc_b_detection.get_bool();
		properties[RESERVED_B_DETECTION] = specification.bool_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_B_DETECTION);
	}
}


void
Schema::process_s_detection(MsgPack& properties, const MsgPack& doc_s_detection, specification_t& specification)
{
	// RESERVED_S_DETECTION isn't heritable and can't change.
	if likely(specification.found_field) {
		return;
	}

	try {
		specification.string_detection = doc_s_detection.get_bool();
		properties[RESERVED_S_DETECTION] = specification.string_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be boolean", RESERVED_S_DETECTION);
	}
}


void
Schema::process_bool_term(MsgPack&, const MsgPack& doc_bool_term, specification_t& specification)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	if likely(specification.set_type) {
		return;
	}

	try {
		specification.bool_term = doc_bool_term.get_bool();
		specification.set_bool_term = true;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be a boolean", RESERVED_BOOL_TERM);
	}
}


void
Schema::process_value(MsgPack&, const MsgPack& doc_value, specification_t& specification)
{
	// RESERVED_VALUE isn't heritable and is not saved in schema.
	specification.value = std::make_unique<const MsgPack>(doc_value);
}


void
Schema::process_name(MsgPack&, const MsgPack& doc_name, specification_t& specification)
{
	// RESERVED_NAME isn't heritable and is not saved in schema.
	try {
		specification.name.assign(doc_name.get_str());
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_NAME);
	}
}


void
Schema::process_values(MsgPack& properties, const MsgPack doc_values, data_t& data)
{
	L_CALL(this, "Schema::process_values()");

	data.specification.index = Index::VALUE;
	fixed_index(properties, doc_values, data);
}


void
Schema::process_texts(MsgPack& properties, const MsgPack doc_texts, data_t& data)
{
	L_CALL(this, "Schema::process_texts()");

	data.specification.index = Index::TEXT;
	fixed_index(properties, doc_texts, data);
}


void
Schema::process_terms(MsgPack& properties, const MsgPack doc_terms, data_t& data)
{
	L_CALL(this, "Schema::process_terms()");

	data.specification.index = Index::TERM;
	fixed_index(properties, doc_terms, data);
}


void
Schema::fixed_index(MsgPack& properties, const MsgPack& object, data_t& data)
{
	data.specification.fixed_index = true;
	switch (object.get_type()) {
		case msgpack::type::MAP:
			return index_object(properties, object, data);
		case msgpack::type::ARRAY:
			return index_array(properties, object, data);
		default:
			throw MSG_ClientError("%s must be an object or an array of objects", RESERVED_VALUES);
	}
}


void
Schema::index_object(MsgPack& global_properties, const MsgPack object, data_t& data, const std::string name)
{
	L_CALL(this, "Schema::index_object()");

	const auto spc_start = data.specification;
	MsgPack properties;
	if (name.empty()) {
		properties.reset(global_properties);
		data.specification.found_field = true;
	} else {
		if (data.specification.full_name.empty()) {
			data.specification.full_name.assign(name);
		} else {
			data.specification.full_name.append(DB_OFFSPRING_UNION).append(name);
		}
		data.specification.name.assign(name);
		properties.reset(get_subproperties(global_properties, data.specification));
	}

	switch (object.get_type()) {
		case msgpack::type::MAP: {
			bool offsprings = false;
			TaskVector tasks;
			tasks.reserve(object.size());
			for (const auto item_key : object) {
				const auto str_key = item_key.get_str();
				try {
					auto func = map_dispatch_reserved.at(str_key);
					(this->*func)(properties, object.at(str_key), data.specification);
				} catch (const std::out_of_range&) {
					if (is_valid(str_key)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), object.at(str_key), std::ref(data), std::move(str_key)));
						offsprings = true;
					}
				}
			}

			const auto spc_object = data.specification;

			if unlikely(!data.specification.found_field && data.specification.sep_types[2] != NO_TYPE) {
				validate_required_data(properties, data.specification.value.get(), data.specification);
			}

			if (data.specification.name.empty()) {
				if (data.specification.value) {
					index_item(properties, *data.specification.value, data);
				}
			} else {
				if (data.specification.full_name.empty()) {
					data.specification.full_name.assign(data.specification.name);
				} else {
					data.specification.full_name.append(DB_OFFSPRING_UNION).append(data.specification.name);
				}
				if (data.specification.value) {
					auto subproperties = get_subproperties(properties, data.specification);
					index_item(subproperties, *data.specification.value, data);
				}
			}

			if (offsprings) {
				set_type_to_object(properties);
			}

			for (auto& task : tasks) {
				data.specification = spc_object;
				task.get();
			}
			break;
		}
		case msgpack::type::ARRAY:
			set_type_to_array(properties);
			index_array(properties, object, data);
			break;
		default:
			index_item(properties, object, data);
			break;
	}

	data.specification = spc_start;
}


void
Schema::index_array(MsgPack& properties, const MsgPack& array, data_t& data)
{
	L_CALL(this, "Schema::index_array()");

	const auto spc_start = data.specification;
	bool offsprings = false;
	for (auto item : array) {
		if (item.get_type() == msgpack::type::MAP) {
			TaskVector tasks;
			tasks.reserve(item.size());
			data.specification.value = nullptr;
			for (const auto property : item) {
				auto str_prop = property.get_str();
				try {
					auto func = map_dispatch_reserved.at(str_prop);
					(this->*func)(properties, item.at(str_prop), data.specification);
				} catch (const std::out_of_range&) {
					if (is_valid(str_prop)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), item.at(str_prop), std::ref(data), std::move(str_prop)));
						offsprings = true;
					}
				}
			}

			const auto spc_item = data.specification;

			if (data.specification.name.empty()) {
				data.specification.found_field = true;
				if (data.specification.value) {
					index_item(properties, *data.specification.value, data);
				}
			} else {
				if (data.specification.full_name.empty()) {
					data.specification.full_name.assign(data.specification.name);
				} else {
					data.specification.full_name.append(DB_OFFSPRING_UNION).append(data.specification.name);
				}
				if (data.specification.value) {
					auto subproperties = get_subproperties(properties, data.specification);
					index_item(subproperties, *data.specification.value, data);
				}
			}

			for (auto& task : tasks) {
				data.specification = spc_item;
				task.get();
			}
		} else {
			index_item(properties, item, data);
		}
	}

	if (offsprings) {
		set_type_to_object(properties);
	}

	data.specification = spc_start;
}


void
Schema::index_item(MsgPack& properties, const MsgPack& value, data_t& data)
{
	try {
		if unlikely(!data.specification.set_type) {
			validate_required_data(properties, &value, data.specification);
		}

		switch (data.specification.index) {
			case Index::VALUE:
				return index_values(properties, value, data);
			case Index::TERM:
				return index_terms(properties, value, data);
			case Index::TEXT:
				return index_texts(properties, value, data);
			case Index::ALL:
				return index_values(properties, value, data, true);
		}
	} catch (const DummyException&) { }
}


void
Schema::validate_required_data(MsgPack& properties, const MsgPack* value, specification_t& specification)
{
	L_CALL(this, "Schema::validate_required_data()");

	if (specification.sep_types[2] == NO_TYPE && value) {
		set_type(*value, specification);
	}

	if (!specification.full_name.empty()) {
		// Process RESERVED_ACCURACY and RESERVED_ACC_PREFIX
		std::set<double> set_acc;
		switch (specification.sep_types[2]) {
			case GEO_TYPE: {
				if (!specification.doc_acc) {
					specification.doc_acc = std::make_unique<const MsgPack>(def_accuracy_geo);
				}

				try {
					specification.accuracy.push_back(specification.doc_acc->at(0).get_bool());
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, partials value in %s: %s must be boolean", RESERVED_ACCURACY, GEO_STR);
				}

				try {
					auto error = specification.doc_acc->at(1).get_f64();
					specification.accuracy.push_back(error > HTM_MAX_ERROR ? HTM_MAX_ERROR : error < HTM_MIN_ERROR ? HTM_MIN_ERROR : error);
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, error value in %s: %s must be positive integer", RESERVED_ACCURACY, GEO_STR);
				} catch (const std::out_of_range&) {
					specification.accuracy.push_back(def_accuracy_geo.at(1).get_f64());
				}

				try {
					const auto it_e = specification.doc_acc->end();
					for (auto it = specification.doc_acc->begin() + 2; it != it_e; ++it) {
						uint64_t val_acc = (*it).get_u64();
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
					for (const auto _accuracy : *specification.doc_acc) {
						std::string str_accuracy(lower_string(_accuracy.get_str()));
						if (str_accuracy == str_time[5]) {
							set_acc.insert(toUType(unitTime::YEAR));
						} else if (str_accuracy == str_time[4]) {
							set_acc.insert(toUType(unitTime::MONTH));
						} else if (str_accuracy == str_time[3]) {
							set_acc.insert(toUType(unitTime::DAY));
						} else if (str_accuracy == str_time[2]) {
							set_acc.insert(toUType(unitTime::HOUR));
						} else if (str_accuracy == str_time[1]) {
							set_acc.insert(toUType(unitTime::MINUTE));
						} else if (str_accuracy == str_time[0]) {
							set_acc.insert(toUType(unitTime::SECOND));
						} else {
							throw MSG_ClientError("Data inconsistency, %s: %s must be subset of {%s, %s, %s, %s, %s, %s} (%s not supported)", RESERVED_ACCURACY, DATE_STR, str_time[0].c_str(), str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str(), str_accuracy.c_str());
						}
					}
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, %s in %s must be subset of {%s, %s, %s, %s, %s, %s}", RESERVED_ACCURACY, DATE_STR, str_time[0].c_str(), str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
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
					for (const auto _accuracy : *specification.doc_acc) {
						set_acc.insert(_accuracy.get_u64());
					}
					break;
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, %s: %s must be an array of positive numbers", RESERVED_ACCURACY, specification.sep_types[2]);
				}
			}
			case NO_TYPE:
				throw MSG_Error("%s must be defined for validate data to index", RESERVED_TYPE);
		}

		size_t size_acc = set_acc.size();
		if (size_acc) {
			if (specification.acc_prefix.empty()) {
				for (const auto& acc : set_acc) {
					specification.acc_prefix.push_back(get_prefix(specification.full_name + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, specification.sep_types[2]));
				}
			} else if (specification.acc_prefix.size() != size_acc) {
				throw MSG_ClientError("Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
			}

			specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
			properties[RESERVED_ACCURACY] = specification.accuracy;
			properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
		}

		to_store.store(true);

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
	} else if (specification.index == Index::VALUE) {
		// For index::VALUE is neccesary the field_name.
		throw MSG_DummyException();
	}
}


void
Schema::index_texts(MsgPack& properties, const MsgPack& texts, data_t& data)
{
	L_CALL(this, "Schema::index_texts()");

	// L_INDEX(this, "Texts => Specifications: %s", data.specification.to_string().c_str());
	if (!data.specification.found_field && !data.specification.dynamic) {
		throw MSG_ClientError("%s is not dynamic", data.specification.full_name.c_str());
	}

	if (data.specification.store) {
		if (data.specification.bool_term) {
			throw MSG_ClientError("A boolean term can not be indexed as text");
		}

		try {
			if (texts.get_type() == msgpack::type::ARRAY) {
				set_type_to_array(properties);
				size_t pos = 0;
				for (auto text : texts) {
					index_text(data, text.get_str(), pos++);
				}
			} else {
				index_text(data, texts.get_str(), 0);
			}
		} catch (const msgpack::type_error&) {
			throw MSG_ClientError("%s should be a string or array of strings", RESERVED_TEXTS);
		}
	}
}


void
Schema::index_text(data_t& data, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_text()");

	// Xapian::WritableDatabase *wdb = nullptr;

	Xapian::TermGenerator term_generator;
	term_generator.set_document(data.doc);
	term_generator.set_stemmer(Xapian::Stem(data.specification.language[getPos(pos, data.specification.language.size())]));
	if (data.specification.spelling[getPos(pos, data.specification.spelling.size())]) {
		// wdb = static_cast<Xapian::WritableDatabase *>(database->db.get());
		// term_generator.set_database(*wdb);
		// term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		term_generator.set_stemming_strategy((Xapian::TermGenerator::stem_strategy)data.specification.analyzer[getPos(pos, data.specification.analyzer.size())]);
	}

	if (data.specification.positions[getPos(pos, data.specification.positions.size())]) {
		if (data.specification.prefix.empty()) {
			term_generator.index_text(serialise_val, data.specification.weight[getPos(pos, data.specification.weight.size())]);
		} else {
			term_generator.index_text(serialise_val, data.specification.weight[getPos(pos, data.specification.weight.size())], data.specification.prefix);
		}
		L_INDEX(this, "Text index with positions = %s: %s", data.specification.prefix.c_str(), serialise_val.c_str());
	} else {
		if (data.specification.prefix.empty()) {
			term_generator.index_text_without_positions(serialise_val, data.specification.weight[getPos(pos, data.specification.weight.size())]);
		} else {
			term_generator.index_text_without_positions(serialise_val, data.specification.weight[getPos(pos, data.specification.weight.size())], data.specification.prefix);
		}
		L_INDEX(this, "Text to Index => %s: %s", data.specification.prefix.c_str(), serialise_val.c_str());
	}
}


void
Schema::index_terms(MsgPack& properties, const MsgPack& terms, data_t& data)
{
	L_CALL(this, "Schema::index_terms()");

	// L_INDEX(this, "Terms => Specifications: %s", data.specification.to_string().c_str());
	if (!data.specification.found_field && !data.specification.dynamic) {
		throw MSG_ClientError("%s is not dynamic", data.specification.full_name.c_str());
	}

	if (data.specification.store) {
		if (terms.get_type() == msgpack::type::ARRAY) {
			set_type_to_array(properties);
			size_t pos = 0;
			for (auto term : terms) {
				index_term(data, Serialise::serialise(data.specification.sep_types[2], term), pos++);
			}
		} else {
			index_term(data, Serialise::serialise(data.specification.sep_types[2], terms), 0);
		}
	}
}


void
Schema::index_term(data_t& data, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_term()");

	if (serialise_val.empty()) {
		return;
	}

	if (data.specification.sep_types[2] == STRING_TYPE && !data.specification.bool_term) {
		if (serialise_val.find(" ") != std::string::npos) {
			return index_text(data, std::move(serialise_val), pos);
		}
		to_lower(serialise_val);
	}

	L_INDEX(this, "Term[%d] -> %s: %s", pos, data.specification.prefix.c_str(), serialise_val.c_str());
	std::string nameterm(prefixed(serialise_val, data.specification.prefix));
	unsigned position = data.specification.position[getPos(pos, data.specification.position.size())];
	if (position) {
		if (data.specification.bool_term) {
			data.doc.add_posting(nameterm, position, 0);
		} else {
			data.doc.add_posting(nameterm, position, data.specification.weight[getPos(pos, data.specification.weight.size())]);
		}
		L_INDEX(this, "Bool: %s  Posting: %s", data.specification.bool_term ? "true" : "false", repr(nameterm).c_str());
	} else {
		if (data.specification.bool_term) {
			data.doc.add_boolean_term(nameterm);
		} else {
			data.doc.add_term(nameterm, data.specification.weight[getPos(pos, data.specification.weight.size())]);
		}
		L_INDEX(this, "Bool: %s  Term: %s", data.specification.bool_term ? "true" : "false", repr(nameterm).c_str());
	}
}


void
Schema::index_values(MsgPack& properties, const MsgPack& values, data_t& data, bool is_term)
{
	L_CALL(this, "Schema::index_values()");

	// L_INDEX(this, "Values => Specifications: %s", data.specification.to_string().c_str());
	if (!(data.specification.found_field || data.specification.dynamic)) {
		throw MSG_ClientError("%s is not dynamic", data.specification.full_name.c_str());
	}

	if (data.specification.store) {
		StringSet& s = data.map_values[data.specification.slot];
		size_t pos = 0;
		if (values.get_type() == msgpack::type::ARRAY) {
			set_type_to_array(properties);
			for (auto value : values) {
				index_value(data, value, s, pos, is_term);
			}
		} else {
			index_value(data, values, s, pos, is_term);
		}
		L_INDEX(this, "Slot: %u serialized: %s", data.specification.slot, repr(s.serialise()).c_str());
	}
}


void
Schema::index_value(data_t& data, const MsgPack& value, StringSet& s, size_t& pos, bool is_term) const
{
	L_CALL(this, "Schema::index_value()");

	std::string value_v;

	// Index terms generated by accuracy.
	switch (data.specification.sep_types[2]) {
		case FLOAT_TYPE:
		case INTEGER_TYPE:
		case POSITIVE_TYPE: {
			try {
				value_v.assign(Serialise::serialise(data.specification.sep_types[2], value));
				int64_t int_value = static_cast<int64_t>(value.get_f64());
				auto it = data.specification.acc_prefix.begin();
				for (const auto& acc : data.specification.accuracy) {
					std::string term_v = Serialise::integer(data.specification.sep_types[2], int_value - int_value % (uint64_t)acc);
					data.doc.add_term(prefixed(term_v, *(it++)));
				}
				s.insert(value_v);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for numeric: %s", value.to_json_string().c_str());
			}
		}
		case DATE_TYPE: {
			Datetime::tm_t tm;
			value_v.assign(Serialise::date(value, tm));
			auto it = data.specification.acc_prefix.begin();
			for (const auto& acc : data.specification.accuracy) {
				switch ((unitTime)acc) {
					case unitTime::YEAR:
						data.doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "y"), *it++));
						break;
					case unitTime::MONTH:
						data.doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "M"), *it++));
						break;
					case unitTime::DAY:
						data.doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "d"), *it++));
						break;
					case unitTime::HOUR:
						data.doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "h"), *it++));
						break;
					case unitTime::MINUTE:
						data.doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "m"), *it++));
						break;
					case unitTime::SECOND:
						data.doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "s"), *it++));
						break;
				}
			}
			s.insert(value_v);
			break;
		}
		case GEO_TYPE: {
			std::string ewkt(value.get_str());
			if (is_term) {
				value_v.assign(Serialise::ewkt(ewkt));
			}

			RangeList ranges;
			CartesianUSet centroids;

			EWKT_Parser::getRanges(ewkt, data.specification.accuracy[0], data.specification.accuracy[1], ranges, centroids);

			// Index Values and looking for terms generated by accuracy.
			std::unordered_set<std::string> set_terms;
			for (const auto& range : ranges) {
				int idx = -1;
				uint64_t val;
				if (range.start != range.end) {
					std::bitset<SIZE_BITS_ID> b1(range.start), b2(range.end), res;
					for (idx = SIZE_BITS_ID - 1; b1.test(idx) == b2.test(idx); --idx) {
						res.set(idx, b1.test(idx));
					}
					val = res.to_ullong();
				} else {
					val = range.start;
				}
				for (size_t i = 2; i < data.specification.accuracy.size(); ++i) {
					int pos = START_POS - data.specification.accuracy[i] * 2;
					if (idx < pos) {
						uint64_t vterm = val >> pos;
						set_terms.insert(prefixed(Serialise::trixel_id(vterm), data.specification.acc_prefix[i - 2]));
					} else {
						break;
					}
				}
			}
			// Insert terms generated by accuracy.
			for (const auto& term : set_terms) {
				data.doc.add_term(term);
			}

			s.insert(Serialise::geo(ranges, centroids));
			break;
		}
		default:
			value_v.assign(Serialise::serialise(data.specification.sep_types[2], value));
			s.insert(value_v);
			break;
	}

	// Index like a term.
	if (is_term) {
		index_term(data, std::move(value_v), pos++);
	}
}


data_field_t
Schema::get_data_field(const std::string& field_name) const
{
	L_CALL(this, "Schema::get_data_field()");

	data_field_t res = { Xapian::BAD_VALUENO, "", NO_TYPE, std::vector<double>(), std::vector<std::string>(), false };

	if (field_name.empty()) {
		return res;
	}

	std::vector<std::string> fields;
	stringTokenizer(field_name, DB_OFFSPRING_UNION, fields);
	try {
		auto properties = getPropertiesSchema().path(fields);

		res.type = properties.at(RESERVED_TYPE).at(2).get_u64();
		if (res.type == NO_TYPE) {
			return res;
		}

		res.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).get_u64());

		auto prefix = properties.at(RESERVED_PREFIX);
		res.prefix = prefix.get_str();

		res.bool_term = properties.at(RESERVED_BOOL_TERM).get_bool();

		// Strings and booleans do not have accuracy.
		if (res.type != STRING_TYPE && res.type != BOOLEAN_TYPE) {
			for (const auto acc : properties.at(RESERVED_ACCURACY)) {
				res.accuracy.push_back(acc.get_f64());
			}

			for (const auto acc_p : properties.at(RESERVED_ACC_PREFIX)) {
				res.acc_prefix.push_back(acc_p.get_str());
			}
		}
	} catch (const std::exception&) { }

	return res;
}


data_field_t
Schema::get_slot_field(const std::string& field_name) const
{
	L_CALL(this, "Schema::get_slot_field()");

	data_field_t res = { Xapian::BAD_VALUENO, "", NO_TYPE, std::vector<double>(), std::vector<std::string>(), false };

	if (field_name.empty()) {
		return res;
	}

	std::vector<std::string> fields;
	stringTokenizer(field_name, DB_OFFSPRING_UNION, fields);
	try {
		auto properties = getPropertiesSchema().path(fields);
		res.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).get_u64());
		res.type = properties.at(RESERVED_TYPE).at(2).get_u64();
	} catch (const std::exception&) { }

	return res;
}
