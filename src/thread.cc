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
#include <system_error>          // for std::system_error

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


struct ThreadPolicy {
	int priority;
	uint64_t afinity;

	ThreadPolicy(ThreadPolicyType thread_policy) {
		switch (thread_policy) {
			case ThreadPolicyType::regular:
				priority = 0;
				afinity = 0b0000000000000000000000000000000000000000000000000000000000000000;
				break;
			case ThreadPolicyType::wal_writer:
				priority = 20;
				afinity = 0b0000000000000000000000000000000000000000000000000000000000001111;
				break;
			case ThreadPolicyType::logging:
				priority = 0;
				afinity = 0b0000000000000000000000000000000000000000000000000000000011111111;
				break;
			case ThreadPolicyType::replication:
				priority = 10;
				afinity = 0b0000000000000000000000000000000000000000000000000000000000000000;
				break;
			case ThreadPolicyType::committers:
				priority = 100;
				afinity = 0b1111111111111111111100000000000000000000000000000000000000000000;
				break;
			case ThreadPolicyType::fsynchers:
				priority = 20;
				afinity = 0b0000000000000000000000000000000000000000000000001111111111111111;
				break;
			case ThreadPolicyType::updaters:
				priority = 10;
				afinity = 0b0000000000000000000000000000000000000000000000000000000000000001;
				break;
			case ThreadPolicyType::servers:
				priority = 5;
				afinity = 0b0000000000000000000000000000000000000000000000001111111111111111;
				break;
			case ThreadPolicyType::http_clients:
				priority = 10;
				afinity = 0b0000000000000000111111111111111111111111111111110000000000000000;
				break;
			case ThreadPolicyType::binary_clients:
				priority = 20;
				afinity = 0b0000000000000000000000000000000011111111111111111111111111111111;
				break;
		}
	}
};


#ifdef __APPLE__
#include <mach/mach_time.h>
#include <mach/thread_act.h>
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


void
start_thread(void *(*thread_routine)(void *), void *arg, ThreadPolicyType thread_policy)
{
	static const unsigned int hardware_concurrency = std::thread::hardware_concurrency();

	ThreadPolicy policy(thread_policy);

	int errnum;
	pthread_t thread;

	errnum = pthread_create_suspended_np(&thread, nullptr, thread_routine, arg);
	if (errnum != 0) {
		throw std::system_error(std::error_code(errnum, std::system_category()), "thread creation failed");
	}

	mach_port_t mach_thread = pthread_mach_thread_np(thread);
	pthread_detach(thread);

	thread_affinity_policy_data_t policy_data;
	if (policy.afinity) {
		int tag = 0;
		for (size_t core = 0; core < sizeof(policy.afinity) * 8; ++core) {
			if ((policy.afinity >> core) & 1) {
				tag = core / hardware_concurrency + 1;
				break;
			}
		}
		policy_data.affinity_tag = tag;
		if (thread_policy_set(mach_thread,
				THREAD_AFFINITY_POLICY, (thread_policy_t)&policy_data,
				THREAD_AFFINITY_POLICY_COUNT) != KERN_SUCCESS) {
			L_WARNING_ONCE("Cannot set thread affinity policy!");
		}
	}

	if (policy.priority) {
		thread_precedence_policy_data_t tppolicy;
		tppolicy.importance = policy.priority;
		if (thread_policy_set(mach_thread,
				THREAD_PRECEDENCE_POLICY, (thread_policy_t)&tppolicy,
				THREAD_PRECEDENCE_POLICY_COUNT) != KERN_SUCCESS) {
			L_WARNING_ONCE("Cannot set thread precedence policy!");
		}

		static double clock2abs = []{
			mach_timebase_info_data_t tbinfo;
			mach_timebase_info(&tbinfo);
			return ((double)tbinfo.denom / (double)tbinfo.numer) * 1000000;
		}();
		thread_time_constraint_policy_data_t ttcpolicy;
		ttcpolicy.period = 50 * clock2abs;
		ttcpolicy.computation = 1 * clock2abs;
		ttcpolicy.constraint = 2 * clock2abs;
		ttcpolicy.preemptible = TRUE;
		if (thread_policy_set(mach_thread,
				THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&ttcpolicy,
				THREAD_TIME_CONSTRAINT_POLICY_COUNT) != KERN_SUCCESS) {
			L_WARNING_ONCE("Cannot set thread time constraint policy!");
		}
	}

	thread_resume(mach_thread);

	return thread;
}
#else

#include <sched.h>

void
start_thread(void *(*thread_routine)(void *), void *arg, ThreadPolicyType thread_policy)
{
	static const unsigned int hardware_concurrency = std::thread::hardware_concurrency();

	ThreadPolicy policy(thread_policy);

	int errnum;
	pthread_t thread;

	pthread_attr_t thread_attr;
	errnum = pthread_attr_init(&thread_attr);
	if (errnum != 0) {
		throw std::system_error(std::error_code(errnum, std::system_category()), "thread creation failed");
	}

	if (policy.priority) {
		// [https://docs.oracle.com/cd/E19455-01/806-5257/attrib-16/index.html]
		sched_param param;
		if (pthread_attr_getschedparam(&thread_attr, &param) == 0) {
			param.sched_priority = policy.priority;
			if (pthread_attr_setschedparam(&thread_attr, &param) != 0) {
				L_WARNING_ONCE("Cannot set thread priority!");
			}
		}
	}

	if (policy.afinity) {
		// [https://stackoverflow.com/a/25494651/167522]
		cpu_set_t cpuset;
		CPU_ZERO(&cpuset);
		for (size_t core = 0; core < sizeof(policy.afinity) * 8; ++core) {
			if ((policy.afinity >> core) & 1) {
				CPU_SET(core / hardware_concurrency, &cpuset);
			}
		}
		if (pthread_attr_setaffinity_np(&thread_attr, sizeof(cpu_set_t), &cpuset) != 0) {
			L_WARNING_ONCE("Cannot set thread affinity!");
		}
	}

	errnum = pthread_create(&thread, &thread_attr, thread_routine, arg);
	if (errnum != 0) {
		throw std::system_error(std::error_code(errnum, std::system_category()), "thread creation failed");
	}

	pthread_attr_destroy(&thread_attr);

	pthread_detach(thread);
}
#endif


void
setup_thread(const std::string& name, ThreadPolicyType thread_policy)
{
	set_thread_name(name);
}


////////////////////////////////////////////////////////////////////////////////


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
