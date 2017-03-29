/*
The MIT License (MIT)

Copyright (c):
 2014 Graeme Hill (http://graemehill.ca)
 2016 deipi.com LLC and contributors

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

#include "guid.h"

#include "serialise.h"

#include <algorithm>
#include <iomanip>
#include <random>

#ifdef GUID_LIBUUID
#include <uuid/uuid.h>
#endif

#ifdef GUID_FREEBSD
#include <cstdint>
#include <cstring>
#include <uuid.h>
#endif

#ifdef GUID_CFUUID
#include <CoreFoundation/CFUUID.h>
#endif

#ifdef GUID_WINDOWS
#include <objbase.h>
#endif


// 0x11f0241243c00ULL = 1yr
constexpr uint64_t UUID_TIME_INITIAL = 0x1e6bfffffffffffULL;


#define SALT_MASK ((1ULL << SALT_BITS) - 1)
#define CLOCK_MASK ((1ULL << CLOCK_BITS) - 1)
#define NODE_MASK ((1ULL << NODE_BITS) - 1)
#define VERSION_MASK ((1ULL << VERSION_BITS) - 1)


// This is the linux friendly implementation, but it could work on other
// systems that have libuuid available
#ifdef GUID_LIBUUID
inline Guid
GuidGenerator::_newGuid()
{
	std::array<unsigned char, 16> id;
	uuid_generate_time(id.data());
	return id;
}
#endif


// This is the FreBSD version.
#ifdef GUID_FREEBSD
inline Guid
GuidGenerator::_newGuid()
{
	uuid_t id;
	uint32_t status;
	uuid_create(&id, &status);
	if (status != uuid_s_ok) {
		// Can only be uuid_s_no_memory it seems.
		throw std::bad_alloc();
	}
	std::array<unsigned char, 16> byteArray;
	uuid_enc_be(byteArray.data(), &id);
	return byteArray;
}
#endif


// this is the mac and ios version
#ifdef GUID_CFUUID
inline Guid
GuidGenerator::_newGuid()
{
	auto newId = CFUUIDCreate(nullptr);
	auto bytes = CFUUIDGetUUIDBytes(newId);
	CFRelease(newId);

	return std::array<unsigned char, 16>{{
		bytes.byte0,
		bytes.byte1,
		bytes.byte2,
		bytes.byte3,
		bytes.byte4,
		bytes.byte5,
		bytes.byte6,
		bytes.byte7,
		bytes.byte8,
		bytes.byte9,
		bytes.byte10,
		bytes.byte11,
		bytes.byte12,
		bytes.byte13,
		bytes.byte14,
		bytes.byte15
	}};
}
#endif


// obviously this is the windows version
#ifdef GUID_WINDOWS
inline Guid
GuidGenerator::_newGuid()
{
	GUID newId;
	CoCreateGuid(&newId);

	return std::array<unsigned char, 16>{{
		(newId.Data1 >> 24) & 0xFF,
		(newId.Data1 >> 16) & 0xFF,
		(newId.Data1 >> 8) & 0xFF,
		(newId.Data1) & 0xff,

		(newId.Data2 >> 8) & 0xFF,
		(newId.Data2) & 0xff,

		(newId.Data3 >> 8) & 0xFF,
		(newId.Data3) & 0xFF,

		newId.Data4[0],
		newId.Data4[1],
		newId.Data4[2],
		newId.Data4[3],
		newId.Data4[4],
		newId.Data4[5],
		newId.Data4[6],
		newId.Data4[7]
	}};
}
#endif


// android version that uses a call to a java api
#ifdef GUID_ANDROID
GuidGenerator::GuidGenerator(JNIEnv *env)
{
	_env = env;
	_uuidClass = env->FindClass("java/util/UUID");
	_newGuidMethod = env->GetStaticMethodID(_uuidClass, "randomUUID", "()Ljava/util/UUID;");
	_mostSignificantBitsMethod = env->GetMethodID(_uuidClass, "getMostSignificantBits", "()J");
	_leastSignificantBitsMethod = env->GetMethodID(_uuidClass, "getLeastSignificantBits", "()J");
}


inline Guid
GuidGenerator::_newGuid()
{
	jobject javaUuid = _env->CallStaticObjectMethod(_uuidClass, _newGuidMethod);
	jlong mostSignificant = _env->CallLongMethod(javaUuid, _mostSignificantBitsMethod);
	jlong leastSignificant = _env->CallLongMethod(javaUuid, _leastSignificantBitsMethod);

	return std::array<unsigned char, 16>{{
		(mostSignificant >> 56) & 0xFF,
		(mostSignificant >> 48) & 0xFF,
		(mostSignificant >> 40) & 0xFF,
		(mostSignificant >> 32) & 0xFF,
		(mostSignificant >> 24) & 0xFF,
		(mostSignificant >> 16) & 0xFF,
		(mostSignificant >> 8) & 0xFF,
		(mostSignificant) & 0xFF,
		(leastSignificant >> 56) & 0xFF,
		(leastSignificant >> 48) & 0xFF,
		(leastSignificant >> 40) & 0xFF,
		(leastSignificant >> 32) & 0xFF,
		(leastSignificant >> 24) & 0xFF,
		(leastSignificant >> 16) & 0xFF,
		(leastSignificant >> 8) & 0xFF,
		(leastSignificant) & 0xFF,
	}};
}
#endif


Guid
GuidGenerator::newGuid(bool compact)
{
	auto guid = _newGuid();
	if (compact) {
		guid.compact();
	}
	return guid;
}


// overload << so that it's easy to convert to a string
std::ostream& operator<<(std::ostream& s, const Guid& guid) {
	std::ios::fmtflags flags(s.flags());
	s << std::hex << std::setfill('0')
		<< std::setw(2) << (int)guid._bytes[0]
		<< std::setw(2) << (int)guid._bytes[1]
		<< std::setw(2) << (int)guid._bytes[2]
		<< std::setw(2) << (int)guid._bytes[3]
		<< "-"
		<< std::setw(2) << (int)guid._bytes[4]
		<< std::setw(2) << (int)guid._bytes[5]
		<< "-"
		<< std::setw(2) << (int)guid._bytes[6]
		<< std::setw(2) << (int)guid._bytes[7]
		<< "-"
		<< std::setw(2) << (int)guid._bytes[8]
		<< std::setw(2) << (int)guid._bytes[9]
		<< "-"
		<< std::setw(2) << (int)guid._bytes[10]
		<< std::setw(2) << (int)guid._bytes[11]
		<< std::setw(2) << (int)guid._bytes[12]
		<< std::setw(2) << (int)guid._bytes[13]
		<< std::setw(2) << (int)guid._bytes[14]
		<< std::setw(2) << (int)guid._bytes[15];
	s.flags(flags);
	return s;
}


// create a guid from vector of bytes
Guid::Guid(const std::array<unsigned char, 16>& bytes)
	: _bytes(bytes) { }


// converts a single hex char to a number (0 - 15)
unsigned char hexDigitToChar(char ch) {
	if (ch > 47 && ch < 58)
		return ch - 48;

	if (ch > 96 && ch < 103)
		return ch - 87;

	if (ch > 64 && ch < 71)
		return ch - 55;

	return 0;
}


// converts the two hexadecimal characters to an unsigned char (a byte)
unsigned char hexPairToChar(char a, char b) {
	return hexDigitToChar(a) * 16 + hexDigitToChar(b);
}


// create a guid from string
Guid::Guid(const std::string& fromString)
{
	char charOne, charTwo;
	bool lookingForFirstChar = true;

	auto bytes = _bytes.begin();

	for (const char& ch : fromString) {
		if (ch == '-')
			continue;

		if (lookingForFirstChar) {
			charOne = ch;
			lookingForFirstChar = false;
		} else {
			charTwo = ch;
			auto byte = hexPairToChar(charOne, charTwo);
			*bytes++ = byte;
			lookingForFirstChar = true;
		}
	}
}


// create empty guid
Guid::Guid()
	: _bytes{ } { }


// copy constructor
Guid::Guid(const Guid& other)
	: _bytes(other._bytes) { }


// move constructor
Guid::Guid(Guid&& other)
	: _bytes(std::move(other._bytes)) { }


// overload assignment operator
Guid&
Guid::operator=(const Guid& other)
{
	_bytes = other._bytes;
	return *this;
}


// overload move operator
Guid&
Guid::operator=(Guid&& other)
{
	_bytes = std::move(other._bytes);
	return *this;
}


// overload equality operator
bool
Guid::operator==(const Guid& other) const
{
	return _bytes == other._bytes;
}


// overload inequality operator
bool
Guid::operator!=(const Guid& other) const
{
	return !operator==(other);
}


// converts GUID to std::string.
std::string
Guid::to_string() const
{
	std::ostringstream stream;
	stream << *this;
	return stream.str();
}


static inline uint64_t fnv_1a(uint64_t num) {
	// calculate FNV-1a hash
	uint64_t fnv = 0xcbf29ce484222325ULL;
	while (num) {
		fnv ^= num & 0xff;
		fnv *= 0x100000001b3ULL;
		num >>= 8;
	}
	return fnv;
}


GuidCompactor::GuidCompactor()
{
	std::memset(this, 0, sizeof(GuidCompactor));
}


std::string
GuidCompactor::serialise() const
{
	int bits, skip;
	if (compact.compacted) {
		bits = TIME_BITS + SALT_BITS + CLOCK_BITS;
		skip = PADDING2_BITS + COMPACTED_BITS;
	} else {
		bits = TIME_BITS + VERSION_BITS + NODE_BITS + CLOCK_BITS;
		skip = COMPACTED_BITS;
	}

	auto val1 = *(reinterpret_cast<const uint64_t*>(this));
	auto val2 = *(reinterpret_cast<const uint64_t*>(this) + 1);
	auto ls64 = (val1 & ((1ULL << skip) - 1)) << (64 - skip) | (val2 >> skip);
	auto ms64 = (val1 >> skip) & ((1ULL << (bits - 64)) - 1);

	char buf[16];
	*(reinterpret_cast<uint64_t*>(buf)) = ls64;
	*(reinterpret_cast<uint64_t*>(buf) + 1) = ms64;

	auto ptr = buf + 16;
	while (ptr != buf && !*--ptr);
	return std::string(buf, ptr - buf + 1);
}


GuidCompactor
GuidCompactor::unserialise(const std::string& bytes)
{
	auto compacted = bytes.length() < 12;

	int skip;
	if (compacted) {
		skip = PADDING2_BITS + COMPACTED_BITS;
	} else {
		skip = COMPACTED_BITS;
	}

	char buf[16];
	std::memset(buf, 0, sizeof(buf));
	std::memcpy(buf, bytes.data(), bytes.length());
	auto ls64 = *(reinterpret_cast<uint64_t*>(buf));
	auto ms64 = *(reinterpret_cast<uint64_t*>(buf) + 1);
	auto val1 = (ms64 << skip) | (ls64 >> (64 - skip));
	auto val2 = ls64 << skip;

	GuidCompactor compactor;
	*(reinterpret_cast<uint64_t*>(&compactor)) = val1;
	*(reinterpret_cast<uint64_t*>(&compactor) + 1) = val2;

	compactor.compact.compacted = compacted;

	return compactor;
}


inline uint64_t
GuidCompactor::calculate_node() {
	uint32_t seed = 0;
	if (compact.time) {
		seed ^= fnv_1a(compact.time);
	}
	if (compact.clock) {
		seed ^= fnv_1a(compact.clock);
	}
	if (compact.salt) {
		seed ^= fnv_1a(compact.salt);
	}
	if (!seed) {
		return 0;
	}
	std::mt19937 g(seed);
	uint64_t node = g();
	node <<= 32;
	node |= g();
	node &= NODE_MASK & ~SALT_MASK;
	node |= compact.salt;
	return node;
}


inline uint64_t
Guid::get_uuid1_node()
{
	return BYTE_SWAP_8(*reinterpret_cast<uint64_t*>(&_bytes[8])) & 0xffffffffffffULL;
}


inline uint64_t
Guid::get_uuid1_time()
{
	uint64_t tmp = BYTE_SWAP_2(*reinterpret_cast<uint16_t*>(&_bytes[6])) & 0xfffULL;
	tmp <<= 16;
	tmp |= BYTE_SWAP_2(*reinterpret_cast<uint16_t*>(&_bytes[4]));
	tmp <<= 32;
	tmp |= BYTE_SWAP_4(*reinterpret_cast<uint32_t*>(&_bytes[0]));
	return tmp;
}


inline uint16_t
Guid::get_uuid1_clock_seq()
{
	return BYTE_SWAP_2(*reinterpret_cast<uint16_t*>(&_bytes[8])) & 0x3fffULL;
}


inline int
Guid::get_uuid_version()
{
	return _bytes[6] >> 4;
}


inline GuidCompactor
Guid::get_compactor(bool compacted)
{
	GuidCompactor compactor;
	compactor.compact.compacted = compacted;
	auto time = get_uuid1_time();
	if (time) {
		time -= UUID_TIME_INITIAL;
	}
	compactor.compact.time = time;
	compactor.compact.clock = get_uuid1_clock_seq();

	return compactor;
}


inline void
Guid::compact()
{
	auto node = get_uuid1_node();

	auto salt = fnv_1a(node);
	salt &= SALT_MASK;

	auto compactor = get_compactor(true);
	compactor.compact.salt = salt;
	node = compactor.calculate_node();

	uint64_t num_last = BYTE_SWAP_8(*reinterpret_cast<uint64_t*>(&_bytes[8]));
	num_last = (num_last & 0xffff000000000000ULL) | node;
	*reinterpret_cast<uint64_t*>(&_bytes[8]) = BYTE_SWAP_8(num_last);
}


std::string
Guid::serialise()
{
	auto node = get_uuid1_node();
	auto salt = node & SALT_MASK;

	auto compactor = get_compactor(true);
	compactor.compact.salt = salt;
	auto compacted_node = compactor.calculate_node();

	if (node != compacted_node) {
		compactor = get_compactor(false);
		compactor.expanded.node = node;
		if (get_uuid_version() == 1) {
			compactor.expanded.version = 0;  // Version 1
		} else {
			compactor.expanded.version = 1;  // Version 4
		}
	}

	return compactor.serialise();
}


Guid
Guid::unserialise(const std::string& bytes)
{
	if (bytes.empty()) {
		throw std::invalid_argument("Cannot unserialise empty codes");
	}

	GuidCompactor compactor = GuidCompactor::unserialise(bytes);

	uint64_t node;
	uint64_t time = compactor.compact.time;
	if (time) {
		time += UUID_TIME_INITIAL;
	}
	if (compactor.compact.compacted) {
		node = compactor.calculate_node();
	} else {
		node = compactor.expanded.node;
	}

	unsigned char clock_seq_low = compactor.compact.clock & 0xff;
	unsigned char clock_seq_hi_variant = ((compactor.compact.clock >> 8) & 0x3f) | 0x80;  // Variant: RFC 4122
	unsigned time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	if (!compactor.compact.compacted && compactor.expanded.version) {
		time_hi_version |= 0x4000;  // Version 4
	} else {
		time_hi_version |= 0x1000;  // Version 1
	}

	Guid out;
	*reinterpret_cast<uint32_t*>(&out._bytes[0]) = BYTE_SWAP_4(time_low);
	*reinterpret_cast<uint16_t*>(&out._bytes[4]) = BYTE_SWAP_2(time_mid);
	*reinterpret_cast<uint16_t*>(&out._bytes[6]) = BYTE_SWAP_2(time_hi_version);
	*reinterpret_cast<uint64_t*>(&out._bytes[8]) = BYTE_SWAP_8(node);
	out._bytes[8] = clock_seq_hi_variant;
	out._bytes[9] = clock_seq_low;
	return out;
}
