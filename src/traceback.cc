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
#include <chrono>                                 // for std::chrono
#include <cstdlib>                                // for std::free, std::exit
#include <cstdio>                                 // for std::perror, std::snprintf, std::fprintf
#include <cstring>                                // for std::memset
#include <cxxabi.h>                               // for abi::__cxa_demangle
#include <dlfcn.h>                                // for dladdr, dlsym
#include <exception>
#include <memory>                                 // for std::make_shared
#include <mutex>                                  // for std::mutex, std::lock_guard
#include <unwind.h>                               // for _Unwind_Exception
#if defined(__FreeBSD__)
#include <ucontext.h>                             // for ucontext_t
#else
#include <sys/ucontext.h>                         // for ucontext_t
#endif

#include "atomic_shared_ptr.h"                    // for atomic_shared_ptr
#include "error.hh"                               // for error::name
#include "likely.h"                               // for likely/unlikely
#include "strings.hh"                             // for strings::format
#include "time_point.hh"                          // for wait


constexpr const size_t max_frames = 128;

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

	bool empty() const {
		return callstack ? static_cast<void**>(*callstack) == callstack : true;

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
			rep.append(strings::format("{}", callstack[idx + 1]));
		}
		rep.append(" }");
		return rep;
	}
};


struct ThreadInfo {
	const char* name;
	std::atomic<pthread_t> pthread;

	std::atomic_size_t callstack_frames;
	std::array<std::atomic<void*>, max_frames> callstack;

	std::atomic_size_t snapshot_frames;
	std::array<std::atomic<void*>, max_frames> snapshot;

	std::atomic_int errnum;

	std::atomic_size_t req;
};


static std::array<ThreadInfo, 1000> pthreads;


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

	for (int t = 0; t <= 10; ++t) {
		const unsigned int MINLINE = 3;
		const unsigned int MAXLINE = 4096;
		char line[MAXLINE];
		size_t nread = 0;
		char c = '\0';
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
		if (nread > MAXLINE - 4) {
			std::fprintf(stderr, "Line read from `atos` was too long.\n");
			return "";
		}
		if (nread < MINLINE) {
			std::fprintf(stderr, "Line read from `atos` was too short.\n");
			return "";
		}
		if (t != 10 && line[nread - 3] == ':' && line[nread - 2] == '0' && line[nread - 1] == ')') {
			address = static_cast<const char*>(address) - 1;
			int size = std::snprintf(tmp, sizeof(tmp), "%p\n", address);
			write(fd, tmp, size);
		} else {
			if (t != 0) {
				line[nread++] = ' ';
				line[nread++] = '+';
				line[nread++] = ' ';
				line[nread++] = t + '0';
			}
			return std::string(line, nread);
		}
	}
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
	for (size_t i = skip; i < frames; ++i) {
		auto address = callstack[i + 1];

		std::string result = strings::format("{:3} ", frames - i - 1);

		auto address_string = atos(address);
		if (address_string.size() > 2 && address_string.compare(0, 2, "0x") != 0) {
			std::snprintf(tmp, sizeof(tmp), "%p ", address);
			result.append(tmp);
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
	void* tmp[max_frames];
	auto frames = backtrace(tmp, max_frames);
	void** callstack = (void**)malloc((frames + 1) * sizeof(void*));
	if (callstack != nullptr) {
		memcpy(callstack, tmp + 1, frames * sizeof(void*));
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
	if (callstack != nullptr) {
		free(callstack);
	}
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

#if defined(XAPIAND_TRACEBACKS) || defined(DEBUG)

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

// GCC's built-in protype for __cxa_throw uses 'void *', not 'std::type_info *'
#ifdef __clang__
typedef std::type_info __cxa_throw_type_info_t;
#else
typedef void __cxa_throw_type_info_t;
#endif
typedef void (*cxa_throw_type)(void*, __cxa_throw_type_info_t*, void (*)(void*));
void __cxa_throw(void* thrown_object, __cxa_throw_type_info_t* tinfo, void (*dest)(void*))
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
		auto frame = uc ? ((const frameinfo*)uc->uc_mcontext.mc_ebp) : nullptr;
	#elif defined(__linux__)
		auto frame = uc ? ((const frameinfo*)uc->uc_mcontext.gregs[REG_EBP]) : nullptr;
	#elif defined(__APPLE__)
		auto frame = uc && uc->uc_mcontext ? ((const frameinfo*)uc->uc_mcontext->__ss.__ebp) : nullptr;
	#else
		#error Unsupported OS.
	#endif
#elif defined(__x86_64__)
	#if defined(__FreeBSD__)
		auto frame = uc ? ((const frameinfo*)uc->uc_mcontext.mc_rbp) : nullptr;
	#elif defined(__linux__)
		auto frame = uc ? ((const frameinfo*)uc->uc_mcontext.gregs[REG_RBP]) : nullptr;
	#elif defined(__APPLE__)
		auto frame = uc && uc->uc_mcontext ? ((const frameinfo*)uc->uc_mcontext->__ss.__rbp) : nullptr;
	#else
		#error Unsupported OS.
	#endif
#else
	#error Unsupported architecture.
#endif
#ifdef __MACHINE_STACK_GROWS_UP
	#define BELOW >
#else
	#define BELOW <
#endif
	void* stack = &stack;
	auto return_address = frame && !(frame BELOW stack) ? frame->return_address : nullptr;

	auto pthread = pthread_self();
	for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
		auto& thread_info = pthreads[idx];
		if (thread_info.pthread.load(std::memory_order_relaxed) == pthread) {
			void* buf[max_frames];
			size_t frames = backtrace(buf, max_frames);
			void** callstack = buf;
			for (size_t n = 0; n < frames; ++n) {
				if (buf[n] == return_address) {
					callstack = &buf[n];
					frames -= n;
					break;
				}
			}
			for (size_t n = 0; n < frames; ++n) {
				thread_info.callstack[n].store(callstack[n], std::memory_order_relaxed);
			}
			thread_info.callstack_frames.store(frames, std::memory_order_release);
			thread_info.req.store(pthreads_req.load(std::memory_order_acquire), std::memory_order_release);
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
		auto& thread_info = pthreads[idx];
		auto pthread = thread_info.pthread.load(std::memory_order_relaxed);
		if (pthread) {
			thread_info.errnum.store(pthread_kill(pthread, SIGUSR2), std::memory_order_release);
		}
	}

	// try waiting for callstacks
	for (int w = 10; w >= 0; --w) {
		size_t ok = 0;
		for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
			auto& thread_info = pthreads[idx];
			if (thread_info.req.load() >= req) {
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
	size_t active = 0;
	for (; idx < pthreads.size() && idx < pthreads_cnt.load(std::memory_order_acquire); ++idx) {
		auto& thread_info = pthreads[idx];
		auto pthread = thread_info.pthread.load(std::memory_order_relaxed);
		if (pthread) {
			auto errnum = thread_info.errnum.load(std::memory_order_acquire);

			auto snapshot_frames = thread_info.snapshot_frames.load(std::memory_order_acquire);
			void* snapshot[max_frames + 1];
			for (size_t n = 0; n < snapshot_frames; ++n) {
				snapshot[n + 1] = thread_info.snapshot[n].load(std::memory_order_relaxed);
			}
			snapshot[0] = &snapshot[snapshot_frames];

			auto callstack_frames = thread_info.callstack_frames.load(std::memory_order_acquire);
			void* callstack[max_frames + 1];
			for (size_t n = 0; n < callstack_frames; ++n) {
				callstack[n + 1] = thread_info.callstack[n].load(std::memory_order_relaxed);
			}
			callstack[0] = &callstack[callstack_frames];

			if (!snapshot_frames || !callstack_frames) {
				++active;
				ret.append(strings::format("        " + STEEL_BLUE + "<Thread {}: {}{}{}>\n", idx, thread_info.name, !snapshot_frames ? " " + DARK_STEEL_BLUE + "(no snapshot)" + STEEL_BLUE : " " + DARK_STEEL_BLUE + "(no callstack)" + STEEL_BLUE, errnum ? " " + RED + "(" + error::name(errnum) + ")" + STEEL_BLUE : ""));
				if (callstack_frames) {
	#if defined(XAPIAND_TRACEBACKS) || defined(DEBUG)
					ret.append(strings::format(DEBUG_COL + "{}\n", strings::indent(traceback(thread_info.name, "", idx, callstack, skip), ' ', 8, true)));
	#endif
				}
			} else if (callstack[1 + skip] != snapshot[1 + skip]) {
				++active;
				ret.append(strings::format("        " + STEEL_BLUE + "<Thread {}: {}{}{}>\n", idx, thread_info.name, callstack[1 + skip] == snapshot[1 + skip] ? " " + DARK_STEEL_BLUE + "(idle)" + STEEL_BLUE : " " + DARK_ORANGE + "(active)" + STEEL_BLUE, errnum ? " " + RED + "(" + error::name(errnum) + ")" + STEEL_BLUE : ""));
	#if defined(XAPIAND_TRACEBACKS) || defined(DEBUG)
				ret.append(strings::format(DEBUG_COL + "{}\n", strings::indent(traceback(thread_info.name, "", idx, callstack, skip), ' ', 8, true)));
	#endif
			}
		}
		skip = 0;
	}
	return strings::format("    " + STEEL_BLUE + "<Threads {{total:{}, active:{}}}>\n", idx, active) + ret;
}


void
callstacks_snapshot()
{
	// Try to get a stable snapshot of callbacks for all threads.

	for (int t = 10; t >= 0; --t) {
		bool retry = true;
		auto start = std::chrono::steady_clock::now();
		auto end = start;
		while (retry && std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() < 100) {
			size_t req = pthreads_req.fetch_add(1) + 1;

			// request all threads to collect their callstack
			for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(); ++idx) {
				auto& thread_info = pthreads[idx];
				auto pthread = thread_info.pthread.load(std::memory_order_relaxed);
				if (pthread) {
					auto snapshot_frames = thread_info.snapshot_frames.load(std::memory_order_acquire);
					auto callstack_frames = thread_info.callstack_frames.load(std::memory_order_acquire);
					if (!snapshot_frames || !callstack_frames || snapshot_frames != callstack_frames) {
						thread_info.errnum.store(pthread_kill(pthread, SIGUSR2), std::memory_order_release);
					}
				}
			}

			// try waiting for callstacks
			for (int w = 10; w >= 0; --w) {
				size_t ok = 0;
				for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(); ++idx) {
					auto& thread_info = pthreads[idx];
					if (thread_info.req.load() >= req) {
						++ok;
					}
				}
				if (ok == pthreads_cnt.load()) {
					break;
				}
				sched_yield();
			}

			// save snapshots:
			retry = false;
			for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_cnt.load(); ++idx) {
				auto& thread_info = pthreads[idx];
				auto pthread = thread_info.pthread.load(std::memory_order_relaxed);
				if (pthread) {
					auto snapshot_frames = thread_info.snapshot_frames.load(std::memory_order_acquire);
					auto callstack_frames = thread_info.callstack_frames.load(std::memory_order_acquire);
					if (!snapshot_frames || !callstack_frames || snapshot_frames != callstack_frames) {
						retry = true;
					} else {
						for (size_t n = 0; n < snapshot_frames; ++n) {
							if (thread_info.snapshot[n].load(std::memory_order_relaxed) != thread_info.callstack[n].load(std::memory_order_relaxed)) {
								retry = true;
								break;
							}
						}
					}
					if (retry) {
						for (size_t n = 0; n < callstack_frames; ++n) {
							thread_info.snapshot[n].store(thread_info.callstack[n].load(std::memory_order_acquire), std::memory_order_relaxed);
						}
						thread_info.snapshot_frames.store(callstack_frames, std::memory_order_release);
					}
				} else {
					retry = true;
				}
			}

			sched_yield();

			end = std::chrono::steady_clock::now();
		}

		if (t == 0) {
			if (retry) {
				L_WARNING("Cannot take a snapshot of callbacks");
			}
			break;
		}

		nanosleep(10000000);  // sleep for 10 milliseconds
	}
}


void
init_thread_info(pthread_t pthread, const char* name)
{
	auto idx = pthreads_cnt.fetch_add(1);
	if (idx < pthreads.size()) {
		pthreads[idx].name = name;
		pthreads[idx].pthread.store(pthread, std::memory_order_release);
	}
}
