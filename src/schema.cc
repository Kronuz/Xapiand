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


static const MsgPack def_accuracy_geo  { true, 0.2, 0, 5, 10, 15, 20, 25 };
static const MsgPack def_accuracy_num  { 100, 1000, 10000, 100000 };
static const MsgPack def_accuracy_date { "hour", "day", "month", "year" };


const specification_t default_spc;


const std::unordered_map<std::string, dispatch_reserved> map_dispatch_document({
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


const std::unordered_map<std::string, dispatch_reserved> map_dispatch_properties({
	{ RESERVED_WEIGHT,       &Schema::update_weight            },
	{ RESERVED_POSITION,     &Schema::update_position          },
	{ RESERVED_LANGUAGE,     &Schema::update_language          },
	{ RESERVED_SPELLING,     &Schema::update_spelling          },
	{ RESERVED_POSITIONS,    &Schema::update_positions         },
	{ RESERVED_ACCURACY,     &Schema::update_accuracy          },
	{ RESERVED_ACC_PREFIX,   &Schema::update_acc_prefix        },
	{ RESERVED_STORE,        &Schema::update_store             },
	{ RESERVED_TYPE,         &Schema::update_type              },
	{ RESERVED_ANALYZER,     &Schema::update_analyzer          },
	{ RESERVED_DYNAMIC,      &Schema::update_dynamic           },
	{ RESERVED_D_DETECTION,  &Schema::update_d_detection       },
	{ RESERVED_N_DETECTION,  &Schema::update_n_detection       },
	{ RESERVED_G_DETECTION,  &Schema::update_g_detection       },
	{ RESERVED_B_DETECTION,  &Schema::update_b_detection       },
	{ RESERVED_S_DETECTION,  &Schema::update_s_detection       },
	{ RESERVED_BOOL_TERM,    &Schema::update_bool_term         },
	{ RESERVED_SLOT,         &Schema::update_slot              },
	{ RESERVED_INDEX,        &Schema::update_index             },
	{ RESERVED_PREFIX,       &Schema::update_prefix            },
});


const std::unordered_map<std::string, dispatch_root> map_dispatch_root({
	{ RESERVED_TEXTS,        &Schema::process_texts             },
	{ RESERVED_VALUES,       &Schema::process_values            },
	{ RESERVED_TERMS,        &Schema::process_terms             },
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


Schema::Schema(const std::shared_ptr<const MsgPack>& other)
	: schema(other)
{
	if (schema->is_null()) {
		MsgPack new_schema = {
			{ RESERVED_VERSION, DB_VERSION_SCHEMA },
			{ RESERVED_SCHEMA, nullptr },
		};
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
		prop_id[RESERVED_INDEX] = static_cast<unsigned>(Index::ALL);
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
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;
	specification.bool_term = default_spc.bool_term;
	specification.set_type = default_spc.set_type;
	specification.name = default_spc.name;
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

	try {
		auto& _type = get_mutable(specification.full_name).at(RESERVED_TYPE).at(1);
		if (_type.as_u64() == NO_TYPE) {
			_type = ARRAY_TYPE;
		}
	} catch (const std::out_of_range&) { }
}


void
Schema::set_type_to_object()
{
	L_CALL(this, "Schema::set_type_to_object()");

	try {
		auto& _type = get_mutable(specification.full_name).at(RESERVED_TYPE).at(0);
		if (_type.as_u64() == NO_TYPE) {
			_type = OBJECT_TYPE;
		}
	} catch (const std::out_of_range&) { }
}


std::string
Schema::to_string(bool prettify) const
{
	L_CALL(this, "Schema::to_string()");

	auto schema_readable = *schema;
	auto& properties = schema_readable.at(RESERVED_SCHEMA);
	if unlikely(properties.is_null()) {
		schema_readable.erase(RESERVED_SCHEMA);
	} else {
		readable(properties, true);
	}

	return schema_readable.to_string(prettify);
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
	try {
		if (sep_types[2] == DATE_TYPE) {
			for (auto& _accuracy : properties.at(RESERVED_ACCURACY)) {
				_accuracy = str_time[_accuracy.as_f64()];
			}
		} else if (sep_types[2] == GEO_TYPE) {
			auto& _partials = properties.at(RESERVED_ACCURACY).at(0);
			_partials = _partials.as_f64() ? true : false;
		}
	} catch (const std::out_of_range&) { }
}


void
Schema::readable_analyzer(MsgPack& prop_analyzer, MsgPack&)
{
	for (auto& _analyzer : prop_analyzer) {
		_analyzer = str_analyzer[_analyzer.as_u64()];
	}
}


void
Schema::readable_index(MsgPack& prop_index, MsgPack&)
{
	prop_index = str_index[prop_index.as_u64()];
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
				auto _analyzer(upper_string(analyzer.as_string()));
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
			auto _analyzer(upper_string(doc_analyzer.as_string()));
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
		std::set<std::string> set_acc_prefix;
		try {
			for (const auto& _acc_prefix : doc_acc_prefix) {
				set_acc_prefix.insert(_acc_prefix.as_string());
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
	// RESERVED_INDEX is heritable and can change if fixed_index is false.
	if unlikely(specification.fixed_index) {
		return;
	}

	try {
		auto _index(upper_string(doc_index.as_string()));
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
			get_mutable(specification.full_name)[RESERVED_INDEX] = (unsigned)specification.index;
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Data inconsistency, %s must be string", RESERVED_INDEX);
	}
}


void
Schema::process_store(const MsgPack& doc_store)
{
	// RESERVED_STORE is heritable and can change.
	try {
		specification.store = doc_store.as_bool();

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


void
Schema::index(const MsgPack& properties, const MsgPack& object, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index()");

	try {
		TaskVector tasks;
		tasks.reserve(object.size());
		for (const auto& item_key : object) {
			const auto str_key = item_key.as_string();
			try {
				auto func = map_dispatch_document.at(str_key);
				(this->*func)(object.at(str_key));
			} catch (const std::out_of_range&) {
				if (is_valid(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(doc), std::move(str_key)));
				} else {
					try {
						auto func = map_dispatch_root.at(str_key);
						tasks.push_back(std::async(std::launch::deferred, func, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(doc)));
					} catch (const std::out_of_range&) { }
				}
			}
		}

		restart_specification();
		const specification_t spc_start = specification;
		for (auto& task : tasks) {
			task.get();
			specification = spc_start;
		}

		for (const auto& elem : map_values) {
			doc.add_value(elem.first, elem.second.serialise());
		}
	} catch (...) {
		mut_schema.reset();
		throw;
	}
}


void
Schema::process_values(const MsgPack& properties, const MsgPack& doc_values, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_values()");

	specification.index = Index::VALUE;
	fixed_index(properties, doc_values, doc);
}


void
Schema::process_texts(const MsgPack& properties, const MsgPack& doc_texts, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_texts()");

	specification.index = Index::TEXT;
	fixed_index(properties, doc_texts, doc);
}


void
Schema::process_terms(const MsgPack& properties, const MsgPack& doc_terms, Xapian::Document& doc)
{
	L_CALL(this, "Schema::process_terms()");

	specification.index = Index::TERM;
	fixed_index(properties, doc_terms, doc);
}


void
Schema::fixed_index(const MsgPack& properties, const MsgPack& object, Xapian::Document& doc)
{
	specification.fixed_index = true;
	switch (object.type()) {
		case msgpack::type::MAP:
			return index_object(properties, object, doc);
		case msgpack::type::ARRAY:
			return index_array(properties, object, doc);
		default:
			throw MSG_ClientError("%s must be an object or an array of objects", RESERVED_VALUES);
	}
}


void
Schema::index_object(const MsgPack& parent_properties, const MsgPack& object, Xapian::Document& doc, const std::string& name)
{
	L_CALL(this, "Schema::index_object()");

	const auto spc_start = specification;
	const MsgPack* properties = nullptr;
	if (name.empty()) {
		properties = &parent_properties;
		specification.found_field = true;
	} else {
		if (specification.full_name.empty()) {
			specification.full_name.assign(name);
		} else {
			specification.full_name.append(DB_OFFSPRING_UNION).append(name);
		}
		specification.name.assign(name);
		properties = &get_subproperties(parent_properties);
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
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(*properties), std::ref(object.at(str_key)), std::ref(doc), std::move(str_key)));
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
					index_item(*specification.value, doc);
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
					index_item(*specification.value, doc);
				}
			}

			if (offsprings) {
				set_type_to_object();
			}

			for (auto& task : tasks) {
				specification = spc_object;
				task.get();
			}
			break;
		}
		case msgpack::type::ARRAY:
			set_type_to_array();
			index_array(*properties, object, doc);
			break;
		default:
			index_item(object, doc);
			break;
	}

	specification = spc_start;
}


void
Schema::index_array(const MsgPack& properties, const MsgPack& array, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_array()");

	const auto spc_start = specification;
	bool offsprings = false;
	for (const auto& item : array) {
		if (item.is_map()) {
			TaskVector tasks;
			tasks.reserve(item.size());
			specification.value = nullptr;

			for (const auto& property : item) {
				auto str_prop = property.as_string();
				try {
					auto func = map_dispatch_document.at(str_prop);
					(this->*func)(item.at(str_prop));
				} catch (const std::out_of_range&) {
					if (is_valid(str_prop)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(item.at(str_prop)), std::ref(doc), std::move(str_prop)));
						offsprings = true;
					}
				}
			}

			const auto spc_item = specification;

			if (specification.name.empty()) {
				specification.found_field = true;
				if (specification.value) {
					index_item(*specification.value, doc);
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
					index_item(*specification.value, doc);
				}
			}

			for (auto& task : tasks) {
				specification = spc_item;
				task.get();
			}
		} else {
			index_item(item, doc);
		}
	}

	if (offsprings) {
		set_type_to_object();
	}

	specification = spc_start;
}


void
Schema::index_item(const MsgPack& value, Xapian::Document& doc)
{
	try {
		if unlikely(!specification.set_type) {
			validate_required_data(&value);
		}

		switch (specification.index) {
			case Index::VALUE:
				return index_values(value, doc);
			case Index::TERM:
				return index_terms(value, doc);
			case Index::TEXT:
				return index_texts(value, doc);
			case Index::ALL:
				return index_values(value, doc, true);
		}
	} catch (const DummyException&) { }
}


void
Schema::validate_required_data(const MsgPack* value)
{
	L_CALL(this, "Schema::validate_required_data()");

	if (specification.sep_types[2] == NO_TYPE && value) {
		set_type(*value);
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
					specification.accuracy.push_back(specification.doc_acc->at(0).as_bool());
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, partials value in %s: %s must be boolean", RESERVED_ACCURACY, GEO_STR);
				}

				try {
					auto error = specification.doc_acc->at(1).as_f64();
					specification.accuracy.push_back(error > HTM_MAX_ERROR ? HTM_MAX_ERROR : error < HTM_MIN_ERROR ? HTM_MIN_ERROR : error);
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, error value in %s: %s must be positive integer", RESERVED_ACCURACY, GEO_STR);
				} catch (const std::out_of_range&) {
					specification.accuracy.push_back(def_accuracy_geo.at(1).as_f64());
				}

				try {
					const auto it_e = specification.doc_acc->end();
					for (auto it = specification.doc_acc->begin() + 2; it != it_e; ++it) {
						auto val_acc = (*it).as_u64();
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
					for (const auto& _accuracy : *specification.doc_acc) {
						set_acc.insert(_accuracy.as_u64());
					}
					break;
				} catch (const msgpack::type_error&) {
					throw MSG_ClientError("Data inconsistency, %s: %s must be an array of positive numbers", RESERVED_ACCURACY, specification.sep_types[2]);
				}
			}
			case NO_TYPE:
				throw MSG_Error("%s must be defined for validate data to index", RESERVED_TYPE);
		}

		auto& properties = get_mutable(specification.full_name);

		auto size_acc = set_acc.size();
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
Schema::index_texts(const MsgPack& texts, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_texts()");

	// L_INDEX(this, "Texts => Specifications: %s", specification.to_string().c_str());
	if (!specification.found_field && !specification.dynamic) {
		throw MSG_ClientError("%s is not dynamic", specification.full_name.c_str());
	}

	if (specification.store) {
		if (specification.bool_term) {
			throw MSG_ClientError("A boolean term can not be indexed as text");
		}

		try {
			if (texts.is_array()) {
				set_type_to_array();
				size_t pos = 0;
				for (const auto& text : texts) {
					index_text(doc, text.as_string(), pos++);
				}
			} else {
				index_text(doc, texts.as_string(), 0);
			}
		} catch (const msgpack::type_error&) {
			throw MSG_ClientError("%s should be a string or array of strings", RESERVED_TEXTS);
		}
	}
}


void
Schema::index_text(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_text()");

	// Xapian::WritableDatabase *wdb = nullptr;

	Xapian::TermGenerator term_generator;
	term_generator.set_document(doc);
	term_generator.set_stemmer(Xapian::Stem(specification.language[getPos(pos, specification.language.size())]));
	if (specification.spelling[getPos(pos, specification.spelling.size())]) {
		// wdb = static_cast<Xapian::WritableDatabase *>(database->db.get());
		// term_generator.set_database(*wdb);
		// term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
		term_generator.set_stemming_strategy((Xapian::TermGenerator::stem_strategy)specification.analyzer[getPos(pos, specification.analyzer.size())]);
	}

	if (specification.positions[getPos(pos, specification.positions.size())]) {
		if (specification.prefix.empty()) {
			term_generator.index_text(serialise_val, specification.weight[getPos(pos, specification.weight.size())]);
		} else {
			term_generator.index_text(serialise_val, specification.weight[getPos(pos, specification.weight.size())], specification.prefix);
		}
		L_INDEX(this, "Text index with positions = %s: %s", specification.prefix.c_str(), serialise_val.c_str());
	} else {
		if (specification.prefix.empty()) {
			term_generator.index_text_without_positions(serialise_val, specification.weight[getPos(pos, specification.weight.size())]);
		} else {
			term_generator.index_text_without_positions(serialise_val, specification.weight[getPos(pos, specification.weight.size())], specification.prefix);
		}
		L_INDEX(this, "Text to Index => %s: %s", specification.prefix.c_str(), serialise_val.c_str());
	}
}


void
Schema::index_terms(const MsgPack& terms, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_terms()");

	// L_INDEX(this, "Terms => Specifications: %s", specification.to_string().c_str());
	if (!specification.found_field && !specification.dynamic) {
		throw MSG_ClientError("%s is not dynamic", specification.full_name.c_str());
	}

	if (specification.store) {
		if (terms.is_array()) {
			set_type_to_array();
			size_t pos = 0;
			for (const auto& term : terms) {
				index_term(doc, Serialise::serialise(specification.sep_types[2], term), pos++);
			}
		} else {
			index_term(doc, Serialise::serialise(specification.sep_types[2], terms), 0);
		}
	}
}


void
Schema::index_term(Xapian::Document& doc, std::string&& serialise_val, size_t pos) const
{
	L_CALL(this, "Schema::index_term()");

	if (serialise_val.empty()) {
		return;
	}

	if (specification.sep_types[2] == STRING_TYPE && !specification.bool_term) {
		if (serialise_val.find(" ") != std::string::npos) {
			return index_text(doc, std::move(serialise_val), pos);
		}
		to_lower(serialise_val);
	}

	L_INDEX(this, "Term[%d] -> %s: %s", pos, specification.prefix.c_str(), serialise_val.c_str());
	std::string nameterm(prefixed(serialise_val, specification.prefix));
	unsigned position = specification.position[getPos(pos, specification.position.size())];
	if (position) {
		if (specification.bool_term) {
			doc.add_posting(nameterm, position, 0);
		} else {
			doc.add_posting(nameterm, position, specification.weight[getPos(pos, specification.weight.size())]);
		}
		L_INDEX(this, "Bool: %s  Posting: %s", specification.bool_term ? "true" : "false", repr(nameterm).c_str());
	} else {
		if (specification.bool_term) {
			doc.add_boolean_term(nameterm);
		} else {
			doc.add_term(nameterm, specification.weight[getPos(pos, specification.weight.size())]);
		}
		L_INDEX(this, "Bool: %s  Term: %s", specification.bool_term ? "true" : "false", repr(nameterm).c_str());
	}
}


void
Schema::index_values(const MsgPack& values, Xapian::Document& doc, bool is_term)
{
	L_CALL(this, "Schema::index_values()");

	// L_INDEX(this, "Values => Specifications: %s", specification.to_string().c_str());
	if (!(specification.found_field || specification.dynamic)) {
		throw MSG_ClientError("%s is not dynamic", specification.full_name.c_str());
	}

	if (specification.store) {
		StringSet& s = map_values[specification.slot];
		size_t pos = 0;
		if (values.is_array()) {
			set_type_to_array();
			for (const auto& value : values) {
				index_value(doc, value, s, pos, is_term);
			}
		} else {
			index_value(doc, values, s, pos, is_term);
		}
		L_INDEX(this, "Slot: %u serialized: %s", specification.slot, repr(s.serialise()).c_str());
	}
}


void
Schema::index_value(Xapian::Document& doc, const MsgPack& value, StringSet& s, size_t& pos, bool is_term) const
{
	L_CALL(this, "Schema::index_value()");

	std::string value_v;

	// Index terms generated by accuracy.
	switch (specification.sep_types[2]) {
		case FLOAT_TYPE:
		case INTEGER_TYPE:
		case POSITIVE_TYPE: {
			try {
				value_v.assign(Serialise::serialise(specification.sep_types[2], value));
				int64_t int_value = static_cast<int64_t>(value.as_f64());
				auto it = specification.acc_prefix.begin();
				for (const auto& acc : specification.accuracy) {
					std::string term_v = Serialise::integer(specification.sep_types[2], int_value - int_value % (uint64_t)acc);
					doc.add_term(prefixed(term_v, *(it++)));
				}
				s.insert(value_v);
				break;
			} catch (const msgpack::type_error&) {
				throw MSG_ClientError("Format invalid for numeric: %s", value.to_string().c_str());
			}
		}
		case DATE_TYPE: {
			Datetime::tm_t tm;
			value_v.assign(Serialise::date(value, tm));
			auto it = specification.acc_prefix.begin();
			for (const auto& acc : specification.accuracy) {
				switch ((unitTime)acc) {
					case unitTime::YEAR:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "y"), *it++));
						break;
					case unitTime::MONTH:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "M"), *it++));
						break;
					case unitTime::DAY:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "d"), *it++));
						break;
					case unitTime::HOUR:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "h"), *it++));
						break;
					case unitTime::MINUTE:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "m"), *it++));
						break;
					case unitTime::SECOND:
						doc.add_term(prefixed(Serialise::date_with_math(tm, "//", "s"), *it++));
						break;
				}
			}
			s.insert(value_v);
			break;
		}
		case GEO_TYPE: {
			std::string ewkt(value.as_string());
			if (is_term) {
				value_v.assign(Serialise::ewkt(ewkt));
			}

			RangeList ranges;
			CartesianUSet centroids;

			EWKT_Parser::getRanges(ewkt, specification.accuracy[0], specification.accuracy[1], ranges, centroids);

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
				for (size_t i = 2; i < specification.accuracy.size(); ++i) {
					int pos = START_POS - specification.accuracy[i] * 2;
					if (idx < pos) {
						uint64_t vterm = val >> pos;
						set_terms.insert(prefixed(Serialise::trixel_id(vterm), specification.acc_prefix[i - 2]));
					} else {
						break;
					}
				}
			}
			// Insert terms generated by accuracy.
			for (const auto& term : set_terms) {
				doc.add_term(term);
			}

			s.insert(Serialise::geo(ranges, centroids));
			break;
		}
		default:
			value_v.assign(Serialise::serialise(specification.sep_types[2], value));
			s.insert(value_v);
			break;
	}

	// Index like a term.
	if (is_term) {
		index_term(doc, std::move(value_v), pos++);
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
		const auto& properties = schema->at(RESERVED_SCHEMA).path(fields);

		res.type = properties.at(RESERVED_TYPE).at(2).as_u64();
		if (res.type == NO_TYPE) {
			return res;
		}

		res.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).as_u64());
		res.prefix = properties.at(RESERVED_PREFIX).as_string();
		res.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();

		// Strings and booleans do not have accuracy.
		if (res.type != STRING_TYPE && res.type != BOOLEAN_TYPE) {
			for (const auto& acc : properties.at(RESERVED_ACCURACY)) {
				res.accuracy.push_back(acc.as_f64());
			}

			for (const auto& acc_p : properties.at(RESERVED_ACC_PREFIX)) {
				res.acc_prefix.push_back(acc_p.as_string());
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
		const auto& properties = schema->at(RESERVED_SCHEMA).path(fields);
		res.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).as_u64());
		res.type = properties.at(RESERVED_TYPE).at(2).as_u64();
	} catch (const std::exception&) { }

	return res;
}
