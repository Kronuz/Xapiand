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

#include "rapidjson/error/en.h"

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <random>



#define PATCH_ADD "add"
#define PATCH_REM "remove"
#define PATCH_REP "replace"
#define PATCH_MOV "move"
#define PATCH_COP "copy"
#define PATCH_TES "test"

#define PATCH_PATH "path"
#define PATCH_FROM "from"


const std::regex find_types_re("(" OBJECT_STR "/)?(" ARRAY_STR "/)?(" DATE_STR "|" NUMERIC_STR "|" GEO_STR "|" BOOLEAN_STR "|" STRING_STR ")|(" OBJECT_STR ")", std::regex::icase | std::regex::optimize);


long long save_mastery(const std::string& dir) {
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


long long read_mastery(const std::string& dir, bool force) {
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


bool is_reserved(const std::string& word) {
	return word.at(0) == '_' ? true : false;
}


bool is_language(const std::string& language) {
	if (language.find(" ") != std::string::npos) {
		return false;
	}
	return std::string(DB_LANGUAGES).find(language) != std::string::npos ? true : false;
}


bool set_types(const std::string& type, std::vector<char>& sep_types) {
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


std::string str_type(const std::vector<char>& sep_types) {
	std::stringstream str;
	if (sep_types[0] == OBJECT_TYPE) str << OBJECT_STR << "/";
	if (sep_types[1] == ARRAY_TYPE) str << ARRAY_STR << "/";
	str << Serialise::type(sep_types[2]);
	return str.str();
}


void clean_reserved(MsgPack& document) {
	if (document.obj->type == msgpack::type::MAP) {
		for (auto item_key : document) {
			std::string str_key(item_key.obj->via.str.ptr, item_key.obj->via.str.size);
			if (is_reserved(str_key) && str_key != RESERVED_VALUE) {
				document.erase(str_key);
			} else {
				auto item_doc = document.at(str_key);
				clean_reserved(item_doc);
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
		throw MSG_Error("JSON parse error: %s (%u)\n", GetParseError_En(parse_done.Code()), parse_done.Offset());
	}
}


void apply_patch(const MsgPack& patch, MsgPack& object) {
	if (patch.obj->type == msgpack::type::ARRAY) {
		for (const auto elem : patch) {
			try {
				MsgPack op = elem.at("op");
				std::string op_str = op.to_json_string();

				if      (op_str.compare(PATCH_ADD) == 0) patch_add(elem, object);
				else if (op_str.compare(PATCH_REM) == 0) patch_remove(elem, object);
				else if (op_str.compare(PATCH_REP) == 0) patch_replace(elem, object);
				else if (op_str.compare(PATCH_MOV) == 0) patch_move(elem, object);
				else if (op_str.compare(PATCH_COP) == 0) patch_copy(elem, object);
				else if (op_str.compare(PATCH_TES) == 0) patch_test(elem, object);
			} catch (const std::out_of_range& err) {
				throw MSG_Error("Objects MUST have exactly one \"op\" member");
			}
		}
	}

	throw msgpack::type_error();
}


bool patch_add(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;
	try {
		MsgPack o = get_patch_path(obj_patch, object, PATCH_PATH, target);
		MsgPack val = get_patch_value(obj_patch);
		o[target] = val;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch add: %s", e.what());
		return false;
	}
	return true;
}


bool patch_remove(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;
	try {
		MsgPack o = get_patch_path(obj_patch, object, PATCH_PATH, target);
		o.erase(target);
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch remove: %s", e.what());
		return false;
	} catch (msgpack::type_error& e){
		L_ERR(nullptr, "Error in patch remove: %s", e.what());
		return false;
	}
	return true;
}


bool patch_replace(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;

	try {
		MsgPack o = get_patch_path(obj_patch, object, PATCH_PATH, target);
		MsgPack val = get_patch_value(obj_patch);
		o[target] = val;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch replace: %s", e.what());
		return false;
	}
	return true;
}


bool patch_move(const MsgPack& obj_patch, MsgPack& object) {
	std::string old_target;
	std::string new_target;
	try {
		MsgPack path = get_patch_path(obj_patch, object, PATCH_PATH, new_target);
		MsgPack from = get_patch_path(obj_patch, object, PATCH_FROM, old_target, true);
		MsgPack val = from[old_target];
		from.erase(old_target);
		path[new_target] = val;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch move: %s", e.what());
		return false;
	}
	return true;
}


bool patch_copy(const MsgPack& obj_patch, MsgPack& object) {
	std::string old_target;
	std::string new_target;
	try {
		MsgPack path = get_patch_path(obj_patch, object, PATCH_PATH, new_target);
		MsgPack from = get_patch_path(obj_patch, object, PATCH_FROM, old_target, true);
		MsgPack val = from[old_target];
		path[new_target] = val;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch copy: %s", e.what());
		return false;
	}
	return true;
}


bool patch_test(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;
	try {
		MsgPack o = get_patch_path(obj_patch, object, PATCH_PATH, target);
		MsgPack val = get_patch_value(obj_patch);
		MsgPack o_val = o[target];
		if (val == o_val) {
			return true;
		} else {
			return false;
		}
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch test: %s", e.what());
		return false;
	}
	return true;
}


MsgPack get_patch_path(const MsgPack& obj_patch, MsgPack& object, const char* path, std::string& target, bool verify_exist) {
	try {
		MsgPack path = obj_patch.at(path);
		std::string path_str = path.to_json_string();
		path_str = path_str.substr(1, path_str.size()-2);

		std::vector<std::string> path_split;
		stringTokenizer(path_str, "\\/", path_split);

		bool is_target = false;
		int ct = 0;

		MsgPack o = object;
		for(auto s: path_split) {
			ct++;
			if (ct == path_split.size()) {
				is_target = true;
				target = s;

				if (!verify_exist) {
					return o;
				}
			}

			try {
				if (is_target && verify_exist) {
					o.at(s);
				} else {
					o = o.at(s);
				}
			} catch (std::out_of_range err) {
				try {
					//If the "-" character is used to index the end of the array
					int offset = strict_stoi(s);
					o = o.at(offset);
				} catch (const std::invalid_argument& e) {
					std::string err_msg("The object itself or an array containing it does need to exist in: ");
					err_msg.append(obj_patch.at("path").to_json_string());
					throw MSG_Error(err_msg.c_str());
				} catch (const std::out_of_range& e) {
					std::string err_msg("The index MUST NOT be greater than the array size in: ");
					err_msg.append(obj_patch.at("path").to_json_string());
					throw MSG_Error(err_msg.c_str());
				}
			}
		}

		return o;

	} catch (const std::out_of_range& err) {
		throw MSG_Error("Object MUST have exactly one \"path\" member");
	}

}


MsgPack get_patch_value(const MsgPack& obj_patch) {
	try {
		MsgPack value = obj_patch.at("value");
		return value;
	} catch(std::out_of_range err) {
		throw MSG_Error("Object MUST have exactly one \"value\" member in \"add\" operation");
	}
}
