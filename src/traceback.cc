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

#include "traceback.h"

#include "config.h"           // for XAPIAND_TRACEBACKS

#include <cstdlib>            // for std::free, std::exit
#include <cstdio>             // for std::perror, std::snprintf, std::fprintf
#include <cstring>            // for std::memset
#include <cxxabi.h>           // for abi::__cxa_demangle
#include <dlfcn.h>            // for dladdr
#include <exception>
#include <mutex>              // for std::mutex, std::lock_guard
#include <unwind.h>

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
#ifdef HAVE_POLL
#include <poll.h>             // for poll
#else
#include <sys/select.h>       // for select
#endif
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
		std::memset(&info, 0, sizeof(Dl_info));
		if (dladdr(reinterpret_cast<const void*>(&atos), &info) == 0) {
			std::perror("Could not get base address for `atos`.");
			return "";
		}

		struct termios term_opts;
		cfmakeraw(&term_opts);  // have to set this first, otherwise queries echo until child kicks in
		pid_t childpid;
		if unlikely((childpid = forkpty(&fd, nullptr, &term_opts, nullptr)) < 0) {
			std::perror("Could not forkpty for `atos` call.");
			return "";
		}

		if (childpid == 0) {
			std::snprintf(tmp, sizeof(tmp), "%p", static_cast<const void *>(info.dli_fbase));
			execlp("/usr/bin/atos", "atos", "-o", info.dli_fname, "-l", tmp, nullptr);
			std::fprintf(stderr,"Could not exec `atos` for stack trace!\n");
			std::exit(1);
		}

		int size = std::snprintf(tmp, sizeof(tmp), "%p\n", address);
		write(fd, tmp, size);

		// atos can take a while to parse symbol table on first request, which
		// is why we leave it running if we see a delay, explain what's going on...
		int err = 0;

#ifdef HAVE_POLL
		struct pollfd fds;
		fds.fd = fd;
		fds.events = POLLIN;
		err = poll(&fds, 1, 3000);
#else
		if (fdin < FD_SETSIZE) {
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(fd, &fds);
			struct timeval tv = {3, 0};
			err = select(fd + 1, &fds, nullptr, nullptr, &tv);
		}
#endif
		if unlikely(err < 0) {
			std::perror("Generating... first call takes some time for `atos` to cache the symbol table.");
		} else if (err == 0) {  // timeout
			std::fprintf(stderr, "Generating... first call takes some time for `atos` to cache the symbol table.\n");
		}
	} else {
		int size = std::snprintf(tmp, sizeof(tmp), "%p\n", address);
		write(fd, tmp, size);
	}

	const unsigned int MAXLINE = 1024;
	char line[MAXLINE];
	char c = '\0';
	size_t nread = 0;
	while (c != '\n' && nread < MAXLINE) {
		if unlikely(read(fd, &c, 1) <= 0) {
			std::perror("Lost `atos` connection.");
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
	std::fprintf(stderr, "Line read from `atos` was too long.\n");
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
traceback(const char* function, const char* filename, int line, void** callstack, int skip)
{
	char tmp[100];

	std::string tb = "\n== Traceback (most recent call first): ";
	if (filename && *filename) {
		tb.append(filename);
	}
	if (line) {
		if (filename && *filename) {
			tb.push_back(':');
		}
		tb.append(std::to_string(line));
	}
	if (function && *function) {
		if ((filename && *filename) || line) {
			tb.append(" at ");
		}
		tb.append(function);
	}

	if (callstack == nullptr) {
		tb.append(":\n    <invalid traceback>");
	}

	size_t frames = static_cast<void**>(*callstack) - callstack;

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
	for (size_t i = skip; i < frames; ++i) {
		auto address = callstack[i + 1];

		result.assign(std::to_string(frames - i - 1));
		result.push_back(' ');

		auto address_string = atos(address);
		if (address_string.size() > 2 && address_string.compare(0, 2, "0x") != 0) {
			result.append(address_string);
		} else {
			Dl_info info;
			std::memset(&info, 0, sizeof(Dl_info));
			if (dladdr(address, &info) != 0) {
				// Address:
				std::snprintf(tmp, sizeof(tmp), "%p ", address);
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
							std::free(unmangled);
						} catch(...) {
							std::free(unmangled);
							throw;
						}
					}
				} else {
					result.append("[unknown symbol]");
				}
				// Offset:
				std::snprintf(tmp, sizeof(tmp), " + %zu", static_cast<const char*>(address) - static_cast<const char*>(info.dli_saddr));
				result.append(tmp);
			} else {
				std::snprintf(tmp, sizeof(tmp), "%p [unknown symbol]", address);
				result.append(tmp);
			}
		}
		tb.append("\n    ");
		tb.append(result);
	}

	return tb;
}


void**
backtrace()
{
	void* tmp[128];
	auto frames = backtrace(tmp, 128);
	void** callstack = (void**)malloc((frames + 1) * sizeof(void*));
	if (callstack != nullptr) {
		memcpy(callstack + 1, tmp, frames * sizeof(void*));
		callstack[0] = &callstack[frames];
	}
	return callstack;
}


std::string
traceback(const char* function, const char* filename, int line, int skip)
{
	// retrieve current stack addresses
	auto callstack = backtrace();
	auto tb = traceback(function, filename, line, callstack, skip);
	free(callstack);
	return tb;
}


typedef void (*unexpected_handler)();

struct __cxa_exception {
    size_t referenceCount;

    std::type_info *exceptionType;
    void (*exceptionDestructor)(void *);
    unexpected_handler unexpectedHandler;
    std::terminate_handler  terminateHandler;

    __cxa_exception *nextException;

    int handlerCount;

    int handlerSwitchValue;
    const unsigned char *actionRecord;
    const unsigned char *languageSpecificData;
    void *catchTemp;
    void *adjustedPtr;

    _Unwind_Exception unwindHeader;
};


void**
exception_callstack(std::exception_ptr& eptr)
{
	void* thrown_object = *static_cast<void**>(static_cast<void*>(&eptr));
	auto exception_header = static_cast<__cxa_exception*>(thrown_object) - 1;
	auto callstack = static_cast<void***>(static_cast<void*>(exception_header)) - 1;
	return *callstack;
}

#ifdef XAPIAND_TRACEBACKS

extern "C" {

typedef void* (*cxa_allocate_exception_type)(size_t thrown_size);
void* __cxa_allocate_exception(size_t thrown_size) noexcept
{
	// call original __cxa_allocate_exception (reserving extra space for the callstack):
	static cxa_allocate_exception_type orig_cxa_allocate_exception = (cxa_allocate_exception_type)dlsym(RTLD_NEXT, "__cxa_allocate_exception");
	void* thrown_object = orig_cxa_allocate_exception(sizeof(void**) + thrown_size);
	return thrown_object;
}

typedef void (*cxa_free_exception_type)(void* thrown_object);
void __cxa_free_exception(void* thrown_object) noexcept
{
	// free callstack (if any):
	auto exception_header = static_cast<__cxa_exception*>(thrown_object) - 1;
	auto callstack = static_cast<void***>(static_cast<void*>(exception_header)) - 1;
	if (*callstack != nullptr) {
		free(*callstack);
	}
	// call original __cxa_free_exception:
	static cxa_free_exception_type orig_cxa_free_exception = (cxa_free_exception_type)dlsym(RTLD_NEXT, "__cxa_free_exception");
	orig_cxa_free_exception(thrown_object);
}

typedef void (*cxa_throw_type)(void*, std::type_info*, void (*)(void*));
void __cxa_throw(void* thrown_object, std::type_info* tinfo, void (*dest)(void*))
{
	// save callstack for exception (at the start of the reserved memory)
	auto exception_header = static_cast<__cxa_exception*>(thrown_object) - 1;
	auto callstack = static_cast<void***>(static_cast<void*>(exception_header)) - 1;
	*callstack = backtrace();
	// call original __cxa_throw:
	static cxa_throw_type orig_cxa_throw = (cxa_throw_type)dlsym(RTLD_NEXT, "__cxa_throw");
	orig_cxa_throw(thrown_object, tinfo, dest);
	__builtin_unreachable();
}

}

#endif
