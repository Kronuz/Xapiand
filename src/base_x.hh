/*
base_x.hh
BaseX encoder/decoder for C++

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

#include <algorithm>

#include "uint_t.hh"

template <typename Integer = uint_t>
class BaseX {
	size_t sz;
	uint8_t map[256];
	uint8_t rev[256];

public:
	template <typename A, std::size_t alphabet_size, typename I, std::size_t ignored_size>
	constexpr BaseX(A (&alphabet)[alphabet_size], I (&ignored)[ignored_size], bool ignore_case = false)
		: sz(alphabet_size - 1), map(), rev() {
		for (auto i = 256; i; --i) {
			map[i - 1] = 0;
			rev[i - 1] = 0xff;
		}
		for (auto i = ignored_size - 1; i; --i) {
			auto ch = ignored[i - 1];
			rev[(int)ch] = 0;
		}
		for (auto i = sz; i; --i) {
			auto ch = alphabet[i - 1];
			map[i - 1] = ch;
			rev[(int)ch] = i - 1;
			if (ignore_case) {
				if (ch >= 'A' && ch <='Z') {
					rev[(int)ch - 'A' + 'a'] = i - 1;
				} else if (ch >= 'a' && ch <='z') {
					rev[(int)ch - 'a' + 'A'] = i - 1;
				}
			}
		}
	}

	template <typename Result = std::string>
	Result encode(Integer num) const {
		Result result;
		if (num) {
			while (num) {
				auto r = num.divmod(sz);
				auto c = map[static_cast<int>(r.second)];
				// cout << num << "/" << sz << " = " << r.first << ", " << r.second << " (" << c << ")" << endl;
				result.push_back(c);
				num = r.first;
			}
		} else {
			result.push_back(map[0]);
		}

		std::reverse(result.begin(), result.end());
		return result;
	}

	template <typename Result = std::string>
	Result encode(const char* bytes, size_t size) const {
		auto num = Integer(bytes, size, 256);
		return encode<Result>(num);
	}

	template <typename Result = std::string, typename T, std::size_t N>
	Result encode(T (&s)[N]) const {
		return encode<Result>(s, N - 1);
	}

	template <typename Result = std::string>
	Result encode(const std::string& binary) const {
		return encode<Result>(binary.data(), binary.size());
	}

	void decode(Integer& result, const char* encoded, size_t encoded_size) const {
		result = 0;
		const char* p = encoded;
		for (auto i = encoded_size; i; --i, ++p) {
			result *= sz;
			auto ch = *p;
			auto c = rev[static_cast<int>(ch)];
			if (c == 0xff) {
				throw std::invalid_argument("Invalid character '" + std::string(1, ch) + "' at " + std::to_string(i));
			}
			result += c;
		}
	}

	template <typename Result, typename = typename std::enable_if<!std::is_integral<Result>::value>::type>
	void decode(Result& binary_result, const char* encoded, size_t encoded_size) const {
		Integer num;
		decode(num, encoded, encoded_size);
		binary_result = num.template str<Result>(256);
	}

	template <typename Result> Result decode(const char* encoded, size_t encoded_size) const {
		Result result;
		decode(result, encoded, encoded_size);
		return result;
	}

	template <typename Result = std::string, typename T, std::size_t N>
	Result decode(T (&s)[N]) const {
		return decode<Result>(s, N - 1);
	}

	template <typename Result = std::string>
	Result decode(const std::string& encoded) const {
		return decode<Result>(encoded.data(), encoded.size());
	}

	bool is_valid(const char* encoded, size_t encoded_size) const {
		const char* p = encoded;
		for (auto i = encoded_size; i; --i, ++p) {
			auto ch = *p;
			auto c = rev[static_cast<int>(ch)];
			if (c == 0xff) {
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
	template <typename Integer = uint_t>
	const BaseX<Integer>& base2() {
		static constexpr auto _ = BaseX<Integer>("01", " \n\r\t");
		return _;
	}
}

// base8
namespace base8 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& base8() {
		static constexpr auto _ = BaseX<Integer>("01234567", " \n\r\t");
		return _;
	}
}

// base11
namespace base11 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& base11() {
		static constexpr auto _ = BaseX<Integer>("0123456789a", " \n\r\t", true);
		return _;
	}
}

// base16
namespace base16 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& base16() {
		static constexpr auto _ = BaseX<Integer>("0123456789abcdef", " \n\r\t", true);
		return _;
	}
}

// base32
namespace base32 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& base32() {
		static constexpr auto _ = BaseX<Integer>("0123456789ABCDEFGHJKMNPQRSTVWXYZ", " \n\r\t", true);
		return _;
	}
}

// base36
namespace base36 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& base36() {
		static constexpr auto _ = BaseX<Integer>("0123456789abcdefghijklmnopqrstuvwxyz", " \n\r\t", true);
		return _;
	}
}

// base58
namespace base58 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& gmp() {
		static constexpr auto _ = BaseX<Integer>("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuv", " \n\r\t");
		return _;
	}
	template <typename Integer = uint_t>
	const BaseX<Integer>& bitcoin() {
		static constexpr auto _ = BaseX<Integer>("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", " \n\r\t");
		return _;
	}
	template <typename Integer = uint_t>
	const BaseX<Integer>& ripple() {
		static constexpr auto _ = BaseX<Integer>("rpshnaf39wBUDNEGHJKLM4PQRST7VWXYZ2bcdeCg65jkm8oFqi1tuvAxyz", " \n\r\t");
		return _;
	}
	template <typename Integer = uint_t>
	const BaseX<Integer>& flickr() {
		static constexpr auto _ = BaseX<Integer>("123456789abcdefghijkmnopqrstuvwxyzABCDEFGHJKLMNPQRSTUVWXYZ", " \n\r\t");
		return _;
	}
	template <typename Integer = uint_t>
	const BaseX<Integer>& base58() {
		return bitcoin<Integer>();
	}
}

// base62
namespace base62 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& inverted() {
		static constexpr auto _ = BaseX<Integer>("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", " \n\r\t");
		return _;
	}
	template <typename Integer = uint_t>
	const BaseX<Integer>& base62() {
		static constexpr auto _ = BaseX<Integer>("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz", " \n\r\t");
		return _;
	}
}

// base64
namespace base64 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& urlsafe() {
		static constexpr auto _ = BaseX<Integer>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_", " \n\r\t");
		return _;
	}
	template <typename Integer = uint_t>
	const BaseX<Integer>& base64() {
		static constexpr auto _ = BaseX<Integer>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", " \n\r\t");
		return _;
	}
}

// base66
namespace base66 {
	template <typename Integer = uint_t>
	const BaseX<Integer>& base66() {
		static constexpr auto _ = BaseX<Integer>("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.!~", " \n\r\t");
		return _;
	}
}

#endif
