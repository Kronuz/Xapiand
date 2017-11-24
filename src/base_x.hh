/*
base_x.hh
BaseX encoder / decoder for C++

Copyright (c) 2017 German Mendez Bravo (Kronuz) @ german dot mb at gmail.com

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
*/

#ifndef __BASE_X__H_
#define __BASE_X__H_

#include <array>
#include <algorithm>

#include "uinteger_t.hh"

class Alphabet {
	unsigned char _ord[256];
	char _chr[256];

public:
	const size_t base;
	const unsigned base_size;
	const unsigned base_bits;
	const uinteger_t::digit base_mask;

	template <typename A, std::size_t alphabet_size, typename I, std::size_t ignored_size>
	constexpr Alphabet(A (&alphabet)[alphabet_size], I (&ignored)[ignored_size], bool ignore_case = false) :
		_ord(),
		_chr(),
		base(alphabet_size - 1),
		base_size(uinteger_t::constexpr_base_size(base)),
		base_bits(uinteger_t::constexpr_base_bits(base)),
		base_mask(base - 1)
	{
		for (auto i = 256; i; --i) {
			_chr[i - 1] = 0;
			_ord[i - 1] = 0xff;
		}
		for (auto i = ignored_size - 1; i; --i) {
			auto ch = ignored[i - 1];
			_ord[(int)ch] = 0;
		}
		for (auto i = base; i; --i) {
			auto ch = alphabet[i - 1];
			_chr[i - 1] = ch;
			_ord[(int)ch] = i - 1;
			if (ignore_case) {
				if (ch >= 'A' && ch <='Z') {
					_ord[(int)ch - 'A' + 'a'] = i - 1;
				} else if (ch >= 'a' && ch <='z') {
					_ord[(int)ch - 'a' + 'A'] = i - 1;
				}
			}
		}
	}

	std::array<uinteger_t, 256> ord() {
		std::array<uinteger_t, 256> _;
		for (int i = 0; i < 256; ++i) {
			_[i] = _ord[i];
		}
		return _;
	}

	std::array<char, 256> chr() {
		std::array<char, 256> _;
		for (int i = 0; i < 256; ++i) {
			_[i] = _chr[i];
		}
		return _;
	}
};

class BaseX {
	Alphabet alphabet;

	std::array<uinteger_t, 256> _ord;
	std::array<char, 256> _chr;

public:
	BaseX(const Alphabet& a) :
		alphabet(a),
		_ord(alphabet.ord()),
		_chr(alphabet.chr()) { }

	const char& chr(int ord) const {
		return _chr[ord];
	}

	const uinteger_t& ord(int chr) const {
		return _ord[chr];
	}

	// Get string representation of value
	template <typename Result = std::string>
	void encode(Result& result, const uinteger_t& num) const {
		auto num_sz = num.size();
		if (num_sz) {
			result.reserve(num_sz * alphabet.base_size);
			if (alphabet.base_bits) {
				std::size_t shift = 0;
				auto ptr = reinterpret_cast<const uinteger_t::half_digit*>(num.data());
				uinteger_t::digit num = *ptr++;
				num <<= uinteger_t::half_digit_bits;
				for (auto i = num_sz * 2 - 1; i; --i) {
					num >>= uinteger_t::half_digit_bits;
					num |= (static_cast<uinteger_t::digit>(*ptr++) << uinteger_t::half_digit_bits);
					do {
						result.push_back(chr(static_cast<int>((num >> shift) & alphabet.base_mask)));
						shift += alphabet.base_bits;
					} while (shift <= uinteger_t::half_digit_bits);
					shift -= uinteger_t::half_digit_bits;
				}
				num >>= (shift + uinteger_t::half_digit_bits);
				while (num) {
					result.push_back(chr(static_cast<int>(num & alphabet.base_mask)));
					num >>= alphabet.base_bits;
				}
				auto s = chr(0);
				auto rit_f = std::find_if(result.rbegin(), result.rend(), [s](const char& c) { return c != s; });
				result.resize(result.rend() - rit_f); // shrink
			} else {
				uinteger_t quotient = num;
				uinteger_t uint_base = alphabet.base;
				do {
					auto r = quotient.divmod(uint_base);
					result.push_back(chr(static_cast<int>(r.second)));
					quotient = std::move(r.first);
				} while (quotient);
			}
			std::reverse(result.begin(), result.end());
		} else {
			result.push_back(chr(0));
		}
	}

	template <typename Result = std::string>
	Result encode(uinteger_t num) const {
		Result result;
		encode(result, num);
		return result;
	}

	template <typename Result = std::string>
	void encode(Result& result, const char* bytes, size_t size) const {
		encode(result, uinteger_t(bytes, size, 256));
	}

	template <typename Result = std::string>
	Result encode(const char* bytes, size_t size) const {
		Result result;
		encode(result, uinteger_t(bytes, size, 256));
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N>
	void encode(Result& result, T (&s)[N]) const {
		encode(result, s, N - 1);
	}

	template <typename Result = std::string, typename T, std::size_t N>
	Result encode(T (&s)[N]) const {
		Result result;
		encode(result, s, N - 1);
		return result;
	}

	template <typename Result = std::string>
	void encode(Result& result, const std::string& binary) const {
		return encode(result, binary.data(), binary.size());
	}

	template <typename Result = std::string>
	Result encode(const std::string& binary) const {
		Result result;
		encode(result, binary.data(), binary.size());
		return result;
	}

	void decode(uinteger_t& result, const char* encoded, std::size_t encoded_size) const {
		if (alphabet.base_bits) {
			for (; encoded_size; --encoded_size, ++encoded) {
				auto d = ord(static_cast<int>(*encoded));
				if (d == 0xff) {
					throw std::runtime_error("Error: Invalid character: '" + std::string(1, *encoded) + "' at " + std::to_string(encoded_size));
				}
				result = (result << alphabet.base_bits) | d;
			}
		} else {
			for (; encoded_size; --encoded_size, ++encoded) {
				auto d = ord(static_cast<int>(*encoded));
				if (d == 0xff) {
					throw std::runtime_error("Error: Invalid character: '" + std::string(1, *encoded) + "' at " + std::to_string(encoded_size));
				}
				result = (result * alphabet.base) + d;
			}
		}
	}

	template <typename Result, typename = typename std::enable_if<!std::is_integral<Result>::value>::type>
	void decode(Result& result, const char* encoded, size_t encoded_size) const {
		uinteger_t num;
		decode(num, encoded, encoded_size);
		result = num.template str<Result>(256);
	}

	template <typename Result>
	Result decode(const char* encoded, size_t encoded_size) const {
		Result result;
		decode(result, encoded, encoded_size);
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N>
	void decode(Result& result, T (&s)[N]) const {
		decode(result, s, N - 1);
	}

	template <typename Result = std::string, typename T, std::size_t N>
	Result decode(T (&s)[N]) const {
		Result result;
		decode(result, s, N - 1);
		return result;
	}

	template <typename Result = std::string>
	void decode(Result& result, const std::string& encoded) const {
		decode(result, encoded.data(), encoded.size());
	}

	template <typename Result = std::string>
	Result decode(const std::string& encoded) const {
		Result result;
		decode(result, encoded.data(), encoded.size());
		return result;
	}

	bool is_valid(const char* encoded, size_t encoded_size) const {
		for (; encoded_size; --encoded_size, ++encoded) {
			auto d = ord(static_cast<int>(*encoded));
			if (d == 0xff) {
				return false;
			}
		}
		return true;
	}

	template <typename T, std::size_t N>
	bool is_valid(T (&s)[N]) const {
		return is_valid(s, N - 1);
	}

	bool is_valid(const std::string& encoded) const {
		return is_valid(encoded.data(), encoded.size());
	}
};


// base2
namespace base2 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base2() {
		constexpr Alphabet alphabet("01", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base8
namespace base8 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base8() {
		constexpr Alphabet alphabet("01234567", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base11
namespace base11 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base11() {
		constexpr Alphabet alphabet("0123456789a", " \n\r\t", true);
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base16
namespace base16 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base16() {
		constexpr Alphabet alphabet("0123456789abcdef", " \n\r\t", true);
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base32
namespace base32 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base32() {
		constexpr Alphabet alphabet("0123456789ABCDEFGHJKMNPQRSTVWXYZ", " \n\r\t", true);
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base36
namespace base36 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base36() {
		constexpr Alphabet alphabet("0123456789abcdefghijklmnopqrstuvwxyz", " \n\r\t", true);
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base58
namespace base58 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& gmp() {
		constexpr Alphabet alphabet("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& bitcoin() {
		constexpr Alphabet alphabet("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& ripple() {
		constexpr Alphabet alphabet("rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& flickr() {
		constexpr Alphabet alphabet("123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& base58() {
		return bitcoin<uinteger_t>();
	}
}

// base62
namespace base62 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& inverted() {
		constexpr Alphabet alphabet("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& base62() {
		constexpr Alphabet alphabet("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base64
namespace base64 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& urlsafe() {
		constexpr Alphabet alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
	template <typename uinteger_t = uinteger_t>
	const BaseX& base64() {
		constexpr Alphabet alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
}

// base66
namespace base66 {
	template <typename uinteger_t = uinteger_t>
	const BaseX& base66() {
		constexpr Alphabet alphabet("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~", " \n\r\t");
		static BaseX encoder(alphabet);
		return encoder;
	}
}

#endif
