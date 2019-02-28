/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "config.h"                // for XAPIAND_CHAISCRIPT

#ifdef XAPIAND_CHAISCRIPT

#include <string>
#include <unordered_map>

#include "exception.h"
#include "schema.h"


class ScriptNotFoundError : public ClientError {
public:
	template<typename... Args>
	ScriptNotFoundError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class Script {
	enum class Type : uint8_t {
		EMPTY,
		CHAI,
	};

	void process_body(const MsgPack& _body);
	void process_name(const MsgPack& _name);
	void process_type(const MsgPack& _type);
	void process_value(const MsgPack& _value);
	void process_chai(const MsgPack& _chai);
	void process_params(const MsgPack& _params);

	std::string body;
	std::string name;
	MsgPack params;
	Type type;
	bool with_value;
	bool with_data;
	std::array<FieldType, SPC_TOTAL_TYPES> sep_types;

public:
	Script(const MsgPack& _obj);

	MsgPack process_chai(bool strict);
	MsgPack process_script(bool strict);
};

#endif
