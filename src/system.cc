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

#include "system.hh"

#include "config.h"                 // for HAVE_SYS_SYSCTL_H

#include <array>                    // for std::array
#include <errno.h>                  // for errno
#include <fcntl.h>                  // for O_CREAT, O_RDONLY, O_WRONLY
#include <poll.h>                   // for poll, pollfd, POLLNVAL
#include <sys/resource.h>           // for getrlimit, rlimit, RLIMIT_NOFILE...
#include <unistd.h>                 // for sysconf

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>             // for sysctl, sysctlnametomib...
#endif

#include "error.hh"                 // for error:name, error::description
#include "io.hh"                    // for io::*
#include "log.h"                    // for L_ERR, L_WARNING, L_INFO
#include "likely.h"                 // for likely, unlikely


#ifndef OPEN_MAX
#define OPEN_MAX 10240
#endif
#ifndef POLLSTANDARD
#define POLLSTANDARD (POLLIN|POLLPRI|POLLOUT|POLLRDNORM|POLLRDBAND|POLLWRBAND|POLLERR|POLLHUP|POLLNVAL)
#endif

#define STATE_ERR_UNEXPECTED_SLASH_UPL -10
#define STATE_ERR_UNEXPECTED_SLASH_CMD -9
#define STATE_ERR_UNEXPECTED_AT -8
#define STATE_ERR_UNEXPECTED_AT_NSP -7
#define STATE_ERR_UNEXPECTED_COLON -6
#define STATE_ERR_UNEXPECTED_COLON_NSP -5
#define STATE_ERR_UNEXPECTED_COMMA_UPL -4
#define STATE_ERR_UNEXPECTED_END_UPL -4
#define STATE_ERR_UNEXPECTED_COMMA_HST -3
#define STATE_ERR_UNEXPECTED_END_HST -3
#define STATE_ERR_UNEXPECTED_COMMA_PTH -2
#define STATE_ERR_UNEXPECTED_END_PTH -2
#define STATE_ERR_NO_SLASH -1
#define STATE_CM0 0
#define STATE_CMD 1
#define STATE_PMT 2 /* case parameter operation if exist could be _upload or _stats */
#define STATE_NSP 3
#define STATE_PTH 4
#define STATE_HST 5


std::size_t get_max_files_per_user()
{
	// The maximum number of open files per user id:
	long max_files_per_user = ::sysconf(_SC_OPEN_MAX);
	if (max_files_per_user == -1) {
		L_WARNING("ERROR: Unable to get number of max files per user, assuming 1024: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 1024;
	}
	return max_files_per_user;
}


std::size_t get_max_files_per_proc()
{
	auto max_files_per_proc = get_max_files_per_user();

	struct rlimit rl;
	if (::getrlimit(RLIMIT_NOFILE, &rl) == -1) {
		L_ERR("ERROR: Unable to obtain the current NOFILE limit, assuming {}: getrlimit(RLIMIT_NOFILE): {} ({}): {}", max_files_per_proc, error::name(errno), errno, error::description(errno));
		return max_files_per_proc;
	}
	if (rl.rlim_cur < max_files_per_proc) {
		max_files_per_proc = static_cast<std::size_t>(rl.rlim_cur);
	}
	return max_files_per_proc;
}


std::size_t get_open_max_fd()
{
#ifdef F_MAXFD
	int fcntl_open_max = io::unchecked_fcntl(0, F_MAXFD);
	if likely(fcntl_open_max != -1) {
		return fcntl_open_max;
	}
#endif
	return get_max_files_per_proc();
}


std::size_t get_open_files_per_proc()
{
	std::array<struct pollfd, OPEN_MAX> fds;
	std::size_t open_max_fd = get_open_max_fd();
	off_t off = 0;
	std::size_t cnt = 0;
	while (open_max_fd) {
		std::size_t nfds = (open_max_fd > OPEN_MAX) ? OPEN_MAX : open_max_fd;
		for (std::size_t idx = 0; idx < nfds; ++idx) {
			fds[idx].events = POLLSTANDARD;
			fds[idx].revents = 0;
			fds[idx].fd = idx + off;
		}
		int err = io::RetryAfterSignal(::poll, fds.data(), nfds, 0);
		if likely(err != -1) {
			for (std::size_t idx = 0; idx < nfds; ++idx) {
				if likely((fds[idx].revents & POLLNVAL) == 0) {
					++cnt;
					// char filePath[PATH_MAX];
					// if unlikely(io::unchecked_fcntl(fds[idx].fd, F_GETPATH, filePath) == -1) {
					// 	L_RED("{} -> {} ({}): {}", fds[idx].fd, error::name(errno), errno, error::description(errno));
					// } else {
					// 	L_GREEN("{} -> {}", fds[idx].fd, filePath);
					// }
				}
			}
		}
		open_max_fd -= nfds;
		off += OPEN_MAX;
	}
	return cnt;
}


std::size_t get_open_files_system_wide()
{
	std::size_t open_files_system_wide = 0;

#ifdef HAVE_SYS_SYSCTL_H
#if defined(KERN_OPENFILES)
#define _SYSCTL_NAME "kern.openfiles"  // FreeBSD
	int mib[] = {CTL_KERN, KERN_OPENFILES};
	std::size_t mib_len = sizeof(mib) / sizeof(int);
#elif defined(__APPLE__)
#define _SYSCTL_NAME "kern.num_files"  // Apple
	int mib[CTL_MAXNAME + 2];
	std::size_t mib_len = sizeof(mib) / sizeof(int);
	if (sysctlnametomib(_SYSCTL_NAME, mib, &mib_len) < 0) {
		L_ERR("ERROR: sysctl(" _SYSCTL_NAME "): {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
#endif
#endif
#ifdef _SYSCTL_NAME
	auto open_files_system_wide_len = sizeof(open_files_system_wide);
	if (sysctl(mib, mib_len, &open_files_system_wide, &open_files_system_wide_len, nullptr, 0) < 0) {
		L_ERR("ERROR: Unable to get number of open files: sysctl(" _SYSCTL_NAME "): {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
#undef _SYSCTL_NAME
#elif defined(__linux__)
	int fd = io::open("/proc/sys/fs/file-nr", O_RDONLY);
	if unlikely(fd == -1) {
		L_ERR("ERROR: Unable to open /proc/sys/fs/file-nr: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
	char line[100];
	ssize_t n = io::read(fd, line, sizeof(line));
	if unlikely(n == -1) {
		L_ERR("ERROR: Unable to read from /proc/sys/fs/file-nr: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
	open_files_system_wide = atoi(line);
#else
	L_WARNING_ONCE("WARNING: No way of getting number of open files.");
#endif

	return open_files_system_wide;
}


std::size_t get_max_files_system_wide()
{
	std::size_t max_files_system_wide = 0;

#ifdef HAVE_SYS_SYSCTL_H
#if defined(KERN_OPENFILES)
#define _SYSCTL_NAME "kern.maxfiles"  // FreeBSD
	int mib[] = {CTL_KERN, KERN_OPENFILES};
	std::size_t mib_len = sizeof(mib) / sizeof(int);
#elif defined(__APPLE__)
#define _SYSCTL_NAME "kern.maxfiles"  // Apple
	int mib[CTL_MAXNAME + 2];
	std::size_t mib_len = sizeof(mib) / sizeof(int);
	if (sysctlnametomib(_SYSCTL_NAME, mib, &mib_len) < 0) {
		L_ERR("ERROR: sysctl(" _SYSCTL_NAME "): {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
#endif
#endif
#ifdef _SYSCTL_NAME
	auto max_files_system_wide_len = sizeof(max_files_system_wide);
	if (sysctl(mib, mib_len, &max_files_system_wide, &max_files_system_wide_len, nullptr, 0) < 0) {
		L_ERR("ERROR: Unable to get number of open files: sysctl(" _SYSCTL_NAME "): {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
#undef _SYSCTL_NAME
#elif defined(__linux__)
	int fd = io::open("/proc/sys/fs/file-nr", O_RDONLY);
	if unlikely(fd == -1) {
		L_ERR("ERROR: Unable to open /proc/sys/fs/file-nr: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
	char line[100];
	ssize_t n = io::read(fd, line, sizeof(line));
	if unlikely(n == -1) {
		L_ERR("ERROR: Unable to read from /proc/sys/fs/file-nr: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
	char* field = line;
	field = strchar(field, '\t');
	if (field) field = strchar(field, '\t');
	if (field) max_files_system_wide = atoi(field);
	else {
		L_ERR("ERROR: Unable to retrieve data from /proc/sys/fs/file-nr: {} ({}): {}", error::name(errno), errno, error::description(errno));
		return 0;
	}
#else
	L_WARNING_ONCE("WARNING: No way of getting number of open files.");
#endif

	return max_files_system_wide;
}


std::string check_compiler() {
#ifdef _MSC_VER
	return "Visual Studio";
#elif __clang__
	return "clang";
#elif __GNUC__
	return "gcc";
#else
	return "Unknown compiler";
#endif
}


std::string check_OS() {
#ifdef _WIN32
	return "Windows 32-bit";
#elif _WIN64
	return "Windows 64-bit";
#elif __unix || __unix__
	return "Unix";
#elif __APPLE__ || __MACH__
	return "Mac OSX";
#elif __linux__
	return "Linux";
#elif __FreeBSD__
	return "FreeBSD";
#else
	return "Unknown OS";
#endif
}


std::string check_architecture() {
#ifdef __i386__
	return "i386";
#elif __x86_64__
	return "x86_64";
#elif __powerpc64__
	return "powerpc64";
#elif __aarch64__
	return "aarch64";
#else
	return "Unknown architecture";
#endif
}
