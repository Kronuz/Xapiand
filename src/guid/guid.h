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
#include <iomanip>
#include <iostream>        // for ostream
#include <sstream>
#include <string>          // for string
#include <unordered_set>   // for unordered_set

#include "cppcodec/base64_default_url_unpadded.hpp"   // for base64 namespace

#ifdef GUID_ANDROID
#include <jni.h>
#endif


constexpr uint8_t TIME_BITS       = 60;
constexpr uint8_t VERSION_BITS    = 64 - TIME_BITS;
constexpr uint8_t COMPACTED_BITS  = 1;
constexpr uint8_t SALT_BITS       = 5;
constexpr uint8_t CLOCK_BITS      = 14;
constexpr uint8_t NODE_BITS       = 48;
constexpr uint8_t PADDING_BITS    = 64 - COMPACTED_BITS - SALT_BITS - CLOCK_BITS;
constexpr uint8_t PADDING1_BITS   = 64 - COMPACTED_BITS - NODE_BITS - CLOCK_BITS;


constexpr uint8_t UUID_LENGTH             = 36;


class Guid;


/*
 * Union for compact uuids
 */
union GuidCompactor {
	struct compact_t {
		uint64_t time        : TIME_BITS;
		uint64_t version     : VERSION_BITS;

		uint64_t compacted   : COMPACTED_BITS;
		uint64_t padding     : PADDING_BITS;
		uint64_t salt        : SALT_BITS;
		uint64_t clock       : CLOCK_BITS;
	} compact;

	struct expanded_t {
		uint64_t time        : TIME_BITS;
		uint64_t version     : VERSION_BITS;

		uint64_t compacted   : COMPACTED_BITS;
		uint64_t padding     : PADDING1_BITS;
		uint64_t node        : NODE_BITS;
		uint64_t clock       : CLOCK_BITS;
	} expanded;

	GuidCompactor();

private:
	friend class Guid;

	uint64_t calculate_node() const;
	std::string serialise(uint8_t variant) const;
	static GuidCompactor unserialise(uint8_t lenght, const char** bytes);
	static GuidCompactor unserialise_full(uint8_t lenght, const char** bytes);
};


/*
 * Class to represent a GUID/UUID. Each instance acts as a wrapper around a
 * 16 byte value that can be passed around by value. It also supports
 * conversion to string (via the stream operator <<) and conversion from a
 * string via constructor.
 */
class Guid {
public:

	// create a guid from vector of bytes
	Guid(const std::array<unsigned char, 16>& bytes);

	// create a guid from string
	explicit Guid(const std::string& fromString);

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

	inline const std::array<unsigned char, 16>& get_bytes() const {
		return _bytes;
	}

	std::string to_string() const;

	void compact();
	std::string serialise() const;

	static bool is_valid(const std::string& bytes);
	static Guid unserialise(const std::string& bytes);

	// Serialise a uuid's list.
	template <typename InputIt>
	static std::string serialise(InputIt first, InputIt last) {
		std::string serialised;
		while (first != last) {
			const auto& uuid = *first;
			if (uuid.length() == UUID_LENGTH) {
				if (uuid[8] != '-' || uuid[13] != '-' || uuid[18] != '-' || uuid[23] != '-') {
					THROW(SerialisationError, "Invalid UUID format in: %s", uuid.c_str());
				}
				int i = 0;
				for (const auto& c : uuid) {
					if (!std::isxdigit(c) && i != 8 && i != 13 && i != 18 && i != 23) {
						THROW(SerialisationError, "Invalid UUID format in: %s [%c -> %d]", uuid.c_str(), c, i);
					}
					++i;
				}
				Guid guid(uuid);
				serialised.append(guid.serialise());
			} else {
				try {
					const auto decoded = base64::decode<std::string>(uuid);
					if (is_valid(decoded)) {
						serialised.append(decoded);
					} else {
						THROW(SerialisationError, "Invalid base64 UUID format in: %s", uuid.c_str());
					}
				} catch (const cppcodec::padding_error&) {
					THROW(SerialisationError, "Invalid base64 UUID format in: %s", uuid.c_str());
				}
			}
			++first;
		}

		return serialised;
	}

	// unserialise a serialised uuid's list
	template <typename OutputIt>
	static void unserialise(const char** ptr, const char* end, OutputIt d_first) {
		const char* pos = *ptr;
		while (pos != end) {
			uint8_t length = *pos & 0x0f;
			if (length == 0) {
				length = (*pos & 0xf0) >> 4;
				if (length < 2 || (end - pos) < (length + 2)) {
					THROW(SerialisationError, "Bad encoded uuid");
				}
				*d_first = unserialise_full(length, &pos);
			} else if (length < 2 || (end - pos) < (length + 1)) {
				THROW(SerialisationError, "Bad encoded uuid");
			} else {
				*d_first = unserialise(length, &pos);
			}
		}
	}

	template <typename OutputIt>
	static void unserialise(const std::string& serialised, OutputIt d_first) {
		const char* ptr = serialised.data();
		const char* end = serialised.data() + serialised.length();
		unserialise(&ptr, end, d_first);
	}

private:
	// actual data
	std::array<unsigned char, 16> _bytes;

	// make the << operator a friend so it can access _bytes
	friend std::ostream &operator<<(std::ostream& s, const Guid& guid);

	uint64_t get_uuid1_node() const;
	uint64_t get_uuid1_time() const;
	uint16_t get_uuid1_clock_seq() const;
	uint8_t get_uuid_variant() const;
	uint8_t get_uuid_version() const;
	GuidCompactor get_compactor(bool compacted) const;

	// Aux functions for unserialise a serialised uuid's list.
	static Guid unserialise_full(uint8_t length, const char** pos);
	static Guid unserialise(uint8_t length, const char** pos);
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
	GuidGenerator(JNIEnv* env);
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
