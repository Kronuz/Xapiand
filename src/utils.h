/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "xapiand.h"

#include <cctype>             // for tolower, toupper
#include <chrono>             // for system_clock, time_point, duration_cast, seconds
#include <cstdio>             // for size_t, vsnprintf
#include <dirent.h>           // for DIR
#include <math.h>             // for log10, floor, pow
#include <regex>              // for regex
#include <string>             // for string, allocator
#include <sys/errno.h>        // for EAGAIN, ECONNRESET, EHOSTDOWN, EHOSTUNREACH
#include <sys/types.h>        // for uint64_t, uint16_t, uint8_t, int32_t, uint32_t
#include <type_traits>        // for forward, underlying_type_t
#include <unistd.h>           // for usleep
#include <unordered_map>      // for unordered_map
#include <vector>             // for vector

#include "ev/ev++.h"    // for ::EV_ASYNC, ::EV_CHECK, ::EV_CHILD, ::EV_EMBED
#include "exception.h"  // for InvalidArgument, OutOfRange
#include "split.h"      // for Split


template<class T, class... Args>
struct is_callable {
	template<class U> static auto test(U*p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
	template<class U> static auto test(...) -> decltype(std::false_type());
	static constexpr auto value = decltype(test<T>(nullptr))::value;
};


/* Wrapper to get c_str() from either std::string or a raw const char* */
class cstr {
	const char* _str;
public:
	explicit cstr(const std::string &s) : _str(s.c_str())  { }
	explicit cstr(const char* s) : _str(s) { }
	operator const char* () {
		return _str;
	}
	const char* c_str() {
		return _str;
	}
};


/* Strict converter for unsigned types */
#define stoux(func, s) \
	[](const std::string& str) { \
		if (str.empty() || str[0] < '0' || str[0] > '9') { \
			THROW(InvalidArgument, "Cannot convert value: '%s'", str.c_str()); \
		} else { \
			std::size_t sz; \
			try { \
				auto ret = (func)(str, &sz); \
				if (sz != str.length()) { \
					THROW(InvalidArgument, "Cannot convert value: %s", str.c_str()); \
				} \
				return ret; \
			} catch (const std::out_of_range&) { \
				THROW(OutOfRange, "Out of range value: %s", str.c_str()); \
			} catch (const std::invalid_argument&) { \
				THROW(InvalidArgument, "Cannot convert value: %s", str.c_str()); \
			} \
		} \
	}(s)


/* Strict converter for signed types */
#define stosx(func, s) \
	[](const std::string& str) { \
		if (str.empty() || str[0] == ' ') { \
			THROW(InvalidArgument, "Cannot convert value: '%s'", str.c_str()); \
		} else { \
			std::size_t sz; \
			try { \
				auto ret = (func)(str, &sz); \
				if (sz != str.length()) { \
					THROW(InvalidArgument, "Cannot convert value: %s", str.c_str()); \
				} \
				return ret; \
			} catch (const std::out_of_range&) { \
				THROW(OutOfRange, "Out of range value: %s", str.c_str()); \
			} catch (const std::invalid_argument&) { \
				THROW(InvalidArgument, "Cannot convert value: %s", str.c_str()); \
			} \
		} \
	}(s)


#define strict_stoul(s)     stoux(std::stoul, s)
#define strict_stoull(s)    stoux(std::stoull, s)
#define strict_stoi(s)      stosx(std::stoi, s)
#define strict_stol(s)      stosx(std::stol, s)
#define strict_stoll(s)     stosx(std::stoll, s)
#define strict_stof(s)      stosx(std::stof, s)
#define strict_stod(s)      stosx(std::stod, s)
#define strict_stold(s)     stosx(std::stold, s)
#define stox(func, s)       strict_##func(#s)


struct File_ptr {
	struct dirent *ent;

	File_ptr()
		: ent(nullptr) { }
};


// It'll return the enum's underlying type.
template<typename E>
inline constexpr auto toUType(E enumerator) noexcept {
	return static_cast<std::underlying_type_t<E>>(enumerator);
}

template<typename T, std::size_t N>
inline constexpr std::size_t arraySize(T (&)[N]) noexcept {
	return N;
}

double random_real(double initial, double last);
uint64_t random_int(uint64_t initial, uint64_t last);

void set_thread_name(const std::string& name);
std::string get_thread_name();


std::string repr(const void* p, size_t size, bool friendly = true, bool quote = true, size_t max_size = 0);

inline std::string repr(const std::string& string, bool friendly = true, bool quote = true, size_t max_size = 0) {
	return repr(string.c_str(), string.length(), friendly, quote, max_size);
}

template<typename T, std::size_t N>
inline std::string repr(T (&s)[N], bool friendly = true, bool quote = true, size_t max_size = 0) {
	return repr(s, N - 1, friendly, quote, max_size);
}


std::string escape(const void* p, size_t size, bool quote = true);

inline std::string escape(const std::string& string, bool quote = true) {
	return escape(string.c_str(), string.length(), quote);
}

template<typename T, std::size_t N>
inline std::string escape(T (&s)[N], bool quote = true) {
	return escape(s, N - 1, quote);
}


inline bool ignored_errorno(int e, bool tcp, bool udp) {
	switch(e) {
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
			return true;  //  Ignore error
		case EINTR:
		case EPIPE:
		case EINPROGRESS:
			return tcp;  //  Ignore error

		case ENETDOWN:
		case EPROTO:
		case ENOPROTOOPT:
		case EHOSTDOWN:
#ifdef ENONET  // Linux-specific
		case ENONET:
#endif
		case EHOSTUNREACH:
		case EOPNOTSUPP:
		case ENETUNREACH:
		case ECONNRESET:
			return udp;  //  Ignore error on UDP sockets

		default:
			return false;  // Do not ignore error
	}
}


std::string name_generator();
int32_t jump_consistent_hash(uint64_t key, int32_t num_buckets);


namespace std {
	template<typename T, int N>
	inline std::string to_string(const T (&s)[N])
	{
		return std::string(s, N - 1);
	}

	inline auto& to_string(std::string& str) {
		return str;
	}

	inline const auto& to_string(const std::string& str) {
		return str;
	}
}


inline std::string vformat_string(const char* format, va_list argptr) {
	// Figure out the length of the formatted message.
	va_list argptr_copy;
	va_copy(argptr_copy, argptr);
	auto len = vsnprintf(nullptr, 0, format, argptr_copy);
	va_end(argptr_copy);

	// Make a string to hold the formatted message.
	std::string str;
	str.resize(len + 1);
	vsnprintf(&str[0], len + 1, format, argptr);
	str.resize(len);

	return str;
}


inline std::string _format_string(const char* format, ...) {
	va_list argptr;

	va_start(argptr, format);
	auto str = vformat_string(format, argptr);
	va_end(argptr);

	return str;
}


template<typename F, typename... Args>
inline std::string format_string(F&& format, Args&&... args) {
	return _format_string(cstr(std::forward<F>(format)), std::forward<Args>(args)...);
}


template<typename T>
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter, const std::string& last_delimiter)
{
	auto it = values.begin();
	auto it_e = values.end();

	auto rit = values.rbegin();
	auto rit_e = values.rend();
	if (rit != rit_e) ++rit;
	auto it_l = rit != rit_e ? rit.base() : it_e;

	std::string result;

	if (it != it_e) {
		result.append(std::to_string(*it++));
	}
	for (; it != it_l; ++it) {
		result.append(delimiter);
		result.append(std::to_string(*it));
	}
	if (it != it_e) {
		result.append(last_delimiter);
		result.append(std::to_string(*it));
	}

	return result;
}


template<typename T>
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter) {
	return join_string(values, delimiter, delimiter);
}


template<typename T, typename UnaryPredicate, typename = std::enable_if_t<is_callable<UnaryPredicate, T>::value>>
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter, const std::string& last_delimiter, UnaryPredicate pred) {
	std::vector<T> filtered_values(values.size());
	std::remove_copy_if(values.begin(), values.end(), filtered_values.begin(), pred);
	return join_string(filtered_values, delimiter, last_delimiter);
}


template<typename T, typename UnaryPredicate, typename = std::enable_if_t<is_callable<UnaryPredicate, T>::value>>
inline std::string join_string(const std::vector<T>& values, const std::string& delimiter, UnaryPredicate pred) {
	return join_string(values, delimiter, delimiter, pred);
}


template<typename T>
inline std::vector<std::string> split_string(const std::string& value, const T& sep) {
	std::vector<std::string> values;
	Split<T>::split(value, sep, std::back_inserter(values));
	return values;
}


inline std::string indent_string(const std::string& str, char sep, int level, bool indent_first=true) {
	std::string ret = str;

	std::string indentation(level, sep);
	if (indent_first) ret.insert(0, indentation);

	std::string::size_type pos = ret.find('\n');
	while (pos != std::string::npos) {
		ret.insert(pos + 1, indentation);
		pos = ret.find('\n', pos + level + 1);
	}

	return ret;
}


inline std::string center_string(const std::string& str, int width) {
	std::string result;
	for (auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2); idx > 0; --idx) {
		result += " ";
	}
	result += str;
	return result;
}

inline std::string right_string(const std::string& str, int width) {
	std::string result;
	for (auto idx = int(width - str.size()); idx > 0; --idx) {
		result += " ";
	}
	result += str;
	return result;
}

template<typename... Args>
inline std::string upper_string(Args&&... args) {
	std::string tmp(std::forward<Args>(args)...);
	for (auto& c : tmp) c = toupper(c);
	return tmp;
}

template<typename... Args>
inline std::string lower_string(Args&&... args) {
	std::string tmp(std::forward<Args>(args)...);
	for (auto& c : tmp) c = tolower(c);
	return tmp;
}

void to_upper(std::string& str);
void to_lower(std::string& str);

char* normalize_path(const char* src, const char* end, char* dst, bool slashed=false);
char* normalize_path(const std::string& src, char* dst, bool slashed=false);
std::string normalize_path(const std::string& src, bool slashed=false);
int url_qs(const char *, const char *, size_t);

bool strhasupper(const std::string& str);

bool isRange(const std::string& str);

bool startswith(const std::string& text, const std::string& token);
bool startswith(const std::string& text, char ch);
bool endswith(const std::string& text, const std::string& token);
bool endswith(const std::string& text, char ch);
void delete_files(const std::string& path);
void move_files(const std::string& src, const std::string& dst);
bool exists(const std::string& path);
bool build_path(const std::string& path);
bool build_path_index(const std::string& path_index);

void find_file_dir(DIR* dir, File_ptr& fptr, const std::string& pattern, bool pre_suf_fix);
DIR* opendir(const char* filename, bool create);
// Copy all directory if file_name and new_name are empty
int copy_file(const std::string& src, const std::string& dst, bool create=true, const std::string& file_name=std::string(), const std::string& new_name=std::string());

std::string bytes_string(size_t bytes, bool colored=false);
std::string delta_string(long double nanoseconds, bool colored=false);
std::string delta_string(const std::chrono::time_point<std::chrono::system_clock>& start, const std::chrono::time_point<std::chrono::system_clock>& end, bool colored=false);

void _tcp_nopush(int sock, int optval);

unsigned long long file_descriptors_cnt();

inline void tcp_nopush(int sock) {
	_tcp_nopush(sock, 1);
}

inline void tcp_push(int sock) {
	_tcp_nopush(sock, 0);
}

namespace epoch {
	template<typename Period = std::chrono::seconds>
	inline auto now() noexcept {
		return std::chrono::duration_cast<Period>(std::chrono::system_clock::now().time_since_epoch()).count();
	}
}


struct Clk {
	unsigned long long mul;

	Clk() {
		auto a = std::chrono::system_clock::now();
		usleep(5000);
		auto b = std::chrono::system_clock::now();
		auto delta = *reinterpret_cast<unsigned long long*>(&b) - *reinterpret_cast<unsigned long long*>(&a);
		mul = 1000000 / static_cast<unsigned long long>(pow(10, floor(log10(delta))));
	}

	template <typename T>
	unsigned long long time_point_to_ullong(std::chrono::time_point<T> t) const {
		return *reinterpret_cast<unsigned long long*>(&t) * mul;
	}

	template <typename T=std::chrono::system_clock>
	std::chrono::time_point<T> time_point_from_ullong(unsigned long long t) const {
		t /= mul;
		return *reinterpret_cast<std::chrono::time_point<T>*>(&t);
	}

	static const Clk& clk() {
		static const Clk clk;
		return clk;
	}
};


template <typename T>
inline
unsigned long long
time_point_to_ullong(std::chrono::time_point<T> t) {
	return Clk::clk().time_point_to_ullong<T>(t);
}


template <typename T=std::chrono::system_clock>
inline
std::chrono::time_point<T>
time_point_from_ullong(unsigned long long t) {
	return Clk::clk().time_point_from_ullong<T>(t);
}


inline std::string readable_revents(int revents) {
	std::vector<std::string> values;
	if (revents == EV_NONE) values.push_back("EV_NONE");
	if ((revents & EV_READ) == EV_READ) values.push_back("EV_READ");
	if ((revents & EV_WRITE) == EV_WRITE) values.push_back("EV_WRITE");
	if ((revents & EV_TIMEOUT) == EV_TIMEOUT) values.push_back("EV_TIMEOUT");
	if ((revents & EV_TIMER) == EV_TIMER) values.push_back("EV_TIMER");
	if ((revents & EV_PERIODIC) == EV_PERIODIC) values.push_back("EV_PERIODIC");
	if ((revents & EV_SIGNAL) == EV_SIGNAL) values.push_back("EV_SIGNAL");
	if ((revents & EV_CHILD) == EV_CHILD) values.push_back("EV_CHILD");
	if ((revents & EV_STAT) == EV_STAT) values.push_back("EV_STAT");
	if ((revents & EV_IDLE) == EV_IDLE) values.push_back("EV_IDLE");
	if ((revents & EV_CHECK) == EV_CHECK) values.push_back("EV_CHECK");
	if ((revents & EV_PREPARE) == EV_PREPARE) values.push_back("EV_PREPARE");
	if ((revents & EV_FORK) == EV_FORK) values.push_back("EV_FORK");
	if ((revents & EV_ASYNC) == EV_ASYNC) values.push_back("EV_ASYNC");
	if ((revents & EV_EMBED) == EV_EMBED) values.push_back("EV_EMBED");
	if ((revents & EV_ERROR) == EV_ERROR) values.push_back("EV_ERROR");
	if ((revents & EV_UNDEF) == EV_UNDEF) values.push_back("EV_UNDEF");
	return join_string(values, " | ");
}


template<typename T, typename M, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value && std::is_integral<std::decay_t<M>>::value>>
inline M modulus(T val, M mod) {
	if (mod < 0) {
		throw std::invalid_argument("Modulus must be positive");
	}
	if (val < 0) {
		val = -val;
		auto m = static_cast<M>(val) % mod;
		return m ? mod - m : m;
	}
	return static_cast<M>(val) % mod;
}


template <typename T>
inline std::string get_map_keys(const std::unordered_map<std::string, T>& map) {
	std::string res("{ ");
	char comma[3] = { '\0', ' ', '\0' };
	for (const auto& p : map) {
		res.append(comma).append(repr(p.first));
		comma[0] = ',';
	}
	res.append(" }");
	return res;
}


// converts the two hexadecimal characters to an int (a byte)
inline int hexdec(const char** ptr) noexcept {
	constexpr const int _[256] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,

		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,

		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
	};
	auto pos = *ptr;
	auto a = _[static_cast<unsigned char>(*pos++)];
	auto b = _[static_cast<unsigned char>(*pos++)];
	if (a == -1 || b == -1) {
		return -1;
	}
	*ptr = pos;
	return a << 4 | b;
}
