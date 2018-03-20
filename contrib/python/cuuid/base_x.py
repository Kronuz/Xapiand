#
# Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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
"""
BaseX encoding
"""

__version__ = '0.0.1'


class BaseX(object):
    def __init__(self, alphabet, translate):
        self.alphabet = alphabet
        self.translate = translate

        self.base = len(self.alphabet)
        self.decoder = [self.base] * 256
        for i, a in enumerate(self.alphabet):
            o = ord(a)
            self.decoder[o] = i

        x = -1
        for a in self.translate:
            o = ord(a)
            i = self.decoder[o]
            if i < self.base:
                x = i
            else:
                self.decoder[o] = x

    def encode_int(self, i, default_one=True):
        """Encode an integer using BaseX"""
        if not i and default_one:
            return self.alphabet[0]

        string = ""
        sum_chk = 0
        while i:
            i, idx = divmod(i, self.base)
            string = self.alphabet[idx] + string
            sum_chk += idx

        sumsz = len(string)
        sum_chk += sumsz + sumsz / self.base

        return string, sum_chk % self.base

    def encode(self, v):
        """Encode a string using BaseX"""
        if not isinstance(v, bytes):
            raise TypeError("a bytes-like object is required, not '%s'" % type(v).__name__)

        p, acc = 1, 0
        for c in map(ord, reversed(v)):
            acc += p * c
            p = p << 8

        result, sum_chk = self.encode_int(acc, default_one=False)

        sum_chk = (self.base - (sum_chk % self.base)) % self.base
        return result + self.alphabet[sum_chk]

    def decode_int(self, v):
        """Decode a BaseX encoded string as an integer"""

        if not isinstance(v, str):
            v = v.decode('ascii')

        decimal = 0
        sum_chk = 0
        sumsz = 0
        for char in v:
            o = ord(char)
            i = self.decoder[o]
            if i < 0:
                continue
            if i >= self.base:
                raise ValueError("Invalid character")
            decimal = decimal * self.base + i
            sum_chk += i
            sumsz += 1

        sum_chk += sumsz + sumsz / self.base

        return decimal, sum_chk % self.base

    def decode(self, v):
        """Decode a BaseX encoded string"""

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

        acc, sum_chk = self.decode_int(v)

        sum_chk += chk
        if sum_chk % self.base:
            raise ValueError("Invalid checksum")

        result = []
        while acc:
            result.append(acc & 0xff)
            acc >>= 8

        return ''.join(map(chr, reversed(result)))

    def chksum(self, v):
        """Get checksum character for BaseX encoded string"""

        if not isinstance(v, str):
            v = v.decode('ascii')

        acc, sum_chk = self.decode_int(v)

        sum_chk = (self.base - (sum_chk % self.base)) % self.base
        return self.alphabet[sum_chk]


b59 = BaseX('zGLUAC2EwdDRrkWBatmscxyYlg6jhP7K53TibenZpMVuvoO9H4XSQq8FfJN', '~l1IO0')
b59decode = b59.decode
b59encode = b59.encode


def main():
    """BaseX encode or decode FILE, or standard input, to standard output."""

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
        (True, False): b59decode,
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
