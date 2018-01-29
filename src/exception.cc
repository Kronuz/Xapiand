/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors
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

#include "exception.h"

#include <cstdarg>            // for va_end, va_list, va_start
#include <cstdio>             // for vsnprintf
#include <cstdlib>            // for free
#include <cstring>            // for strtok_r
#include <cxxabi.h>           // for abi::__cxa_demangle
#include <execinfo.h>         // for backtrace, backtrace_symbols


#define BUFFER_SIZE 1024


std::string traceback(const char *filename, int line) {
	std::string t;
	void* callstack[128];

	// retrieve current stack addresses
	int frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));

	t = "\n== Traceback at (" + std::string(filename) + ":" + std::to_string(line) + "):";

	if (frames == 0) {
		t += "\n    <empty, possibly corrupt>";
		return t;
	}

	// resolve addresses into strings containing "filename(function+address)"
	char** strs = backtrace_symbols(callstack, frames);

	// iterate over the returned symbol lines. skip the first, it is the
	// address of this function.
	for (int i = 1; i < frames; ++i) {
		int status = 0;
		const char *sep = "\t ()+";
		char *mangled, *lasts;
		std::string result;
		for (mangled = strtok_r(strs[i], sep, &lasts); mangled; mangled = strtok_r(nullptr, sep, &lasts)) {
			char* unmangled = abi::__cxa_demangle(mangled, nullptr, 0, &status);
			if (!result.empty()) {
				result += " ";
			}
			if (status == 0) {
				result += unmangled;
			} else {
				result += mangled;
			}
			free(unmangled);
		}
		t += "\n    " + result;
	}

	free(strs);
	return t;
}


BaseException::BaseException(const char *filename, int line, const char* type, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);

	// Figure out the length of the formatted message.
	va_list argptr_copy;
	va_copy(argptr_copy, argptr);
	auto len = vsnprintf(nullptr, 0, format, argptr_copy);
	va_end(argptr_copy);

	// Make a string to hold the formatted message.
	message.resize(len + 1);
	vsnprintf(&message[0], len + 1, format, argptr);
	message.resize(len);

	va_end(argptr);

	if (message.empty()) {
		message.assign(type);
	}

	context.assign(std::string(filename) + ":" + std::to_string(line) + ": " + message);
#ifdef XAPIAND_TRACEBACKS
	traceback = ::traceback(filename, line);
#else
	traceback = "\n== Exception at (" + std::string(filename) + ":" + std::to_string(line) + ")";
#endif
}
