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


BaseException::BaseException()
	: line{0}
{
}


BaseException::BaseException(const BaseException& exc)
	: type{exc.type},
	  message{exc.message},
	  context{exc.context},
	  function{exc.function},
	  filename{exc.filename},
	  line{exc.line}
{
}


BaseException::BaseException(BaseException&& exc)
	: type{std::move(exc.type)},
	  message{std::move(exc.message)},
	  context{std::move(exc.context)},
	  function{std::move(exc.function)},
	  filename{std::move(exc.filename)},
	  line{std::move(exc.line)}
{
}


BaseException::BaseException(const BaseException* exc)
	: BaseException(exc != nullptr ? *exc : BaseException())
{
}


BaseException::BaseException(BaseException::private_ctor, const BaseException& exc, const char *function_, const char *filename_, int line_, const char* type, std::string_view format, format_args args)
	: type(type),
	  message(vformat(format, args)),
	  function(exc.type.empty() ? function_ : exc.function),
	  filename(exc.type.empty() ? filename_ : exc.filename),
	  line(exc.type.empty() ? line_ : exc.line)
{
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
