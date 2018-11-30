/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <cstdlib>            // for free
#include <cstring>            // for strtok_r
#include <cxxabi.h>           // for abi::__cxa_demangle
#include <dlfcn.h>            // for dladdr

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>         // for backtrace
#else
static inline int backtrace(void**, int) { return 0; }
#endif

#include "likely.h"

#ifndef NDEBUG
#ifndef XAPIAND_TRACEBACKS
#define XAPIAND_TRACEBACKS 1
#endif
#endif

#define BUFFER_SIZE 1024

#ifdef __APPLE__
#include <util.h>             // for forkpty
#include <unistd.h>           // for execlp
#include <termios.h>          // for termios, cfmakeraw
#include <sys/select.h>       // for select
/* Use `atos` to do symbol lookup, can lookup non-dynamic symbols and also line
 * numbers. This function is more complicated than you'd expect because `atos`
 * doesn't flush after each line, so plain pipe() or socketpair() won't work
 * until we close the write side. But the whole point is we want to keep `atos`
 * around so we don't have to reprocess the symbol table over and over. What we
 * wind up doing is using `forkpty()` to make a new pseudoterminal for atos to
 * run in, and thus will use line-buffering for stdout, and then we can get
 * each line.
 */
static std::string
atos(const void* address)
{
	static std::mutex mtx;
	std::lock_guard lk(mtx);

	char tmp[20];
	static int fd = -1;
	if (fd == -1) {
		Dl_info info;
		memset(&info, 0, sizeof(Dl_info));
		if (dladdr(reinterpret_cast<const void*>(&atos), &info) == 0) {
			perror("Could not get base address for `atos`.");
			return "";
		}

		struct termios term_opts;
		cfmakeraw(&term_opts);  // have to set this first, otherwise queries echo until child kicks in
		pid_t childpid;
		if unlikely((childpid = forkpty(&fd, nullptr, &term_opts, nullptr)) < 0) {
			perror("Could not forkpty for `atos` call.");
			return "";
		}

		if (childpid == 0) {
			snprintf(tmp, sizeof(tmp), "%p", static_cast<const void *>(info.dli_fbase));
			execlp("/usr/bin/atos", "atos", "-o", info.dli_fname, "-l", tmp, nullptr);
			fprintf(stderr,"Could not exec `atos` for stack trace!\n");
			exit(1);
		}

		int size = snprintf(tmp, sizeof(tmp), "%p\n", address);
		write(fd, tmp, size);

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
		int size = snprintf(tmp, sizeof(tmp), "%p\n", address);
		write(fd, tmp, size);
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
	fprintf(stderr, "Line read from `atos` was too long.\n");
	return "";
}
#else
static inline std::string
atos(const void*)
{
	return "";
}
#endif


std::string
traceback(const char* function, const char* filename, int line, const std::vector<void*>& callstack, int skip = 1)
{
	char tmp[20];

	std::string tb = "\n== Traceback (most recent call first): ";
	tb.append(filename);
	tb.push_back(':');
	tb.append(std::to_string(line));
	tb.append(" at ");
	tb.append(function);

	int frames = callstack.size();

	if (frames == 0) {
		tb.append(":\n    <empty traceback>");
		return tb;
	}

	if (frames < 2) {
		tb.append(":\n    <no traceback>");
		return tb;
	}

	tb.push_back(':');

	// Iterate over the callstack. Skip the first, it is the address of this function.
	std::string result;
	for (int i = skip; i < frames; ++i) {
		auto address = callstack[i];

		result.assign(std::to_string(frames - i - 1));
		result.push_back(' ');

		auto address_string = atos(address);
		if (address_string.size() > 2 && address_string.compare(0, 2, "0x") != 0) {
			result.append(address_string);
		} else {
			Dl_info info;
			memset(&info, 0, sizeof(Dl_info));
			if (dladdr(address, &info) != 0) {
				// Address:
				snprintf(tmp, sizeof(tmp), "%p ", address);
				result.append(tmp);
				// Symbol name:
				if (info.dli_sname != nullptr) {
					int status = 0;
					char* unmangled = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
					if (status != 0) {
						result.append(info.dli_sname);
					} else {
						try {
							result.append(unmangled);
							free(unmangled);
						} catch(...) {
							free(unmangled);
							throw;
						}
					}
				} else {
					result.append("[unknown symbol]");
				}
				// Offset:
				snprintf(tmp, sizeof(tmp), " + %zu", static_cast<const char*>(address) - static_cast<const char*>(info.dli_saddr));
				result.append(tmp);
			} else {
				snprintf(tmp, sizeof(tmp), "%p [unknown symbol]", address);
				result.append(tmp);
			}
		}
		tb.append("\n    ");
		tb.append(result);
	}

	return tb;
}


std::string
traceback(const char* function, const char* filename, int line, int skip)
{
	std::vector<void*> callstack;

	// retrieve current stack addresses
	callstack.resize(128);
	callstack.resize(backtrace(callstack.data(), callstack.size()));
	callstack.shrink_to_fit();

	auto tb = traceback(function, filename, line, callstack, skip);

	return tb;
}


extern "C" void
__assert_tb(const char* function, const char* filename, unsigned int line, const char* expression)
{
#ifdef XAPIAND_TRACEBACKS
	(void)fprintf(stderr, "Assertion failed: %s in %s %s:%u%s\n",
		expression, function, filename, line, traceback(function, filename, line, 2).c_str());
#else
	(void)fprintf(stderr, "Assertion failed: %s in %s %s:%u\n",
		expression, function, filename, line);
#endif
	abort();
}


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


BaseException::BaseException(BaseException::private_ctor, const BaseException& exc, const char *function_, const char *filename_, int line_, const char* type, std::string_view format, VFPRINTF_ARGS args)
	: type(type),
	  function(function_),
	  filename(filename_),
	  line(line_),
	  message(VSPRINTF(format, args))
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
