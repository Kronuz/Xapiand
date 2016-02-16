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

#include "log.h"
#include "database.h"
#include "namegen.h"
#include "hash/md5.h"
#include "xapiand.h"

#include <cassert>
#include <cstdlib>
#include <string>
#include <thread>
#include <mutex>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <xapian.h>
#include <unistd.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h> /* for IPPROTO_TCP */
#include <netinet/tcp.h> /* for TCP_NODELAY */


#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#ifdef HAVE_PTHREAD_NP_H
#include <pthread_np.h>
#endif
#endif

#define STATE_ERR -1
#define STATE_CM0 0
#define STATE_CMD 1
#define STATE_UPL 2 /* case _upload */
#define STATE_NSP 3
#define STATE_PTH 4
#define STATE_HST 5

#define HTTP_UPLOAD "_upload"
#define HTTP_UPLOAD_SIZE 7


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
	return repr(string.c_str(), string.size(), friendly, max_size);
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


int url_qs(const char *name, const char *qs, size_t size, parser_query_t *par) {
	const char *nf = qs + size;
	const char *n1, *n0;
	const char *v0 = nullptr;

	if (par->offset == nullptr) {
		n0 = n1 = qs;
	} else {
		n0 = n1 = par->offset + par->length;
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
								par->offset = v0 + 1;
								par->length = v1 - v0 - 1;
								return 0;
							}
							++v1;
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
					v0 = nullptr;
				}
		}
		++n1;
	}
	return -1;
}


int url_path(const char* ni, size_t size, parser_url_path_t *par) {
	const char *nf = ni + size;
	const char *n0, *n1, *n2 = nullptr;
	int state, direction;
	size_t length;

	if (par->offset == nullptr) {
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
						++state;
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
						if (cn) ++n1;
						par->offset = n1;
						return state;
					case STATE_HST:
						length = n1 - n0;
						par->off_host = n0;
						par->len_host = length;
						state = length ? 0 : STATE_ERR;
						if (cn) ++n1;
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
						++state;
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
						++state;
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
						++state;
						n0 = n1;
						break;
				}
				break;
		}
		n1 += direction;
	}
	return -1;
}


void to_upper(std::string& str) {
	for (auto& c : str) c = toupper(c);
}


void to_lower(std::string& str) {
	for (auto& c : str) c = tolower(c);
}


std::string prefixed(const std::string& term, const std::string& prefix) {
	if (isupper(term.at(0))) {
		return (prefix.empty()) ? term : prefix + ":" + term;
	}

	return prefix + term;
}


unsigned get_slot(const std::string& name) {
	MD5 md5;
	// We are left with the last 8 characters.
	std::string _md5(md5(strhasupper(name) ? upper_string(name) : name), 24, 8);
	unsigned slot = static_cast<unsigned int>(std::stoul(_md5, nullptr, 16));
	if (slot < DB_SLOT_RESERVED) {
		slot += DB_SLOT_RESERVED;
	} else if (slot == Xapian::BAD_VALUENO) {
		slot = 0xfffffffe;
	}
	return slot;
}


std::string get_prefix(const std::string& name, const std::string& prefix, char type) {
	std::string slot(get_slot_hex(name));
	// Mapped [0-9] -> [A-J] and [A-F] -> [R-W]
	for (auto& c : slot) c += 17;

	std::string res(prefix);
	res.append(1, toupper(type));
	return res + slot;
}


std::string get_slot_hex(const std::string& name) {
	MD5 md5;
	// We are left with the last 8 characters.
	std::string _md5(upper_string(md5(strhasupper(name) ? upper_string(name): name), 24, 8));

	return _md5;
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
	return std::regex_match(str, m, numeric_re) && static_cast<size_t>(m.length(0)) == str.size();
}


bool startswith(const std::string& text, const std::string& token) {
	if (text.length() < token.length()) {
		return false;
	}

	return text.compare(0, token.length(), token) == 0;
}


bool endswith(const std::string& text, const std::string& token) {
	if (token.size() > text.size()) return false;
	return std::equal(text.begin() + text.size() - token.size(), text.end(), token.begin());
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


void add_stats_min(uint16_t start, uint16_t end, std::vector<uint64_t>& cnt, std::vector<double>& tm_cnt, times_row_t& stats_cnt_cpy) {
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


void add_stats_sec(uint8_t start, uint8_t end, std::vector<uint64_t>& cnt, std::vector<double>& tm_cnt, times_row_t& stats_cnt_cpy) {
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

	if (rmdir(src.c_str()) != 0) {
		L_ERR(nullptr, "Directory %s could not be deleted", src.c_str());
	}
}


inline bool exist(const std::string& path) {
	struct stat buffer;
	return stat(path.c_str(), &buffer) == 0;
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
		for (const auto& _dir : directories) {
			dir = dir + _dir + "/";
			if (mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
				continue;
			} else {
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


int strict_stoi(const std::string& str) {
	if (str.substr(str.at(0) == '-').find_first_not_of("0123456789") == std::string::npos) {
		return std::stoi(str, nullptr, 10);
	}
	throw std::invalid_argument("Can not convert value: " + str);
}


void stringTokenizer(const std::string& str, const std::string& delimiter, std::vector<std::string>& tokens) {
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


unsigned levenshtein_distance(const std::string& str1, const std::string& str2) {
	const size_t len1 = str1.size(), len2 = str2.size();
	std::vector<unsigned> col(len2 + 1), prev_col(len2 + 1);

	for (unsigned i = 0; i < prev_col.size(); ++i) {
		prev_col[i] = i;
	}

	for (unsigned i = 0; i < len1; ++i) {
		col[0] = i + 1;
		for (unsigned j = 0; j < len2; ++j) {
			col[j + 1] = std::min(std::min(prev_col[j + 1] + 1, col[j] + 1), prev_col[j] + (str1[i] == str2[j] ? 0 : 1));
		}
		col.swap(prev_col);
	}

	return prev_col[len2];
}


std::string delta_string(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end) {
	static const char *units[] = { "s", "ms", "\xc2\xb5s", "ns" };
	static const long double scaling[] = { 1, 1e3, 1e6, 1e9 };

	long double delta = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

	delta /= 1e9;  // convert nanoseconds to seconds (as a double)
	long double timespan = delta;

	if (delta < 0) delta = -delta;

	int order = (delta > 0) ? -floorl(log10l(delta)) / 3 : 3;
	if (order > 3) order = 3;

	timespan = (timespan * scaling[order] * 1000.0 + 0.5) / 1000.0;

	char buf[100];
	snprintf(buf, 100, "%Lg%s", timespan, units[order]);
	return buf;
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
