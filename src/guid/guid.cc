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

#include "serialise.h"    // for BYTE_SWAP_*

#include <algorithm>      // for std::copy
#include <iomanip>        // for std::setw and std::setfill
#include <random>         // for std::mt19937
#include <sstream>        // for std::ostringstream
#include <stdexcept>      // for std::invalid_argument

// 0x01b21dd213814000 is the number of 100-ns intervals between the
// UUID epoch 1582-10-15 00:00:00 and the Unix epoch 1970-01-01 00:00:00.
// 0x00011f0241243c00 = 1yr (365.2425 x 24 x 60 x 60 = 31556952s = 31556952000000000 nanoseconds)
constexpr uint64_t UUID_TIME_EPOCH             = 0x01b21dd213814000ULL;
constexpr uint64_t UUID_TIME_YEAR              = 0x00011f0241243c00ULL;
constexpr uint64_t UUID_TIME_INITIAL           = UUID_TIME_EPOCH + (2016 - 1970) * UUID_TIME_YEAR;
constexpr uint64_t UUID_TIME_DIVISOR           = 10000;
constexpr uint8_t  UUID_MAX_SERIALISED_LENGTH  = 17;

constexpr uint8_t TIME_BITS       = 60;
constexpr uint8_t PADDING_C0_BITS = 64 - TIME_BITS;
constexpr uint8_t PADDING_E0_BITS = 64 - TIME_BITS;
constexpr uint8_t COMPACTED_BITS  = 1;
constexpr uint8_t SALT_BITS       = 7;
constexpr uint8_t CLOCK_BITS      = 14;
constexpr uint8_t NODE_BITS       = 48;
constexpr uint8_t PADDING_C1_BITS = 64 - COMPACTED_BITS - SALT_BITS - CLOCK_BITS;
constexpr uint8_t PADDING_E1_BITS = 64 - COMPACTED_BITS - NODE_BITS - CLOCK_BITS;

constexpr uint64_t SALT_MASK     =  ((1ULL << SALT_BITS)    - 1);
constexpr uint64_t NODE_MASK     =  ((1ULL << NODE_BITS)    - 1);

// Variable-length length encoding table for condensed UUIDs (prefix, mask)
static constexpr uint8_t VL[13][2][2] = {
    { { 0x1c, 0xfc }, { 0x1c, 0xfc } },  //  4: 00011100 11111100  00011100 11111100
    { { 0x18, 0xfc }, { 0x18, 0xfc } },  //  5: 00011000 11111100  00011000 11111100
    { { 0x14, 0xfc }, { 0x14, 0xfc } },  //  6: 00010100 11111100  00010100 11111100
    { { 0x10, 0xfc }, { 0x10, 0xfc } },  //  7: 00010000 11111100  00010000 11111100
    { { 0x04, 0xfc }, { 0x40, 0xc0 } },  //  8: 00000100 11111100  01000000 11000000
    { { 0x0a, 0xfe }, { 0xa0, 0xe0 } },  //  9: 00001010 11111110  10100000 11100000
    { { 0x08, 0xfe }, { 0x80, 0xe0 } },  // 10: 00001000 11111110  10000000 11100000
    { { 0x02, 0xff }, { 0x20, 0xf0 } },  // 11: 00000010 11111111  00100000 11110000
    { { 0x03, 0xff }, { 0x30, 0xf0 } },  // 12: 00000011 11111111  00110000 11110000
    { { 0x0c, 0xff }, { 0xc0, 0xf0 } },  // 13: 00001100 11111111  11000000 11110000
    { { 0x0d, 0xff }, { 0xd0, 0xf0 } },  // 14: 00001101 11111111  11010000 11110000
    { { 0x0e, 0xff }, { 0xe0, 0xf0 } },  // 15: 00001110 11111111  11100000 11110000
    { { 0x0f, 0xff }, { 0xf0, 0xf0 } },  // 16: 00001111 11111111  11110000 11110000
};


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


static inline uint64_t xor_fold(uint64_t num, int bits) {
	// xor-fold to n bits:
	uint64_t folded = 0;
	while (num) {
		folded ^= num;
		num >>= bits;
	}
	return folded;
}


/*
 * Union for condensed UUIDs
 */
union GuidCondenser {
	struct compact_t {
		uint64_t time        : TIME_BITS;
		uint64_t padding0    : PADDING_C0_BITS;

		uint64_t padding1    : PADDING_C1_BITS;
		uint64_t compacted   : COMPACTED_BITS;
		uint64_t salt        : SALT_BITS;
		uint64_t clock       : CLOCK_BITS;
	} compact;

	struct expanded_t {
		uint64_t time        : TIME_BITS;
		uint64_t padding0    : PADDING_E0_BITS;

		uint64_t padding1    : PADDING_E1_BITS;
		uint64_t compacted   : COMPACTED_BITS;
		uint64_t node        : NODE_BITS;
		uint64_t clock       : CLOCK_BITS;
	} expanded;

	uint64_t calculate_node() const;

	std::string serialise() const;
	static GuidCondenser unserialise(const char** ptr, const char* end);

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
	node |= 0x010000000000; // set multicast bit
	return node;
}


inline std::string
GuidCondenser::serialise() const
{
	auto val0 = *(reinterpret_cast<const uint64_t*>(this));
	auto val1 = *(reinterpret_cast<const uint64_t*>(this) + 1);

	uint64_t buf0, buf1;
	if (compact.compacted) {
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// v0:PPPPTTTTTTTTTTTTTTTTtttttttttttttttttttttttttttttttttttttttttttt v1:KKKKKKKKKKKKKKSSSSSCPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
	// b0:                                                TTTTTTTTTTTTTTTT b1:ttttttttttttttttttttttttttttttttttttttttttttKKKKKKKKKKKKKKSSSSSC
		buf0 = val0 >> PADDING_C1_BITS;
		buf1 = val0 << (64 - PADDING_C1_BITS) | val1 >> PADDING_C1_BITS;
	} else {
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// v0:PPPPTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTt v1:KKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNCP
	// b0:     TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT b1:tKKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNC
		buf0 = val0 >> PADDING_E1_BITS;
		buf1 = val0 << (64 - PADDING_E1_BITS) | val1 >> PADDING_E1_BITS;
	}

	char buf[UUID_MAX_SERIALISED_LENGTH];
	*(reinterpret_cast<uint64_t*>(buf + 1)) = BYTE_SWAP_8(buf0);
	*(reinterpret_cast<uint64_t*>(buf + 1) + 1) = BYTE_SWAP_8(buf1);
	buf[0] = '\0';

	auto ptr = buf;
	const auto end = ptr + sizeof(buf) - 4; // serialized must be at least 4 bytes long.
	while (ptr != end && !*++ptr); // remove all leading zeros

	auto length = end - ptr;
	if (*ptr & VL[length][0][1]) {
		if (*ptr & VL[length][1][1]) {
			--ptr;
			++length;
			*ptr |= VL[length][0][0];
		} else {
			*ptr |= VL[length][1][0];
		}
	} else {
		*ptr |= VL[length][0][0];
	}

	return std::string(ptr, length + 4);
}


inline GuidCondenser
GuidCondenser::unserialise(const char** ptr, const char* end)
{
	auto size = end - *ptr;
	auto length = size + 1;
	auto l = **ptr;
	bool q = (l & 0xf0);
	int i = 0;
	for (; i < 13; ++i) {
		if (VL[i][q][0] == (l & VL[i][q][1])) {
			length = i + 4;
			break;
		}
	}
	if (size < length) {
		THROW(SerialisationError, "Bad condensed UUID");
	}

	char buf[UUID_MAX_SERIALISED_LENGTH];
	auto start = buf + UUID_MAX_SERIALISED_LENGTH - length;
	std::fill(buf, start, 0);
	std::copy(*ptr, *ptr + length, start);

	*start &= ~VL[i][q][1];

	auto buf0 = BYTE_SWAP_8(*(reinterpret_cast<uint64_t*>(buf + 1)));
	auto buf1 = BYTE_SWAP_8(*(reinterpret_cast<uint64_t*>(buf + 1) + 1));

	uint64_t val0, val1;
	if (buf1 & 1) {  // compacted
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// b0:                                                TTTTTTTTTTTTTTTT b1:ttttttttttttttttttttttttttttttttttttttttttttKKKKKKKKKKKKKKSSSSSC
	// v0:PPPPTTTTTTTTTTTTTTTTtttttttttttttttttttttttttttttttttttttttttttt v1:KKKKKKKKKKKKKKSSSSSCPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPP
		val0 = buf0 << PADDING_C1_BITS | buf1 >> (64 - PADDING_C1_BITS);
		val1 = buf1 << PADDING_C1_BITS;
	} else {
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// b0:     TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT b1:tKKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNC
	// v0:PPPPTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTt v1:KKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNCP
		val0 = buf0 << PADDING_E1_BITS | buf1 >> (64 - PADDING_E1_BITS);
		val1 = buf1 << PADDING_E1_BITS;
	}

	GuidCondenser condenser;
	*(reinterpret_cast<uint64_t*>(&condenser)) = val0;
	*(reinterpret_cast<uint64_t*>(&condenser) + 1) = val1;

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
	auto a = _[static_cast<unsigned char>(*pos++)];
	auto b = _[static_cast<unsigned char>(*pos++)];
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
Guid::Guid(const char* str, size_t size)
	: _bytes(uuid_to_bytes(str, size)) { }


Guid::Guid(const std::string& string)
	: Guid(string.data(), string.size()) { }


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


void
Guid::uuid1_node(uint64_t node)
{
	auto node_ptr = reinterpret_cast<uint64_t*>(&_bytes[8]);
	*node_ptr = BYTE_SWAP_8((BYTE_SWAP_8(*node_ptr) & 0xffff000000000000ULL) | (node & 0xffffffffffffULL));
}


void
Guid::uuid1_time(uint64_t time)
{
	auto time_low_ptr = reinterpret_cast<uint32_t*>(&_bytes[0]);
	auto time_mid_ptr = reinterpret_cast<uint16_t*>(&_bytes[4]);
	auto time_hi_and_version_ptr = reinterpret_cast<uint16_t*>(&_bytes[6]);

	unsigned time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= BYTE_SWAP_2(*time_hi_and_version_ptr) & 0xf000ULL;

	*time_low_ptr = BYTE_SWAP_4(time_low);
	*time_mid_ptr = BYTE_SWAP_2(time_mid);
	*time_hi_and_version_ptr = BYTE_SWAP_2(time_hi_version);
}


void
Guid::uuid1_clock_seq(uint16_t clock_seq)
{
	uint8_t clock_seq_low = clock_seq & 0xffULL;
	uint8_t clock_seq_hi_variant = (clock_seq >> 8) & 0x3fULL;
	clock_seq_hi_variant |= _bytes[8] & 0xc0ULL;
	_bytes[8] = clock_seq_hi_variant;
	_bytes[9] = clock_seq_low;
}


void
Guid::uuid_variant(uint8_t variant)
{
	uint8_t clock_seq_hi_variant = variant & 0xc0ULL;
	clock_seq_hi_variant |= _bytes[8] & 0x3fULL;
	_bytes[8] = clock_seq_hi_variant;
}


void
Guid::uuid_version(uint8_t version)
{
	_bytes[6] = (_bytes[6] & 0x0fULL) | ((version & 0x0f) << 4);
}


uint64_t
Guid::uuid1_node() const
{
	auto node_ptr = reinterpret_cast<const uint64_t*>(&_bytes[8]);
	return BYTE_SWAP_8(*node_ptr) & 0xffffffffffffULL;
}


uint64_t
Guid::uuid1_time() const
{
	auto time_low_ptr = reinterpret_cast<const uint32_t*>(&_bytes[0]);
	auto time_mid_ptr = reinterpret_cast<const uint16_t*>(&_bytes[4]);
	auto time_hi_and_version_ptr = reinterpret_cast<const uint16_t*>(&_bytes[6]);
	uint64_t time = BYTE_SWAP_2(*time_hi_and_version_ptr) & 0xfffULL;
	time <<= 16;
	time |= BYTE_SWAP_2(*time_mid_ptr);
	time <<= 32;
	time |= BYTE_SWAP_4(*time_low_ptr);
	return time;
}


uint16_t
Guid::uuid1_clock_seq() const
{
	auto clock_seq_ptr = reinterpret_cast<const uint16_t*>(&_bytes[8]);
	return BYTE_SWAP_2(*clock_seq_ptr) & 0x3fffULL;
}


uint8_t
Guid::uuid_variant() const
{
	return _bytes[8] & 0xc0ULL;
}


uint8_t
Guid::uuid_version() const
{
	return _bytes[6] >> 4;
}


void
Guid::compact_crush()
{
	if (uuid_variant() == 0x80 && uuid_version() == 1) {
		auto time = uuid1_time();
		if (!time || time > UUID_TIME_INITIAL) {
			if (time) time -= UUID_TIME_INITIAL;

			auto node = uuid1_node();

			GuidCondenser condenser;
			condenser.compact.compacted = true;
			condenser.compact.clock = uuid1_clock_seq();
			condenser.compact.time = time / UUID_TIME_DIVISOR;
			if (node & 0x010000000000) {
				condenser.compact.salt = node & SALT_MASK;
			} else {
				auto salt = fnv_1a(node);
				salt = xor_fold(salt, SALT_BITS);
				condenser.compact.salt = salt & SALT_MASK;
			}

			uuid1_node(condenser.calculate_node());
		}
	}
}


std::string
Guid::serialise() const
{
	if (uuid_variant() == 0x80 && uuid_version() == 1) {
		return serialise_condensed();
	}

	return serialise_full();
}


std::string
Guid::serialise_full() const
{
	auto buf = reinterpret_cast<const char*>(&_bytes[0]);

	auto ptr = buf;
	const auto end = ptr + 16 - 10;
	while (ptr != end && !*++ptr); // remove all leading zeros

	auto length = end - ptr;
	if (*ptr) ++length;

	uint8_t l = (length << 5) | 0x10ULL;

	std::string serialised;
	serialised.reserve(length + 10);
	serialised.push_back(l);
	serialised.append(ptr, length + 9);

	return serialised;
}


std::string
Guid::serialise_condensed() const
{
	auto time = uuid1_time();
	if (time) time -= UUID_TIME_INITIAL;

	auto node = uuid1_node();

	GuidCondenser condenser;
	condenser.compact.compacted = true;
	condenser.compact.clock = uuid1_clock_seq();
	condenser.compact.time = time / UUID_TIME_DIVISOR;
	condenser.compact.salt = node & SALT_MASK;

	auto compacted_node = condenser.calculate_node();
	if (node != compacted_node) {
		condenser.expanded.compacted = false;
		condenser.compact.time = time;
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


static inline bool
_is_serialised(const char** ptr, const char* end)
{
	auto size = end - *ptr;
	if (size < 2 || size > UUID_MAX_SERIALISED_LENGTH) {
		return false;
	}
	auto length = size + 1;
	uint8_t l = **ptr;
	if (l == 1) {
		length = 17;
	} else {
		bool q = (l & 0xf0);
		for (int i = 0; i < 13; ++i) {
			if (VL[i][q][0] == (l & VL[i][q][1])) {
				length = i + 4;
				break;
			}
		}
	}
	if (size < length) {
		return false;
	}
	*ptr += length;
	return true;
}


bool
Guid::is_serialised(const char** ptr, const char* end)
{
	while (*ptr != end) {
		if (!_is_serialised(ptr, end)) {
			return false;
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
	if (**ptr == 1) {
		return unserialise_full(ptr, end);
	} else {
		return unserialise_condensed(ptr, end);
	}
}


Guid
Guid::unserialise_full(const char** ptr, const char* end)
{
	auto size = end - *ptr;
	auto length = 17;
	if (size < length) {
		THROW(SerialisationError, "Bad encoded UUID");
	}

	Guid out;

	auto buf = reinterpret_cast<char*>(&out._bytes[0]);
	auto start = buf + 16 - length + 1;
	std::fill(buf, start, 0);
	std::copy(*ptr + 1, *ptr + length, start);

	*ptr += length;
	return out;
}


Guid
Guid::unserialise_condensed(const char** ptr, const char* end)
{
	GuidCondenser condenser = GuidCondenser::unserialise(ptr, end);

	uint64_t time = condenser.compact.time;
	if (condenser.compact.compacted) time *= UUID_TIME_DIVISOR;
	if (time) time += UUID_TIME_INITIAL;

	uint32_t time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= 0x1000ULL; // Version 1: RFC 4122

	uint64_t node = condenser.compact.compacted ? condenser.calculate_node() : condenser.expanded.node;

	uint8_t clock_seq_hi_variant = condenser.compact.clock >> 8 | 0x80ULL;  // Variant: RFC 4122
	uint8_t clock_seq_low = condenser.compact.clock & 0xffULL;

	Guid out;
	auto time_low_ptr = reinterpret_cast<uint32_t*>(&out._bytes[0]);
	auto time_mid_ptr = reinterpret_cast<uint16_t*>(&out._bytes[4]);
	auto time_hi_and_version_ptr = reinterpret_cast<uint16_t*>(&out._bytes[6]);
	auto node_ptr = reinterpret_cast<uint64_t*>(&out._bytes[8]);

	*time_low_ptr = BYTE_SWAP_4(time_low);
	*time_mid_ptr = BYTE_SWAP_2(time_mid);
	*time_hi_and_version_ptr = BYTE_SWAP_2(time_hi_version);
	*node_ptr = BYTE_SWAP_8(node);
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
