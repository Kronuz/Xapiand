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

#pragma once

#include "exception.h"
#include "msgpack.h"
#include "utils.h"

#include "rapidjson/pointer.h"


/* Support for RFC 6902 */

void apply_patch(const MsgPack& patch, MsgPack& object);
void patch_add(const MsgPack& obj_patch, MsgPack& object);
void patch_remove(const MsgPack& obj_patch, MsgPack& object);
void patch_replace(const MsgPack& obj_patch, MsgPack& object);
void patch_move(const MsgPack& obj_patch, MsgPack& object);
void patch_copy(const MsgPack& obj_patch, MsgPack& object);
void patch_test(const MsgPack& obj_patch, MsgPack& object);
void patch_incr(const MsgPack& obj_patch, MsgPack& object);
void patch_incr_decr(const MsgPack& obj_patch, MsgPack& object, bool decr=false);
const MsgPack& get_patch_value(const MsgPack& obj_patch);
bool get_patch_custom_limit(int& limit, const MsgPack& obj_patch);


inline void _add(MsgPack& o, const MsgPack& val, const std::string& target) {
	if (o.is_map()) {
		o[target] = val;
	} else if (o.is_array()) {
		if (target.compare("-") == 0) {
			o.push_back(val);
		} else {
			int offset = strict(std::stoi, target);
			o.insert(static_cast<size_t>(offset), val);
		}
	} else {
		throw MSG_ClientError("Object is not array or map");
	}
}


inline void _erase(MsgPack& o, const std::string& target) {
	try {
		if (o.is_array()) {
			o.erase(strict(std::stoi, target));
		} else {
			o.erase(target);
		}
	} catch (const msgpack::type_error&) {
		throw MSG_ClientError("Object is not array or map");
	}
}


inline void _incr_decr(MsgPack& o, int val) {
	if (o.type() == msgpack::type::NEGATIVE_INTEGER) {
		o += val;
	} else {
		throw MSG_ClientError("Object is not integer");
	}
}


inline void _incr_decr(MsgPack& o, int val, int limit) {
	if (o.type() == msgpack::type::NEGATIVE_INTEGER) {
		o += val;
		if (val < 0) {
			if (static_cast<int>(o.as_i64()) <= limit) {
				throw MSG_LimitError("Limit exceeded");
			}
		} else if (static_cast<int>(o.as_i64()) >= limit) {
			throw MSG_LimitError("Limit exceeded");
		}
	} else {
		throw MSG_ClientError("Object is not integer");
	}
}


//Support for RFC 6901
inline void _tokenizer(const MsgPack& obj, std::vector<std::string>& path_split, const char* path_c) {
	try {
		const auto& path = obj.at(path_c);
		std::string path_str = path.unformatted_string();
		rapidjson::GenericPointer<rapidjson::GenericValue<rapidjson::UTF8<> > > json_pointer(path_str.data(), path_str.size());
		size_t n_tok = json_pointer.GetTokenCount();

		for (int i=0; i<static_cast<int>(n_tok); i++) {
			auto& t = json_pointer.GetTokens()[i];
			path_split.push_back(std::string(t.name, t.length));
		}

		if (path_split.size() == 0 and path_str != "") {
			throw MSG_ClientError("Bad syntax of a JSON Pointer in path:%s %s", path_str.c_str(), startswith(path_str, "/") ? "" : "perhaps forgot prefixed '/'");
		}

	} catch (const std::out_of_range&) {
		throw MSG_ClientError("Object MUST have exactly one \"path\" member for this operation");
	}
}
