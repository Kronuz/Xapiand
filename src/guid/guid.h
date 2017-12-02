/*
The MIT License (MIT)

Copyright (c):
 2014 Graeme Hill (http://graemehill.ca)
 2016,2017 deipi.com LLC and contributors

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

#pragma once

#include "exception.h"

#include <array>           // for array
#include <iostream>        // for ostream
#include <string>          // for string

#ifdef GUID_ANDROID
#include <jni.h>
#endif


constexpr uint8_t UUID_LENGTH = 36;


/*
 * Class to represent a GUID/UUID. Each instance acts as a wrapper around a
 * 16 byte value that can be passed around by value. It also supports
 * conversion to string (via the stream operator <<) and conversion from a
 * string via constructor.
 */
class Guid {
	// actual data
	std::array<unsigned char, 16> _bytes;

public:
	// create a guid from vector of bytes
	explicit Guid(const std::array<unsigned char, 16>& bytes);

	// create a guid from string
	explicit Guid(const char* str, size_t size);

	explicit Guid(const std::string& string);

	// create empty guid
	Guid();

	// copy constructor
	Guid(const Guid& other);

	// move constructor
	Guid(Guid&& other);

	// overload assignment operator
	Guid& operator=(const Guid& other);

	// overload move operator
	Guid& operator=(Guid&& other);

	// overload equality and inequality operator
	bool operator==(const Guid& other) const;
	bool operator!=(const Guid& other) const;

	const std::array<unsigned char, 16>& get_bytes() const {
		return _bytes;
	}

	static bool is_valid(const char** ptr, const char* end);
	static bool is_valid(const std::string& bytes) {
		const char* pos = bytes.data();
		const char* end = pos + bytes.size();
		return is_valid(&pos, end);
	}

	static bool is_serialised(const char** ptr, const char* end);
	static bool is_serialised(const std::string& bytes) {
		const char* pos = bytes.data();
		const char* end = pos + bytes.size();
		return is_serialised(&pos, end);
	}

	std::string to_string() const;
	std::string serialise() const;

	static Guid unserialise(const std::string& bytes);
	static Guid unserialise(const char** ptr, const char* end);

	// unserialise a serialised uuid's list
	template <typename OutputIt>
	static void unserialise(const char** ptr, const char* end, OutputIt d_first) {
		while (*ptr != end) {
			*d_first++ = unserialise(ptr, end);
		}
	}

	template <typename OutputIt>
	static void unserialise(const std::string& serialised, OutputIt d_first) {
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

private:
	// make the << operator a friend so it can access _bytes
	friend std::ostream &operator<<(std::ostream& s, const Guid& guid);

	union GuidCompactor get_compactor(bool compacted) const;

	// Aux functions for serialise/unserialise UUIDs.

	std::string serialise_full() const;
	std::string serialise_condensed() const;

	static Guid unserialise_full(const char** ptr, const char* end);
	static Guid unserialise_condensed(const char** ptr, const char* end);
};


/*
 * Class that can create new guids. The only reason this exists instead of
 * just a global "newGuid" function is because some platforms will require
 * that there is some attached context. In the case of android, we need to
 * know what JNIEnv is being used to call back to Java, but the newGuid()
 * function would no longer be cross-platform if we parameterized the android
 * version. Instead, construction of the GuidGenerator may be different on
 * each platform, but the use of newGuid is uniform.
 */
class GuidGenerator {
	Guid _newGuid();

public:
#ifdef GUID_ANDROID
	explicit GuidGenerator(JNIEnv* env);
#else
	GuidGenerator() { }
#endif

	Guid newGuid(bool compact=true);

#ifdef GUID_ANDROID
private:
	JNIEnv* _env;
	jclass _uuidClass;
	jmethodID _newGuidMethod;
	jmethodID _mostSignificantBitsMethod;
	jmethodID _leastSignificantBitsMethod;
#endif
};

namespace std {
	inline auto to_string(Guid& uuid) {
		return uuid.to_string();
	}

	inline const auto to_string(const Guid& uuid) {
		return uuid.to_string();
	}
}
