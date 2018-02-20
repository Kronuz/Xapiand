/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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

#ifdef __APPLE__
#include <util.h>
#include <dlfcn.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
/* Use `atos` to do symbol lookup, can lookup non-dynamic symbols and also line
 * numbers. This function is more complicated than you'd expect because `atos`
 * doesn't flush after each line, so plain pipe() or socketpair() won't work
 * until we close the write side. But the whole point is we want to keep `atos`
 * around so we don't have to reprocess the symbol table over and over. What we
 * wind up doing is using `forkpty()` to make a new pseudoterminal for atos to
 * run in, and thus will use line-buffering for stdout, and then we can get
 * each line.
 */
inline static std::string atos(string_view address) {
	static int fd = -1;
	if (fd == -1) {
		Dl_info info;
		memset(&info, 0, sizeof(Dl_info));
		if (!dladdr((const void*)&atos, &info)) {
			perror("Could not get base address for `atos`.");
			return "";
		}

		struct termios term_opts;
		cfmakeraw(&term_opts);  // have to set this first, otherwise queries echo until child kicks in
		pid_t childpid;
		if unlikely((childpid = forkpty(&fd, NULL, &term_opts, NULL)) < 0) {
			perror("Could not forkpty for `atos` call.");
			return "";
		}

		if (childpid == 0) {
			char base[20];
			snprintf(base, 20, "%p", info.dli_fbase);
			execlp("/usr/bin/atos", "atos", "-o", info.dli_fname, "-l", base, (char*)0);
			fprintf(stderr,"Could not exec `atos` for stack trace!\n");
			exit(1);
		}

		write(fd, address.data(), address.size());
		write(fd, "\n", 1);

		// atos can take a while to parse symbol table on first request, which
		// is why we leave it running if we see a delay, explain what's going on...
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(fd, &fds);
		struct timeval tv = {3, 0};
		int err = select(fd + 1, &fds, nullptr, nullptr, &tv);
		if unlikely(err < 0) {
			perror("Generating... first call takes some time for `atos` to cache the symbol table.");
		} else if (err == 0) {  // timeout
			fprintf(stderr, "Generating... first call takes some time for `atos` to cache the symbol table.\n");
		}
	} else {
		write(fd, address.data(), address.size());
		write(fd, "\n", 1);
	}

	const unsigned int MAXLINE = 1024;
	char line[MAXLINE];
	char c = '\0';
	size_t nread = 0;
	while (c != '\n' && nread < MAXLINE) {
		if unlikely(read(fd, &c, 1) <= 0) {
			perror("Lost `atos` connection.");
			close(fd);
			fd = -1;
			return "";
		}
		if (c != '\n') {
			line[nread++] = c;
		}
	}
	if (nread < MAXLINE) {
		return std::string(line, nread);
	}
	return "";
}
#else
inline static std::string atos(string_view address) {
	return "";
}
#endif


std::string
traceback(string_view filename, int line, void *const * callstack, int frames)
{
	std::string tb = "\n== Traceback at (";
	tb.append(filename.data(), filename.size());
	tb.push_back(':');
	tb.append(std::to_string(line));
	tb.push_back(')');

	if (frames == 0) {
		return tb;
	}

	tb.push_back(':');

	// resolve addresses into strings containing "filename(function+address)"
	char** strs = backtrace_symbols(callstack, frames);

	// iterate over the returned symbol lines. skip the first, it is the
	// address of this function.
	for (int i = 1; i < frames; ++i) {
		const char *sep = "\t ()+";
		char *mangled, *lasts;
		std::string result;
		for (mangled = strtok_r(strs[i], sep, &lasts); mangled; mangled = strtok_r(nullptr, sep, &lasts)) {
			int status = 0;
			char* unmangled = abi::__cxa_demangle(mangled, nullptr, 0, &status);
			if (!result.empty()) {
				result.push_back(' ');
			}
			if (status) {
				string_view address(mangled);
				if (address.size() > 2 && address.compare(0, 2, "0x") == 0) {
					auto tmp = atos(address);
					if (tmp.size() > 2 && tmp.compare(0, 2, "0x") != 0) {
						result.append(tmp);
						break;
					}
				}
				result.append(mangled);
			} else {
				result.append(unmangled);
				free(unmangled);
			}
		}
		tb.append("\n    ");
		tb.append(result);
	}

	free(strs);
	return tb;
}


std::string
traceback(string_view filename, int line)
{
	void* callstack[128];

	// retrieve current stack addresses
	int frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));

	auto tb = traceback(filename, line, callstack, frames);
	if (frames == 0) {
		tb.append(":\n    <empty, possibly corrupt>");
	}

	return tb;
}


extern "C" void
__assert_tb(const char* function, const char* filename, unsigned int line, const char* expression)
{
	(void)fprintf(stderr, "Assertion failed: %s, function %s, file %s, line %u.%s\n",
		expression, function, filename, line, traceback(filename, line).c_str());
	abort();
}



BaseException::BaseException()
	: line{0},
	  callstack{{}},
	  frames{0}
{ }


BaseException::BaseException(const BaseException& exc)
	: type{exc.type},
	  filename{exc.filename},
	  line{exc.line},
	  frames{exc.frames},
	  message{exc.message},
	  context{exc.context},
	  traceback{exc.traceback}
{
	memcpy(callstack, exc.callstack, frames * sizeof(void*));
}


BaseException::BaseException(BaseException&& exc)
	: type{std::move(exc.type)},
	  filename{std::move(exc.filename)},
	  line{std::move(exc.line)},
	  callstack{std::move(exc.callstack)},
	  frames{std::move(exc.frames)},
	  message{std::move(exc.message)},
	  context{std::move(exc.context)},
	  traceback{std::move(exc.traceback)}
{ }


BaseException::BaseException(const BaseException* exc)
	: BaseException(exc ? *exc : BaseException())
{ }


BaseException::BaseException(BaseException::private_ctor, const char *filename, int line, const char* type, string_view format, int n, ...)
	: type(type),
	  filename(filename),
	  line(line),
	  frames(0)
{
	va_list argptr;
	va_start(argptr, n);

	auto format_c_str = string_view_data_as_c_str(format);

	// Figure out the length of the formatted message.
	va_list argptr_copy;
	va_copy(argptr_copy, argptr);
	auto len = vsnprintf(nullptr, 0, format_c_str, argptr_copy);
	va_end(argptr_copy);

	// Make a string to hold the formatted message.
	message.resize(len + 1);
	message.resize(vsnprintf(&message[0], len + 1, format_c_str, argptr));

	va_end(argptr);

#ifdef XAPIAND_TRACEBACKS
	frames = backtrace(callstack, sizeof(callstack) / sizeof(void*));
#endif
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
		context.assign(filename + ":" + std::to_string(line) + ": " + get_message());
	}
	return context.c_str();
}


const char*
BaseException::get_traceback() const
{
	if (traceback.empty()) {
		traceback = ::traceback(filename, line, callstack, frames);
	}
	return traceback.c_str();
}
