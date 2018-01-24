# -*- coding: utf-8 -*-
from __future__ import absolute_import, division

import six
import uuid
import string

try:
    from . import base_x
except ValueError:
    import base_x

try:
    from .mertwis import MT19937
except ValueError:
    from mertwis import MT19937


__all__ = ['UUID']


ENCODER = base_x.b59


def _fnv_1a(num):
    # calculate FNV-1a hash
    fnv = 0xcbf29ce484222325
    while num:
        fnv ^= num & 0xff
        fnv *= 0x100000001b3
        fnv &= 0xffffffffffffffff
        num >>= 8
    return fnv


def xor_fold(num, bits):
    # xor-fold to n bits:
    folded = 0
    while num:
        folded ^= num
        num >>= bits
    return folded


def _unserialise_condensed(bytes_):
    size = len(bytes_)
    length = size
    byte0 = ord(bytes_[0])
    q = bool(byte0 & 0xf0)
    for i in range(13):
        if UUID.VL[i][q][0] == (byte0 & UUID.VL[i][q][1]):
            length = i + 4
            break
    if size < length:
        raise ValueError("Bad encoded uuid")

    list_bytes_ = list(bytes_[:length])
    byte0 &= ~UUID.VL[i][q][1]
    list_bytes_[0] = chr(byte0)

    meat = 0
    for s in list_bytes_:
        meat <<= 8
        meat |= ord(s)

    compacted = meat & 1
    meat >>= UUID.COMPACTED_BITS

    if compacted:
        salt = meat & UUID.SALT_MASK
        meat >>= UUID.SALT_BITS
        clock = meat & UUID.CLOCK_MASK
        meat >>= UUID.CLOCK_BITS
        time = meat & UUID.TIME_MASK
        node = UUID._calculate_node(time, clock, salt)
    else:
        node = meat & UUID.NODE_MASK
        meat >>= UUID.NODE_BITS
        clock = meat & UUID.CLOCK_MASK
        meat >>= UUID.CLOCK_BITS
        time = meat & UUID.TIME_MASK

    if time:
        if compacted:
            time = ((time << UUID.CLOCK_BITS) + UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK
        elif not (node & 0x010000000000):
            time = (time + UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK

    time_low = time & 0xffffffff
    time_mid = (time >> 32) & 0xffff
    time_hi_version = (time >> 48) & 0xfff
    time_hi_version |= 0x1000
    clock_seq_low = clock & 0xff
    clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122

    return UUID(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node)), length


def _unserialise_full(bytes_):
    if len(bytes_) < 17:
        raise ValueError("Bad encoded uuid %s" % repr(bytes_))
    return UUID(bytes=bytes_[1:17]), 17


def _unserialise(bytes_):
    if bytes_ is None or len(bytes_) < 2:
        raise ValueError("Bad encoded uuid %s" % repr(bytes_))

    if (bytes_ and ord(bytes_[0]) == 1):
        return _unserialise_full(bytes_)
    else:
        return _unserialise_condensed(bytes_)


def unserialise(bytes_):
    uuids = []
    while bytes_:
        uuid, length = _unserialise(bytes_)
        uuids.append(uuid)
        bytes_ = bytes_[length:]
    return uuids


def unserialise_compound(bytes_, repr='encoded'):
    if isinstance(bytes_, UUID):
        return unserialise_compound(bytes_.serialise())
    elif isinstance(bytes_, six.string_types):
        if repr == 'guid':
            return ";".join("{%s}" % u for u in unserialise(bytes_))
        elif repr == 'urn':
            return "urn:uuid:" + ";".join(unserialise(bytes_))
        elif repr == 'encoded':
            if ord(bytes_[0]) != 1 and ((ord(bytes_[-1]) & 1) or (len(bytes_) >= 6 and ord(bytes_[-6]) & 2)):
                return '~' + ENCODER.encode(bytes_)
        return ";".join(unserialise(bytes_))


def _is_serialised(uuid_):
    if (uuid_.length < 2):
        return False
    length = uuid_.length + 1
    if (uuid_[0] == 1):
        length = 17
    else:
        q = bool(uuid_[0] & 0xf0)
        for i in range(13):
            if (UUID.VL[i][q][0] == (uuid_[0] & UUID.VL[i][q][1])):
                length = i + 4
                break
    if (uuid_ < length):
        return False
    return True


def serialise(uuids_):
    serialised = b''
    for uuid_ in uuids_:
        uuid_sz = len(uuid_)
        if uuid_sz == UUID.UUID_LENGTH and uuid_[8] == '-' and uuid_[13] == '-' and uuid_[18] == '-' and uuid_[23] == '-':
            hex_uuid = uuid_[0:8] + uuid_[9:4] + uuid_[14:4] + uuid_[19:4] + uuid_[24:12]
            if all(c in string.hexdigits for c in hex_uuid):
                u = UUID(uuid_)
                serialised += u.serialise()
                continue
        elif uuid_sz >= 7 and uuid_[0] == '~':
            serialised += ENCODER.decode(uuid_)
            if _is_serialised(serialised):
                continue
        raise ValueError("Invalid UUID format %s" % uuid_)

    return serialised


def serialise_compound(compound_uuid):
    if isinstance(compound_uuid, six.string_types):
        if len(compound_uuid) > 2:
            if compound_uuid[0] == '{' and compound_uuid[-1] == '}':
                compound_uuid = compound_uuid[1:-1]
            elif compound_uuid.startswith('urn:uuid:'):
                compound_uuid = compound_uuid[9:]
            if compound_uuid:
                uuids_ = compound_uuid.split(';')
                bytes_ = serialise(uuids_)
                return bytes_
    elif isinstance(compound_uuid, (list, tuple)):
        bytes_ = serialise(compound_uuid)
        return bytes_
    raise ValueError("Invalid UUID format in: %s" % compound_uuid)


class UUID(six.binary_type, uuid.UUID):
    """
    Anonymous UUID is 00000000-0000-1000-8000-010000000000
    """
    UUID_TIME_EPOCH = 0x01b21dd213814000
    UUID_TIME_YEAR = 0x00011f0241243c00
    UUID_TIME_INITIAL = UUID_TIME_EPOCH + (2016 - 1970) * UUID_TIME_YEAR

    UUID_MIN_SERIALISED_LENGTH = 2
    UUID_MAX_SERIALISED_LENGTH = 17
    UUID_LENGTH = 36

    TIME_BITS = 60
    VERSION_BITS = 64 - TIME_BITS
    COMPACTED_BITS = 1
    SALT_BITS = 7
    CLOCK_BITS = 14
    NODE_BITS = 48
    PADDING_BITS = 64 - COMPACTED_BITS - SALT_BITS - CLOCK_BITS
    PADDING1_BITS = 64 - COMPACTED_BITS - NODE_BITS - CLOCK_BITS
    VARIANT_BITS = 2

    TIME_MASK = ((1 << TIME_BITS) - 1)
    SALT_MASK = ((1 << SALT_BITS) - 1)
    CLOCK_MASK = ((1 << CLOCK_BITS) - 1)
    NODE_MASK = ((1 << NODE_BITS) - 1)
    COMPACTED_MASK = ((1 << COMPACTED_BITS) - 1)
    VERSION_MASK = ((1 << VERSION_BITS) - 1)
    VARIANT_MASK = ((1 << VARIANT_BITS) - 1)

    VL = [
        [[0x1c, 0xfc], [0x1c, 0xfc]],  # 4:  00011100 11111100  00011100 11111100
        [[0x18, 0xfc], [0x18, 0xfc]],  # 5:  00011000 11111100  00011000 11111100
        [[0x14, 0xfc], [0x14, 0xfc]],  # 6:  00010100 11111100  00010100 11111100
        [[0x10, 0xfc], [0x10, 0xfc]],  # 7:  00010000 11111100  00010000 11111100
        [[0x04, 0xfc], [0x40, 0xc0]],  # 8:  00000100 11111100  01000000 11000000
        [[0x0a, 0xfe], [0xa0, 0xe0]],  # 9:  00001010 11111110  10100000 11100000
        [[0x08, 0xfe], [0x80, 0xe0]],  # 10: 00001000 11111110  10000000 11100000
        [[0x02, 0xff], [0x20, 0xf0]],  # 11: 00000010 11111111  00100000 11110000
        [[0x03, 0xff], [0x30, 0xf0]],  # 12: 00000011 11111111  00110000 11110000
        [[0x0c, 0xff], [0xc0, 0xf0]],  # 13: 00001100 11111111  11000000 11110000
        [[0x0d, 0xff], [0xd0, 0xf0]],  # 14: 00001101 11111111  11010000 11110000
        [[0x0e, 0xff], [0xe0, 0xf0]],  # 15: 00001110 11111111  11100000 11110000
        [[0x0f, 0xff], [0xf0, 0xf0]],  # 16: 00001111 11111111  11110000 11110000
    ]

    def __new__(cls, *args, **kwargs):
        return six.binary_type.__new__(cls, uuid.UUID(*args, **kwargs))

    @classmethod
    def _calculate_node(cls, time, clock, salt):
        if not time and not clock and not salt:
            return 0x010000000000
        seed = 0
        seed ^= _fnv_1a(time)
        seed ^= _fnv_1a(clock)
        seed ^= _fnv_1a(salt)
        g = MT19937(seed & 0xffffffff)
        node = g()
        node <<= 32
        node |= g()
        node &= UUID.NODE_MASK & ~UUID.SALT_MASK
        node |= salt
        node |= 0x010000000000  # set multicast bit
        return node

    @classmethod
    def new(cls, data=None, compacted=None):
        # 0                   1                   2                   3
        # 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |                           time_low                            |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |           time_mid            |       time_hi_and_version     |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |clk_seq_hi_res |  clk_seq_low  |          node (0-1)           |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # |                         node (2-5)                            |
        # +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        # time = 60 bits
        # clock = 14 bits
        # node = 48 bits
        if data is not None:
            num = 0
            for d in data:
                num <<= 8
                num |= ord(d)
            node = ((num << 1) & 0xfe0000000000) | num & 0x00ffffffffff | 0x010000000000
            num >>= 47
            clock_seq_low = num & 0xff
            num >>= 8
            clock_seq_hi_variant = num & 0x3f
            num >>= 6
            time_low = num & 0xffffffff
            num >>= 32
            time_mid = num & 0xffff
            num >>= 16
            time_hi_version = num & 0xfff
            num >>= 12
            if num:
                raise ValueError("UUIDs can only store as much as 15 bytes")
            time_hi_version |= 0x1000  # Version 1
            clock_seq_hi_variant |= 0x80  # Variant: RFC 4122
            num = cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node))
            compacted = False
        else:
            num = uuid.uuid1()
        if compacted or compacted is None:
            clock = num.clock_seq & UUID.CLOCK_MASK
            time = num.time & UUID.TIME_MASK
            compacted_time = ((time - UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK) if time else 0
            compacted_time_clock = compacted_time & UUID.CLOCK_MASK
            compacted_time >>= UUID.CLOCK_BITS
            compacted_clock = clock ^ compacted_time_clock
            salt = None
            if num.node & 0x010000000000:
                salt = num.node & UUID.SALT_MASK
            else:
                salt = _fnv_1a(num.node)
                salt = xor_fold(salt, UUID.SALT_BITS)
                salt = salt & UUID.SALT_MASK
            compacted_node = cls._calculate_node(compacted_time, compacted_clock, salt)
            clock = compacted_clock
            time = compacted_time
            if time:
                time = ((time << UUID.CLOCK_BITS) + UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK
            time_low = time & 0xffffffff
            time_mid = (time >> 32) & 0xffff
            time_hi_version = (time >> 48) & 0xfff
            time_hi_version |= 0x1000
            clock_seq_low = clock & 0xff
            clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122
            num = cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, compacted_node))
        return num

    def data(self):
        num = 0
        version = self.version
        variant = self.clock_seq_hi_variant & 0x80
        if variant == 0x80 and version == 1 and self.node & 0x010000000000:
            num <<= 12
            num |= self.time_hi_version & 0xfff
            num <<= 16
            num |= self.time_mid & 0xffff
            num <<= 32
            num |= self.time_low & 0xffffffff
            num <<= 6
            num |= self.clock_seq_hi_variant & 0x3f
            num <<= 8
            num |= self.clock_seq_low & 0xff
            num <<= 47
            num |= ((self.node & 0xfe0000000000) >> 1) | (self.node & 0x00ffffffffff)
        data = []
        while num:
            data.append(chr(num & 0xff))
            num >>= 8
        return ''.join(reversed(data))

    @classmethod
    def unserialise(cls, bytes_):
        return _unserialise(bytes_)[0]

    def _serialise(self, variant):
        cls = self.__class__
        version = ord(self.bytes[6]) >> 4
        if variant == 0x80 and version == 1:
            node = self.node & UUID.NODE_MASK
            clock = self.clock_seq & UUID.CLOCK_MASK
            time = self.time & UUID.TIME_MASK
            compacted_time = ((time - UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK) if time else 0
            compacted_time_clock = compacted_time & UUID.CLOCK_MASK
            compacted_time >>= UUID.CLOCK_BITS
            compacted_clock = clock ^ compacted_time_clock
            if node & 0x010000000000:
                salt = node & UUID.SALT_MASK
            else:
                salt = _fnv_1a(node)
                salt = xor_fold(salt, UUID.SALT_BITS)
                salt = salt & UUID.SALT_MASK
            compacted_node = cls._calculate_node(compacted_time, compacted_clock, salt)
            compacted = node == compacted_node

            if compacted:
                meat = compacted_time
                meat <<= UUID.CLOCK_BITS
                meat |= compacted_clock
                meat <<= UUID.SALT_BITS
                meat |= salt
                meat <<= UUID.COMPACTED_BITS
                meat |= 1
            else:
                if not (node & 0x010000000000):
                    if time:
                        time = (time - UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK
                meat = time
                meat <<= UUID.CLOCK_BITS
                meat |= clock
                meat <<= UUID.NODE_BITS
                meat |= node
                meat <<= UUID.COMPACTED_BITS

            bytes_ = []
            while meat or len(bytes_) < 4:
                bytes_.append(meat & 0xff)
                meat >>= 8
            length = len(bytes_) - 4
            if bytes_[-1] & UUID.VL[length][0][1]:
                if bytes_[-1] & UUID.VL[length][1][1]:
                    bytes_.append(UUID.VL[length + 1][0][0])
                else:
                    bytes_[-1] |= UUID.VL[length][1][0]
            else:
                bytes_[-1] |= UUID.VL[length][0][0]
            return ''.join(chr(c) for c in reversed(bytes_))
        else:
            return chr(0x01) + self.bytes

    def serialise(self):
        return self._serialise(ord(self.bytes[8]) & 0xc0)

    def compact_crush(self):
        cls = self.__class__
        version = self.version
        variant = self.clock_seq_hi_variant & 0x80
        if variant == 0x80 and version == 1:
            node = self.node & UUID.NODE_MASK
            clock = self.clock_seq & UUID.CLOCK_MASK
            time = self.time & UUID.TIME_MASK
            compacted_time = ((time - UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK) if time else 0
            compacted_time_clock = compacted_time & UUID.CLOCK_MASK
            compacted_time >>= UUID.CLOCK_BITS
            compacted_clock = clock ^ compacted_time_clock
            if node & 0x010000000000:
                salt = node & UUID.SALT_MASK
            else:
                salt = _fnv_1a(node)
                salt = xor_fold(salt, UUID.SALT_BITS)
                salt = salt & UUID.SALT_MASK
            compacted_node = cls._calculate_node(compacted_time, compacted_clock, salt)
            clock = compacted_clock
            time = compacted_time
            if time:
                time = ((time << UUID.CLOCK_BITS) + UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK
            time_low = time & 0xffffffff
            time_mid = (time >> 32) & 0xffff
            time_hi_version = (time >> 48) & 0xfff
            time_hi_version |= 0x1000
            clock_seq_low = clock & 0xff
            clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122
            return cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, compacted_node))

    def get_calculated_node(self):
        cls = self.__class__
        version = self.version
        variant = self.clock_seq_hi_variant & 0x80
        if variant == 0x80 and version == 1:
            node = self.node & UUID.NODE_MASK
            clock = self.clock_seq & UUID.CLOCK_MASK
            time = self.time & UUID.TIME_MASK
            compacted_time = ((time - UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK) if time else 0
            compacted_time_clock = compacted_time & UUID.CLOCK_MASK
            compacted_time >>= UUID.CLOCK_BITS
            compacted_clock = clock ^ compacted_time_clock
            if node & 0x010000000000:
                salt = node & UUID.SALT_MASK
            else:
                salt = _fnv_1a(node)
                salt = xor_fold(salt, UUID.SALT_BITS)
                salt = salt & UUID.SALT_MASK
            compacted_node = cls._calculate_node(compacted_time, compacted_clock, salt)
            compacted = node == compacted_node
            if compacted:
                return compacted_node

    @property
    def calculated_node(self):
        return self.get_calculated_node()

    def iscompact(self):
        return self.calculated_node == self.node


if __name__ == '__main__':
    errors = 0

    uuids = [
        # Full:
        ('5759b016-10c0-4526-a981-47d6d19f6fb4', repr('5759b016-10c0-4526-a981-47d6d19f6fb4'), repr('\x01WY\xb0\x16\x10\xc0E&\xa9\x81G\xd6\xd1\x9fo\xb4')),
        ('e8b13d1b-665f-4f4c-aa83-76fa782b030a', repr('e8b13d1b-665f-4f4c-aa83-76fa782b030a'), repr('\x01\xe8\xb1=\x1bf_OL\xaa\x83v\xfax+\x03\n')),
        # Condensed:
        ('00000000-0000-1000-8000-000000000000', repr('00000000-0000-1000-8000-000000000000'), repr('\x1c\x00\x00\x00')),
        ('11111111-1111-1111-8111-111111111111', repr('~yc9DnemYGNTMdKXsYYiTKOc'), repr('\x0f\x88\x88\x88\x88\x88\x88\x88\x82"""""""')),
        # Condensed + Compacted:
        ('230c0800-dc3c-11e7-b966-a3ab262e682b', repr('~SsQq3dJdg3P'), repr('\x06,\x02[\b9fW')),
        ('f2238800-debf-11e7-bbf7-dffcee0c03ab', repr('~SUkSiXYTT8c'), repr('\x06.\x86*\x1f\xbb\xf7W')),
        # Condensed + Expanded:
        ('60579016-dec5-11e7-b616-34363bc9ddd6', repr('60579016-dec5-11e7-b616-34363bc9ddd6'), repr('\xe1\x17E\xcc)\xc4\x0bl,hlw\x93\xbb\xac')),
        ('4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', repr('4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'), repr('\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc4')),
        # Other:
        ('00000000-0000-0000-0000-000000000000', repr('00000000-0000-0000-0000-000000000000'), repr('\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')),
        ('00000000-0000-1000-8000-010000000000', repr('~notmet'), repr('\x1c\x00\x00\x01')),
        ('11111111-1111-1111-8111-101111111111', repr('11111111-1111-1111-8111-101111111111'), repr('\xf7\x95\xb0k\xa4\x86\x84\x88\x82" """""')),
        ('00000000-0000-1000-a000-000000000000', repr('00000000-0000-1000-a000-000000000000'), repr('\n@\x00\x00\x00\x00\x00\x00\x00')),
        # Coumpound:
        ('5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a', repr('5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a'), repr('\x01WY\xb0\x16\x10\xc0E&\xa9\x81G\xd6\xd1\x9fo\xb4\x01\xe8\xb1=\x1bf_OL\xaa\x83v\xfax+\x03\n')),
        ('00000000-0000-1000-8000-000000000000;11111111-1111-1111-8111-111111111111', repr('~WPQUDun7rkRr7TkQ2PSfCHGo4WWz'), repr('\x1c\x00\x00\x00\x0f\x88\x88\x88\x88\x88\x88\x88\x82"""""""')),
        ('230c0800-dc3c-11e7-b966-a3ab262e682b;f2238800-debf-11e7-bbf7-dffcee0c03ab', repr('~EYBuNUmS8MZs98Mq64McVQ'), repr('\x06,\x02[\b9fW\x06.\x86*\x1f\xbb\xf7W')),
        ('60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', repr('60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'), repr('\xe1\x17E\xcc)\xc4\x0bl,hlw\x93\xbb\xac\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc4')),
        ('00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111', repr('00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111'), repr('\x1c\x00\x00\x01\xf7\x95\xb0k\xa4\x86\x84\x88\x82" """""')),
    ]

    ################################################

    for i, (str_uuid, expected_encoded, expected_serialised) in enumerate(uuids):
        try:
            serialised = serialise_compound(str_uuid)

            result_str_uuid = ';'.join(unserialise(serialised))
            if result_str_uuid != str_uuid:
                errors += 1
                print("Error in unserialise:\n\tResult: %s\n\tExpected: %s" % (result_str_uuid, str_uuid))

            result_encoded = repr(unserialise_compound(serialised))
            if result_encoded != expected_encoded:
                errors += 1
                print("Error in serialise: %s\n\tResult: %s\n\tExpected: %s" % (str_uuid, result_encoded, expected_encoded))

            result_seriaised = repr(serialised)
            if result_seriaised != expected_serialised:
                errors += 1
                print("Error in serialise: %s\n\tResult: %s\n\tExpected: %s" % (str_uuid, result_seriaised, expected_serialised))
        except ValueError as e:
            errors += 1
            print(e.message)

    if errors == 0:
        print("Pass all tests")
    else:
        print("Finished with %d errors" % errors)
