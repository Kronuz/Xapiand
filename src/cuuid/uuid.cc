/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "cuuid/uuid.h"

#include <algorithm>                              // for std::copy_n
#include <cassert>                                // for assert
#include <iomanip>                                // for std::setw and std::setfill
#include <random>                                 // for std::mt19937
#include <sstream>                                // for std::ostringstream
#include <stdexcept>                              // for std::bad_alloc

#include "repr.hh"                                // for repr
#include "endian.hh"                              // for htobe16, be16toh, htobe32, be32toh, htobe64, be64toh
#include "exception_xapian.h"                     // for THROW, SerialisationError, InvalidArgument
#include "chars.hh"                               // for chars::char_repr, chars::hexdigit, chars::hexdec
#include "log.h"                                  // for L_*
#include "node.h"                                 // for Node


#ifndef L_UUID
#define L_UUID_DEFINED
#define L_UUID L_NOTHING
#endif


// 0x01b21dd213814000 is the number of 100-ns intervals between the
// UUID epoch 1582-10-15 00:00:00 and the Unix epoch 1970-01-01 00:00:00.
// 0x00011f0241243c00 = 1yr (365.2425 x 24 x 60 x 60 = 31556952s = 31556952000000000 nanoseconds)
constexpr uint64_t UUID_TIME_EPOCH             = 0x01b21dd213814000ULL;
constexpr uint64_t UUID_TIME_YEAR              = 0x00011f0241243c00ULL;
constexpr uint64_t UUID_TIME_INITIAL           = UUID_TIME_EPOCH + (2016 - 1970) * UUID_TIME_YEAR;
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

constexpr uint64_t TIME_MASK     =  ((1ULL << TIME_BITS)    - 1);
constexpr uint64_t SALT_MASK     =  ((1ULL << SALT_BITS)    - 1);
constexpr uint64_t CLOCK_MASK    =  ((1ULL << CLOCK_BITS)   - 1);
constexpr uint64_t NODE_MASK     =  ((1ULL << NODE_BITS)    - 1);

// Variable-length length encoding table for condensed UUIDs (prefix, mask)
static constexpr uint8_t VL[13][2][2] = {
    { { 0x1c, 0xfc }, { 0x1c, 0xfc } },  // 4:  00011100 11111100  00011100 11111100
    { { 0x18, 0xfc }, { 0x18, 0xfc } },  // 5:  00011000 11111100  00011000 11111100
    { { 0x14, 0xfc }, { 0x14, 0xfc } },  // 6:  00010100 11111100  00010100 11111100
    { { 0x10, 0xfc }, { 0x10, 0xfc } },  // 7:  00010000 11111100  00010000 11111100
    { { 0x04, 0xfc }, { 0x40, 0xc0 } },  // 8:  00000100 11111100  01000000 11000000
    { { 0x0a, 0xfe }, { 0xa0, 0xe0 } },  // 9:  00001010 11111110  10100000 11100000
    { { 0x08, 0xfe }, { 0x80, 0xe0 } },  // 10: 00001000 11111110  10000000 11100000
    { { 0x02, 0xff }, { 0x20, 0xf0 } },  // 11: 00000010 11111111  00100000 11110000
    { { 0x03, 0xff }, { 0x30, 0xf0 } },  // 12: 00000011 11111111  00110000 11110000
    { { 0x0c, 0xff }, { 0xc0, 0xf0 } },  // 13: 00001100 11111111  11000000 11110000
    { { 0x0d, 0xff }, { 0xd0, 0xf0 } },  // 14: 00001101 11111111  11010000 11110000
    { { 0x0e, 0xff }, { 0xe0, 0xf0 } },  // 15: 00001110 11111111  11100000 11110000
    { { 0x0f, 0xff }, { 0xf0, 0xf0 } },  // 16: 00001111 11111111  11110000 11110000
};


template <typename T>
static inline void
pack(char** p, T num)
{
	auto ptr = *p;
	for (size_t i = 0; i < sizeof(num); ++i) {
		*ptr++ = static_cast<char>(num & 0xff);
		num >>= 8;
	}
	*p = ptr;
}


template <typename T>
static inline T
unpack(char** const p)
{
	T num = 0;
	auto ptr = *p;
	for (size_t i = 0; i < sizeof(num); ++i) {
		num |= static_cast<T>(*ptr++) << (i * 8);
	}
	*p = ptr;
	return num;
}


static inline uint64_t fnv_1a(uint64_t num) {
	// calculate FNV-1a hash
	uint64_t fnv = 0xcbf29ce484222325ULL;
	while (num != 0u) {
		fnv ^= num & 0xff;
		fnv *= 0x100000001b3ULL;
		num >>= 8;
	}
	return fnv;
}


static inline uint64_t xor_fold(uint64_t num, int bits) {
	// xor-fold to n bits:
	uint64_t folded = 0;
	while (num != 0u) {
		folded ^= num;
		num >>= bits;
	}
	return folded;
}


/*
 * Union for condensed UUIDs
 */
union UUIDCondenser {
	struct value_t {
		uint64_t val0;
		uint64_t val1;
	} value;

	struct compact_t {
		uint64_t time        : TIME_BITS;
		uint64_t padding0    : PADDING_C0_BITS;

		uint64_t compacted   : COMPACTED_BITS;
		uint64_t padding1    : PADDING_C1_BITS;
		uint64_t salt        : SALT_BITS;
		uint64_t clock       : CLOCK_BITS;
	} compact;

	struct expanded_t {
		uint64_t time        : TIME_BITS;
		uint64_t padding0    : PADDING_E0_BITS;

		uint64_t compacted   : COMPACTED_BITS;
		uint64_t padding1    : PADDING_E1_BITS;
		uint64_t node        : NODE_BITS;
		uint64_t clock       : CLOCK_BITS;
	} expanded;

	uint64_t calculate_node() const;

	std::string serialise() const;
	static UUIDCondenser unserialise(const char** ptr, const char* end);

	UUIDCondenser();
};


UUIDCondenser::UUIDCondenser()
	: compact({ 0, 0, 0, 0, 0, 0 }) { }


inline uint64_t
UUIDCondenser::calculate_node() const
{
	L_CALL("UUIDCondenser::calculate_node()");

	if ((compact.time == 0u) && (compact.clock == 0u) && (compact.salt == 0u)) {
		return 0x010000000000;
	}

	uint32_t seed = 0;
	seed ^= fnv_1a(compact.time);
	seed ^= fnv_1a(compact.clock);
	seed ^= fnv_1a(compact.salt);
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
UUIDCondenser::serialise() const
{
	L_CALL("UUIDCondenser::serialise()");

	uint64_t buf0, buf1;
	if (compact.compacted != 0u) {
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// v0:PPPPTTTTTTTTTTTTTTTTTTtttttttttttttttttttttttttttttttttttttttttt v1:KKKKKKKKKKKKKKSSSSSSSPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPC
	// b0:                                              TTTTTTTTTTTTTTTTTT b1:ttttttttttttttttttttttttttttttttttttttttttKKKKKKKKKKKKKKSSSSSSSC
		assert(compact.padding0 == 0);
		assert(compact.padding1 == 0);
		buf0 = (value.val0 >> PADDING_C1_BITS);
		buf1 = (value.val0 << (64 - PADDING_C1_BITS)) | (value.val1 >> PADDING_C1_BITS) | 1;
	} else {
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// v0:PPPPTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTt v1:KKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNPC
	// b0:     TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT b1:tKKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNC
		assert(expanded.padding0 == 0);
		assert(expanded.padding1 == 0);
		buf0 = (value.val0 >> PADDING_E1_BITS);
		buf1 = (value.val0 << (64 - PADDING_E1_BITS)) | (value.val1 >> PADDING_E1_BITS);
	}

	char buf[UUID_MAX_SERIALISED_LENGTH];

	char* end = buf;
	*end++ = '\0';
	pack(&end, htobe64(buf0));
	pack(&end, htobe64(buf1));
	end -= 4; // serialized must be at least 4 bytes long.

	auto ptr = buf;
	while (ptr != end && (*++ptr == 0)) {}; // remove all leading zeros

	auto length = end - ptr;
	if ((*ptr & VL[length][0][1]) != 0) {
		if ((*ptr & VL[length][1][1]) != 0) {
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


inline UUIDCondenser
UUIDCondenser::unserialise(const char** ptr, const char* end)
{
	L_CALL("UUIDCondenser::unserialise({})", repr(*ptr, end));

	auto size = end - *ptr;
	auto length = size + 1;
	auto l = **ptr;
	bool q = (l & 0xf0) != 0;
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
	std::copy_n(*ptr, length, start);

	*start &= ~VL[i][q][1];

	char* p = &buf[1];
	auto buf0 = unpack<uint64_t>(&p);
	auto buf1 = unpack<uint64_t>(&p);

	UUIDCondenser condenser;
	if ((buf1 & 1) != 0u) {  // compacted
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// b0:                                                TTTTTTTTTTTTTTTT b1:ttttttttttttttttttttttttttttttttttttttttttttKKKKKKKKKKKKKKSSSSSC
	// v0:PPPPTTTTTTTTTTTTTTTTtttttttttttttttttttttttttttttttttttttttttttt v1:KKKKKKKKKKKKKKSSSSSPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPPC
		condenser.value.val0 = (buf0 << PADDING_C1_BITS) | (buf1 >> (64 - PADDING_C1_BITS));
		condenser.value.val1 = (buf1 << PADDING_C1_BITS) | 1;
	} else {
	//           .       .       .       .       .       .       .       .           .       .       .       .       .       .       .       .
	// b0:     TTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTT b1:tKKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNC
	// v0:PPPPTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTTt v1:KKKKKKKKKKKKKKNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNNPC
		condenser.value.val0 = (buf0 << PADDING_E1_BITS) | (buf1 >> (64 - PADDING_E1_BITS));
		condenser.value.val1 = (buf1 << PADDING_E1_BITS);
	}

	*ptr += length;
	return condenser;
}


// overload << so that it's easy to convert to a string
std::ostream& operator<<(std::ostream& os, const UUID& uuid) {
	os << uuid.to_string();
	return os;
}


static inline unsigned char hexPairToChar(const char** ptr) {
	auto dec = chars::hexdec(ptr);
	if (dec < 256) {
		return dec;
	}
	THROW(InvalidArgument, "Invalid UUID string hex character");
}


static inline std::array<unsigned char, 16>
uuid_to_bytes(const char* pos, size_t size)
{
	L_CALL("uuid_to_bytes({})", repr(pos, size));

	if (size != UUID_LENGTH) {
		THROW(InvalidArgument, "Invalid UUID string length");
	}
	std::array<unsigned char, 16> bytes;
	auto b = bytes.begin();
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') {
		THROW(InvalidArgument, "Invalid UUID string character");
	}
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') {
		THROW(InvalidArgument, "Invalid UUID string character");
	}
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') {
		THROW(InvalidArgument, "Invalid UUID string character");
	}
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	if (*pos++ != '-') {
		THROW(InvalidArgument, "Invalid UUID string character");
	}
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	*b++ = hexPairToChar(&pos);
	return bytes;
}


// create a UUID from vector of bytes
UUID::UUID(const std::array<unsigned char, 16>& bytes, bool little_endian)
	: _bytes(bytes)
{
	if (little_endian) {
		 std::swap(_bytes[0], _bytes[3]);
		 std::swap(_bytes[1], _bytes[2]);
		 std::swap(_bytes[4], _bytes[5]);
		 std::swap(_bytes[6], _bytes[7]);
	}
}


// create a UUID from string
UUID::UUID(const char* str, size_t size)
	: _bytes(uuid_to_bytes(str, size)) { }


UUID::UUID(std::string_view string)
	: UUID(string.data(), string.size()) { }


// create empty UUID
UUID::UUID()
	: _bytes{ } { }


// move constructor
UUID::UUID(UUID&& other)
	: _bytes(std::move(other._bytes)) { }


// overload move operator
UUID&
UUID::operator=(UUID&& other)
{
	_bytes = std::move(other._bytes);
	return *this;
}


// overload equality operator
bool
UUID::operator==(const UUID& other) const
{
	return _bytes == other._bytes;
}


// overload inequality operator
bool
UUID::operator!=(const UUID& other) const
{
	return !operator==(other);
}


// converts UUID to std::string.
std::string
UUID::to_string() const
{
	std::string uuid;
	uuid.resize(36);
	char *ptr = &uuid[0];
	chars::char_repr(_bytes[0], &ptr);
	chars::char_repr(_bytes[1], &ptr);
	chars::char_repr(_bytes[2], &ptr);
	chars::char_repr(_bytes[3], &ptr);
	*ptr++ = '-';
	chars::char_repr(_bytes[4], &ptr);
	chars::char_repr(_bytes[5], &ptr);
	*ptr++ = '-';
	chars::char_repr(_bytes[6], &ptr);
	chars::char_repr(_bytes[7], &ptr);
	*ptr++ = '-';
	chars::char_repr(_bytes[8], &ptr);
	chars::char_repr(_bytes[9], &ptr);
	*ptr++ = '-';
	chars::char_repr(_bytes[10], &ptr);
	chars::char_repr(_bytes[11], &ptr);
	chars::char_repr(_bytes[12], &ptr);
	chars::char_repr(_bytes[13], &ptr);
	chars::char_repr(_bytes[14], &ptr);
	chars::char_repr(_bytes[15], &ptr);
	return uuid;
}


void
UUID::uuid1_node(uint64_t node)
{
	auto node_ptr = reinterpret_cast<uint64_t*>(&_bytes[8]);
	auto hnode = be64toh(*node_ptr) & 0xffff000000000000ULL;
	*node_ptr = htobe64(hnode | (node & 0xffffffffffffULL));
}


void
UUID::uuid1_time(uint64_t time)
{
	auto time_low_ptr = reinterpret_cast<uint32_t*>(&_bytes[0]);
	auto time_mid_ptr = reinterpret_cast<uint16_t*>(&_bytes[4]);
	auto time_hi_and_version_ptr = reinterpret_cast<uint16_t*>(&_bytes[6]);

	unsigned time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= be16toh(*time_hi_and_version_ptr) & 0xf000ULL;

	*time_low_ptr = htobe32(time_low);
	*time_mid_ptr = htobe16(time_mid);
	*time_hi_and_version_ptr = htobe16(time_hi_version);
}


void
UUID::uuid1_clock_seq(uint16_t clock_seq)
{
	uint8_t clock_seq_low = clock_seq & 0xffULL;
	uint8_t clock_seq_hi_variant = (clock_seq >> 8) & 0x3fULL;
	clock_seq_hi_variant |= _bytes[8] & 0xc0ULL;
	_bytes[8] = clock_seq_hi_variant;
	_bytes[9] = clock_seq_low;
}


void
UUID::uuid_variant(uint8_t variant)
{
	uint8_t clock_seq_hi_variant = variant & 0xc0ULL;
	clock_seq_hi_variant |= _bytes[8] & 0x3fULL;
	_bytes[8] = clock_seq_hi_variant;
}


void
UUID::uuid_version(uint8_t version)
{
	_bytes[6] = (_bytes[6] & 0x0fULL) | ((version & 0x0f) << 4);
}


uint64_t
UUID::uuid1_node() const
{
	auto node_ptr = reinterpret_cast<const uint64_t*>(&_bytes[8]);
	return be64toh(*node_ptr) & 0xffffffffffffULL;
}


uint64_t
UUID::uuid1_time() const
{
	auto time_low_ptr = reinterpret_cast<const uint32_t*>(&_bytes[0]);
	auto time_mid_ptr = reinterpret_cast<const uint16_t*>(&_bytes[4]);
	auto time_hi_and_version_ptr = reinterpret_cast<const uint16_t*>(&_bytes[6]);
	uint64_t time = be16toh(*time_hi_and_version_ptr) & 0xfffULL;
	time <<= 16;
	time |= be16toh(*time_mid_ptr);
	time <<= 32;
	time |= be32toh(*time_low_ptr);
	return time;
}


uint16_t
UUID::uuid1_clock_seq() const
{
	auto clock_seq_ptr = reinterpret_cast<const uint16_t*>(&_bytes[8]);
	return be16toh(*clock_seq_ptr) & 0x3fffULL;
}


uint8_t
UUID::uuid_variant() const
{
	return _bytes[8] & 0xc0ULL;
}


uint8_t
UUID::uuid_version() const
{
	return _bytes[6] >> 4;
}

bool
UUID::empty() const
{
	return memcmp(_bytes.data(), "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16) == 0;
}


void
UUID::compact_crush()
{
	L_CALL("UUID::compact_crush()");

	if (uuid_variant() == 0x80 && uuid_version() == 1) {
		auto node = uuid1_node();

		auto clock = uuid1_clock_seq();

		auto time = uuid1_time();
		auto compacted_time = time != 0u ? ((time - UUID_TIME_INITIAL) & TIME_MASK) : time;
		auto compacted_time_clock = compacted_time & CLOCK_MASK;
		compacted_time >>= CLOCK_BITS;

		UUIDCondenser condenser;
		condenser.compact.compacted = 1u;
		condenser.compact.clock = clock ^ compacted_time_clock;
		condenser.compact.time = compacted_time;
		if ((node & 0x010000000000) != 0u) {
			condenser.compact.salt = node & SALT_MASK;
		} else {
			auto local_node = Node::local_node();
			auto salt = fnv_1a((local_node ? local_node->idx : 0) || node);
			salt = xor_fold(salt, SALT_BITS);
			condenser.compact.salt = salt & SALT_MASK;
		}

		uuid1_node(condenser.calculate_node());

		uuid1_clock_seq(condenser.compact.clock);

		time = condenser.compact.time;
		if (time != 0u) {
			time = ((time << CLOCK_BITS) + UUID_TIME_INITIAL) & TIME_MASK;
		}
		uuid1_time(time);
	}

}


std::string
UUID::serialise() const
{
	L_CALL("UUID::serialise()");

	if (uuid_variant() == 0x80 && uuid_version() == 1) {
		return serialise_condensed();
	}

	return serialise_full();
}


std::string
UUID::serialise_full() const
{
	L_CALL("UUID::serialise_full()");

	std::string serialised;
	serialised.reserve(17);
	serialised.push_back(0x01);
	serialised.append(reinterpret_cast<const char*>(&_bytes[0]), 16);
	return serialised;
}


std::string
UUID::serialise_condensed() const
{
	L_CALL("UUID::serialise_condensed()");

	auto node = uuid1_node();

	auto clock = uuid1_clock_seq();

	auto time = uuid1_time();
	auto compacted_time = time != 0u ? ((time - UUID_TIME_INITIAL) & TIME_MASK) : time;
	auto compacted_time_clock = compacted_time & CLOCK_MASK;
	compacted_time >>= CLOCK_BITS;

	UUIDCondenser condenser;
	condenser.compact.compacted = 1u;
	condenser.compact.clock = clock ^ compacted_time_clock;
	condenser.compact.time = compacted_time;
	condenser.compact.salt = node & SALT_MASK;

	auto compacted_node = condenser.calculate_node();
	if (node != compacted_node) {
		condenser.expanded.compacted = 0u;
		if ((node & 0x010000000000) == 0u) {
			if (time != 0u) {
				time = (time - UUID_TIME_INITIAL) & TIME_MASK;
			}
		}
		condenser.expanded.clock = clock;
		condenser.expanded.time = time;
		condenser.expanded.node = node;
	}

	return condenser.serialise();
}


bool
UUID::is_valid(const char** ptr, const char* end)
{
	auto pos = *ptr;
	auto size = end - pos;
	if (
		size == UUID_LENGTH &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		*pos++ == '-' &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		*pos++ == '-' &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		*pos++ == '-' &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		*pos++ == '-' &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16) &&
		(chars::hexdigit(*pos++) < 16)
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
	if (size < 2) {
		return false;
	}
	auto length = size + 1;
	uint8_t l = **ptr;
	if (l == 1) {
		length = 17;
	} else {
		bool q = (l & 0xf0) != 0;
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
UUID::is_serialised(const char** ptr, const char* end)
{
	while (*ptr != end) {
		if (!_is_serialised(ptr, end)) {
			return false;
		}
	}
	return true;
}


UUID
UUID::unserialise(std::string_view bytes)
{
	const char* pos = bytes.data();
	const char* end = pos + bytes.size();
	return unserialise(&pos, end);
}


UUID
UUID::unserialise(const char** ptr, const char* end)
{
	L_CALL("UUID::unserialise({})", repr(*ptr, end));

	auto size = end - *ptr;
	if (size < 2) {
		THROW(SerialisationError, "Bad encoded UUID");
	}

	if (**ptr == 1) {
		return unserialise_full(ptr, end);
	}

	return unserialise_condensed(ptr, end);
}


UUID
UUID::unserialise_full(const char** ptr, const char* end)
{
	L_CALL("UUID::unserialise_full({})", repr(*ptr, end));

	auto size = end - *ptr;
	if (size < 17) {
		THROW(SerialisationError, "Bad encoded UUID");
	}

	UUID out;
	std::copy_n(*ptr + 1, 16, reinterpret_cast<char*>(&out._bytes[0]));

	*ptr += 17;
	return out;
}


UUID
UUID::unserialise_condensed(const char** ptr, const char* end)
{
	L_CALL("UUID::unserialise_condensed({})", repr(*ptr, end));

	UUIDCondenser condenser = UUIDCondenser::unserialise(ptr, end);

	uint64_t node = condenser.compact.compacted != 0u ? condenser.calculate_node() : condenser.expanded.node;

	uint64_t time = condenser.compact.time;
	if (time != 0u) {
		if (condenser.compact.compacted != 0u) {
			time = ((time << CLOCK_BITS) + UUID_TIME_INITIAL) & TIME_MASK;
		} else if ((node & 0x010000000000) == 0u) {
			time = (time + UUID_TIME_INITIAL) & TIME_MASK;
		}
	}

	uint32_t time_low = time & 0xffffffffULL;
	uint16_t time_mid = (time >> 32) & 0xffffULL;
	uint16_t time_hi_version = (time >> 48) & 0xfffULL;
	time_hi_version |= 0x1000ULL; // Version 1: RFC 4122

	uint8_t clock_seq_hi_variant = condenser.compact.clock >> 8 | 0x80ULL;  // Variant: RFC 4122
	uint8_t clock_seq_low = condenser.compact.clock & 0xffULL;

	UUID out;
	auto time_low_ptr = reinterpret_cast<uint32_t*>(&out._bytes[0]);
	auto time_mid_ptr = reinterpret_cast<uint16_t*>(&out._bytes[4]);
	auto time_hi_and_version_ptr = reinterpret_cast<uint16_t*>(&out._bytes[6]);
	auto node_ptr = reinterpret_cast<uint64_t*>(&out._bytes[8]);

	*time_low_ptr = htobe32(time_low);
	*time_mid_ptr = htobe16(time_mid);
	*time_hi_and_version_ptr = htobe16(time_hi_version);
	*node_ptr = htobe64(node);
	out._bytes[8] = clock_seq_hi_variant;
	out._bytes[9] = clock_seq_low;

	return out;
}


// This is the FreBSD version.
#if defined UUID_FREEBSD
#include <cstdint>
#include <cstring>
#include <uuid.h>

inline UUID
UUIDGenerator::newUUID()
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
	return UUID(byteArray);
}


// For systems that have libuuid available.
#elif defined UUID_LIBUUID
#include <uuid/uuid.h>

#if defined(__APPLE__)
std::mutex UUIDGenerator::mtx;
#endif

inline UUID
UUIDGenerator::newUUID()
{
#if defined(__APPLE__)
	std::lock_guard<std::mutex> lk(mtx);
#endif
	std::array<unsigned char, 16> byteArray;
	uuid_generate_time(byteArray.data());
	return UUID(byteArray);
}


// This is the macOS and iOS version
#elif defined UUID_CFUUID
#include <CoreFoundation/CFUUID.h>

inline UUID
UUIDGenerator::newUUID()
{
	auto newId = CFUUIDCreate(nullptr);
	auto bytes = CFUUIDGetUUIDBytes(newId);
	CFRelease(newId);

	return UUID(std::array<unsigned char, 16>{{
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
#elif defined UUID_WINDOWS
#include <objbase.h>

inline UUID
UUIDGenerator::newUUID()
{
	GUID newId;
	CoCreateUUID(&newId);

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


UUID
UUIDGenerator::operator ()(bool compact)
{
	auto uuid = newUUID();
	if (compact) {
		uuid.compact_crush();
	}
	return uuid;
}


#ifdef L_UUID_DEFINED
#undef L_UUID_DEFINED
#undef L_UUID
#endif
