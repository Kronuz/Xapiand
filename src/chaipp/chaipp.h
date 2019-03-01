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

#include "config.h"  // for XAPIAND_CHAISCRIPT

#if XAPIAND_CHAISCRIPT

#include "string_view.hh"

#include "chaiscript/chaiscript_basic.hpp"
#include "script.h"


class MsgPack;


namespace chaipp {

class Processor {
	size_t hash;
	chaiscript::ChaiScript_Basic chai;
	chaiscript::AST_NodePtr ast;
	MsgPack script_params;

public:
	Processor(const Script& script);

	void operator()(std::string_view method, MsgPack& doc, const MsgPack& old_doc, const MsgPack& params);
	static std::shared_ptr<Processor> compile(const Script& script);

	size_t get_hash() {
		return hash;
	}
};

}; // End namespace chaipp

#endif
