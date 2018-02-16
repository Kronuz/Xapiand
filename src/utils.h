/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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

#include <algorithm>          // for std::count
#include <cctype>             // for tolower, toupper
#include <chrono>             // for system_clock, time_point, duration_cast, seconds
#include <cstdarg>            // for va_list, va_end, va_start
#include <cstdio>             // for size_t, vsnprintf
#include <dirent.h>           // for DIR
#include <math.h>             // for log10, floor, pow
#include <regex>              // for regex
#include <string>             // for std::string
#include <sys/errno.h>        // for errno, EAGAIN, ECONNRESET, EHOSTDOWN, EHOSTUNREACH
#include <sys/types.h>        // for uint64_t, uint16_t, uint8_t, int32_t, uint32_t
#include <type_traits>        // for forward, underlying_type_t
#include <unistd.h>           // for usleep
#include <unordered_map>      // for unordered_map
#include <vector>             // for vector

#include "ev/ev++.h"          // for ::EV_ASYNC, ::EV_CHECK, ::EV_CHILD, ::EV_EMBED
#include "exception.h"        // for InvalidArgument, OutOfRange
#include "split.h"            // for Split
#include "static_str.hh"      // for static_str
#include "string_view.h"      // for string_view


template<class T, class... Args>
struct is_callable {
	template<class U> static auto test(U*p) -> decltype((*p)(std::declval<Args>()...), void(), std::true_type());
	template<class U> static auto test(...) -> decltype(std::false_type());
	static constexpr auto value = decltype(test<T>(nullptr))::value;
};


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

	inline std::string to_string(string_view& str) {
		return std::string(str);
	}

	inline const std::string to_string(const string_view& str) {
		return std::string(str);
	}
}


// Strict converter types
template <typename F, typename... Args>
auto stox_helper(const char* name, F f, string_view str, std::size_t* idx, Args&&... args) {
	auto b = str.data();
	auto e = b + str.size();
	auto ptr = const_cast<char*>(e);
	auto errno_save = errno;
	errno = 0;
	auto r = f(b, &ptr, std::forward<Args>(args)...);
	std::swap(errno, errno_save);
	if (errno_save == ERANGE) {
		throw std::out_of_range(name);
		THROW(OutOfRange, "%s: Out of range value: %s", name, std::string(str).c_str());
	}
	if (ptr == b) {
		THROW(InvalidArgument, "%s: Cannot convert value: %s", name, std::string(str).c_str());
	}
	if (idx) {
		*idx = static_cast<size_t>(ptr - b);
	} else if (ptr != e) {
		THROW(InvalidArgument, "%s: Cannot convert value: %s", name, std::string(str).c_str());
	}
	return r;
}
template <typename T, typename F, typename... Args>
auto stox_helper_numeric_limits(const char* name, F f, string_view str, std::size_t* idx, Args&&... args) {
	auto r = stox_helper(name, f, str, idx, std::forward<Args>(args)...);
	if (r < std::numeric_limits<T>::min() || std::numeric_limits<T>::max() < r) {
		THROW(OutOfRange, "%s: Out of range value: %s", name, std::string(str).c_str());
	}
	return r;
}
#define STOXIFYB(wrapper, name, func) \
inline auto strict_##name(string_view str, std::size_t* idx = nullptr, int base = 10) { \
	return wrapper(#name, std::func, str, idx, base); \
}
#define STOXIFY(wrapper, name, func) \
inline auto strict_##name(string_view str, std::size_t* idx = nullptr) { \
	return wrapper(#name, std::func, str, idx); \
}
STOXIFYB(stox_helper, stoul, strtoul);
STOXIFYB(stox_helper, stoull, strtoull);
STOXIFYB(stox_helper_numeric_limits<int>, stoi, strtol);
STOXIFYB(stox_helper_numeric_limits<unsigned>, stou, strtoul);
STOXIFYB(stox_helper, stol, strtol);
STOXIFYB(stox_helper, stoll, strtoll);
STOXIFY(stox_helper, stof, strtof);
STOXIFY(stox_helper, stod, strtod);
STOXIFY(stox_helper, stold, strtold);
#undef STOXIFY


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

void set_thread_name(string_view name);
std::string get_thread_name();


std::string repr(const void* p, size_t size, bool friendly = true, char quote = '\'', size_t max_size = 0);

inline std::string repr(string_view string, bool friendly = true, char quote = '\'', size_t max_size = 0) {
	return repr(string.data(), string.size(), friendly, quote, max_size);
}

template<typename T, std::size_t N>
inline std::string repr(T (&s)[N], bool friendly = true, char quote = '\'', size_t max_size = 0) {
	return repr(s, N - 1, friendly, quote, max_size);
}


std::string escape(const void* p, size_t size, char quote = '\'');

inline std::string escape(string_view string, char quote = '\'') {
	return escape(string.data(), string.size(), quote);
}

template<typename T, std::size_t N>
inline std::string escape(T (&s)[N], char quote = '\'') {
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


inline std::string vformat_string(string_view format, va_list argptr) {
	// Figure out the length of the formatted message.
	va_list argptr_copy;
	va_copy(argptr_copy, argptr);
	auto len = vsnprintf(nullptr, 0, string_view_data_as_c_str(format), argptr_copy);
	va_end(argptr_copy);

	// Make a string to hold the formatted message.
	std::string str;
	str.resize(len + 1);
	str.resize(vsnprintf(&str[0], len + 1, string_view_data_as_c_str(format), argptr));

	return str;
}


inline std::string _format_string(string_view format, int n, ...) {
	va_list argptr;

	va_start(argptr, n);
	auto str = vformat_string(format, argptr);
	va_end(argptr);

	return str;
}


template<typename... Args>
inline std::string format_string(string_view format, Args&&... args) {
	return _format_string(format, 0, std::forward<Args>(args)...);
}


template<typename T>
inline std::string join_string(const std::vector<T>& values, string_view delimiter, string_view last_delimiter)
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
		result.append(delimiter.data(), delimiter.size());
		result.append(std::to_string(*it));
	}
	if (it != it_e) {
		result.append(last_delimiter.data(), last_delimiter.size());
		result.append(std::to_string(*it));
	}

	return result;
}


template<typename T>
inline std::string join_string(const std::vector<T>& values, string_view delimiter) {
	return join_string(values, delimiter, delimiter);
}


template<typename T, typename UnaryPredicate, typename = std::enable_if_t<is_callable<UnaryPredicate, T>::value>>
inline std::string join_string(const std::vector<T>& values, string_view delimiter, string_view last_delimiter, UnaryPredicate pred) {
	std::vector<T> filtered_values(values.size());
	std::remove_copy_if(values.begin(), values.end(), filtered_values.begin(), pred);
	return join_string(filtered_values, delimiter, last_delimiter);
}


template<typename T, typename UnaryPredicate, typename = std::enable_if_t<is_callable<UnaryPredicate, T>::value>>
inline std::string join_string(const std::vector<T>& values, string_view delimiter, UnaryPredicate pred) {
	return join_string(values, delimiter, delimiter, pred);
}


template<typename T>
inline std::vector<string_view> split_string(string_view value, const T& sep) {
	std::vector<string_view> values;
	Split<T>::split(value, sep, std::back_inserter(values));
	return values;
}


inline std::string indent_string(string_view str, char sep, int level, bool indent_first=true) {
	std::string result;
	result.reserve(((indent_first ? 1 : 0) + std::count(str.begin(), str.end(), '\n')) * level);

	// std::string indentation(level, sep);
	if (indent_first) {
		result.append(level, sep);
	}

	Split<char> lines(str, '\n');
	auto it = lines.begin();
	assert(it != lines.end());
	for (; !it.last(); ++it) {
		const auto& line = *it;
		result.append(line);
		result.append(level, sep);
	}
	const auto& line = *it;
	result.append(line);

	return result;
}


inline std::string center_string(string_view str, int width) {
	std::string result;
	for (auto idx = int((width + 0.5f) / 2 - (str.size() + 0.5f) / 2); idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
	return result;
}

inline std::string right_string(string_view str, int width) {
	std::string result;
	for (auto idx = int(width - str.size()); idx > 0; --idx) {
		result += " ";
	}
	result.append(str.data(), str.size());
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
char* normalize_path(string_view src, char* dst, bool slashed=false);
std::string normalize_path(string_view src, bool slashed=false);
int url_qs(const char *, const char *, size_t);

bool strhasupper(string_view str);

bool isRange(const std::string& str);

bool startswith(string_view text, string_view token);
bool startswith(string_view text, char ch);
bool endswith(string_view text, string_view token);
bool endswith(string_view text, char ch);
void delete_files(string_view path);
void move_files(string_view src, string_view dst);
bool exists(string_view path);
bool build_path(string_view path);
bool build_path_index(string_view path_index);

DIR* opendir(string_view path, bool create);
void find_file_dir(DIR* dir, File_ptr& fptr, string_view pattern, bool pre_suf_fix);
// Copy all directory if file_name and new_name are empty
int copy_file(string_view src, string_view dst, bool create=true, string_view file_name="", string_view new_name="");

std::string bytes_string(size_t bytes, bool colored=false);
std::string small_time_string(long double seconds, bool colored=false);
std::string time_string(long double seconds, bool colored=false);
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
