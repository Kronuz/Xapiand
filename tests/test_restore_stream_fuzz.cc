/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Fuzzes a streamed MessagePack RESTORE body's feed (Request::append): structured arrays and streams, their
// declared counts off by a few, nested near the depth limit, byte-mutated and cut into random pieces. Each body is
// checked against the body decoded whole:
//   - one non-empty top-level array and nothing after it yields its elements, in order, however it is cut;
//   - any other body that starts with a non-empty array header fails with a ClientError;
//   - any other body yields what the unpacker yields fed the same pieces, as before.
// The yielded objects are read after the request is gone, so one that refers to its buffer is a use after free.
//
//   xapiand_test_restore_stream_fuzz [SECONDS [SEED]]   (2 seconds by default)
//
// Exits non-zero on the first violation. Under -fsanitize it finds the memory errors and overflows.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "database/utils.h"
#include "msgpack.h"
#include "server/search_views.h"
#include "xapiand_exception.h"

namespace {

struct Rng {
	std::uint64_t s;
	std::uint64_t next() {
		s += 0x9e3779b97f4a7c15ULL;
		std::uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}
	std::size_t below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(next() % n); }
	bool one_in(std::size_t n) { return below(n) == 0; }
};

using Packer = msgpack::packer<msgpack::sbuffer>;

void value(Rng& rng, Packer& pk, std::size_t depth) {
	const std::size_t what = depth >= 4 ? rng.below(6) : rng.below(9);
	switch (what) {
		case 0: pk.pack_nil(); break;
		case 1: pk.pack(static_cast<std::int64_t>(rng.next())); break;
		case 2: pk.pack(static_cast<int>(rng.below(300)) - 150); break;
		case 3: pk.pack(std::string(rng.below(40), static_cast<char>('a' + rng.below(26)))); break;
		case 4: pk.pack(rng.one_in(2)); break;
		case 5: pk.pack(static_cast<double>(rng.below(1000)) / 7.0); break;
		case 6: {
			const auto n = rng.below(5);
			pk.pack_array(static_cast<std::uint32_t>(n));
			for (std::size_t i = 0; i < n; ++i) {
				value(rng, pk, depth + 1);
			}
			break;
		}
		case 7: {
			const std::string bin(rng.below(20), static_cast<char>(rng.below(256)));
			pk.pack_bin(static_cast<std::uint32_t>(bin.size()));
			pk.pack_bin_body(bin.data(), static_cast<std::uint32_t>(bin.size()));
			break;
		}
		default: {
			const auto n = rng.below(5);
			pk.pack_map(static_cast<std::uint32_t>(n));
			for (std::size_t i = 0; i < n; ++i) {
				pk.pack(std::string(1 + rng.below(8), static_cast<char>('a' + rng.below(26))));
				value(rng, pk, depth + 1);
			}
			break;
		}
	}
}

// One element: mostly a document (a map), sometimes any value, nested near the depth limit, or an array of maps.
std::string element(Rng& rng) {
	msgpack::sbuffer buffer;
	Packer pk(buffer);
	switch (rng.below(10)) {
		case 0: {
			const auto depth = MAX_JSON_DEPTH - 3 + rng.below(5);
			std::string s(depth, static_cast<char>(0x91));
			s += '\x01';
			return s;
		}
		case 1:
			value(rng, pk, 2);
			break;
		case 2: {
			pk.pack_array(2);
			pk.pack_map(0);
			pk.pack_map(0);
			break;
		}
		default: {
			const auto n = 1 + rng.below(4);
			pk.pack_map(static_cast<std::uint32_t>(n + 1));
			pk.pack(std::string("_id"));
			pk.pack(static_cast<int>(rng.below(1000)));
			for (std::size_t i = 0; i < n; ++i) {
				pk.pack(std::string(1 + rng.below(6), static_cast<char>('a' + rng.below(26))));
				value(rng, pk, 1);
			}
			break;
		}
	}
	return std::string(buffer.data(), buffer.size());
}

std::string header(Rng& rng, std::uint32_t count) {
	std::size_t size = count > 0xffff ? 5 : count > 15 ? 3 : 1;
	if (size == 1 && rng.one_in(3)) {
		size = 3;
	}
	if (size == 3 && rng.one_in(3)) {
		size = 5;
	}
	std::string h;
	if (size == 1) {
		h += static_cast<char>(0x90 | count);
	} else if (size == 3) {
		h += static_cast<char>(0xdc);
		h += static_cast<char>(count >> 8 & 0xff);
		h += static_cast<char>(count & 0xff);
	} else {
		h += static_cast<char>(0xdd);
		for (int shift = 24; shift >= 0; shift -= 8) {
			h += static_cast<char>(count >> shift & 0xff);
		}
	}
	return h;
}

std::string body(Rng& rng) {
	std::string b;
	switch (rng.below(8)) {
		case 0: {   // a stream of objects
			const auto k = rng.below(6);
			for (std::size_t i = 0; i < k; ++i) {
				b += element(rng);
			}
			break;
		}
		case 1: {   // random bytes
			const auto n = rng.below(64);
			for (std::size_t i = 0; i < n; ++i) {
				b += static_cast<char>(rng.below(256));
			}
			break;
		}
		case 2: {   // a header declaring a huge count
			b += rng.one_in(2) ? std::string("\xdd\xff\xff\xff\xff") : std::string("\xdc\xff\xff");
			const auto k = rng.below(4);
			for (std::size_t i = 0; i < k; ++i) {
				b += element(rng);
			}
			break;
		}
		default: {   // an array, its count off by a few now and then
			const auto declared = static_cast<std::uint32_t>(rng.one_in(8) ? 0 : rng.below(20));
			std::int64_t actual = declared;
			if (rng.one_in(3)) {
				actual += static_cast<std::int64_t>(rng.below(5)) - 2;
			}
			b += header(rng, declared);
			for (std::int64_t i = 0; i < actual; ++i) {
				b += element(rng);
			}
			if (rng.one_in(8)) {
				b += rng.one_in(2) ? std::string("\xc0") : header(rng, 1) + element(rng);
			}
			break;
		}
	}
	// Byte mutations.
	const auto mutations = rng.one_in(3) ? 1 + rng.below(3) : 0;
	for (std::size_t m = 0; m < mutations && !b.empty(); ++m) {
		const auto at = rng.below(b.size());
		switch (rng.below(5)) {
			case 0: b[at] = static_cast<char>(rng.below(256)); break;
			case 1: b.insert(at, 1, static_cast<char>(rng.below(256))); break;
			case 2: b.erase(at, 1); break;
			case 3: b.resize(at); break;
			default: b.insert(at, b.substr(at, rng.below(8))); break;
		}
	}
	return b;
}

std::vector<std::string> pieces(Rng& rng, const std::string& b) {
	if (b.size() <= 64 && rng.one_in(6)) {
		std::vector<std::string> bytes;
		for (char c : b) {
			bytes.emplace_back(1, c);
		}
		return bytes;
	}
	std::vector<std::size_t> cuts;
	const auto n = rng.below(6);
	for (std::size_t i = 0; i < n && b.size() > 1; ++i) {
		cuts.push_back(1 + (rng.one_in(2) ? rng.below(std::min<std::size_t>(6, b.size() - 1)) : rng.below(b.size() - 1)));
	}
	std::sort(cuts.begin(), cuts.end());
	std::vector<std::string> out;
	std::size_t from = 0;
	for (auto cut : cuts) {
		if (cut > from) {
			out.push_back(b.substr(from, cut - from));
			from = cut;
		}
	}
	out.push_back(b.substr(from));
	return out;
}

struct Outcome {
	std::deque<MsgPack> objects;
	std::optional<std::string> error;
};

// The feed under test; the request is gone when its objects are read.
Outcome fed(const std::vector<std::string>& parts) {
	Outcome out;
	{
		Request request;
		request.mode = Request::Mode::STREAM_MSGPACK;
		request.has_content_length = true;
		try {
			for (const auto& part : parts) {
				request.append(part.data(), part.size());
			}
			request.append(nullptr, 0);
		} catch (const ClientError& exc) {
			out.error = exc.what();
		}
		out.objects = std::move(request.objects);
	}
	return out;
}

// The unpacker fed `parts` as before: each object it yields, packed; whether it threw.
std::vector<std::string> as_before(const std::vector<std::string>& parts, bool& threw) {
	std::vector<std::string> objects;
	auto unpacker = make_msgpack_unpacker();
	threw = false;
	for (const auto& part : parts) {
		if (part.empty()) {
			continue;
		}
		unpacker.reserve_buffer(part.size());
		std::memcpy(unpacker.buffer(), part.data(), part.size());
		unpacker.buffer_consumed(part.size());
		try {
			msgpack::object_handle result;
			while (unpacker.next(result)) {
				objects.push_back(MsgPack(result.get()).serialize());
			}
		} catch (const std::exception&) {
			threw = true;
		}
	}
	return objects;
}

// The body decoded whole, when it is one non-empty array and nothing after it: its elements, packed.
std::optional<std::vector<std::string>> whole_array(const std::string& b) {
	auto unpacker = make_msgpack_unpacker();
	unpacker.reserve_buffer(b.size());
	std::memcpy(unpacker.buffer(), b.data(), b.size());
	unpacker.buffer_consumed(b.size());
	try {
		msgpack::object_handle result;
		if (!unpacker.next(result) || unpacker.nonparsed_size() != 0) {
			return std::nullopt;
		}
		const MsgPack obj(result.get());
		if (!obj.is_array() || obj.size() == 0) {
			return std::nullopt;
		}
		std::vector<std::string> elements;
		for (const auto& item : obj) {
			elements.push_back(item.serialize());
		}
		return elements;
	} catch (const std::exception&) {
		return std::nullopt;
	}
}

bool non_empty_array_header(const std::string& b) {
	if (b.empty()) {
		return false;
	}
	const auto first = static_cast<std::uint8_t>(b[0]);
	if ((first & 0xf0) == 0x90) {
		return first != 0x90;
	}
	const std::size_t size = first == 0xdc ? 3 : first == 0xdd ? 5 : 0;
	if (size == 0) {
		return false;
	}
	if (b.size() < size) {
		return true;   // incomplete: refused as a short array
	}
	for (std::size_t i = 1; i < size; ++i) {
		if (b[i] != 0) {
			return true;
		}
	}
	return false;
}

std::string hex(const std::string& b) {
	static const char* digits = "0123456789abcdef";
	std::string s;
	for (std::size_t i = 0; i < b.size() && i < 256; ++i) {
		s += digits[static_cast<std::uint8_t>(b[i]) >> 4];
		s += digits[static_cast<std::uint8_t>(b[i]) & 0x0f];
	}
	return s;
}

[[noreturn]] void violation(const char* what, const std::string& b, const std::vector<std::string>& parts) {
	std::fprintf(stderr, "VIOLATION: %s\n  body (%zu bytes) %s\n  pieces", what, b.size(), hex(b).c_str());
	for (const auto& part : parts) {
		std::fprintf(stderr, " %zu", part.size());
	}
	std::fprintf(stderr, "\n");
	std::exit(1);
}

}  // namespace

int main(int argc, char** argv) {
	const double seconds = argc > 1 ? std::atof(argv[1]) : 2.0;
	const std::uint64_t seed = argc > 2 ? std::strtoull(argv[2], nullptr, 10) : static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
	Rng rng{seed};
	std::printf("seed %llu, %.0f s\n", static_cast<unsigned long long>(seed), seconds);
	std::fflush(stdout);

	std::size_t bodies = 0, streamed = 0, refused = 0, before = 0;
	const auto end = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
	while (std::chrono::steady_clock::now() < end) {
		for (int batch = 0; batch < 64; ++batch) {
			const auto b = body(rng);
			const auto parts = pieces(rng, b);
			auto out = fed(parts);
			++bodies;
			if (non_empty_array_header(b)) {
				const auto elements = whole_array(b);
				if (!elements) {
					if (!out.error) {
						violation("a body that is not one array was not refused", b, parts);
					}
					++refused;
					continue;
				}
				if (out.error) {
					violation("one array was refused", b, parts);
				}
				std::vector<std::string> got;
				for (const auto& obj : out.objects) {
					if (!obj.is_array()) {
						violation("an array's elements did not arrive in an array", b, parts);
					}
					for (const auto& item : obj) {
						got.push_back(item.serialize());
					}
				}
				if (got != *elements) {
					violation("an array's elements differ from the array decoded whole", b, parts);
				}
				++streamed;
			} else {
				if (out.error) {
					violation("a body that is not a non-empty array was refused", b, parts);
				}
				bool threw = false;
				const auto expected = as_before(parts, threw);
				if (!threw) {
					std::vector<std::string> got;
					for (const auto& obj : out.objects) {
						got.push_back(obj.serialize());
					}
					if (got != expected) {
						violation("a body that is not a non-empty array yields other objects than before", b, parts);
					}
				}
				++before;
			}
		}
	}
	std::printf("%zu bodies: %zu streamed arrays, %zu refused, %zu as before\n", bodies, streamed, refused, before);
	return 0;
}
