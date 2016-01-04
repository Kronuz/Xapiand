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
#include "log.h"
#include "datetime.h"
#include "wkt_parser.h"
#include "serialise.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <random>


const std::regex find_types_re("(" OBJECT_STR "/)?(" ARRAY_STR "/)?(" DATE_STR "|" NUMERIC_STR "|" GEO_STR "|" BOOLEAN_STR "|" STRING_STR ")|(" OBJECT_STR ")", std::regex::icase | std::regex::optimize);


long long save_mastery(const std::string &dir) {
	char buf[20];
	long long mastery_level = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << 16;
	mastery_level |= static_cast<int>(random_int(0, 0xffff));
	int fd = open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
	if (fd >= 0) {
		snprintf(buf, sizeof(buf), "%llx", mastery_level);
		write(fd, buf, strlen(buf));
		close(fd);
	}
	return mastery_level;
}


long long read_mastery(const std::string &dir, bool force) {
	L_DATABASE(nullptr, "+ READING MASTERY OF INDEX '%s'...", dir.c_str());

	struct stat info;
	if (stat(dir.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
		L_DATABASE(nullptr, "- NO MASTERY OF INDEX '%s'", dir.c_str());
		return -1;
	}

	long long mastery_level = -1;

	int fd = open((dir + "/mastery").c_str(), O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		if (force) {
			mastery_level = save_mastery(dir);
		}
	} else {
		char buf[20];
		mastery_level = 0;
		size_t length = read(fd, buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			mastery_level = std::stoll(buf, nullptr, 16);
		}
		close(fd);
		if (!mastery_level) {
			mastery_level = save_mastery(dir);
		}
	}

	L_DATABASE(nullptr, "- MASTERY OF INDEX '%s' is %llx", dir.c_str(), mastery_level);

	return mastery_level;
}


bool is_reserved(const std::string &word) {
	return word.at(0) == '_' ? true : false;
}


bool is_language(const std::string &language) {
	if (language.find(" ") != std::string::npos) {
		return false;
	}
	return (std::string(DB_LANGUAGES).find(language) != std::string::npos) ? true : false;
}


bool set_types(const std::string &type, std::vector<char> &sep_types) {
	std::smatch m;
	if (std::regex_match(type, m, find_types_re) && static_cast<size_t>(m.length(0)) == type.size()) {
		if (m.length(4) != 0) {
			sep_types[0] = OBJECT_TYPE;
			sep_types[1] = NO_TYPE;
			sep_types[2] = NO_TYPE;
		} else {
			if (m.length(1) != 0) {
				sep_types[0] = OBJECT_TYPE;
			}
			if (m.length(2) != 0) {
				sep_types[1] = ARRAY_TYPE;
			}
			sep_types[2] = m.str(3).at(0);
		}
		return true;
	}

	return false;
}


std::string str_type(const std::vector<char> &sep_types) {
	std::stringstream str;
	if (sep_types[0] == OBJECT_TYPE) str << OBJECT_STR << "/";
	if (sep_types[1] == ARRAY_TYPE) str << ARRAY_STR << "/";
	str << Serialise::type(sep_types[2]);
	return str.str();
}


std::vector<std::string> split_fields(const std::string &field_name) {
	std::vector<std::string> fields;
	std::string aux(field_name.c_str());
	size_t pos = 0;
	while (aux.at(pos) == DB_OFFSPRING_UNION[0]) {
		++pos;
	}
	size_t start = pos;
	while ((pos = aux.substr(start, aux.size()).find(DB_OFFSPRING_UNION)) != std::string::npos) {
		std::string token = aux.substr(0, start + pos);
		fields.push_back(token);
		aux.assign(aux, start + pos + strlen(DB_OFFSPRING_UNION), aux.size());
		pos = 0;
		while (aux.at(pos) == DB_OFFSPRING_UNION[0]) {
			++pos;
		}
		start = pos;
	}
	fields.push_back(aux);
	return fields;
}


void clean_reserved(cJSON *root) {
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
			++i;
		}
	}
}


void clean_reserved(cJSON *root, cJSON *item) {
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
				++i;
			}
		}
	}
}
