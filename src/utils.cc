/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include <math.h>                // for powl, logl, floorl, roundl
#include <netinet/in.h>          // for IPPROTO_TCP
#include <netinet/tcp.h>         // for TCP_NOPUSH
#include <stdio.h>               // for size_t, sprintf, remove, rename, snp...
#include <string.h>              // for strerror, strcmp
#include <sys/fcntl.h>           // for O_CREAT, O_RDONLY, O_WRONLY
#include <sys/resource.h>        // for rlim_t, rlimit, RLIMIT_NOFILE, getrl...
#include <sys/socket.h>          // for setsockopt
#include <sys/stat.h>            // for mkdir, stat
#include <sysexits.h>            // for EX_OSFILE
#include <unistd.h>              // for close, rmdir, write, ssize_t
#include <algorithm>             // for equal, uniform_int_distribution
#include <cstdint>               // for uint64_t
#include <functional>            // for function, __base
#include <memory>                // for allocator
#include <random>                // for mt19937_64, random_device, uniform_r...
#include <ratio>                 // for ratio
#include <string>                // for string, operator+, char_traits, basi...
#include <thread>                // for this_thread

#include "config.h"              // for HAVE_PTHREAD_GETNAME_NP_3, HAVE_PTHR...
#include "exception.h"           // for Exit
#include "field_parser.h"        // for FieldParser, FieldParserError
#include "io_utils.h"            // for open, read
#include "log.h"                 // for Log, L_ERR, L_WARNING, L_INFO
#include "namegen.h"             // for Generator


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


const std::regex numeric_re("-?(\\d*\\.\\d+|\\d+)", std::regex::optimize);


pos_time_t b_time;
std::chrono::time_point<std::chrono::system_clock> init_time;
times_row_t stats_cnt;


static std::random_device rd;  // Random device engine, usually based on /dev/random on UNIX-like systems
static std::mt19937_64 rng(rd()); // Initialize Mersennes' twister using rd to generate the seed


void set_thread_name(const std::string& name) {
#if defined(HAVE_PTHREAD_SETNAME_NP_1)
	pthread_setname_np(name.c_str());
#elif defined(HAVE_PTHREAD_SETNAME_NP_2)
	pthread_setname_np(pthread_self(), name.c_str());
#elif defined(HAVE_PTHREAD_SETNAME_NP_3)
	pthread_setname_np(pthread_self(), name.c_str(), nullptr);
#elif defined(HAVE_PTHREAD_SET_NAME_NP_2)
	pthread_set_name_np(pthread_self(), name.c_str());
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


std::string repr(const void* p, size_t size, bool friendly, bool quote, size_t max_size) {
	const char* q = (const char *)p;
	char *buff = new char[size * 4 + 3];
	char *d = buff;
	if (quote) *d++ = '\'';
	const char *p_end = q + size;
	const char *max_a = max_size ? q + (max_size * 2 / 3) : p_end + 1;
	const char *max_b = max_size ? p_end - (max_size / 3) : q - 1;
	while (q != p_end) {
		char c = *q++;
		if (q >= max_a && q <= max_b) {
			if (q == max_a) {
				*d++ = '.';
				*d++ = '.';
				*d++ = '.';
			}
			continue;
		}
		if (friendly) {
			switch (c) {
				case '\b':
					*d++ = '\\';
					*d++ = 'b';
					break;
				case '\t':
					*d++ = '\\';
					*d++ = 't';
					break;
				case '\n':
					*d++ = '\\';
					*d++ = 'n';
					break;
				case '\f':
					*d++ = '\\';
					*d++ = 'f';
					break;
				case '\r':
					*d++ = '\\';
					*d++ = 'r';
					break;
				case '\'':
					*d++ = '\\';
					*d++ = '\'';
					break;
				case '\\':
					*d++ = '\\';
					*d++ = '\\';
					break;
				default:
					if (c >= ' ' && c <= '~') {
						*d++ = c;
					} else {
						*d++ = '\\';
						*d++ = 'x';
						sprintf(d, "%02x", (unsigned char)c);
						d += 2;
					}
					break;
			}
		} else {
			*d++ = '\\';
			*d++ = 'x';
			sprintf(d, "%02x", (unsigned char)c);
			d += 2;
		}
		//printf("%02x: %ld < %ld\n", (unsigned char)c, (unsigned long)(d - buff), (unsigned long)(size * 4 + 1));
	}
	if (quote) *d++ = '\'';
	*d = '\0';
	std::string ret(buff);
	delete [] buff;
	return ret;
}


std::string repr(const std::string& string, bool friendly, bool quote, size_t max_size) {
	return repr(string.c_str(), string.length(), friendly, quote, max_size);
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


char* normalize_path(const char* src, const char* end, char* dst) {
	int levels = 0;
	char* ret = dst;
	while (*src && src < end) {
		char ch = *src++;
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
	*dst++ = '\0';
	return ret;
}


char* normalize_path(const std::string& src, char* dst) {
	const char* src_str = src.data();
	return normalize_path(src_str, src_str + src.length(), dst);
}


std::string normalize_path(const std::string& src) {
	char buffer[PATH_MAX];
	const char* src_str = src.data();
	return normalize_path(src_str, src_str + src.length(), buffer);
}


void to_upper(std::string& str) {
	for (auto& c : str) c = toupper(c);
}


void to_lower(std::string& str) {
	for (auto& c : str) c = tolower(c);
}


bool strhasupper(const std::string& str) {
	for (const auto& c : str) {
		if (isupper(c)) {
			return true;
		}
	}

	return false;
}


bool isRange(const std::string& str) {
	try {
		FieldParser fieldparser(str);
		fieldparser.parse();
		return fieldparser.isrange;
	} catch (const FieldParserError&) {
		return false;
	}
}


bool isNumeric(const std::string& str) {
	std::smatch m;
	return std::regex_match(str, m, numeric_re) && static_cast<size_t>(m.length(0)) == str.length();
}


bool startswith(const std::string& text, const std::string& token) {
	auto text_len = text.length();
	auto token_len = token.length();
	return text_len >= token_len && text.compare(0, token_len, token) == 0;
}


bool startswith(const std::string& text, char ch) {
	auto text_len = text.length();
	return text_len >= 1 && text.at(0) == ch;
}


bool endswith(const std::string& text, const std::string& token) {
	auto text_len = text.length();
	auto token_len = token.length();
	return text_len >= token_len && std::equal(text.begin() + text_len - token_len, text.end(), token.begin());
}


bool endswith(const std::string& text, char ch) {
	auto text_len = text.length();
	return text_len >= 1 && text.at(text_len - 1) == ch;
}


void update_pos_time() {
	auto b_time_second = b_time.second;
	auto b_time_minute = b_time.minute;

	auto t_current = std::chrono::system_clock::now();
	auto t_elapsed = std::chrono::duration_cast<std::chrono::seconds>(t_current - init_time).count();

	if (t_elapsed >= SLOT_TIME_SECOND) {
		fill_zeros_stats_sec(0, SLOT_TIME_SECOND - 1);
		b_time.minute += t_elapsed / SLOT_TIME_SECOND;
		b_time.second = t_elapsed % SLOT_TIME_SECOND;
	} else {
		b_time.second += t_elapsed;
		if (b_time.second >= SLOT_TIME_SECOND) {
			fill_zeros_stats_sec(b_time_second + 1, SLOT_TIME_SECOND - 1);
			fill_zeros_stats_sec(0, b_time.second % SLOT_TIME_SECOND);
			b_time.minute += b_time.second / SLOT_TIME_SECOND;
			b_time.second = t_elapsed % SLOT_TIME_SECOND;
		} else {
			fill_zeros_stats_sec(b_time_second + 1, b_time.second);
		}
	}

	init_time = t_current;

	if (b_time.minute >= SLOT_TIME_MINUTE) {
		fill_zeros_stats_min(b_time_minute + 1, SLOT_TIME_MINUTE - 1);
		fill_zeros_stats_min(0, b_time.minute % SLOT_TIME_MINUTE);
		b_time.minute = b_time.minute % SLOT_TIME_MINUTE;
	} else {
		fill_zeros_stats_min(b_time_minute + 1, b_time.minute);
	}

	ASSERT(b_time.second < SLOT_TIME_SECOND);
	ASSERT(b_time.minute < SLOT_TIME_MINUTE);
}


void fill_zeros_stats_min(uint16_t start, uint16_t end) {
	for (auto i = start; i <= end; ++i) {
		stats_cnt.index.min[i] = 0;
		stats_cnt.index.tm_min[i] = 0;
		stats_cnt.search.min[i] = 0;
		stats_cnt.search.tm_min[i] = 0;
		stats_cnt.del.min[i] = 0;
		stats_cnt.del.tm_min[i] = 0;
		stats_cnt.patch.min[i] = 0;
		stats_cnt.patch.tm_min[i] = 0;
	}
}


void fill_zeros_stats_sec(uint8_t start, uint8_t end) {
	for (auto i = start; i <= end; ++i) {
		stats_cnt.index.sec[i] = 0;
		stats_cnt.index.tm_sec[i] = 0;
		stats_cnt.search.sec[i] = 0;
		stats_cnt.search.tm_sec[i] = 0;
		stats_cnt.del.sec[i] = 0;
		stats_cnt.del.tm_sec[i] = 0;
		stats_cnt.patch.sec[i] = 0;
		stats_cnt.patch.tm_sec[i] = 0;
	}
}


void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt, times_row_t& stats_cnt_cpy) {
	for (auto i = start; i <= end; ++i) {
		cnt[0] += stats_cnt_cpy.index.min[i];
		cnt[1] += stats_cnt_cpy.search.min[i];
		cnt[2] += stats_cnt_cpy.del.min[i];
		cnt[3] += stats_cnt_cpy.patch.min[i];
		tm_cnt[0] += stats_cnt_cpy.index.tm_min[i];
		tm_cnt[1] += stats_cnt_cpy.search.tm_min[i];
		tm_cnt[2] += stats_cnt_cpy.del.tm_min[i];
		tm_cnt[3] += stats_cnt_cpy.patch.tm_min[i];
	}
}


void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t>& cnt, std::vector<long double>& tm_cnt, times_row_t& stats_cnt_cpy) {
	for (auto i = start; i <= end; ++i) {
		cnt[0] += stats_cnt_cpy.index.sec[i];
		cnt[1] += stats_cnt_cpy.search.sec[i];
		cnt[2] += stats_cnt_cpy.del.sec[i];
		cnt[3] += stats_cnt_cpy.patch.sec[i];
		tm_cnt[0] += stats_cnt_cpy.index.tm_sec[i];
		tm_cnt[1] += stats_cnt_cpy.search.tm_sec[i];
		tm_cnt[2] += stats_cnt_cpy.del.tm_sec[i];
		tm_cnt[3] += stats_cnt_cpy.patch.tm_sec[i];
	}
}


void delete_files(const std::string& path) {
	DIR *dirp = opendir(path.c_str());
	if (!dirp) {
		return;
	}

	bool contains_folder = false;
	struct dirent *ent;
	while ((ent = readdir(dirp)) != nullptr) {
		const char *s = ent->d_name;
		if (ent->d_type == DT_DIR) {
			if (s[0] == '.' && (s[1] == '\0' || (s[1] == '.' && s[2] == '\0'))) {
				continue;
			}
			contains_folder = true;
		}
		if (ent->d_type == DT_REG) {
			std::string file = path + "/" + std::string(ent->d_name);
			if (remove(file.c_str()) != 0) {
				L_ERR(nullptr, "File %s could not be deleted", ent->d_name);
			}
		}
	}

	closedir(dirp);
	if (!contains_folder) {
		if (rmdir(path.c_str()) != 0) {
			L_ERR(nullptr, "Directory %s could not be deleted", path.c_str());
		}
	}
}


void move_files(const std::string& src, const std::string& dst) {
	DIR *dirp = opendir(src.c_str());
	if (!dirp) {
		return;
	}

	struct dirent *ent;
	while ((ent = readdir(dirp)) != nullptr) {
		if (ent->d_type == DT_REG) {
			std::string old_name = src + "/" + ent->d_name;
			std::string new_name = dst + "/" + ent->d_name;
			if (::rename(old_name.c_str(), new_name.c_str()) != 0) {
				L_ERR(nullptr, "Couldn't rename %s to %s", old_name.c_str(), new_name.c_str());
			}
		}
	}

	closedir(dirp);
	if (rmdir(src.c_str()) != 0) {
		L_ERR(nullptr, "Directory %s could not be deleted", src.c_str());
	}
}


bool exists(const std::string& path) {
	struct stat buffer;
	return stat(path.c_str(), &buffer) == 0;
}


bool build_path_index(const std::string& path) {
	std::string dir = path;
	std::size_t found = dir.find_last_of("/\\");
	if (found != std::string::npos) {
		dir.resize(found);
	}
	if (exists(dir)) {
		return true;
	} else {
		std::vector<std::string> directories;
		stringTokenizer(dir, "/", directories);
		dir.clear();
		for (const auto& _dir : directories) {
			dir.append(_dir).append(1, '/');
			if (mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
				return false;
			}
		}
		return true;
	}
}


DIR* opendir(const char* filename, bool create) {
	DIR* dirp = opendir(filename);
	if (!dirp) {
		if (errno == ENOENT && create) {
			if (::mkdir(filename, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
				return nullptr;
			} else {
				dirp = opendir(filename);
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


void find_file_dir(DIR* dir, File_ptr& fptr, const std::string& pattern, bool pre_suf_fix) {
	bool(*match_pattern)(const std::string&, const std::string&);
	if (pre_suf_fix) {
		match_pattern = startswith;
	} else {
		match_pattern = endswith;
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

	while ((fptr.ent = readdir(dir)) != nullptr) {
		if (fptr.ent->d_type == DT_REG) {
			std::string filename(fptr.ent->d_name);
			if (match_pattern(filename, pattern)) {
				return;
			}
		}
	}
}


int copy_file(const std::string& src, const std::string& dst, bool create, const std::string& file_name, const std::string& new_name) {
	DIR* dir_src = opendir(src.c_str());
	if (!dir_src) {
		L_ERR(nullptr, "ERROR: %s", strerror(errno));
		return -1;
	}

	struct stat s;
	int err = stat(dst.c_str(), &s);

	if (-1 == err) {
		if (ENOENT == errno && create) {
			if (::mkdir(dst.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
				L_ERR(nullptr, "ERROR: couldn't create directory %s (%s)", dst.c_str(), strerror(errno));
				return -1;
			}
		} else {
			L_ERR(nullptr, "ERROR: couldn't obtain directory information %s (%s)", dst.c_str(), strerror(errno));
			return -1;
		}
	}

	bool ended = false;
	struct dirent *ent;
	unsigned char buffer[4096];

	while ((ent = readdir(dir_src)) != nullptr and not ended) {
		if (ent->d_type == DT_REG) {

			if (not file_name.empty()) {
				if (strcmp(ent->d_name, file_name.c_str()) != 0) {
					continue;
				} else {
					ended = true;
				}
			}

			std::string src_path (src + "/" + std::string(ent->d_name));
			std::string dst_path (dst + "/" + (new_name.empty() ? std::string(ent->d_name) : new_name));

			int src_fd = io::open(src_path.c_str(), O_RDONLY);
			if (-1 == src_fd) {
				L_ERR(nullptr, "ERROR: opening file. %s\n", src_path.c_str());
				return -1;
			}

			int dst_fd = io::open(dst_path.c_str(), O_CREAT | O_WRONLY, 0644);
			if (-1 == src_fd) {
				L_ERR(nullptr, "ERROR: opening file. %s\n", dst_path.c_str());
				return -1;
			}

			while (1) {
				ssize_t bytes = io::read(src_fd, buffer, 4096);
				if (-1 == bytes) {
					L_ERR(nullptr, "ERROR: reading file. %s (%s)\n", src_path.c_str(), strerror(errno));
					return -1;
				}

				if (0 == bytes) break;

				bytes = write(dst_fd, buffer, bytes);
				if (-1 == bytes) {
					L_ERR(nullptr, "ERROR: writing file. %s (%s)\n", dst_path.c_str(), strerror(errno));
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


void stringTokenizer(const std::string& str, const std::string& delimiter, std::vector<std::string>& tokens) {
	size_t prev = 0, next = 0, len;

	while ((next = str.find(delimiter, prev)) != std::string::npos) {
		len = next - prev;
		if (len > 0) {
			tokens.push_back(str.substr(prev, len));
		}
		prev = next + delimiter.length();
	}

	if (prev < str.length()) {
		tokens.push_back(str.substr(prev));
	}
}


static inline std::string humanize(long double delta, bool colored, const int i, const int n, const long double div, const char* const units[], const long double scaling[], const char* const colors[], long double rounding) {
	long double num = delta;

	if (delta < 0) delta = -delta;
	int order = (delta == 0) ? n : -floorl(logl(delta) / div);
	order += i;
	if (order < 0) order = 0;
	else if (order > n) order = n;

	const char* color = colored ? colors[order] : "";
	num = roundl(rounding * num / scaling[order]) / rounding;
	const char* unit = units[order];
	const char* reset = colored ? colors[n + 1] : "";

	return format_string("%s%Lg%s%s", color, num, unit, reset);
}


static inline int find_val(long double val, const long double* input, int i=0) {
	return (*input == val) ? i : find_val(val, input + 1, i + 1);
}


std::string bytes_string(size_t bytes, bool colored) {
	static const long double base = 1024;
	static const long double div = logl(base);
	static const long double scaling[] = { powl(base, 8), powl(base, 7), powl(base, 6), powl(base, 5), powl(base, 4), powl(base, 3), powl(base, 2), powl(base, 1), 1 };
	static const char* const units[] = { "YiB", "ZiB", "EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B" };
	static const char* const colors[] = { "\033[1;31m", "\033[1;31m", "\033[1;31m", "\033[1;31m", "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0;32m", "\033[0;32m", "\033[0m" };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(bytes, colored, i, n, div, units, scaling, colors, 10.0L);
}


std::string small_time_string(long double seconds, bool colored) {
	static const long double base = 1000;
	static const long double div = logl(base);
	static const long double scaling[] = { 1, powl(base, -1), powl(base, -2), powl(base, -3), powl(base, -4) };
	static const char* const units[] = { "s", "ms", "\xc2\xb5s", "ns", "ps" };
	static const char* const colors[] = { "\033[1;31m", "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0;32m", "\033[0m" };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(seconds, colored, i, n, div, units, scaling, colors, 1000.0L);
}


std::string time_string(long double seconds, bool colored) {
	static const long double base = 60;
	static const long double div = logl(base);
	static const long double scaling[] = { powl(base, 2), powl(base, 1), 1 };
	static const char* const units[] = { "hrs", "min", "s" };
	static const char* const colors[] = { "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0m" };
	static const int n = sizeof(units) / sizeof(const char*) - 1;
	static const int i = find_val(1, scaling);

	return humanize(seconds, colored, i, n, div, units, scaling, colors, 100.0L);
}


std::string delta_string(long double nanoseconds, bool colored) {
	long double seconds = nanoseconds / 1e9;  // convert nanoseconds to seconds (as a double)
	return (seconds < 1) ? small_time_string(seconds, colored) : time_string(seconds, colored);
}


std::string delta_string(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, bool colored) {
	return delta_string(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count(), colored);
}


void _tcp_nopush(int sock, int optval) {
#ifdef TCP_NOPUSH
	if (setsockopt(sock, IPPROTO_TCP, TCP_NOPUSH, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: setsockopt TCP_NOPUSH (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif

#ifdef TCP_CORK
	if (setsockopt(sock, IPPROTO_TCP, TCP_CORK, &optval, sizeof(optval)) < 0) {
		L_ERR(nullptr, "ERROR: setsockopt TCP_CORK (sock=%d): [%d] %s", sock, errno, strerror(errno));
	}
#endif
}


/*
 * From https://github.com/antirez/redis/blob/b46239e58b00774d121de89e0e033b2ed3181eb0/src/server.c#L1496
 *
 * This function will try to raise the max number of open files accordingly to
 * the configured max number of clients. It also reserves a number of file
 * descriptors for extra operations of persistence, listening sockets, log files and so forth.
 *
 * If it will not be possible to set the limit accordingly to the configured
 * max number of clients, the function will do the reverse setting
 * to the value that we can actually handle.
 */
void adjustOpenFilesLimit(size_t& max_clients) {
	rlim_t maxfiles = max_clients + RESERVED_FDS;
	struct rlimit limit;

	if (getrlimit(RLIMIT_NOFILE, &limit) == -1) {
		L_WARNING(nullptr, "Unable to obtain the current NOFILE limit (%s), assuming 1024 and setting the max clients configuration accordingly", strerror(errno));
		max_clients = 1024 - RESERVED_FDS;
	} else {
		rlim_t oldlimit = limit.rlim_cur;

		/* Set the max number of files if the current limit is not enough
		* for our needs. */
		if (oldlimit < maxfiles) {
			rlim_t bestlimit;
			int setrlimit_error = 0;

			/* Try to set the file limit to match 'maxfiles' or at least
			* to the higher value supported less than maxfiles. */
			bestlimit = maxfiles;
			while (bestlimit > oldlimit) {
				rlim_t decr_step = 16;

				limit.rlim_cur = bestlimit;
				limit.rlim_max = bestlimit;
				if (setrlimit(RLIMIT_NOFILE,&limit) != -1) break;
				setrlimit_error = errno;

				/* We failed to set file limit to 'bestlimit'. Try with a
				* smaller limit decrementing by a few FDs per iteration. */
				if (bestlimit < decr_step) break;
				bestlimit -= decr_step;
			}

			/* Assume that the limit we get initially is still valid if
			* our last try was even lower. */
			if (bestlimit < oldlimit) {
				bestlimit = oldlimit;
			}

			if (bestlimit < maxfiles) {
				int old_maxclients = max_clients;
				max_clients = bestlimit - RESERVED_FDS;

				if (max_clients < 1) {
					L_WARNING(nullptr, "Your current 'ulimit -n' of %llu is not enough for the server to start. Please increase your open file limit to at least %llu",
						(unsigned long long) oldlimit,
						(unsigned long long) maxfiles);
					throw Exit(EX_OSFILE);
				}
				L_WARNING(nullptr, "You requested maxclients of %d requiring at least %llu max file descriptors", old_maxclients, (unsigned long long) maxfiles);
				L_WARNING(nullptr, "Server can't set maximum open files to %llu because of OS error: %s", (unsigned long long) maxfiles, strerror(setrlimit_error));
				L_WARNING(nullptr, "Current maximum open files is %llu maxclients has been reduced to %d to compensate for low ulimit. If you need higher maxclients increase 'ulimit -n'", (unsigned long long) bestlimit, max_clients);
			} else {
				L_INFO(nullptr, "Increased maximum number of open files to %llu (it was originally set to %llu)", (unsigned long long) maxfiles, (unsigned long long) oldlimit);
			}
		}
	}
}
