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

#pragma once

#include "msgpack.h"
#include "utils.h"


bool apply_patch(const MsgPack& patch, MsgPack& object);
bool patch_add(const MsgPack& obj_patch, MsgPack& object);
bool patch_remove(const MsgPack& obj_patch, MsgPack& object);
bool patch_replace(const MsgPack& obj_patch, MsgPack& object);
bool patch_move(const MsgPack& obj_patch, MsgPack& object);
bool patch_copy(const MsgPack& obj_patch, MsgPack& object);
bool patch_test(const MsgPack& obj_patch, MsgPack& object);
MsgPack get_patch_value(const MsgPack& obj_patch);


inline void _add(MsgPack& o, MsgPack& val, const std::string& target) {
	if (o.obj->type == msgpack::type::MAP) {
		o[target] = val;
	} else if (o.obj->type == msgpack::type::ARRAY) {
		if (target.compare("-") == 0) {
			o.insert_item_to_array(o.obj->via.array.size, val);
		} else {
			int offset = strict_stoi(target);
			o.insert_item_to_array(offset, val);
		}
	}
}


inline void _erase(MsgPack&& o, const std::string& target) {
	if (o.obj->type == msgpack::type::MAP) {
		o.erase(target);
	} else if (o.obj->type == msgpack::type::ARRAY) {
		o.erase(strict_stoi(target));
	}
}


inline void _tokenizer(const MsgPack& obj, std::vector<std::string>& path_split, const char* path_c) {
	MsgPack path = obj.at(path_c);
	std::string path_str(path.get_str());
	stringTokenizer(path_str, "/", path_split);
}
