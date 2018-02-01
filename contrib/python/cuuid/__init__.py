# -*- coding: utf-8 -*-
#
# Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
from __future__ import absolute_import, division

__all__ = ['UUID', 'uuid', 'uuid1', 'uuid3', 'uuid4', 'uuid5',
           'unserialise', 'encode', 'decode',
           'NAMESPACE_DNS', 'NAMESPACE_URL', 'NAMESPACE_OID', 'NAMESPACE_X500']

import six
import uuid as _uuid

try:
    from . import base_x
except ValueError:
    import base_x

try:
    from .mertwis import MT19937
except ValueError:
    from mertwis import MT19937


def fnv_1a(num):
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


def unserialise(serialised):
    uuids = []
    while serialised:
        uuid, length = UUID._unserialise(serialised)
        uuids.append(uuid)
        serialised = serialised[length:]
    return uuids


def encode(serialised, encoding='encoded'):
    if isinstance(serialised, six.string_types):
        if encoding == 'guid':
            return b';'.join('{%s}' % u for u in unserialise(serialised))
        elif encoding == 'urn':
            return b'urn:uuid:' + ';'.join(unserialise(serialised))
        elif encoding == 'encoded':
            if ord(serialised[0]) != 1 and ((ord(serialised[-1]) & 1) or (len(serialised) >= 6 and ord(serialised[-6]) & 2)):
                return b'~' + UUID.ENCODER.encode(serialised)
        return b';'.join(unserialise(serialised))
    raise ValueError("Invalid serialised UUID: %r" % serialised)


def decode(encoded):
    if isinstance(encoded, six.string_types):
        if len(encoded) > 2:
            if encoded[0] == '{' and encoded[-1] == '}':
                encoded = encoded[1:-1]
            elif encoded.startswith('urn:uuid:'):
                encoded = encoded[9:]
            if encoded:
                encoded = encoded.split(';')
    if isinstance(encoded, (list, tuple)):
        return b''.join(UUID._decode(u) for u in encoded)
    raise ValueError("Invalid encoded UUID: %r" % encoded)


class UUID(six.binary_type, _uuid.UUID):
    """
    Compact UUID
    Anonymous UUID is 00000000-0000-1000-8000-010000000000
    """

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
    #
    # time = 60 bits
    # clock = 14 bits
    # node = 48 bits

    ENCODER = base_x.b59

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

    # Lengths table
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

    def __new__(cls, hex=None, bytes=None, bytes_le=None, fields=None, int=None, version=None):
        if [hex, bytes, bytes_le, fields, int].count(None) != 4:
            raise TypeError('need one of hex, bytes, bytes_le, fields, or int')
        if isinstance(hex, six.binary_type):
            hex = cls.unserialise(cls._decode(hex, 1))
        if isinstance(hex, _uuid.UUID):
            u = hex
        else:
            u = _uuid.UUID(hex=hex, bytes=bytes, bytes_le=bytes_le, fields=fields, int=int, version=version)
        self = six.binary_type.__new__(cls, u)
        self.__dict__['int'] = u.int
        return self

    def __init__(self, hex=None, bytes=None, bytes_le=None, fields=None, int=None, version=None):
        if 'int' not in self.__dict__:
            cls = self.__class__
            u = cls(hex=hex, bytes=bytes, bytes_le=bytes_le, fields=fields, int=int, version=version)
            self.__dict__['int'] = u.int

    @classmethod
    def _is_serialised(cls, serialised, count=None):
        while serialised:
            if count is not None:
                if not count:
                    return False
                count -= 1
            size = len(serialised)
            if size < 2:
                return False
            byte0 = ord(serialised[0])
            if byte0 == 1:
                length = 17
            else:
                length = size
                q = bool(byte0 & 0xf0)
                for i in range(13):
                    if cls.VL[i][q][0] == (byte0 & cls.VL[i][q][1]):
                        length = i + 4
                        break
            if size < length:
                return False
            serialised = serialised[length:]
        return True

    @classmethod
    def _decode(cls, encoded, count=None):
        if len(encoded) >= 7 and encoded[0] == '~':
            serialised = cls.ENCODER.decode(encoded)
            if cls._is_serialised(serialised, count):
                return serialised
        u = cls(_uuid.UUID(encoded))
        return u.serialise()

    @classmethod
    def _unserialise_condensed(cls, serialised):
        size = len(serialised)
        length = size
        byte0 = ord(serialised[0])
        q = bool(byte0 & 0xf0)
        for i in range(13):
            if cls.VL[i][q][0] == (byte0 & cls.VL[i][q][1]):
                length = i + 4
                break
        if size < length:
            raise ValueError("Bad encoded uuid")

        list_bytes_ = list(serialised[:length])
        byte0 &= ~cls.VL[i][q][1]
        list_bytes_[0] = chr(byte0)

        meat = 0
        for s in list_bytes_:
            meat <<= 8
            meat |= ord(s)

        compacted = meat & 1
        meat >>= cls.COMPACTED_BITS

        if compacted:
            salt = meat & cls.SALT_MASK
            meat >>= cls.SALT_BITS
            clock = meat & cls.CLOCK_MASK
            meat >>= cls.CLOCK_BITS
            time = meat & cls.TIME_MASK
            node = cls._calculate_node(time, clock, salt)
        else:
            node = meat & cls.NODE_MASK
            meat >>= cls.NODE_BITS
            clock = meat & cls.CLOCK_MASK
            meat >>= cls.CLOCK_BITS
            time = meat & cls.TIME_MASK

        if time:
            if compacted:
                time = ((time << cls.CLOCK_BITS) + cls.UUID_TIME_INITIAL) & cls.TIME_MASK
            elif not (node & 0x010000000000):
                time = (time + cls.UUID_TIME_INITIAL) & cls.TIME_MASK

        time_low = time & 0xffffffff
        time_mid = (time >> 32) & 0xffff
        time_hi_version = (time >> 48) & 0xfff
        time_hi_version |= 0x1000
        clock_seq_low = clock & 0xff
        clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122

        return cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node)), length

    @classmethod
    def _unserialise_full(cls, serialised):
        if len(serialised) < 17:
            raise ValueError("Bad encoded uuid %s" % repr(serialised))
        return cls(bytes=serialised[1:17]), 17

    @classmethod
    def _unserialise(cls, serialised):
        if serialised is None or len(serialised) < 2:
            raise ValueError("Bad encoded uuid %s" % repr(serialised))

        if (serialised and ord(serialised[0]) == 1):
            return cls._unserialise_full(serialised)
        else:
            return cls._unserialise_condensed(serialised)

    @classmethod
    def _calculate_node(cls, time, clock, salt):
        if not time and not clock and not salt:
            return 0x010000000000
        seed = 0
        seed ^= fnv_1a(time)
        seed ^= fnv_1a(clock)
        seed ^= fnv_1a(salt)
        g = MT19937(seed & 0xffffffff)
        node = g()
        node <<= 32
        node |= g()
        node &= cls.NODE_MASK & ~cls.SALT_MASK
        node |= salt
        node |= 0x010000000000  # set multicast bit
        return node

    @classmethod
    def _serialise(cls, self):
        variant = ord(self.bytes[8]) & 0xc0
        version = ord(self.bytes[6]) >> 4
        if variant == 0x80 and version == 1:
            node = self.node & cls.NODE_MASK
            clock = self.clock_seq & cls.CLOCK_MASK
            time = self.time & cls.TIME_MASK
            compacted_time = ((time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK) if time else 0
            compacted_time_clock = compacted_time & cls.CLOCK_MASK
            compacted_time >>= cls.CLOCK_BITS
            compacted_clock = clock ^ compacted_time_clock
            if node & 0x010000000000:
                salt = node & cls.SALT_MASK
            else:
                salt = fnv_1a(node)
                salt = xor_fold(salt, cls.SALT_BITS)
                salt = salt & cls.SALT_MASK
            compacted_node = cls._calculate_node(compacted_time, compacted_clock, salt)
            compacted = node == compacted_node

            if compacted:
                meat = compacted_time
                meat <<= cls.CLOCK_BITS
                meat |= compacted_clock
                meat <<= cls.SALT_BITS
                meat |= salt
                meat <<= cls.COMPACTED_BITS
                meat |= 1
            else:
                if not (node & 0x010000000000):
                    if time:
                        time = (time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK
                meat = time
                meat <<= cls.CLOCK_BITS
                meat |= clock
                meat <<= cls.NODE_BITS
                meat |= node
                meat <<= cls.COMPACTED_BITS

            serialised = []
            while meat or len(serialised) < 4:
                serialised.append(meat & 0xff)
                meat >>= 8
            length = len(serialised) - 4
            if serialised[-1] & cls.VL[length][0][1]:
                if serialised[-1] & cls.VL[length][1][1]:
                    serialised.append(cls.VL[length + 1][0][0])
                else:
                    serialised[-1] |= cls.VL[length][1][0]
            else:
                serialised[-1] |= cls.VL[length][0][0]
            serialised = ''.join(chr(c) for c in reversed(serialised))
            if compacted_time:
                compacted_time = ((compacted_time << cls.CLOCK_BITS) + cls.UUID_TIME_INITIAL) & cls.TIME_MASK
        else:
            compacted_node = None
            compacted_time = None
            compacted_clock = None
            serialised = chr(0x01) + self.bytes
        return serialised, compacted_node, compacted_time, compacted_clock

    @classmethod
    def new(cls, data=None, compacted=None):
        if data is not None:
            num = 0
            for d in data:
                num <<= 8
                num |= ord(d)
            node = ((num << 1) & 0xfe0000000000) | num & 0x00ffffffffff
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
            node |= 0x010000000000  # Multicast bit set
            return cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node))

        num = UUID(_uuid.uuid1())
        if compacted or compacted is None:
            num = num.compact_crush() or num
        return num

    @classmethod
    def unserialise(cls, serialised):
        uuid, length = cls._unserialise(serialised)
        if length > len(serialised):
            raise ValueError("Invalid serialised uuid %s" % serialised)
        return uuid

    def serialise(self):
        if '_serialised' not in self.__dict__:
            self.__dict__['_serialised'], self.__dict__['_compacted_node'], self.__dict__['_compacted_time'], self.__dict__['_compacted_clock'] = self._serialise(self)
        return self.__dict__['_serialised']

    def compact_crush(self):
        if '_compacted_node' not in self.__dict__:
            self.serialise()
        node = self.__dict__['_compacted_node']
        if node is not None:
            time = self.__dict__['_compacted_time']
            clock = self.__dict__['_compacted_clock']
            time_low = time & 0xffffffff
            time_mid = (time >> 32) & 0xffff
            time_hi_version = (time >> 48) & 0xfff
            time_hi_version |= 0x1000
            clock_seq_low = clock & 0xff
            clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122
            cls = self.__class__
            return cls(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node))

    def encode(self, encoding='encoded'):
        return encode(self.serialise(), encoding)

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

    def iscompact(self):
        if '_compacted_node' not in self.__dict__:
            self.serialise()
        return self.__dict__['_compacted_node'] == self.node


# Compatibility with builtin uuid

def uuid(data=None, compacted=None):
    return UUID.new(data=data, compacted=compacted)


def uuid1(node=None, clock_seq=None):
    return UUID(_uuid.uuid1(node=node, clock_seq=clock_seq))


def uuid3(namespace, name):
    return UUID(_uuid.uuid3(namespace, name))


def uuid4():
    return UUID(_uuid.uuid4())


def uuid5(namespace, name):
    return UUID(_uuid.uuid5(namespace, name))


# The following standard UUIDs are for use with uuid3() or uuid5().

NAMESPACE_DNS = UUID('6ba7b810-9dad-11d1-80b4-00c04fd430c8')
NAMESPACE_URL = UUID('6ba7b811-9dad-11d1-80b4-00c04fd430c8')
NAMESPACE_OID = UUID('6ba7b812-9dad-11d1-80b4-00c04fd430c8')
NAMESPACE_X500 = UUID('6ba7b814-9dad-11d1-80b4-00c04fd430c8')


if __name__ == '__main__':
    errors = 0

    uuids = [
        # Full:
        ('5759b016-10c0-4526-a981-47d6d19f6fb4', ['5759b016-10c0-4526-a981-47d6d19f6fb4'], repr('5759b016-10c0-4526-a981-47d6d19f6fb4'), repr('\x01WY\xb0\x16\x10\xc0E&\xa9\x81G\xd6\xd1\x9fo\xb4')),
        ('e8b13d1b-665f-4f4c-aa83-76fa782b030a', ['e8b13d1b-665f-4f4c-aa83-76fa782b030a'], repr('e8b13d1b-665f-4f4c-aa83-76fa782b030a'), repr('\x01\xe8\xb1=\x1bf_OL\xaa\x83v\xfax+\x03\n')),
        # Condensed:
        ('00000000-0000-1000-8000-000000000000', ['00000000-0000-1000-8000-000000000000'], repr('00000000-0000-1000-8000-000000000000'), repr('\x1c\x00\x00\x00')),
        ('11111111-1111-1111-8111-111111111111', ['11111111-1111-1111-8111-111111111111'], repr('~yc9DnemYGNTMdKXsYYiTKOc'), repr('\x0f\x88\x88\x88\x88\x88\x88\x88\x82"""""""')),
        # Condensed + Compacted:
        ('230c0800-dc3c-11e7-b966-a3ab262e682b', ['230c0800-dc3c-11e7-b966-a3ab262e682b'], repr('~SsQq3dJdg3P'), repr('\x06,\x02[\b9fW')),
        ('f2238800-debf-11e7-bbf7-dffcee0c03ab', ['f2238800-debf-11e7-bbf7-dffcee0c03ab'], repr('~SUkSiXYTT8c'), repr('\x06.\x86*\x1f\xbb\xf7W')),
        # Condensed + Expanded:
        ('60579016-dec5-11e7-b616-34363bc9ddd6', ['60579016-dec5-11e7-b616-34363bc9ddd6'], repr('60579016-dec5-11e7-b616-34363bc9ddd6'), repr('\xe1\x17E\xcc)\xc4\x0bl,hlw\x93\xbb\xac')),
        ('4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', ['4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'], repr('4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'), repr('\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc4')),
        # Other:
        ('00000000-0000-0000-0000-000000000000', ['00000000-0000-0000-0000-000000000000'], repr('00000000-0000-0000-0000-000000000000'), repr('\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00')),
        ('00000000-0000-1000-8000-010000000000', ['00000000-0000-1000-8000-010000000000'], repr('~notmet'), repr('\x1c\x00\x00\x01')),
        ('11111111-1111-1111-8111-101111111111', ['11111111-1111-1111-8111-101111111111'], repr('11111111-1111-1111-8111-101111111111'), repr('\xf7\x95\xb0k\xa4\x86\x84\x88\x82" """""')),
        ('00000000-0000-1000-a000-000000000000', ['00000000-0000-1000-a000-000000000000'], repr('00000000-0000-1000-a000-000000000000'), repr('\n@\x00\x00\x00\x00\x00\x00\x00')),
        # Coumpound:
        ('5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a', ['5759b016-10c0-4526-a981-47d6d19f6fb4', 'e8b13d1b-665f-4f4c-aa83-76fa782b030a'], repr('5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a'), repr('\x01WY\xb0\x16\x10\xc0E&\xa9\x81G\xd6\xd1\x9fo\xb4\x01\xe8\xb1=\x1bf_OL\xaa\x83v\xfax+\x03\n')),
        ('00000000-0000-1000-8000-000000000000;11111111-1111-1111-8111-111111111111', ['00000000-0000-1000-8000-000000000000', '11111111-1111-1111-8111-111111111111'], repr('~WPQUDun7rkRr7TkQ2PSfCHGo4WWz'), repr('\x1c\x00\x00\x00\x0f\x88\x88\x88\x88\x88\x88\x88\x82"""""""')),
        ('230c0800-dc3c-11e7-b966-a3ab262e682b;f2238800-debf-11e7-bbf7-dffcee0c03ab', ['230c0800-dc3c-11e7-b966-a3ab262e682b', 'f2238800-debf-11e7-bbf7-dffcee0c03ab'], repr('~EYBuNUmS8MZs98Mq64McVQ'), repr('\x06,\x02[\b9fW\x06.\x86*\x1f\xbb\xf7W')),
        ('60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', ['60579016-dec5-11e7-b616-34363bc9ddd6', '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'], repr('60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'), repr('\xe1\x17E\xcc)\xc4\x0bl,hlw\x93\xbb\xac\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc4')),
        ('00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111', ['00000000-0000-1000-8000-010000000000', '11111111-1111-1111-8111-101111111111'], repr('00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111'), repr('\x1c\x00\x00\x01\xf7\x95\xb0k\xa4\x86\x84\x88\x82" """""')),
        # Compound + Encoded:
        ('~HahbAtfQHjMfBztms7OdcPNp34A8PQ3RA3KD2gYdzO55QK', ['5759b016-10c0-4526-a981-47d6d19f6fb4', 'e8b13d1b-665f-4f4c-aa83-76fa782b030a'], repr('5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a'), repr('\x01WY\xb0\x16\x10\xc0E&\xa9\x81G\xd6\xd1\x9fo\xb4\x01\xe8\xb1=\x1bf_OL\xaa\x83v\xfax+\x03\n')),
        ('~WPQUDun7rkRr7TkQ2PSfCHGo4WWz', ['00000000-0000-1000-8000-000000000000', '11111111-1111-1111-8111-111111111111'], repr('~WPQUDun7rkRr7TkQ2PSfCHGo4WWz'), repr('\x1c\x00\x00\x00\x0f\x88\x88\x88\x88\x88\x88\x88\x82"""""""')),
        ('~EYBuNUmS8MZs98Mq64McVQ', ['230c0800-dc3c-11e7-b966-a3ab262e682b', 'f2238800-debf-11e7-bbf7-dffcee0c03ab'], repr('~EYBuNUmS8MZs98Mq64McVQ'), repr('\x06,\x02[\b9fW\x06.\x86*\x1f\xbb\xf7W')),
        ('~bo9AghAbwcXA2PzQQmZUlSFts6SmBNlgK76ctOiw7f', ['60579016-dec5-11e7-b616-34363bc9ddd6', '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'], repr('60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'), repr('\xe1\x17E\xcc)\xc4\x0bl,hlw\x93\xbb\xac\x0e\x89\xb7\xc3b\xb6<w\xa1H\xd7St\xac\xc4')),
        ('~WPQUxPCuJZ3YBTcUmoAMhJFiMicb', ['00000000-0000-1000-8000-010000000000', '11111111-1111-1111-8111-101111111111'], repr('00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111'), repr('\x1c\x00\x00\x01\xf7\x95\xb0k\xa4\x86\x84\x88\x82" """""')),
    ]

    ################################################

    for i, (str_uuid, expected, expected_encoded, expected_serialised) in enumerate(uuids):
        try:
            serialised = decode(str_uuid)

            result = unserialise(serialised)
            if result != expected:
                errors += 1
                print("Error in unserialise:\n\tResult: %s\n\tExpected: %s" % (result, expected))

            result_encoded = repr(encode(serialised))
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
