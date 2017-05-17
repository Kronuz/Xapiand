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
#include <sys/sysctl.h>          // for xsw_usage, sysctlbyname, sysctl

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


uint64_t get_total_ram() {
	int mib[2];
	int64_t physical_memory;
	mib[0] = CTL_HW;
#if defined(__APPLE__)
	mib[1] = HW_MEMSIZE;
#elif defined(__FreeBSD__)
	mib[1] = HW_REALMEM;
#endif
	auto length = sizeof(int64_t);
	sysctl(mib, 2, &physical_memory, &length, NULL, 0);
	return physical_memory;
}

#if defined(__APPLE__)
std::pair<int64_t, int64_t> get_current_ram() {
	/* Total ram current in use */
	vm_size_t page_size;
	mach_port_t mach_port;
	mach_msg_type_number_t count;
	vm_statistics64_data_t vm_stats;

	mach_port = mach_host_self();
	count = sizeof(vm_stats) / sizeof(natural_t);
	if (KERN_SUCCESS == host_page_size(mach_port, &page_size) &&
		KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO, (host_info64_t)&vm_stats, &count)) {
		int64_t free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
		int64_t used_memory = ((int64_t)vm_stats.active_count +
								 (int64_t)vm_stats.inactive_count +
								 (int64_t)vm_stats.wire_count) *  (int64_t)page_size;
		return std::make_pair(used_memory, free_memory);
	}
	return std::make_pair(0, 0);
}


uint64_t get_total_virtual_used() {
	xsw_usage vmusage = {0, 0, 0, 0, false};
	size_t size = sizeof(vmusage);
	if (sysctlbyname("vm.swapusage", &vmusage, &size, NULL, 0) != 0) {
		L_ERR(nullptr, "Unable to get swap usage by calling sysctlbyname(\"vm.swapusage\",...)");
	}
	return vmusage.xsu_used;
}

#endif


uint64_t get_current_memory_by_process(bool resident) {
#if defined(__APPLE__)
	struct task_basic_info t_info;
	mach_msg_type_number_t t_info_count = TASK_BASIC_INFO_COUNT;

	if (KERN_SUCCESS != task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&t_info, &t_info_count)) {
		return 0;
	}
	if (resident) {
		return  t_info.resident_size;
	} else {
		return  t_info.virtual_size;
	}
#elif defined(__FreeBSD__)
	char errbuf[100];
    struct kinfo_proc *p;
    int cnt;

    kvm_t *kd = kvm_openfiles(NULL, NULL, NULL, O_RDWR, errbuf);
    p = kvm_getprocs(kd, KERN_PROC_PID | KERN_PROC_INC_THREAD, getpid(), &cnt);
    return p->ki_rssize * getpagesize();
#endif
}


uint64_t get_total_virtual_memory() {
#if defined(__APPLE__)
	uint64_t myFreeSwap = 0;
	struct statfs stats;
	if (0 == statfs("/", &stats)) {
		myFreeSwap = (uint64_t)stats.f_bsize * stats.f_bfree;
	}
	return myFreeSwap;
#elif defined(__FreeBSD__)
	int total_pages = 0;
    size_t len = sizeof(total_pages);
    if (sysctlbyname("vm.stats.vm.v_page_count", &total_pages, &len, NULL, 0) != 0) {
        L_ERR(nullptr, "Unable to get v_page count by calling sysctlbyname\n");
    }
    return total_pages * getpagesize();
#endif
}
