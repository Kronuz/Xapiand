# Copyright (c) 2015-2019 Dubalu LLC
# Copyright (c) 2007,2009,2015,2016 Olly Betts
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
#

#  ____             _        _     _        ____            _       _ _
# / ___|  ___  _ __| |_ __ _| |__ | | ___  / ___|  ___ _ __(_) __ _| (_)___  ___
# \___ \ / _ \| '__| __/ _` | '_ \| |/ _ \ \___ \ / _ \ '__| |/ _` | | / __|/ _ \
#  ___) | (_) | |  | || (_| | |_) | |  __/  ___) |  __/ |  | | (_| | | \__ \  __/
# |____/ \___/|_|   \__\__,_|_.__/|_|\___| |____/ \___|_|  |_|\__,_|_|_|___/\___|
#
# Serialise floating point values to string which sort the same way.

import math

LDBL_MAX = 1.79769313486231571e+308
LDBL_MAX_EXP = 1024


def sortable_serialise(value):
    # Negative infinity.
    if value < -LDBL_MAX:
        return ''

    mantissa, exponent = math.frexp(value)

    # Deal with zero specially.
    #
    # IEEE representation of long doubles uses 15 bits for the exponent, with a
    # bias of 16383.  We bias this by subtracting 8, and non-IEEE
    # representations may allow higher exponents, so allow exponents down to
    # -32759 - if smaller exponents are possible anywhere, we underflow such
    #  numbers to 0.

    if mantissa == 0.0 or exponent < -(LDBL_MAX_EXP + LDBL_MAX_EXP - 1 - 8):
        return '\x80'

    negative = mantissa < 0
    if negative:
        mantissa = -mantissa

    # Infinity, or extremely large non-IEEE representation.
    if value > LDBL_MAX or exponent > LDBL_MAX_EXP + LDBL_MAX_EXP - 1 + 8:
        if negative:
            # This can only happen with a non-IEEE representation, because
            # we've already tested for value < -LDBL_MAX
            return ''
        return '\xff' * 17

    # Encoding:
    #
    # [ 7 | 6 | 5 | 4 3 2 1 0]
    #   Sm  Se  Le
    #
    # Sm stores the sign of the mantissa: 1 = positive or zero, 0 = negative.
    # Se stores the sign of the exponent: Sm for positive/zero, !Sm for neg.
    # Le stores the length of the exponent: !Se for 7 bits, Se for 15 bits.
    nxt = 0 if negative else 0xe0

    # Bias the exponent by 8 so that more small integers get short encodings.
    exponent -= 8
    exponent_negative = exponent < 0
    if exponent_negative:
        exponent = -exponent
        nxt ^= 0x60

    buf = ''

    # We store the exponent in 7 or 15 bits.  If the number is negative, we
    # flip all the bits of the exponent, since larger negative numbers should
    # sort first.
    #
    # If the exponent is negative, we flip the bits of the exponent, since
    # larger negative exponents should sort first (unless the number is
    # negative, in which case they should sort later).

    assert exponent >= 0
    if exponent < 128:
        nxt ^= 0x20
        # Put the top 5 bits of the exponent into the lower 5 bits of the
        # first byte:
        nxt |= (exponent >> 2) & 0xff
        if negative ^ exponent_negative:
            nxt ^= 0x1f
        buf += chr(nxt)

        # And the lower 2 bits of the exponent go into the upper 2 bits
        # of the second byte:
        nxt = (exponent << 6) & 0xff
        if negative ^ exponent_negative:
            nxt ^= 0xc0

    else:
        assert exponent >> 15 == 0
        # Put the top 5 bits of the exponent into the lower 5 bits of the
        # first byte:
        nxt |= (exponent >> 10) & 0xff
        if negative ^ exponent_negative:
            nxt ^= 0x1f
        buf += chr(nxt)

        # Put the bits 3-10 of the exponent into the second byte:
        nxt = (exponent >> 2) & 0xff
        if negative ^ exponent_negative:
            nxt ^= 0xff
        buf += chr(nxt)

        # And the lower 2 bits of the exponent go into the upper 2 bits
        # of the third byte:
        nxt = (exponent << 6) & 0xff
        if negative ^ exponent_negative:
            nxt ^= 0xc0

    # Convert the 112 (or 113) bits of the mantissa into two 32-bit words.

    mantissa *= 1073741824.0 if negative else 2147483648.0  # 1<<30 : 1<<31
    word1 = int(mantissa) & 0xffffffff
    mantissa -= word1

    mantissa *= 4294967296.0  # 1<<32
    word2 = int(mantissa) & 0xffffffff
    mantissa -= word2

    mantissa *= 4294967296.0  # 1<<32
    word3 = int(mantissa) & 0xffffffff
    mantissa -= word3

    mantissa *= 4294967296.0  # 1<<32
    word4 = int(mantissa) & 0xffffffff

    # If the number is positive, the first bit will always be set because 0.5
    # <= mantissa < 1, unless mantissa is zero, which we handle specially
    # above).  If the number is negative, we negate the mantissa instead of
    # flipping all the bits, so in the case of 0.5, the first bit isn't set
    # so we need to store it explicitly.  But for the cost of one extra
    # leading bit, we can save several trailing 0xff bytes in lots of common
    # cases.

    assert negative or (word1 & (1 << 30))
    if negative:
        # We negate the mantissa for negative numbers, so that the sort order
        # is reversed (since larger negative numbers should come first).
        word1 = -word1
        if word2 != 0 or word3 != 0 or word4 != 0:
            ++word1
        word2 = -word2
        if word3 != 0 or word4 != 0:
            ++word2
        word3 = -word3
        if word4 != 0:
            ++word3
        word4 = -word4

    word1 &= 0x3fffffff
    nxt |= (word1 >> 24) & 0xff
    buf += chr(nxt)

    if word1 != 0:
        buf += chr((word1 >> 16) & 0xff)
        buf += chr((word1 >> 8) & 0xff)
        buf += chr((word1) & 0xff)

    if word2 != 0 or word3 != 0 or word4 != 0:
        buf += chr((word2 >> 24) & 0xff)
        buf += chr((word2 >> 16) & 0xff)
        buf += chr((word2 >> 8) & 0xff)
        buf += chr((word2) & 0xff)

    if word3 != 0 or word4 != 0:
        buf += chr((word3 >> 24) & 0xff)
        buf += chr((word3 >> 16) & 0xff)
        buf += chr((word3 >> 8) & 0xff)
        buf += chr((word3) & 0xff)

    if word4 != 0:
        buf += chr((word4 >> 24) & 0xff)
        buf += chr((word4 >> 16) & 0xff)
        buf += chr((word4 >> 8) & 0xff)
        buf += chr((word4) & 0xff)

    # Finally, we can chop off any trailing zero bytes.
    buf = buf.rstrip('\x00')

    return buf


def numfromstr(value, pos):
    return ord(value[pos]) if pos < len(value) else 0


def sortable_unserialise(value):
    # Zero.
    if value == '\x80':
        return 0.0

    # Positive infinity.
    if value == "\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff":
        return LDBL_MAX + LDBL_MAX

    # Negative infinity.
    if not value:
        return -LDBL_MAX - LDBL_MAX

    first = numfromstr(value, 0)
    idx = 0

    first ^= (first & 0xc0) >> 1
    negative = (first & 0x80) == 0
    exponent_negative = (first & 0x40) != 0
    explen = (first & 0x20) == 0
    exponent = first & 0x1f
    if not explen:
        idx += 1
        first = numfromstr(value, idx)
        exponent <<= 2
        exponent |= (first >> 6)
        if negative ^ exponent_negative:
            exponent ^= 0x07f
    else:
        idx += 1
        first = numfromstr(value, idx)
        exponent <<= 8
        exponent |= first
        idx += 1
        first = numfromstr(value, idx)
        exponent <<= 2
        exponent |= (first >> 6)
        if negative ^ exponent_negative:
            exponent ^= 0x7fff

    word1 = (first & 0x3f) << 24
    idx += 1
    word1 |= numfromstr(value, idx) << 16
    idx += 1
    word1 |= numfromstr(value, idx) << 8
    idx += 1
    word1 |= numfromstr(value, idx)

    word2 = 0
    if idx < len(value):
        idx += 1
        word2 = numfromstr(value, idx) << 24
        idx += 1
        word2 |= numfromstr(value, idx) << 16
        idx += 1
        word2 |= numfromstr(value, idx) << 8
        idx += 1
        word2 |= numfromstr(value, idx)

    word3 = 0
    if idx < len(value):
        idx += 1
        word3 = numfromstr(value, idx) << 24
        idx += 1
        word3 |= numfromstr(value, idx) << 16
        idx += 1
        word3 |= numfromstr(value, idx) << 8
        idx += 1
        word3 |= numfromstr(value, idx)

    word4 = 0
    if idx < len(value):
        idx += 1
        word4 = numfromstr(value, idx) << 24
        idx += 1
        word4 |= numfromstr(value, idx) << 16
        idx += 1
        word4 |= numfromstr(value, idx) << 8
        idx += 1
        word4 |= numfromstr(value, idx)

    if negative:
        word1 = -word1
        if word2 != 0 or word3 != 0 or word4 != 0:
            word1 += 1
        word2 = -word2
        if word3 != 0 or word4 != 0:
            word2 += 1
        word3 = -word3
        if word4 != 0:
            word3 += 1
        word4 = -word4
        assert (word1 & 0xf0000000) != 0
        word1 &= 0x3fffffff

    if not negative:
        word1 |= 1 << 30

    mantissa = 0
    if word4 != 0:
        mantissa += word4 / 79228162514264337593543950336.0  # 1<<96
    if word3 != 0:
        mantissa += word3 / 18446744073709551616.0  # 1<<64
    if word2 != 0:
        mantissa += word2 / 4294967296.0  # 1<<32
    if word1 != 0:
        mantissa += word1
    mantissa /= 1073741824.0 if negative else 2147483648.0  # 1<<30 : 1<<31

    if exponent_negative:
        exponent = -exponent
    exponent += 8

    if negative:
        mantissa = -mantissa

    if exponent > LDBL_MAX_EXP:
        return LDBL_MAX + LDBL_MAX

    return math.ldexp(mantissa, exponent)
