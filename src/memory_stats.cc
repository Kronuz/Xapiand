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

#include "memory_stats.h"

#include "config.h"

#include "log.h"                 // for L_ERR

#include <errno.h>               // for errno, strerror
#include <sys/param.h>           // for statfs
#include <sys/mount.h>           // for statfs
#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>          // for xsw_usage, sysctlnametomib, sysctl
#endif
#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/mach.h>           // for task_basic_info
#elif defined(__FreeBSD__)
#include <fcntl.h>
#include <unistd.h>              // for getpagesize
#elif defined(__linux__)
#include <unistd.h>
#include <ios>
#include <iostream>
#include <fstream>
#include <string>
#include <sys/sysinfo.h>         // for sysinfo
#include <sys/statvfs.h>         // for statvfs
#include <sys/vfs.h>             // for statfs
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

#ifdef HAVE_SYS_SYSCTL_H
#if defined(VM_SWAPUSAGE)
#define _SYSCTL_NAME "vm.swapusage"  // Apple
	int mib[] = {CTL_VM, VM_SWAPUSAGE};
	size_t mib_len = sizeof(mib) / sizeof(int);
#endif
#endif
#ifdef _SYSCTL_NAME
	xsw_usage vmusage = {0, 0, 0, 0, 0u};
	size_t vmusage_len = sizeof(vmusage);
	if (sysctl(mib, mib_len, &vmusage, &vmusage_len, nullptr, 0) < 0) {
		L_ERR("ERROR: Unable to get swap usage: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_virtual_used = vmusage.xsu_used;
#undef _SYSCTL_NAME
#else
	L_WARNING_ONCE("WARNING: No way of getting swap usage.");
#endif

	return total_virtual_used;
}


uint64_t get_total_ram()
{
	uint64_t total_ram = 0;

#ifdef HAVE_SYS_SYSCTL_H
#if defined(HW_REALMEM)
#define _SYSCTL_NAME "hw.realmem"  // FreeBSD
	int mib[] = {CTL_HW, HW_REALMEM};
	size_t mib_len = sizeof(mib) / sizeof(int);
#elif defined(HW_MEMSIZE)
#define _SYSCTL_NAME "hw.memsize"  // Apple
	int mib[] = {CTL_HW, HW_MEMSIZE};
	size_t mib_len = sizeof(mib) / sizeof(int);
#endif
#endif
#ifdef _SYSCTL_NAME
	auto total_ram_len = sizeof(total_ram);
	if (sysctl(mib, mib_len, &total_ram, &total_ram_len, nullptr, 0) < 0) {
		L_ERR("ERROR: Unable to get total memory size: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
		return 0;
	}
#undef _SYSCTL_NAME
#elif defined(__linux__)
	struct sysinfo info;
	if (sysinfo(&info) < 0) {
		L_ERR("ERROR: Unable to get total memory size: sysinfo(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_ram = info.totalram;
#else
	L_WARNING_ONCE("WARNING: No way of getting total memory size.");
#endif

	return total_ram;
}


uint64_t get_current_memory_by_process(bool resident)
{
	uint64_t current_memory_by_process = 0;
#if defined(__APPLE__)
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
#elif defined(__linux__)
	using std::ios_base;
	using std::ifstream;
	using std::string;

	// 'file' stat seems to give the most reliable results
	ifstream stat_stream("/proc/self/stat",ios_base::in);

	// dummy vars for leading entries in stat that we don't care about
	string pid, comm, state, ppid, pgrp, session, tty_nr;
	string tpgid, flags, minflt, cminflt, majflt, cmajflt;
	string utime, stime, cutime, cstime, priority, nice;
	string O, itrealvalue, starttime;

	// the two fields we want
	unsigned long vsize;
	long rss;

	stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
				>> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
				>> utime >> stime >> cutime >> cstime >> priority >> nice
				>> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

	stat_stream.close();
	long page_size_kb = sysconf(_SC_PAGE_SIZE); // in case x86-64 is configured to use 2MB pages
	if (resident) {
		current_memory_by_process = rss * page_size_kb;
	} else {
		current_memory_by_process = vsize;
	}
#else
	L_WARNING_ONCE("WARNING: No way of getting total %s memory size by the process.", resident ? "resident" : "virtual");
#endif
	return current_memory_by_process;
}


uint64_t get_total_virtual_memory()
{
	uint64_t total_virtual_memory = 0;

#ifdef HAVE_SYS_SYSCTL_H
#if defined(__FreeBSD__)
#define _SYSCTL_NAME "vm.stats.vm.v_page_count"  // FreeBSD
	int mib[CTL_MAXNAME + 2];
	size_t mib_len = sizeof(mib) / sizeof(int);
	if (sysctlnametomib(_SYSCTL_NAME, mib, &mib_len) < 0) {
		L_ERR("ERROR: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
		return 0;
	}
#endif
#endif
#ifdef _SYSCTL_NAME
	int64_t total_pages;
	auto total_pages_len = sizeof(total_pages);
	if (sysctl(mib, mib_len, &total_pages, &total_pages_len, nullptr, 0) < 0) {
		L_ERR("ERROR: Unable to get total virtual memory size: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_virtual_memory = total_pages * getpagesize();
#undef _SYSCTL_NAME
#elif defined(__APPLE__)
	struct statfs stats;
	if (0 != statfs("/", &stats)) {
		L_ERR("ERROR: Unable to get total virtual memory size: statfs(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_virtual_memory = (uint64_t)stats.f_bsize * stats.f_bfree;
#elif defined(__linux__)
	struct sysinfo info;
	if (sysinfo(&info) < 0) {
		L_ERR("ERROR: Unable to get total virtual memory size: sysinfo(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_virtual_memory = info.totalswap;
#else
	L_WARNING_ONCE("WARNING: No way of getting total virtual memory size.");
#endif

	return total_virtual_memory;
}


uint64_t get_total_inodes()
{
	uint64_t total_inodes = 0;
#if defined(__APPLE__)
	struct statfs statf;
	if (statfs(".", &statf) < 0) {
		L_ERR("ERROR: Unable to get total inodes statfs(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_inodes = statf.f_files;
#elif defined(__linux__)
	struct statvfs info;
	if (statvfs(".", &info) < 0) {
		L_ERR("ERROR: Unable to get total inodes statvfs(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_inodes = info.f_files;
#else
	L_WARNING_ONCE("WARNING: No way of getting total inodes");
#endif
	return total_inodes;
}


uint64_t get_free_inodes()
{
	uint64_t free_inodes = 0;
#if defined(__APPLE__)
	struct statfs statf;
	if (statfs(".", &statf) < 0) {
		L_ERR("ERROR: Unable to get free inodes statfs(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	free_inodes = statf.f_ffree;
#elif defined(__linux__)
	struct statvfs info;
	if (statvfs(".", &info) < 0) {
		L_ERR("ERROR: Unable to get free inodes statvfs(): [%d] %s", errno, strerror(errno));
		return 0;
	}
	free_inodes = info.f_ffree;
#else
	L_WARNING_ONCE("WARNING: No way of getting free inodes");
#endif
	return free_inodes;
}


uint64_t get_total_disk_size()
{
	uint64_t total_disk_size = 0;
#if defined(__APPLE__)|| defined(__linux__)
	struct statfs statf;
	if (statfs(".", &statf) < 0) {
		L_ERR("ERROR: Unable to get total disk size (): [%d] %s", errno, strerror(errno));
		return 0;
	}
	total_disk_size = statf.f_blocks * statf.f_bsize;
#else
	L_WARNING_ONCE("WARNING: No way of getting total disk size");
#endif
	return total_disk_size;
}

uint64_t get_free_disk_size()
{
	uint64_t free_disk_size = 0;
#if defined(__APPLE__)|| defined(__linux__)
	struct statfs statf;
	if (statfs(".", &statf) < 0) {
		L_ERR("ERROR: Unable to get free disk size (): [%d] %s", errno, strerror(errno));
		return 0;
	}
	free_disk_size = statf.f_bfree * statf.f_bsize;
#else
	L_WARNING_ONCE("WARNING: No way of getting free disk size");
#endif
	return free_disk_size;
}