"""Reference model of the exact memcmp-ordered number codec in docs/design/sortable-codec.md (D2-D5).

Executable spec, not production code: the C++ library must produce the same bytes (T6 compares them).
Usage: python3 -I model.py selftest | vectors | encode (reads "i|u|d VALUE" lines)
"""

from __future__ import annotations

import math
import random
import struct
import sys
from fractions import Fraction

ZERO, NAN = 0x80, 0xFF
ESC_LOW, ESC_HIGH = (
    0x81,
    0xFE,
)  # then a 2-byte field: 2 * (exponent - base) + has_fraction
DIRECT_MIN_E, DIRECT_MAX_E = (
    -4,
    57,
)  # tags 0x82..0xFD: 2 per exponent, power of two first
E_MIN, E_MAX = -1074, 1023
INF_FIELD = 0xFFFF  # +inf: the high escape with field 0xFFFF
MAX_FRAC_BYTES = 8  # 63 fraction bits: every uint64


def _tag(e: int, frac: bool) -> tuple[int, bytes]:
    """Positive tag and exponent field for exponent e."""
    if DIRECT_MIN_E <= e <= DIRECT_MAX_E:
        return 0x82 + 2 * (e - DIRECT_MIN_E) + frac, b""
    if e < DIRECT_MIN_E:
        return ESC_LOW, (2 * (e - E_MIN) + frac).to_bytes(2, "big")
    return ESC_HIGH, (2 * (e - DIRECT_MAX_E - 1) + frac).to_bytes(2, "big")


def _finite(m: int, q: int, neg: bool) -> bytes:
    """Encode (-1 if neg else 1) * m * 2**q, m odd and positive."""
    bl = m.bit_length()
    e, frac_len = bl - 1 + q, bl - 1
    tag, field = _tag(e, frac_len > 0)
    n = -(-frac_len // 8)
    big = (m - (1 << frac_len)) << (
        8 * n - frac_len
    )  # fraction left-aligned in n bytes
    if neg:
        tag = 0xFF - tag
        field = bytes(0xFF - c for c in field)
        if n:
            big = (1 << (8 * n)) - big  # 1 - f: reverses the order, keeps the length
    return bytes([tag]) + field + big.to_bytes(n, "big")


def _split(x: Fraction) -> tuple[int, int]:
    """x = m * 2**q with m odd (x > 0, dyadic)."""
    num, den = x.numerator, x.denominator
    assert den & (den - 1) == 0, "not dyadic"
    q = -(den.bit_length() - 1)
    while num % 2 == 0:
        num //= 2
        q += 1
    return num, q


POS_INF = bytes([ESC_HIGH]) + INF_FIELD.to_bytes(2, "big")
NEG_INF = bytes(0xFF - c for c in POS_INF)


def encode(value: float) -> bytes:
    if isinstance(value, float):
        if math.isnan(value):
            return bytes([NAN])
        if math.isinf(value):
            return POS_INF if value > 0 else NEG_INF
    else:
        assert -(1 << 63) <= value < (1 << 64)
    x = Fraction(value)
    if x == 0:
        return bytes([ZERO])  # -0.0 is canonicalised to 0 (D4)
    return _finite(*_split(abs(x)), x < 0)


def decode(b: bytes) -> Fraction | float | None:
    """Strict decode: None unless b is exactly what encode() returns for some value."""
    if b == bytes([ZERO]):
        return Fraction(0)
    if b == bytes([NAN]):
        return math.nan
    if b == POS_INF:
        return math.inf
    if b == NEG_INF:
        return -math.inf
    if not b or b[0] in (0x00, 0x7F, 0x80, 0xFF):  # 0x00 and 0x7F are never written
        return None
    neg = b[0] < ZERO
    tag = 0xFF - b[0] if neg else b[0]
    i = 1
    if 0x82 <= tag <= 0xFD:
        e, frac = DIRECT_MIN_E + (tag - 0x82) // 2, (tag - 0x82) % 2 == 1
    elif tag in (ESC_LOW, ESC_HIGH):
        if len(b) < 3:
            return None
        raw = int.from_bytes(b[1:3], "big")
        if neg:
            raw = 0xFFFF - raw
        i = 3
        frac = raw % 2 == 1
        e = raw // 2 + (E_MIN if tag == ESC_LOW else DIRECT_MAX_E + 1)
        if (tag == ESC_LOW and e >= DIRECT_MIN_E) or (tag == ESC_HIGH and e > E_MAX):
            return None
    else:
        return None
    body = b[i:]
    n = len(body)
    if not frac:
        if n:
            return None
        frac_len, f = 0, 0
    else:
        if not 1 <= n <= MAX_FRAC_BYTES:
            return None
        big = int.from_bytes(body, "big")
        if neg:
            big = (1 << (8 * n)) - big
            if not 0 < big < (1 << (8 * n)):
                return None
        if big & 0xFF == 0:
            return None
        frac_len, f = 8 * n, big
        while f & 1 == 0:
            f >>= 1
            frac_len -= 1
    m = (1 << frac_len) | f
    x = Fraction(m) * Fraction(2) ** (e - frac_len)
    low = e - frac_len  # exponent of the lowest set bit
    is_double = frac_len <= 52 and low >= E_MIN and e <= E_MAX
    is_int = low >= 0 and (x <= (1 << 63) if neg else x < (1 << 64))
    if not (is_double or is_int):
        return None
    return -x if neg else x


def _key(v: float) -> tuple[int, Fraction]:
    if isinstance(v, float) and math.isnan(v):
        return (3, Fraction(0))
    if isinstance(v, float) and math.isinf(v):
        return (0 if v < 0 else 2, Fraction(0))
    return (1, Fraction(v))


def selftest(n: int = 200000) -> None:
    rnd = random.Random(1)
    vals: list[int | float] = [
        0,
        1,
        -1,
        2**63 - 1,
        -(2**63),
        2**64 - 1,
        math.inf,
        -math.inf,
        math.nan,
        -0.0,
        5e-324,
        -5e-324,
        2.2250738585072014e-308,
        1.7976931348623157e308,
        -1.7976931348623157e308,
    ]
    for k in range(64):
        for d in range(-3, 4):
            for sgn in (1, -1):
                v = sgn * ((1 << k) + d)
                if -(1 << 63) <= v < (1 << 64):
                    vals.append(v)
        vals.append(float(2**k))
        vals.append(1.0 + 2.0 ** -(k % 53))
    for e in range(
        -1074, 1024
    ):  # every exponent: each side of every tag and escape boundary
        p = math.ldexp(1.0, e)
        vals += [
            p,
            -p,
            math.nextafter(p, math.inf),
            -math.nextafter(p, math.inf),
            math.nextafter(p, 0.0),
        ]
    for _ in range(n):
        r = rnd.random()
        if r < 0.33:
            vals.append(rnd.randrange(-(1 << 63), 1 << 64))
        elif r < 0.66:
            f = struct.unpack("<d", rnd.getrandbits(64).to_bytes(8, "little"))[0]
            vals.append(f)
        else:
            vals.append(
                rnd.randrange(-(1 << rnd.randrange(1, 64)), 1 << rnd.randrange(1, 64))
            )
    bad = 0
    for v in vals:
        enc = encode(v)
        dec = decode(enc)
        if isinstance(v, float) and math.isnan(v):
            ok = isinstance(dec, float) and math.isnan(dec)
        elif isinstance(v, float) and math.isinf(v):
            ok = dec == v
        else:
            ok = dec == Fraction(v)
        assert enc[0] != 0x00, v  # never the StringList magic
        bad += not ok
    order = sorted(vals, key=_key)
    encs = [encode(v) for v in order]
    for a, b, va, vb in zip(encs, encs[1:], order, order[1:]):
        if _key(va) < _key(vb) and not a < b:
            bad += 1
        if _key(va) == _key(vb) and a != b:
            bad += 1
    print(f"selftest: {len(vals)} values, {bad} failures")
    sys.exit(1 if bad else 0)


def vectors(n: int = 100000) -> None:
    """The T6 fixture: one "kind value hex" line per value, boundary set then seeded random values."""
    rnd = random.Random(6)
    vals: list[int | float] = [
        0,
        1,
        -1,
        -0.0,
        math.inf,
        -math.inf,
        math.nan,
        2**63 - 1,
        -(2**63),
        2**64 - 1,
    ]
    for k in range(64):
        vals += [
            s * ((1 << k) + d)
            for s in (1, -1)
            for d in range(-3, 4)
            if -(1 << 63) <= s * ((1 << k) + d) < (1 << 64)
        ]
    for e in range(-1074, 1024):
        p = math.ldexp(1.0, e)
        vals += [p, -p, math.nextafter(p, math.inf), math.nextafter(p, 0.0)]
    for _ in range(n):
        if rnd.random() < 0.5:
            vals.append(rnd.randrange(-(1 << 63), 1 << 64))
        else:
            vals.append(
                struct.unpack("<d", rnd.getrandbits(64).to_bytes(8, "little"))[0]
            )
    for v in vals:
        kind = "d" if isinstance(v, float) else ("u" if v >= 1 << 63 else "i")
        print(kind, repr(v) if kind == "d" else v, encode(v).hex())


if __name__ == "__main__":
    if sys.argv[1:2] == ["selftest"]:
        selftest()
    elif sys.argv[1:2] == ["vectors"]:
        vectors()
    elif sys.argv[1:2] == ["encode"]:
        for line in sys.stdin:
            kind, text = line.split()
            v = float(text) if kind == "d" else int(text)
            print(kind, text, encode(v).hex())
