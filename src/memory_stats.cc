/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#include "memory_stats.h"

#include "log.h"                 // for L_ERR

#include <sys/param.h>           // for statfs, sysctl
#include <sys/mount.h>           // for statfs, sysctl
#include <sys/sysctl.h>          // for xsw_usage, sysctlnametomib, sysctl

#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/mach.h>           // for task_basic_info
#elif defined(__FreeBSD__)
#include <fcntl.h>
#include <kvm.h>
#include <sys/stat.h>
#include <unistd.h>              // for getpagesize
#endif


std::pair<int64_t, int64_t> get_current_ram()
{
#ifdef __APPLE__
	/* Total ram current in use */
	vm_size_t page_size;
	mach_port_t mach_port;
	mach_msg_type_number_t count;
	vm_statistics64_data_t vm_stats;

	mach_port = mach_host_self();
	count = sizeof(vm_stats) / sizeof(natural_t);
	if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
		KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count)) {
		int64_t free_memory = ((int64_t)vm_stats.free_count * (int64_t)page_size);
		int64_t used_memory = ((int64_t)vm_stats.active_count +
							   (int64_t)vm_stats.inactive_count +
							   (int64_t)vm_stats.wire_count) * (int64_t)page_size;
		return std::make_pair(used_memory, free_memory);
	}
#endif
	return std::make_pair(0, 0);
}


uint64_t get_total_virtual_used()
{
	uint64_t total_virtual_used = 0;

#if defined(VM_SWAPUSAGE)
#define _SYSCTL_NAME "vm.swapusage"  // Apple
	int mib[] = {CTL_VM, VM_SWAPUSAGE};
	size_t mib_len = sizeof(mib) / sizeof(int);
#endif
#ifdef _SYSCTL_NAME
	xsw_usage vmusage = {0, 0, 0, 0, false};
	size_t vmusage_len = sizeof(vmusage);
	if (sysctl(mib, mib_len, &vmusage, &vmusage_len, nullptr, 0) < 0) {
		L_ERR(nullptr, "ERROR: Unable to get swap usage: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
	} else {
		total_virtual_used = vmusage.xsu_used;
	}
#undef _SYSCTL_NAME
#else
	L_WARNING(nullptr, "WARNING: No way of getting swap usage.");
#endif

	return total_virtual_used;
}


uint64_t get_total_ram()
{
	uint64_t total_ram = 0;

#if defined(HW_REALMEM)
#define _SYSCTL_NAME "hw.realmem"  // FreeBSD
	int mib[] = {CTL_HW, HW_REALMEM};
	size_t mib_len = sizeof(mib) / sizeof(int);
#elif defined(HW_MEMSIZE)
#define _SYSCTL_NAME "hw.memsize"  // Apple
	int mib[] = {CTL_HW, HW_MEMSIZE};
	size_t mib_len = sizeof(mib) / sizeof(int);
#endif
#ifdef _SYSCTL_NAME
	auto total_ram_len = sizeof(total_ram);
	if (sysctl(mib, mib_len, &total_ram, &total_ram_len, nullptr, 0) < 0) {
		L_ERR(nullptr, "ERROR: Unable to get total memory size: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
	}
#undef _SYSCTL_NAME
#else
	L_WARNING(nullptr, "WARNING: No way of getting total memory size.");
#endif

	return total_ram;
}


uint64_t get_current_memory_by_process(bool resident)
{
	uint64_t current_memory_by_process;
#if defined(__FreeBSD__)
	char errbuf[100];
	struct kinfo_proc *p;
	int cnt;

	kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDWR, errbuf);
	p = kvm_getprocs(kd, KERN_PROC_PID | KERN_PROC_INC_THREAD, getpid(), &cnt);
	current_memory_by_process = p->ki_rssize * getpagesize();
#elif defined(__APPLE__)
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count)) {
		return 0;
	}
	if (resident) {
		current_memory_by_process = t_info.resident_size;
	} else {
		current_memory_by_process = t_info.virtual_size;
	}
#endif
	return current_memory_by_process;
}


uint64_t get_total_virtual_memory()
{
	uint64_t total_virtual_memory = 0;

#if defined(__APPLE__)
	struct statfs stats;
	if (0 == statfs("/", &stats)) {
		total_virtual_memory = (uint64_t)stats.f_bsize * stats.f_bfree;
	}
#elif defined(__FreeBSD__)
#define _SYSCTL_NAME "vm.stats.vm.v_page_count"  // FreeBSD
	int mib[CTL_MAXNAME + 2];
	size_t mib_len = sizeof(mib) / sizeof(int);
	if (sysctlnametomib(_SYSCTL_NAME, mib, &mib_len) < 0) {
		L_ERR(nullptr, "ERROR: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
		return 0;
	}
#endif
#ifdef _SYSCTL_NAME
	int64_t total_pages;
	auto total_pages_len = sizeof(total_pages);
	if (sysctl(mib, mib_len, &total_pages, &total_pages_len, nullptr, 0) < 0) {
		L_ERR(nullptr, "ERROR: Unable to get total virtual memory size: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
	} else {
		total_virtual_memory = total_pages * getpagesize();
	}
#undef _SYSCTL_NAME
#else
	L_WARNING(nullptr, "WARNING: No way of getting total virtual memory size.");
#endif

	return total_virtual_memory;
}
