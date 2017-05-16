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

#include "exception.h"
#include "msgpack.h"


namespace chaipp {

// Generic converter.
template <typename T, typename Enabler = void>
struct convert;


// MsgPack converter.
template <>
struct convert<MsgPack> {
private:
	static MsgPack process(const chaiscript::Boxed_Value& value) {
		if (value.is_type(chaiscript::user_type<std::map<std::string, chaiscript::Boxed_Value>>())) {
			const auto& cast_val = chaiscript::boxed_cast<std::map<std::string, chaiscript::Boxed_Value>>(value);
			MsgPack conv;
			conv.reserve(cast_val.size());
			for (const auto& pair : cast_val) {
				conv.insert(std::make_pair(pair.first, process(pair.second)));
			}
			return conv;
		} else if (value.is_type(chaiscript::user_type<std::vector<chaiscript::Boxed_Value>>())) {
			const auto& cast_val = chaiscript::boxed_cast<std::vector<chaiscript::Boxed_Value>>(value);
			MsgPack conv;
			conv.reserve(cast_val.size());
			for (const auto& val : cast_val) {
				conv.push_back(process(val));
			}
			return conv;
		} else if (value.is_type(chaiscript::user_type<std::string>())) {
			return MsgPack(chaiscript::boxed_cast<std::string>(value));
		} else {
			const auto& info = value.get_type_info();
			if (info.is_arithmetic()) {
				if (value.is_type(chaiscript::user_type<int>())) {
					return MsgPack(chaiscript::boxed_cast<int>(value));
				} else if (value.is_type(chaiscript::user_type<long>())) {
					return MsgPack(chaiscript::boxed_cast<long>(value));
				} else if (value.is_type(chaiscript::user_type<long long>())) {
					return MsgPack(chaiscript::boxed_cast<long long>(value));
				} else if (value.is_type(chaiscript::user_type<double>())) {
					return MsgPack(chaiscript::boxed_cast<double>(value));
				} else if (value.is_type(chaiscript::user_type<float>())) {
					return MsgPack(chaiscript::boxed_cast<float>(value));
				} else if (value.is_type(chaiscript::user_type<bool>())) {
					return MsgPack(chaiscript::boxed_cast<bool>(value));
				} else if (value.is_type(chaiscript::user_type<unsigned>())) {
					return MsgPack(chaiscript::boxed_cast<unsigned>(value));
				} else if (value.is_type(chaiscript::user_type<unsigned long>())) {
					return MsgPack(chaiscript::boxed_cast<unsigned long>(value));
				} else if (value.is_type(chaiscript::user_type<unsigned long long>())) {
					return MsgPack(chaiscript::boxed_cast<unsigned long long>(value));
				} else {
					return MsgPack(chaiscript::boxed_cast<long long>(value));
				}
			} else if (value.is_undef()) {
				return MsgPack();
			} else if (value.is_null()) {
				return MsgPack(nullptr);
			}

			throw Error("Cannot convert to MsgPack: " + info.name());
		}
	}

public:
	MsgPack operator()(const chaiscript::Boxed_Value& value) const {
		return process(value);
	}
};

}; // End namespace chaipp

#endif
