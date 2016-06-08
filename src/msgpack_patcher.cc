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
#define PATCH_INC "incr"
#define PATCH_DEC "decr"

#define PATCH_PATH "path"
#define PATCH_FROM "from"


void apply_patch(const MsgPack& patch, MsgPack& object) {
	if (patch.is_array()) {
		for (const auto& elem : patch) {
			try {
				const auto& op = elem.at("op");
				auto op_str = op.as_string();

				if      (op_str.compare(PATCH_ADD) == 0) { patch_add(elem, object);          }
				else if (op_str.compare(PATCH_REM) == 0) { patch_remove(elem, object);       }
				else if (op_str.compare(PATCH_REP) == 0) { patch_replace(elem, object);      }
				else if (op_str.compare(PATCH_MOV) == 0) { patch_move(elem, object);         }
				else if (op_str.compare(PATCH_COP) == 0) { patch_copy(elem, object);         }
				else if (op_str.compare(PATCH_TES) == 0) { patch_test(elem, object);         }
				else if (op_str.compare(PATCH_INC) == 0) { patch_incr_decr(elem, object);    }
				else if (op_str.compare(PATCH_DEC) == 0) { patch_incr_decr(elem, object, 1); }
			} catch (const std::out_of_range&) {
				throw MSG_ClientError("Objects MUST have exactly one \"op\" member");
			}
		}
	} else {
		throw MSG_ClientError("A JSON Patch document MUST be an array of objects");
	}
}


void patch_add(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		if (path_split.size() != 0) {
			auto target = path_split.back();
			path_split.pop_back();
			auto& o = object.path(path_split);
			const auto& val = get_patch_value(obj_patch);
			_add(o, val, target);
		} else {
			throw MSG_ClientError("Is not allowed path:\"\" ");
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch add: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch add: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch add: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch add: %s", exc.what());
	}
}


void patch_remove(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		if (path_split.size() != 0) {
			std::string target = path_split.back();
			path_split.pop_back();
			auto& o = object.path(path_split);
			try {
				if (o.type() == msgpack::type::MAP) {
					o.at(target);
				} else if  (o.type() == msgpack::type::ARRAY) {
					o.at(strict(std::stoi, target));
				}
			} catch(const std::out_of_range& exc) {
				throw MSG_ClientError("target %s not found", target.c_str());
			}
			_erase(o, target);
		} else {
			throw MSG_ClientError("Is not allowed path:\"\" ");
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch remove: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch remove: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch remove: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch remove: %s", exc.what());
	}
}


void patch_replace(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		auto& o = object.path(path_split);
		const auto& val = get_patch_value(obj_patch);
		o = val;
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch replace: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch replace: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch replace: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch replace: %s", exc.what());
	}
}


void patch_move(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		std::vector<std::string> from_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		_tokenizer(obj_patch, from_split, PATCH_FROM);
		if (path_split.size() != 0 and from_split.size() != 0) {
			auto target_path = path_split.back();
			path_split.pop_back();
			auto& to = object.path(path_split);
			auto& from = object.path(from_split);
			_add(to, from, target_path);

			auto target_from = from_split.back();
			from_split.pop_back();
			auto& from_parent = object.path(from_split);
			_erase(from_parent, target_from);
		} else {
			throw MSG_ClientError("Is not allowed path:\"\" or from:\"\"");
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch move: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch move: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch move: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch move: %s", exc.what());
	}
}


void patch_copy(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		if (path_split.size() != 0) {
			std::vector<std::string> from_split;
			_tokenizer(obj_patch, from_split, PATCH_FROM);
			if (path_split.size() != 0) {
				auto target = path_split.back();
				path_split.pop_back();
				auto& to = object.path(path_split);
				const auto& from = object.path(from_split);
				_add(to, from, target);
			}
		} else {
			throw MSG_ClientError("Is not allowed path:\"\" ");
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch copy: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch copy: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch copy: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch copy: %s", exc.what());
	}
}


void patch_test(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		const auto& o = object.path(path_split);
		const auto& val = get_patch_value(obj_patch);
		if (val != o) {
			throw MSG_ClientError("In patch test: Objects are not equals. Expected: %s Result: %s", val.to_string().c_str(), o.to_string().c_str());
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch test: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch test: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch test: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch test: %s", exc.what());
	}
}


void patch_incr_decr(const MsgPack& obj_patch, MsgPack& object, bool decr) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH);
		auto& o = object.path(path_split);
		const auto& val = get_patch_value(obj_patch);
		int val_num, limit;
		if (val.is_string()) {
			val_num = strict(std::stoi, val.as_string());
		} else if (val.type() == msgpack::type::NEGATIVE_INTEGER) {
			val_num = static_cast<int>(val.as_i64());
		} else {
			throw  MSG_ClientError("\"value\" must be string or integer");
		}
		if (get_patch_custom_limit(limit, obj_patch)) {
			_incr_decr(o, decr ? -val_num : val_num, limit);
		} else {
			_incr_decr(o, decr ? -val_num : val_num, val_num);
		}
	} catch (const LimitError& exc){
		throw MSG_ClientError("In patch increment: %s", exc.what());
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("In patch increment: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		throw MSG_ClientError("In patch increment: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		throw MSG_ClientError("In patch increment: %s", exc.what());
	} catch (const std::exception& exc) {
		throw MSG_Error("In patch increment: %s", exc.what());
	}
}


const MsgPack& get_patch_value(const MsgPack& obj_patch) {
	try {
		return obj_patch.at("value");
	} catch (const std::out_of_range&) {
		throw MSG_ClientError("Object MUST have exactly one \"value\" member for this operation");
	}
}


bool get_patch_custom_limit(int& limit, const MsgPack& obj_patch) {
	try {
		const auto& o = obj_patch.at("limit");
		if (o.is_string()) {
			limit = strict(std::stoi, o.as_string());
			return true;
		} else if (o.type() == msgpack::type::NEGATIVE_INTEGER) {
			limit = static_cast<int>(o.as_i64());
			return true;
		} else {
			throw MSG_ClientError("\"limit\" must be string or integer");
		}
	} catch (const std::out_of_range&) {
		return false;
	}
}
