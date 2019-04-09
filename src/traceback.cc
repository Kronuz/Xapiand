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

#include "config.h"                               // for XAPIAND_TRACEBACKS

#include <array>                                  // for std::array
#include <atomic>                                 // for std::atomic_size
#include <cassert>                                // for std::array
#include <cstdlib>                                // for std::free, std::exit
#include <cstdio>                                 // for std::perror, std::snprintf, std::fprintf
#include <cstring>                                // for std::memset
#include <cxxabi.h>                               // for abi::__cxa_demangle
#include <dlfcn.h>                                // for dladdr, dlsym
#include <exception>
#include <memory>                                 // for std::make_shared
#include <mutex>                                  // for std::mutex, std::lock_guard
#include <unwind.h>                               // for _Unwind_Exception
#include <unistd.h>                               // for usleep
#if defined(__FreeBSD__)
#include <ucontext.h>                             // for ucontext_t
#else
#include <sys/ucontext.h>                         // for ucontext_t
#endif

#include "atomic_shared_ptr.h"                    // for atomic_shared_ptr
#include "likely.h"
#include "string.hh"                              // for string::format


#ifndef NDEBUG
#ifndef XAPIAND_TRACEBACKS
#define XAPIAND_TRACEBACKS 1
#endif
#endif

static std::atomic_size_t pthreads_req;
static std::atomic_size_t pthreads_cnt;


class Callstack {
	void** callstack;

public:
	Callstack(void** callstack) : callstack(callstack) {}

	Callstack(const Callstack& other) {
		callstack = (void**)malloc((other.size() + 1) * sizeof(void*));
		memcpy(callstack + 1, other.callstack + 1, other.size() * sizeof(void*));
		callstack[0] = &callstack[other.size()];
	}

	~Callstack() {
		if (callstack) {
			free(callstack);
		}
	}

	size_t size() const {
		return callstack ? static_cast<void**>(*callstack) - callstack : 0;
	}

	void** get() const {
		return callstack;
	}

	void** release() {
		auto ret = callstack;
		callstack = nullptr;
		return ret;
	}

	void* operator[](size_t idx) const {
		return idx < size() ? callstack[idx + 1] : nullptr;
	}

	bool operator==(const Callstack& other) const {
		if (size() != other.size()) {
			return false;
		}
		for (size_t idx = 0; idx < size(); ++idx) {
			if (callstack[idx + 1] != other.callstack[idx + 1]) {
				return false;
			}
		}
		return true;
	}

	bool operator!=(const Callstack& other) const {
		return !operator==(other);
	}

	std::string __repr__() const {
		std::string rep;
		rep.push_back('{');
		for (size_t idx = 0; idx < size(); ++idx) {
			rep.push_back(' ');
			rep.append(string::format("{}", callstack[idx + 1]));
		}
		rep.append(" }");
		return rep;
	}
};


struct ThreadInfo {
	pthread_t pthread;
	const char* name;
	std::shared_ptr<Callstack> callstack;
	std::shared_ptr<Callstack> snapshot;
	size_t req;

	ThreadInfo(pthread_t pthread, const char* name) :
		pthread(pthread),
		name(name),
		callstack(std::make_shared<Callstack>(nullptr)),
		snapshot(std::make_shared<Callstack>(nullptr)),
		req(pthreads_req.load(std::memory_order_acquire)) {}

	ThreadInfo(const ThreadInfo& other) :
		pthread(other.pthread),
		name(other.name),
		callstack(other.callstack),
		snapshot(other.snapshot),
		req(pthreads_req.load(std::memory_order_acquire)) {}

	~ThreadInfo() {

	}
};


static std::array<atomic_shared_ptr<ThreadInfo>, 1000> pthreads;


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

	const unsigned int MAXLINE = 4096;
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

		std::snprintf(tmp, sizeof(tmp), "[%p] ", address);
		result.append(tmp);

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
	assert(orig_cxa_allocate_exception != nullptr);
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
	assert(orig_cxa_free_exception != nullptr);
	orig_cxa_free_exception(thrown_object);
}

typedef void* (*cxa_allocate_dependent_exception_type)(size_t thrown_size);
void* __cxa_allocate_dependent_exception(size_t thrown_size) noexcept
{
	// call original __cxa_allocate_dependent_exception (reserving extra space for the callstack):
	static cxa_allocate_dependent_exception_type orig_cxa_allocate_dependent_exception = (cxa_allocate_dependent_exception_type)dlsym(RTLD_NEXT, "__cxa_allocate_dependent_exception");
	assert(orig_cxa_allocate_dependent_exception != nullptr);
	void* thrown_object = orig_cxa_allocate_dependent_exception(sizeof(void**) + thrown_size);
	return thrown_object;
}

typedef void (*cxa_free_dependent_exception_type)(void* thrown_object);
void __cxa_free_dependent_exception(void* thrown_object) noexcept
{
	// free callstack (if any):
	auto exception_header = static_cast<__cxa_exception*>(thrown_object) - 1;
	auto callstack = static_cast<void***>(static_cast<void*>(exception_header)) - 1;
	if (*callstack != nullptr) {
		free(*callstack);
	}
	// call original __cxa_free_dependent_exception:
	static cxa_free_dependent_exception_type orig_cxa_free_dependent_exception = (cxa_free_dependent_exception_type)dlsym(RTLD_NEXT, "__cxa_free_dependent_exception");
	assert(orig_cxa_free_dependent_exception != nullptr);
	orig_cxa_free_dependent_exception(thrown_object);
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
	assert(orig_cxa_throw != nullptr);
	orig_cxa_throw(thrown_object, tinfo, dest);
	__builtin_unreachable();
}

}

#endif

////////////////////////////////////////////////////////////////////////////////


void
collect_callstack_sig_handler(int /*signum*/, siginfo_t* /*info*/, void* ptr)
{
	struct frameinfo {
		frameinfo *next;
		void *return_address;
	};

	ucontext_t *uc = static_cast<ucontext_t *>(ptr);
#if defined(__i386__)
	#if defined(__FreeBSD__)
		auto return_address = ((const frameinfo*)uc->uc_mcontext.mc_ebp)->return_address;
	#elif defined(__linux__)
		auto return_address = ((const frameinfo*)uc->uc_mcontext.gregs[REG_EBP])->return_address;
	#elif defined(__APPLE__)
		auto return_address = ((const frameinfo*)uc->uc_mcontext->__ss.__ebp)->return_address;
	#else
		#error Unsupported OS.
	#endif
#elif defined(__x86_64__)
	#if defined(__FreeBSD__)
		auto return_address = ((const frameinfo*)uc->uc_mcontext.mc_rbp)->return_address;
	#elif defined(__linux__)
		auto return_address = ((const frameinfo*)uc->uc_mcontext.gregs[REG_RBP])->return_address;
	#elif defined(__APPLE__)
		auto return_address = ((const frameinfo*)uc->uc_mcontext->__ss.__rbp)->return_address;
	#else
		#error Unsupported OS.
	#endif
#else
	#error Unsupported architecture.
#endif

	auto pthread = pthread_self();
	for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
		std::shared_ptr<ThreadInfo> thread_info;
		while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
		if (thread_info->pthread == pthread) {
			auto callstack = backtrace();
			if (callstack) {
				size_t frames = static_cast<void**>(*callstack) - callstack;
				void** actual = nullptr;
				for (size_t n = 0; n < frames; ++n) {
					if (callstack[n + 1] == return_address) {
						actual = callstack + n;
						break;
					}
				}
				if (actual) {
					frames = static_cast<void**>(*callstack) - actual;
					auto new_callstack = (void**)malloc((frames + 1) * sizeof(void*));
					memcpy(new_callstack + 1, actual + 1, frames * sizeof(void*));
					new_callstack[0] = &new_callstack[frames];
					free(callstack);
					callstack = new_callstack;
				}
			}
			auto new_info = std::make_shared<ThreadInfo>(*thread_info);
			new_info->callstack = std::make_shared<Callstack>(callstack);
			pthreads[idx].store(new_info);
			return;
		}
	}
}


std::string
dump_callstacks()
{
	size_t req = pthreads_req.fetch_add(1) + 1;

	// request all threads to collect their callstack
	for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
		std::shared_ptr<ThreadInfo> thread_info;
		while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
		pthread_kill(thread_info->pthread, SIGUSR2);
	}

	// try waiting for callstacks
	for (int w = 10; w >= 0; --w) {
		size_t ok = 0;
		for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
			std::shared_ptr<ThreadInfo> thread_info;
			while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
			if (thread_info->req >= req) {
				++ok;
			}
		}
		if (ok == pthreads_cnt.load(std::memory_order_acquire)) {
			break;
		}
		sched_yield();
	}


	// print tracebacks:
	// The first idx is main thread, skip 4 frames:
	//     callstacks_snapshot -> setup_node_async_cb -> ev::base -> ev_invoke_pending
	//     collect_callstacks -> signal_sig_impl -> ev::base -> ev_invoke_pending
	std::string ret;
	size_t skip = 4;
	size_t idx = 0;
	for (; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
		std::shared_ptr<ThreadInfo> thread_info;
		while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
		auto& callstack = *thread_info->callstack;
		auto& snapshot = *thread_info->snapshot;
		if (callstack[skip] != snapshot[skip]) {
			ret.append(string::format("        <Thread {}: {} ({})>\n", idx, thread_info->name, callstack[skip] == snapshot[skip] ? "idle" : "active"));
#ifdef XAPIAND_TRACEBACKS
			ret.append(string::format(DEBUG_COL + "{}\n" + STEEL_BLUE, string::indent(traceback(thread_info->name, "", idx, callstack.get(), skip), ' ', 8, true)));
#endif
		}
		skip = 0;
	}
	return string::format("    <Threads {{cnt:{}}}>\n", idx) + ret;
}


void
callstacks_snapshot()
{
	// Try to get a stable snapshot of callbacks for all threads.

	for (int t = 10; t >= 0; --t) {
		do {
			size_t req = pthreads_req.fetch_add(1) + 1;

			// request all threads to collect their callstack
			for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
				std::shared_ptr<ThreadInfo> thread_info;
				while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
				auto& callstack = *thread_info->callstack;
				auto& snapshot = *thread_info->snapshot;
				if (!callstack.size() || callstack != snapshot) {
					pthread_kill(thread_info->pthread, SIGUSR2);
				} else {
					auto new_info = std::make_shared<ThreadInfo>(*thread_info);
					pthreads[idx].store(new_info);
				}
			}

			// try waiting for callstacks
			for (int w = 10; w >= 0; --w) {
				size_t ok = 0;
				for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
					std::shared_ptr<ThreadInfo> thread_info;
					while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
					if (thread_info->req >= req) {
						++ok;
					}
				}
				if (ok == pthreads_cnt.load(std::memory_order_acquire)) {
					break;
				}
				sched_yield();
			}

			auto retry = false;

			// save snapshots:
			for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
				std::shared_ptr<ThreadInfo> thread_info;
				while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
				auto& callstack = *thread_info->callstack;
				auto& snapshot = *thread_info->snapshot;
				if (!callstack.size() || callstack != snapshot) {
					auto new_info = std::make_shared<ThreadInfo>(*thread_info);
					new_info->snapshot = std::make_shared<Callstack>(callstack);
					pthreads[idx].store(new_info);
					retry = true;
				}
			}

			if (!retry) {
				break;
			}

			sched_yield();
		} while (true);

		if (t == 0) {
			break;
		}

		////////////////////////////////////////////////////////////////////////
		// Final check, to try making sure snapshot is stable:

		::usleep(10000);  // sleep for 10 milliseconds

		size_t req = pthreads_req.fetch_add(1) + 1;

		// request all threads to collect their callstack
		for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
			std::shared_ptr<ThreadInfo> thread_info;
			while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
			pthread_kill(thread_info->pthread, SIGUSR2);
		}

		// try waiting for callstacks
		for (int w = 10; w >= 0; --w) {
			size_t ok = 0;
			for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
				std::shared_ptr<ThreadInfo> thread_info;
				while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
				if (thread_info->req >= req) {
					++ok;
				}
			}
			if (ok == pthreads_cnt.load(std::memory_order_acquire)) {
				break;
			}
			sched_yield();
		}

		// check snapshots, invalidate if any is different:
		for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
			std::shared_ptr<ThreadInfo> thread_info;
			while (!(thread_info = pthreads[idx].load(std::memory_order_acquire))) {};
			auto& callstack = *thread_info->callstack;
			auto& snapshot = *thread_info->snapshot;
			if (callstack[0] != snapshot[0]) {
				auto new_info = std::make_shared<ThreadInfo>(*thread_info);
				new_info->callstack = std::make_shared<Callstack>(nullptr);
				pthreads[idx].store(new_info);
			}
		}
	}
}


void
init_thread_info(pthread_t pthread, const char* name)
{
	auto idx = pthreads_cnt.fetch_add(1);
	if (idx < pthreads.size()) {
		pthreads[idx].store(std::make_shared<ThreadInfo>(pthread, name));
	}
}
