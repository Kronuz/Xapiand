# -*- coding: utf-8 -*-
from __future__ import absolute_import, division

import six
import uuid
import string

try:
    from . import base59
except ValueError:
    import base59

try:
    from .mertwis import MT19937
except ValueError:
    from mertwis import MT19937


__all__ = ['UUID']


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
        if (UUID.VL[i][q][0] == (byte0 & UUID.VL[i][q][1])):
            length = i + 4
            break
    if (size < length):
        raise ValueError("Bad encoded uuid")

    list_bytes_ = list(bytes_[:length])
    byte0 &= ~UUID.VL[i][q][1]
    list_bytes_[0] = chr(byte0)

    meat = 0
    for s in list_bytes_:
        meat <<= 8
        meat |= ord(s)

    compacted = not (meat & 1)
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
            time *= UUID.UUID_TIME_DIVISOR
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


def unserialise_compound(bytes_, repr='base59'):
    byte0 = ord(bytes_[0])
    byte1 = ord(bytes_[-1])
    if repr == 'guid':
        return ";".join("{%s}" % u for u in unserialise(bytes_))
    elif repr == 'urn':
        return "urn:uuid:" + ";".join(unserialise(bytes_))
    elif repr == 'base59':
        if byte0 != 1 and (byte1 & 1) == 0:
            return base59.b59encode(bytes_)
    return ";".join(unserialise(bytes_))


def serialise(uuids_):
    serialised = b''
    for uuid_ in uuids_:
        if all(c in base59.alphabet for c in uuid_):
            try:
                serialised += base59.b59decode(uuid_)
            except Exception:
                raise ValueError("Invalid UUID format %s" % uuid_)
        elif len(uuid_) == UUID.UUID_LENGTH and uuid_[8] == '-' and uuid_[13] == '-' and uuid_[18] == '-' and uuid_[23] == '-':
            hex_uuid = uuid_[0:8] + uuid_[9:4] + uuid_[14:4] + uuid_[19:4] + uuid_[24:12]
            if all(c in string.hexdigits for c in hex_uuid):
                u = UUID(uuid_)
                serialised += u.serialise()
            else:
                raise ValueError("Invalid UUID format %s" % uuid_)
        else:
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
    Anonymous UUID is 00000000-0000-1000-8000-000000000000
    """
    UUID_TIME_INITIAL = 0x1e5b039c8040800

    UUID_MIN_SERIALISED_LENGTH = 2
    UUID_MAX_SERIALISED_LENGTH = 17
    UUID_LENGTH = 36
    UUID_TIME_DIVISOR = 10000

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
    COMPACTED_MASK = ((1 << COMPACTED_BITS) - 1)
    NODE_MASK = ((1 << NODE_BITS) - 1)
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

    def __new__(self, *args, **kwargs):
        return six.binary_type.__new__(self, uuid.UUID(*args, **kwargs))

    @classmethod
    def _calculate_node(cls, time, clock, salt):
        seed = 0
        seed ^= _fnv_1a(time)
        seed ^= _fnv_1a(clock)
        seed ^= _fnv_1a(salt)
        g = MT19937(seed & 0xffffffff)
        node = g()
        node <<= 32
        node |= g()
        node &= cls.NODE_MASK & ~cls.SALT_MASK
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
            data_len = len(data)
            if data_len < cls.UUID_MIN_SERIALISED_LENGTH or data_len > cls.UUID_MAX_SERIALISED_LENGTH:
                raise ValueError("Serialise UUID can store [%d, %d] bytes, given %d" % (2, cls.UUID_MAX_SERIALISED_LENGTH, data_len))
            num = 0
            node = 0
            for d in reversed(data):
                num <<= 8
                num |= ord(d)
                node ^= num
            node &= 0xffffffffffff
            time_low = num & 0xffffffff
            num >>= 32
            time_mid = num & 0xffff
            num >>= 16
            time_hi_version = num & 0xfff
            num >>= 12
            clock_seq_hi_variant = num & 0x3f
            num >>= 6
            clock_seq_low = num & 0xff
            if compacted or compacted is None and data_len <= 9:
                compacted = True
                time_hi_version |= 0x1000  # Version 1
            else:
                compacted = False
                time_hi_version |= 0x4000  # Version 4
            clock_seq_hi_variant |= 0x80  # Variant: RFC 4122
            num = cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node))
        else:
            num = uuid.uuid1()
        if compacted or compacted is None:
            time = num.time
            if time:
                time = (time - cls.UUID_TIME_INITIAL)
                time = time // cls.UUID_TIME_DIVISOR
            clock = num.clock_seq
            salt = None
            if num.node & 0x010000000000:
                salt = num.node & cls.SALT_MASK
            else:
                salt = _fnv_1a(num.node)
                salt = xor_fold(salt, cls.SALT_BITS)
                salt = salt & cls.SALT_MASK
            node = cls._calculate_node(time, clock, salt)
            num = cls(fields=(num.fields[:-1] + (node,)))
        return num

    def data(self):
        num = 0
        if self.version == 4:
            num = self.node
        num <<= 8
        num |= self.clock_seq_low & 0xff
        num <<= 6
        num |= self.clock_seq_hi_variant & 0x3f
        num <<= 12
        num |= self.time_hi_version & 0xfff
        num <<= 16
        num |= self.time_mid & 0xffff
        num <<= 32
        num |= self.time_low & 0xffffffff
        data = ''
        while num:
            data += chr(num & 0xff)
            num >>= 8
        return data

    @classmethod
    def unserialise(cls, bytes_):
        return _unserialise(bytes_)[0]

    def _serialise(self, variant):
        cls = self.__class__
        version = ord(self.bytes[6]) >> 4
        if variant == 0x80 and version == 1:
            time = self.time & cls.TIME_MASK
            if time:
                time = (time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK
            clock = self.clock_seq & cls.CLOCK_MASK
            node = self.node & cls.NODE_MASK
            if node & 0x010000000000:
                salt = node & cls.SALT_MASK
            else:
                salt = _fnv_1a(node)
                salt = xor_fold(salt, cls.SALT_BITS)
                salt = salt & cls.SALT_MASK
            compacted_time = time // cls.UUID_TIME_DIVISOR
            compacted_node = cls._calculate_node(compacted_time, clock, salt)
            compacted = node == compacted_node

            if compacted:
                meat = compacted_time
                meat <<= cls.CLOCK_BITS
                meat |= clock
                meat <<= cls.SALT_BITS
                meat |= salt
                meat <<= cls.COMPACTED_BITS
            else:
                meat = time
                meat <<= cls.CLOCK_BITS
                meat |= clock
                meat <<= cls.NODE_BITS
                meat |= node
                meat <<= cls.COMPACTED_BITS
                meat |= 1

            bytes_ = []
            while meat or len(bytes_) < 4:
                bytes_.append(meat & 0xff)
                meat >>= 8
            length = len(bytes_) - 4
            if bytes_[-1] & cls.VL[length][0][1]:
                if bytes_[-1] & cls.VL[length][1][1]:
                    bytes_.append(cls.VL[length + 1][0][0])
                else:
                    bytes_[-1] |= cls.VL[length][1][0]
            else:
                bytes_[-1] |= cls.VL[length][0][0]
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
            time = self.time & cls.TIME_MASK
            if not time or time > cls.UUID_TIME_INITIAL:
                if time:
                    time = (time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK
                clock = self.clock_seq & cls.CLOCK_MASK
                node = self.node & cls.NODE_MASK
                if node & 0x010000000000:
                    salt = node & cls.SALT_MASK
                else:
                    salt = _fnv_1a(node)
                    salt = xor_fold(salt, cls.SALT_BITS)
                    salt = salt & cls.SALT_MASK
                compacted_time = time // cls.UUID_TIME_DIVISOR
                compacted_node = cls._calculate_node(compacted_time, clock, salt)
                time = compacted_time * cls.UUID_TIME_DIVISOR
                if time:
                    time = (time + cls.UUID_TIME_INITIAL) & cls.TIME_MASK
                time_low = time & 0xffffffff
                time_mid = (time >> 32) & 0xffff
                time_hi_version = (time >> 48) & 0xfff
                time_hi_version |= 0x1000
                clock_seq_low = clock & 0xff
                clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122
                return UUID(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, compacted_node))


    def get_calculated_node(self):
        cls = self.__class__
        version = self.version
        variant = self.clock_seq_hi_variant & 0x80
        if variant == 0x80 and version == 1:
            time = self.time
            if time:
                time = (time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK
            clock = self.clock_seq & cls.CLOCK_MASK
            node = self.node & cls.NODE_MASK
            if node & 0x010000000000:
                salt = node & cls.SALT_MASK
            else:
                salt = _fnv_1a(node)
                salt = xor_fold(salt, cls.SALT_BITS)
                salt = salt & cls.SALT_MASK
            compacted_time = time // cls.UUID_TIME_DIVISOR
            compacted_node = cls._calculate_node(compacted_time, clock, salt)
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

    str_uuids = [
        # Full:
        '5759b016-10c0-4526-a981-47d6d19f6fb4',
        'e8b13d1b-665f-4f4c-aa83-76fa782b030a',
        # Condensed:
        '00000000-0000-1000-8000-000000000000',
        '11111111-1111-1111-8111-111111111111',
        # Condensed + Compacted:
        '230c3300-dc3c-11e7-9266-a9cf6771112b',
        'f223c600-debf-11e7-85f7-cdf2b3c2e82b',
        # Condensed + Expanded:
        '60579016-dec5-11e7-b616-34363bc9ddd6',
        '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662',
    ]
    expected_serialised = [
        repr('\x01WY\xb0\x16\x10\xc0E&\xa9\x81G\xd6\xd1\x9fo\xb4'),
        repr('\x01\xe8\xb1=\x1bf_OL\xaa\x83v\xfax+\x03\n'),
        repr('\x1c\x00\x00\x01'),
        repr('\xf7\x95\xb0k\xa4\x86\x84\x88\x82""""""#'),
        repr('\x07\x8e\xf7)l\x12fV'),
        repr('\x07\x93\x15\xfax\x05\xf7V'),
        repr('\xe1\x17E\xcc)\xc4\x0bl,hlw\x93\xbb\xad'),
        repr('\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc5'),
    ]
    expected_compund = [
        repr('5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a'),
        repr('00000000-0000-1000-8000-000000000000;11111111-1111-1111-8111-111111111111'),
        repr('njMlLhOOkwOFCDe3bqolhf'),
        repr('60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'),
    ]

    ################################################
    for i, uuids in enumerate((str_uuids[0:2], str_uuids[2:4], str_uuids[4:6], str_uuids[6:8])):
        expected = expected_compund[i]
        result = repr(unserialise_compound(serialise_compound(uuids)))
        if result != expected:
            errors += 1
            print("Error in compund serialization: %s  Expected: %s Result: %s" % (uuids, expected, result))
    ################################################

    for i, str_uuid in enumerate(str_uuids):
        expected = expected_serialised[i]
        try:
            u = UUID(str_uuid)
            serialised = u.serialise()
            result = repr(serialised)
            res = unserialise(serialised)
            if result != expected:
                errors += 1
                print("Error in serialise: %s  Expected: %s Result: %s" % (str_uuid, expected, result))
            uns_u = UUID.unserialise(serialised)
            if uns_u != u:
                errors += 1
                print("Error in unserialise: Expected: %s Result: %s" % (str_uuid, uns_u))
        except ValueError as e:
            errors += 1
            print(e.message)

    str_uuids = [
        '{00000000-0000-0000-0000-000000000000;00000000-0000-1000-8000-000000000000;00000000-0000-1000-a000-000000000000}',
        '{00000000-0000-4000-b000-000000000000;00000000-2000-1000-c000-000000000000;00000000-2000-4000-c000-000000000000}',
        '{00000000-2000-2000-0000-000000000000;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662;b6e0e797-80fc-11e6-b58a-60f81dc76762}',
        '{d095e48f-c64f-4f08-91ec-888e6068dfe0;c5c52a08-c3b4-11e6-9231-339cb51d7742;c5c52a08-c3b4-51e6-7231-339cb51d7742}',
    ]
    expected_serialised = [
        repr('\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1c\x00\x00\x01\n@\x00\x00\x00\x00\x00\x00\x01'),
        repr('\x01\x00\x00\x00\x00\x00\x00@\x00\xb0\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00 \x00\x10\x00\xc0\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00 \x00@\x00\xc0\x00\x00\x00\x00\x00\x00\x00'),
        repr('\x01\x00\x00\x00\x00 \x00 \x00\x00\x00\x00\x00\x00\x00\x00\x00\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc5\x0ehawno\xcb\xeb\x14\xc1\xf0;\x8e\xce\xc5'),
        repr('\x01\xd0\x95\xe4\x8f\xc6OO\x08\x91\xec\x88\x8e`h\xdf\xe0\x0e\x89\xbd~\xe0\x91\x04$bg9j:\xee\x85\x01\xc5\xc5*\x08\xc3\xb4Q\xe6r13\x9c\xb5\x1dwB'),
    ]
    i = 0
    for str_uuid in str_uuids:
        try:
            serialised = serialise_compound(str_uuid)
            expected = expected_serialised[i]
            result = repr(serialised)
            if result != expected:
                errors += 1
                print("Error in serialise: %s  Expected: %s Result: %s" % (str_uuid, expected, result))
            uns_uuuids = unserialise(serialised)
            compound_uuid = '{'
            for uuid_ in uns_uuuids:
                compound_uuid += uuid_ + ';'
            compound_uuid = compound_uuid[:-1] + '}'
            if compound_uuid != str_uuid:
                errors += 1
                print("Error in unserialise: Expected: %s Result: %s" % (str_uuid, compound_uuid))
        except ValueError as e:
            errors += 1
            print(e.message)
        i += 1
    if errors == 0:
        print("Pass all tests")
    else:
        print("Finished with %d errors" % errors)
