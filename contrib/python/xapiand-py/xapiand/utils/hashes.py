# Copyright (c) 2015-2019 Dubalu LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#

#  _   _           _
# | | | | __ _ ___| |__   ___  ___
# | |_| |/ _` / __| '_ \ / _ \/ __|
# |  _  | (_| \__ \ | | |  __/\__ \
# |_| |_|\__,_|___/_| |_|\___||___/
#

from ..compat import integer_types, integer_type


def jump_consistent_hash(key, num_buckets):
    # Computes the bucket number for key in the range [0, num_buckets).
    # The algorithm used is the jump consistent hash by Lamping and Veach.
    # A Fast, Minimal Memory, Consistent Hash Algorithm
    # [http://arxiv.org/pdf/1406.2294v1.pdf]
    if num_buckets < 1:
        raise ValueError("num_buckets must be a positive number")
    if not isinstance(key, integer_types):
        # calculates FNV-1a 64 bit hash (skipping slashes)
        h = 14695981039346656037
        for c in key:
            if c != '/':
                h = ((h ^ ord(c)) * 0x100000001b3) & 0xffffffffffffffff
        key = h
    b, j = -1, 0
    while j < num_buckets:
        b = j
        key = (key * 2862933555777941757 + 1) & 0xffffffffffffffff
        j = integer_type(float(b + 1) * (float(1 << 31) / float((key >> 33) + 1)))
    # b cannot exceed the range of num_buckets, see while condition
    return b
