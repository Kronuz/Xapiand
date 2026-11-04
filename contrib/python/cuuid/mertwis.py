# -*- coding: utf-8 -*-
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# Copyright © 2015-2020 Dubalu LLC
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

# Mersenne Twister implementation

from _random import Random


class MT19937(object):
    def __init__(self, seed):
        mt = [0] * 624
        mt[0] = p = seed & 0xffffffff
        for mti in range(1, 624):
            mt[mti] = p = (1812433253 * (p ^ (p >> 30)) + mti) & 0xffffffff
        mt.append(624)
        self.random = Random()
        self.random.setstate(tuple(mt))

    def __call__(self):
        return self.random.getrandbits(32)
