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

#include "exception.h"

#include "log.h"

#include <string.h>
#include <execinfo.h>
#include <cxxabi.h>
#include <stdarg.h>

#define BUFFER_SIZE 1024


std::string traceback(const char *filename, int line) {
	std::string t;
#ifdef TRACEBACK
	void* callstack[128];

	// retrieve current stack addresses
	int frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));

	t = "\nTraceback (" + std::string(filename) + ":" + std::to_string(line) + "):";

	if (frames == 0) {
		t += "\n    <empty, possibly corrupt>";
		return t.c_str();
	}

	// resolve addresses into strings containing "filename(function+address)"
	char** strs = backtrace_symbols(callstack, frames);

	// iterate over the returned symbol lines. skip the first, it is the
	// address of this function.
	for (int i = 0; i < frames; ++i) {
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
#endif
	return t;
}


Exception::Exception(const char *filename, int line, const char *format, ...)
	: std::runtime_error("")
{
	char buffer[BUFFER_SIZE];

	va_list argptr;
	va_start(argptr, format);
	vsnprintf(buffer, BUFFER_SIZE, format, argptr);
	va_end(argptr);
	msg.assign(buffer);

	snprintf(buffer, BUFFER_SIZE, "%s:%d", filename, line);
	context.assign(std::string(buffer) + ": " + msg);

#ifdef TRACEBACK
	traceback = ::traceback(filename, line);
#endif
}
