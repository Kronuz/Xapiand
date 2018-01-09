'''Base59 encoding

Implementations of Base58 and Base58Check endcodings that are compatible
with the bitcoin network.
'''

# This module is based upon base58 snippets found scattered over many bitcoin
# tools written in python. From what I gather the original source is from a
# forum post by Gavin Andresen, so direct your praise to him.
# This module adds shiny packaging and support for python3.

__version__ = '0.2.5'

from hashlib import sha256

# 59 character alphabet used
alphabet = 'zy9MalDxwpKLdvW2AtmscgbYUq6jhP7E53TiXenZRkVCrouBH4GSQf8FNJO'


if bytes == str:  # python2
    iseq = lambda s: map(ord, s)
    bseq = lambda s: ''.join(map(chr, s))
    buffer = lambda s: s
else:  # python3
    iseq = lambda s: s
    bseq = bytes
    buffer = lambda s: s.buffer


def b59encode_int(i, default_one=True):
    '''Encode an integer using Base59'''
    if not i and default_one:
        return alphabet[0]
    string = ""
    sum_chk = 0
    while i:
        i, idx = divmod(i, 59)
        string = alphabet[idx] + string
        sum_chk += idx
    return string, sum_chk


def b59encode(v):
    '''Encode a string using Base59'''
    if not isinstance(v, bytes):
        raise TypeError("a bytes-like object is required, not '%s'" %
                        type(v).__name__)

    origlen = len(v)
    v = v.lstrip(b'\0')
    newlen = len(v)

    p, acc = 1, 0
    for c in iseq(reversed(v)):
        acc += p * c
        p = p << 8

    result, sum_chk = b59encode_int(acc, default_one=False)
    _result = (alphabet[0] * (origlen - newlen) + result)
    sz = len(result)
    sz = (sz + (sz / 59)) % 59
    sum_chk += sz
    sum_chk = (59 - (sum_chk % 59)) % 59
    return _result + alphabet[sum_chk]


def b59decode_int(v):
    '''Decode a Base59 encoded string as an integer'''

    if not isinstance(v, str):
        v = v.decode('ascii')

    decimal = 0
    sum_chk = 0
    sumsz = 0
    for char in v:
        decimal = decimal * 59 + alphabet.index(char)
        sum_chk += alphabet.index(char)
        sumsz += 1
    return decimal, sum_chk, sumsz


def b59decode(v):
    '''Decode a Base59 encoded string'''

    if not isinstance(v, str):
        v = v.decode('ascii')

    origlen = len(v) - 1
    v = v.lstrip(alphabet[0])
    newlen = len(v) - 1

    chk = v[-1:]
    v = v[:-1]

    acc, sum_chk, sumsz = b59decode_int(v)

    result = []
    while acc > 0:
        acc, mod = divmod(acc, 256)
        result.append(mod)

    sum_chk += alphabet.index(chk)
    sum_chk += (sumsz + sumsz / 59) % 59
    if (sum_chk % 59):
        raise ValueError("Invalid checksum")
    return (b'\0' * (origlen - newlen) + bseq(reversed(result)))


def main():
    '''Base59 encode or decode FILE, or standard input, to standard output.'''

    import sys
    import argparse

    stdout = buffer(sys.stdout)

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

    data = buffer(args.file).read().rstrip(b'\n')

    try:
        result = fun(data)
    except Exception as e:
        sys.exit(e)

    if not isinstance(result, bytes):
        result = result.encode('ascii')

    stdout.write(result)


if __name__ == '__main__':
    main()
