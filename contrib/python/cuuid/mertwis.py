# -*- coding: utf-8 -*-
# Mersenne Twister implementation

from _random import Random


class MT19937(object):
    def __init__(self, seed):
        try:
            mt = [0] * 624
            mt[0] = seed
            for i, p in enumerate(mt, 1):
                v = (1812433253 * (p ^ (p >> 30)) + i) & 0xffffffff
                mt[i] = v
        except IndexError:
            pass
        mt.append(624)
        self.random = Random()
        self.random.setstate(tuple(mt))

    def __call__(self):
        return self.random.getrandbits(32)
