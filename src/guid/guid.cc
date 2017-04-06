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
constexpr uint64_t UUID_TIME_INITIAL           = 0x1e6bfffffffffffULL;
constexpr uint8_t  UUID_MAX_SERIALISED_LENGTH  = 17;


constexpr uint64_t SALT_MASK     =  ((1ULL << SALT_BITS)    - 1);
constexpr uint64_t NODE_MASK     =  ((1ULL << NODE_BITS)    - 1);


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


inline uint64_t
GuidCompactor::calculate_node() const
{
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


std::string
GuidCompactor::serialise(uint8_t variant) const
{
	if (variant == 0x80 && (compact.version == 1 || compact.version == 4)) {
		auto val1 = *(reinterpret_cast<const uint64_t*>(this));
		auto val2 = *(reinterpret_cast<const uint64_t*>(this) + 1);

		uint64_t ls64, ms64;
		if (compact.compacted) {
			static const uint8_t skip1  = PADDING_BITS - VERSION_BITS - COMPACTED_BITS;
			static const uint8_t skip2 = 64 - skip1;
			static const uint8_t skip3 = skip1 + VERSION_BITS;
			ls64 = val1 << skip2 | val2 >> skip1;
			ms64 = (val1 << VERSION_BITS) >> skip3;
		} else {
			ms64 = (val1 << VERSION_BITS) | (val2 >> TIME_BITS);
			ls64 = val2 << VERSION_BITS;
		}

		char buf[16];
		*(reinterpret_cast<uint64_t*>(buf)) = ls64;
		*(reinterpret_cast<uint64_t*>(buf) + 1) = ms64;

		auto ptr = buf + 16;
		const auto min_buf = buf + 1;
		while (ptr != min_buf && !*--ptr);
		auto length = ptr - buf;

		buf[0] = (buf[0] & 0xc0) | (compact.version & 0x01) << 5 | compact.compacted << 4 | length;

		return std::string(buf, length + 1);
	} else {
		auto ls64 = *(reinterpret_cast<const uint64_t*>(this));

		auto ms64 = *(reinterpret_cast<const uint64_t*>(this) + 1);
		ms64 = (ms64 & 0xfffffffffffffffc) | (variant & 0xc0) >> 6;

		char buf[17];
		*(reinterpret_cast<uint64_t*>(buf + 1)) = ls64;
		*(reinterpret_cast<uint64_t*>(buf + 1) + 1) = ms64;

		auto ptr = buf + 17;
		const auto min_buf = buf + 2;
		while (ptr != min_buf && !*--ptr);
		auto length = ptr - buf - 1;

		buf[0] = length << 4;

		return std::string(buf, length + 2);
	}
}


GuidCompactor
GuidCompactor::unserialise(uint8_t length, const char** pos)
{
	bool compacted = **pos & 0x10;
	bool version   = **pos & 0x20;

	char buf[16];
	std::memset(buf, 0, sizeof(buf));
	std::memcpy(buf, *pos, length + 1);
	auto ls64 = *(reinterpret_cast<uint64_t*>(buf));
	auto ms64 = *(reinterpret_cast<uint64_t*>(buf) + 1);

	uint64_t val1, val2;
	if (compacted) {
		static const uint8_t skip1 = PADDING_BITS - VERSION_BITS - COMPACTED_BITS;
		static const uint8_t skip2 = 64 - skip1;
		val1 = ms64 << skip1 | ls64 >> skip2;
		val2 = ls64 << skip1;
	} else {
		val1 = ms64 >> VERSION_BITS;
		val2 = ms64 << TIME_BITS | ls64 >> VERSION_BITS;
	}

	GuidCompactor compactor;
	*(reinterpret_cast<uint64_t*>(&compactor)) = val1;
	*(reinterpret_cast<uint64_t*>(&compactor) + 1) = val2;
	compactor.compact.version = version ? 1 : 4;
	compactor.compact.compacted = compacted;
	*pos += length + 1;
	return compactor;
}


GuidCompactor
GuidCompactor::unserialise_full(uint8_t length, const char** pos)
{
	char buf[16];
	std::memset(buf, 0, sizeof(buf));
	std::memcpy(buf, *pos + 1, length + 1);

	GuidCompactor compactor;
	*(reinterpret_cast<uint64_t*>(&compactor)) = *(reinterpret_cast<uint64_t*>(buf));
	*(reinterpret_cast<uint64_t*>(&compactor) + 1) = *(reinterpret_cast<uint64_t*>(buf) + 1);
	*pos += length + 2;
	return compactor;
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


// converts a single hex char to a number (0 - 15)
unsigned char hexDigitToChar(char ch) {
	// 0 - 9
	if (ch > 47 && ch < 58) {
		return ch - 48;
	}

	// a - f
	if (ch > 96 && ch < 103) {
		return ch - 87;
	}

	// A - F
	if (ch > 64 && ch < 71) {
		return ch - 55;
	}

	return 0;
}


// converts the two hexadecimal characters to an unsigned char (a byte)
unsigned char hexPairToChar(char a, char b) {
	return hexDigitToChar(a) * 16 + hexDigitToChar(b);
}


// create a guid from vector of bytes
Guid::Guid(const std::array<unsigned char, 16>& bytes)
	: _bytes(bytes) { }


// create a guid from string
Guid::Guid(const std::string& fromString)
{
	char charOne;
	bool lookingForFirstChar = true;

	auto bytes = _bytes.begin();

	for (const char& ch : fromString) {
		if (ch == '-') {
			continue;
		}

		if (lookingForFirstChar) {
			charOne = ch;
			lookingForFirstChar = false;
		} else {
			*bytes = hexPairToChar(charOne, ch);
			++bytes;
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


inline uint64_t
Guid::get_uuid1_node() const
{
	return BYTE_SWAP_8(*reinterpret_cast<const uint64_t*>(&_bytes[8])) & 0xffffffffffffULL;
}


inline uint64_t
Guid::get_uuid1_time() const
{
	uint64_t tmp = BYTE_SWAP_2(*reinterpret_cast<const uint16_t*>(&_bytes[6])) & 0xfffULL;
	tmp <<= 16;
	tmp |= BYTE_SWAP_2(*reinterpret_cast<const uint16_t*>(&_bytes[4]));
	tmp <<= 32;
	tmp |= BYTE_SWAP_4(*reinterpret_cast<const uint32_t*>(&_bytes[0]));
	return tmp;
}


inline uint16_t
Guid::get_uuid1_clock_seq() const
{
	return BYTE_SWAP_2(*reinterpret_cast<const uint16_t*>(&_bytes[8])) & 0x3fffULL;
}


inline uint8_t
Guid::get_uuid_variant() const
{
	return _bytes[8] & 0xc0ULL;
}


inline uint8_t
Guid::get_uuid_version() const
{
	return _bytes[6] >> 4;
}


inline GuidCompactor
Guid::get_compactor(bool compacted) const
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
	compactor.compact.version = 1;
	compactor.compact.salt = salt;
	node = compactor.calculate_node();

	uint64_t num_last = BYTE_SWAP_8(*reinterpret_cast<uint64_t*>(&_bytes[8]));
	num_last = (num_last & 0xffff000000000000ULL) | node;
	*reinterpret_cast<uint64_t*>(&_bytes[8]) = BYTE_SWAP_8(num_last);
}


std::string
Guid::serialise() const
{
	auto node = get_uuid1_node();
	auto salt = node & SALT_MASK;

	auto compactor = get_compactor(true);
	compactor.compact.salt = salt;
	auto compacted_node = compactor.calculate_node();

	if (node != compacted_node) {
		compactor = get_compactor(false);
		compactor.expanded.node = node;
	}

	compactor.expanded.version = get_uuid_version();

	return compactor.serialise(get_uuid_variant());
}


bool
Guid::is_valid(const std::string& bytes)
{
	if (bytes.length() < 2) {
		return false;
	}

	const char* pos = bytes.c_str();
	const char* end = pos + bytes.length();
	while (pos != end) {
		uint8_t length = *pos & 0x0f;
		if (length == 0) {
			length = (*pos & 0xf0) >> 4;
			if (length == 0 || (end - pos) < (length + 2)) {
				return false;
			}
			pos += length + 2;
		} else {
			if ((end - pos) < (length + 1)) {
				return false;
			}
			pos += length + 1;
		}
	}
	return true;
}


Guid
Guid::unserialise(const std::string& bytes)
{
	if (bytes.length() < 2 || bytes.length() > UUID_MAX_SERIALISED_LENGTH) {
		THROW(SerialisationError, "Bad encoded uuid");
	}

	const char* pos = bytes.c_str();
	uint8_t length = *pos & 0x0f;
	if (length == 0) {
		length = (*pos & 0xf0) >> 4;
		if (length == 0 || bytes.length() != (length + 2)) {
			THROW(SerialisationError, "Bad encoded uuid");
		}
		return unserialise_full(length, &pos);
	} else if (bytes.length() != (length + 1)) {
		THROW(SerialisationError, "Bad encoded uuid");
	}

	return unserialise(length, &pos);
}


Guid
Guid::unserialise(uint8_t length, const char** pos)
{
	GuidCompactor compactor = GuidCompactor::unserialise(length, pos);

	uint64_t time = compactor.compact.time;
	if (time) {
		time += UUID_TIME_INITIAL;
	}

	unsigned char clock_seq_low = compactor.compact.clock & 0xffULL;
	unsigned char clock_seq_hi_variant = compactor.compact.clock >> 8 | 0x80ULL;  // Variant: RFC 4122
	uint64_t node = compactor.compact.compacted ? compactor.calculate_node() : node = compactor.expanded.node;
	unsigned time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= compactor.compact.version << 12;

	Guid out;
	*reinterpret_cast<uint32_t*>(&out._bytes[0]) = BYTE_SWAP_4(time_low);
	*reinterpret_cast<uint16_t*>(&out._bytes[4]) = BYTE_SWAP_2(time_mid);
	*reinterpret_cast<uint16_t*>(&out._bytes[6]) = BYTE_SWAP_2(time_hi_version);
	*reinterpret_cast<uint64_t*>(&out._bytes[8]) = BYTE_SWAP_8(node);
	out._bytes[8] = clock_seq_hi_variant;
	out._bytes[9] = clock_seq_low;
	return out;
}


Guid
Guid::unserialise_full(uint8_t length, const char** pos)
{
	GuidCompactor compactor = GuidCompactor::unserialise_full(length, pos);

	uint64_t time = compactor.compact.time;
	if (time) {
		time += UUID_TIME_INITIAL;
	}

	unsigned char clock_seq_low = compactor.compact.clock & 0xffULL;
	unsigned char clock_seq_hi_variant = compactor.compact.clock >> 8 | compactor.expanded.padding << 7 | compactor.expanded.compacted << 6;
	uint64_t node = compactor.expanded.node;
	unsigned time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= compactor.compact.version << 12;

	Guid out;
	*reinterpret_cast<uint32_t*>(&out._bytes[0]) = BYTE_SWAP_4(time_low);
	*reinterpret_cast<uint16_t*>(&out._bytes[4]) = BYTE_SWAP_2(time_mid);
	*reinterpret_cast<uint16_t*>(&out._bytes[6]) = BYTE_SWAP_2(time_hi_version);
	*reinterpret_cast<uint64_t*>(&out._bytes[8]) = BYTE_SWAP_8(node);
	out._bytes[8] = clock_seq_hi_variant;
	out._bytes[9] = clock_seq_low;
	return out;
}


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
		bytes.byte15,
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
		newId.Data4[7],
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
