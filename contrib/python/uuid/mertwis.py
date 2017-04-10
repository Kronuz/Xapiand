# -*- coding: utf-8 -*-
# Mersenne Twister implementation


class MT19937(object):
    def __init__(self, seed):
        self.index = 624
        self.state = [0] * 624
        self.state[0] = seed  # Initialize the state to the seed
        try:
            for i, p in enumerate(self.state, 1):
                v = int((1812433253 * (p ^ (p >> 30)) + i) & 0xffffffff)
                self.state[i] = v
        except IndexError:
            pass

    def twist(self):
        def gen():
            g = enumerate(self.state)
            s = g.next()
            yield s
            for q in g:
                yield q
            yield s
        g = gen()
        i, p = next(g)
        for j, q in g:
            y = (p & 0x80000000) | (q & 0x7fffffff)
            v = self.state[(i + 397) % 624] ^ (y >> 1)
            if y % 2:
                v ^= 0x9908b0df
            self.state[i] = v
            i, p = j, q
        self.index = 0

    def __call__(self):
        if self.index >= 624:
            self.twist()

        y = self.state[self.index]

        # Right shift by 11 bits
        y ^= y >> 11
        # Shift y left by 7 and take the bitwise and of 2636928640
        y ^= ((y << 7) & 2636928640)
        # Shift y left by 15 and take the bitwise and of y and 4022730752
        y ^= ((y << 15) & 4022730752)
        # Right shift by 18 bits
        y ^= y >> 18

        self.index += 1

        return int(y & 0xffffffff)
