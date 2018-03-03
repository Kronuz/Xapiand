/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "utils.h"

#include <algorithm>             // for equal, uniform_int_distribution
#include <cstdint>               // for uint64_t
#include <functional>            // for function, __base
#include <math.h>                // for powl, logl, floorl, roundl
#include <memory>                // for allocator
#include <netinet/in.h>          // for IPPROTO_TCP
#include <netinet/tcp.h>         // for TCP_NOPUSH
#include <random>                // for mt19937_64, random_device, uniform_r...
#include <ratio>                 // for ratio
#include <stdio.h>               // for size_t, sprintf, remove, rename, snp...
#include <string.h>              // for strerror, strcmp
#include <string>                // for string, operator+, char_traits, basi...
#include <sys/fcntl.h>           // for O_CREAT, O_RDONLY, O_WRONLY
#include <sys/resource.h>        // for rlim_t, rlimit, RLIMIT_NOFILE, getrl...
#include <sys/socket.h>          // for setsockopt
#include <sys/stat.h>            // for mkdir, stat
#include <sysexits.h>            // for EX_OSFILE
#include <thread>                // for this_thread
#include <unistd.h>              // for close, rmdir, write, ssize_t

#include "config.h"              // for HAVE_PTHREAD_GETNAME_NP_3, HAVE_PTHR...
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


#if !defined(HAVE_PTHREAD_GETNAME_NP_3) && !defined(HAVE_PTHREAD_GET_NAME_NP_3) && !defined(HAVE_PTHREAD_GET_NAME_NP_1)
#if defined (__FreeBSD__)
#if defined (HAVE_PTHREADS) && defined (HAVE_PTHREAD_NP_H)
#define HAVE_PTHREAD_GET_NAME_NP_2

#include <mutex>
#include <unordered_map>

#include <errno.h>

#include <stdlib.h>
#include <sys/sysctl.h>
#include <sys/user.h>

int
pthread_get_name_np(char* buffer, size_t size)
{
	int tid = pthread_getthreadid_np();

	static std::unordered_map<int, std::string> names;
	static std::mutex mtx;

	std::unique_lock<std::mutex> lk(mtx);

	auto it = names.find(tid);
	if (it == names.end()) {
		lk.unlock();
		if (!buffer) {
			return 1;
		}
		size_t kp_len = 0;
		struct kinfo_proc *kp = nullptr;
		while (true) {
			int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID | KERN_PROC_INC_THREAD, static_cast<int>(::getpid())};
			size_t mib_len = sizeof(mib) / sizeof(int);
			int error = sysctl(mib, mib_len, kp, &kp_len, nullptr, 0);
			if (kp == nullptr || (error < 0 && errno == ENOMEM)) {
				struct kinfo_proc *nkp = (struct kinfo_proc *)realloc(kp, kp_len);
				if (nkp == nullptr) {
					free(kp);
					return -1;
				}
				kp = nkp;
				continue;
			}
			if (error < 0) {
				kp_len = 0;
			}
			break;
		}
		auto items = kp_len / sizeof(*kp);
		lk.lock();
		if (std::abs(long(names.size() - items)) > 20) {
			names.clear();
		}
		for (size_t i = 0; i < items; i++) {
			auto k_tid = static_cast<int>(kp[i].ki_tid);
			auto oit = names.insert(std::make_pair(k_tid, kp[i].ki_tdname)).first;
			if (k_tid == tid) {
				it = oit;
			}
		}
		free(kp);
	} else {
		if (!buffer) {
			names.erase(it);
			return 1;
		}
	}
	if (it != names.end()) {
		strncpy(buffer, it->second.c_str(), size);
		return 0;
	}
	return -1;
}
#endif
#endif
#endif


void set_thread_name(std::string_view name) {
#if defined(HAVE_PTHREAD_SETNAME_NP_1)
	pthread_setname_np(stringified(name).c_str());
#elif defined(HAVE_PTHREAD_SETNAME_NP_2)
	pthread_setname_np(pthread_self(), stringified(name).c_str());
#elif defined(HAVE_PTHREAD_SETNAME_NP_3)
	pthread_setname_np(pthread_self(), stringified(name).c_str(), nullptr);
#elif defined(HAVE_PTHREAD_SET_NAME_NP_2)
	pthread_set_name_np(pthread_self(), stringified(name).c_str());
#endif
#if defined(HAVE_PTHREAD_GET_NAME_NP_2)
	pthread_get_name_np(nullptr, 0);
#endif
}


std::string get_thread_name() {
	char name[100] = {0};
#if defined(HAVE_PTHREAD_GETNAME_NP_3)
	pthread_getname_np(pthread_self(), name, sizeof(name));
#elif defined(HAVE_PTHREAD_GET_NAME_NP_3)
	pthread_get_name_np(pthread_self(), name, sizeof(name));
#elif defined(HAVE_PTHREAD_GET_NAME_NP_1)
	strncpy(name, pthread_get_name_np(pthread_self()), sizeof(name));
#elif defined(HAVE_PTHREAD_GET_NAME_NP_2)
	pthread_get_name_np(name, sizeof(name));
#else
	static std::hash<std::thread::id> thread_hasher;
	snprintf(name, sizeof(name), "%zx", thread_hasher(std::this_thread::get_id()));
#endif
	return std::string(name);
}


double random_real(double initial, double last) {
	std::uniform_real_distribution<double> distribution(initial, last);
	return distribution(rng);  // Use rng as a generator
}


uint64_t random_int(uint64_t initial, uint64_t last) {
	std::uniform_int_distribution<uint64_t> distribution(initial, last);
	return distribution(rng);  // Use rng as a generator
}


std::string repr(const void* p, size_t size, bool friendly, char quote, size_t max_size) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const char* q = static_cast<const char *>(p);
	const char *p_end = q + size;
	const char *max_a = max_size ? q + (max_size * 2 / 3) : p_end + 1;
	const char *max_b = max_size ? p_end - (max_size / 3) : q - 1;
	if (max_size) size = ((max_a - q) + (p_end - max_b) - 1) * 4 + 2 + 3;  // Consider "\xNN", two quotes and '...'
	else size = size * 4 + 2;  // Consider "\xNN" and two quotes
	std::string ret;
	ret.resize(size);
	char *buff = &ret[0];
	char *d = buff;
	if (quote == '\1') quote = '\'';
	if (quote) *d++ = quote;
	while (q != p_end) {
		unsigned char c = *q++;
		if (q >= max_a && q <= max_b) {
			if (q == max_a) {
				*d++ = '.';
				*d++ = '.';
				*d++ = '.';
			}
		} else if (friendly) {
			switch (c) {
				// case '\a':
				// 	*d++ = '\\';
				// 	*d++ = 'a';
				// 	break;
				// case '\b':
				// 	*d++ = '\\';
				// 	*d++ = 'b';
				// 	break;
				// case '\f':
				// 	*d++ = '\\';
				// 	*d++ = 'f';
				// 	break;
				// case '\v':
				// 	*d++ = '\\';
				// 	*d++ = 'v';
				// 	break;
				case '\n':
					*d++ = '\\';
					*d++ = 'n';
					break;
				case '\r':
					*d++ = '\\';
					*d++ = 'r';
					break;
				case '\t':
					*d++ = '\\';
					*d++ = 't';
					break;
				case '\\':
					*d++ = '\\';
					*d++ = '\\';
					break;
				default:
					if (quote && c == quote) {
						*d++ = '\\';
						*d++ = quote;
					} else if (c < ' ' || c >= 0x7f) {
						sprintf(d, "\\x%02x", c);
						d += 4;
					} else {
						*d++ = c;
					}
					break;
			}
		} else {
			sprintf(d, "\\x%02x", c);
			d += 4;
		}
		// fprintf(stderr, "%02x: %ld < %ld\n", c, (unsigned long)(d - buff), (unsigned long)(size));
	}
	if (quote) *d++ = quote;
	ret.resize(d - buff);
	return ret;
}

std::string escape(const void* p, size_t size, char quote) {
	assert(quote == '\0' || quote == '\1' || quote == '\'' || quote == '"');
	const char* q = static_cast<const char *>(p);
	const char *p_end = q + size;
	size = size * 4 + 2;  // Consider "\xNN" and two quotes
	std::string ret;
	ret.resize(size);  // Consider "\xNN" and quotes
	char *buff = &ret[0];
	char *d = buff;
	if (quote == '\1') quote = '\'';
	if (quote) *d++ = quote;
	while (q != p_end) {
		unsigned char c = *q++;
		switch (c) {
			// case '\a':
			// 	*d++ = '\\';
			// 	*d++ = 'a';
			// 	break;
			// case '\b':
			// 	*d++ = '\\';
			// 	*d++ = 'b';
			// 	break;
			// case '\f':
			// 	*d++ = '\\';
			// 	*d++ = 'f';
			// 	break;
			// case '\v':
			// 	*d++ = '\\';
			// 	*d++ = 'v';
			// 	break;
			case '\n':
				*d++ = '\\';
				*d++ = 'n';
				break;
			case '\r':
				*d++ = '\\';
				*d++ = 'r';
				break;
			case '\t':
				*d++ = '\\';
				*d++ = 't';
				break;
			case '\\':
				*d++ = '\\';
				*d++ = '\\';
				break;
			default:
				if (quote && c == quote) {
					*d++ = '\\';
					*d++ = quote;
				} else if (c < ' ' || c >= 0x7f) {
					sprintf(d, "\\x%02x", c);
					d += 4;
				} else {
					*d++ = c;
				}
				break;
		}
		// fprintf(stderr, "%02x: %ld < %ld\n", c, (unsigned long)(d - buff), (unsigned long)(size));
	}
	if (quote) *d++ = quote;
	ret.resize(d - buff);
	return ret;
}


int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets) {
	/* It outputs a bucket number in the range [0, num_buckets).
	   A Fast, Minimal Memory, Consistent Hash Algorithm
	   [http://arxiv.org/pdf/1406.2294v1.pdf] */
	int64_t b = 0, j = 0;
	while (j < num_buckets) {
		b = j;
		key = key * 2862933555777941757ULL + 1;
		j = (b + 1) * (double(1LL << 31) / double((key >> 33) + 1));
	}
	return (int32_t) b;
}


std::string name_generator() {
	static NameGen::Generator generator("!<K|E><k|e|l><|||s>");
	return generator.toString();
}


char* normalize_path(const char* src, const char* end, char* dst, bool slashed) {
	int levels = 0;
	char* ret = dst;
	char ch = '\0';
	while (*src && src < end) {
		ch = *src++;
		if (ch == '.' && (levels || dst == ret || *(dst - 1) == '/' )) {
			*dst++ = ch;
			++levels;
		} else if (ch == '/') {
			while (levels && dst > ret) {
				if (*--dst == '/') levels -= 1;
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
		if (isupper(c)) {
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
	if (!dirp) {
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
			L_ERR("Directory %s could not be deleted", path_string.c_str());
		}
	}
}


void move_files(std::string_view src, std::string_view dst) {
	stringified src_string(src);
	DIR *dirp = ::opendir(src_string.c_str());
	if (!dirp) {
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
				L_ERR("Couldn't rename %s to %s", old_name.c_str(), new_name.c_str());
			}
		}
	}

	closedir(dirp);
	if (::rmdir(src_string.c_str()) != 0) {
		L_ERR("Directory %s could not be deleted", src_string.c_str());
	}
}


bool exists(std::string_view path) {
	struct stat buf;
	return ::stat(stringified(path).c_str(), &buf) == 0;
}


bool build_path(std::string_view path) {
	if (exists(path)) {
		return true;
	} else {
		Split<char> directories(path, '/');
		std::string dir;
		dir.reserve(path.size());
		for (const auto& _dir : directories) {
			dir.append(_dir).push_back('/');
			if (::mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
				return false;
			}
		}
		return true;
	}
}


bool build_path_index(std::string_view path_index) {
	size_t found = path_index.find_last_of('/');
	if (found == std::string_view::npos) {
		return build_path(path_index);
	} else {
		return build_path(path_index.substr(0, found));
	}
}


DIR* opendir(std::string_view path, bool create) {
	stringified path_string(path);
	DIR* dirp = ::opendir(path_string.c_str());
	if (!dirp) {
		if (errno == ENOENT && create) {
			if (::mkdir(path_string.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
				return nullptr;
			} else {
				dirp = ::opendir(path_string.c_str());
				if (!dirp) {
					return nullptr;
				}
			}
		} else {
			return nullptr;
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

	if (fptr.ent) {
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
	if (!dir_src) {
		L_ERR("ERROR: couldn't open directory %s: %s", strerror(errno));
		return -1;
	}

	struct stat buf;
	stringified dst_string(dst);
	int err = ::stat(dst_string.c_str(), &buf);

	if (-1 == err) {
		if (ENOENT == errno && create) {
			if (::mkdir(dst_string.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
				L_ERR("ERROR: couldn't create directory %s: %s", dst_string.c_str(), strerror(errno));
				return -1;
			}
		} else {
			L_ERR("ERROR: couldn't obtain directory information %s: %s", dst_string.c_str(), strerror(errno));
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
			if (-1 == src_fd) {
				L_ERR("ERROR: opening file. %s\n", src_path.c_str());
				return -1;
			}

			int dst_fd = io::open(dst_path.c_str(), O_CREAT | O_WRONLY, 0644);
			if (-1 == src_fd) {
				L_ERR("ERROR: opening file. %s\n", dst_path.c_str());
				return -1;
			}

			while (1) {
				ssize_t bytes = io::read(src_fd, buffer, 4096);
				if (-1 == bytes) {
					L_ERR("ERROR: reading file. %s: %s\n", src_path.c_str(), strerror(errno));
					return -1;
				}

				if (0 == bytes) break;

				bytes = write(dst_fd, buffer, bytes);
				if (-1 == bytes) {
					L_ERR("ERROR: writing file. %s: %s\n", dst_path.c_str(), strerror(errno));
					return -1;
				}
			}
			close(src_fd);
			close(dst_fd);
		}
	}
	closedir(dir_src);
	return 0;
}


void _tcp_nopush(int sock, int optval) {
#ifdef TCP_NOPUSH
	if (setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) < 0) {
		L_ERR("ERROR: setsockopt TCP_NOPUSH (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

#ifdef TCP_CORK
	if (setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) < 0) {
		L_ERR("ERROR: setsockopt TCP_CORK (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif
}


/*
 * From http://stackoverflow.com/questions/17088204/number-of-open-file-in-a-c-program
 */
unsigned long long file_descriptors_cnt() {
	unsigned long long fdmax;
	struct rlimit limit;
	if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
		fdmax = 4096;
	} else {
		fdmax = static_cast<unsigned long long>(limit.rlim_cur);
	}
	unsigned long long n = 0;
	for (unsigned long long fd = 0; fd < fdmax; ++fd) {
		struct stat buf;
		if (fstat(fd, &buf)) {
			// errno should be EBADF (not a valid open file descriptor)
			// or other error. In either case, don't count.
			continue;
		}
		++n;
		// char filePath[PATH_MAX];
		// if (fcntl(fd, F_GETPATH, filePath) != -1) {
		// 	fprintf(stderr, "%llu - %s\n", fd, filePath);
		// }
	}
	return n;
}
