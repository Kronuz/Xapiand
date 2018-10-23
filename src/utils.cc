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

#include "utils.h"

#include <array>                 // for std::array
#include <algorithm>             // for equal, uniform_int_distribution
#include <cstdint>               // for uint64_t
#include <functional>            // for function, __base
#include <cmath>                 // for powl, logl, floorl, roundl
#include <memory>                // for allocator
#include <mutex>                 // for std::mutex
#include <netinet/in.h>          // for IPPROTO_TCP
#include <netinet/tcp.h>         // for TCP_NOPUSH
#include <random>                // for mt19937_64, random_device, uniform_r...
#include <ratio>                 // for ratio
#include <cstdio>                // for size_t, sprintf, remove, rename, snp...
#include <cstring>               // for strerror, strcmp
#include <string>                // for string, operator+, char_traits, basi...
#include <fcntl.h>               // for O_CREAT, O_RDONLY, O_WRONLY
#include <poll.h>                // for poll, pollfd, POLLNVAL
#include <sys/resource.h>        // for rlim_t, rlimit, RLIMIT_NOFILE, getrl...
#include <sys/socket.h>          // for setsockopt
#include <sys/stat.h>            // for mkdir, stat
#include <sysexits.h>            // for EX_OSFILE
#include <unistd.h>              // for close, rmdir, write, ssize_t
#include <unordered_map>         // for std::unordered_map

#include "config.h"              // for HAVE_PTHREAD_GETNAME_NP, HAVE_PTHR...
#include "exception.h"           // for Exit
#include "field_parser.h"        // for FieldParser, FieldParserError
#include "io_utils.h"            // for open, read
#include "log.h"                 // for L_ERR, L_WARNING, L_INFO
#include "namegen.h"             // for Generator
#include "stringified.hh"        // for stringified

#ifdef HAVE_PTHREADS
#include <pthread.h>             // for pthread_self
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>          // for pthread_getname_np
#endif
#endif

#ifdef HAVE_SYS_SYSCTL_H
#include <sys/sysctl.h>          // for sysctl, sysctlnametomib...
#endif

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


static std::random_device rd;  // Random device engine, usually based on /dev/random on UNIX-like systems
static std::mt19937_64 rng(rd()); // Initialize Mersennes' twister using rd to generate the seed

static std::unordered_map<std::thread::id, std::string> thread_names;
static std::mutex thread_names_mutex;


void set_thread_name(const std::string& name) {
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


const std::string& get_thread_name(std::thread::id thread_id) {
	std::lock_guard<std::mutex> lk(thread_names_mutex);
	auto thread = thread_names.find(thread_id);
	if (thread == thread_names.end()) {
		static std::string _ = "???";
		return _;
	}
	return thread->second;
}


const std::string& get_thread_name() {
	return get_thread_name(std::this_thread::get_id());
}


double random_real(double initial, double last) {
	std::uniform_real_distribution<double> distribution(initial, last);
	return distribution(rng);  // Use rng as a generator
}


uint64_t random_int(uint64_t initial, uint64_t last) {
	std::uniform_int_distribution<uint64_t> distribution(initial, last);
	return distribution(rng);  // Use rng as a generator
}


std::string name_generator() {
	static NameGen::Generator generator("!<s<v|V>(tia|nia|lia|cia|sia)|s<v|V>(os)|B<v|V>c(ios)|B<v|V><c|C>v(ios|os)>");
	return generator.toString();
}


char* normalize_path(const char* src, const char* end, char* dst, bool slashed) {
	int levels = 0;
	char* ret = dst;
	char ch = '\0';
	while (*src != '\0' && src < end) {
		ch = *src++;
		if (ch == '.' && (levels != 0 || dst == ret || *(dst - 1) == '/' )) {
			*dst++ = ch;
			++levels;
		} else if (ch == '/') {
			while (levels != 0 && dst > ret) {
				if (*--dst == '/') {
					levels -= 1;
				}
			}
			if (dst == ret || *(dst - 1) != '/') {
				*dst++ = ch;
			}
		} else {
			*dst++ = ch;
			levels = 0;
		}
	}
	if (slashed && ch != '/') {
		*dst++ = '/';
	}
	*dst++ = '\0';
	return ret;
}


char* normalize_path(std::string_view src, char* dst, bool slashed) {
	size_t src_size = src.size();
	const char* src_str = src.data();
	return normalize_path(src_str, src_str + src_size, dst, slashed);
}


std::string normalize_path(std::string_view src, bool slashed) {
	size_t src_size = src.size();
	const char* src_str = src.data();
	std::vector<char> dst;
	dst.resize(src_size + 2);
	return normalize_path(src_str, src_str + src_size, &dst[0], slashed);
}


bool strhasupper(std::string_view str) {
	for (const auto& c : str) {
		if (isupper(c) != 0) {
			return true;
		}
	}

	return false;
}


bool isRange(std::string_view str) {
	try {
		FieldParser fieldparser(str);
		fieldparser.parse();
		return fieldparser.is_range();
	} catch (const FieldParserError&) {
		return false;
	}
}


void delete_files(std::string_view path) {
	stringified path_string(path);
	DIR *dirp = ::opendir(path_string.c_str());
	if (dirp == nullptr) {
		return;
	}

	bool contains_folder = false;
	struct dirent *ent;
	while ((ent = ::readdir(dirp)) != nullptr) {
		const char *s = ent->d_name;
		if (ent->d_type == DT_DIR) {
			if (s[0] == '.' && (s[1] == '\0' || (s[1] == '.' && s[2] == '\0'))) {
				continue;
			}
			contains_folder = true;
		}
		if (ent->d_type == DT_REG) {
			std::string file(path);
			file.push_back('/');
			file.append(ent->d_name);
			if (::remove(file.c_str()) != 0) {
				L_ERR("File %s could not be deleted", ent->d_name);
			}
		}
	}

	closedir(dirp);
	if (!contains_folder) {
		if (::rmdir(path_string.c_str()) != 0) {
			L_ERR("Directory %s could not be deleted", path_string);
		}
	}
}


void move_files(std::string_view src, std::string_view dst) {
	stringified src_string(src);
	DIR *dirp = ::opendir(src_string.c_str());
	if (dirp == nullptr) {
		return;
	}

	struct dirent *ent;
	while ((ent = ::readdir(dirp)) != nullptr) {
		if (ent->d_type == DT_REG) {
			std::string old_name(src);
			old_name.push_back('/');
			old_name.append(ent->d_name);
			std::string new_name(dst);
			new_name.push_back('/');
			new_name.append(ent->d_name);
			if (::rename(old_name.c_str(), new_name.c_str()) != 0) {
				L_ERR("Couldn't rename %s to %s", old_name, new_name);
			}
		}
	}

	closedir(dirp);
	if (::rmdir(src_string.c_str()) != 0) {
		L_ERR("Directory %s could not be deleted", src_string);
	}
}


bool exists(std::string_view path) {
	struct stat buf;
	return ::stat(stringified(path).c_str(), &buf) == 0;
}


bool build_path(std::string_view path) {
	if (exists(path)) {
		return true;
	}
	Split<char> directories(path, '/');
	std::string dir;
	dir.reserve(path.size());
	if (path.front() == '/') {
		dir.push_back('/');
	}
	for (const auto& _dir : directories) {
		dir.append(_dir).push_back('/');
		if (::mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
			return false;
		}
	}
	return true;
}


bool build_path_index(std::string_view path_index) {
	size_t found = path_index.find_last_of('/');
	if (found == std::string_view::npos) {
		return build_path(path_index);
	}
	return build_path(path_index.substr(0, found));
}


DIR* opendir(std::string_view path, bool create) {
	stringified path_string(path);
	DIR* dirp = ::opendir(path_string.c_str());
	if (dirp == nullptr && errno == ENOENT && create) {
		if (::mkdir(path_string.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
			dirp = ::opendir(path_string.c_str());
		}
	}
	return dirp;
}


void find_file_dir(DIR* dir, File_ptr& fptr, std::string_view pattern, bool pre_suf_fix) {
	bool(*match_pattern)(std::string_view, std::string_view);
	if (pre_suf_fix) {
		match_pattern = string::startswith;
	} else {
		match_pattern = string::endswith;
	}

	if (fptr.ent != nullptr) {
#if defined(__APPLE__) && defined(__MACH__)
		seekdir(dir, fptr.ent->d_seekoff);
#elif defined(__FreeBSD__)
		seekdir(dir, telldir(dir));
#else
		seekdir(dir, fptr.ent->d_off);
#endif
	}

	while ((fptr.ent = ::readdir(dir)) != nullptr) {
		if (fptr.ent->d_type == DT_REG) {
			std::string_view filename(fptr.ent->d_name);
			if (match_pattern(filename, pattern)) {
				return;
			}
		}
	}
}


int copy_file(std::string_view src, std::string_view dst, bool create, std::string_view file_name, std::string_view new_name) {
	stringified src_string(src);
	DIR* dir_src = ::opendir(src_string.c_str());
	if (dir_src == nullptr) {
		L_ERR("ERROR: couldn't open directory %s: %s", strerror(errno));
		return -1;
	}

	struct stat buf;
	stringified dst_string(dst);
	int err = ::stat(dst_string.c_str(), &buf);

	if (err == -1) {
		if (ENOENT == errno && create) {
			if (::mkdir(dst_string.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
				L_ERR("ERROR: couldn't create directory %s: %s", dst_string, strerror(errno));
				return -1;
			}
		} else {
			L_ERR("ERROR: couldn't obtain directory information %s: %s", dst_string, strerror(errno));
			return -1;
		}
	}

	bool ended = false;
	struct dirent *ent;
	unsigned char buffer[4096];

	while ((ent = ::readdir(dir_src)) != nullptr and not ended) {
		if (ent->d_type == DT_REG) {

			if (not file_name.empty()) {
				if (file_name == ent->d_name) {
					ended = true;
				} else {
					continue;
				}
			}

			std::string src_path(src);
			src_path.push_back('/');
			src_path.append(ent->d_name);
			std::string dst_path(dst);
			dst_path.push_back('/');
			if (new_name.empty()) {
				dst_path.append(ent->d_name);
			} else {
				dst_path.append(new_name.data(), new_name.size());
			}

			int src_fd = io::open(src_path.c_str(), O_RDONLY);
			if (src_fd == -1) {
				L_ERR("ERROR: opening file. %s\n", src_path);
				return -1;
			}

			int dst_fd = io::open(dst_path.c_str(), O_CREAT | O_WRONLY, 0644);
			if (src_fd == -1) {
				L_ERR("ERROR: opening file. %s\n", dst_path);
				return -1;
			}

			while (true) {
				ssize_t bytes = io::read(src_fd, buffer, 4096);
				if (bytes == -1) {
					L_ERR("ERROR: reading file. %s: %s\n", src_path, strerror(errno));
					return -1;
				}

				if (bytes == 0) { break; }

				bytes = io::write(dst_fd, buffer, bytes);
				if (bytes == -1) {
					L_ERR("ERROR: writing file. %s: %s\n", dst_path, strerror(errno));
					return -1;
				}
			}
			io::close(src_fd);
			io::close(dst_fd);
		}
	}
	closedir(dir_src);
	return 0;
}


void _tcp_nopush(int sock, int optval) {
#ifdef TCP_NOPUSH
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_NOPUSH (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

#ifdef TCP_CORK
	if (io::setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) == -1) {
		L_ERR("ERROR: setsockopt TCP_CORK (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif
}


size_t get_max_files_per_proc()
{
	size_t rlimit_max_files;
	struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) == -1) {
        rlimit_max_files = 2;
    } else {
        rlimit_max_files = static_cast<size_t>(rl.rlim_cur);
    }
	long sysconf_max_files = sysconf(_SC_OPEN_MAX);
	if (sysconf_max_files == -1 || static_cast<size_t>(sysconf_max_files) < rlimit_max_files) {
		return rlimit_max_files;
	}
	return sysconf_max_files;
}


size_t get_open_max_fd()
{
#ifdef F_MAXFD
	int fcntl_open_max = io::unchecked_fcntl(0, F_MAXFD);
	if likely(fcntl_open_max != -1) {
		return fcntl_open_max;
	}
#endif
	return get_max_files_per_proc();
}


size_t get_open_files_per_proc()
{
	std::array<struct pollfd, OPEN_MAX> fds;
	size_t open_max_fd = get_open_max_fd();
	off_t off = 0;
	size_t cnt = 0;
	while (open_max_fd) {
		size_t nfds = (open_max_fd > OPEN_MAX) ? OPEN_MAX : open_max_fd;
		for (size_t idx = 0; idx < nfds; ++idx) {
			fds[idx].events = POLLSTANDARD;
			fds[idx].revents = 0;
			fds[idx].fd = idx + off;
		}
		int err = io::RetryAfterSignal(::poll, fds.data(), nfds, 0);
		if likely(err != -1) {
			for (size_t idx = 0; idx < nfds; ++idx) {
				if likely((fds[idx].revents & POLLNVAL) == 0) {
					++cnt;
					// char filePath[PATH_MAX];
					// if unlikely(io::unchecked_fcntl(fds[idx].fd, F_GETPATH, filePath) == -1) {
					// 	L_RED("%d -> %s (%d): %s", fds[idx].fd, io::strerrno(errno), errno, strerror(errno));
					// } else {
					// 	L_GREEN("%d -> %s", fds[idx].fd, filePath);
					// }
				}
			}
		}
		open_max_fd -= nfds;
		off += OPEN_MAX;
	}
	return cnt;
}


size_t get_open_files_system_wide()
{
	size_t max_files_per_proc = 0;

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
		L_ERR("ERROR: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, std::strerror(errno));
		return 0;
	}
#endif
#endif
#ifdef _SYSCTL_NAME
	auto max_files_per_proc_len = sizeof(max_files_per_proc);
	if (sysctl(mib, mib_len, &max_files_per_proc, &max_files_per_proc_len, nullptr, 0) < 0) {
		L_ERR("ERROR: Unable to get number of open files: sysctl(" _SYSCTL_NAME "): [%d] %s", errno, std::strerror(errno));
		return 0;
	}
#undef _SYSCTL_NAME
#elif defined(__linux__)
	int fd = io::open("/proc/sys/fs/file-nr", O_RDONLY);
	if unlikely(fd == -1) {
		L_ERR("ERROR: Unable to open /proc/sys/fs/file-nr: [%d] %s", errno, std::strerror(errno));
		return 0;
	}
	char line[100];
	ssize_t n = io::read(fd, line, sizeof(line));
	if unlikely(n == -1) {
		L_ERR("ERROR: Unable to read from /proc/sys/fs/file-nr: [%d] %s", errno, std::strerror(errno));
		return 0;
	}
	max_files_per_proc = atoi(line);
#else
	L_WARNING_ONCE("WARNING: No way of getting number of open files.");
#endif

	return max_files_per_proc;
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
