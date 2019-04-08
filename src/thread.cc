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

#include "thread.hh"

#include "config.h"              // for HAVE_PTHREADS, HAVE_PTHREAD_SETNAME_NP

#include <errno.h>               // for errno
#include <array>                 // for std::array
#include <atomic>                // for std::atomic_int
#include <mutex>                 // for std::mutex, std::lock_guard
#include <string>                // for std::string
#include <string_view>           // for std::string_view
#include <system_error>          // for std::system_error
#include <thread>                // for std::thread
#include <tuple>                 // for std::forward_as_tuple
#include <unordered_map>         // for std::unordered_map
#include <vector>                // for std::vector
#include <unistd.h>              // for getpid
#include <dlfcn.h>               // for dlsym
#ifdef HAVE_PTHREADS
#include <pthread.h>             // for pthread_self
#endif
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>          // for pthread_setname_np
#endif

#include "error.hh"              // for error::name, error::description
#include "log.h"                 // for L_WARNING_ONCE
#include "traceback.h"           // for backtrace, traceback


static std::mutex thread_names_mutex;
static std::unordered_map<std::thread::id, std::string> thread_names;
static std::array<pthread_t, 1000> pthreads;
static std::array<const char*, 1000> pthreads_names;
static std::array<void**, 1000> callstacks;
static std::array<void**, 1000> snapshots;
static std::atomic_size_t pthreads_num;
static std::atomic_size_t pthreads_busy;


#ifdef __APPLE__
#include <cpuid.h>
int
sched_getcpu()
{
	uint32_t info[4];
	__cpuid_count(1, 0, info[0], info[1], info[2], info[3]);
	if ( (info[3] & (1 << 9)) == 0) {
		return -1;  // no APIC on chip
	}
	// info[1] is EBX, bits 24-31 are APIC ID
	return (unsigned)info[1] >> 24;
}
#endif


void
run_thread(void *(*thread_routine)(void *), void *arg, ThreadPolicyType)
{
	std::thread(thread_routine, arg).detach();
}


void
setup_thread(const std::string& name, ThreadPolicyType)
{
	set_thread_name(name);
}


////////////////////////////////////////////////////////////////////////////////


void
set_thread_name(const std::string& name)
{
	auto pthread = pthread_self();
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__linux__)
	pthread_setname_np(pthread, ("Xapiand:" + name).c_str());
	// pthread_setname_np(pthread, ("Xapiand:" + name).c_str(), nullptr);
#elif defined(HAVE_PTHREAD_SETNAME_NP)
	pthread_setname_np(("Xapiand:" + name).c_str());
#elif defined(HAVE_PTHREAD_SET_NAME_NP)
	pthread_set_name_np(pthread, ("Xapiand:" + name).c_str());
#endif
	std::lock_guard<std::mutex> lk(thread_names_mutex);
	auto emplaced = thread_names.emplace(std::piecewise_construct,
		std::forward_as_tuple(std::this_thread::get_id()),
		std::forward_as_tuple(name));

	size_t zero;
	do {
		zero = 0;
		sched_yield();
	} while (!pthreads_busy.compare_exchange_weak(zero, 1));

	auto idx = pthreads_num.fetch_add(1);
	if (idx < pthreads.size()) {
		pthreads[idx] = pthread;
		pthreads_names[idx] = emplaced.first->second.c_str();
	}

	pthreads_busy.store(0);
}


const std::string&
get_thread_name(std::thread::id thread_id)
{
	std::lock_guard<std::mutex> lk(thread_names_mutex);
	auto thread = thread_names.find(thread_id);
	if (thread == thread_names.end()) {
		static std::string _ = "???";
		return _;
	}
	return thread->second;
}


const std::string&
get_thread_name()
{
	return get_thread_name(std::this_thread::get_id());
}


////////////////////////////////////////////////////////////////////////////////


void
collect_callstack_sig_handler(int /*signum*/)
{
	auto pthread = pthread_self();
	for (size_t idx = 0; idx < pthreads.size() && idx < pthreads_num.load(); ++idx) {
		if (pthreads[idx] == pthread) {
			auto& callstack = callstacks[idx];
			if (callstack != nullptr) {
				free(callstack);
			}
			callstack = backtrace();
			pthreads_busy.fetch_sub(1);
			return;
		}
	}
}


void
collect_callstacks()
{
	size_t total;
	size_t zero;
	do {
		sched_yield();
		total = pthreads_num.load();
		zero = 0;
	} while (!pthreads_busy.compare_exchange_weak(zero, total));

	for (size_t idx = 0; idx < pthreads.size() && idx < total; ++idx) {
		if (pthread_kill(pthreads[idx], SIGUSR2) != 0) {
			pthreads_busy.fetch_sub(1);
		}
	}

	do {
		zero = 0;
		sched_yield();
	} while (!pthreads_busy.compare_exchange_weak(zero, 1));

	for (size_t idx = 1; idx < pthreads.size() && idx < total; ++idx) {
		auto& callstack = callstacks[idx];
		auto& snapshot = snapshots[idx];
		size_t callstack_frames = static_cast<void**>(*callstack) - callstack;
		size_t snapshot_frames = snapshot ? static_cast<void**>(*snapshot) - snapshot : 0;
		if (snapshot_frames <= 3 || callstack_frames <= 3 || snapshot[3] != callstack[3]) {
			print(traceback(pthreads_names[idx], "", idx, callstack), 3);
		}
	}

	pthreads_busy.store(0);
}


void
callstacks_snapshot()
{
	bool retry;
	do {
		retry = false;

		size_t total;
		size_t zero;
		do {
			sched_yield();
			total = pthreads_num.load();
			zero = 0;
		} while (!pthreads_busy.compare_exchange_weak(zero, total));

		for (size_t idx = 0; idx < pthreads.size() && idx < total; ++idx) {
			if (pthread_kill(pthreads[idx], SIGUSR2) != 0) {
				pthreads_busy.fetch_sub(1);
			}
		}

		do {
			zero = 0;
			sched_yield();
		} while (!pthreads_busy.compare_exchange_weak(zero, 1));

		for (size_t idx = 0; idx < pthreads.size() && idx < total; ++idx) {
			auto& callstack = callstacks[idx];
			auto& snapshot = snapshots[idx];
			auto different = true;
			size_t callstack_frames = static_cast<void**>(*callstack) - callstack;
			if (snapshot) {
				size_t snapshot_frames = static_cast<void**>(*snapshot) - snapshot;
				if (snapshot_frames == callstack_frames) {
					different = false;
					for (size_t i = 0; i < callstack_frames; ++i) {
						if (callstack[i + 1] != snapshot[i + 1]) {
							different = true;
							break;
						}
					}
				}
			}
			if (different) {
				if (snapshot) {
					free(snapshot);
				}
				snapshot = (void**)malloc((callstack_frames + 1) * sizeof(void*));
				memcpy(snapshot + 1, callstack + 1, callstack_frames * sizeof(void*));
				snapshot[0] = &snapshot[callstack_frames];
				retry = true;
			}
		}

		pthreads_busy.store(0);
	} while (retry);
}
