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

#include "log.h"
#include "length.h"
#include "datetime.h"
#include "wkt_parser.h"
#include "serialise.h"
#include "io_utils.h"

#include "rapidjson/error/en.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <random>

#define DATABASE_DATA_HEADER_MAGIC 0x42
#define DATABASE_DATA_FOOTER_MAGIC 0x2A


const std::regex find_types_re("(" OBJECT_STR "/)?(" ARRAY_STR "/)?(" DATE_STR "|" NUMERIC_STR "|" GEO_STR "|" BOOLEAN_STR "|" STRING_STR ")|(" OBJECT_STR ")", std::regex::icase | std::regex::optimize);


long long save_mastery(const std::string& dir) {
	char buf[20];
	long long mastery_level = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count() << 16;
	mastery_level |= static_cast<int>(random_int(0, 0xffff));
	int fd = io::open((dir + "/mastery").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0600);
	if (fd >= 0) {
		snprintf(buf, sizeof(buf), "%llx", mastery_level);
		io::write(fd, buf, strlen(buf));
		io::close(fd);
	}
	return mastery_level;
}


long long read_mastery(const std::string& dir, bool force) {
	L_DATABASE(nullptr, "+ READING MASTERY OF INDEX '%s'...", dir.c_str());

	struct stat info;
	if (stat(dir.c_str(), &info) || !(info.st_mode & S_IFDIR)) {
		L_DATABASE(nullptr, "- NO MASTERY OF INDEX '%s'", dir.c_str());
		return -1;
	}

	long long mastery_level = -1;

	int fd = io::open((dir + "/mastery").c_str(), O_RDONLY | O_CLOEXEC, 0644);
	if (fd < 0) {
		if (force) {
			mastery_level = save_mastery(dir);
		}
	} else {
		char buf[20];
		mastery_level = 0;
		size_t length = io::read(fd, buf, sizeof(buf) - 1);
		if (length > 0) {
			buf[length] = '\0';
			mastery_level = std::stoll(buf, nullptr, 16);
		}
		io::close(fd);
		if (!mastery_level) {
			mastery_level = save_mastery(dir);
		}
	}

	L_DATABASE(nullptr, "- MASTERY OF INDEX '%s' is %llx", dir.c_str(), mastery_level);

	return mastery_level;
}


bool is_valid(const std::string& word) {
	return word.front() != '_' && word.back() != '_' && word.find(DB_OFFSPRING_UNION) == std::string::npos;
}


bool is_language(const std::string& language) {
	if (language.find(" ") == std::string::npos) {
		return std::string(DB_LANGUAGES).find(language) == std::string::npos ? false : true;
	}
	return false;
}


bool set_types(const std::string& type, std::vector<unsigned>& sep_types) {
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


std::string str_type(const std::vector<unsigned>& sep_types) {
	std::stringstream str;
	if (sep_types[0] == OBJECT_TYPE) str << OBJECT_STR << "/";
	if (sep_types[1] == ARRAY_TYPE) str << ARRAY_STR << "/";
	str << Serialise::type(sep_types[2]);
	return str.str();
}


void clean_reserved(MsgPack& document) {
	if (document.get_type() == msgpack::type::MAP) {
		for (auto item_key : document) {
			std::string str_key(item_key.get_str());
			if (is_valid(str_key) || str_key == RESERVED_VALUE) {
				auto item_doc = document.at(str_key);
				clean_reserved(item_doc);
			} else {
				document.erase(str_key);
			}
		}
	}
}


MIMEType get_mimetype(const std::string& type) {
	if (type == JSON_TYPE) {
		return MIMEType::APPLICATION_JSON;
	} else if (type == FORM_URLENCODED_TYPE) {
		return MIMEType::APPLICATION_XWWW_FORM_URLENCODED;
	} else if (type == MSGPACK_TYPE) {
		return MIMEType::APPLICATION_X_MSGPACK;
	} else {
		return MIMEType::UNKNOW;
	}
}


void json_load(rapidjson::Document& doc, const std::string& str) {
	rapidjson::ParseResult parse_done = doc.Parse(str.data());
	if (!parse_done) {
		throw MSG_ClientError("JSON parse error at position %u: %s", parse_done.Offset(), GetParseError_En(parse_done.Code()));
	}
}


void
set_data(Xapian::Document& doc, const std::string& obj_data_str, const std::string& blob_str)
{
	char h = DATABASE_DATA_HEADER_MAGIC;
	char f = DATABASE_DATA_FOOTER_MAGIC;
	doc.set_data(std::string(&h, 1) + serialise_length(obj_data_str.size()) + obj_data_str + std::string(&f, 1) + blob_str);
}

MsgPack
get_MsgPack(const Xapian::Document& doc)
{
	std::string data = doc.get_data();

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) return MsgPack();
	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return MsgPack();
	}
	if (*(p + length) != DATABASE_DATA_FOOTER_MAGIC) return MsgPack();
	return MsgPack(std::string(p, length));
}


std::string
get_blob(const Xapian::Document& doc)
{
	std::string data = doc.get_data();

	size_t length;
	const char *p = data.data();
	const char *p_end = p + data.size();
	if (*p++ != DATABASE_DATA_HEADER_MAGIC) return data;
	try {
		length = unserialise_length(&p, p_end, true);
	} catch (Xapian::SerialisationError) {
		return data;
	}
	p += length;
	if (*p++ != DATABASE_DATA_FOOTER_MAGIC) return data;
	return std::string(p, p_end - p);
}


std::string to_query_string(std::string str) {
	// '-'' in not accepted by the field processors.
	if (str.at(0) == '-') {
		str[0] = '_';
	}
	return str;
}
