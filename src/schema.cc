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
		if (schema.obj.type != msgpack::type::MAP) {
			try {
				auto version = schema.at(RESERVED_VERSION);
				if (version.obj.via.f64 != DB_VERSION_SCHEMA) {
					throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
				}
			} catch (const msgpack::type_error&) {
				throw MSG_Error("Schema is corrupt, you need provide a new one");
			}
		} else {
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
		auto type = properties_id[RESERVED_TYPE];
		type.add_item_to_array(NO_TYPE);
		type.add_item_to_array(NO_TYPE);
		type.add_item_to_array(NO_TYPE);

		properties_id[RESERVED_INDEX] = (unsigned)Index::ALL;
		properties_id[RESERVED_SLOT] = DB_SLOT_ID;
		properties_id[RESERVED_PREFIX] = DOCUMENT_ID_TERM_PREFIX;
		properties_id[RESERVED_BOOL_TERM] = true;
		to_store = true;
		insert(properties, RESERVED_SCHEMA, item_doc, true);
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
		insert(subproperties, item_key, item_doc);
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
	specification.accuracy.clear();
	specification.acc_prefix.clear();
	specification.sep_types[0] = default_spc.sep_types[0];
	specification.sep_types[1] = default_spc.sep_types[1];
	specification.sep_types[2] = STRING_TYPE;
	specification.bool_term = default_spc.bool_term;
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;

	// RESERVED_POSITION is heritable and can change between documents.
	try {
		auto doc_position = item_doc.at(RESERVED_POSITION);
		specification.position.clear();
		if (doc_position.obj.type == msgpack::type::POSITIVE_INTEGER) {
			specification.position.push_back(static_cast<unsigned>(doc_position.obj.via.u64));
		} else if (doc_position.obj.type == msgpack::type::ARRAY) {
			for (const auto _position : doc_position) {
				if (_position.obj.type == msgpack::type::POSITIVE_INTEGER) {
					specification.position.push_back(static_cast<unsigned>(_position.obj.via.u64));
				} else {
					throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_POSITION);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_POSITION);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_WEIGHT is heritable and can change between documents.
	try {
		auto doc_weight = item_doc.at(RESERVED_WEIGHT);
		specification.weight.clear();
		if (doc_weight.obj.type == msgpack::type::POSITIVE_INTEGER) {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.obj.via.u64));
		} else if (doc_weight.obj.type == msgpack::type::ARRAY) {
			for (const auto _weight : doc_weight) {
				if (_weight.obj.type == msgpack::type::POSITIVE_INTEGER) {
					specification.weight.push_back(static_cast<unsigned>(_weight.obj.via.u64));
				} else {
					throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_WEIGHT);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_WEIGHT);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_LANGUAGE is heritable and can change between documents.
	try {
		auto doc_language = item_doc.at(RESERVED_LANGUAGE);
		specification.language.clear();
		if (doc_language.obj.type == msgpack::type::STR) {
			std::string _str_language(doc_language.obj.via.str.ptr, doc_language.obj.via.str.size);
			if (is_language(_str_language)) {
				specification.language.push_back(std::move(_str_language));
			} else {
				throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
			}
		} else if (doc_language.obj.type == msgpack::type::ARRAY) {
			for (const auto _language : doc_language) {
				if (_language.obj.type == msgpack::type::STR) {
					std::string _str_language(_language.obj.via.str.ptr, _language.obj.via.str.size);
					if (is_language(_str_language)) {
						specification.language.push_back(std::move(_str_language));
					} else {
						throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_SPELLING is heritable and can change between documents.
	try {
		auto doc_spelling = item_doc.at(RESERVED_SPELLING);
		specification.spelling.clear();
		if (doc_spelling.obj.type == msgpack::type::BOOLEAN) {
			specification.spelling.push_back(doc_spelling.obj.via.boolean);
		} else if (doc_spelling.obj.type == msgpack::type::ARRAY) {
			for (const auto _spelling : doc_spelling) {
				if (_spelling.obj.type == msgpack::type::BOOLEAN) {
					specification.spelling.push_back(_spelling.obj.via.boolean);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_POSITIONS is heritable and can change between documents.
	try {
		auto doc_positions = item_doc.at(RESERVED_POSITIONS);
		specification.positions.clear();
		if (doc_positions.obj.type == msgpack::type::BOOLEAN) {
			specification.positions.push_back(doc_positions.obj.via.boolean);
		} else if (doc_positions.obj.type == msgpack::type::ARRAY) {
			for (const auto _positions : doc_positions) {
				if (_positions.obj.type == msgpack::type::BOOLEAN) {
					specification.positions.push_back(_positions.obj.type);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_ANALYZER is heritable and can change between documents.
	try {
		auto doc_analyzer = item_doc.at(RESERVED_ANALYZER);
		specification.analyzer.clear();
		if (doc_analyzer.obj.type == msgpack::type::STR) {
			std::string _analyzer(upper_string(doc_analyzer.obj.via.str.ptr, doc_analyzer.obj.via.str.size));
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
		} else if (doc_analyzer.obj.type == msgpack::type::ARRAY) {
			for (const auto analyzer : doc_analyzer) {
				if (analyzer.obj.type == msgpack::type::STR) {
					std::string _analyzer(upper_string(analyzer.obj.via.str.ptr, analyzer.obj.via.str.size));
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
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_STORE is heritable and can change.
	try {
		auto doc_store = item_doc.at(RESERVED_STORE);
		if (doc_store.obj.type == msgpack::type::BOOLEAN) {
			specification.store = doc_store.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
		}
	} catch (const msgpack::type_error&) { }

	// RESERVED_INDEX is heritable and can change.
	try {
		auto doc_index = item_doc.at(RESERVED_INDEX);
		if (doc_index.obj.type == msgpack::type::STR) {
			std::string _index(upper_string(doc_index.obj.via.str.ptr, doc_index.obj.via.str.size));
			if (_index == str_index[0]) {
				specification.index = Index::ALL;
			} else if (_index == str_index[1]) {
				specification.index = Index::TERM;
			} else if (_index == str_index[2]) {
				specification.index = Index::VALUE;
			} else {
				throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
		}
	} catch (const msgpack::type_error&) { }
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
		if (_type.obj.via.u64 == NO_TYPE) {
			_type = ARRAY_TYPE;
			to_store = true;
		}
	} catch (const msgpack::type_error&) { }
}

void
Schema::set_type_to_object(MsgPack& properties)
{
	try {
		auto _type = properties.at(RESERVED_TYPE).at(0);
		if (_type.obj.via.u64 == NO_TYPE) {
			_type = OBJECT_TYPE;
			to_store = true;
		}
	} catch (const msgpack::type_error&) { }
}


std::string
Schema::to_string(bool prettify)
{
	MsgPack schema_readable = schema.duplicate();
	auto properties = schema_readable.at(RESERVED_SCHEMA);
	for (const auto item_key : properties) {
		std::string str_key(item_key.obj.via.str.ptr, item_key.obj.via.str.size);
		if (!is_reserved(str_key)) {
			readable(properties.at(str_key));
		}
	}

	return schema_readable.to_json_string(prettify);
}


void
Schema::insert(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc, bool is_root)
{
	try {
		auto doc_d_detection = item_doc.at(RESERVED_D_DETECTION);
		if (doc_d_detection.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_D_DETECTION] = doc_d_detection.obj.via.boolean;
			specification.date_detection = doc_d_detection.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_D_DETECTION);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_n_detection = item_doc.at(RESERVED_N_DETECTION);
		if (doc_n_detection.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_N_DETECTION] = doc_n_detection.obj.via.boolean;
			specification.numeric_detection = doc_n_detection.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_N_DETECTION);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_g_detection = item_doc.at(RESERVED_G_DETECTION);
		if (doc_g_detection.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_G_DETECTION] = doc_g_detection.obj.via.boolean;
			specification.geo_detection = doc_g_detection.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_G_DETECTION);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_b_detection = item_doc.at(RESERVED_B_DETECTION);
		if (doc_b_detection.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_B_DETECTION] = doc_b_detection.obj.via.boolean;
			specification.bool_detection = doc_b_detection.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_B_DETECTION);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_s_detection = item_doc.at(RESERVED_S_DETECTION);
		if (doc_s_detection.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_S_DETECTION] = doc_s_detection.obj.via.boolean;
			specification.string_detection = doc_s_detection.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_S_DETECTION);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_position = item_doc.at(RESERVED_POSITION);
		specification.position.clear();
		if (doc_position.obj.type == msgpack::type::POSITIVE_INTEGER) {
			specification.position.push_back(static_cast<unsigned>(doc_position.obj.via.u64));
			properties[RESERVED_POSITION].add_item_to_array(doc_position.obj.via.u64);
		} else if (doc_position.obj.type == msgpack::type::ARRAY) {
			MsgPack position = properties[RESERVED_POSITION];
			for (const auto _position : doc_position) {
				if (_position.obj.type == msgpack::type::POSITIVE_INTEGER) {
					specification.position.push_back(static_cast<unsigned>(_position.obj.via.u64));
					position.add_item_to_array(_position.obj.via.u64);
				} else {
					throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_POSITION);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_POSITION);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_weight = item_doc.at(RESERVED_WEIGHT);
		specification.weight.clear();
		if (doc_weight.obj.type == msgpack::type::POSITIVE_INTEGER) {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.obj.via.u64));
			properties[RESERVED_WEIGHT].add_item_to_array(doc_weight.obj.via.u64);
		} else if (doc_weight.obj.type == msgpack::type::ARRAY) {
			auto weight = properties[RESERVED_WEIGHT];
			for (const auto _weight : doc_weight) {
				if (_weight.obj.type == msgpack::type::POSITIVE_INTEGER) {
					specification.weight.push_back(static_cast<unsigned>(_weight.obj.via.u64));
					weight.add_item_to_array(_weight.obj.via.u64);
				} else {
					throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_WEIGHT);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_WEIGHT);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_language = item_doc.at(RESERVED_LANGUAGE);
		specification.language.clear();
		if (doc_language.obj.type == msgpack::type::STR) {
			std::string _str_language(doc_language.obj.via.str.ptr, doc_language.obj.via.str.size);
			if (is_language(_str_language)) {
				properties[RESERVED_LANGUAGE].add_item_to_array(_str_language);
				specification.language.push_back(std::move(_str_language));
			} else {
				throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
			}
		} else if (doc_language.obj.type == msgpack::type::ARRAY) {
			auto language = properties[RESERVED_LANGUAGE];
			for (const auto _language : doc_language) {
				if (_language.obj.type == msgpack::type::STR) {
					std::string _str_language(_language.obj.via.str.ptr, _language.obj.via.str.size);
					if (is_language(_str_language)) {
						language.add_item_to_array(_str_language);
						specification.language.push_back(std::move(_str_language));
					} else {
						throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		}
	} catch (const msgpack::type_error&) {
		size_t pfound = item_key.rfind(DB_OFFSPRING_UNION);
		if (pfound != std::string::npos) {
			std::string _str_language(item_key.substr(pfound + strlen(DB_OFFSPRING_UNION)));
			if (is_language(_str_language)) {
				specification.language.clear();
				properties[RESERVED_LANGUAGE].add_item_to_array(_str_language);
				specification.language.push_back(std::move(_str_language));
			}
		}
	}

	try {
		auto doc_spelling = item_doc.at(RESERVED_SPELLING);
		specification.spelling.clear();
		if (doc_spelling.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_SPELLING].add_item_to_array(doc_spelling.obj.via.boolean);
			specification.spelling.push_back(doc_spelling.obj.via.boolean);
		} else if (doc_spelling.obj.type == msgpack::type::ARRAY) {
			auto spelling = properties[RESERVED_SPELLING];
			for (const auto _spelling : doc_spelling) {
				if (_spelling.obj.type == msgpack::type::BOOLEAN) {
					spelling.add_item_to_array(_spelling.obj.via.boolean);
					specification.spelling.push_back(_spelling.obj.via.boolean);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_positions = item_doc.at(RESERVED_POSITIONS);
		specification.positions.clear();
		if (doc_positions.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_POSITIONS].add_item_to_array(doc_positions.obj.via.boolean);
			specification.positions.push_back(doc_positions.obj.via.boolean);
		} else if (doc_positions.obj.type == msgpack::type::ARRAY) {
			auto positions = properties[RESERVED_POSITIONS];
			for (const auto _positions : doc_positions) {
				if (_positions.obj.type == msgpack::type::BOOLEAN) {
					positions.add_item_to_array(_positions.obj.via.boolean);
					specification.positions.push_back(_positions.obj.via.boolean);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_store = item_doc.at(RESERVED_STORE);
		if (doc_store.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_STORE] = doc_store.obj.via.boolean;
			specification.store = doc_store.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_index = item_doc.at(RESERVED_INDEX);
		if (doc_index.obj.type == msgpack::type::STR) {
			std::string _index(upper_string(doc_index.obj.via.str.ptr, doc_index.obj.via.str.size));
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
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_analyzer = item_doc.at(RESERVED_ANALYZER);
		specification.analyzer.clear();
		if (doc_analyzer.obj.type == msgpack::type::STR) {
			std::string _str_analyzer(upper_string(doc_analyzer.obj.via.str.ptr, doc_analyzer.obj.via.str.size));
			if (_str_analyzer == str_analyzer[0]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
				properties[RESERVED_ANALYZER].add_item_to_array((unsigned)Xapian::TermGenerator::STEM_SOME);
			} else if (_str_analyzer == str_analyzer[1]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
				properties[RESERVED_ANALYZER].add_item_to_array((unsigned)Xapian::TermGenerator::STEM_NONE);
			} else if (_str_analyzer == str_analyzer[2]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
				properties[RESERVED_ANALYZER].add_item_to_array((unsigned)Xapian::TermGenerator::STEM_ALL);
			} else if (_str_analyzer == str_analyzer[3]) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
				properties[RESERVED_ANALYZER].add_item_to_array((unsigned)Xapian::TermGenerator::STEM_ALL_Z);
			} else {
				throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
			}
		} else if (doc_analyzer.obj.type == msgpack::type::ARRAY) {
			auto analyzer = properties[RESERVED_ANALYZER];
			for (const auto _analyzer : doc_analyzer) {
				if (_analyzer.obj.type == msgpack::type::STR) {
					std::string _str_analyzer(upper_string(_analyzer.obj.via.str.ptr, _analyzer.obj.via.str.size));
					if (_str_analyzer == str_analyzer[0]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
						analyzer.add_item_to_array((unsigned)Xapian::TermGenerator::STEM_SOME);
					} else if (_str_analyzer == str_analyzer[1]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
						analyzer.add_item_to_array((unsigned)Xapian::TermGenerator::STEM_NONE);
					} else if (_str_analyzer == str_analyzer[2]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
						analyzer.add_item_to_array((unsigned)Xapian::TermGenerator::STEM_ALL);
					} else if (_str_analyzer == str_analyzer[3]) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
						analyzer.add_item_to_array((unsigned)Xapian::TermGenerator::STEM_ALL_Z);
					} else {
						throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analyzer[0].c_str(), str_analyzer[1].c_str(), str_analyzer[2].c_str(), str_analyzer[3].c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_dynamic = item_doc.at(RESERVED_DYNAMIC);
		if (doc_dynamic.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_DYNAMIC] = doc_dynamic.obj.via.boolean;
			specification.dynamic = doc_dynamic.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_DYNAMIC);
		}
	} catch (const msgpack::type_error&) { }

	if (!is_root) {
		insert_noninheritable_data(properties, item_doc);
	}
}


void
Schema::update(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc, bool is_root)
{
	// RESERVED_POSITION is heritable and can change between documents.
	try {
		auto doc_position = item_doc.at(RESERVED_POSITION);
		specification.position.clear();
		if (doc_position.obj.type == msgpack::type::POSITIVE_INTEGER) {
			specification.position.push_back(static_cast<unsigned>(doc_position.obj.via.u64));
		} else if (doc_position.obj.type == msgpack::type::ARRAY) {
			for (const auto _position : doc_position) {
				if (_position.obj.type == msgpack::type::POSITIVE_INTEGER) {
					specification.position.push_back(static_cast<unsigned>(_position.obj.via.u64));
				} else {
					throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_POSITION);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_POSITION);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.position.clear();
			for (const auto _position : properties.at(RESERVED_POSITION)) {
				specification.position.push_back(static_cast<unsigned>(_position.obj.via.u64));
			}
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_WEIGHT is heritable and can change between documents.
	try {
		auto doc_weight = item_doc.at(RESERVED_WEIGHT);
		specification.weight.clear();
		if (doc_weight.obj.type == msgpack::type::POSITIVE_INTEGER) {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.obj.via.u64));
		} else if (doc_weight.obj.type == msgpack::type::ARRAY) {
			for (const auto _weight : doc_weight) {
				if (_weight.obj.type == msgpack::type::POSITIVE_INTEGER) {
					specification.weight.push_back(static_cast<unsigned>(_weight.obj.via.u64));
				} else {
					throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_WEIGHT);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer or array of positive integers", RESERVED_WEIGHT);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.weight.clear();
			for (const auto _weight : properties.at(RESERVED_WEIGHT)) {
				specification.weight.push_back(static_cast<unsigned>(_weight.obj.via.u64));
			}
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_LANGUAGE is heritable and can change between documents.
	try {
		auto doc_language = item_doc.at(RESERVED_LANGUAGE);
		specification.language.clear();
		if (doc_language.obj.type == msgpack::type::STR) {
			std::string _str_language(doc_language.obj.via.str.ptr, doc_language.obj.via.str.size);
			if (is_language(_str_language)) {
				specification.language.push_back(std::move(_str_language));
			} else {
				throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
			}
		} else if (doc_language.obj.type == msgpack::type::ARRAY) {
			for (const auto _language : doc_language) {
				if (_language.obj.type == msgpack::type::STR) {
					std::string _str_language(_language.obj.via.str.ptr, _language.obj.via.str.size);
					if (is_language(_str_language)) {
						specification.language.push_back(std::move(_str_language));
					} else {
						throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _str_language.c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.language.clear();
			for (const auto _language : properties.at(RESERVED_LANGUAGE)) {
				specification.language.emplace_back(_language.obj.via.str.ptr, _language.obj.via.str.size);
			}
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_SPELLING is heritable and can change between documents.
	try {
		auto doc_spelling = item_doc.at(RESERVED_SPELLING);
		specification.spelling.clear();
		if (doc_spelling.obj.type == msgpack::type::BOOLEAN) {
			specification.spelling.push_back(doc_spelling.obj.via.boolean);
		} else if (doc_spelling.obj.type == msgpack::type::ARRAY) {
			for (const auto _spelling : doc_spelling) {
				if (_spelling.obj.type == msgpack::type::BOOLEAN) {
					specification.spelling.push_back(_spelling.obj.via.boolean);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.spelling.clear();
			for (const auto _spelling : properties.at(RESERVED_SPELLING)) {
				specification.spelling.push_back(_spelling.obj.via.boolean);
			}
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_POSITIONS is heritable and can change between documents.
	try {
		auto doc_positions = item_doc.at(RESERVED_POSITIONS);
		specification.positions.clear();
		if (doc_positions.obj.type == msgpack::type::BOOLEAN) {
			specification.positions.push_back(doc_positions.obj.via.boolean);
		} else if (doc_positions.obj.type == msgpack::type::ARRAY) {
			for (const auto _positions : doc_positions) {
				if (_positions.obj.type == msgpack::type::BOOLEAN) {
					specification.positions.push_back(_positions.obj.type);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.positions.clear();
			for (const auto _positions : properties.at(RESERVED_POSITIONS)) {
				specification.positions.push_back(_positions.obj.via.boolean);
			}
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_ANALYZER is heritable and can change between documents.
	try {
		auto doc_analyzer = item_doc.at(RESERVED_ANALYZER);
		specification.analyzer.clear();
		if (doc_analyzer.obj.type == msgpack::type::STR) {
			std::string _str_analyzer(upper_string(doc_analyzer.obj.via.str.ptr, doc_analyzer.obj.via.str.size));
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
		} else if (doc_analyzer.obj.type == msgpack::type::ARRAY) {
			for (const auto _analyzer : doc_analyzer) {
				if (doc_analyzer.obj.type == msgpack::type::STR) {
					std::string _str_analyzer(upper_string(_analyzer.obj.via.str.ptr, _analyzer.obj.via.str.size));
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
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.analyzer.clear();
			for (const auto _analyzer : properties.at(RESERVED_ANALYZER)) {
				specification.analyzer.push_back(static_cast<unsigned>(_analyzer.obj.via.u64));
			}
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_STORE is heritable and can change.
	try {
		auto doc_store = item_doc.at(RESERVED_STORE);
		if (doc_store.obj.type == msgpack::type::BOOLEAN) {
			specification.store = doc_store.obj.via.boolean;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
		}
	} catch (const msgpack::type_error&) {
		try {
			auto store = properties.at(RESERVED_STORE);
			specification.store = store.obj.via.boolean;
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_INDEX is heritable and can change.
	try {
		auto doc_index = item_doc.at(RESERVED_INDEX);
		if (doc_index.obj.type == msgpack::type::STR) {
			std::string _str_index(upper_string(doc_index.obj.via.str.ptr, doc_index.obj.via.str.size));
			if (_str_index == str_index[0]) {
				specification.index = Index::ALL;
			} else if (_str_index == str_index[1]) {
				specification.index = Index::TERM;
			} else if (_str_index == str_index[2]) {
				specification.index = Index::VALUE;
			} else {
				throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
		}
	} catch (const msgpack::type_error&) {
		try {
			specification.index = (Index)properties.at(RESERVED_INDEX).obj.via.u64;
		} catch (const msgpack::type_error&) { }
	}

	// RESERVED_?_DETECTION is heritable but can't change.
	try {
		specification.date_detection = properties.at(RESERVED_D_DETECTION).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	try {
		specification.numeric_detection = properties.at(RESERVED_N_DETECTION).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	try {
		specification.geo_detection = properties.at(RESERVED_G_DETECTION).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	try {
		specification.bool_detection = properties.at(RESERVED_B_DETECTION).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	try {
		specification.string_detection = properties.at(RESERVED_S_DETECTION).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	// RESERVED_DYNAMIC is heritable but can't change.
	try {
		specification.dynamic = properties.at(RESERVED_DYNAMIC).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	// RESERVED_BOOL_TERM isn't heritable and can't change. It always will be in all fields.
	try {
		specification.bool_term = properties.at(RESERVED_BOOL_TERM).obj.via.boolean;
	} catch (const msgpack::type_error&) { }

	// RESERVED_TYPE isn't heritable and can't change once fixed the type field value.
	if (!is_root) {
		try {
			auto type = properties.at(RESERVED_TYPE);
			specification.sep_types[0] = type.at(0).obj.via.u64;
			specification.sep_types[1] = type.at(1).obj.via.u64;
			specification.sep_types[2] = type.at(2).obj.via.u64;
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
				auto prefix = properties.at(RESERVED_PREFIX);
				specification.prefix = std::string(prefix.obj.via.str.ptr, prefix.obj.via.str.size);
				specification.slot = static_cast<unsigned>(properties.at(RESERVED_SLOT).obj.via.u64);
				specification.bool_term = properties.at(RESERVED_BOOL_TERM).obj.via.boolean;
				specification.accuracy.clear();
				specification.acc_prefix.clear();
				if (specification.sep_types[2] != STRING_TYPE && specification.sep_types[2] != BOOLEAN_TYPE) {
					auto accuracy = properties.at(RESERVED_ACCURACY);
					for (const auto acc : accuracy) {
						specification.accuracy.push_back(acc.obj.via.f64);
					}
					auto acc_prefix = properties.at(RESERVED_ACC_PREFIX);
					for (const auto acc_p : acc_prefix) {
						specification.acc_prefix.emplace_back(acc_p.obj.via.str.ptr, acc_p.obj.via.str.size);
					}
				}
			}
		} catch (const msgpack::type_error&) {
			try {
				specification.index = (Index)item_doc.at(RESERVED_TYPE).obj.via.u64;
				// If RESERVED_TYPE has not been fixed yet and its specified in the document, properties is updated.
				insert_noninheritable_data(properties, item_doc);
				update_required_data(properties, item_key);
			} catch (const msgpack::type_error&) { }
		}
	}
}


void
Schema::insert_noninheritable_data(MsgPack& properties, const MsgPack& item_doc)
{
	// Restarting reserved words than which are not inherited.
	specification.accuracy.clear();
	specification.acc_prefix.clear();
	specification.sep_types[0] = default_spc.sep_types[0];
	specification.sep_types[1] = default_spc.sep_types[1];
	specification.sep_types[2] = default_spc.sep_types[2];
	specification.bool_term = default_spc.bool_term;
	specification.prefix = default_spc.prefix;
	specification.slot = default_spc.slot;

	try {
		auto doc_type = item_doc.at(RESERVED_TYPE);
		if (doc_type.obj.type == msgpack::type::STR) {
			if (set_types(lower_string(doc_type.obj.via.str.ptr, doc_type.obj.via.str.size), specification.sep_types)) {
				auto type = properties[RESERVED_TYPE];
				type.add_item_to_array(specification.sep_types[0]);
				type.add_item_to_array(specification.sep_types[1]);
				type.add_item_to_array(specification.sep_types[2]);
				to_store = true;
			} else {
				throw MSG_Error("%s can be [object/][array/]< %s | %s | %s | %s | %s >", RESERVED_TYPE, NUMERIC_STR, STRING_STR, DATE_STR, BOOLEAN_STR, GEO_STR);
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_TYPE);
		}
	} catch (const msgpack::type_error&) { }

	try {
		size_t size_acc = 0;
		auto doc_accuracy = item_doc.at(RESERVED_ACCURACY);
		if (specification.sep_types[2] == NO_TYPE) {
			throw MSG_Error("You should specify %s, for verify if the %s is correct", RESERVED_TYPE, RESERVED_ACCURACY);
		}
		if (doc_accuracy.obj.type == msgpack::type::ARRAY) {
			switch (specification.sep_types[2]) {
				case GEO_TYPE: {
					auto partials = doc_accuracy.at(0);
					if (partials.obj.type == msgpack::type::BOOLEAN) {
						specification.accuracy.push_back(partials.obj.via.boolean);
					} else {
						throw MSG_Error("Data inconsistency, partials value in %s: %s should be boolean", RESERVED_ACCURACY, GEO_STR);
					}
					try {
						auto error = doc_accuracy.at(1);
						if (error.obj.type == msgpack::type::POSITIVE_INTEGER) {
							specification.accuracy.push_back(error.obj.via.u64 > HTM_MAX_ERROR ? HTM_MAX_ERROR : error.obj.via.u64 < HTM_MIN_ERROR ? HTM_MIN_ERROR : error.obj.via.u64);
						} else {
							throw MSG_Error("Data inconsistency, error value in %s: %s should be positive integer", RESERVED_ACCURACY, GEO_STR);
						}
						const auto it_e = doc_accuracy.end();
						for (auto it = doc_accuracy.begin() + 2; it != it_e; ++it) {
							auto _accuracy = *it;
							if (_accuracy.obj.type == msgpack::type::POSITIVE_INTEGER && _accuracy.obj.via.u64 <= HTM_MAX_LEVEL) {
								specification.accuracy.push_back(_accuracy.obj.via.u64);
							} else {
								throw MSG_Error("Data inconsistency, level value in %s: %s should be a positive number between 0 and %d", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
							}
						}
					} catch (const msgpack::type_error&) {
						specification.accuracy.push_back(def_accuracy_geo[1]);
					}
					std::sort(specification.accuracy.begin() + 2, specification.accuracy.end());
					std::unique(specification.accuracy.begin() + 2, specification.accuracy.end());
					size_acc = specification.accuracy.size() - 2;
					break;
				}
				case DATE_TYPE: {
					for (const auto _accuracy : doc_accuracy) {
						if (_accuracy.obj.type == msgpack::type::STR) {
							std::string str_accuracy(upper_string(_accuracy.obj.via.str.ptr, _accuracy.obj.via.str.size));
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
								throw MSG_Error("Data inconsistency, %s: %s should be subset of {%s, %s, %s, %s, %s, %s}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
							}
						} else {
							throw MSG_Error("Data inconsistency, %s in %s should be subset of {%s, %s, %s, %s, %s, %s]}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
						}
					}
					std::sort(specification.accuracy.begin(), specification.accuracy.end());
					std::unique(specification.accuracy.begin(), specification.accuracy.end());
					size_acc = specification.accuracy.size();
					break;
				}
				case NUMERIC_TYPE: {
					for (const auto _accuracy : doc_accuracy) {
						if (_accuracy.obj.type == msgpack::type::POSITIVE_INTEGER) {
							specification.accuracy.push_back(_accuracy.obj.via.u64);
						} else {
							throw MSG_Error("Data inconsistency, %s: %s should be an array of positive numbers", RESERVED_ACCURACY, NUMERIC_STR);
						}
					}
					std::sort(specification.accuracy.begin(), specification.accuracy.end());
					std::unique(specification.accuracy.begin(), specification.accuracy.end());
					size_acc = specification.accuracy.size();
					break;
				}
				default:
					throw MSG_Error("%s type does not have accuracy", Serialise::type(default_spc.sep_types[2]).c_str());
			}

			auto accuracy = properties[RESERVED_ACCURACY];
			for (const auto& acc : specification.accuracy) {
				accuracy.add_item_to_array(acc);
			}
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be array", RESERVED_ACCURACY);
		}

		// Accuracy prefix is taken into account only if accuracy is defined.
		try {
			auto doc_acc_prefix = item_doc.at(RESERVED_ACC_PREFIX);
			if (doc_acc_prefix.obj.type == msgpack::type::ARRAY) {
				if (doc_acc_prefix.obj.via.array.size != size_acc) {
					throw MSG_Error("Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
				}
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				for (const auto _acc_prefix : doc_acc_prefix) {
					if (_acc_prefix.obj.type == msgpack::type::STR) {
						specification.acc_prefix.emplace_back(_acc_prefix.obj.via.str.ptr, _acc_prefix.obj.via.str.size);
						acc_prefix.add_item_to_array(specification.acc_prefix.back());
					} else {
						throw MSG_Error("Data inconsistency, %s should be an array of strings", RESERVED_ACC_PREFIX);
					}
				}
				to_store = true;
			} else {
				throw MSG_Error("Data inconsistency, %s should be an array of strings", RESERVED_ACC_PREFIX);
			}
		} catch (const msgpack::type_error&) { }
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_prefix = item_doc.at(RESERVED_PREFIX);
		if (doc_prefix.obj.type == msgpack::type::STR) {
			specification.prefix = std::string(doc_prefix.obj.via.str.ptr, doc_prefix.obj.via.str.size);
			properties[RESERVED_PREFIX] = specification.prefix;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_PREFIX);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_slot = item_doc.at(RESERVED_SLOT);
		if (doc_slot.obj.type == msgpack::type::POSITIVE_INTEGER) {
			unsigned _slot = static_cast<unsigned>(doc_slot.obj.via.u64);
			if (_slot < DB_SLOT_RESERVED) {
				_slot += DB_SLOT_RESERVED;
			} else if (_slot == Xapian::BAD_VALUENO) {
				_slot = 0xfffffffe;
			}
			properties[RESERVED_SLOT] = _slot;
			specification.slot = _slot;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be a positive integer", RESERVED_SLOT);
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto doc_bool_term = item_doc.at(RESERVED_BOOL_TERM);
		if (doc_bool_term.obj.type == msgpack::type::BOOLEAN) {
			properties[RESERVED_BOOL_TERM] = doc_bool_term.obj.via.boolean;
			specification.bool_term = doc_bool_term.obj.via.boolean;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be a boolean", RESERVED_BOOL_TERM);
		}
	} catch (const msgpack::type_error&) { }
}


void
Schema::update_required_data(MsgPack& properties, const std::string& item_key)
{
	// Adds type to properties, if this has not been added.
	auto type = properties[RESERVED_TYPE];
	if (!type) {
		type.add_item_to_array(specification.sep_types[0]);
		type.add_item_to_array(specification.sep_types[1]);
		type.add_item_to_array(specification.sep_types[2]);
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
				auto accuracy = properties[RESERVED_ACCURACY];
				accuracy.add_item_to_array(def_accuracy_geo[0]);
				accuracy.add_item_to_array(def_accuracy_geo[1]);
				specification.accuracy.push_back(def_accuracy_geo[0]);
				specification.accuracy.push_back(def_accuracy_geo[1]);
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				const auto it_e = def_accuracy_geo.end();
				for (auto it = def_accuracy_geo.begin() + 2; it != it_e; ++it) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE));
					acc_prefix.add_item_to_array(specification.acc_prefix.back());
					accuracy.add_item_to_array(*it);
					specification.accuracy.push_back(*it);
				}
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				const auto it_e = specification.accuracy.end();
				for (auto it = specification.accuracy.begin() + 2; it != it_e; ++it) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE));
					acc_prefix.add_item_to_array(specification.acc_prefix.back());
				}
				to_store = true;
			}
			break;
		}
		case NUMERIC_TYPE: {
			if (specification.accuracy.empty()) {
				auto accuracy = properties[RESERVED_ACCURACY];
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				for (const auto& acc : def_accuracy_num) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE));
					acc_prefix.add_item_to_array(specification.acc_prefix.back());
					accuracy.add_item_to_array(acc);
					specification.accuracy.push_back(acc);
				}
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				for (const auto& acc : specification.accuracy) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE));
					acc_prefix.add_item_to_array(specification.acc_prefix.back());
				}
				to_store = true;
			}
			break;
		}
		case DATE_TYPE: {
			// Use default accuracy.
			if (specification.accuracy.empty()) {
				auto accuracy = properties[RESERVED_ACCURACY];
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				for (const auto& acc : def_acc_date) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE));
					acc_prefix.add_item_to_array(specification.acc_prefix.back());
					accuracy.add_item_to_array(acc);
					specification.accuracy.push_back(acc);
				}
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				auto acc_prefix = properties[RESERVED_ACC_PREFIX];
				for (const auto& acc : specification.accuracy) {
					specification.acc_prefix.push_back(get_prefix(item_key + std::to_string(acc), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE));
					acc_prefix.add_item_to_array(specification.acc_prefix.back());
				}
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
	try {
		auto type = item_schema.at(RESERVED_TYPE);
		std::vector<char> sep_types({ static_cast<char>(type.at(0).obj.via.u64), static_cast<char>(type.at(1).obj.via.u64), static_cast<char>(type.at(2).obj.via.u64) });
		type = str_type(sep_types);
		try {
			auto accuracy = item_schema.at(RESERVED_ACCURACY);
			if (sep_types[2] == DATE_TYPE) {
				for (auto _accuracy : accuracy) {
					_accuracy = str_time[_accuracy.obj.via.u64];
				}
			} else if (sep_types[2] == GEO_TYPE) {
				auto _partials = accuracy.at(0);
				_partials = _partials.obj.via.u64 ? true : false;
			}
		} catch (const msgpack::type_error&) { }
	} catch (const msgpack::type_error&) { }

	try {
		auto analyzer = item_schema.at(RESERVED_ANALYZER);
		for (auto _analyzer : analyzer) {
			_analyzer = str_analyzer[_analyzer.obj.via.u64];
		}
	} catch (const msgpack::type_error&) { }

	try {
		auto index = item_schema.at(RESERVED_INDEX);
		index = str_index[index.obj.via.u64];
	} catch (const msgpack::type_error&) { }

	// Process its offsprings.
	for (const auto item_key : item_schema) {
		std::string str_key(item_key.obj.via.str.ptr, item_key.obj.via.str.size);
		if (!is_reserved(str_key)) {
			readable(item_schema.at(str_key));
		}
	}
}


char
Schema::get_type(const MsgPack& item_doc)
{
	if (item_doc.obj.type == msgpack::type::MAP) {
		throw MSG_Error("%s can not be object", RESERVED_VALUE);
	}

	MsgPack field = item_doc.obj.type == msgpack::type::ARRAY ? item_doc.at(0) : item_doc;
	int type = field.obj.type;
	if (type == msgpack::type::ARRAY) {
		const auto it_e = item_doc.end();
		for (auto it = item_doc.begin() + 1; it != it_e; ++it) {
			if ((*it).obj.type != type) {
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
			std::string str_value(field.obj.via.str.ptr, field.obj.via.str.size);
			if (specification.bool_detection && !Serialise::boolean(str_value).empty()) {
				return BOOLEAN_TYPE;
			} else if (specification.date_detection && Datetime::isDate(str_value)) {
				return DATE_TYPE;
			} else if (specification.geo_detection && EWKT_Parser::isEWKT(str_value)) {
				return GEO_TYPE;
			} else if (specification.string_detection) {
				return STRING_TYPE;
			}
			break;
	}

	throw MSG_Error("%s: %s is ambiguous", RESERVED_VALUE, item_doc.to_json_string().c_str());
}
