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

#include "msgpack_wrapper_patch.h"

#include "exception.h"
#include "log.h"
#include "utils.h"


#define PATCH_ADD "add"
#define PATCH_REM "remove"
#define PATCH_REP "replace"
#define PATCH_MOV "move"
#define PATCH_COP "copy"
#define PATCH_TES "test"

#define PATCH_PATH "path"
#define PATCH_FROM "from"


bool apply_patch(MsgPack& patch, MsgPack& object) {
	if (patch.obj->type == msgpack::type::ARRAY) {
		for (auto elem : patch) {
			try {
				MsgPack op = elem.at("op");
				std::string op_tmp = op.to_json_string();
				std::string op_str = std::string(op_tmp, 1, op_tmp.size() - 2);

				if      (op_str.compare(PATCH_ADD) == 0) { if (!patch_add(elem, object)) return false; }
				else if (op_str.compare(PATCH_REM) == 0) { if (!patch_remove(elem, object)) return false; }
				else if (op_str.compare(PATCH_REP) == 0) { if (!patch_replace(elem, object)) return false; }
				else if (op_str.compare(PATCH_MOV) == 0) { if (!patch_move(elem, object)) return false; }
				else if (op_str.compare(PATCH_COP) == 0) { if (!patch_copy(elem, object)) return false; }
				else if (op_str.compare(PATCH_TES) == 0) { if (!patch_test(elem, object)) return false; }
			} catch (const std::out_of_range& err) {
				L_ERR(nullptr, "ERROR: Objects MUST have exactly one \"op\" member");
				return false;
			}
		}
		return true;
	} else {
		L_ERR(nullptr, "ERROR: A JSON Patch document MUST be an array of objects");
		return false;
	}
}


bool patch_add(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;
	try {
		MsgPack path = obj_patch.at(PATCH_PATH);
		std::string path_str = path.to_json_string();
		path_str = path_str.substr(1, path_str.size()-2);
		std::vector<std::string> path_split;
		stringTokenizer(path_str, "\\/", path_split);
		std::string target = path_split.back();
		path_split.pop_back();

		MsgPack o = object.path(path_split);
		MsgPack val = get_patch_value(obj_patch);

		if (o.obj->type == msgpack::type::MAP) {
			o[target] = val;
		} else if (o.obj->type == msgpack::type::ARRAY) {
			int offset = strict_stoi(target);
			o.insert_item_to_array(offset, val);
		}
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch add: %s", e.what());
		return false;
	} catch (const msgpack::type_error e){
		L_ERR(nullptr, "Error in patch add: %s", e.what());
		return false;
	}
	return true;
}


bool patch_remove(const MsgPack& obj_patch, MsgPack& object) {
	try {
		MsgPack path = obj_patch.at(PATCH_PATH);
		std::string path_str = path.to_json_string();
		path_str = path_str.substr(1, path_str.size()-2);
		std::vector<std::string> path_split;
		stringTokenizer(path_str, "\\/", path_split);
		MsgPack o = object.path(path_split);
		MsgPack parent = o.parent();

		if (parent.obj->type == msgpack::type::MAP) {
			parent.erase(path_split.back());
		} else if (parent.obj->type == msgpack::type::ARRAY) {
			parent.erase(strict_stoi(path_split.back()));
		}
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch remove: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "Error in patch remove: %s", e.what());
		return false;
	}
	return true;
}


bool patch_replace(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;

	try {
		MsgPack path = obj_patch.at(PATCH_PATH);
		std::string path_str = path.to_json_string();
		path_str = path_str.substr(1, path_str.size()-2);
		std::vector<std::string> path_split;
		stringTokenizer(path_str, "\\/", path_split);
		MsgPack o = object.path(path_split);
		MsgPack val = get_patch_value(obj_patch);
		o = val;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch replace: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "Error in patch replace: %s", e.what());
		return false;
	}
	return true;
}


bool patch_move(const MsgPack& obj_patch, MsgPack& object) {
	try {
		MsgPack old_path = obj_patch.at(PATCH_PATH);
		MsgPack new_path = obj_patch.at(PATCH_FROM);

		std::string path_str = old_path.to_json_string();
		std::string from_str = new_path.to_json_string();
		path_str = path_str.substr(1, path_str.size()-2);
		from_str = from_str.substr(1, from_str.size()-2);
		std::vector<std::string> path_split;
		std::vector<std::string> from_split;
		stringTokenizer(path_str, "\\/", path_split);
		stringTokenizer(from_str, "\\/", from_split);

		MsgPack to = object.path(path_split);
		MsgPack from = object.path(from_split);
		to = from;
		from.erase(from_split.back());
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch move: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "Error in patch move: %s", e.what());
		return false;
	}
	return true;
}


bool patch_copy(const MsgPack& obj_patch, MsgPack& object) {
	try {
		MsgPack old_path = obj_patch.at(PATCH_PATH);
		MsgPack new_path = obj_patch.at(PATCH_FROM);

		std::string path_str = old_path.to_json_string();
		std::string from_str = new_path.to_json_string();
		path_str = path_str.substr(1, path_str.size()-2);
		from_str = from_str.substr(1, from_str.size()-2);
		std::vector<std::string> path_split;
		std::vector<std::string> from_split;
		stringTokenizer(path_str, "\\/", path_split);
		stringTokenizer(from_str, "\\/", from_split);

		MsgPack to = object.path(path_split);
		MsgPack from = object.path(from_split);
		to = from;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch copy: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "Error in patch copy: %s", e.what());
		return false;
	}
	return true;
}


bool patch_test(const MsgPack& obj_patch, MsgPack& object) {
	std::string target;
	try {
		MsgPack o = obj_patch.at(PATCH_PATH);
		MsgPack val = get_patch_value(obj_patch);
		if (val == o) {
			return true;
		} else {
			return false;
		}
	} catch (const std::exception& e) {
		L_ERR(nullptr, "Error in patch test: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "Error in patch test: %s", e.what());
		return false;
	}
	return true;
}


MsgPack get_patch_value(const MsgPack& obj_patch) {
	try {
		MsgPack value = obj_patch.at("value");
		return value;
	} catch (const std::out_of_range&) {
		throw MSG_Error("Object MUST have exactly one \"value\" member in \"add\" operation");
	}
}
