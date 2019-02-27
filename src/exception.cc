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

#include "exception.h"

#include "config.h"           // for XAPIAND_TRACEBACKS

#include "traceback.h"        // for traceback


BaseException::BaseException()
	: line{0}
{ }


BaseException::BaseException(const BaseException& exc)
	: type{exc.type},
	  function{exc.function},
	  filename{exc.filename},
	  line{exc.line},
	  callstack{exc.callstack},
	  message{exc.message},
	  context{exc.context},
	  traceback{exc.traceback}
{
}


BaseException::BaseException(BaseException&& exc)
	: type{std::move(exc.type)},
	  function{std::move(exc.function)},
	  filename{std::move(exc.filename)},
	  line{std::move(exc.line)},
	  callstack{std::move(exc.callstack)},
	  message{std::move(exc.message)},
	  context{std::move(exc.context)},
	  traceback{std::move(exc.traceback)}
{ }


BaseException::BaseException(const BaseException* exc)
	: BaseException(exc != nullptr ? *exc : BaseException())
{ }


BaseException::BaseException(BaseException::private_ctor, const BaseException& exc, const char *function_, const char *filename_, int line_, const char* type, std::string_view format, format_args args)
	: type(type),
	  function(function_),
	  filename(filename_),
	  line(line_),
	  message(vformat(format, args))
{
	if (!exc.type.empty()) {
		function = exc.function;
		filename = exc.filename;
		line = exc.line;
		callstack = exc.callstack;
	} else {
#ifdef XAPIAND_TRACEBACKS
		// retrieve current stack addresses
		callstack.resize(128);
		callstack.resize(backtrace(callstack.data(), callstack.size()));
		callstack.shrink_to_fit();
#endif
	}
}

const char*
BaseException::get_message() const
{
	if (message.empty()) {
		message.assign(type);
	}
	return message.c_str();
}


const char*
BaseException::get_context() const
{
	if (context.empty()) {
		context.append(filename);
		context.push_back(':');
		context.append(std::to_string(line));
		context.append(" at ");
		context.append(function);
		context.append(": ");
		context.append(get_message());
	}
	return context.c_str();
}


const char*
BaseException::get_traceback() const
{
	if (traceback.empty()) {
		traceback = ::traceback(function, filename, line, callstack);
	}
	return traceback.c_str();
}
