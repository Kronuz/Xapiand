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

#include "thread.hh"

#include "config.h"              // for HAVE_PTHREADS, HAVE_PTHREAD_SETNAME_NP

#include <mutex>                 // for std::mutex, std::lock_guard
#include <string>                // for std::string
#include <tuple>                 // for std::forward_as_tuple
#include <unordered_map>         // for std::unordered_map

#ifdef HAVE_PTHREADS
#include <pthread.h>             // for pthread_self
#endif
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>          // for pthread_setaffinity_np
#endif

#include "log.h"                 // for L_WARNING_ONCE
#include "stringified.hh"        // for stringified


static std::mutex thread_names_mutex;
static std::unordered_map<std::thread::id, std::string> thread_names;


#ifndef HAVE_PTHREAD_SETAFFINITY_NP

typedef struct cpu_set {
	uint32_t count;
} cpu_set_t;


static inline void
CPU_ZERO(cpu_set_t *cs) {
	cs->count = 0;
}


static inline void
CPU_SET(int num, cpu_set_t *cs) {
	cs->count |= (1 << num);
}


static inline int
CPU_ISSET(int num, cpu_set_t *cs) {
	return (cs->count & (1 << num));
}


#if defined(__APPLE__)
#include <mach/thread_act.h>
#endif

static inline int
pthread_setaffinity_np(pthread_t thread, size_t cpu_size, cpu_set_t *cpu_set)
{
#if defined(__APPLE__)
	int core = 0;
	for (; core < 8 * static_cast<int>(cpu_size); ++core) {
		if (CPU_ISSET(core, cpu_set)) {
			break;
		}
	}
	thread_affinity_policy_data_t policy = { core };
	thread_port_t mach_thread = pthread_mach_thread_np(thread);
	thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY, (thread_policy_t)&policy, 1);
#else
	L_WARNING_ONCE("WARNING: No way of setting cpu affinity.");
#endif
	return 0;
}

#endif


////////////////////////////////////////////////////////////////////////////////


void
set_thread_afinity(uint64_t afinity_map)
{
	if (afinity_map) {
		static const unsigned int hardware_concurrency = std::thread::hardware_concurrency();

		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		for (int core = 0; core < 64; ++core) {
			if ((afinity_map >> core) & 1) {
				CPU_SET(core / hardware_concurrency, &cpuset);
			}
		}
		pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	}
}


void
set_thread_name(const std::string& name)
{
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__linux__)
	pthread_setname_np(pthread_self(), stringified(name).c_str());
	// pthread_setname_np(pthread_self(), stringified(name).c_str(), nullptr);
#elif defined(HAVE_PTHREAD_SETNAME_NP)
	pthread_setname_np(stringified(name).c_str());
#elif defined(HAVE_PTHREAD_SET_NAME_NP)
	pthread_set_name_np(pthread_self(), stringified(name).c_str());
#endif
	std::lock_guard<std::mutex> lk(thread_names_mutex);
	thread_names.emplace(std::piecewise_construct,
		std::forward_as_tuple(std::this_thread::get_id()),
		std::forward_as_tuple(name));
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
