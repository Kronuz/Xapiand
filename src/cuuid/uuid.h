/*
 * Copyright (c) 2015-2018 Dubalu LLC
 * Copyright (c) 2014 Graeme Hill (http://graemehill.ca).
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

#include "config.h"        // for UUID_*

#include <array>           // for std::array
#include <mutex>           // for std::mutex
#include <ostream>         // for std::ostream
#include <string>          // for std::string
#include "string_view.hh"  // for std::string_view


constexpr uint8_t UUID_LENGTH = 36;


/*
 * Class to represent a UUID. Each instance acts as a wrapper around a
 * 16 byte value that can be passed around by value. It also supports
 * conversion to string (via the stream operator <<) and conversion from a
 * string via constructor.
 */
class UUID {
	// actual data
	std::array<unsigned char, 16> _bytes;

public:
	// create a UUID from vector of bytes
	explicit UUID(const std::array<unsigned char, 16>& bytes, bool little_endian=false);

	// create a UUID from string
	explicit UUID(const char* str, size_t size);

	explicit UUID(std::string_view string);

	// create empty UUID
	UUID();

	// copy constructor
	UUID(const UUID& other) = default;
	// copy-assignment operator
	UUID& operator=(const UUID& other) = default;

	// move constructor
	UUID(UUID&& other);

	// overload move operator
	UUID& operator=(UUID&& other);

	// overload equality and inequality operator
	bool operator==(const UUID& other) const;
	bool operator!=(const UUID& other) const;

	const std::array<unsigned char, 16>& get_bytes() const {
		return _bytes;
	}

	static bool is_valid(const char** ptr, const char* end);
	static bool is_valid(std::string_view bytes) {
		const char* pos = bytes.data();
		const char* end = pos + bytes.size();
		return is_valid(&pos, end);
	}

	static bool is_serialised(const char** ptr, const char* end);
	static bool is_serialised(std::string_view bytes) {
		const char* pos = bytes.data();
		const char* end = pos + bytes.size();
		return is_serialised(&pos, end);
	}

	std::string to_string() const;
	std::string serialise() const;

	static UUID unserialise(std::string_view bytes);
	static UUID unserialise(const char** ptr, const char* end);

	// unserialise a serialised uuid's list
	template <typename OutputIt>
	static void unserialise(const char** ptr, const char* end, OutputIt d_first) {
		while (*ptr != end) {
			*d_first++ = unserialise(ptr, end);
		}
	}

	template <typename OutputIt>
	static void unserialise(std::string_view serialised, OutputIt d_first) {
		const char* pos = serialised.data();
		const char* end = pos + serialised.size();
		unserialise(&pos, end, d_first);
	}

	void compact_crush();

	void uuid1_node(uint64_t node);
	void uuid1_time(uint64_t time);
	void uuid1_clock_seq(uint16_t clock_seq);
	void uuid_variant(uint8_t variant);
	void uuid_version(uint8_t version);

	uint64_t uuid1_node() const;
	uint64_t uuid1_time() const;
	uint16_t uuid1_clock_seq() const;
	uint8_t uuid_variant() const;
	uint8_t uuid_version() const;

	bool empty() const;

private:
	// make the << operator a friend so it can access _bytes
	friend std::ostream &operator<<(std::ostream& os, const UUID& uuid);

	union UUIDCompactor get_compactor(bool compacted) const;

	// Aux functions for serialise/unserialise UUIDs.

	std::string serialise_full() const;
	std::string serialise_condensed() const;

	static UUID unserialise_full(const char** ptr, const char* end);
	static UUID unserialise_condensed(const char** ptr, const char* end);
};


/*
 * Class that can create new UUIDs. The only reason this exists instead of
 * just a global "newUUID" function is because some platforms will require
 * that there is some attached context. In the case of android, we need to
 * know what JNIEnv is being used to call back to Java, but the generator
 * function would no longer be cross-platform if we parameterized the android
 * version. Instead, construction of the UUIDGenerator may be different on
 * each platform, but the use of the generator is uniform.
 */
class UUIDGenerator {
#if defined (UUID_LIBUUID) && defined(__APPLE__)
	static std::mutex mtx;
#endif

	UUID newUUID();

public:
	UUIDGenerator() { }

	UUID operator ()(bool compact = true);
};
