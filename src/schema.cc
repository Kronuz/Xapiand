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

#include "database.h"
#include "log.h"


const specification_t default_spc;


specification_t::specification_t()
		: position({ -1 }),
		  weight({ 1 }),
		  language({ "en" }),
		  spelling({ false }),
		  positions({ false }),
		  analyzer({ Xapian::TermGenerator::STEM_SOME }),
		  slot(0),
		  sep_types({ NO_TYPE, NO_TYPE, NO_TYPE }),
		  index(ALL),
		  store(true),
		  dynamic(true),
		  date_detection(true),
		  numeric_detection(true),
		  geo_detection(true),
		  bool_detection(true),
		  string_detection(true),
		  bool_term(false) { }


std::string
specification_t::to_string()
{
	std::stringstream str;
	str << "\n{\n";
	str << "\t" << RESERVED_POSITION << ": [ ";
	for (size_t i = 0; i < position.size(); ++i) {
		str << position[i] << " ";
	}
	str << "]\n";
	str << "\t" << RESERVED_WEIGHT   << ": [ ";
	for (size_t i = 0; i < weight.size(); ++i) {
		str << weight[i] << " ";
	}
	str << "]\n";
	str << "\t" << RESERVED_LANGUAGE << ": [ ";
	for (size_t i = 0; i < language.size(); ++i) {
		str << language[i] << " ";
	}
	str << "]\n";
	str << "\t" << RESERVED_ACCURACY << ": [ ";
	if (sep_types[2] == DATE_TYPE) {
		for (size_t i = 0; i < accuracy.size(); ++i) {
			str << str_time[accuracy[i]] << " ";
		}
	} else {
		for (size_t i = 0; i < accuracy.size(); ++i) {
			str << accuracy[i] << " ";
		}
	}
	str << "]\n";
	str << "\t" << RESERVED_ACC_PREFIX  << ": [ ";
	for (size_t i = 0; i < acc_prefix.size(); ++i) {
		str << acc_prefix[i] << " ";
	}
	str << "]\n";
	str << "\t" << RESERVED_ANALYZER    << ": [ ";
	for (size_t i = 0; i < analyzer.size(); ++i) {
		str << str_analizer[analyzer[i]] << " ";
	}
	str << "]\n";
	str << "\t" << RESERVED_SPELLING    << ": [ ";
	for (size_t i = 0; i < spelling.size(); ++i) {
		str << (spelling[i] ? "true " : "false ");
	}
	str << "]\n";
	str << "\t" << RESERVED_POSITIONS   << ": [ ";
	for (size_t i = 0; i < positions.size(); ++i) {
		str << (positions[i] ? "true " : "false ");
	}
	str << "]\n";
	str << "\t" << RESERVED_TYPE        << ": " << str_type(sep_types) << "\n";
	str << "\t" << RESERVED_INDEX       << ": " << str_index[index] << "\n";
	str << "\t" << RESERVED_STORE       << ": " << ((store)             ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DYNAMIC     << ": " << ((dynamic)           ? "true" : "false") << "\n";
	str << "\t" << RESERVED_D_DETECTION << ": " << ((date_detection)    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_N_DETECTION << ": " << ((numeric_detection) ? "true" : "false") << "\n";
	str << "\t" << RESERVED_G_DETECTION << ": " << ((geo_detection)     ? "true" : "false") << "\n";
	str << "\t" << RESERVED_B_DETECTION << ": " << ((bool_detection)    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_S_DETECTION << ": " << ((string_detection)  ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_TERM   << ": " << ((bool_term)         ? "true" : "false") << "\n}\n";

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
		schema = unique_cJSON(cJSON_CreateObject());
		cJSON_AddItemToObject(schema.get(), RESERVED_VERSION, cJSON_CreateNumber(DB_VERSION_SCHEMA));
		cJSON_AddItemToObject(schema.get(), RESERVED_SCHEMA, cJSON_CreateObject());
	} else {
		to_store = false;
		schema = unique_cJSON(cJSON_Parse(s_schema.c_str()));

		if (!schema) {
			schema.reset();
			throw MSG_Error("Schema is corrupt, you need provide a new one. JSON Before: [%s]", cJSON_GetErrorPtr());
		}

		cJSON* version = cJSON_GetObjectItem(schema.get(), RESERVED_VERSION);
		if (!version || version->valuedouble != DB_VERSION_SCHEMA) {
			L(nullptr, "version: %s  %f ", version ? "EXIST" : "NULL", version ? version->valuedouble : 0.0);
			schema.reset();
			throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
		}
	}
}


cJSON*
Schema::get_properties_schema() {
	return cJSON_GetObjectItem(schema.get(), RESERVED_SCHEMA);
}


void
Schema::update_root(cJSON* properties, cJSON* root)
{
	// Reset specification
	specification = default_spc;

	cJSON* properties_id = cJSON_GetObjectItem(properties, RESERVED_ID);
	if (properties_id) {
		update(root, properties, true);
	} else {
		properties_id = cJSON_CreateObject(); // It is managed by properties.
		cJSON* type = cJSON_CreateArray(); // Managed by properties
		cJSON_AddItemToArray(type, cJSON_CreateNumber(NO_TYPE));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(NO_TYPE));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(STRING_TYPE));
		cJSON_AddItemToObject(properties_id, RESERVED_TYPE, type);
		cJSON_AddItemToObject(properties_id, RESERVED_INDEX, cJSON_CreateNumber(ALL));
		cJSON_AddItemToObject(properties_id, RESERVED_SLOT, cJSON_CreateNumber(DB_SLOT_ID));
		cJSON_AddItemToObject(properties_id, RESERVED_PREFIX, cJSON_CreateString(DOCUMENT_ID_TERM_PREFIX));
		cJSON_AddItemToObject(properties_id, RESERVED_BOOL_TERM, cJSON_CreateTrue());
		cJSON_AddItemToObject(properties, RESERVED_ID, properties_id);

		to_store = true;
		insert(root, properties, true);
	}
}


cJSON*
Schema::get_subproperties(cJSON* properties, const char* attr, cJSON* item)
{
	cJSON* subproperties = cJSON_GetObjectItem(properties, attr);
	if (subproperties) {
		found_field = true;
		update(item, subproperties);
	} else {
		to_store = true;
		found_field = false;
		subproperties = cJSON_CreateObject(); // It is managed by item.
		cJSON_AddItemToObject(properties, attr, subproperties);
		insert(item, subproperties);
	}

	return subproperties;
}


void
Schema::store()
{
	if (to_store) {
		unique_char_ptr _cprint(cJSON_Print(schema.get()));
		db->set_metadata(RESERVED_SCHEMA, _cprint.get());
		to_store = false;
	}
}


void
Schema::update_specification(cJSON* item)
{
	cJSON* spc;

	// RESERVED_POSITION is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITION))) {
		specification.position.clear();
		if (spc->type == cJSON_Number) {
			specification.position.push_back(spc->valueint);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _position = cJSON_GetArrayItem(spc, i);
				if (_position->type == cJSON_Number) {
					specification.position.push_back(_position->valueint);
				} else {
					throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
		}
	}

	// RESERVED_WEIGHT is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT))) {
		specification.weight.clear();
		if (spc->type == cJSON_Number) {
			specification.weight.push_back(spc->valueint);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _weight = cJSON_GetArrayItem(spc, i);
				if (_weight->type == cJSON_Number) {
					specification.weight.push_back(_weight->valueint);
				} else {
					throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
		}
	}

	// RESERVED_LANGUAGE is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE))) {
		specification.language.clear();
		if (spc->type == cJSON_String) {
			std::string lan = is_language(spc->valuestring) ? spc->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, spc->valuestring);
			specification.language.push_back(lan);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _language = cJSON_GetArrayItem(spc, i);
				if (_language->type == cJSON_String) {
					std::string lan = is_language(_language->valuestring) ? _language->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _language->valuestring);
					specification.language.push_back(lan.c_str());
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		}
	}

	// RESERVED_SPELLING is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_SPELLING))) {
		specification.spelling.clear();
		if (spc->type < cJSON_NULL) {
			specification.spelling.push_back(spc->type);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _spelling = cJSON_GetArrayItem(spc, i);
				if (_spelling->type < cJSON_NULL) {
					specification.spelling.push_back(_spelling->type);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		}
	}

	// RESERVED_POSITIONS is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS))) {
		specification.positions.clear();
		if (spc->type < cJSON_NULL) {
			specification.positions.push_back(spc->type);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _positions = cJSON_GetArrayItem(spc, i);
				if (_positions->type < cJSON_NULL) {
					specification.positions.push_back(_positions->type);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		}
	}

	// RESERVED_ANALYZER is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_ANALYZER))) {
		specification.analyzer.clear();
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0)      specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
			else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
			else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
			else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
			else throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* analyzer = cJSON_GetArrayItem(spc, i);
				if (spc->type == cJSON_String) {
					std::string _analyzer = stringtoupper(analyzer->valuestring);
					if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
					} else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
					} else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
					} else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
					} else {
						throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		}
	}

	// RESERVED_STORE is heritable and can change.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_STORE))) {
		if (spc->type < cJSON_NULL) {
			specification.store = spc->type;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
		}
	}

	// RESERVED_INDEX is heritable and can change.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_INDEX))) {
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_index[0].c_str()) == 0) {
				specification.index = ALL;
			} else if (strcasecmp(spc->valuestring, str_index[1].c_str()) == 0) {
				specification.index = TERM;
			} else if (strcasecmp(spc->valuestring, str_index[2].c_str()) == 0) {
				specification.index = VALUE;
			} else {
				throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
		}
	}
}


void
Schema::set_type(cJSON* field, const std::string &field_name, cJSON* properties) {
	specification.sep_types[2] = get_type(field);
	update_required_data(field_name, properties);
}


void
Schema::set_type_to_array(cJSON* properties)
{
	cJSON* _type = cJSON_GetObjectItem(properties, RESERVED_TYPE);
	if (_type && cJSON_GetArrayItem(_type, 1)->valueint == NO_TYPE) {
		cJSON_ReplaceItemInArray(_type, 1, cJSON_CreateNumber(ARRAY_TYPE));
		to_store = true;
	}
}

void
Schema::set_type_to_object(cJSON* properties)
{
	cJSON* _type = cJSON_GetObjectItem(properties, RESERVED_TYPE);
	if (_type && cJSON_GetArrayItem(_type, 0)->valueint == NO_TYPE) {
		cJSON_ReplaceItemInArray(_type, 0, cJSON_CreateNumber(OBJECT_TYPE));
		to_store = true;
	}
}


std::string
Schema::to_string(bool pretty)
{
	unique_cJSON schema_readable(cJSON_Duplicate(schema.get(), 1));
	cJSON* properties = cJSON_GetObjectItem(schema_readable.get(), RESERVED_SCHEMA);
	int elements = cJSON_GetArraySize(properties);
	for (int i = 0; i < elements; ++i) {
		cJSON* field = cJSON_GetArrayItem(properties, i);
		if (!is_reserved(field->string) || strcmp(field->string, RESERVED_ID) == 0) {
			readable(field);
		}
	}

	std::string str;
	if (pretty) {
		str = unique_char_ptr(cJSON_Print(schema_readable.get())).get();
	} else {
		str = unique_char_ptr(cJSON_PrintUnformatted(schema_readable.get())).get();
	}

	return str;
}


void
Schema::insert(cJSON* item, cJSON* properties, bool root)
{
	cJSON* spc;
	if ((spc = cJSON_GetObjectItem(item, RESERVED_D_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_D_DETECTION);
			specification.date_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_D_DETECTION);
			specification.date_detection = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_D_DETECTION);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_N_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_N_DETECTION);
			specification.numeric_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_N_DETECTION);
			specification.numeric_detection = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_N_DETECTION);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_G_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_G_DETECTION);
			specification.geo_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_G_DETECTION);
			specification.geo_detection = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_G_DETECTION);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_B_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_B_DETECTION);
			specification.bool_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_B_DETECTION);
			specification.bool_detection = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_B_DETECTION);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_S_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_S_DETECTION);
			specification.string_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_S_DETECTION);
			specification.string_detection = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_S_DETECTION);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITION))) {
		specification.position.clear();
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_Number) {
			specification.position.push_back(spc->valueint);
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(spc->valueint));
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _position = cJSON_GetArrayItem(spc, i);
				if (_position->type == cJSON_Number) {
					specification.position.push_back(_position->valueint);
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(_position->valueint));
				} else {
					throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
		}
		cJSON_AddItemToObject(properties, RESERVED_POSITION, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT))) {
		specification.weight.clear();
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_Number) {
			specification.weight.push_back(spc->valueint);
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(spc->valueint));
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _weight = cJSON_GetArrayItem(spc, i);
				if (_weight->type == cJSON_Number) {
					specification.weight.push_back(_weight->valueint);
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(_weight->valueint));
				} else {
					throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
		}
		cJSON_AddItemToObject(properties, RESERVED_WEIGHT, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE))) {
		specification.language.clear();
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_String) {
			std::string lan = is_language(spc->valuestring) ? spc->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, spc->valuestring);
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateString(lan.c_str()));
			specification.language.push_back(lan);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _language = cJSON_GetArrayItem(spc, i);
				if (_language->type == cJSON_String) {
					std::string lan = is_language(_language->valuestring) ? _language->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _language->valuestring);
					specification.language.push_back(lan.c_str());
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateString(lan.c_str()));
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		}
		cJSON_AddItemToObject(properties, RESERVED_LANGUAGE, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_SPELLING))) {
		specification.spelling.clear();
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_False) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
			specification.spelling.push_back(false);
		} else if (spc->type == cJSON_True) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
			specification.spelling.push_back(true);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _spelling = cJSON_GetArrayItem(spc, i);
				if (_spelling->type == cJSON_False) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
					specification.spelling.push_back(false);
				} else if (_spelling->type == cJSON_True) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
					specification.spelling.push_back(true);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		}
		cJSON_AddItemToObject(properties, RESERVED_SPELLING, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS))) {
		specification.positions.clear();
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_False) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
			specification.positions.push_back(false);
		} else if (spc->type == cJSON_True) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
			specification.positions.push_back(true);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _positions = cJSON_GetArrayItem(spc, i);
				if (_positions->type == cJSON_False) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
					specification.positions.push_back(false);
				} else if (_positions->type == cJSON_True) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
					specification.positions.push_back(true);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		}
		cJSON_AddItemToObject(properties, RESERVED_POSITIONS, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_STORE))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_STORE);
			specification.store = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_STORE);
			specification.store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_INDEX))) {
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_index[0].c_str()) == 0) {
				specification.index = ALL;
				cJSON_AddNumberToObject(properties, RESERVED_INDEX, ALL);
			} else if (strcasecmp(spc->valuestring, str_index[1].c_str()) == 0) {
				specification.index = TERM;
				cJSON_AddNumberToObject(properties, RESERVED_INDEX, TERM);
			} else if (strcasecmp(spc->valuestring, str_index[2].c_str()) == 0) {
				specification.index = VALUE;
				cJSON_AddNumberToObject(properties, RESERVED_INDEX, VALUE);
			} else {
				throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_ANALYZER))) {
		specification.analyzer.clear();
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_SOME));
			} else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_NONE));
			} else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL));
			} else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) {
				specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL_Z));
			} else {
				throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
			}
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* analyzer = cJSON_GetArrayItem(spc, i);
				if (spc->type == cJSON_String) {
					std::string _analyzer = stringtoupper(analyzer->valuestring);
					if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_SOME));
					} else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_NONE));
					} else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL));
					} else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL_Z));
					} else {
						throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		}
		cJSON_AddItemToObject(properties, RESERVED_ANALYZER, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_DYNAMIC))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_DYNAMIC);
			specification.dynamic = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_DYNAMIC);
			specification.dynamic = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_DYNAMIC);
		}
	}

	if (!root) {
		insert_inheritable_specifications(item, properties);
	}
}


void
Schema::update(cJSON* item, cJSON* properties, bool root)
{
	cJSON* spc;

	// RESERVED_POSITION is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITION))) {
		specification.position.clear();
		if (spc->type == cJSON_Number) {
			specification.position.push_back(spc->valueint);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _position = cJSON_GetArrayItem(spc, i);
				if (_position->type == cJSON_Number) {
					specification.position.push_back(_position->valueint);
				} else {
					throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_POSITION))) {
		specification.position.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; ++i) {
			specification.position.push_back(cJSON_GetArrayItem(spc, i)->valueint);
		}
	}

	// RESERVED_WEIGHT is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT))) {
		specification.weight.clear();
		if (spc->type == cJSON_Number) {
			specification.weight.push_back(spc->valueint);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _weight = cJSON_GetArrayItem(spc, i);
				if (_weight->type == cJSON_Number) {
					specification.weight.push_back(_weight->valueint);
				} else {
					throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_WEIGHT))) {
		specification.weight.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; ++i) {
			specification.weight.push_back(cJSON_GetArrayItem(spc, i)->valueint);
		}
	}

	// RESERVED_LANGUAGE is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE))) {
		specification.language.clear();
		if (spc->type == cJSON_String) {
			std::string lan = is_language(spc->valuestring) ? spc->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, spc->valuestring);
			specification.language.push_back(lan);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _language = cJSON_GetArrayItem(spc, i);
				if (_language->type == cJSON_String) {
					std::string lan = is_language(_language->valuestring) ? _language->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _language->valuestring);
					specification.language.push_back(lan.c_str());
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_LANGUAGE))) {
		specification.language.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; ++i) {
			specification.language.push_back(cJSON_GetArrayItem(spc, i)->valuestring);
		}
	}

	// RESERVED_SPELLING is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_SPELLING))) {
		specification.spelling.clear();
		if (spc->type < cJSON_NULL) {
			specification.spelling.push_back(spc->type);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _spelling = cJSON_GetArrayItem(spc, i);
				if (_spelling->type < cJSON_NULL) {
					specification.spelling.push_back(_spelling->type);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_SPELLING))) {
		specification.spelling.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; ++i) {
			specification.spelling.push_back(cJSON_GetArrayItem(spc, i)->type);
		}
	}

	// RESERVED_POSITIONS is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS))) {
		specification.positions.clear();
		if (spc->type < cJSON_NULL) {
			specification.positions.push_back(spc->type);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* _positions = cJSON_GetArrayItem(spc, i);
				if (_positions->type < cJSON_NULL) {
					specification.positions.push_back(_positions->type);
				} else {
					throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_POSITIONS))) {
		specification.positions.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; ++i) {
			specification.positions.push_back(cJSON_GetArrayItem(spc, i)->type);
		}
	}

	// RESERVED_ANALYZER is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_ANALYZER))) {
		specification.analyzer.clear();
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0)      specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
			else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
			else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
			else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
			else throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; ++i) {
				cJSON* analyzer = cJSON_GetArrayItem(spc, i);
				if (spc->type == cJSON_String) {
					std::string _analyzer = stringtoupper(analyzer->valuestring);
					if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
					} else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
					} else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
					} else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) {
						specification.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
					} else {
						throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
					}
				} else {
					throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
				}
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_ANALYZER))) {
		specification.analyzer.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; ++i) {
			specification.analyzer.push_back(cJSON_GetArrayItem(spc, i)->valueint);
		}
	}

	// RESERVED_STORE is heritable and can change.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_STORE))) {
		if (spc->type < cJSON_NULL) {
			specification.store = spc->type;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_STORE))) {
		specification.store = spc->type;
	}

	// RESERVED_INDEX is heritable and can change.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_INDEX))) {
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_index[0].c_str()) == 0) {
				specification.index = ALL;
			} else if (strcasecmp(spc->valuestring, str_index[1].c_str()) == 0) {
				specification.index = TERM;
			} else if (strcasecmp(spc->valuestring, str_index[2].c_str()) == 0) {
				specification.index = VALUE;
			} else {
				throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
		}
	} else if ((spc = cJSON_GetObjectItem(properties, RESERVED_INDEX))) {
		specification.index = spc->valueint;
	}

	// RESERVED_?_DETECTION is heritable but can't change.
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_D_DETECTION))) specification.date_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_N_DETECTION))) specification.numeric_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_G_DETECTION))) specification.geo_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_B_DETECTION))) specification.bool_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_S_DETECTION))) specification.string_detection = spc->type;

	// RESERVED_DYNAMIC is heritable but can't change.
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_DYNAMIC))) specification.dynamic = spc->type;

	// RESERVED_BOOL_TERM isn't heritable and can't change. It always will be in all fields.
	if ((spc = cJSON_GetObjectItem(properties, RESERVED_BOOL_TERM))) specification.bool_term = spc->type;

	// RESERVED_TYPE isn't heritable and can't change once fixed the type field value.
	if (!root) {
		if ((spc = cJSON_GetObjectItem(properties, RESERVED_TYPE))) {
			specification.sep_types[0] = cJSON_GetArrayItem(spc, 0)->valueint;
			specification.sep_types[1] = cJSON_GetArrayItem(spc, 1)->valueint;
			specification.sep_types[2] = cJSON_GetArrayItem(spc, 2)->valueint;
			// If the type field value hasn't fixed yet and its specified in the document, properties is updated.
			if (specification.sep_types[2] == NO_TYPE) {
				if ((spc = cJSON_GetObjectItem(item, RESERVED_TYPE))) {
					// In this point means that terms or values haven't been inserted with this field,
					// therefore, lets us to change prefix, slot and bool_term in properties.
					insert_inheritable_specifications(item, properties);
					update_required_data(item->string, properties);
				}
			} else {
				// If type has been defined, the next reserved words have been defined too.
				spc = cJSON_GetObjectItem(properties, RESERVED_PREFIX);
				specification.prefix = spc->valuestring;
				spc = cJSON_GetObjectItem(properties, RESERVED_SLOT);
				specification.slot = (unsigned int)spc->valuedouble;
				spc = cJSON_GetObjectItem(properties, RESERVED_BOOL_TERM);
				specification.bool_term = spc->type;
				spc = cJSON_GetObjectItem(properties, RESERVED_ACCURACY);
				specification.accuracy.clear();
				specification.acc_prefix.clear();
				if (specification.sep_types[2] != STRING_TYPE && specification.sep_types[2] != BOOLEAN_TYPE) {
					int elements = cJSON_GetArraySize(spc);
					for (int i = 0; i < elements; ++i) {
						cJSON* _acc = cJSON_GetArrayItem(spc, i);
						specification.accuracy.push_back(_acc->valuedouble);
					}
					spc = cJSON_GetObjectItem(properties, RESERVED_ACC_PREFIX);
					elements = cJSON_GetArraySize(spc);
					for (int i = 0; i < elements; ++i) {
						cJSON* _acc_p = cJSON_GetArrayItem(spc, i);
						specification.acc_prefix.push_back(_acc_p->valuestring);
					}
				}
			}
		} else if ((spc = cJSON_GetObjectItem(item, RESERVED_TYPE))) {
			// If RESERVED_TYPE has not been fixed yet and its specified in the document, properties is updated.
			insert_inheritable_specifications(item, properties);
			update_required_data(item->string, properties);
		}
	}
}


void
Schema::insert_inheritable_specifications(cJSON* item, cJSON* properties)
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

	cJSON* spc;
	if ((spc = cJSON_GetObjectItem(item, RESERVED_TYPE))) {
		if (spc->type == cJSON_String) {
			if (set_types(stringtolower(spc->valuestring), specification.sep_types)) {
				cJSON* type = cJSON_CreateArray(); // Managed by properties
				cJSON_AddItemToArray(type, cJSON_CreateNumber(specification.sep_types[0]));
				cJSON_AddItemToArray(type, cJSON_CreateNumber(specification.sep_types[1]));
				cJSON_AddItemToArray(type, cJSON_CreateNumber(specification.sep_types[2]));
				cJSON_AddItemToObject(properties, RESERVED_TYPE, type);
				to_store = true;
			} else {
				throw MSG_Error("This %s does not exist, it can be [object/][array/]< %s | %s | %s | %s | %s >", RESERVED_TYPE, NUMERIC_STR, STRING_STR, DATE_STR, BOOLEAN_STR, GEO_STR);
			}
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_TYPE);
		}
	}

	size_t size_acc = 0;
	if ((spc = cJSON_GetObjectItem(item, RESERVED_ACCURACY))) {
		if (default_spc.sep_types[2] == NO_TYPE) {
			throw MSG_Error("You should specify %s, for verify if the accuracy is correct", RESERVED_TYPE);
		}
		unique_cJSON acc_s(cJSON_CreateArray());
		if (spc->type == cJSON_Array) {
			if (default_spc.sep_types[2] == GEO_TYPE) {
				int elements = cJSON_GetArraySize(spc);
				cJSON* acc = cJSON_GetArrayItem(spc, 0);
				double val;
				if (acc->type == cJSON_Number) {
					val = acc->valuedouble > 0 ? 1 : 0;
				} else if (acc->type < cJSON_NULL) {
					val = acc->type;
				} else {
					throw MSG_Error("Data inconsistency, partials in %s should be a number or boolean", GEO_STR);
				}
				specification.accuracy.push_back(val);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(val));
				if (elements > 1) {
					acc = cJSON_GetArrayItem(spc, 1);
					if (acc->type == cJSON_Number) {
						val = acc->valuedouble > HTM_MAX_ERROR ? HTM_MAX_ERROR : acc->valuedouble < HTM_MIN_ERROR ? HTM_MIN_ERROR : acc->valuedouble;
					} else {
						throw MSG_Error("Data inconsistency, error in %s should be a number", GEO_STR);
					}
					specification.accuracy.push_back(val);
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(val));
					for (int i = 2; i < elements; ++i) {
						acc = cJSON_GetArrayItem(spc, i);
						if (acc->type == cJSON_Number && acc->valueint >= 0 && acc->valueint <= HTM_MAX_LEVEL) {
							specification.accuracy.push_back(acc->valueint);
						} else {
							throw MSG_Error("Data inconsistency, level for accuracy in %s should be an number between 0 and %d", GEO_STR, HTM_MAX_LEVEL);
						}
					}
				} else {
					specification.accuracy.push_back(def_accuracy_geo[1]);
				}
				std::sort(specification.accuracy.begin() + 2, specification.accuracy.end());
				std::unique(specification.accuracy.begin() + 2, specification.accuracy.end());
				size_acc = specification.accuracy.size() - 2;
			} else if (default_spc.sep_types[2] == DATE_TYPE) {
				int elements = cJSON_GetArraySize(spc);
				for (int i = 0; i < elements; ++i) {
					cJSON* acc = cJSON_GetArrayItem(spc, i);
					if (acc->type == cJSON_String) {
						if (strcasecmp(acc->valuestring, str_time[5].c_str()) == 0)      specification.accuracy.push_back(DB_YEAR2INT);
						else if (strcasecmp(acc->valuestring, str_time[4].c_str()) == 0) specification.accuracy.push_back(DB_MONTH2INT);
						else if (strcasecmp(acc->valuestring, str_time[3].c_str()) == 0) specification.accuracy.push_back(DB_DAY2INT);
						else if (strcasecmp(acc->valuestring, str_time[2].c_str()) == 0) specification.accuracy.push_back(DB_HOUR2INT);
						else if (strcasecmp(acc->valuestring, str_time[1].c_str()) == 0) specification.accuracy.push_back(DB_MINUTE2INT);
						else if (strcasecmp(acc->valuestring, str_time[0].c_str()) == 0) specification.accuracy.push_back(DB_SECOND2INT);
						else throw MSG_Error("Data inconsistency, %s in %s should be a subset of {%s, %s, %s, %s, %s, %s}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
					} else {
						throw MSG_Error("Data inconsistency, %s in %s should be a subset of {%s, %s, %s, %s, %s, %s]}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
					}
				}
				std::set<double> set_acc(specification.accuracy.begin(), specification.accuracy.end());
				specification.accuracy.assign(set_acc.begin(), set_acc.end());
				size_acc = specification.accuracy.size();
			} else if (default_spc.sep_types[2] == NUMERIC_TYPE) {
				int elements = cJSON_GetArraySize(spc);
				for (int i = 0; i < elements; ++i) {
					cJSON* acc = cJSON_GetArrayItem(spc, i);
					if (acc->type == cJSON_Number && acc->valuedouble >= 1.0) {
						specification.accuracy.push_back((uInt64)(acc->valuedouble));
					} else {
						throw MSG_Error("Data inconsistency, accuracy in %s should be an array of positive numbers", NUMERIC_STR);
					}
				}
				std::set<double> set_acc(specification.accuracy.begin(), specification.accuracy.end());
				specification.accuracy.assign(set_acc.begin(), set_acc.end());
				size_acc = specification.accuracy.size();
			} else {
				throw MSG_Error("%s type does not have accuracy", Serialise::type(default_spc.sep_types[2]).c_str());
			}
			for (auto it = specification.accuracy.begin(); it != specification.accuracy.end(); ++it) {
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(*it));
			}
			cJSON_AddItemToObject(properties, RESERVED_ACCURACY, acc_s.release());
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be an array");
		}

		// Accuracy prefix is taken into account only if accuracy is defined.
		if ((spc = cJSON_GetObjectItem(item, RESERVED_ACC_PREFIX))) {
			unique_cJSON acc_s(cJSON_CreateArray());
			if (spc->type == cJSON_Array) {
				size_t elements = cJSON_GetArraySize(spc);
				if (elements != size_acc) {
					throw "Data inconsistency, there must be a prefix for each accuracy";
				}
				for (size_t i = 0; i < elements; ++i) {
					cJSON* acc = cJSON_GetArrayItem(spc, (int)i);
					if (acc->type == cJSON_String) {
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateString(acc->valuestring));
						specification.acc_prefix.push_back(acc->valuestring);
					} else {
						throw MSG_Error("Data inconsistency, %s should be an array of strings", RESERVED_ACC_PREFIX);
					}
				}
				cJSON_AddItemToObject(properties, RESERVED_ACCURACY, acc_s.release());
				to_store = true;
			} else {
				throw MSG_Error("Data inconsistency, %s should be an array of strings", RESERVED_ACC_PREFIX);
			}
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_PREFIX))) {
		if (spc->type == cJSON_String) {
			cJSON_AddStringToObject(properties, RESERVED_PREFIX, spc->valuestring);
			specification.prefix = spc->valuestring;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be string", RESERVED_PREFIX);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_SLOT))) {
		if (spc->type == cJSON_Number) {
			unsigned int _slot = (unsigned int)spc->valuedouble;
			if (_slot < DB_SLOT_RESERVED) {
				_slot += DB_SLOT_RESERVED;
			} else if (_slot == Xapian::BAD_VALUENO) {
				_slot = 0xfffffffe;
			}
			cJSON_AddNumberToObject(properties, RESERVED_SLOT, _slot);
			specification.slot = _slot;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be positive integer", RESERVED_SLOT);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_BOOL_TERM))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(properties, RESERVED_BOOL_TERM);
			specification.bool_term = false;
			to_store = true;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(properties, RESERVED_BOOL_TERM);
			specification.bool_term = true;
			to_store = true;
		} else {
			throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_BOOL_TERM);
		}
	}
}


void
Schema::update_required_data(const std::string &name, cJSON* properties)
{
	// Add type to properties, if this has not been added.
	if (!cJSON_GetObjectItem(properties, RESERVED_TYPE)) {
		cJSON* type = cJSON_CreateArray(); // Managed by shema
		cJSON_AddItemToArray(type, cJSON_CreateNumber(specification.sep_types[0]));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(specification.sep_types[1]));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(specification.sep_types[2]));
		cJSON_AddItemToObject(properties, RESERVED_TYPE, type);
		to_store = true;
	}

	// Insert prefix
	if (!name.empty()) {
		if (specification.prefix == default_spc.prefix) {
			specification.prefix = get_prefix(name, DOCUMENT_CUSTOM_TERM_PREFIX, specification.sep_types[2]);
			cJSON_AddStringToObject(properties, RESERVED_PREFIX, specification.prefix.c_str());
			to_store = true;
		}
	} else {
		specification.prefix = default_spc.prefix;
	}

	// Insert slot.
	if (specification.slot == default_spc.slot) {
		specification.slot = get_slot(name);
		cJSON_AddNumberToObject(properties, RESERVED_SLOT, specification.slot);
		to_store = true;
	}

	if (!name.empty() && !cJSON_GetObjectItem(properties, RESERVED_BOOL_TERM)) {
		// By default, if the field name has upper characters then it is consider bool term.
		if (strhasupper(name)) {
			cJSON_AddTrueToObject(properties, RESERVED_BOOL_TERM);
			specification.bool_term = true;
			to_store = true;
		} else {
			cJSON_AddFalseToObject(properties, RESERVED_BOOL_TERM);
			specification.bool_term = false;
			to_store = true;
		}
	}

	// Set defualt accuracies.
	switch (specification.sep_types[2]) {
		case GEO_TYPE: {
			if (specification.accuracy.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray()), _accuracy(cJSON_CreateArray());
				cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(def_accuracy_geo[0]));
				cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(def_accuracy_geo[1]));
				specification.accuracy.push_back(def_accuracy_geo[0]);
				specification.accuracy.push_back(def_accuracy_geo[1]);
				for (auto it = def_accuracy_geo.begin() + 2; it != def_accuracy_geo.end(); ++it) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(*it));
					specification.accuracy.push_back(*it);
					specification.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(properties, RESERVED_ACCURACY, _accuracy.release());
				cJSON_AddItemToObject(properties, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray());
				for (auto it = specification.accuracy.begin() + 2; it != specification.accuracy.end(); ++it) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					specification.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(properties, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
				to_store = true;
			}
			break;
		}
		case NUMERIC_TYPE: {
			if (specification.accuracy.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray()), _accuracy(cJSON_CreateArray());
				for (auto it = def_accuracy_num.begin(); it != def_accuracy_num.end(); ++it) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(*it));
					specification.accuracy.push_back(*it);
					specification.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(properties, RESERVED_ACCURACY, _accuracy.release());
				cJSON_AddItemToObject(properties, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray());
				for (auto it = specification.accuracy.begin(); it != specification.accuracy.end(); ++it) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					specification.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(properties, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
				to_store = true;
			}
			break;
		}
		case DATE_TYPE: {
			// Use default accuracy.
			if (specification.accuracy.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray()), _accuracy(cJSON_CreateArray());
				for (auto it = def_acc_date.begin(); it != def_acc_date.end(); ++it) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(*it));
					specification.accuracy.push_back(*it);
					specification.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(properties, RESERVED_ACCURACY, _accuracy.release());
				cJSON_AddItemToObject(properties, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
				to_store = true;
			} else if (specification.acc_prefix.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray());
				for (auto it = specification.accuracy.begin(); it != specification.accuracy.end(); ++it) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					specification.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(properties, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
				to_store = true;
			}
			break;
		}
	}
}


void
Schema::readable(cJSON* field)
{
	// Change this field in readable form.
	cJSON* item;
	if ((item = cJSON_GetObjectItem(field, RESERVED_TYPE))) {
		std::vector<char> sep_types({ (char)(cJSON_GetArrayItem(item, 0)->valueint), (char)(cJSON_GetArrayItem(item, 1)->valueint), (char)(cJSON_GetArrayItem(item, 2)->valueint) });
		cJSON_ReplaceItemInObject(field, RESERVED_TYPE, cJSON_CreateString(str_type(sep_types).c_str()));
		item = cJSON_GetObjectItem(field, RESERVED_ACCURACY);
		if (item && sep_types[2] == DATE_TYPE) {
			int _size = cJSON_GetArraySize(item);
			for (int i = 0; i < _size; ++i) {
				cJSON_ReplaceItemInArray(item, i, cJSON_CreateString(str_time[(cJSON_GetArrayItem(item, i)->valueint)].c_str()));
			}
		} else if (item && sep_types[2] == GEO_TYPE) {
			cJSON_ReplaceItemInArray(item, 0, cJSON_GetArrayItem(item, 0)->valueint ? cJSON_CreateTrue() : cJSON_CreateFalse());
		}
	}
	if ((item = cJSON_GetObjectItem(field, RESERVED_ANALYZER))) {
		int _size = cJSON_GetArraySize(item);
		for (int i = 0; i < _size; ++i) {
			cJSON_ReplaceItemInArray(item, i, cJSON_CreateString(str_analizer[cJSON_GetArrayItem(item, i)->valueint].c_str()));
		}
	}
	if ((item = cJSON_GetObjectItem(field, RESERVED_INDEX))) {
		cJSON_ReplaceItemInObject(field, RESERVED_INDEX, cJSON_CreateString(str_index[item->valueint].c_str()));
	}

	// Process its offsprings.
	int _size = cJSON_GetArraySize(field);
	for (int i = 0; i < _size; ++i) {
		item = cJSON_GetArrayItem(field, i);
		if (!is_reserved(item->string)) {
			readable(item);
		}
	}
}


char
Schema::get_type(cJSON* _field)
{
	if (_field->type == cJSON_Object) {
		throw MSG_Error("%s can not be an object", RESERVED_VALUE);
	}

	cJSON* field;
	int type = _field->type;
	if (type == cJSON_Array) {
		int num_ele = cJSON_GetArraySize(_field);
		field = cJSON_GetArrayItem(_field, 0);
		type = field->type;
		if (type == cJSON_Array) {
			throw MSG_Error("It can not be indexed array of arrays");
		}
		for (int i = 1; i < num_ele; ++i) {
			field = cJSON_GetArrayItem(_field, i);
			if (field->type != type && type < 1 && field->type == 4) {
				throw MSG_Error("Different types of data");
			}
		}
		specification.sep_types[1] = ARRAY_TYPE;
	} else {
		field = _field;
	}

	switch (type) {
		case cJSON_Number:
			if (specification.numeric_detection) return NUMERIC_TYPE;
			break;
		case cJSON_False:
		case cJSON_True:
			if (specification.bool_detection) return BOOLEAN_TYPE;
			break;
		case cJSON_String:
			if (specification.bool_detection && !Serialise::boolean(field->valuestring).empty()) {
				return BOOLEAN_TYPE;
			} else if (specification.date_detection && Datetime::isDate(field->valuestring)) {
				return DATE_TYPE;
			} else if(specification.geo_detection && EWKT_Parser::isEWKT(field->valuestring)) {
				return GEO_TYPE;
			} else if (specification.string_detection) {
				return STRING_TYPE;
			}
			break;
	}

	unique_char_ptr _cprint(cJSON_Print(_field));
	throw MSG_Error("%s: %s is ambiguous", RESERVED_VALUE, _cprint.get());
}
