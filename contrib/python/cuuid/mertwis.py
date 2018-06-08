# -*- coding: utf-8 -*-
# Mersenne Twister implementation

from _random import Random
import six


class MT19937(object):
    def __init__(self, seed):
        mt = [0] * 624
        mt[0] = p = seed & 0xffffffff
        for mti in six.moves.range(1, 624):
            mt[mti] = p = (1812433253 * (p ^ (p >> 30)) + mti) & 0xffffffff
        mt.append(624)
        self.random = Random()
        self.random.setstate(tuple(mt))

    def __call__(self):
        return self.random.getrandbits(32)
