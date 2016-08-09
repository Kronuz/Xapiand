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

#include "database.h"
#include "hash/md5.h"
#include "log.h"
#include "namegen.h"
#include "xapiand.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <xapian.h>

#include <sys/resource.h> /* for getrlimit */
#include <sysexits.h>  /* EX_* */

#include <netinet/in.h> /* for IPPROTO_TCP */
#include <netinet/tcp.h> /* for TCP_NODELAY */
#include <sys/socket.h>


#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
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
const std::regex find_range_re("(.*)\\.\\.(.*)", std::regex::optimize);


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


std::string repr(const void* p, size_t size, bool friendly, size_t max_size) {
	const char* q = (const char *)p;
	char *buff = new char[size * 4 + 1];
	char *d = buff;
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
				case 9:
					*d++ = '\\';
					*d++ = 't';
					break;
				case 10:
					*d++ = '\\';
					*d++ = 'n';
					break;
				case 13:
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
	*d = '\0';
	std::string ret(buff);
	delete [] buff;
	return ret;
}


std::string repr(const std::string& string, bool friendly, size_t max_size) {
	return repr(string.c_str(), string.length(), friendly, max_size);
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

	while (src < SRC_LAST_DEC) {
		if (*src == '%') {
			char dec1, dec2;
			if (-1 != (dec1 = HEX2DEC[static_cast<int>(*(src + 1))])
				&& -1 != (dec2 = HEX2DEC[static_cast<int>(*(src + 2))])) {
				*pEnd++ = (dec1 << 4) + dec2;
				src += 3;
				continue;
			}
		}

		*pEnd++ = *src++;
	}

	// the last 2- chars
	while (src < SRC_END) {
		*pEnd++ = *src++;
	}

	std::string sResult(pStart, pEnd);
	delete [] pStart;
	//std::replace( sResult.begin(), sResult.end(), '+', ' ');
	return sResult;
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


QueryParser::QueryParser()
	: len(0),
	  off(nullptr) { }


void
QueryParser::clear()
{
	rewind();
	query.clear();
}


void
QueryParser::rewind()
{
	len = 0;
	off = nullptr;
}


int
QueryParser::init(const std::string& q)
{
	clear();
	query = q;
	return 0;
}


int
QueryParser::next(const char *name)
{
	const char *ni = query.data();
	const char *nf = ni + query.length();
	const char *n0, *n1 = nullptr;
	const char *v0 = nullptr;

	if (off == nullptr) {
		n0 = n1 = ni;
	} else {
		n0 = n1 = off + len;
	}

	while (true) {
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
								off = v0 + 1;
								len = v1 - v0 - 1;
								return 0;
							}
							++v1;
						}
					} else {
						off = n1 + 1;
						len = 0;
						return 0;
					}
				} else if (!cn) {
					return -1;
				} else if (cn != '=') {
					n0 = n1 + 1;
					v0 = nullptr;
				}
		}
		++n1;
	}
	return -1;
}


std::string
QueryParser::get()
{
	if (!off) return std::string();
	return urldecode(off, len);
}


PathParser::PathParser()
	: len_pth(0), off_pth(nullptr),
	  len_hst(0), off_hst(nullptr),
	  len_nsp(0), off_nsp(nullptr),
	  len_pmt(0), off_pmt(nullptr),
	  len_cmd(0), off_cmd(nullptr),
	  len_id(0), off_id(nullptr) { }


void
PathParser::clear()
{
	rewind();
	len_pmt = 0;
	off_pmt = nullptr;
	len_cmd = 0;
	off_cmd = nullptr;
	len_id = 0;
	off_id = nullptr;
	path.clear();
}


void
PathParser::rewind()
{
	off = path.data();
	len_pth = 0;
	off_pth = nullptr;
	len_hst = 0;
	off_hst = nullptr;
	len_nsp = 0;
	off_nsp = nullptr;
}


PathParser::State
PathParser::init(const std::string& p)
{
	clear();
	path = p;

	size_t length;
	const char *ni = path.data();
	const char *nf = ni + path.length();
	const char *n0, *n1 = nullptr;
	PathParser::State state;

	// This first goes backwards, looking for pmt, cmd and id
	// id is filled only if there's no pmt already:
	state = start;
	n0 = n1 = nf - 1;
	while (state >= 0) {
		char cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
		switch (cn) {
			case '\0':
			case '/':
				switch (state) {
					case start:
						if (!cn) {
							n0 = n1;
							state = pmt;
						}
						break;
					case pmt:
						assert(n0 >= n1);
						length = n0 - n1;
						if (length && *(n1 + 1) != '_') {
							off_id = n1 + 1;
							len_id = length;
							n0 = n1 - 1;
							state = cmd;
							break;
						}
					case cmd:
						assert(n0 >= n1);
						length = n0 - n1;
						if (length && *(n1 + 1) == '_') {
							off_pmt = off_id;
							len_pmt = len_id;
							off_id = nullptr;
							len_id = 0;
							off_cmd = n1 + 1;
							len_cmd = length;
							n0 = n1 - 1;
							state = id;
							break;
						}
					case id:
						if (!off_id) {
							assert(n0 >= n1);
							length = n0 - n1;
							if (length) {
								off_id = n1 + 1;
								len_id = length;
							}
						}
						off = ni;
						return state;
					default:
						break;
				}
				break;

			case ',':
			case ':':
			case '@':
				switch (state) {
					case start:
						n0 = n1;
						state = pmt;
						break;
					case id:
						off = ni;
						return state;
					default:
						break;
				}
				break;

			default:
				switch (state) {
					case start:
						n0 = n1;
						state = pmt;
						break;
					default:
						break;
				}
				break;
		}
		--n1;
	}

	return state;
}


PathParser::State
PathParser::next()
{
	size_t length;
	const char *ni = path.data();
	const char *nf = ni + path.length();
	const char *n0, *n1 = nullptr;
	PathParser::State state;

	// Then goes forward, looking for endpoints:
	state = nsp;
	off_hst = nullptr;
	n0 = n1 = off;
	if (off_pmt && off_pmt < nf) {
		nf = off_pmt - 1;
	}
	if (off_cmd && off_cmd < nf) {
		nf = off_cmd - 1;
	}
	if (off_id && off_id < nf) {
		nf = off_id - 1;
	}
	if (nf < ni) {
		nf = ni;
	}
	if (n1 > nf) {
		return end;
	}
	while (state >= 0) {
		char cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
		switch (cn) {
			case '\0':
			case ',':
				switch (state) {
					case nsp:
					case pth:
						assert(n1 >= n0);
						length = n1 - n0;
						off_pth = n0;
						len_pth = length;
						off = ++n1;
						return state;
					case hst:
						assert(n1 >= n0);
						length = n1 - n0;
						if (!length) return INVALID_HST;
						off_hst = n0;
						len_hst = length;
						off = ++n1;
						return state;
					default:
						break;
				}
				break;

			case ':':
				switch (state) {
					case nsp:
						assert(n1 >= n0);
						length = n1 - n0;
						if (!length) return INVALID_NSP;
						off_nsp = n0;
						len_nsp = length;
						n0 = n1 + 1;
						state = pth;
						break;
					default:
						break;
				}
				break;

			case '@':
				switch (state) {
					case nsp:
					case pth:
						assert(n1 >= n0);
						length = n1 - n0;
						off_pth = n0;
						len_pth = length;
						n0 = n1 + 1;
						state = hst;
						break;
					default:
						break;
				}
				break;

			default:
				break;
		}
		++n1;
	}

	return state;
}


std::string
PathParser::get_pth()
{
	if (!off_pth) return std::string();
	return urldecode(off_pth, len_pth);
}


std::string
PathParser::get_hst()
{
	if (!off_hst) return std::string();
	return urldecode(off_hst, len_hst);
}


std::string
PathParser::get_nsp()
{
	if (!off_nsp) return std::string();
	return urldecode(off_nsp, len_nsp);
}


std::string
PathParser::get_pmt()
{
	if (!off_pmt) return std::string();
	return urldecode(off_pmt, len_pmt);
}


std::string
PathParser::get_cmd()
{
	if (!off_cmd) return std::string();
	return urldecode(off_cmd, len_cmd);
}


std::string
PathParser::get_id()
{
	if (!off_id) return std::string();
	return urldecode(off_id, len_id);
}


void to_upper(std::string& str) {
	for (auto& c : str) c = toupper(c);
}


void to_lower(std::string& str) {
	for (auto& c : str) c = tolower(c);
}


std::string prefixed(const std::string& term, const std::string& prefix) {
	if (isupper(term.at(0))) {
		if (prefix.empty()) {
			return term;
		} else {
			std::string result;
			result.reserve(prefix.length() + term.length() + 1);
			result.assign(prefix).push_back(':');
			result.append(term);
			return result;
		}
	} else {
		std::string result;
		result.reserve(prefix.length() + term.length());
		result.assign(prefix).append(term);
		return result;
	}
}


unsigned get_slot(const std::string& name) {
	MD5 md5;
	// We are left with the last 8 characters.
	std::string _md5(md5(strhasupper(name) ? upper_string(name) : name), 24, 8);
	unsigned slot = static_cast<unsigned>(std::stoul(_md5, nullptr, 16));
	if (slot < DB_SLOT_RESERVED) {
		slot += DB_SLOT_RESERVED;
	} else if (slot == Xapian::BAD_VALUENO) {
		slot = 0xfffffffe;
	}
	return slot;
}


std::string get_prefix(const std::string& name, const std::string& prefix, char type) {
	MD5 md5;
	// We are left with the last 8 characters.
	auto _md5 = get_slot_hex(name);
	// Mapped [0-9] -> [A-J] and [A-F] -> [R-W]
	for (auto& c : _md5) c += 17;

	std::string result;
	result.reserve(prefix.length() + _md5.length() + 1);
	result.assign(prefix).push_back(type);
	result.append(_md5);
	return result;
}


std::string get_slot_hex(const std::string& name) {
	MD5 md5;
	// We are left with the last 8 characters.
	return upper_string(md5(strhasupper(name) ? upper_string(name) : name), 24, 8);
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
	std::smatch m;
	return std::regex_match(str, m, find_range_re);
}


bool isNumeric(const std::string& str) {
	std::smatch m;
	return std::regex_match(str, m, numeric_re) && static_cast<size_t>(m.length(0)) == str.length();
}


bool startswith(const std::string& text, const std::string& token) {
	if (text.length() < token.length()) {
		return false;
	}

	return text.compare(0, token.length(), token) == 0;
}


bool endswith(const std::string& text, const std::string& token) {
	if (token.length() > text.length()) return false;
	return std::equal(text.begin() + text.length() - token.length(), text.end(), token.begin());
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

	assert(b_time.second >= 0 && b_time.second < SLOT_TIME_SECOND);
	assert(b_time.minute >= 0 && b_time.minute < SLOT_TIME_MINUTE);
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


bool exist(const std::string& path) {
	struct stat buffer;
	return stat(path.c_str(), &buffer) == 0;
}


bool build_path_index(const std::string& path) {
	std::string dir = path;
	std::size_t found = dir.find_last_of("/\\");
	if (found != std::string::npos) {
		dir.resize(found);
	}
	if (exist(dir)) {
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
	std::function<bool(const std::string&, const std::string&)> match_pattern = pre_suf_fix ? startswith : endswith;

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

			int src_fd = open(src_path.c_str(), O_RDONLY);
			if (-1 == src_fd) {
				L_ERR(nullptr, "ERROR: opening file. %s\n", src_path.c_str());
				return -1;
			}

			int dst_fd = open(dst_path.c_str(), O_CREAT | O_WRONLY, 0644);
			if (-1 == src_fd) {
				L_ERR(nullptr, "ERROR: opening file. %s\n", dst_path.c_str());
				return -1;
			}

			while (1) {
				ssize_t bytes = read(src_fd, buffer, 4096);
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


std::string delta_string(long double delta, bool colored) {
	static const char* units[] = { "s", "ms", "\xc2\xb5s", "ns" };
	static const long double scaling[] = { 1, 1e3, 1e6, 1e9 };
	static const char* colors[] = { "\033[1;31m", "\033[1;33m", "\033[0;33m", "\033[0;32m", "\033[0m" };

	delta /= 1e9;  // convert nanoseconds to seconds (as a double)
	long double timespan = delta;

	if (delta < 0) delta = -delta;

	int order = (delta > 0) ? -floorl(floorl(log10l(delta)) / 3) : 3;
	if (order > 3) order = 3;

	char buf[100];
	snprintf(buf, 100, "%s%Lg%s%s", colored ? colors[order] : "", timespan * scaling[order], units[order], colored ? colors[4] : "");
	return buf;
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
				L_NOTICE(nullptr, "Increased maximum number of open files to %llu (it was originally set to %llu)", (unsigned long long) maxfiles, (unsigned long long) oldlimit);
			}
		}
	}
}
