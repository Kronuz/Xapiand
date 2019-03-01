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

#include "module.h"

#if XAPIAND_CHAISCRIPT

#include "chaiscript/dispatchkit/bootstrap.hpp"
#include "chaiscript/dispatchkit/bootstrap_stl.hpp"
#include "chaiscript/language/chaiscript_prelude.hpp"


namespace chaipp {

void
Module::std_lib(chaiscript::Module& m)
{
	chaiscript::bootstrap::Bootstrap::bootstrap(m);

	chaiscript::bootstrap::standard_library::vector_type<std::vector<chaiscript::Boxed_Value>>("Vector", m);
	chaiscript::bootstrap::standard_library::string_type<std::string>("string", m);
	chaiscript::bootstrap::standard_library::map_type<std::map<std::string, chaiscript::Boxed_Value>>("Map", m);
	chaiscript::bootstrap::standard_library::pair_type<std::pair<chaiscript::Boxed_Value, chaiscript::Boxed_Value >>("Pair", m);

	m.eval(chaiscript::ChaiScript_Prelude::chaiscript_prelude() /*, "standard prelude"*/ );
}

}; // End namespace chaipp

#endif
