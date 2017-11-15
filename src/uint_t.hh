/*
uint_t.hh
An unsigned integer type for C++

Copyright (c) 2017 German Mendez Bravo (Kronuz) @ german dot mb at gmail.com
Copyright (c) 2013 - 2017 Jason Lee @ calccrypto at gmail.com

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

With much help from Auston Sterling

Thanks to Stefan Deigmüller for finding
a bug in operator*.

Thanks to François Dessenne for convincing me
to do a general rewrite of this class.

Germán Mández Bravo (Kronuz) converted Jason Lee's uint128_t
to header-only and extended to arbitrary bit length.
*/

#ifndef __uint_t__
#define __uint_t__

#include <vector>
#include <string>
#include <utility>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <type_traits>


// Compatibility inlines
#ifndef __has_builtin         // Optional of course
#define __has_builtin(x) 0    // Compatibility with non-clang compilers
#endif

#if defined _MSC_VER
#  define HAVE___ADDCARRY_U64
#  define HAVE___SUBBORROW_U64
#  define HAVE___UMUL128
#  include <intrin.h>
  typedef unsigned __int64 uint64_t;
#endif

#if (defined(__clang__) && __has_builtin(__builtin_clzll)) || (defined(__GNUC__ ) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)))
#  define HAVE____BUILTIN_CLZLL
#endif
#if (defined(__clang__) && __has_builtin(__builtin_addcll)) || (defined(__GNUC__ ) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)))
#  define HAVE____BUILTIN_ADDCLL
#endif
#if (defined(__clang__) && __has_builtin(__builtin_subcll)) || (defined(__GNUC__ ) && (__GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 3)))
#  define HAVE____BUILTIN_SUBCLL
#endif

#if defined __SIZEOF_INT128__
#define HAVE____INT64_T
#endif


inline uint64_t bits(uint64_t x) {
#if defined HAVE____BUILTIN_CLZLL
	return x ? 64 - __builtin_clzll(x) : 1;
#else
	uint64_t c = x ? 0 : 1;
	while (x) {
		x >>= 1;
		++c;
	}
	return c;
#endif
}

inline uint64_t muladd(uint64_t x, uint64_t y, uint64_t c, uint64_t* result) {
#if defined HAVE___UMUL128 && defined HAVE___ADDCARRY_U64
	uint64_t h;
	uint64_t l = _umul128(x, y, &h);  // _umul128(x, y, *hi) -> lo
	return h + _addcarry_u64(0, l, c, result);  // _addcarry_u64(carryin, x, y, *sum) -> carryout
#elif defined HAVE____INT64_T
	auto r = static_cast<__uint128_t>(x) * static_cast<__uint128_t>(y) + static_cast<__uint128_t>(c);
	*result = r;
	return r >> 64;
#else
	uint64_t x0 = x & 0xffffffff;
	uint64_t x1 = x >> 32;
	uint64_t y0 = y & 0xffffffff;
	uint64_t y1 = y >> 32;

	uint64_t u = (x0 * y0) + (c & 0xffffffff);
	uint64_t v = (x1 * y0) + (u >> 32) + (c >> 32);
	uint64_t w = (x0 * y1) + (v & 0xffffffff);

	*result = (w << 32) + (u & 0xffffffff); // low
	return (x1 * y1) + (v >> 32) + (w >> 32); // high
#endif
}

inline uint64_t addcarry(uint64_t x, uint64_t y, uint64_t c, uint64_t* result) {
#if defined HAVE___ADDCARRY_U64
	return _addcarry_u64(c, x, y, result);  // _addcarry_u64(carryin, x, y, *sum) -> carryout
#elif defined HAVE____BUILTIN_ADDCLL
	uint64_t carryout;
	*result = __builtin_addcll(x, y, c, &carryout);  // __builtin_addcll(x, y, carryin, *carryout) -> sum
	return carryout;
#elif defined HAVE____INT64_T
	auto r = static_cast<__uint128_t>(x) + static_cast<__uint128_t>(y) + static_cast<__uint128_t>(c);
	*result = r;
	return static_cast<bool>(r >> 64);
#else
	uint64_t x0 = x & 0xffffffff;
	uint64_t x1 = x >> 32;
	uint64_t y0 = y & 0xffffffff;
	uint64_t y1 = y >> 32;

	auto u = x0 + y0 + c;
	auto v = x1 + y1 + static_cast<bool>(u >> 32);
	*result = (v << 32) + (u & 0xffffffff);
	return static_cast<bool>(v >> 32);
#endif
}

inline uint64_t subborrow(uint64_t x, uint64_t y, uint64_t c, uint64_t* result) {
#if defined HAVE___SUBBORROW_U64
	return _subborrow_u64(c, x, y, result);  // _addcarry_u64(carryin, x, y, *sum) -> carryout
#elif defined HAVE____BUILTIN_SUBCLL
	uint64_t carryout;
	*result = __builtin_subcll(x, y, c, &carryout);  // __builtin_addcll(x, y, carryin, *carryout) -> sum
	return carryout;
#elif defined HAVE____INT64_T
	auto r = static_cast<__uint128_t>(x) - static_cast<__uint128_t>(y) - static_cast<__uint128_t>(c);
	*result = r;
	return static_cast<bool>(r >> 64);
#else
	uint64_t x0 = x & 0xffffffff;
	uint64_t x1 = x >> 32;
	uint64_t y0 = y & 0xffffffff;
	uint64_t y1 = y >> 32;

	auto u = x0 - y0 - c;
	auto v = x1 - y1 - static_cast<bool>(u >> 32);
	*result = (v << 32) + (u & 0xffffffff);
	return static_cast<bool>(v >> 32);
#endif
}


class uint_t;

namespace std {  // This is probably not a good idea
	// Give uint_t type traits
	template <> struct is_arithmetic <uint_t> : std::true_type {};
	template <> struct is_integral   <uint_t> : std::true_type {};
	template <> struct is_unsigned   <uint_t> : std::true_type {};
}

class uint_t {
	private:
		bool _carry;
		std::vector<uint64_t> _value;

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		void _uint_t(const T & value) {
			_value.push_back(static_cast<uint64_t>(value));
		}

		template <typename T, typename... Args, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		void _uint_t(const T & value, Args... args) {
		    _uint_t(args...);
			_value.push_back(static_cast<uint64_t>(value));
		}

		void trim(uint64_t mask = 0) {
			auto rit = _value.rbegin();
			auto rit_e = _value.rend();

			// Masks the last value of internal vector
			mask &= 0x3f;
			if (mask && rit != rit_e) {
				*rit &= (1ULL << mask) - 1;
			}

			// Removes all unused zeros from the internal vector
			for (; rit != rit_e; ++rit) {
				if (*rit) break;
			}
			_value.resize(rit_e - rit);
		}

		int compare(const uint_t& rhs) const {
			if (_value.size() > rhs._value.size()) return 1;
			if (_value.size() < rhs._value.size()) return -1;
			auto rit = _value.rbegin();
			auto rit_e = _value.rend();
			auto rhs_rit = rhs._value.rbegin();
			for (; rit != rit_e; ++rit, ++rhs_rit) {
				auto& a = *rit;
				auto& b = *rhs_rit;
				if (a > b) return 1;
				if (a < b) return -1;
			}
			return 0;
		}

	public:
		// Constructors
		uint_t()
			: _carry(false) { }

		uint_t(const uint_t& o)
			: _carry(o._carry),
			  _value(o._value) { }

		uint_t(uint_t&& o)
			: _carry(std::move(o._carry)),
			  _value(std::move(o._value)) { }

		template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint_t(const T & value)
			: _carry(false) {
			if (value) {
				_value.push_back(static_cast<uint64_t>(value));
			}
		}

		template <typename T, typename... Args, typename = typename std::enable_if<std::is_integral<T>::value>::type>
		uint_t(const T & value, Args... args)
			: _carry(false) {
		    _uint_t(args...);
			_value.push_back(static_cast<uint64_t>(value));
			trim();
		}

		explicit uint_t(const char* bytes, size_t size, size_t base)
			: _carry(false) {
			if (base >= 2 && base <= 36) {
				for (; size; --size, ++bytes) {
					uint8_t d = std::tolower(*bytes);
					if (std::isdigit(d)) { // 0-9
						d -= '0';
					} else {
						d -= 'a' - 10;
					}
					if (d >= base) {
						throw std::runtime_error("Error: Not a digit in base " + std::to_string(base) + ": '" + std::string(1, *bytes) + "'");
					}
					*this = (*this * base) + d;
				}
			} else if (base == 256) {
				bytes += size - 1;
				while (size) {
					uint64_t num = 0;
					uint8_t* ptr = reinterpret_cast<uint8_t*>(&num);
					uint8_t* ptr_e = ptr + 8;
					for (; size && ptr < ptr_e; --size) {
						*ptr++ = *bytes--;
					}
					_value.push_back(num);
				}
			} else {
				throw std::runtime_error("Error: Cannot convert from base " + std::to_string(base));
			}
		}

		template <typename T, size_t N>
		explicit uint_t(T (&s)[N], size_t base=10)
			: uint_t(s, N - 1, base) { }

		template <typename T>
		explicit uint_t(const std::vector<T>& bytes, size_t base=10)
			: uint_t(bytes.data(), bytes.size(), base) { }

		explicit uint_t(const std::string& bytes, size_t base=10)
			: uint_t(bytes.data(), bytes.size(), base) { }

		//  RHS input args only

		// Assignment Operator
		uint_t& operator=(const uint_t& o) {
			_carry = o._carry;
			_value = o._value;
			return *this;
		}
		uint_t& operator=(uint_t&& o) {
			_carry = std::move(o._carry);
			_value = std::move(o._value);
			return *this;
		}

		// Typecast Operators
		explicit operator bool() const {
			return static_cast<bool>(_value.size());
		}
		explicit operator unsigned char() const {
			return static_cast<unsigned char>(_value.size() ? _value.front() : 0);
		}
		explicit operator unsigned short() const {
			return static_cast<unsigned short>(_value.size() ? _value.front() : 0);
		}
		explicit operator unsigned int() const {
			return static_cast<unsigned int>(_value.size() ? _value.front() : 0);
		}
		explicit operator unsigned long() const {
			return static_cast<unsigned long>(_value.size() ? _value.front() : 0);
		}
		explicit operator unsigned long long() const {
			return static_cast<unsigned long long>(_value.size() ? _value.front() : 0);
		}
		explicit operator char() const {
			return static_cast<char>(_value.size() ? _value.front() : 0);
		}
		explicit operator short() const {
			return static_cast<short>(_value.size() ? _value.front() : 0);
		}
		explicit operator int() const {
			return static_cast<int>(_value.size() ? _value.front() : 0);
		}
		explicit operator long() const {
			return static_cast<long>(_value.size() ? _value.front() : 0);
		}
		explicit operator long long() const {
			return static_cast<long long>(_value.size() ? _value.front() : 0);
		}

		// Bitwise Operators
		uint_t operator&(const uint_t& rhs) const {
			uint_t result(*this);
			result &= rhs;
			return result;
		}

		uint_t& operator&=(const uint_t& rhs) {
			if (_value.size() > rhs._value.size()) {
				_value.resize(rhs._value.size());
			}
			auto it = _value.begin();
			auto it_e = _value.end();
			auto rhs_it = rhs._value.begin();
			for (; it != it_e; ++it, ++rhs_it) {
				*it &= *rhs_it;
			}
			trim();
			return *this;
		}

		uint_t operator|(const uint_t& rhs) const {
			uint_t result(*this);
			result |= rhs;
			return result;
		}

		uint_t& operator|=(const uint_t& rhs) {
			if (_value.size() < rhs._value.size()) {
				_value.resize(rhs._value.size(), 0);
			}
			auto it = _value.begin();
			auto rhs_it = rhs._value.begin();
			auto rhs_it_e = rhs._value.end();
			for (; rhs_it != rhs_it_e; ++it, ++rhs_it) {
				*it |= *rhs_it;
			}
			trim();
			return *this;
		}

		uint_t operator^(const uint_t& rhs) const {
			uint_t result(*this);
			result ^= rhs;
			return result;
		}

		uint_t& operator^=(const uint_t& rhs) {
			if (_value.size() < rhs._value.size()) {
				_value.resize(rhs._value.size(), 0);
			}
			auto it = _value.begin();
			auto rhs_it = rhs._value.begin();
			auto rhs_it_e = rhs._value.end();
			for (; rhs_it != rhs_it_e; ++it, ++rhs_it) {
				*it ^= *rhs_it;
			}
			trim();
			return *this;
		}

		uint_t& inv() {
			if (!_value.size()) {
				_value.push_back(0);
			}
			auto b = bits();
			auto it = _value.begin();
			auto it_e = _value.end();
			for (; it != it_e; ++it) {
				*it = ~*it;
			}
			trim(b);
			return *this;
		}

		uint_t operator~() const {
			uint_t result(*this);
			result.inv();
			return result;
		}

		// Bit Shift Operators
		uint_t operator<<(const uint_t& rhs) const {
			uint_t result(*this);
			result <<= rhs;
			return result;
		}

		uint_t& operator<<=(const uint_t& rhs) {
			if (rhs == 0) {
				return *this;
			}
			auto shift = rhs._value.front();
			auto shifts = shift / 64;
			shift = shift % 64;
			if (shift) {
				uint64_t shifted = 0;
				auto it = _value.begin();
				auto it_e = _value.end();
				for (; it != it_e; ++it) {
					auto v = (*it << shift) | shifted;
					shifted = *it >> (64 - shift);
					*it = v;
				}
				if (shifted) {
					_value.push_back(shifted);
				}
			}
			if (shifts) {
				_value.insert(_value.begin(), shifts, 0);
			}
			return *this;
		}

		uint_t operator>>(const uint_t& rhs) const {
			uint_t result(*this);
			result >>= rhs;
			return result;
		}

		uint_t& operator>>=(const uint_t& rhs) {
			if (rhs >= _value.size() * 64) {
				_value.clear();
				return *this;
			} else if (rhs == 0) {
				return *this;
			}
			auto shift = rhs._value.front();
			auto shifts = shift / 64;
			shift = shift % 64;
			if (shifts) {
				_value.erase(_value.begin(), _value.begin() + shifts);
			}
			if (shift) {
				uint64_t shifted = 0;
				auto rit = _value.rbegin();
				auto rit_e = _value.rend();
				for (; rit != rit_e; ++rit) {
					auto v = (*rit >> shift) | shifted;
					shifted = *rit << (64 - shift);
					*rit = v;
				}
				trim();
			}
			return *this;
		}

		// Logical Operators
		bool operator!() const {
			return !static_cast<bool>(*this);
		}

		bool operator&&(const uint_t& rhs) const {
			return static_cast<bool>(*this) && rhs;
		}

		bool operator||(const uint_t& rhs) const {
			return static_cast<bool>(*this) || rhs;
		}

		// Comparison Operators
		bool operator==(const uint_t& rhs) const {
			return compare(rhs) == 0;
		}

		bool operator!=(const uint_t& rhs) const {
			return compare(rhs) != 0;
		}

		bool operator>(const uint_t& rhs) const {
			return compare(rhs) > 0;
		}

		bool operator<(const uint_t& rhs) const {
			return compare(rhs) < 0;
		}

		bool operator>=(const uint_t& rhs) const {
			return compare(rhs) >= 0;
		}

		bool operator<=(const uint_t& rhs) const {
			return compare(rhs) <= 0;
		}

		// Arithmetic Operators
		uint_t operator+(const uint_t& rhs) const {
			uint_t result(*this);
			result += rhs;
			return result;
		}

		uint_t& operator+=(const uint_t& rhs) {
			// First try saving some calculations:
			if (!rhs) {
				return *this;
			}

			if (_value.size() < rhs._value.size()) {
				_value.resize(rhs._value.size(), 0);
			}
			auto it = _value.begin();
			auto it_e = _value.end();
			auto rhs_it = rhs._value.begin();
			auto rhs_it_e = rhs._value.end();
			uint64_t carry = 0;
			for (; it != it_e && rhs_it != rhs_it_e; ++it, ++rhs_it) {
				carry = addcarry(*it, *rhs_it, carry, &*it);
			}
			for (; it != it_e && carry; ++it) {
				carry = addcarry(*it, 0, carry, &*it);
			}
			if (carry) {
				_value.push_back(1);
			}
			_carry = false;
			trim();
			return *this;
		}

		uint_t operator-(const uint_t& rhs) const {
			uint_t result(*this);
			result -= rhs;
			return result;
		}

		uint_t& operator-=(const uint_t& rhs) {
			// First try saving some calculations:
			if (!rhs) {
				return *this;
			}

			if (_value.size() < rhs._value.size()) {
				_value.resize(rhs._value.size(), 0);
			}
			auto it = _value.begin();
			auto it_e = _value.end();
			auto rhs_it = rhs._value.begin();
			auto rhs_it_e = rhs._value.end();
			uint64_t carry = 0;
			for (; it != it_e && rhs_it != rhs_it_e; ++it, ++rhs_it) {
				carry = subborrow(*it, *rhs_it, carry, &*it);
			}
			for (; it != it_e && carry; ++it) {
				carry = subborrow(*it, 0, carry, &*it);
			}
			_carry = carry;
			trim();
			return *this;
		}

		uint_t operator*(const uint_t& rhs) const {
			// First try saving some calculations:
			if (!*this || !rhs) {
				return uint_0();
			} else if (*this == uint_1()) {
				return rhs;
			} else if (rhs == uint_1()) {
				return *this;
			}

			// Long multiplication
			uint_t row, result = 0;
			auto it = _value.begin();
			auto it_e = _value.end();
			size_t zeros = 0;
			for (; it != it_e; ++it) {
				row._value = std::vector<uint64_t>(zeros++, 0); // zeros on the right hand side
				uint64_t carry = 0;
				auto rhs_it = rhs._value.begin();
				auto rhs_it_e = rhs._value.end();
				for (; rhs_it != rhs_it_e; ++rhs_it) {
					uint64_t prod;
					carry = muladd(*it, *rhs_it, carry, &prod);
					row._value.push_back(prod);
				}
				if (carry) {
					row._value.push_back(carry);
				}
				result += row;
			}
			result.trim();
			return result;
		}

		uint_t& operator*=(const uint_t& rhs) {
			*this = *this * rhs;
			return *this;
		}

		static const uint_t uint_0() {
			static uint_t uint_0(0);
			return uint_0;
		}

		static const uint_t uint_1() {
			static uint_t uint_1(1);
			return uint_1;
		}

		std::pair<uint_t, uint_t> divmod(const uint_t& rhs) const {
			// First try saving some calculations:
			if (!rhs) {
				throw std::domain_error("Error: division or modulus by 0");
			} else if (rhs == uint_1()) {
				return std::make_pair(*this, uint_0());
			} else if (*this == rhs) {
				return std::make_pair(uint_1(), uint_0());
			} else if (!*this || *this < rhs) {
				return std::make_pair(uint_0(), *this);
			}

			// Long division
			std::pair<uint_t, uint_t> qr(uint_0(), uint_0());
			for (size_t x = bits(); x > 0; --x) {
				qr.first  <<= uint_1();
				qr.second <<= uint_1();
				if ((*this >> (x - 1U)) & 1) {
					++qr.second;
				}
				if (qr.second >= rhs) {
					qr.second -= rhs;
					++qr.first;
				}
			}
			return qr;
		}

		uint_t operator/(const uint_t& rhs) const {
			return divmod(rhs).first;
		}

		uint_t& operator/=(const uint_t& rhs) {
			*this = *this / rhs;
			return *this;
		}

		uint_t operator%(const uint_t& rhs) const {
			return divmod(rhs).second;
		}

		uint_t& operator%=(const uint_t& rhs) {
			*this = *this % rhs;
			return *this;
		}

		// Increment Operator
		uint_t& operator++() {
			return *this += uint_1();
		}
		uint_t operator++(int) {
			uint_t temp(*this);
			++*this;
			return temp;
		}

		// Decrement Operator
		uint_t& operator--() {
			return *this -= uint_1();
		}
		uint_t operator--(int) {
			uint_t temp(*this);
			--*this;
			return temp;
		}

		// Nothing done since promotion doesn't work here
		uint_t operator+() const {
			return *this;
		}

		// two's complement
		uint_t operator-() const {
			return uint_0() - *this;
		}

		// Get private values
		const uint64_t& operator[](size_t idx) const {
			static const uint64_t zero = 0;
			return idx < _value.size() ? _value[idx] : zero;
		}

		// Get bitsize of value
		size_t bits() const {
			size_t out = 0;
			if (_value.size()) {
				out = (_value.size() - 1) * 64;
				uint64_t ms = _value.back();
				out += ::bits(ms);
			}
			return out;
		}

		// Get string representation of value
		template <typename Result = std::string>
		Result str(size_t base = 10) const {
			if (base >= 2 && base <= 36) {
				Result result;
				if (!*this) {
					result.push_back('0');
				} else {
					std::pair<uint_t, uint_t> qr(*this, uint_0());
					do {
						qr = qr.first.divmod(base);
						if (qr.second < 10) {
							result.push_back((uint8_t)qr.second + '0');
						} else {
							result.push_back((uint8_t)qr.second + 'a' - 10);
						}
					} while (qr.first);
				}
				std::reverse(result.begin(), result.end());
				return result;
			} else if (base == 256) {
				Result result;
				auto it = _value.begin();
				auto it_e = _value.end();
				for (; it != it_e; ++it) {
					result.append(Result(reinterpret_cast<const char*>(&*it), 8));
				}
				auto found = std::find_if(result.rbegin(), result.rend(), [](const char& c) { return c; });
				result.resize(result.rend() - found);
				std::reverse(result.begin(), result.end());
				return result;
			} else {
				throw std::invalid_argument("Base must be in the range [2, 36]");
			}
		}
};

namespace std {  // This is probably not a good idea
	// Make it work with std::string()
	inline std::string to_string(uint_t& num) {
		return num.str();
	}
	inline const std::string to_string(const uint_t& num) {
		return num.str();
	}
};

// lhs type T as first arguemnt
// If the output is not a bool, casts to type T

// Bitwise Operators
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator&(const T & lhs, const uint_t& rhs) {
	return rhs & lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator&=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(rhs & lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator|(const T & lhs, const uint_t& rhs) {
	return rhs | lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator|=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(rhs | lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator^(const T & lhs, const uint_t& rhs) {
	return rhs ^ lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator^=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(rhs ^ lhs);
}

// Bitshift operators
inline uint_t operator<<(const bool     & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const uint8_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const uint16_t & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const uint32_t & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const uint64_t & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const int8_t   & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const int16_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const int32_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}
inline uint_t operator<<(const int64_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) << rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator<<=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(uint_t(lhs) << rhs);
}

inline uint_t operator>>(const bool     & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const uint8_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const uint16_t & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const uint32_t & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const uint64_t & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const int8_t   & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const int16_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const int32_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}
inline uint_t operator>>(const int64_t  & lhs, const uint_t& rhs) {
	return uint_t(lhs) >> rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator>>=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(uint_t(lhs) >> rhs);
}

// Comparison Operators
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator==(const T & lhs, const uint_t& rhs) {
	return rhs == lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator!=(const T & lhs, const uint_t& rhs) {
	return rhs != lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator>(const T & lhs, const uint_t& rhs) {
	return rhs < lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator<(const T & lhs, const uint_t& rhs) {
	return rhs > lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator>=(const T & lhs, const uint_t& rhs) {
	return rhs <= lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
bool operator<=(const T & lhs, const uint_t& rhs) {
	return rhs >= lhs;
}

// Arithmetic Operators
template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator+(const T & lhs, const uint_t& rhs) {
	return rhs + lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator+=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(rhs + lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator-(const T & lhs, const uint_t& rhs) {
	return -(rhs - lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator-=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(-(rhs - lhs));
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator*(const T & lhs, const uint_t& rhs) {
	return rhs * lhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator*=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(rhs * lhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator/(const T & lhs, const uint_t& rhs) {
	return uint_t(lhs) / rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator/=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(uint_t(lhs) / rhs);
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
uint_t operator%(const T & lhs, const uint_t& rhs) {
	return uint_t(lhs) % rhs;
}

template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
T & operator%=(T & lhs, const uint_t& rhs) {
	return lhs = static_cast<T>(uint_t(lhs) % rhs);
}

// IO Operator
inline std::ostream& operator<<(std::ostream & stream, const uint_t& rhs) {
	if (stream.flags() & stream.oct) {
		stream << rhs.str(8);
	} else if (stream.flags() & stream.dec) {
		stream << rhs.str(10);
	} else if (stream.flags() & stream.hex) {
		stream << rhs.str(16);
	}
	return stream;
}

#endif
