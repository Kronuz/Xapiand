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

#pragma once

#include "xapiand.h"

#include <arpa/inet.h>        // for inet_ntop
#include <cerrno>             // for EAGAIN, ECONNRESET, EHOSTDOWN, EHOSTUNREACH
#include <chrono>             // for std::chrono
#include <cmath>              // for std::log10, std::floor, std::pow
#include <cstddef>            // for std::size_t
#include <cstdint>            // for std::uint64_t, std::int32_t
#include <string>             // for std::string
#include "string_view.hh"     // for std::string_view
#include <thread>             // for std::thread, std::this_thread
#include <type_traits>        // for std::underlying_type_t
#include <unistd.h>           // for usleep
#include <vector>             // for std::vector

#include "ev/ev++.h"          // for ::EV_ASYNC, ::EV_CHECK, ::EV_CHILD, ::EV_EMBED
#include "escape.h"           // for repr() and escape()

#include "string.hh"
#include "fs.h"

// Reversed iterable
template <typename T> struct reversion_wrapper { T& iterable; };
template <typename T> auto begin (reversion_wrapper<T> w) { return std::rbegin(w.iterable); }
template <typename T> auto end (reversion_wrapper<T> w) { return std::rend(w.iterable); }
template <typename T> reversion_wrapper<T> reversed(T&& iterable) { return { iterable }; }

// It'll return the enum's underlying type.
template<typename E>
static inline constexpr auto toUType(E enumerator) noexcept {
	return static_cast<std::underlying_type_t<E>>(enumerator);
}

template<typename T, std::size_t N>
static inline constexpr std::size_t arraySize(T (&)[N]) noexcept {
	return N;
}

double random_real(double initial, double last);
std::uint64_t random_int(std::uint64_t initial, std::uint64_t last);

void set_thread_name(const std::string& name);
const std::string& get_thread_name(std::thread::id thread_id);
const std::string& get_thread_name();


static inline std::string fast_inet_ntop4(const struct in_addr& addr) {
	// char ip[INET_ADDRSTRLEN];
	// inet_ntop(AF_INET, &addr.sin_addr, ip, INET_ADDRSTRLEN);
	// return std::string(ip);
	return string::format("%d.%d.%d.%d",
		addr.s_addr & 0xff,
		(addr.s_addr >> 8) & 0xff,
		(addr.s_addr >> 16) & 0xff,
		(addr.s_addr >> 24) & 0xff);
}


std::string name_generator();

bool strhasupper(std::string_view str);

bool isRange(std::string_view str);

void _tcp_nopush(int sock, int optval);

size_t get_open_files_per_proc();
size_t get_max_files_per_proc();
size_t get_open_files_system_wide();

std::string check_compiler();
std::string check_OS();
std::string check_architecture();

static inline void tcp_nopush(int sock) {
	_tcp_nopush(sock, 1);
}

static inline void tcp_push(int sock) {
	_tcp_nopush(sock, 0);
}

namespace epoch {
	template<typename Period = std::chrono::seconds>
	static inline auto now() noexcept {
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
		mul = 1000000 / static_cast<unsigned long long>(std::pow(10, std::floor(std::log10(delta))));
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
static inline unsigned long long
time_point_to_ullong(std::chrono::time_point<T> t) {
	return Clk::clk().time_point_to_ullong<T>(t);
}


template <typename T=std::chrono::system_clock>
static inline std::chrono::time_point<T>
time_point_from_ullong(unsigned long long t) {
	return Clk::clk().time_point_from_ullong<T>(t);
}


static inline std::string readable_revents(int revents) {
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
	return string::join(values, " | ");
}


template<typename T, typename M, typename = std::enable_if_t<std::is_integral<std::decay_t<T>>::value && std::is_integral<std::decay_t<M>>::value>>
static inline M modulus(T val, M mod) {
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


static constexpr inline int
hexdigit(char c) noexcept {
	constexpr int _[256]{
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
	return _[static_cast<unsigned char>(c)];
}

// converts the two hexadecimal characters to an int (a byte)
static constexpr inline int
hexdec(const char** ptr) noexcept {
	auto pos = *ptr;
	auto a = hexdigit(*pos++);
	auto b = hexdigit(*pos++);
	if (a == -1 || b == -1) {
		return -1;
	}
	*ptr = pos;
	return a << 4 | b;
}
