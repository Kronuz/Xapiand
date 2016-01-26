/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "log.h"
#include "database.h"


const specification_t default_spc;


specification_t::specification_t()
		: position({ 0 }),
		  weight({ 1 }),
		  language({ "en" }),
		  spelling({ false }),
		  positions({ false }),
		  analyzer({ Xapian::TermGenerator::STEM_SOME }),
		  slot(0),
		  sep_types({ NO_TYPE, NO_TYPE, NO_TYPE }),
		  index(Index::ALL),
		  store(true),
		  dynamic(true),
		  date_detection(true),
		  numeric_detection(true),
		  geo_detection(true),
		  bool_detection(true),
		  string_detection(true),
		  bool_term(false) { }


std::string
specification_t::to_string() const
{
	std::stringstream str;
	str << "\n{\n";
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


Schema::Schema() : to_store(false) { }


void
Schema::setDatabase(Database* _db)
{
	db = _db;

	// Reload schema.
	std::string s_schema;
	db->get_metadata(RESERVED_SCHEMA, s_schema);

	if (s_schema.empty()) {
		to_store = true;
		schema[RESERVED_VERSION] = DB_VERSION_SCHEMA;
		schema[RESERVED_SCHEMA];
	} else {
		to_store = false;
		schema = MsgPack(s_schema);
		try {
			auto version = schema.at(RESERVED_VERSION);
			if (version.get_f64() != DB_VERSION_SCHEMA) {
				throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
			}
		} catch (const std::out_of_range&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Schema is corrupt, you need provide a new one");
		}
	}
}


void
Schema::update_root(MsgPack& properties, const MsgPack& item_doc)
{
	// Reset specification
	specification = default_spc;

	auto properties_id = properties[RESERVED_ID];
	if (properties_id) {
		update(properties, RESERVED_SCHEMA, item_doc, true);
	} else {
		properties_id[RESERVED_TYPE] = std::vector<unsigned>({ NO_TYPE, NO_TYPE, STRING_TYPE });
		properties_id[RESERVED_INDEX] = static_cast<unsigned>(Index::ALL);
		properties_id[RESERVED_SLOT] = DB_SLOT_ID;
		properties_id[RESERVED_PREFIX] = DOCUMENT_ID_TERM_PREFIX;
		properties_id[RESERVED_BOOL_TERM] = true;
		to_store = true;
		insert(properties, item_doc, true);
	}
}


MsgPack
Schema::get_subproperties(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc)
{
	auto subproperties = properties[item_key];
	if (subproperties) {
		found_field = true;
		update(subproperties, item_key, item_doc);
	} else {
		to_store = true;
		found_field = false;
		insert(subproperties, item_doc);
	}

	return subproperties;
}


void
Schema::store()
{
	if (to_store) {
		db->set_metadata(RESERVED_SCHEMA, schema.to_string());
		to_store = false;
	}
}


void
Schema::update_specification(const MsgPack& item_doc)
{
	// RESERVED_POSITION is heritable and can change between documents.
	try {
		auto doc_position = item_doc.at(RESERVED_POSITION);
		specification.position.clear();
		if (doc_position.obj->type == msgpack::type::ARRAY) {
			for (const auto _position : doc_position) {
				specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
			}
		} else {
			specification.position.push_back(static_cast<unsigned>(doc_position.get_u64()));
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_POSITION);
	} catch (const std::out_of_range&) { }

	// RESERVED_WEIGHT is heritable and can change between documents.
	try {
		auto doc_weight = item_doc.at(RESERVED_WEIGHT);
		specification.weight.clear();
		if (doc_weight.obj->type == msgpack::type::ARRAY) {
			for (const auto _weight : doc_weight) {
				specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
			}
		} else {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.get_u64()));
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_WEIGHT);
	} catch (const std::out_of_range&) { }

	// RESERVED_LANGUAGE is heritable and can change between documents.
	try {
		auto doc_language = item_doc.at(RESERVED_LANGUAGE);
		specification.language.clear();
		if (doc_language.obj->type == msgpack::type::ARRAY) {
			for (const auto _language : doc_language) {
				std::string _str_language(_language.get_str());
				if (is_language(_str_language)) {
					specification.language.push_back(std::move(_str_language));
				} else {
					throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
				}
			}
		} else {
			std::string _str_language(doc_language.get_str());
			if (is_language(_str_language)) {
				specification.language.push_back(std::move(_str_language));
			} else {
				throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
			}
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string or array of strings", RESERVED_LANGUAGE);
	} catch (const std::out_of_range&) { }

	// RESERVED_SPELLING is heritable and can change between documents.
	try {
		auto doc_spelling = item_doc.at(RESERVED_SPELLING);
		specification.spelling.clear();
		if (doc_spelling.obj->type == msgpack::type::ARRAY) {
			for (const auto _spelling : doc_spelling) {
				specification.spelling.push_back(_spelling.get_bool());
			}
		} else {
			specification.spelling.push_back(doc_spelling.get_bool());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean or array of booleans", RESERVED_SPELLING);
	} catch (const std::out_of_range&) { }

	// RESERVED_POSITIONS is heritable and can change between documents.
	try {
		auto doc_positions = item_doc.at(RESERVED_POSITIONS);
		specification.positions.clear();
		if (doc_positions.obj->type == msgpack::type::ARRAY) {
			for (const auto _positions : doc_positions) {
				specification.positions.push_back(_positions.get_bool());
			}
		} else {
			specification.positions.push_back(doc_positions.get_bool());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean or array of booleans", RESERVED_POSITIONS);
	} catch (const std::out_of_range&) { }

	// RESERVED_ANALYZER is heritable and can change between documents.
	try {
		auto doc_analyzer = item_doc.at(RESERVED_ANALYZER);
		specification.analyzer.clear();
		if (doc_analyzer.obj->type == msgpack::type::ARRAY) {
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
					throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
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
				throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
			}
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string or array of strings", RESERVED_ANALYZER);
	} catch (const std::out_of_range&) { }

	// RESERVED_STORE is heritable and can change.
	try {
		specification.store = item_doc.at(RESERVED_STORE).get_bool();
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_STORE);
	} catch (const std::out_of_range&) { }

	// RESERVED_INDEX is heritable and can change.
	try {
		std::string _index(upper_string(item_doc.at(RESERVED_INDEX).get_str()));
		if (_index == str_index[0]) {
			specification.index = Index::ALL;
		} else if (_index == str_index[1]) {
			specification.index = Index::TERM;
		} else if (_index == str_index[2]) {
			specification.index = Index::VALUE;
		} else {
			throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string", RESERVED_INDEX);
	} catch (const std::out_of_range&) { }
}


void
Schema::set_type(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc)
{
	specification.sep_types[2] = get_type(item_doc);
	update_required_data(properties, item_key);
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
Schema::to_json_string(bool prettify)
{
	MsgPack schema_readable = schema.duplicate();
	auto properties = schema_readable.at(RESERVED_SCHEMA);
	for (const auto item_key : properties) {
		std::string str_key(item_key.get_str());
		if (!is_reserved(str_key) || str_key == RESERVED_ID) {
			readable(properties.at(str_key));
		}
	}

	return schema_readable.to_json_string(prettify);
}


void
Schema::insert(MsgPack& properties, const MsgPack& item_doc, bool is_root)
{
	// Restarting reserved words than are not inherited.
	specification.accuracy.clear();
	specification.acc_prefix.clear();
	specification.sep_types = default_spc.sep_types;
	specification.bool_term = default_spc.bool_term;
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;

	// If item_doc is not a MAP, there are not properties to insert
	if (item_doc.obj->type != msgpack::type::MAP) {
		return;
	}

	try {
		specification.date_detection = item_doc.at(RESERVED_D_DETECTION).get_bool();
		properties[RESERVED_D_DETECTION] = specification.date_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_D_DETECTION);
	} catch (const std::out_of_range&) { }

	try {
		specification.numeric_detection = item_doc.at(RESERVED_N_DETECTION).get_bool();
		properties[RESERVED_N_DETECTION] = specification.numeric_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_N_DETECTION);
	} catch (const std::out_of_range&) { }

	try {
		specification.geo_detection = item_doc.at(RESERVED_G_DETECTION).get_bool();
		properties[RESERVED_G_DETECTION] = specification.geo_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_G_DETECTION);
	} catch (const std::out_of_range&) { }

	try {
		specification.bool_detection = item_doc.at(RESERVED_B_DETECTION).get_bool();
		properties[RESERVED_B_DETECTION] = specification.bool_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_B_DETECTION);
	} catch (const std::out_of_range&) { }

	try {
		specification.string_detection = item_doc.at(RESERVED_S_DETECTION).get_bool();
		properties[RESERVED_S_DETECTION] = specification.string_detection;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_S_DETECTION);
	} catch (const std::out_of_range&) { }

	try {
		auto doc_position = item_doc.at(RESERVED_POSITION);
		specification.position.clear();
		if (doc_position.obj->type == msgpack::type::ARRAY) {
			for (const auto _position : doc_position) {
				specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
			}
		} else {
			specification.position.push_back(static_cast<unsigned>(doc_position.get_u64()));
		}
		properties[RESERVED_POSITION] = specification.position;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_POSITION);
	} catch (const std::out_of_range&) { }

	try {
		auto doc_weight = item_doc.at(RESERVED_WEIGHT);
		specification.weight.clear();
		if (doc_weight.obj->type == msgpack::type::ARRAY) {
			for (const auto _weight : doc_weight) {
				specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
			}
		} else {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.get_u64()));
		}
		properties[RESERVED_WEIGHT] = specification.weight;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_WEIGHT);
	} catch (const std::out_of_range&) { }

	try {
		auto doc_language = item_doc.at(RESERVED_LANGUAGE);
		specification.language.clear();
		if (doc_language.obj->type == msgpack::type::ARRAY) {
			for (const auto _language : doc_language) {
				std::string _str_language(_language.get_str());
				if (is_language(_str_language)) {
					specification.language.push_back(std::move(_str_language));
				} else {
					throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
				}
			}
		} else {
			std::string _str_language(doc_language.get_str());
			if (is_language(_str_language)) {
				specification.language.push_back(std::move(_str_language));
			}
		}
		properties[RESERVED_LANGUAGE] = specification.language;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string or array of strings", RESERVED_LANGUAGE);
	} catch (const std::out_of_range&) { }

	try {
		auto doc_spelling = item_doc.at(RESERVED_SPELLING);
		specification.spelling.clear();
		if (doc_spelling.obj->type == msgpack::type::ARRAY) {
			for (const auto _spelling : doc_spelling) {
				specification.spelling.push_back(_spelling.get_bool());
			}
		} else {
			specification.spelling.push_back(doc_spelling.get_bool());
		}
		properties[RESERVED_SPELLING] = specification.spelling;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean or array of booleans", RESERVED_SPELLING);
	} catch (const std::out_of_range&) { }

	try {
		auto doc_positions = item_doc.at(RESERVED_POSITIONS);
		specification.positions.clear();
		if (doc_positions.obj->type == msgpack::type::ARRAY) {
			for (const auto _positions : doc_positions) {
				specification.positions.push_back(_positions.get_bool());
			}
		} else {
			specification.positions.push_back(doc_positions.get_bool());
		}
		properties[RESERVED_POSITIONS] = specification.positions;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean or array of booleans", RESERVED_POSITIONS);
	} catch (const std::out_of_range&) { }

	try {
		specification.store = item_doc.at(RESERVED_STORE).get_bool();
		properties[RESERVED_STORE] = specification.store;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_STORE);
	} catch (const std::out_of_range&) { }

	try {
		std::string _index(upper_string(item_doc.at(RESERVED_INDEX).get_str()));
		if (_index == str_index[0]) {
			specification.index = Index::ALL;
			properties[RESERVED_INDEX] = (unsigned)Index::ALL;
		} else if (_index == str_index[1]) {
			specification.index = Index::TERM;
			properties[RESERVED_INDEX] = (unsigned)Index::TERM;
		} else if (_index == str_index[2]) {
			specification.index = Index::VALUE;
			properties[RESERVED_INDEX] = (unsigned)Index::VALUE;
		} else {
			throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string", RESERVED_INDEX);
	} catch (const std::out_of_range&) { }

	try {
		auto doc_analyzer = item_doc.at(RESERVED_ANALYZER);
		specification.analyzer.clear();
		if (doc_analyzer.obj->type == msgpack::type::ARRAY) {
			for (const auto _analyzer : doc_analyzer) {
				std::string _str_analyzer(upper_string(_analyzer.get_str()));
				if (_str_analyzer == str_analyzer[0]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
				} else if (_str_analyzer == str_analyzer[1]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
				} else if (_str_analyzer == str_analyzer[2]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
				} else if (_str_analyzer == str_analyzer[3]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
				} else {
					throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
				}
			}
		} else {
			std::string _str_analyzer(upper_string(doc_analyzer.get_str()));
			if (_str_analyzer == str_analyzer[0]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
			} else if (_str_analyzer == str_analyzer[1]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
			} else if (_str_analyzer == str_analyzer[2]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
			} else if (_str_analyzer == str_analyzer[3]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
			} else {
				throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
			}
		}
		properties[RESERVED_ANALYZER] = specification.analyzer;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string or array of strings", RESERVED_ANALYZER);
	} catch (const std::out_of_range&) { }

	try {
		specification.dynamic = item_doc.at(RESERVED_DYNAMIC).get_bool();
		properties[RESERVED_DYNAMIC] = specification.dynamic;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_DYNAMIC);
	} catch (const std::out_of_range&) { }

	if (!is_root) {
		insert_noninheritable_data(properties, item_doc);
	}
}


void
Schema::update(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc, bool is_root)
{
	// Restarting reserved words than are not inherited.
	specification.accuracy.clear();
	specification.acc_prefix.clear();
	specification.sep_types = default_spc.sep_types;
	specification.bool_term = default_spc.bool_term;
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;

	if (item_doc.obj->type == msgpack::type::MAP) {
		// RESERVED_POSITION is heritable and can change between documents.
		try {
			auto doc_position = item_doc.at(RESERVED_POSITION);
			specification.position.clear();
			if (doc_position.obj->type == msgpack::type::ARRAY) {
				for (const auto _position : doc_position) {
					specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
				}
			} else {
				specification.position.push_back(static_cast<unsigned>(doc_position.get_u64()));
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be positive integer or array of positive NEGAgers", RESERVED_POSITION);
		} catch (const std::out_of_range&) {
			try {
				auto position = properties.at(RESERVED_POSITION);
				specification.position.clear();
				for (const auto _position : position) {
					specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
				}
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_WEIGHT is heritable and can change between documents.
		try {
			auto doc_weight = item_doc.at(RESERVED_WEIGHT);
			specification.weight.clear();
			if (doc_weight.obj->type == msgpack::type::ARRAY) {
				for (const auto _weight : doc_weight) {
					specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
				}
			} else {
				specification.weight.push_back(static_cast<unsigned>(doc_weight.get_u64()));
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be positive integer or array of positive integers", RESERVED_WEIGHT);
		} catch (const std::out_of_range&) {
			try {
				auto weight = properties.at(RESERVED_WEIGHT);
				specification.weight.clear();
				for (const auto _weight : weight) {
					specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
				}
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_LANGUAGE is heritable and can change between documents.
		try {
			auto doc_language = item_doc.at(RESERVED_LANGUAGE);
			specification.language.clear();
			if (doc_language.obj->type == msgpack::type::ARRAY) {
				for (const auto _language : doc_language) {
					std::string _str_language(_language.get_str());
					if (is_language(_str_language)) {
						specification.language.push_back(std::move(_str_language));
					} else {
						throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
					}
				}
			} else {
				std::string _str_language(doc_language.get_str());
				if (is_language(_str_language)) {
					specification.language.push_back(std::move(_str_language));
				} else {
					throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
				}
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be string or array of strings", RESERVED_LANGUAGE);
		} catch (const std::out_of_range&) {
			try {
				auto language = properties.at(RESERVED_LANGUAGE);
				specification.language.clear();
				for (const auto _language : language) {
					specification.language.emplace_back(_language.get_str());
				}
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_SPELLING is heritable and can change between documents.
		try {
			auto doc_spelling = item_doc.at(RESERVED_SPELLING);
			specification.spelling.clear();
			if (doc_spelling.obj->type == msgpack::type::ARRAY) {
				for (const auto _spelling : doc_spelling) {
					specification.spelling.push_back(_spelling.get_bool());
				}
			} else {
				specification.spelling.push_back(doc_spelling.get_bool());
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be boolean or array of booleans", RESERVED_SPELLING);
		} catch (const std::out_of_range&) {
			try {
				auto spelling = properties.at(RESERVED_SPELLING);
				specification.spelling.clear();
				for (const auto _spelling : spelling) {
					specification.spelling.push_back(_spelling.get_bool());
				}
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_POSITIONS is heritable and can change between documents.
		try {
			auto doc_positions = item_doc.at(RESERVED_POSITIONS);
			specification.positions.clear();
			if (doc_positions.obj->type == msgpack::type::ARRAY) {
				for (const auto _positions : doc_positions) {
					specification.positions.push_back(_positions.obj->type);
				}
			} else {
				specification.positions.push_back(doc_positions.get_bool());
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be boolean or array of booleans", RESERVED_POSITIONS);
		} catch (const std::out_of_range&) {
			try {
				auto positions = properties.at(RESERVED_POSITIONS);
				specification.positions.clear();
				for (const auto _positions : positions) {
					specification.positions.push_back(_positions.get_bool());
				}
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_ANALYZER is heritable and can change between documents.
		try {
			auto doc_analyzer = item_doc.at(RESERVED_ANALYZER);
			specification.analyzer.clear();
			if (doc_analyzer.obj->type == msgpack::type::ARRAY) {
				for (const auto _analyzer : doc_analyzer) {
					std::string _str_analyzer(upper_string(_analyzer.get_str()));
					if (_str_analyzer == str_analyzer[0]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
					} else if (_str_analyzer == str_analyzer[1]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
					} else if (_str_analyzer == str_analyzer[2]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
					} else if (_str_analyzer == str_analyzer[3]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
					} else {
						throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
					}
				}
			} else {
				std::string _str_analyzer(upper_string(doc_analyzer.get_str()));
				if (_str_analyzer == str_analyzer[0]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
				} else if (_str_analyzer == str_analyzer[1]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
				} else if (_str_analyzer == str_analyzer[2]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
				} else if (_str_analyzer == str_analyzer[3]) {
					specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
				} else {
					throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
				}
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be string or array of strings", RESERVED_ANALYZER);
		} catch (const std::out_of_range&) {
			try {
				auto analyzer = properties.at(RESERVED_ANALYZER);
				specification.analyzer.clear();
				for (const auto _analyzer : analyzer) {
					specification.analyzer.push_back(static_cast<unsigned>(_analyzer.get_u64()));
				}
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_STORE is heritable and can change.
		try {
			specification.store = item_doc.at(RESERVED_STORE).get_bool();
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be boolean", RESERVED_STORE);
		} catch (const std::out_of_range&) {
			try {
				specification.store = properties.at(RESERVED_STORE).get_bool();
			} catch (const std::out_of_range&) { }
		}

		// RESERVED_INDEX is heritable and can change.
		try {
			std::string _str_index(upper_string(item_doc.at(RESERVED_INDEX).get_str()));
			if (_str_index == str_index[0]) {
				specification.index = Index::ALL;
			} else if (_str_index == str_index[1]) {
				specification.index = Index::TERM;
			} else if (_str_index == str_index[2]) {
				specification.index = Index::VALUE;
			} else {
				throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
			}
		} catch (const msgpack::type_error&) {
			throw MSG_Error("Data inconsistency, %s must be string", RESERVED_INDEX);
		} catch (const std::out_of_range&) {
			try {
				specification.index = (Index)properties.at(RESERVED_INDEX).get_u64();
			} catch (const std::out_of_range&) { }
		}
	} else {
		try {
			auto position = properties.at(RESERVED_POSITION);
			specification.position.clear();
			for (const auto _position : position) {
				specification.position.push_back(static_cast<unsigned>(_position.get_u64()));
			}
		} catch (const std::out_of_range&) { }

		try {
			auto weight = properties.at(RESERVED_WEIGHT);
			specification.weight.clear();
			for (const auto _weight : weight) {
				specification.weight.push_back(static_cast<unsigned>(_weight.get_u64()));
			}
		} catch (const std::out_of_range&) { }

		try {
			auto language = properties.at(RESERVED_LANGUAGE);
			specification.language.clear();
			for (const auto _language : language) {
				specification.language.emplace_back(_language.get_str());
			}
		} catch (const std::out_of_range&) { }

		try {
			auto spelling = properties.at(RESERVED_SPELLING);
			specification.spelling.clear();
			for (const auto _spelling : spelling) {
				specification.spelling.push_back(_spelling.get_bool());
			}
		} catch (const std::out_of_range&) { }

		try {
			auto positions = properties.at(RESERVED_POSITIONS);
			specification.positions.clear();
			for (const auto _positions : positions) {
				specification.positions.push_back(_positions.get_bool());
			}
		} catch (const std::out_of_range&) { }

		try {
			auto analyzer = properties.at(RESERVED_ANALYZER);
			specification.analyzer.clear();
			for (const auto _analyzer : analyzer) {
				specification.analyzer.push_back(static_cast<unsigned>(_analyzer.get_u64()));
			}
		} catch (const std::out_of_range&) { }

		try {
			specification.store = properties.at(RESERVED_STORE).get_bool();
		} catch (const std::out_of_range&) { }

		try {
			specification.index = (Index)properties.at(RESERVED_INDEX).get_u64();
		} catch (const std::out_of_range&) { }
	}

	// RESERVED_?_DETECTION is heritable but can't change.
	try {
		specification.date_detection = properties.at(RESERVED_D_DETECTION).get_bool();
	} catch (const std::out_of_range&) { }

	try {
		specification.numeric_detection = properties.at(RESERVED_N_DETECTION).get_bool();
	} catch (const std::out_of_range&) { }

	try {
		specification.geo_detection = properties.at(RESERVED_G_DETECTION).get_bool();
	} catch (const std::out_of_range&) { }

	try {
		specification.bool_detection = properties.at(RESERVED_B_DETECTION).get_bool();
	} catch (const std::out_of_range&) { }

	try {
		specification.string_detection = properties.at(RESERVED_S_DETECTION).get_bool();
	} catch (const std::out_of_range&) { }

	// RESERVED_DYNAMIC is heritable but can't change.
	try {
		specification.dynamic = properties.at(RESERVED_DYNAMIC).get_bool();
	} catch (const std::out_of_range&) { }

	// RESERVED_BOOL_TERM isn't heritable and can't change. It always will be in all fields.
	try {
		specification.bool_term = properties.at(RESERVED_BOOL_TERM).get_bool();
	} catch (const std::out_of_range&) { }

	// RESERVED_TYPE isn't heritable and can't change once fixed the type field value.
	if (!is_root) {
		try {
			auto type = properties.at(RESERVED_TYPE);
			specification.sep_types[0] = static_cast<unsigned>(type.at(0).get_u64());
			specification.sep_types[1] = static_cast<unsigned>(type.at(1).get_u64());
			specification.sep_types[2] = static_cast<unsigned>(type.at(2).get_u64());
			// If the type field value hasn't fixed yet and its specified in the document, properties is updated.
			if (specification.sep_types[2] == NO_TYPE) {
				if (item_doc.find(RESERVED_TYPE)) {
					// In this point means that terms or values haven't been inserted with this field,
					// therefore, lets us to change prefix, slot and bool_term in properties.
					insert_noninheritable_data(properties, item_doc);
					update_required_data(properties, item_key);
				}
			} else {
				// If type has been defined, the next reserved words have been defined too.
				specification.prefix = properties.at(RESERVED_PREFIX).get_str();
				specification.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).get_u64());
				specification.bool_term = properties.at(RESERVED_BOOL_TERM).get_bool();
				specification.accuracy.clear();
				specification.acc_prefix.clear();
				if (specification.sep_types[2] != STRING_TYPE && specification.sep_types[2] != BOOLEAN_TYPE) {
					for (const auto acc : properties.at(RESERVED_ACCURACY)) {
						specification.accuracy.push_back(acc.get_f64());
					}
					for (const auto acc_p : properties.at(RESERVED_ACC_PREFIX)) {
						specification.acc_prefix.emplace_back(acc_p.get_str());
					}
				}
			}
		} catch (const std::out_of_range&) {
			if (item_doc.find(RESERVED_TYPE)) {
				// If RESERVED_TYPE has not been fixed yet and its specified in the document, properties is updated.
				insert_noninheritable_data(properties, item_doc);
				update_required_data(properties, item_key);
			}
		}
	}
}


void
Schema::insert_noninheritable_data(MsgPack& properties, const MsgPack& item_doc)
{
	try {
		if (set_types(lower_string(item_doc.at(RESERVED_TYPE).get_str()), specification.sep_types)) {
			properties[RESERVED_TYPE] = specification.sep_types;
			to_store = true;
		} else {
			throw MSG_Error("%s can be [object/][array/]< %s | %s | %s | %s | %s >", RESERVED_TYPE, NUMERIC_STR, STRING_STR, DATE_STR, BOOLEAN_STR, GEO_STR);
		}
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string", RESERVED_TYPE);
	} catch (const std::out_of_range&) { }

	try {
		size_t size_acc = 0;
		auto doc_accuracy = item_doc.at(RESERVED_ACCURACY);
		if (specification.sep_types[2] == NO_TYPE) {
			throw MSG_Error("You must specify %s, for verify if the %s is correct", RESERVED_TYPE, RESERVED_ACCURACY);
		}
		if (doc_accuracy.obj->type == msgpack::type::ARRAY) {
			switch (specification.sep_types[2]) {
				case GEO_TYPE: {
					try {
						specification.accuracy.push_back(doc_accuracy.at(0).get_bool());
					} catch (const msgpack::type_error&) {
						throw MSG_Error("Data inconsistency, partials value in %s: %s must be boolean", RESERVED_ACCURACY, GEO_STR);
					}
					try {
						auto error = doc_accuracy.at(1);
						try {
							specification.accuracy.push_back(error.get_u64() > HTM_MAX_ERROR ? HTM_MAX_ERROR : error.get_u64() < HTM_MIN_ERROR ? HTM_MIN_ERROR : error.get_u64());
						} catch (const msgpack::type_error&) {
							throw MSG_Error("Data inconsistency, error value in %s: %s must be positive integer", RESERVED_ACCURACY, GEO_STR);
						}
						try {
							const auto it_e = doc_accuracy.end();
							for (auto it = doc_accuracy.begin() + 2; it != it_e; ++it) {
								uint64_t val_acc = (*it).get_u64();
								if (val_acc <= HTM_MAX_LEVEL) {
									specification.accuracy.push_back(val_acc);
								} else {
									throw MSG_Error("Data inconsistency, level value in %s: %s must be a positive number between 0 and %d", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
								}
							}
						} catch (const msgpack::type_error&) {
							throw MSG_Error("Data inconsistency, level value in %s: %s must be a positive number between 0 and %d", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
						}
						std::sort(specification.accuracy.begin() + 2, specification.accuracy.end());
						std::unique(specification.accuracy.begin() + 2, specification.accuracy.end());
						size_acc = specification.accuracy.size() - 2;
						break;
					} catch (const std::out_of_range&) {
						specification.accuracy.push_back(def_accuracy_geo[1]);
					}
				}
				case DATE_TYPE: {
					try {
						for (const auto _accuracy : doc_accuracy) {
							std::string str_accuracy(upper_string(_accuracy.get_str()));
							if (str_accuracy == str_time[5]) {
								specification.accuracy.push_back(toUType(unitTime::YEAR));
							} else if (str_accuracy == str_time[4]) {
								specification.accuracy.push_back(toUType(unitTime::MONTH));
							} else if (str_accuracy == str_time[3]) {
								specification.accuracy.push_back(toUType(unitTime::DAY));
							} else if (str_accuracy == str_time[2]) {
								specification.accuracy.push_back(toUType(unitTime::HOUR));
							} else if (str_accuracy == str_time[1]) {
								specification.accuracy.push_back(toUType(unitTime::MINUTE));
							} else if (str_accuracy == str_time[0]) {
								specification.accuracy.push_back(toUType(unitTime::SECOND));
							} else {
								throw MSG_Error("Data inconsistency, %s: %s must be subset of {%s, %s, %s, %s, %s, %s}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
							}
						}
						std::sort(specification.accuracy.begin(), specification.accuracy.end());
						std::unique(specification.accuracy.begin(), specification.accuracy.end());
						size_acc = specification.accuracy.size();
						break;
					} catch (const msgpack::type_error&) {
						throw MSG_Error("Data inconsistency, %s in %s must be subset of {%s, %s, %s, %s, %s, %s]}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
					}
				}
				case NUMERIC_TYPE: {
					try {
						for (const auto _accuracy : doc_accuracy) {
							specification.accuracy.push_back(_accuracy.get_u64());
						}
						std::sort(specification.accuracy.begin(), specification.accuracy.end());
						std::unique(specification.accuracy.begin(), specification.accuracy.end());
						size_acc = specification.accuracy.size();
						break;
					} catch (const msgpack::type_error&) {
						throw MSG_Error("Data inconsistency, %s: %s must be an array of positive numbers", RESERVED_ACCURACY, NUMERIC_STR);
					}
				}
				default:
					throw MSG_Error("%s type does not have accuracy", Serialise::type(default_spc.sep_types[2]).c_str());
			}

			properties[RESERVED_ACCURACY] = specification.accuracy;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s must be array", RESERVED_ACCURACY);
		}

		// Accuracy prefix is taken into account only if accuracy is defined.
		try {
			auto doc_acc_prefix = item_doc.at(RESERVED_ACC_PREFIX);
			if (doc_acc_prefix.obj->type == msgpack::type::ARRAY) {
				if (doc_acc_prefix.obj->via.array.size != size_acc) {
					throw MSG_Error("Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
				}
				try {
					for (const auto _acc_prefix : doc_acc_prefix) {
						specification.acc_prefix.push_back(_acc_prefix.get_str());
					}
					to_store = true;
					properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				} catch (const msgpack::type_error&) {
					throw MSG_Error("Data inconsistency, %s must be an array of strings", RESERVED_ACC_PREFIX);
				}
			} else {
				throw MSG_Error("Data inconsistency, %s must be an array of strings", RESERVED_ACC_PREFIX);
			}
		} catch (const std::out_of_range&) { }
	} catch (const std::out_of_range&) { }

	try {
		specification.prefix = item_doc.at(RESERVED_PREFIX).get_str();
		properties[RESERVED_PREFIX] = specification.prefix;
		to_store = true;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be string", RESERVED_PREFIX);
	} catch (const std::out_of_range&) { }

	try {
		unsigned _slot = static_cast<unsigned>(item_doc.at(RESERVED_SLOT).get_u64());
		if (_slot < DB_SLOT_RESERVED) {
			_slot += DB_SLOT_RESERVED;
		} else if (_slot == Xapian::BAD_VALUENO) {
			_slot = 0xfffffffe;
		}
		properties[RESERVED_SLOT] = _slot;
		specification.slot = _slot;
		to_store = true;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be a positive integer", RESERVED_SLOT);
	} catch (const std::out_of_range&) { }

	try {
		specification.bool_term = item_doc.at(RESERVED_BOOL_TERM).get_bool();
		properties[RESERVED_BOOL_TERM] = specification.bool_term;
		to_store = true;
	} catch (const msgpack::type_error&) {
		throw MSG_Error("Data inconsistency, %s must be a boolean", RESERVED_BOOL_TERM);
	} catch (const std::out_of_range&) { }
}


void
Schema::update_required_data(MsgPack& properties, const std::string& item_key)
{
	// Adds type to properties, if this has not been added.
	auto type = properties[RESERVED_TYPE];
	if (!type) {
		type = specification.sep_types;
		to_store = true;
	}

	// Inserts prefix
	if (item_key.empty()) {
		specification.prefix = default_spc.prefix;
	} else if (specification.prefix == default_spc.prefix) {
		specification.prefix = get_prefix(item_key, DOCUMENT_CUSTOM_TERM_PREFIX, specification.sep_types[2]);
		properties[RESERVED_PREFIX] = specification.prefix;
		to_store = true;
	}

	// Inserts slot.
	if (specification.slot == default_spc.slot) {
		specification.slot = get_slot(item_key);
		properties[RESERVED_SLOT] = specification.slot;
		to_store = true;
	}

	auto bool_term = properties[RESERVED_BOOL_TERM];
	if (!item_key.empty() && !bool_term) {
		// By default, if item_key has upper characters then it is consider bool term.
		if (strhasupper(item_key)) {
			bool_term = true;
			specification.bool_term = true;
			to_store = true;
		} else {
			bool_term = false;
			specification.bool_term = false;
			to_store = true;
		}
	}

	// Set defualt accuracies.
	switch (specification.sep_types[2]) {
		case GEO_TYPE: {
			if (specification.accuracy.empty()) {
				specification.accuracy.push_back(def_accuracy_geo[0]);
				specification.accuracy.push_back(def_accuracy_geo[1]);
				const auto it_e = def_accuracy_geo.end();
				for (auto it = def_accuracy_geo.begin() + 2; it != it_e; ++it) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE));
					specification.accuracy.push_back(*it);
				}
				properties[RESERVED_ACCURACY] = specification.accuracy;
				properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				const auto it_e = specification.accuracy.end();
				for (auto it = specification.accuracy.begin() + 2; it != it_e; ++it) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE));
				}
				properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				to_store = true;
			}
			break;
		}
		case NUMERIC_TYPE: {
			if (specification.accuracy.empty()) {
				for (const auto& acc : def_accuracy_num) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE));
					specification.accuracy.push_back(acc);
				}
				properties[RESERVED_ACCURACY] = specification.accuracy;
				properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				for (const auto& acc : specification.accuracy) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE));
				}
				properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				to_store = true;
			}
			break;
		}
		case DATE_TYPE: {
			// Use default accuracy.
			if (specification.accuracy.empty()) {
				for (const auto& acc : def_acc_date) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE));
					specification.accuracy.push_back(acc);
				}
				properties[RESERVED_ACCURACY] = specification.accuracy;
				properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				for (const auto& acc : specification.accuracy) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE));
				}
				properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				to_store = true;
			}
			break;
		}
	}
}


void
Schema::readable(MsgPack&& item_schema)
{
	// Change this item of schema in readable form.
	if (item_schema.obj->type == msgpack::type::MAP) {
		try {
			auto type = item_schema.at(RESERVED_TYPE);
			std::vector<unsigned> sep_types({
				static_cast<unsigned>(type.at(0).get_u64()),
				static_cast<unsigned>(type.at(1).get_u64()),
				static_cast<unsigned>(type.at(2).get_u64())
			});
			type = str_type(sep_types);
			try {
				if (sep_types[2] == DATE_TYPE) {
					for (auto _accuracy : item_schema.at(RESERVED_ACCURACY)) {
						_accuracy = str_time[_accuracy.get_f64()];
					}
				} else if (sep_types[2] == GEO_TYPE) {
					auto _partials = item_schema.at(RESERVED_ACCURACY).at(0);
					_partials = _partials.get_f64() ? true : false;
				}
			} catch (const std::out_of_range&) { }
		} catch (const std::out_of_range&) { }

		try {
			for (auto _analyzer : item_schema.at(RESERVED_ANALYZER)) {
				_analyzer = str_analyzer[_analyzer.get_u64()];
			}
		} catch (const std::out_of_range&) { }

		try {
			auto index = item_schema.at(RESERVED_INDEX);
			index = str_index[index.get_u64()];
		} catch (const std::out_of_range&) { }

		// Process its offsprings.
		for (const auto item_key : item_schema) {
			std::string str_key(item_key.get_str());
			if (!is_reserved(str_key)) {
				readable(item_schema.at(str_key));
			}
		}
	}
}


char
Schema::get_type(const MsgPack& item_doc)
{
	if (item_doc.obj->type == msgpack::type::MAP) {
		throw MSG_Error("%s can not be object", RESERVED_VALUE);
	}

	MsgPack field = item_doc.obj->type == msgpack::type::ARRAY ? item_doc.at(0) : item_doc;
	int type = field.obj->type;
	if (type == msgpack::type::ARRAY) {
		const auto it_e = item_doc.end();
		for (auto it = item_doc.begin() + 1; it != it_e; ++it) {
			if ((*it).obj->type != type) {
				throw MSG_Error("Different types of data in array");
			}
		}
		specification.sep_types[1] = ARRAY_TYPE;
	}

	switch (type) {
		case msgpack::type::POSITIVE_INTEGER:
		case msgpack::type::NEGATIVE_INTEGER:
		case msgpack::type::FLOAT:
			if (specification.numeric_detection) {
				return NUMERIC_TYPE;
			}
			break;
		case msgpack::type::BOOLEAN:
			if (specification.bool_detection) {
				return BOOLEAN_TYPE;
			}
			break;
		case msgpack::type::STR:
			std::string str_value(field.get_str());
			if (specification.date_detection && Datetime::isDate(str_value)) {
				return DATE_TYPE;
			} else if (specification.geo_detection && EWKT_Parser::isEWKT(str_value)) {
				return GEO_TYPE;
			} else if (specification.string_detection) {
				return STRING_TYPE;
			} else if (specification.bool_detection) {
				try {
					Serialise::boolean(str_value);
					return BOOLEAN_TYPE;
				} catch (const std::exception&) { }
			}
			break;
	}

	throw MSG_Error("%s: %s is ambiguous", RESERVED_VALUE, item_doc.to_json_string().c_str());
}
