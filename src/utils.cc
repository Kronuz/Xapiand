/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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
#include "namegen.h"

#include <string>
#include <cstdlib>
#include <thread>
#include <mutex>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <xapian.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif
#endif

#define COORDS_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d+|\\d+)"
#define NUMERIC_RE "-?(\\d*\\.\\d+|\\d+)"

#define STATE_ERR -1
#define STATE_CM0 0
#define STATE_CMD 1
#define STATE_UPL 2 /* case _upload */
#define STATE_NSP 3
#define STATE_PTH 4
#define STATE_HST 5

#define HTTP_UPLOAD "_upload"
#define HTTP_UPLOAD_SIZE 7

pcre *compiled_coords_re = NULL;
pcre *compiled_numeric_re = NULL;
std::regex find_range_re = std::regex("([^ ]*)\\.\\.([^ ]*)", std::regex::optimize);

std::mutex log_mutex;

pos_time_t b_time;
std::chrono::time_point<std::chrono::system_clock> init_time;
times_row_t stats_cnt;

static std::random_device rd;  // Random device engine, usually based on /dev/random on UNIX-like systems
static std::mt19937 rng(rd()); // Initialize Mersennes' twister using rd to generate the seed


void set_thread_name(const std::string &name) {
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


std::string repr(const void* p, size_t size, bool friendly) {
	const char* q = (const char *)p;
	char *buff = new char[size * 4 + 1];
	char *d = buff;
	const char *p_end = q + size;
	while (q != p_end) {
		char c = *q++;
		if (friendly && c == 9) {
			*d++ = '\\';
			*d++ = 't';
		} else if (friendly && c == 10) {
			*d++ = '\\';
			*d++ = 'n';
		} else if (friendly && c == 13) {
			*d++ = '\\';
			*d++ = 'r';
		} else if (friendly && c == '\'') {
			*d++ = '\\';
			*d++ = '\'';
		} else if (friendly && c >= ' ' && c <= '~') {
			*d++ = c;
		} else {
			*d++ = '\\';
			*d++ = 'x';
			sprintf(d, "%02x", (unsigned char)c);
			d += 2;
		}
		//printf("%02x: %ld < %ld\n", (unsigned char)c, (unsigned long)(d - buff), (unsigned long)(size * 4 + 1));
	}
	*d = '\0';
	std::string ret(buff);
	delete [] buff;
	return ret;
}


std::string repr(const std::string &string, bool friendly) {
	return repr(string.c_str(), string.size(), friendly);
}


std::atomic_bool log_runner(true);
static std::mutex log_mutex;
static std::map<uint64_t, std::pair<std::chrono::time_point<std::chrono::system_clock>, std::string>> log_map;
std::thread log_thread([] {
	while (log_runner) {
		auto now = std::chrono::system_clock::now();
		{
			std::lock_guard<std::mutex> lk(log_mutex);
			for (auto it = log_map.begin(); it != log_map.end();) {
				if (now > it->second.first) {
					std::cerr << it->second.second;
					it = log_map.erase(it);
				} else {
					++it;
				}
			}
		}
		if(log_runner) std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
});

void log_kill()
{
	log_runner = false;
	log_thread.join();
}


std::string _log(const char *file, int line, void *, const char *format, va_list ap)
{
	char buffer[2048];

	snprintf(buffer, sizeof(buffer), "tid(%s): ../%s:%d: ", get_thread_name().c_str(), file, line);
	size_t buffer_len = strlen(buffer);

	vsnprintf(&buffer[buffer_len], sizeof(buffer) - buffer_len, format, ap);

	return buffer;
}


void
Log::end(const char *file, int line, void *obj, const char *format, ...)
{
	va_list argptr;
	va_start(argptr, format);
	if (!log_runner) {
		std::lock_guard<std::mutex> lk(log_mutex);
		std::cerr << log(file, line, obj, format, argptr);
	}
	va_end(argptr);
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


static NameGen::Generator generator("!<K|E><k|e|l><|||s>");

std::string name_generator() {
	return generator.toString();
}


const char HEX2DEC[256] = {
	/*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
	/* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

	/* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

	/* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

	/* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};


std::string urldecode(const char *src, size_t size) {
	// Note from RFC1630:  "Sequences which start with a percent sign
	// but are not followed by two hexadecimal characters (0-9, A-F) are reserved
	// for future extension"

	const char * SRC_END = src + size;
	const char * SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'

	char * const pStart = new char[size];
	char * pEnd = pStart;

	while (src < SRC_LAST_DEC)
	{
		if (*src == '%')
		{
			char dec1, dec2;
			if (-1 != (dec1 = HEX2DEC[static_cast<int>(*(src + 1))])
				&& -1 != (dec2 = HEX2DEC[static_cast<int>(*(src + 2))]))
			{
				*pEnd++ = (dec1 << 4) + dec2;
				src += 3;
				continue;
			}
		}

		*pEnd++ = *src++;
	}

	// the last 2- chars
	while (src < SRC_END)
	*pEnd++ = *src++;

	std::string sResult(pStart, pEnd);
	delete [] pStart;
	//std::replace( sResult.begin(), sResult.end(), '+', ' ');
	return sResult;
}


int url_qs(const char *name, const char *qs, size_t size, parser_query_t *par) {
	const char *nf = qs + size;
	const char *n1, *n0;
	const char *v0 = NULL;

	if (par->offset == NULL) {
		n0 = n1 = qs;
	} else {
		n0 = n1 = par->offset + par->length;
	}

	while (1) {
		char cn = *n1;
		if (n1 == nf) {
			cn = '\0';
		}
		switch (cn) {
			case '=' :
				v0 = n1;
			case '\0':
			case '&' :
			case ';' :
				if (strlen(name) == static_cast<size_t>(n1 - n0) && strncmp(n0, name, n1 - n0) == 0) {
					if (v0) {
						const char *v1 = v0 + 1;
						while (1) {
							char cv = *v1;
							if (v1 == nf) {
								cv = '\0';
							}
							switch(cv) {
								case '\0':
								case '&' :
								case ';' :
								par->offset = v0 + 1;
								par->length = v1 - v0 - 1;
								return 0;
							}
							v1++;
						}
					} else {
						par->offset = n1 + 1;
						par->length = 0;
						return 0;
					}
				} else if (!cn) {
					return -1;
				} else if (cn != '=') {
					n0 = n1 + 1;
					v0 = NULL;
				}
		}
		n1++;
	}
	return -1;
}


int url_path(const char* ni, size_t size, parser_url_path_t *par) {
	const char *nf = ni + size;
	const char *n0, *n1, *n2 = NULL;
	int state, direction;
	size_t length;

	if (par->offset == NULL) {
		state = STATE_CM0;
		n0 = n1 = n2 = nf - 1;
		direction = -1;
	} else {
		state = STATE_NSP;
		n0 = n1 = n2 = par->offset;
		if (par->off_upload) {
			nf = par->off_upload - 1;
		} else {
			nf = par->off_command - 1;
		}
		direction = 1;
	}

	while (state != STATE_ERR) {
		if (!(n1 >= ni && n1 <= nf)) {
			/*In case direction is backwards and not find any this [/ , @ :] */
			if (state == STATE_UPL) {
				state = STATE_NSP;
				nf = n0;
				n0 = n1 = n2 = ni;
				direction = 1;
				par->offset = n0;
			} else {
				return -1;
			}
		}

		char cn = (n1 >= nf) ? '\0' : *n1;
		switch(cn) {
			case '\0':
				if (n0 == n1) return -1;

			case ',':
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
					case STATE_CMD:
						break;
					case STATE_NSP:
					case STATE_PTH:
						length = n1 - n0;
						par->off_path = n0;
						par->len_path = length;
						state = length ? 0 : STATE_ERR;
						if (cn) n1++;
						par->offset = n1;
						return state;
					case STATE_HST:
						length = n1 - n0;
						par->off_host = n0;
						par->len_host = length;
						state = length ? 0 : STATE_ERR;
						if (cn) n1++;
						par->offset = n1;
						return state;
					case STATE_UPL:
						length = n0 - n1 - 1;
						if (length == HTTP_UPLOAD_SIZE && strncmp(n1 + 1, HTTP_UPLOAD, HTTP_UPLOAD_SIZE) == 0) {
							par->off_upload = n1 + 1;
							par->len_upload = length;
							state = length ? STATE_NSP : STATE_ERR;
							nf = n1;
							n0 = n1 = n2 = ni;
							direction = 1;
							par->offset = n0;
						} else {
							state = STATE_NSP;
							nf = n0;
							n0 = n1 = n2 = ni;
							direction = 1;
							par->offset = n0;
						}
				}
				break;

			case ':':
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
					case STATE_CMD:
						break;
					case STATE_UPL:
						state = STATE_NSP;
						nf = n0;
						n0 = n1 = n2 = ni;
						direction = 1;
						par->offset = n0;
						break;
					case STATE_NSP:
						length = n1 - n0;
						par->off_namespace = n0;
						par->len_namespace = length;
						state = length ? STATE_PTH : STATE_ERR;
						n0 = n1 + 1;
						break;
					case STATE_HST:
						break;
					default:
						state = STATE_ERR;
				}
				break;

			case '@':
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
					case STATE_CMD:
						break;
					case STATE_UPL:
						state = STATE_NSP;
						nf = n0;
						n0 = n1 = n2 = ni;
						direction = 1;
						par->offset = n0;
						break;
					case STATE_NSP:
						length = n1 - n0;
						par->off_path = n0;
						par->len_path = length;
						state = length ? STATE_HST : STATE_ERR;
						n0 = n1 + 1;
						break;
					case STATE_PTH:
						par->off_path = n0;
						par->len_path = n1 - n0;
						state = STATE_HST;
						n0 = n1 + 1;
						break;
					default:
						state = STATE_ERR;
				}
				break;

			case '/':
				switch (state) {
					case STATE_CM0:
						break;
					case STATE_CMD:
						length = n0 - n1;
						par->off_command = n1 + 1;
						par->len_command = length;
						state = length ? STATE_UPL : STATE_ERR;
						n0 = n1;
						break;
					case STATE_UPL:
						length = n0 - n1 - 1;
						if (length == HTTP_UPLOAD_SIZE && strncmp(n1 + 1, HTTP_UPLOAD, HTTP_UPLOAD_SIZE) == 0) {
							par->off_upload = n1 + 1;
							par->len_upload = length;
							state = length ? STATE_NSP : STATE_ERR;
							nf = n1;
							n0 = n1 = n2 = ni;
							direction = 1;
							par->offset = n0;
						} else {
							state = STATE_NSP;
							nf = n0;
							n0 = n1 = n2 = ni;
							direction = 1;
							par->offset = n0;
						}
						break;
				}
				break;

			default:
				switch (state) {
					case STATE_CM0:
						state++;
						n0 = n1;
						break;
				}
				break;
		}
		n1 += direction;
	}
	return -1;
}


int pcre_search(const char *subject, int length, int startoffset, int options, const char *pattern, pcre **code, unique_group &unique_groups) {
	int erroffset;
	const char *error;

	// First, the regex string must be compiled.
	if (*code == NULL) {
		// pcre_free is not used after compiling the regular expression here because
		// it's compiled into a global static variable, which gets freed by the end of the program.
		*code = pcre_compile(pattern, 0, &error, &erroffset, 0);
		if (*code == NULL) {
			LOG_ERR(NULL, "pcre_compile of %s failed (offset: %d), %s\n", pattern, erroffset, error);
			return -1;
		}
	}

	if (*code != NULL) {
		int n;
		if (pcre_fullinfo(*code, NULL, PCRE_INFO_CAPTURECOUNT, &n) != 0) {
			return -1;
		}

		if (unique_groups == NULL) {
			unique_groups = unique_group(static_cast<group_t*>(malloc((n + 1) * 3 * sizeof(int))));
			if (unique_groups == NULL) {
				LOG_ERR(NULL, "Memory can not be reserved\n");
				return -1;
			}
		}

		int *ocvector = (int*)unique_groups.get();
		if (pcre_exec(*code, 0, subject, length, startoffset, options, ocvector, (n + 1) * 3) >= 0) {
			return 0;
		} else return -1;
	}

	return -1;
}


std::string stringtoupper(const std::string &str) {
	std::string tmp = str;
	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM_UPPER());
	return tmp;
}


std::string stringtolower(const std::string &str) {
	std::string tmp = str;
	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM_LOWER());
	return tmp;
}


std::string prefixed(const std::string &term, const std::string &prefix) {
	if (isupper(term.at(0))) {
		return (prefix.empty()) ? term : prefix + ":" + term;
	}

	return prefix + term;
}


unsigned int get_slot(const std::string &name) {
	// We are left with the last 8 characters.
	std::string _md5(md5(strhasupper(name) ? stringtoupper(name) : name), 24, 8);
	unsigned int slot = static_cast<unsigned int>(strtoul(_md5, 16));
	if (slot == 0x00000000) {
		slot = 0x00000001; // 0->id
	} else if (slot == Xapian::BAD_VALUENO) {
		slot = 0xfffffffe;
	}
	return slot;
}


long strtol(const std::string &str, int base) {
	return strtol(str.c_str(), NULL, base);
}


unsigned long strtoul(const std::string &str, int base) {
	return strtoul(str.c_str(), NULL, base);
}


double strtod(const std::string &str) {
	return strtod(str.c_str(), NULL);
}


long long strtoll(const std::string &str, int base) {
	return strtoll(str.c_str(), NULL, base);
}


unsigned long long strtoull(const std::string &str, int base) {
	return strtoull(str.c_str(), NULL, base);
}


std::string get_prefix(const std::string &name, const std::string &prefix, char type) {
	std::string slot(get_slot_hex(name));
	std::transform(slot.begin(), slot.end(), slot.begin(), TRANSFORM_MAP());
	std::string res(prefix);
	res.append(1, toupper(type));
	return res + slot;
}


std::string get_slot_hex(const std::string &name) {
	// We are left with the last 8 characters.
	std::string _md5(md5(strhasupper(name) ? stringtoupper(name): name), 24, 8);
	return stringtoupper(_md5);
}


bool strhasupper(const std::string &str) {
	std::string::const_iterator it(str.begin());
	for ( ; it != str.end(); it++) {
		if (isupper(*it)) return true;
	}

	return false;
}


bool isRange(const std::string &str) {
	std::smatch m;
	return std::regex_match(str, m, find_range_re);
}


bool isNumeric(const std::string &str) {
	unique_group unique_gr;
	int len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, 0, 0, NUMERIC_RE, &compiled_numeric_re, unique_gr);
	group_t *g = unique_gr.get();

	return (ret != -1 && (g[0].end - g[0].start) == len) ? true : false;
}


bool startswith(const std::string &text, const std::string &token) {
	if (text.length() < token.length())
		return false;
	return (text.compare(0, token.length(), token) == 0);
}


void update_pos_time() {
	auto t_current = std::chrono::system_clock::now();
	auto aux_second = b_time.second;
	auto aux_minute = b_time.minute;
	auto t_elapsed = std::chrono::duration_cast<std::chrono::seconds>(t_current - init_time).count();
	if (t_elapsed < SLOT_TIME_SECOND) {
		b_time.second += t_elapsed;
		if (b_time.second >= SLOT_TIME_SECOND) {
			b_time.minute += b_time.second / SLOT_TIME_SECOND;
			fill_zeros_stats_sec(aux_second + 1, SLOT_TIME_SECOND - 1);
			fill_zeros_stats_sec(0, b_time.second % SLOT_TIME_SECOND);
		} else {
			fill_zeros_stats_sec(aux_second + 1, b_time.second);
		}
	} else {
		b_time.second = t_elapsed % SLOT_TIME_SECOND;
		fill_zeros_stats_sec(0, SLOT_TIME_SECOND - 1);
		b_time.minute += t_elapsed / SLOT_TIME_SECOND;
	}
	init_time = t_current;
	if (b_time.minute >= SLOT_TIME_MINUTE) {
		fill_zeros_stats_min(aux_minute + 1, SLOT_TIME_MINUTE - 1);
		fill_zeros_stats_min(0, b_time.minute % SLOT_TIME_MINUTE);
	} else {
		fill_zeros_stats_min(aux_minute + 1, b_time.minute);
	}
}


void fill_zeros_stats_min(uint16_t start, uint16_t end) {
	for (auto i = start; i <= end; ++i) {
		stats_cnt.index.min[i] = 0;
		stats_cnt.index.tm_min[i] = 0;
		stats_cnt.search.min[i] = 0;
		stats_cnt.search.tm_min[i] = 0;
		stats_cnt.del.min[i] = 0;
		stats_cnt.del.tm_min[i] = 0;
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
	}
}


void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t> &cnt, std::vector<double> &tm_cnt, times_row_t &stats_cnt_cpy) {
	for (auto i = start; i <= end; ++i) {
		cnt[0] += stats_cnt_cpy.index.min[i];
		cnt[1] += stats_cnt_cpy.search.min[i];
		cnt[2] += stats_cnt_cpy.del.min[i];
		tm_cnt[0] += stats_cnt_cpy.index.tm_min[i];
		tm_cnt[1] += stats_cnt_cpy.search.tm_min[i];
		tm_cnt[2] += stats_cnt_cpy.del.tm_min[i];
	}
}


void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t> &cnt, std::vector<double> &tm_cnt, times_row_t &stats_cnt_cpy) {
	for (auto i = start; i <= end; ++i) {
		cnt[0] += stats_cnt_cpy.index.sec[i];
		cnt[1] += stats_cnt_cpy.search.sec[i];
		cnt[2] += stats_cnt_cpy.del.sec[i];
		tm_cnt[0] += stats_cnt_cpy.index.tm_sec[i];
		tm_cnt[1] += stats_cnt_cpy.search.tm_sec[i];
		tm_cnt[2] += stats_cnt_cpy.del.tm_sec[i];
	}
}


void delete_files(const std::string &path) {
	unsigned char isFile = 0x8;
	unsigned char isFolder = 0x4;

	bool contains_folder = false;
	DIR *Dir;

	struct dirent *Subdir;
	Dir = opendir(path.c_str());

	if (!Dir) {
		return;
	}

	Subdir = readdir(Dir);
	while (Subdir) {
		if (Subdir->d_type == isFolder) {
			if (strcmp(Subdir->d_name, ".") != 0 && strcmp(Subdir->d_name, "..") != 0) {
				contains_folder = true;
			}
		}

		if (Subdir->d_type == isFile) {
			std::string file = path + "/" + std::string(Subdir->d_name);
			if (remove(file.c_str()) != 0) {
				LOG_ERR(NULL, "File %s could not be deleted\n", Subdir->d_name);
			}
		}
		Subdir = readdir(Dir);
	}

	if (!contains_folder) {
		if (rmdir(path.c_str()) != 0) {
			LOG_ERR(NULL, "Directory %s could not be deleted\n", path.c_str());
		}
	}
}


void move_files(const std::string &src, const std::string &dst) {
	unsigned char isFile = 0x8;

	DIR *Dir;
	struct dirent *Subdir;
	Dir = opendir(src.c_str());

	if (!Dir) {
		return;
	}

	Subdir = readdir(Dir);
	while (Subdir) {
		if (Subdir->d_type == isFile) {
			std::string old_name = src + "/" + Subdir->d_name;
			std::string new_name = dst + "/" + Subdir->d_name;
			if (::rename(old_name.c_str(), new_name.c_str()) != 0) {
				LOG_ERR(NULL, "Couldn't rename %s to %s\n", old_name.c_str(), new_name.c_str());
			}
		}
		Subdir = readdir(Dir);
	}

	if (rmdir(src.c_str()) != 0) {
		LOG_ERR(NULL, "Directory %s could not be deleted\n", src.c_str());
	}
}


inline bool exist(const std::string& path) {
	struct stat buffer;
	return (stat (path.c_str(), &buffer) == 0);
}


bool buid_path_index(const std::string& path) {
	std::string dir = path;
	std::size_t found = dir.find_last_of("/\\");
	dir.resize(found);
	if (exist(dir)) {
		return true;
	} else {
		std::vector<std::string> directories;
		stringTokenizer(dir, "/", directories);
		dir.clear();
		for (std::vector<std::string>::iterator it = directories.begin(); it != directories.end(); it++) {
			dir = dir + *it + "/";
			if (mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
				continue;
			} else {
				return false;
			}
		}
		return true;
	}
}


void stringTokenizer(const std::string &str, const std::string &delimiter, std::vector<std::string> &tokens) {
	size_t prev = 0, next = 0, len;

	while ((next = str.find(delimiter, prev)) != std::string::npos) {
		len = next - prev;
		if (len > 0) {
			tokens.push_back(str.substr(prev, len));
		}
		prev = next + delimiter.size();
	}

	if (prev < str.size()) {
		tokens.push_back(str.substr(prev));
	}
}


unsigned int levenshtein_distance(const std::string &str1, const std::string &str2) {
	const size_t len1 = str1.size(), len2 = str2.size();
	std::vector<unsigned int> col(len2 + 1), prev_col(len2 + 1);

	for (unsigned int i = 0; i < prev_col.size(); i++) prev_col[i] = i;

	for (unsigned int i = 0; i < len1; i++) {
		col[0] = i + 1;
		for (unsigned int j = 0; j < len2; j++)
			col[j + 1] = std::min(std::min(prev_col[j + 1] + 1, col[j] + 1), prev_col[j] + (str1[i] == str2[j] ? 0 : 1));
		col.swap(prev_col);
	}

	return prev_col[len2];
}
