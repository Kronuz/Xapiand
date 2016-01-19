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

#include "msgpack_patcher.h"

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

				if      (op_str.compare(PATCH_ADD) == 0) { if (!patch_add(elem, object))     return false; }
				else if (op_str.compare(PATCH_REM) == 0) { if (!patch_remove(elem, object))  return false; }
				else if (op_str.compare(PATCH_REP) == 0) { if (!patch_replace(elem, object)) return false; }
				else if (op_str.compare(PATCH_MOV) == 0) { if (!patch_move(elem, object))    return false; }
				else if (op_str.compare(PATCH_COP) == 0) { if (!patch_copy(elem, object))    return false; }
				else if (op_str.compare(PATCH_TES) == 0) { if (!patch_test(elem, object))    return false; }
			} catch (const std::out_of_range&) {
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
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		std::string target = path_split.back();
		path_split.pop_back();
		MsgPack o = object.path(path_split);
		MsgPack val = get_patch_value(obj_patch);
		_add(o, val, target);
		return true;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "ERROR: In patch add: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "ERROR: In patch add: %s", e.what());
		return false;
	}
}


bool patch_remove(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		MsgPack o = object.path(path_split);
		_erase(o.parent(), path_split.back());
		return true;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "ERROR: In patch remove: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "ERROR: In patch remove: %s", e.what());
		return false;
	}
}


bool patch_replace(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		MsgPack o = object.path(path_split);
		MsgPack val = get_patch_value(obj_patch);
		o = val;
		return true;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "ERROR: In patch replace: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "ERROR: In patch replace: %s", e.what());
		return false;
	}
}


bool patch_move(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		std::vector<std::string> from_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		_tokenizer(obj_patch, from_split, PATCH_FROM);
		std::string target = path_split.back();
		path_split.pop_back();
		MsgPack to = object.path(path_split);
		MsgPack from = object.path(from_split);
		_add(to, from, target);
		_erase(from.parent(), from_split.back());
		return true;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "ERROR: In patch move: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "ERROR: In patch move: %s", e.what());
		return false;
	}
}


bool patch_copy(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		std::vector<std::string> from_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		_tokenizer(obj_patch, from_split, PATCH_FROM);
		std::string target = path_split.back();
		path_split.pop_back();
		MsgPack to = object.path(path_split);
		MsgPack from = object.path(from_split);
		_add(to, from, target);
		return true;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "ERROR: In patch copy: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "ERROR: In patch copy: %s", e.what());
		return false;
	}
}


bool patch_test(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		MsgPack o = object.path(path_split);
		MsgPack val = get_patch_value(obj_patch);
		return val == o;
	} catch (const std::exception& e) {
		L_ERR(nullptr, "ERROR: In patch test: %s", e.what());
		return false;
	} catch (const msgpack::type_error& e){
		L_ERR(nullptr, "ERROR: In patch test: %s", e.what());
		return false;
	}
}


MsgPack get_patch_value(const MsgPack& obj_patch) {
	try {
		return obj_patch.at("value");
	} catch (const std::out_of_range&) {
		throw MSG_Error("Object MUST have exactly one \"value\" member in \"add\" operation");
	}
}
