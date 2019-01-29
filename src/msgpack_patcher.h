/*
 * Copyright (c) 2015-2018 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include <stddef.h>                         // for size_t
#include <stdexcept>                        // for out_of_range, invalid_arg...
#include <string>                           // for string, basic_string, stoul
#include "string_view.hh"                   // for std::string_view
#include <vector>                           // for vector

#include "exception.h"                      // for ClientError, MSG_ClientError
#include "msgpack.h"                        // for MsgPack, MsgPack::Type, ...
#include "rapidjson/allocators.h"           // for CrtAllocator
#include "rapidjson/document.h"             // for GenericValue
#include "rapidjson/encodings.h"            // for UTF8
#include "rapidjson/pointer.h"              // for GenericPointer, GenericPo...
#include "strict_stox.hh"                   // for strict_stoull


constexpr const char PATCH_PATH[]                   = "path";
constexpr const char PATCH_FROM[]                   = "from";
constexpr const char PATCH_VALUE[]                  = "value";
constexpr const char PATCH_LIMIT[]                  = "limit";
constexpr const char PATCH_OP[]                     = "op";
constexpr const char PATCH_ADD[]                    = "add";
constexpr const char PATCH_REM[]                    = "remove";
constexpr const char PATCH_REP[]                    = "replace";
constexpr const char PATCH_MOV[]                    = "move";
constexpr const char PATCH_COP[]                    = "copy";
constexpr const char PATCH_TES[]                    = "test";
constexpr const char PATCH_INC[]                    = "incr";
constexpr const char PATCH_DEC[]                    = "decr";


/* Support for RFC 6902 */

void apply_patch(const MsgPack& patch, MsgPack& object);
void patch_add(const MsgPack& obj_patch, MsgPack& object);
void patch_remove(const MsgPack& obj_patch, MsgPack& object);
void patch_replace(const MsgPack& obj_patch, MsgPack& object);
void patch_move(const MsgPack& obj_patch, MsgPack& object);
void patch_copy(const MsgPack& obj_patch, MsgPack& object);
void patch_test(const MsgPack& obj_patch, MsgPack& object);
void patch_incr(const MsgPack& obj_patch, MsgPack& object);
void patch_decr(const MsgPack& obj_patch, MsgPack& object);

const MsgPack& get_patch_value(const MsgPack& obj_patch, const char* patch_op);
double get_patch_double(const MsgPack& val, const char* patch_op);


inline void _add(MsgPack& o, const MsgPack& val, std::string_view target) {
	switch (o.getType()) {
		case MsgPack::Type::MAP:
			o[target] = val;
			return;
		case MsgPack::Type::ARRAY:
			if (target.length() == 1 && target[0] == '-') {
				o.push_back(val);
			} else {
				try {
					size_t offset = strict_stoul(target);
					o.emplace(offset, val);
				} catch (const std::invalid_argument&) {
					THROW(ClientError, "Target in array must be a positive integer or '-'");
				}
			}
			return;
		default:
			THROW(ClientError, "Object is not array or map");
	}
}


inline void _erase(MsgPack& o, std::string_view target) {
	try {
		switch (o.getType()) {
			case MsgPack::Type::MAP:
				o.erase(target);
				return;
			case MsgPack::Type::ARRAY:
				try {
					size_t offset = strict_stoul(target);
					o.erase(offset);
				} catch (const std::invalid_argument&) {
					THROW(ClientError, "Target in array must be a positive integer");
				}
				return;
			default:
				THROW(ClientError, "Object is not array or map");
		}
	} catch (const std::out_of_range& e) {
		THROW(ClientError, "Target %s not found [%s]", target, e.what());
	}
}


inline void _incr(MsgPack& o, double val) {
	try {
		o += val;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Object is not numeric");
	}
}


inline void _incr(MsgPack& o, double val, double limit) {
	try {
		o += val;
		if (val < 0) {
			if (o.f64() <= limit) {
				THROW(LimitError, "Limit exceeded");
			}
		} else if (o.f64() >= limit) {
			THROW(LimitError, "Limit exceeded");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Object is not numeric");
	}
}


// Support for RFC 6901
inline void _tokenizer(const MsgPack& obj, std::vector<std::string>& path_split, const char* path_c, const char* patch_op) {
	try {
		const auto& path = obj.at(path_c);
		auto path_str = path.unformatted_string_view();
		rapidjson::GenericPointer<rapidjson::GenericValue<rapidjson::UTF8<>>> json_pointer(path_str.data(), path_str.size());
		size_t n_tok = json_pointer.GetTokenCount();

		for (size_t i = 0; i < n_tok; ++i) {
			auto& t = json_pointer.GetTokens()[i];
			path_split.push_back(std::string(t.name, t.length));
		}

		if (path_split.size() == 0 and path_str != "") {
			THROW(ClientError, "Bad syntax in '%s': %s (check RFC 6901)", path_c, path_str);
		}
	} catch (const std::out_of_range&) {
		THROW(ClientError, "Object MUST have exactly one '%s' member for patch operation: '%s'", path_c, patch_op);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "'%s' must be a string", path_c);
	}
}
