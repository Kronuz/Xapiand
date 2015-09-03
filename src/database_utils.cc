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

#include "database_utils.h"
#include "utils.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define FIND_TYPES_RE "(" OBJECT_STR "/)?(" ARRAY_STR "/)?(" DATE_STR "|" NUMERIC_STR "|" GEO_STR "|" BOOLEAN_STR "|" STRING_STR ")|(" OBJECT_STR ")"

pcre *compiled_find_types_re = NULL;


int read_mastery(const std::string &dir, bool force)
{
	LOG_DATABASE(NULL, "+ READING MASTERY OF INDEX '%s'...\n", dir.c_str());

	struct stat info;
	if (stat(dir.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
		LOG_DATABASE(NULL, "- NO MASTERY OF INDEX '%s'\n", dir.c_str());
		return -1;
	}

	int mastery_level = -1;
	unsigned char buf[512];

	int fd = open((dir + "/mastery").c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if(force) {
			mastery_level = (int)time(0);
			fd = open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
			if (fd >= 0) {
				snprintf((char *)buf, sizeof(buf), "%d", mastery_level);
				write(fd, buf, strlen((char *)buf));
				close(fd);
			}
		}
	} else {
		mastery_level = 0;
		size_t length = read(fd, (char *)buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			mastery_level = atoi((const char *)buf);
		}
		close(fd);
		if (!mastery_level) {
			mastery_level = (int)time(0);
			fd = open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
			if (fd >= 0) {
				snprintf((char *)buf, sizeof(buf), "%d", mastery_level);
				write(fd, buf, strlen((char *)buf));
				close(fd);
			}
		}
	}

	LOG_DATABASE(NULL, "- MASTERY OF INDEX '%s' is %d\n", dir.c_str(), mastery_level);

	return mastery_level;
}


bool is_reserved(const std::string &word)
{
	return word.at(0) == '_' ? true : false;
}


void update_required_data(specifications_t &spc, const std::string &name, cJSON *schema)
{
	// Add type to schema, if this has not been added.
	if (!cJSON_GetObjectItem(schema, RESERVED_TYPE)) {
		cJSON *type = cJSON_CreateArray(); // Managed by shema
		cJSON_AddItemToArray(type, cJSON_CreateNumber(spc.sep_types[0]));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(spc.sep_types[1]));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(spc.sep_types[2]));
		cJSON_AddItemToObject(schema, RESERVED_TYPE, type);
	}

	// Insert prefix
	if (!name.empty()) {
		if (spc.prefix == default_spc.prefix) {
			spc.prefix = get_prefix(name, DOCUMENT_CUSTOM_TERM_PREFIX, spc.sep_types[2]);
			cJSON_AddStringToObject(schema, RESERVED_PREFIX, spc.prefix.c_str());
		}
	} else spc.prefix = DOCUMENT_CUSTOM_TERM_PREFIX;

	// Insert slot.
	if (spc.slot == default_spc.slot) {
		spc.slot = get_slot(name);
		cJSON_AddNumberToObject(schema, RESERVED_SLOT, spc.slot);
	}

	if (!name.empty() && !cJSON_GetObjectItem(schema, RESERVED_BOOL_TERM)) {
		// By default, if the field name has upper characters then it is consider bool term.
		if (strhasupper(name)) {
			cJSON_AddTrueToObject(schema, RESERVED_BOOL_TERM);
			spc.bool_term = true;
		} else {
			cJSON_AddFalseToObject(schema, RESERVED_BOOL_TERM);
			spc.bool_term = false;
		}
	}

	// Set defualt accuracies.
	switch (spc.sep_types[2]) {
		case GEO_TYPE: {
			if (spc.accuracy.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray(), cJSON_Delete), _accuracy(cJSON_CreateArray(), cJSON_Delete);
				cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(def_accuracy_geo[0]));
				cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(def_accuracy_geo[1]));
				spc.accuracy.push_back(def_accuracy_geo[0]);
				spc.accuracy.push_back(def_accuracy_geo[1]);
				for (std::vector<double>::const_iterator it(def_accuracy_geo.begin() + 2); it != def_accuracy_geo.end(); it++) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(*it));
					spc.accuracy.push_back(*it);
					spc.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACCURACY, _accuracy.release());
				cJSON_AddItemToObject(schema, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
			} else if (spc.acc_prefix.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray(), cJSON_Delete);
				for (std::vector<double>::iterator it(spc.accuracy.begin() + 2); it != spc.accuracy.end(); it++) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, GEO_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					spc.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
			}
			break;
		}
		case NUMERIC_TYPE: {
			if (spc.accuracy.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray(), cJSON_Delete), _accuracy(cJSON_CreateArray(), cJSON_Delete);
				for (std::vector<double>::const_iterator it(def_accuracy_num.begin()); it != def_accuracy_num.end(); it++) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(*it));
					spc.accuracy.push_back(*it);
					spc.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACCURACY, _accuracy.release());
				cJSON_AddItemToObject(schema, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
			} else if (spc.acc_prefix.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray(), cJSON_Delete);
				for (std::vector<double>::iterator it(spc.accuracy.begin()); it != spc.accuracy.end(); it++) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, NUMERIC_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					spc.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
			}
			break;
		}
		case DATE_TYPE: {
			// Use default accuracy.
			if (spc.accuracy.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray(), cJSON_Delete), _accuracy(cJSON_CreateArray(), cJSON_Delete);
				for (std::vector<double>::const_iterator it(def_acc_date.begin()); it != def_acc_date.end(); it++) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					cJSON_AddItemToArray(_accuracy.get(), cJSON_CreateNumber(*it));
					spc.accuracy.push_back(*it);
					spc.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACCURACY, _accuracy.release());
				cJSON_AddItemToObject(schema, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
			} else if (spc.acc_prefix.empty()) {
				unique_cJSON _prefix_accuracy(cJSON_CreateArray(), cJSON_Delete);
				for (std::vector<double>::iterator it(spc.accuracy.begin()); it != spc.accuracy.end(); it++) {
					std::string prefix = get_prefix(name + std::to_string(*it), DOCUMENT_CUSTOM_TERM_PREFIX, DATE_TYPE);
					cJSON_AddItemToArray(_prefix_accuracy.get(), cJSON_CreateString(prefix.c_str()));
					spc.acc_prefix.push_back(prefix);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACC_PREFIX, _prefix_accuracy.release());
			}
			break;
		}
	}
}


void update_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema, bool root)
{
	cJSON *spc;
	// RESERVED_POSITION is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITION))) {
		spc_now.position.clear();
		if (spc->type == cJSON_Number) spc_now.position.push_back(spc->valueint);
		else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_position = cJSON_GetArrayItem(spc, i);
				if (_position->type == cJSON_Number) spc_now.position.push_back(_position->valueint);
				else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
			}
		} else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_POSITION))) {
		spc_now.position.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; i++)
			spc_now.position.push_back(cJSON_GetArrayItem(spc, i)->valueint);
	}

	// RESERVED_WEIGHT is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT))) {
		spc_now.weight.clear();
		if (spc->type == cJSON_Number) spc_now.weight.push_back(spc->valueint);
		else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_weight = cJSON_GetArrayItem(spc, i);
				if (_weight->type == cJSON_Number) spc_now.weight.push_back(_weight->valueint);
				else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
			}
		} else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_WEIGHT))) {
		spc_now.weight.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; i++)
			spc_now.weight.push_back(cJSON_GetArrayItem(spc, i)->valueint);
	}

	// RESERVED_LANGUAGE is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE))) {
		spc_now.language.clear();
		if (spc->type == cJSON_String) {
			std::string lan = is_language(spc->valuestring) ? spc->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, spc->valuestring);
			spc_now.language.push_back(lan);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_language = cJSON_GetArrayItem(spc, i);
				if (_language->type == cJSON_String) {
					std::string lan = is_language(_language->valuestring) ? _language->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _language->valuestring);
					spc_now.language.push_back(lan.c_str());
				} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
			}
		} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_LANGUAGE))) {
		spc_now.language.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; i++)
			spc_now.language.push_back(cJSON_GetArrayItem(spc, i)->valuestring);
	}

	// RESERVED_SPELLING is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_SPELLING))) {
		spc_now.spelling.clear();
		if (spc->type < cJSON_NULL) spc_now.spelling.push_back(spc->type);
		else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_spelling = cJSON_GetArrayItem(spc, i);
				if (_spelling->type < cJSON_NULL) spc_now.spelling.push_back(_spelling->type);
				else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
			}
		} else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_SPELLING))) {
		spc_now.spelling.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; i++)
			spc_now.spelling.push_back(cJSON_GetArrayItem(spc, i)->type);
	}

	// RESERVED_POSITIONS is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS))) {
		spc_now.positions.clear();
		if (spc->type < cJSON_NULL) spc_now.positions.push_back(spc->type);
		else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_positions = cJSON_GetArrayItem(spc, i);
				if (_positions->type < cJSON_NULL) spc_now.positions.push_back(_positions->type);
				else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
			}
		} else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_POSITIONS))) {
		spc_now.positions.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; i++)
			spc_now.positions.push_back(cJSON_GetArrayItem(spc, i)->type);
	}

	// RESERVED_ANALYZER is heritable and can change between documents.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_ANALYZER))) {
		spc_now.analyzer.clear();
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0)      spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
			else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
			else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
			else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
			else throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *analyzer = cJSON_GetArrayItem(spc, i);
				if (spc->type == cJSON_String) {
					std::string _analyzer = stringtoupper(analyzer->valuestring);
					if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0)      spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
					else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
					else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
					else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
					else throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
				} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
			}
		} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_ANALYZER))) {
		spc_now.analyzer.clear();
		int elements = cJSON_GetArraySize(spc);
		for (int i = 0; i < elements; i++)
			spc_now.analyzer.push_back(cJSON_GetArrayItem(spc, i)->valueint);
	}

	// RESERVED_STORE is heritable and can change.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_STORE))) {
		if (spc->type < cJSON_NULL) spc_now.store = spc->type;
		else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_STORE))) spc_now.store = spc->type;

	// RESERVED_INDEX is heritable and can change.
	if ((spc = cJSON_GetObjectItem(item, RESERVED_INDEX))) {
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_index[0].c_str()) == 0) spc_now.index = ALL;
			else if (strcasecmp(spc->valuestring, str_index[1].c_str()) == 0) spc_now.index = TERM;
			else if (strcasecmp(spc->valuestring, str_index[2].c_str()) == 0) spc_now.index = VALUE;
			else throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
		} else throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
	} else if ((spc = cJSON_GetObjectItem(schema, RESERVED_INDEX))) spc_now.index = spc->valueint;

	// RESERVED_?_DETECTION is heritable but can't change.
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_D_DETECTION))) spc_now.date_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_N_DETECTION))) spc_now.numeric_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_G_DETECTION))) spc_now.geo_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_B_DETECTION))) spc_now.bool_detection = spc->type;
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_S_DETECTION))) spc_now.string_detection = spc->type;

	// RESERVED_DYNAMIC is heritable but can't change.
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_DYNAMIC))) spc_now.dynamic = spc->type;

	// RESERVED_BOOL_TERM isn't heritable and can't change. It always will be in all fields.
	if ((spc = cJSON_GetObjectItem(schema, RESERVED_BOOL_TERM))) spc_now.bool_term = spc->type;

	// RESERVED_TYPE isn't heritable and can't change once fixed the type field value.
	if (!root) {
		if ((spc = cJSON_GetObjectItem(schema, RESERVED_TYPE))) {
			spc_now.sep_types[0] = cJSON_GetArrayItem(spc, 0)->valueint;
			spc_now.sep_types[1] = cJSON_GetArrayItem(spc, 1)->valueint;
			spc_now.sep_types[2] = cJSON_GetArrayItem(spc, 2)->valueint;
			// If the type field value hasn't fixed yet and its specified in the document, the schema is updated.
			if (spc_now.sep_types[2] == NO_TYPE) {
				if ((spc = cJSON_GetObjectItem(item, RESERVED_TYPE))) {
					// In this point means that terms or values haven't been inserted with this field,
					// therefore, lets us to change prefix, slot and bool_term in the schema.
					insert_inheritable_specifications(item, spc_now, schema);
					update_required_data(spc_now, item->string, schema);
				}
			} else {
				// If type has been defined, the next reserved words have been defined too.
				spc = cJSON_GetObjectItem(schema, RESERVED_PREFIX);
				spc_now.prefix = spc->valuestring;
				spc = cJSON_GetObjectItem(schema, RESERVED_SLOT);
				spc_now.slot = (unsigned int)spc->valuedouble;
				spc = cJSON_GetObjectItem(schema, RESERVED_BOOL_TERM);
				spc_now.bool_term = spc->type;
				spc = cJSON_GetObjectItem(schema, RESERVED_ACCURACY);
				spc_now.accuracy.clear();
				spc_now.acc_prefix.clear();
				if (spc_now.sep_types[2] != STRING_TYPE && spc_now.sep_types[2] != BOOLEAN_TYPE) {
					int elements = cJSON_GetArraySize(spc);
					for (int i = 0; i < elements; i++) {
						cJSON *_acc = cJSON_GetArrayItem(spc, i);
						spc_now.accuracy.push_back(_acc->valuedouble);
					}
					spc = cJSON_GetObjectItem(schema, RESERVED_ACC_PREFIX);
					elements = cJSON_GetArraySize(spc);
					for (int i = 0; i < elements; i++) {
						cJSON *_acc_p = cJSON_GetArrayItem(spc, i);
						spc_now.acc_prefix.push_back(_acc_p->valuestring);
					}
				}
			}
		} else if ((spc = cJSON_GetObjectItem(item, RESERVED_TYPE))) {
			// If RESERVED_TYPE has not been fixed yet and its specified in the document, the schema is updated.
			insert_inheritable_specifications(item, spc_now, schema);
			update_required_data(spc_now, item->string, schema);
		}
	}
}


void insert_inheritable_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema)
{
	// Restarting reserved words than which are not inherited.
	spc_now.accuracy.clear();
	spc_now.acc_prefix.clear();
	spc_now.sep_types[0] = default_spc.sep_types[0];
	spc_now.sep_types[1] = default_spc.sep_types[1];
	spc_now.sep_types[2] = default_spc.sep_types[2];
	spc_now.bool_term = default_spc.bool_term;
	spc_now.prefix = default_spc.prefix;
	spc_now.slot = default_spc.slot;

	cJSON *spc;
	if ((spc = cJSON_GetObjectItem(item, RESERVED_TYPE))) {
		if (spc->type == cJSON_String) {
			if (set_types(stringtolower(spc->valuestring), spc_now.sep_types)) {
				cJSON *type = cJSON_CreateArray(); // Managed by schema
				cJSON_AddItemToArray(type, cJSON_CreateNumber(spc_now.sep_types[0]));
				cJSON_AddItemToArray(type, cJSON_CreateNumber(spc_now.sep_types[1]));
				cJSON_AddItemToArray(type, cJSON_CreateNumber(spc_now.sep_types[2]));
				cJSON_AddItemToObject(schema, RESERVED_TYPE, type);
			}
			else throw MSG_Error("This %s does not exist, it can be [object/][array/]< %s | %s | %s | %s | %s >", RESERVED_TYPE, NUMERIC_STR, STRING_STR, DATE_STR, BOOLEAN_STR, GEO_STR);
		} else throw MSG_Error("Data inconsistency, %s should be string", RESERVED_TYPE);
	}

	size_t size_acc = 0;
	if ((spc = cJSON_GetObjectItem(item, RESERVED_ACCURACY))) {
		if (default_spc.sep_types[2] == NO_TYPE) throw MSG_Error("You should specify %s, for verify if the accuracy is correct", RESERVED_TYPE);
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_Array) {
			if (default_spc.sep_types[2] == GEO_TYPE) {
				int elements = cJSON_GetArraySize(spc);
				cJSON *acc = cJSON_GetArrayItem(spc, 0);
				double val;
				if (acc->type == cJSON_Number) val = acc->valuedouble > 0 ? 1 : 0;
				else if (acc->type < cJSON_NULL) val = acc->type;
				else throw MSG_Error("Data inconsistency, partials in %s should be a number or boolean", GEO_STR);
				spc_now.accuracy.push_back(val);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(val));
				if (elements > 1) {
					acc = cJSON_GetArrayItem(spc, 1);
					if (acc->type == cJSON_Number) {
						val = acc->valuedouble > HTM_MAX_ERROR ? HTM_MAX_ERROR : acc->valuedouble < HTM_MIN_ERROR ? HTM_MIN_ERROR : acc->valuedouble;
					} else throw MSG_Error("Data inconsistency, error in %s should be a number", GEO_STR);
					spc_now.accuracy.push_back(val);
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(val));
					for (int i = 2; i < elements; i++) {
						acc = cJSON_GetArrayItem(spc, i);
						if (acc->type == cJSON_Number && acc->valueint >= 0 && acc->valueint <= HTM_MAX_LEVEL) {
							spc_now.accuracy.push_back(acc->valueint);
						} else throw MSG_Error("Data inconsistency, level for accuracy in %s should be an number between 0 and %d", GEO_STR, HTM_MAX_LEVEL);
					}
				} else spc_now.accuracy.push_back(def_accuracy_geo[1]);
				std::sort(spc_now.accuracy.begin() + 2, spc_now.accuracy.end());
				std::unique(spc_now.accuracy.begin() + 2, spc_now.accuracy.end());
				size_acc = spc_now.accuracy.size() - 2;
			} else if (default_spc.sep_types[2] == DATE_TYPE) {
				int elements = cJSON_GetArraySize(spc);
				for (int i = 0; i < elements; i++) {
					cJSON *acc = cJSON_GetArrayItem(spc, i);
					if (acc->type == cJSON_String) {
						if (strcasecmp(acc->valuestring, str_time[5].c_str()) == 0)      spc_now.accuracy.push_back(DB_YEAR2INT);
						else if (strcasecmp(acc->valuestring, str_time[4].c_str()) == 0) spc_now.accuracy.push_back(DB_MONTH2INT);
						else if (strcasecmp(acc->valuestring, str_time[3].c_str()) == 0) spc_now.accuracy.push_back(DB_DAY2INT);
						else if (strcasecmp(acc->valuestring, str_time[2].c_str()) == 0) spc_now.accuracy.push_back(DB_HOUR2INT);
						else if (strcasecmp(acc->valuestring, str_time[1].c_str()) == 0) spc_now.accuracy.push_back(DB_MINUTE2INT);
						else if (strcasecmp(acc->valuestring, str_time[0].c_str()) == 0) spc_now.accuracy.push_back(DB_SECOND2INT);
						else throw MSG_Error("Data inconsistency, %s in %s should be a subset of {%s, %s, %s, %s, %s, %s}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
					} else throw MSG_Error("Data inconsistency, %s in %s should be a subset of {%s, %s, %s, %s, %s, %s]}", RESERVED_ACCURACY, DATE_STR, str_time[1].c_str(), str_time[2].c_str(), str_time[3].c_str(), str_time[4].c_str(), str_time[5].c_str());
				}
				std::set<double> set_acc(spc_now.accuracy.begin(), spc_now.accuracy.end());
				spc_now.accuracy.assign(set_acc.begin(), set_acc.end());
				size_acc = spc_now.accuracy.size();
			} else if (default_spc.sep_types[2] == NUMERIC_TYPE) {
				int elements = cJSON_GetArraySize(spc);
				for (int i = 0; i < elements; i++) {
					cJSON *acc = cJSON_GetArrayItem(spc, i);
					if (acc->type == cJSON_Number && acc->valuedouble >= 1.0) {
						spc_now.accuracy.push_back((uInt64)(acc->valuedouble));
					} else throw MSG_Error("Data inconsistency, accuracy in %s should be an array of positive numbers", NUMERIC_STR);
				}
				std::set<double> set_acc(spc_now.accuracy.begin(), spc_now.accuracy.end());
				spc_now.accuracy.assign(set_acc.begin(), set_acc.end());
				size_acc = spc_now.accuracy.size();
			} else throw MSG_Error("%s type does not have accuracy", Serialise::type(default_spc.sep_types[2]).c_str());

			for (std::vector<double>::iterator it(spc_now.accuracy.begin()); it != spc_now.accuracy.end(); it++)
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(*it));
			cJSON_AddItemToObject(schema, RESERVED_ACCURACY, acc_s.release());
		} else throw MSG_Error("Data inconsistency, %s should be an array");

		// Accuracy prefix is taken into account only if accuracy is defined.
		if ((spc = cJSON_GetObjectItem(item, RESERVED_ACC_PREFIX))) {
			unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
			if (spc->type == cJSON_Array) {
				int elements = cJSON_GetArraySize(spc);
				if (elements != size_acc) throw "Data inconsistency, there must be a prefix for each accuracy";
				for (int i = 0; i < elements; i++) {
					cJSON *acc = cJSON_GetArrayItem(spc, i);
					if (acc->type == cJSON_String) {
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateString(acc->valuestring));
						spc_now.acc_prefix.push_back(acc->valuestring);
					} else throw MSG_Error("Data inconsistency, %s should be an array of strings", RESERVED_ACC_PREFIX);
				}
				cJSON_AddItemToObject(schema, RESERVED_ACCURACY, acc_s.release());
			} else throw MSG_Error("Data inconsistency, %s should be an array of strings", RESERVED_ACC_PREFIX);
		}
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_PREFIX))) {
		if (spc->type == cJSON_String) {
			cJSON_AddStringToObject(schema, RESERVED_PREFIX, spc->valuestring);
			spc_now.prefix = spc->valuestring;
		} else throw MSG_Error("Data inconsistency, %s should be string", RESERVED_PREFIX);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_SLOT))) {
		if (spc->type == cJSON_Number) {
			unsigned int _slot = (unsigned int)spc->valuedouble;
			if (_slot == 0x00000000) _slot = 0x00000001; // 0->id
			else if (_slot == Xapian::BAD_VALUENO) _slot = 0xfffffffe;
			cJSON_AddNumberToObject(schema, RESERVED_SLOT, _slot);
			spc_now.slot = _slot;
		} else throw MSG_Error("Data inconsistency, %s should be positive integer", RESERVED_SLOT);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_BOOL_TERM))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_BOOL_TERM);
			spc_now.bool_term = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_BOOL_TERM);
			spc_now.bool_term = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_BOOL_TERM);
	}
}


void insert_specifications(cJSON *item, specifications_t &spc_now, cJSON *schema, bool root)
{
	cJSON *spc;
	if ((spc = cJSON_GetObjectItem(item, RESERVED_D_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_D_DETECTION);
			spc_now.date_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_D_DETECTION);
			spc_now.date_detection = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_D_DETECTION);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_N_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_N_DETECTION);
			spc_now.numeric_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_N_DETECTION);
			spc_now.numeric_detection = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_N_DETECTION);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_G_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_G_DETECTION);
			spc_now.geo_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_G_DETECTION);
			spc_now.geo_detection = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_G_DETECTION);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_B_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_B_DETECTION);
			spc_now.bool_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_B_DETECTION);
			spc_now.bool_detection = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_B_DETECTION);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_S_DETECTION))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_S_DETECTION);
			spc_now.string_detection = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_S_DETECTION);
			spc_now.string_detection = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_S_DETECTION);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITION))) {
		spc_now.position.clear();
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_Number) {
			spc_now.position.push_back(spc->valueint);
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(spc->valueint));
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_position = cJSON_GetArrayItem(spc, i);
				if (_position->type == cJSON_Number) {
					spc_now.position.push_back(_position->valueint);
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(_position->valueint));
				} else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
			}
		} else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_POSITION);
		cJSON_AddItemToObject(schema, RESERVED_POSITION, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_WEIGHT))) {
		spc_now.weight.clear();
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_Number) {
			spc_now.weight.push_back(spc->valueint);
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(spc->valueint));
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_weight = cJSON_GetArrayItem(spc, i);
				if (_weight->type == cJSON_Number) {
					spc_now.weight.push_back(_weight->valueint);
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(_weight->valueint));
				} else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
			}
		} else throw MSG_Error("Data inconsistency, %s should be integer or array of integers", RESERVED_WEIGHT);
		cJSON_AddItemToObject(schema, RESERVED_WEIGHT, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_LANGUAGE))) {
		spc_now.language.clear();
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_String) {
			std::string lan = is_language(spc->valuestring) ? spc->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, spc->valuestring);
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateString(lan.c_str()));
			spc_now.language.push_back(lan);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_language = cJSON_GetArrayItem(spc, i);
				if (_language->type == cJSON_String) {
					std::string lan = is_language(_language->valuestring) ? _language->valuestring : throw MSG_Error("%s: %s is not supported", RESERVED_LANGUAGE, _language->valuestring);
					spc_now.language.push_back(lan.c_str());
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateString(lan.c_str()));
				} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
			}
		} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_LANGUAGE);
		cJSON_AddItemToObject(schema, RESERVED_LANGUAGE, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_SPELLING))) {
		spc_now.spelling.clear();
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_False) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
			spc_now.spelling.push_back(false);
		} else if (spc->type == cJSON_True) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
			spc_now.spelling.push_back(true);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_spelling = cJSON_GetArrayItem(spc, i);
				if (_spelling->type == cJSON_False) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
					spc_now.spelling.push_back(false);
				} else if (_spelling->type == cJSON_True) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
					spc_now.spelling.push_back(true);
				} else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
			}
		} else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_SPELLING);
		cJSON_AddItemToObject(schema, RESERVED_SPELLING, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_POSITIONS))) {
		spc_now.positions.clear();
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_False) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
			spc_now.positions.push_back(false);
		} else if (spc->type == cJSON_True) {
			cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
			spc_now.positions.push_back(true);
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *_positions = cJSON_GetArrayItem(spc, i);
				if (_positions->type == cJSON_False) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateFalse());
					spc_now.positions.push_back(false);
				} else if (_positions->type == cJSON_True) {
					cJSON_AddItemToArray(acc_s.get(), cJSON_CreateTrue());
					spc_now.positions.push_back(true);
				} else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
			}
		} else throw MSG_Error("Data inconsistency, %s should be boolean or array of booleans", RESERVED_POSITIONS);
		cJSON_AddItemToObject(schema, RESERVED_POSITIONS, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_STORE))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_STORE);
			spc_now.store = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_STORE);
			spc_now.store = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_STORE);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_INDEX))) {
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_index[0].c_str()) == 0) {
				spc_now.index = ALL;
				cJSON_AddNumberToObject(schema, RESERVED_INDEX, ALL);
			} else if (strcasecmp(spc->valuestring, str_index[1].c_str()) == 0) {
				spc_now.index = TERM;
				cJSON_AddNumberToObject(schema, RESERVED_INDEX, TERM);
			} else if (strcasecmp(spc->valuestring, str_index[2].c_str()) == 0) {
				spc_now.index = VALUE;
				cJSON_AddNumberToObject(schema, RESERVED_INDEX, VALUE);
			} else throw MSG_Error("%s can be in {%s, %s, %s}", RESERVED_INDEX, str_index[0].c_str(), str_index[1].c_str(), str_index[2].c_str());
		} else throw MSG_Error("Data inconsistency, %s should be string", RESERVED_INDEX);
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_ANALYZER))) {
		spc_now.analyzer.clear();
		unique_cJSON acc_s(cJSON_CreateArray(), cJSON_Delete);
		if (spc->type == cJSON_String) {
			if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0) {
				spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_SOME));
			} else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) {
				spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_NONE));
			} else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) {
				spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL));
			} else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) {
				spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
				cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL_Z));
			} else throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
		} else if (spc->type == cJSON_Array) {
			int elements = cJSON_GetArraySize(spc);
			for (int i = 0; i < elements; i++) {
				cJSON *analyzer = cJSON_GetArrayItem(spc, i);
				if (spc->type == cJSON_String) {
					std::string _analyzer = stringtoupper(analyzer->valuestring);
					if (strcasecmp(spc->valuestring, str_analizer[0].c_str()) == 0) {
						spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_SOME);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_SOME));
					} else if (strcasecmp(spc->valuestring, str_analizer[1].c_str()) == 0) {
						spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_NONE);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_NONE));
					} else if (strcasecmp(spc->valuestring, str_analizer[2].c_str()) == 0) {
						spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL));
					} else if (strcasecmp(spc->valuestring, str_analizer[3].c_str()) == 0) {
						spc_now.analyzer.push_back(Xapian::TermGenerator::STEM_ALL_Z);
						cJSON_AddItemToArray(acc_s.get(), cJSON_CreateNumber(Xapian::TermGenerator::STEM_ALL_Z));
					} else throw MSG_Error("%s can be  {%s, %s, %s, %s}", RESERVED_ANALYZER, str_analizer[0].c_str(), str_analizer[1].c_str(), str_analizer[2].c_str(), str_analizer[3].c_str());
				} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
			}
		} else throw MSG_Error("Data inconsistency, %s should be string or array of strings", RESERVED_ANALYZER);
		cJSON_AddItemToObject(schema, RESERVED_ANALYZER, acc_s.release());
	}

	if ((spc = cJSON_GetObjectItem(item, RESERVED_DYNAMIC))) {
		if (spc->type == cJSON_False) {
			cJSON_AddFalseToObject(schema, RESERVED_DYNAMIC);
			spc_now.dynamic = false;
		} else if (spc->type == cJSON_True) {
			cJSON_AddTrueToObject(schema, RESERVED_DYNAMIC);
			spc_now.dynamic = true;
		} else throw MSG_Error("Data inconsistency, %s should be boolean", RESERVED_DYNAMIC);
	}

	if (!root) insert_inheritable_specifications(item, spc_now, schema);
}


bool is_language(const std::string &language)
{
	if (language.find(" ") != std::string::npos) {
		return false;
	}
	return (std::string(DB_LANGUAGES).find(language) != std::string::npos) ? true : false;
}


char get_type(cJSON *_field, specifications_t &spc)
{
	if (_field->type == cJSON_Object) throw MSG_Error("%s can not be an object", RESERVED_VALUE);

	cJSON *field;
	int type = _field->type;
	if (type == cJSON_Array) {
		int num_ele = cJSON_GetArraySize(_field);
		field = cJSON_GetArrayItem(_field, 0);
		type = field->type;
		if (type == cJSON_Array) throw MSG_Error("It can not be indexed array of arrays");
		for (int i = 1; i < num_ele; i++) {
			field = cJSON_GetArrayItem(_field, i);
			if (field->type != type && (field->type > 1 || type > 1)) throw MSG_Error("Different types of data");
		}
		spc.sep_types[1] = ARRAY_TYPE;
	} else field = _field;

	switch (type) {
		case cJSON_Number: if (spc.numeric_detection) return NUMERIC_TYPE; break;
		case cJSON_False:
		case cJSON_True:   if (spc.bool_detection) return BOOLEAN_TYPE; break;
		case cJSON_String:
			if (spc.bool_detection && !Serialise::boolean(field->valuestring).empty()) {
				return BOOLEAN_TYPE;
			} else if (spc.date_detection && Datetime::isDate(field->valuestring)) {
				return DATE_TYPE;
			} else if(spc.geo_detection && EWKT_Parser::isEWKT(field->valuestring)) {
				return GEO_TYPE;
			} else if (spc.string_detection) {
				return STRING_TYPE;
			}
			break;
	}

	unique_char_ptr _cprint(cJSON_Print(_field));
	throw MSG_Error("%s: %s is ambiguous", RESERVED_VALUE, _cprint.get());
}


bool set_types(const std::string &type, char sep_types[])
{
	unique_group unique_gr;
	int len = (int)type.size();
	int ret = pcre_search(type.c_str(), len, 0, 0, FIND_TYPES_RE, &compiled_find_types_re, unique_gr);
	group_t *gr = unique_gr.get();
	if (ret != -1 && len == gr[0].end - gr[0].start) {
		if (gr[4].end - gr[4].start != 0) {
			sep_types[0] = OBJECT_TYPE;
			sep_types[1] = NO_TYPE;
			sep_types[2] = NO_TYPE;
		} else {
			if (gr[1].end - gr[1].start != 0) {
				sep_types[0] = OBJECT_TYPE;
			}
			if (gr[2].end - gr[2].start != 0) {
				sep_types[1] = ARRAY_TYPE;
			}
			sep_types[2] = std::string(type.c_str(), gr[3].start, gr[3].end - gr[3].start).at(0);
		}

		return true;
	}

	return false;
}


std::string str_type(const char sep_types[]) {
	std::stringstream str;
	if (sep_types[0] == OBJECT_TYPE) str << "object/";
	if (sep_types[1] == ARRAY_TYPE) str << "array/";
	str << Serialise::type(sep_types[2]);
	return str.str();
}


std::vector<std::string> split_fields(const std::string &field_name)
{
	std::vector<std::string> fields;
	std::string aux(field_name.c_str());
	std::string::size_type pos = 0;
	while (aux.at(pos) == DB_OFFSPRING_UNION[0]) {
		pos++;
	}
	std::string::size_type start = pos;
	while ((pos = aux.substr(start, aux.size()).find(DB_OFFSPRING_UNION)) != -1) {
		std::string token = aux.substr(0, start + pos);
		fields.push_back(token);
		aux.assign(aux, start + pos + strlen(DB_OFFSPRING_UNION), aux.size());
		pos = 0;
		while (aux.at(pos) == DB_OFFSPRING_UNION[0]) {
			pos++;
		}
		start = pos;
	}
	fields.push_back(aux);
	return fields;
}


void clean_reserved(cJSON *root)
{
	int elements = cJSON_GetArraySize(root);
	for (int i = 0; i < elements; ) {
		cJSON *item = cJSON_GetArrayItem(root, i);
		if (is_reserved(item->string)) {
			cJSON_DeleteItemFromObject(root, item->string);
		} else {
			clean_reserved(root, item);
		}
		if (elements > cJSON_GetArraySize(root)) {
			elements = cJSON_GetArraySize(root);
		} else {
			i++;
		}
	}
}


void clean_reserved(cJSON *root, cJSON *item)
{
	if (is_reserved(item->string) && strcmp(item->string, RESERVED_VALUE) != 0) {
		cJSON_DeleteItemFromObject(root, item->string);
		return;
	}

	if (item->type == cJSON_Object) {
		int elements = cJSON_GetArraySize(item);
		for (int i = 0; i < elements; ) {
			cJSON *subitem = cJSON_GetArrayItem(item, i);
			clean_reserved(item, subitem);
			if (elements > cJSON_GetArraySize(item)) {
				elements = cJSON_GetArraySize(item);
			} else {
				i++;
			}
		}
	}
}


std::string specificationstostr(const specifications_t &spc)
{
	std::stringstream str;
	str << "\n{\n";
	str << "\t" << RESERVED_POSITION << ": [ ";
	for (int i = 0; i < spc.position.size(); i++)
		str << spc.position[i] << " ";
	str << "]\n";
	str << "\t" << RESERVED_WEIGHT   << ": [ ";
	for (int i = 0; i < spc.weight.size(); i++)
		str << spc.weight[i] << " ";
	str << "]\n";
	str << "\t" << RESERVED_LANGUAGE << ": [ ";
	for (int i = 0; i < spc.language.size(); i++)
		str << spc.language[i] << " ";
	str << "]\n";
	str << "\t" << RESERVED_ACCURACY << ": [ ";
	if (spc.sep_types[2] == DATE_TYPE) {
		for (int i = 0; i < spc.accuracy.size(); i++) {
			str << str_time[spc.accuracy[i]] << " ";
		}
	} else {
		for (int i = 0; i < spc.accuracy.size(); i++) {
			str << spc.accuracy[i] << " ";
		}
	}
	str << "]\n";
	str << "\t" << RESERVED_ACC_PREFIX  << ": [ ";
	for (int i = 0; i < spc.acc_prefix.size(); i++)
		str << spc.acc_prefix[i] << " ";
	str << "]\n";
	str << "\t" << RESERVED_ANALYZER    << ": [ ";
	for (int i = 0; i < spc.analyzer.size(); i++)
		str << str_analizer[spc.analyzer[i]] << " ";
	str << "]\n";
	str << "\t" << RESERVED_SPELLING    << ": [ ";
	for (int i = 0; i < spc.spelling.size(); i++)
		str << (spc.spelling[i] ? "true " : "false ");
	str << "]\n";
	str << "\t" << RESERVED_POSITIONS   << ": [ ";
	for (int i = 0; i < spc.positions.size(); i++)
		str << (spc.positions[i] ? "true " : "false ");
	str << "]\n";
	str << "\t" << RESERVED_TYPE        << ": " << str_type(spc.sep_types) << "\n";
	str << "\t" << RESERVED_INDEX       << ": " << str_index[spc.index] << "\n";
	str << "\t" << RESERVED_STORE       << ": " << ((spc.store)             ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DYNAMIC     << ": " << ((spc.dynamic)           ? "true" : "false") << "\n";
	str << "\t" << RESERVED_D_DETECTION << ": " << ((spc.date_detection)    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_N_DETECTION << ": " << ((spc.numeric_detection) ? "true" : "false") << "\n";
	str << "\t" << RESERVED_G_DETECTION << ": " << ((spc.geo_detection)     ? "true" : "false") << "\n";
	str << "\t" << RESERVED_B_DETECTION << ": " << ((spc.bool_detection)    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_S_DETECTION << ": " << ((spc.string_detection)  ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_TERM   << ": " << ((spc.bool_term)         ? "true" : "false") << "\n}\n";

	return str.str();
}


void readable_schema(cJSON *schema) {
	cJSON *_schema = cJSON_GetObjectItem(schema, RESERVED_SCHEMA);
	int elements = cJSON_GetArraySize(_schema);
	for (int i = 0; i < elements; i++) {
		cJSON *field = cJSON_GetArrayItem(_schema, i);
		if (!is_reserved(field->string) || strcmp(field->string, RESERVED_ID) == 0)
			readable_field(field);
	}
}


void readable_field(cJSON *field) {
	int _size = cJSON_GetArraySize(field);
	for (int i = 0; i < _size; i++) {
		cJSON *item = cJSON_GetArrayItem(field, i);
		if (!is_reserved(item->string)) readable_schema(field);
		else if (strcmp(item->string, RESERVED_TYPE) == 0) {
			char sep_types[3] = {(char)(cJSON_GetArrayItem(item, 0)->valueint), (char)(cJSON_GetArrayItem(item, 1)->valueint), (char)(cJSON_GetArrayItem(item, 2)->valueint)};
			cJSON_ReplaceItemInObject(field, RESERVED_TYPE, cJSON_CreateString(str_type(sep_types).c_str()));
			item = cJSON_GetObjectItem(field, RESERVED_ACCURACY);
			if (item && sep_types[2] == DATE_TYPE) {
				int _size = cJSON_GetArraySize(item);
				for (int i = 0; i < _size; i++)
					cJSON_ReplaceItemInArray(item, i, cJSON_CreateString(str_time[(cJSON_GetArrayItem(item, i)->valueint)].c_str()));
			} else if (item && sep_types[2] == GEO_TYPE)
				cJSON_ReplaceItemInArray(item, 0, cJSON_GetArrayItem(item, 0)->valueint ? cJSON_CreateTrue() : cJSON_CreateFalse());
		} else if ((item = cJSON_GetObjectItem(field, RESERVED_ANALYZER))) {
			int _size = cJSON_GetArraySize(item);
			for (int i = 0; i < _size; i++)
				cJSON_ReplaceItemInArray(item, i, cJSON_CreateString(str_analizer[cJSON_GetArrayItem(item, i)->valueint].c_str()));
		} else if ((item = cJSON_GetObjectItem(field, RESERVED_INDEX)))
			cJSON_ReplaceItemInObject(field, RESERVED_INDEX, cJSON_CreateString(str_index[item->valueint].c_str()));
	}
}