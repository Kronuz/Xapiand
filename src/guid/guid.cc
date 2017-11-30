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

#include "serialise.h"  // for BYTE_SWAP_*

#include <algorithm>      // for std::copy
#include <iomanip>        // for std::setw and std::setfill
#include <random>         // for std::mt19937
#include <sstream>        // for std::ostringstream
#include <stdexcept>      // for std::invalid_argument

// 0x01b21dd213814000 is the number of 100-ns intervals between the
// UUID epoch 1582-10-15 00:00:00 and the Unix epoch 1970-01-01 00:00:00.
// 0x00011f0241243c00 = 1yr (31556952000000000 nanoseconds)
constexpr uint64_t UUID_TIME_EPOCH             = 0x01b21dd213814000ULL;
constexpr uint64_t UUID_TIME_YEAR              = 0x00011f0241243c00ULL;
constexpr uint64_t UUID_TIME_INITIAL           = UUID_TIME_EPOCH;
constexpr uint8_t  UUID_MAX_SERIALISED_LENGTH  = 17;

constexpr uint8_t VARIANT_BITS    = 2;

constexpr uint8_t TIME_BITS       = 60;
constexpr uint8_t VERSION_BITS    = 64 - TIME_BITS;  // 4
constexpr uint8_t COMPACTED_BITS  = 1;
constexpr uint8_t SALT_BITS       = 5;
constexpr uint8_t CLOCK_BITS      = 14;
constexpr uint8_t NODE_BITS       = 48;
constexpr uint8_t PADDING_BITS    = 64 - COMPACTED_BITS - SALT_BITS - CLOCK_BITS;  // 44
constexpr uint8_t PADDING1_BITS   = 64 - COMPACTED_BITS - NODE_BITS - CLOCK_BITS;  // 1

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


/*
 * Union for compact uuids
 */
union GuidCondenser {
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

	uint64_t calculate_node() const;

	std::string serialise() const;
	static GuidCondenser unserialise(uint8_t lenght, const char** bytes);

	GuidCondenser();
};


GuidCondenser::GuidCondenser()
	: compact({ 0, 0, 0, 0, 0, 0 }) { }


inline uint64_t
GuidCondenser::calculate_node() const
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
	std::mt19937 rng(seed);
	uint64_t node = rng();
	node <<= 32;
	node |= rng();
	node &= NODE_MASK & ~SALT_MASK;
	node |= compact.salt;
	return node;
}


inline std::string
GuidCondenser::serialise() const
{
	auto val0 = *(reinterpret_cast<const uint64_t*>(this));
	auto val1 = *(reinterpret_cast<const uint64_t*>(this) + 1);

	uint64_t buf0, buf1;
	constexpr uint8_t s4 = VERSION_BITS;  // 4
	if (compact.compacted) {
		constexpr uint8_t s45 = PADDING_BITS - COMPACTED_BITS + VARIANT_BITS;  // 44 - 1 + 2 = 45
		constexpr uint8_t s49 = PADDING_BITS - COMPACTED_BITS + VARIANT_BITS + VERSION_BITS;  // 44 - 1 + 2 + 4 = 49
		buf0 = (val0 << s4) >> s49;
		buf1 = val0 << (64 - s45) | val1 >> s45;
	} else {
		constexpr uint8_t s2 = PADDING1_BITS - COMPACTED_BITS + VARIANT_BITS;  // 1 - 1 + 2 = 2
		constexpr uint8_t s6 = PADDING1_BITS - COMPACTED_BITS + VARIANT_BITS + VERSION_BITS;  // 1 - 1 + 2 + 4 = 6
		buf0 = (val0 << s4) >> s6;
		buf1 = val0 << (64 - s2) | val1 >> s2;
	}

	char buf[UUID_MAX_SERIALISED_LENGTH];
	*(reinterpret_cast<uint64_t*>(buf + 1)) = BYTE_SWAP_8(buf0);
	*(reinterpret_cast<uint64_t*>(buf + 1) + 1) = BYTE_SWAP_8(buf1);
	buf[0] = '\0';

	auto ptr = buf;
	const auto end = ptr + sizeof(buf) - 1;
	while (ptr != end && !*++ptr);
	if (*ptr & 0xfc) --ptr;
	assert(ptr != buf); // UUID is 128bit - 4bit (version) - 2bit (variant) = 122bit really used
	auto length = end - ptr;
	*ptr = (length - 1) << 4 | compact.compacted << 3 | (compact.version & 0x01) << 2 | (*ptr & 0x03);

	return std::string(ptr, length);
}


inline GuidCondenser
GuidCondenser::unserialise(uint8_t length, const char** ptr)
{
	auto l = **ptr;
	bool compacted = l & 0x08;
	bool version1  = l & 0x04;

	char buf[UUID_MAX_SERIALISED_LENGTH];
	auto start = buf + UUID_MAX_SERIALISED_LENGTH - length;
	std::fill(buf, start, 0);
	std::copy(*ptr, *ptr + length, start);

	auto buf0 = BYTE_SWAP_8(*(reinterpret_cast<uint64_t*>(buf)));
	auto buf1 = BYTE_SWAP_8(*(reinterpret_cast<uint64_t*>(buf) + 1));

	uint64_t val0, val1;
	if (compacted) {
		constexpr uint8_t s45 = PADDING_BITS - COMPACTED_BITS + VARIANT_BITS;  // 44 - 1 + 2 = 45
		val0 = buf0 << s45 | buf1 >> (64 - s45);
		val1 = buf1 << s45;
	} else {
		constexpr uint8_t s2 = PADDING1_BITS - COMPACTED_BITS + VARIANT_BITS;  // 1 - 1 + 2 = 2
		val0 = buf0 << s2 | buf1 >> (64 - s2);
		val1 = buf1 << s2;
	}

	GuidCondenser condenser;
	*(reinterpret_cast<uint64_t*>(&condenser)) = val0;
	*(reinterpret_cast<uint64_t*>(&condenser) + 1) = val1;
	condenser.compact.version = version1 ? 1 : 4;
	condenser.compact.compacted = compacted;

	*ptr += length;
	return condenser;
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


// converts the two hexadecimal characters to an unsigned char (a byte)
unsigned char hexPairToChar(const char** ptr) {
	constexpr const int _[256] = {
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
	auto pos = *ptr;
	auto a = _[*pos++];
	auto b = _[*pos++];
	if (a == -1 || b == -1) {
		THROW(InvalidArgument, "Invalid UUID string hex character");
	}
	*ptr = pos;
	return a << 4 | b;
}


static inline std::array<unsigned char, 16>
uuid_to_bytes(const char* pos, size_t size)
{
	if (size != UUID_LENGTH) {
		THROW(InvalidArgument, "Invalid UUID string length");
	}
	std::array<unsigned char, 16> bytes;
	auto b = bytes.begin();
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') THROW(InvalidArgument, "Invalid UUID string character");
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') THROW(InvalidArgument, "Invalid UUID string character");
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') THROW(InvalidArgument, "Invalid UUID string character");
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') THROW(InvalidArgument, "Invalid UUID string character");
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	return bytes;
}


// create a guid from vector of bytes
Guid::Guid(const std::array<unsigned char, 16>& bytes)
	: _bytes(bytes) { }


// create a guid from string
Guid::Guid(const std::string& fromString)
	: _bytes(uuid_to_bytes(fromString.data(), fromString.size())) { }


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


inline void
Guid::compact_crush()
{
	auto variant = get_uuid_variant();
	auto version = get_uuid_version();
	if (variant == 0x80 && (version == 1 || version == 4)) {
		auto node = get_uuid1_node();

		auto salt = fnv_1a(node);
		salt &= SALT_MASK;

		GuidCondenser condenser;
		condenser.compact.compacted = true;
		condenser.compact.time = get_uuid1_time();
		if (version == 1 && condenser.compact.time) {
			condenser.compact.time -= UUID_TIME_INITIAL;
		}
		condenser.compact.clock = get_uuid1_clock_seq();
		condenser.compact.version = version;
		condenser.compact.salt = salt;
		node = condenser.calculate_node();

		uint64_t num_last = BYTE_SWAP_8(*reinterpret_cast<uint64_t*>(&_bytes[8]));
		num_last = (num_last & 0xffff000000000000ULL) | node;
		*reinterpret_cast<uint64_t*>(&_bytes[8]) = BYTE_SWAP_8(num_last);
	}
}


std::string
Guid::serialise() const
{
	auto variant = get_uuid_variant();
	auto version = get_uuid_version();

	if (variant == 0x80 && (version == 1 || version == 4)) {
		return serialise_condensed();
	}

	return serialise_full();
}


std::string
Guid::serialise_full() const
{
	auto buf = reinterpret_cast<const char*>(&_bytes[0]);

	auto ptr = buf;
	const auto end = ptr + 16;
	while (ptr != end && !*++ptr);
	if (*ptr) --ptr;
	assert(ptr != buf); // UUID is 128bit - 4bit (version) - 2bit (variant) = 122bit really used
	auto length = end - ptr;

	std::string serialised;
	serialised.reserve(length + 1);
	serialised.push_back((length - 1) & 0x0f);
	serialised.append(ptr, length);

	return serialised;
}


std::string
Guid::serialise_condensed() const
{
	GuidCondenser condenser;
	condenser.compact.compacted = true;
	condenser.compact.version = get_uuid_version();
	condenser.compact.time = get_uuid1_time();
	condenser.compact.clock = get_uuid1_clock_seq();

	if (condenser.compact.version == 1 && condenser.compact.time) {
		condenser.compact.time -= UUID_TIME_INITIAL;
	}

	auto node = get_uuid1_node();
	auto salt = node & SALT_MASK;
	condenser.compact.salt = salt;
	auto compacted_node = condenser.calculate_node();
	if (node != compacted_node) {
		condenser.expanded.compacted = false;
		condenser.expanded.node = node;
	}

	return condenser.serialise();
}


bool
Guid::is_valid(const char** ptr, const char* end)
{
	auto pos = *ptr;
	auto size = end - pos + 1;
	if (
		size == UUID_LENGTH &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		*pos++ == '-' &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		*pos++ == '-' &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		*pos++ == '-' &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		*pos++ == '-' &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++) &&
		std::isxdigit(*pos++)
	) {
		*ptr = pos;
		return true;
	}
	return false;
}


bool
Guid::is_serialised(const char** ptr, const char* end)
{
	while (*ptr != end) {
		auto size = end - *ptr;
		if (size < 2 || size > UUID_MAX_SERIALISED_LENGTH) {
			return false;
		}
		uint8_t l = **ptr;
		auto length = l >> 4;
		if (length == 0) {
			length = l & 0x0f;
			if (length == 0 || size < length + 2) {
				return false;
			}
			*ptr += length + 2;
		} else {
			if (size < length + 1) {
				return false;
			}
			*ptr += length + 1;
		}
	}
	return true;
}


Guid
Guid::unserialise(const std::string& bytes)
{
	const char* pos = bytes.data();
	const char* end = pos + bytes.size();
	return unserialise(&pos, end);
}


Guid
Guid::unserialise(const char** ptr, const char* end)
{
	auto size = end - *ptr;
	if (size < 2 || size > UUID_MAX_SERIALISED_LENGTH) {
		THROW(SerialisationError, "Bad encoded UUID");
	}
	uint8_t l = **ptr;
	auto length = l >> 4;
	if (length == 0) {
		length = l & 0x0f;
		if (length == 0 || size < length + 1) {
			THROW(SerialisationError, "Bad encoded UUID");
		}
		return unserialise_full(length + 1, ptr);
	} else {
		if (size < length + 1) {
			THROW(SerialisationError, "Bad condensed UUID");
		}
		return unserialise_condensed(length + 1, ptr);
	}
}


Guid
Guid::unserialise_full(uint8_t length, const char** ptr)
{
	Guid out;

	auto buf = reinterpret_cast<char*>(&out._bytes[0]);
	auto start = buf + 16 - length;
	std::fill(buf, start, 0);
	std::copy(*ptr, *ptr + length, start);

	*ptr += length + 1;
	return out;
}


Guid
Guid::unserialise_condensed(uint8_t length, const char** ptr)
{
	GuidCondenser condenser = GuidCondenser::unserialise(length, ptr);

	unsigned char clock_seq_low = condenser.compact.clock & 0xffULL;
	unsigned char clock_seq_hi_variant = condenser.compact.clock >> 8 | 0x80ULL;  // Variant: RFC 4122
	uint64_t node = condenser.compact.compacted ? condenser.calculate_node() : condenser.expanded.node;

	uint64_t time = condenser.compact.time;
	if (condenser.compact.version == 1 && time) {
		time += UUID_TIME_INITIAL;
	}
	unsigned time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= condenser.compact.version << 12;

	Guid out;
	*reinterpret_cast<uint32_t*>(&out._bytes[0]) = BYTE_SWAP_4(time_low);
	*reinterpret_cast<uint16_t*>(&out._bytes[4]) = BYTE_SWAP_2(time_mid);
	*reinterpret_cast<uint16_t*>(&out._bytes[6]) = BYTE_SWAP_2(time_hi_version);
	*reinterpret_cast<uint64_t*>(&out._bytes[8]) = BYTE_SWAP_8(node);
	out._bytes[8] = clock_seq_hi_variant;
	out._bytes[9] = clock_seq_low;
	return out;
}


// This is the FreBSD version.
#if defined GUID_FREEBSD
#include <cstdint>
#include <cstring>
#include <uuid.h>

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
	return Guid(byteArray);
}


// For systems that have libuuid available.
#elif defined GUID_LIBUUID
#include <uuid/uuid.h>

inline Guid
GuidGenerator::_newGuid()
{
	std::array<unsigned char, 16> byteArray;
	uuid_generate_time(byteArray.data());
	return Guid(byteArray);
}


// This is the macOS and iOS version
#elif defined GUID_CFUUID
#include <CoreFoundation/CFUUID.h>

inline Guid
GuidGenerator::_newGuid()
{
	auto newId = CFUUIDCreate(nullptr);
	auto bytes = CFUUIDGetUUIDBytes(newId);
	CFRelease(newId);

	return Guid(std::array<unsigned char, 16>{{
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
	}});
}


// Obviously this is the Windows version
#elif defined GUID_WINDOWS
#include <objbase.h>

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


// Android version that uses a call to a java api
#elif defined GUID_ANDROID

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
		guid.compact_crush();
	}
	return guid;
}
