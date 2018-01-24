'''Base59 encoding

Implementations of Base59 and Base59Check endcodings that are compatible
with the bitcoin network.
'''

# This module is based upon base59 snippets found scattered over many bitcoin
# tools written in python. From what I gather the original source is from a
# forum post by Gavin Andresen, so direct your praise to him.
# This module adds shiny packaging and support for python3.

__version__ = '0.0.1'


class BaseX(object):
    def __init__(self, alphabet, translate):
        # 59 character alphabet used
        self.alphabet = alphabet
        self.translate = translate

        self.base = len(self.alphabet)
        self.decoder = [self.base] * 256
        for i, a in enumerate(self.alphabet):
            self.decoder[ord(a)] = i

        x = -1
        for a in self.translate:
            v = ord(a)
            i = self.decoder[v]
            if i <= self.base:
                x = i
            else:
                self.decoder[v] = x

    def encode_int(self, i, default_one=True):
        '''Encode an integer using Base59'''
        if not i and default_one:
            return self.alphabet[0]
        string = ""
        sum_chk = 0
        while i:
            i, idx = divmod(i, self.base)
            string = self.alphabet[idx] + string
            sum_chk += idx
        return string, sum_chk

    def encode(self, v):
        '''Encode a string using Base59'''
        if not isinstance(v, bytes):
            raise TypeError("a bytes-like object is required, not '%s'" %
                            type(v).__name__)

        p, acc = 1, 0
        for c in map(ord, reversed(v)):
            acc += p * c
            p = p << 8

        result, sum_chk = self.encode_int(acc, default_one=False)

        sz = len(result)
        sz = (sz + (sz / self.base)) % self.base
        sum_chk += sz
        sum_chk = (self.base - (sum_chk % self.base)) % self.base
        return result + self.alphabet[sum_chk]

    def decode_int(self, v):
        '''Decode a Base59 encoded string as an integer'''

        if not isinstance(v, str):
            v = v.decode('ascii')

        decimal = 0
        sum_chk = 0
        sumsz = 0
        for char in v:
            i = self.decoder[ord(char)]
            if i < 0:
                continue
            if i >= self.base:
                print 'char-> '
                print repr(char)
                raise ValueError("Invalid character")
            decimal = decimal * self.base + i
            sum_chk += i
            sumsz += 1
        return decimal, sum_chk, sumsz

    def decode(self, v):
        '''Decode a Base59 encoded string'''

        if not isinstance(v, str):
            v = v.decode('ascii')

        while True:
            chk = self.decoder[ord(v[-1:])]
            v = v[:-1]
            if chk < 0:
                continue
            if chk >= self.base:
                raise ValueError("Invalid character")
            break

        acc, sum_chk, sumsz = self.decode_int(v)

        sum_chk += chk
        sum_chk += (sumsz + sumsz / self.base) % self.base
        if (sum_chk % self.base):
            raise ValueError("Invalid checksum")

        result = []
        while acc:
            result.append(acc & 0xff)
            acc >>= 8

        return ''.join(map(chr, reversed(result)))


b59 = BaseX('zy9MalDxwpKLdvW2AtmscgbYUq6jhP7E53TiXenZRkVCrouBH4GSQf8FNJO', '~l1IO0')
b59decode = b59.decode
b59encode = b59.encode


def main():
    '''Base59 encode or decode FILE, or standard input, to standard output.'''

    import sys
    import argparse

    stdout = sys.stdout

    parser = argparse.ArgumentParser(description=main.__doc__)
    parser.add_argument(
        'file',
        metavar='FILE',
        nargs='?',
        type=argparse.FileType('r'),
        default='-')
    parser.add_argument(
        '-d', '--decode',
        action='store_true',
        help='decode data')
    parser.add_argument(
        '-c', '--check',
        action='store_true',
        help='append a checksum before encoding')

    args = parser.parse_args()
    fun = {
        (False, False): b59encode,
        (False, True): b59encode_check,
        (True, False): b59decode,
        (True, True): b59decode_check
    }[(args.decode, args.check)]

    data = args.file.read().rstrip(b'\n')

    try:
        result = fun(data)
    except Exception as e:
        sys.exit(e)

    if not isinstance(result, bytes):
        result = result.encode('ascii')

    stdout.write(result)


if __name__ == '__main__':
    main()
