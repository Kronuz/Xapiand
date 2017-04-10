# -*- coding: utf-8 -*-
from __future__ import absolute_import

import base64
import six
import string
import uuid
from struct import pack, unpack_from

try:
    from .mertwis import MT19937
except ValueError:
    from mertwis import MT19937


def _fnv_1a(num):
    # calculate FNV-1a hash
    fnv = 0xcbf29ce484222325
    while num:
        fnv ^= num & 0xff
        fnv *= 0x100000001b3
        fnv &= 0xffffffffffffffff
        num >>= 8
    return fnv


def _unserialise(bytes_):
    byte0 = ord(bytes_[0])
    compacted = (byte0 & 0x10) == 0x10
    version = (byte0 & 0x20) == 0x20
    meat = 0
    for s in reversed(bytes_):
        meat <<= 8
        meat |= ord(s)
    meat >>= 6
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
        time = (time + UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK
    clock_seq_low = clock & 0xff
    clock_seq_hi_variant = (clock >> 8) & 0x3f | 0x80  # Variant: RFC 4122
    time_low = time & 0xffffffff
    time_mid = (time >> 32) & 0xffff
    time_hi_version = (time >> 48) & 0xfff
    if version:
        time_hi_version |= 0x1000
    else:
        time_hi_version |= 0x4000
    return UUID(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node))


def _unserialise_full(bytes_):
    bytes_ = bytes_.ljust(17, chr(0x00))
    meat1, meat2 = unpack_from('<QQ', bytes_, 1)
    time = meat1 & UUID.TIME_MASK
    meat1 >>= UUID.TIME_BITS
    version = meat1 & UUID.VERSION_MASK
    meat1 >>= UUID.VERSION_BITS
    if time:
        time = (time + UUID.UUID_TIME_INITIAL) & UUID.TIME_MASK
    variant = meat2 & UUID.VARIANT_MASK
    meat2 >>= UUID.VARIANT_BITS
    node = meat2 & UUID.NODE_MASK
    meat2 >>= UUID.NODE_BITS
    clock = meat2 & UUID.CLOCK_MASK
    clock_seq_low = clock & 0xff
    clock_seq_hi_variant = clock >> 8 | variant << 6
    time_low = time & 0xffffffff
    time_mid = (time >> 32) & 0xffff
    time_hi_version = (time >> 48) & 0xfff | version << 12
    return UUID(fields=(time_low, time_mid, time_hi_version, clock_seq_hi_variant, clock_seq_low, node))


def unserialise(bytes_):
    bytes_length = len(bytes_)
    result = []
    offset = 0
    while offset != bytes_length:
        byte0 = ord(bytes_[offset])
        length = byte0 & 0x0f
        if length == 0:
            length = (byte0 & 0xf0) >> 4
            if length == 0 or len(bytes_[offset:]) < (length + 2):
                raise ValueError('Bad encoded uuid %s' % repr(bytes_))
            final_offset = offset + length + 2
            result.append(_unserialise_full(bytes_[offset:final_offset]))
            offset = final_offset
        elif len(bytes_[offset:]) < (length + 1):
            raise ValueError('Bad encoded uuid %s' % repr(bytes_))
        else:
            final_offset = offset + length + 1
            result.append(_unserialise(bytes_[offset:final_offset]))
            offset = final_offset
    return result


def unserialise_compound(bytes_):
    if isinstance(bytes_, six.string_types):
        if len(bytes_) > 2:
            if bytes_[0] == '{' and bytes_[-1] == '}':
                bytes_ = serialise_compound(bytes_)
    return unserialise(bytes_)


def _validated_serialized(bytes_):
    bytes_length = len(bytes_)
    if bytes_length < 2:
        ValueError('Invalid serialised UUID format: %r' % bytes_)
    offset = 0
    while offset != bytes_length:
        byte0 = ord(bytes_[offset])
        length = byte0 & 0x0f
        if length == 0:
            length = (byte0 & 0xf0) >> 4
            if length == 0 or len(bytes_[offset:]) < (length + 2):
                ValueError('Invalid serialised UUID format: %r' % bytes_)
            offset += length + 2
        elif len(bytes_[offset:]) < (length + 1):
            ValueError('Invalid serialised UUID format: %r' % bytes_)
        else:
            offset += length + 1
    return bytes_


def b64_encode(b64_):
    return base64.urlsafe_b64encode(b64_).strip(b'=')


def b64_decode(b64_):
    pad = b'=' * (-len(b64_) % 4)
    return base64.urlsafe_b64decode(b64_ + pad)


def _serialise_base64(b64_):
    if isinstance(b64_, six.text_type):
        b64_ = b64_.encode('ascii')
    bytes_ = b64_decode(b64_)
    return _validated_serialized(bytes_)


def serialise(uuids_):
    serialised = b''
    for uuid_ in uuids_:
        if len(uuid_) == UUID.UUID_LENGTH:
            if uuid_[8] != '-' or uuid_[13] != '-' or uuid_[18] != '-' or uuid_[23] != '-':
                serialised += _serialise_base64(uuid_)
            else:
                hex_uuid = uuid_[0:8] + uuid_[9:4] + uuid_[14:4] + uuid_[19:4] + uuid_[24:12]
                if all(c in string.hexdigits for c in hex_uuid):
                    u = UUID(uuid_)
                    serialised += u.serialise()
                else:
                    raise ValueError('Invalid UUID format %s' % uuid_)
        else:
            serialised += _serialise_base64(uuid_)
    return serialised


def serialise_compound(compound_uuid):
    if isinstance(compound_uuid, six.string_types):
        if len(compound_uuid) > 2:
            if compound_uuid[0] == '{' and compound_uuid[-1] == '}':
                payload = compound_uuid[1:-1]
                uuids_ = payload.split(';')
                bytes_ = serialise(uuids_)
                return bytes_
            elif len(compound_uuid) == UUID.UUID_LENGTH:
                bytes_ = serialise([compound_uuid])
                return bytes_
    elif isinstance(compound_uuid, (list, tuple)):
        bytes_ = serialise(compound_uuid)
        return bytes_
    raise ValueError('Invalid UUID format in: %s' % compound_uuid)


class UUID(six.binary_type, uuid.UUID):
    """
    Anonymous UUID is 00000000-0000-1000-8000-000000000000
    """
    UUID_TIME_INITIAL = 0x1e6bfffffffffff

    UUID_MIN_SERIALISED_LENGTH = 2
    UUID_MAX_SERIALISED_LENGTH = 17
    UUID_LENGTH = 36

    TIME_BITS = 60
    VERSION_BITS = 64 - TIME_BITS
    COMPACTED_BITS = 1
    SALT_BITS = 5
    CLOCK_BITS = 14
    NODE_BITS = 48
    PADDING_BITS = 64 - COMPACTED_BITS - SALT_BITS - CLOCK_BITS
    PADDING1_BITS = 64 - COMPACTED_BITS - NODE_BITS - CLOCK_BITS
    VARIANT_BITS = 2

    TIME_MASK = ((1 << TIME_BITS) - 1)
    SALT_MASK = ((1 << SALT_BITS) - 1)
    CLOCK_MASK = ((1 << CLOCK_BITS) - 1)
    NODE_MASK = ((1 << NODE_BITS) - 1)
    VERSION_MASK = ((1 << VERSION_BITS) - 1)
    VARIANT_MASK = ((1 << VARIANT_BITS) - 1)

    def __new__(self, *args, **kwargs):
        return six.binary_type.__new__(self, uuid.UUID(*args, **kwargs))

    @classmethod
    def _calculate_node(cls, time, clock, salt):
        seed = 0
        if time:
            seed ^= _fnv_1a(time)
        if clock:
            seed ^= _fnv_1a(clock)
        if salt:
            seed ^= _fnv_1a(salt)
        if not seed:
            return 0
        g = MT19937(seed & 0xffffffff)
        node = g()
        node <<= 32
        node |= g()
        node &= cls.NODE_MASK & ~cls.SALT_MASK
        node |= salt
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
            if len(data) < cls.UUID_MIN_SERIALISED_LENGTH or len(data) > cls.UUID_MAX_SERIALISED_LENGTH:
                raise ValueError('Serialise UUID can store [%d, %d] bytes, given %d' % (cls.UUID_MIN_SERIALISED_LENGTH, cls.UUID_MAX_SERIALISED_LENGTH, len(data)))
            num = 0
            for d in reversed(data):
                num <<= 8
                num |= ord(d)
            time_low = num & 0xffffffff
            num >>= 32
            time_mid = num & 0xffff
            num >>= 16
            time_hi_version = num & 0xfff
            num >>= 12
            clock_seq_hi_variant = num & 0x3f
            num >>= 6
            clock_seq_low = num & 0xff
            num >>= 8
            node = num & 0xffffffffffff

            if compacted or compacted is None and len(data) <= 9:
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
                time = (time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK
            clock = num.clock_seq
            salt = _fnv_1a(num.node) & cls.SALT_MASK
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
        if bytes_ is None or len(bytes_) < cls.UUID_MIN_SERIALISED_LENGTH or len(bytes_) > cls.UUID_MAX_SERIALISED_LENGTH:
            raise ValueError('Bad encoded uuid %s' % repr(bytes_))

        byte0 = ord(bytes_[0])
        length = byte0 & 0x0f
        if length == 0:
            length = (byte0 & 0xf0) >> 4
            if length == 0 or len(bytes_) != (length + 2):
                raise ValueError('Bad encoded uuid %s' % repr(bytes_))
            return _unserialise_full(bytes_)
        elif len(bytes_) != (length + 1):
            raise ValueError('Bad encoded uuid %s' % repr(bytes_))
        return _unserialise(bytes_)

    def _serialise(self, variant):
        cls = self.__class__
        time = self.time
        if time:
            time = (time - cls.UUID_TIME_INITIAL) & cls.TIME_MASK
        clock = self.clock_seq
        node = self.node
        version = ord(self.bytes[6]) >> 4
        if variant == 0x80 and (version == 1 or version == 4):
            salt = node & cls.SALT_MASK
            compacted_node = cls._calculate_node(time, clock, salt)
            compacted = node == compacted_node

            meat = time & cls.TIME_MASK
            meat <<= cls.CLOCK_BITS
            meat |= clock & cls.CLOCK_MASK
            if compacted:
                meat <<= cls.SALT_BITS
                meat |= salt & cls.SALT_MASK
            else:
                meat <<= cls.NODE_BITS
                meat |= node & cls.NODE_MASK
            meat <<= 6

            bytes_ = b''
            while meat:
                bytes_ += chr(meat & 0xff)
                meat >>= 8
            length = len(bytes_)
            if length == 0:
                bytes_ = chr((version & 0x01) << 5 | compacted << 4 | 1) + '\x00'
            elif length == 1:
                bytes_ = chr(ord(bytes_[0]) | (version & 0x01) << 5 | compacted << 4 | length) + '\x00'
            else:
                length -= 1
                bytes_ = chr(ord(bytes_[0]) | (version & 0x01) << 5 | compacted << 4 | length) + bytes_[1:]
            return bytes_
        else:
            meat1 = version & cls.VERSION_MASK
            meat1 <<= cls.TIME_BITS
            meat1 |= time & cls.TIME_MASK
            meat2 = clock & cls.CLOCK_MASK
            meat2 <<= cls.NODE_BITS
            meat2 |= node & cls.NODE_MASK
            meat2 <<= cls.VARIANT_BITS
            meat2 |= (variant & 0xc0) >> 6 & cls.VARIANT_MASK
            bytes_ = pack('<QQ', meat1, meat2)
            length = len(bytes_)
            for byte_ in reversed(bytes_):
                if byte_ == '\x00':
                    bytes_ = bytes_[:-1]
                    length -= 1
                else:
                    break
            if length == 0:
                bytes_ = chr(1 << 4) + '\x00\x00'
            elif length == 1:
                bytes_ = chr(1 << 4) + bytes_ + '\x00'
            else:
                length -= 1
                bytes_ = chr((length & 0x0f) << 4) + bytes_
            return bytes_

    def serialise(self):
        return self._serialise(ord(self.bytes[8]) & 0xc0)


if __name__ == '__main__':
    str_uuids = [
        '00000000-0000-0000-0000-000000000000',
        '00000000-0000-1000-8000-000000000000',
        '00000000-0000-1000-a000-000000000000',
        '00000000-0000-4000-b000-000000000000',
        '00000000-2000-1000-c000-000000000000',
        '00000000-2000-4000-c000-000000000000',
        '00000000-2000-2000-0000-000000000000',
        '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662',
        'b6e0e797-80fc-11e6-b58a-60f81dc76762',
        'd095e48f-c64f-4f08-91ec-888e6068dfe0',
        'c5c52a08-c3b4-11e6-9231-339cb51d7742',
        'c5c52a08-c3b4-51e6-7231-339cb51d7742',
    ]
    expected_serialised = [
        repr('\x10\x00\x00'),
        repr('1\x00'),
        repr('(\x00\x00\x00\x00\x00\x00\x00\b'),
        repr('\b\x00\x00\x00\x00\x00\x00\x00\f'),
        repr('\x80\x01\x00\x00\x00\x00`\x19\x1e\x03'),
        repr('\x80\x01\x00\x00\x00\x00`\x19N\x03'),
        repr('p\x01\x00\x00\x00\x00`\x19.'),
        repr('\xb8\x80\xde\xf3\xe8\x92\x9dR\x07'),
        repr('\xaf\xd8\xd9q\x07>\x98b\x8dy\x0en\xcb\x0f\xfc\xff'),
        repr('\x0f\xf87\x1a\x98#"{\x04I^\t\xfdd \xd2'),
        repr('\xb8\x88\x91\x12T\x8a\x8bi\x07'),
        repr('\xf0\t*\xc5\xc5\xb4\x03\x00P\t\xddu\xd4r\xce\xc4\xc8'),
    ]
    errors = 0
    i = 0
    for str_uuid in str_uuids:
        try:
            u = UUID(str_uuid)
            expected = expected_serialised[i]
            serialised = u.serialise()
            result = repr(serialised)
            if result != expected:
                errors += 1
                print 'Error in serialise: %s  Expected: %s Result: %s' % (str_uuid, expected, result)
            uns_u = UUID.unserialise(serialised)
            if uns_u != u:
                errors += 1
                print 'Error in unserialise: Expected: %s Result: %s' % (str_uuid, uns_u)
        except ValueError, e:
            errors += 1
            print e.message
        i += 1

    str_uuids = [
        '{00000000-0000-0000-0000-000000000000;00000000-0000-1000-8000-000000000000;00000000-0000-1000-a000-000000000000}',
        '{00000000-0000-4000-b000-000000000000;00000000-2000-1000-c000-000000000000;00000000-2000-4000-c000-000000000000}',
        '{00000000-2000-2000-0000-000000000000;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662;b6e0e797-80fc-11e6-b58a-60f81dc76762}',
        '{d095e48f-c64f-4f08-91ec-888e6068dfe0;c5c52a08-c3b4-11e6-9231-339cb51d7742;c5c52a08-c3b4-51e6-7231-339cb51d7742}',
    ]
    expected_serialised = [
        repr('\x10\x00\x001\x00(\x00\x00\x00\x00\x00\x00\x00\b'),
        repr('\b\x00\x00\x00\x00\x00\x00\x00\f\x80\x01\x00\x00\x00\x00`\x19\x1e\x03\x80\x01\x00\x00\x00\x00`\x19N\x03'),
        repr('p\x01\x00\x00\x00\x00`\x19.\xb8\x80\xde\xf3\xe8\x92\x9dR\x07\xaf\xd8\xd9q\x07>\x98b\x8dy\x0en\xcb\x0f\xfc\xff'),
        repr('\x0f\xf87\x1a\x98#"{\x04I^\t\xfdd \xd2\xb8\x88\x91\x12T\x8a\x8bi\x07\xf0\t*\xc5\xc5\xb4\x03\x00P\t\xddu\xd4r\xce\xc4\xc8'),
    ]
    i = 0
    for str_uuid in str_uuids:
        try:
            serialised = serialise_compound(str_uuid)
            expected = expected_serialised[i]
            result = repr(serialised)
            if result != expected:
                errors += 1
                print 'Error in serialise: %s  Expected: %s Result: %s' % (str_uuid, expected, result)
            uns_uuuids = unserialise(serialised)
            compound_uuid = '{'
            for uuid_ in uns_uuuids:
                compound_uuid += uuid_ + ';'
            compound_uuid = compound_uuid[:-1] + '}'
            if compound_uuid != str_uuid:
                errors += 1
                print 'Error in unserialise: Expected: %s Result: %s' % (str_uuid, compound_uuid)
        except ValueError, e:
            errors += 1
            print e.message
        i += 1
    if errors == 0:
        print 'Pass all tests'
    else:
        print 'Finish with ', errors, ' errors'
