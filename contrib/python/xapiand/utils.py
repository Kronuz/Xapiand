# -*- coding: utf-8 -*-
#
# Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
# Copyright (C) 2006,2007,2008,2009,2010,2011,2012 Olly Betts
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

import sys

if sys.version_info[0] > 2:
    long = int


def serialise_length(length):
    if length < 255:
        return chr(length)
    result = chr(0xff)
    length -= 255
    while True:
        b = length & 0x7f
        length >>= 7
        if not length:
            result += chr(b | 0x80)
            break
        result += chr(b)
    return result


def unserialise_length(data, check_remaining=False):
    if not data:
        raise ValueError("Bad encoded length: no data")
    length = ord(data[0])
    if length == 0xff:
        length = 0
        shift = 0
        for i, ch in enumerate(data[1:], 1):
            b = ord(ch)
            length |= (b & 0x7f) << shift
            shift += 7
            if b & 0x80:
                break
        else:
            raise ValueError("Bad encoded length: insufficient data")
        length += 255
        data = data[i + 1:]
    else:
        data = data[1:]
    if check_remaining and length > len(data):
        raise ValueError("Bad encoded length: length greater than data")
    return length, data


def serialise_string(s):
    return serialise_length(len(s)) + s


def unserialise_string(data):
    length, data = unserialise_length(data, True)
    return data[:length], data[length:]


def serialise_char(c):
    if len(c) != 1:
        raise ValueError("Serialisation error: Cannot serialise empty char")
    return c


def unserialise_char(data):
    if len(data) < 1:
        raise ValueError("Bad encoded length: insufficient data")
    return data[:1], data[1:]


def fnv1ah64(s):
    # calculate FNV-1a 64 bit hash
    fnv = 0xcbf29ce484222325
    for c in s:
        fnv = ((fnv ^ ord(c)) * 0x100000001b3) & 0xffffffffffffffff
    return fnv


def jump_consistent_hash(key, num_buckets):
    # Computes the bucket number for key in the range [0, num_buckets).
    # The algorithm used is the jump consistent hash by Lamping and Veach.
    # A Fast, Minimal Memory, Consistent Hash Algorithm
    # [http://arxiv.org/pdf/1406.2294v1.pdf]
    if num_buckets < 1:
        raise ValueError('num_buckets must be a positive number')
    if not isinstance(key, (int, long)):
        key = fnv1ah64(key)
    b, j = -1, 0
    while j < num_buckets:
        b = j
        key = (key * long(2862933555777941757) + 1) & 0xffffffffffffffff
        j = long(float(b + 1) * (float(1 << 31) / float((key >> 33) + 1)))
    # b cannot exceed the range of num_buckets, see while condition
    return int(b)
