/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#if XAPIAND_CHAISCRIPT

#include <chaiscript/chaiscript.hpp>

#include "msgpack.h"


namespace chaipp {

// Generic Wrap.
template <typename T, typename Enabler = void>
struct wrap;


template <>
struct wrap<MsgPack> {
private:
	static inline chaiscript::Boxed_Value process(const MsgPack& obj) {
		switch (obj.getType()) {
			case MsgPack::Type::MAP: {
				std::map<std::string, chaiscript::Boxed_Value> map;
				const auto it_e = obj.end();
				for (auto it = obj.begin(); it != it_e; ++it) {
					map.insert(std::make_pair(it->as_string(), process(it.value())));
				}
				return chaiscript::Boxed_Value(map);
			}
			case MsgPack::Type::ARRAY: {
				std::vector<chaiscript::Boxed_Value> vec;
				vec.reserve(obj.size());
				for (const auto& value : obj) {
					vec.push_back(process(value));
				}
				return chaiscript::Boxed_Value(vec);
			}
			case MsgPack::Type::STR:
				return chaiscript::Boxed_Value(obj.as_string());
			case MsgPack::Type::POSITIVE_INTEGER:
				return chaiscript::Boxed_Value(obj.as_u64());
			case MsgPack::Type::NEGATIVE_INTEGER:
				return chaiscript::Boxed_Value(obj.as_i64());
			case MsgPack::Type::FLOAT:
				return chaiscript::Boxed_Value(obj.as_f64());
			case MsgPack::Type::BOOLEAN:
				return chaiscript::Boxed_Value(obj.as_bool());
			case MsgPack::Type::UNDEFINED:
				return chaiscript::Boxed_Value();
			case MsgPack::Type::NIL:
				return chaiscript::Boxed_Value(nullptr);
			default:
				return chaiscript::Boxed_Value();
		}
	}
public:
	chaiscript::Boxed_Value operator()(const MsgPack& obj) const {
		return process(obj);
	}
};

}; // End namespace chaipp

#endif
