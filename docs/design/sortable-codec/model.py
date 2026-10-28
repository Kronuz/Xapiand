"""Reference model of the exact memcmp-ordered number codec in docs/design/sortable-codec.md (D2-D5).

Executable spec, not production code: the C++ library must produce the same bytes (T6 compares them).
Usage: python3 -I model.py selftest | mutants | rules | vectors | encode (reads "i|u|d VALUE" lines)
"""

from __future__ import annotations

import math
import random
import struct
import sys
from fractions import Fraction

# Layout (D2): one tag per binary exponent for both signs. The tail is the fraction f of a positive
# value, and 1 - f of a negative one; a power of two has an empty tail.
ZERO, NAN = 0x80, 0xFF
LO, HI = -60, 63  # direct window: positive tags 0x82..0xFD cover E = LO..HI, negatives mirror it
ESC_LOW, ESC_HIGH = 0x81, 0xFE  # escapes: then a 2-byte field, complemented for negatives
E_MIN = -1074  # low escape field = E - E_MIN, for E < LO
INF_FIELD = 0xFFFF  # +inf is the high escape with this field
MAX_TAIL = 8  # 63 fraction bits at most: every uint64 and every double
POS_INF = bytes([ESC_HIGH, 0xFF, 0xFF])
NEG_INF = bytes([0xFF - ESC_HIGH, 0, 0])

Number = Fraction | float


def _tag(e: int) -> tuple[int, bytes]:
    """Positive tag byte and escape field for exponent e."""
    if LO <= e <= HI:
        return 0x82 + (e - LO), b""
    if e < LO:
        return ESC_LOW, (e - E_MIN).to_bytes(2, "big")
    return ESC_HIGH, (e - HI - 1).to_bytes(2, "big")


def _tail(f: Fraction) -> bytes:
    """f in [0, 1), dyadic: the fewest big-endian bytes holding f * 2^(8n); empty for 0."""
    if f == 0:
        return b""
    n = 0
    while (f * (1 << (8 * n))).denominator != 1:
        n += 1
    assert n <= MAX_TAIL, "more than 63 fraction bits"
    return int(f * (1 << (8 * n))).to_bytes(n, "big")


MUTANT: str | None = None  # an encoder mutant for `mutants`


def encode(value: Number) -> bytes:
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
    a = abs(x)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    if Fraction(2) ** e > a:
        e -= 1
    f = a / Fraction(2) ** e - 1  # a = 2^e (1 + f)
    if x > 0:
        tag, field = _tag(e)
        return bytes([tag]) + field + _tail(f)
    # a negative in (2^(u-1), 2^u) takes the tag of u with the tail 1 - f; -(2^u) takes it with no tail
    u = e if f == 0 or MUTANT == "neg_tag_not_shifted" else e + 1
    tag, field = _tag(u)
    cfield = field if MUTANT == "neg_field_not_complemented" else bytes(0xFF - c for c in field)
    if f == 0:
        tail = b""
    elif MUTANT == "neg_tail_not_reversed":
        tail = _tail(f)
    else:
        tail = _tail(1 - f)
    return bytes([0xFF - tag]) + cfield + tail


# D5's reject rules that the value check does not already imply; `rules` proves each is needed.
RULES = (
    "special_with_tail",  # zero or NaN followed by bytes
    "short_field",  # an escape tag without its 2-byte field
    "escape_exponent_range",  # an escape exponent that fits a direct tag or lies beyond 1023 (1024 for a negative)
    "inf_with_tail",  # +inf or -inf followed by bytes
    "tail_last_byte_zero",  # a tail whose last byte is 0
    "not_double_or_integer",  # a value that is neither a double nor an integer in [-2^63, 2^64)
)
# Rules the value check implies in the model (a tail of more than 63 fraction bits is neither a double
# nor an integer in range); the C++ decoder tests them first, to bound its reads, so it needs their witnesses.
IMPLIED = ("tail_too_long", "tail_64_bits")
UNASSIGNED = (b"", b"\x00", b"\x7f")  # empty input and the tags never written


def decode(b: bytes, disabled: frozenset[str] = frozenset()) -> Number | None:
    """Strict decode: None unless b is exactly what encode() returns for some value.

    `disabled` switches named rules off (used by `rules` to prove each is needed)."""

    def on(rule: str) -> bool:
        return rule not in disabled

    if not b or b[0] in (0x00, 0x7F):
        return None
    if b[0] in (ZERO, NAN):
        if len(b) > 1 and on("special_with_tail"):
            return None
        return Fraction(0) if b[0] == ZERO else math.nan
    neg = b[0] < ZERO
    t = 0xFF - b[0] if neg else b[0]
    i = 1
    if 0x82 <= t <= 0xFD:
        e = LO + t - 0x82
    else:  # t is ESC_LOW or ESC_HIGH
        if len(b) < 3 and on("short_field"):
            return None
        raw = int.from_bytes((b + b"\0\0")[1:3], "big")
        raw = 0xFFFF - raw if neg else raw
        i = 3
        if t == ESC_HIGH and raw == INF_FIELD:
            if len(b) > 3 and on("inf_with_tail"):
                return None
            return -math.inf if neg else math.inf
        e = raw + E_MIN if t == ESC_LOW else raw + HI + 1
        top = 1024 if neg else 1023  # a negative's exponent is the one above its magnitude's
        if ((t == ESC_LOW and e >= LO) or (t == ESC_HIGH and e > top)) and on("escape_exponent_range"):
            return None
    body = b[i:]
    if len(body) > MAX_TAIL and on("tail_too_long"):
        return None
    if body and body[-1] == 0 and on("tail_last_byte_zero"):
        return None
    if len(body) == MAX_TAIL and body[-1] & 1 and on("tail_64_bits"):
        return None
    f = Fraction(int.from_bytes(body, "big"), 1 << (8 * len(body))) if body else Fraction(0)
    if not neg:
        x = Fraction(2) ** e * (1 + f)
    else:
        x = Fraction(2) ** e if not body else Fraction(2) ** (e - 1) * (2 - f)
    if on("not_double_or_integer"):
        is_int = x.denominator == 1 and (x <= (1 << 63) if neg else x < (1 << 64))
        try:
            is_dbl = Fraction(float(x)) == x
        except OverflowError:
            is_dbl = False
        if not (is_int or is_dbl):
            return None
    return -x if neg else x


def reencode(x: Number) -> bytes:
    """The canonical bytes of a decoded value (an integer when it is one in range, else a double)."""
    if isinstance(x, float):
        return encode(x)
    if x.denominator == 1 and -(1 << 63) <= x < (1 << 64):
        return encode(int(x))
    return encode(float(x))


def _key(v: Number) -> tuple[int, Fraction]:
    if isinstance(v, float) and math.isnan(v):
        return (3, Fraction(0))
    if isinstance(v, float) and math.isinf(v):
        return (0 if v < 0 else 2, Fraction(0))
    return (1, Fraction(v))


def _values(n: int, seed: int = 1) -> list[Number]:
    rnd = random.Random(seed)
    vals: list[Number] = [
        0, 1, -1, 2**63 - 1, -(2**63), 2**64 - 1, math.inf, -math.inf, math.nan, -0.0,
        5e-324, -5e-324, 2.2250738585072014e-308, 1.7976931348623157e308, -1.7976931348623157e308,
    ]  # fmt: skip
    for k in range(64):
        for d in range(-3, 4):
            for sgn in (1, -1):
                v = sgn * ((1 << k) + d)
                if -(1 << 63) <= v < (1 << 64):
                    vals.append(v)
        vals.append(float(2**k))
        vals.append(1.0 + 2.0 ** -(k % 53))
    for e in range(-1074, 1024):  # each side of every tag and escape boundary, both signs
        p = math.ldexp(1.0, e)
        q, r = math.nextafter(p, math.inf), math.nextafter(p, 0.0)
        vals += [p, -p, q, -q, r, -r]
    for _ in range(n):
        r = rnd.random()
        if r < 0.33:
            vals.append(rnd.randrange(-(1 << 63), 1 << 64))
        elif r < 0.66:
            vals.append(struct.unpack("<d", rnd.getrandbits(64).to_bytes(8, "little"))[0])
        else:
            vals.append(rnd.randrange(-(1 << rnd.randrange(1, 64)), 1 << rnd.randrange(1, 64)))
    return vals


def check(n: int = 200000) -> tuple[int, int, int]:
    """Round trip, order and cross-type equality over _values(n): (values, failures, longest)."""
    vals = _values(n)
    bad = longest = 0
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
        longest = max(longest, len(enc))
        bad += not ok
    order = sorted(vals, key=_key)
    encs = [encode(v) for v in order]
    for a, b, va, vb in zip(encs, encs[1:], order, order[1:]):
        if _key(va) < _key(vb) and not a < b:
            bad += 1
        if _key(va) == _key(vb) and a != b:
            bad += 1
    return len(vals), bad, longest


def fuzz_decoder(n: int = 400000, seed: int = 2) -> tuple[int, int]:
    """Random and mutated inputs: (accepted, accepted that do not re-encode to themselves)."""
    rnd = random.Random(seed)
    seeds = [encode(v) for v in _values(2000)]
    acc = bad = 0
    for _ in range(n):
        if rnd.random() < 0.5:
            b = bytes(rnd.getrandbits(8) for _ in range(rnd.randrange(1, 13)))
        else:
            m = bytearray(rnd.choice(seeds))
            for _ in range(rnd.randrange(1, 3)):
                op = rnd.randrange(3)
                if op == 0 and m:
                    m[rnd.randrange(len(m))] = rnd.getrandbits(8)
                elif op == 1 and len(m) > 1:
                    del m[rnd.randrange(len(m))]
                else:
                    m.insert(rnd.randrange(len(m) + 1), rnd.getrandbits(8))
            b = bytes(m)
        x = decode(b)
        if x is not None:
            acc += 1
            bad += reencode(x) != b
    return acc, bad


def selftest() -> None:
    total, bad, longest = check()
    acc, nonc = fuzz_decoder()
    print(f"selftest: {total} values, {bad} failures, longest {longest} B; decoder fuzz: {acc} accepted, {nonc} not canonical")
    sys.exit(1 if bad or nonc or longest > 10 else 0)


def mutants() -> None:
    """Each encoder mutant must make `check` fail."""
    global MUTANT
    lost = 0
    for m in ("neg_tag_not_shifted", "neg_field_not_complemented", "neg_tail_not_reversed"):
        MUTANT = m
        try:
            bad = check(20000)[1]
        except AssertionError:
            bad = 1
        print(f"mutant {m}: {bad} failures")
        lost += bad == 0
    MUTANT = None
    sys.exit(1 if lost else 0)


def _witness(rule: str) -> bytes:
    one = encode(1.0)  # tag only: E = 0
    return {
        "special_with_tail": b"\x80\x01",
        "short_field": b"\x81\x00",
        "escape_exponent_range": bytes([ESC_LOW]) + (LO - E_MIN).to_bytes(2, "big"),  # E = LO fits a direct tag
        "inf_with_tail": POS_INF + b"\x01",
        "tail_last_byte_zero": one + b"\x80\x00",  # 1.5 with a spare zero byte
        "not_double_or_integer": one + b"\x00" * 7 + b"\x10",  # 1 + 2^-60
        "tail_too_long": one + b"\x80" + b"\x00" * 7 + b"\x01",  # 9 tail bytes
        "tail_64_bits": one + b"\x80" + b"\x00" * 6 + b"\x01",  # 8 tail bytes, lowest bit set
    }[rule]


def rules() -> None:
    """Each D5 rule's witness is rejected; with the rule off a rule in RULES accepts it as a
    non-canonical input (it does not re-encode to itself), and a rule in IMPLIED is still rejected."""
    bad = 0
    print("rule | witness | rejected | accepted without the rule | canonical then")
    for r in RULES + IMPLIED:
        w = _witness(r)
        rejected = decode(w) is None
        x = decode(w, frozenset({r}))
        accepted = x is not None
        canon = accepted and reencode(x) == w
        print(f"{r} | {w.hex()} | {rejected} | {accepted} | {canon}")
        bad += (not rejected) or canon or (accepted == (r in IMPLIED))
    for w in UNASSIGNED:
        rejected = decode(w) is None
        print(f"unassigned | {w.hex() or '(empty)'} | {rejected} | - | -")
        bad += not rejected
    sys.exit(1 if bad else 0)


def vectors(n: int = 100000) -> None:
    """The T6 fixture: one "kind value hex" line per value, boundary set then seeded random values."""
    rnd = random.Random(6)
    vals: list[Number] = [0, 1, -1, -0.0, math.inf, -math.inf, math.nan, 2**63 - 1, -(2**63), 2**64 - 1]
    for k in range(64):
        vals += [s * ((1 << k) + d) for s in (1, -1) for d in range(-3, 4) if -(1 << 63) <= s * ((1 << k) + d) < (1 << 64)]
    for e in range(-1074, 1024):
        p = math.ldexp(1.0, e)
        q, r = math.nextafter(p, math.inf), math.nextafter(p, 0.0)
        vals += [p, -p, q, r, -q, -r]
    for _ in range(n):
        if rnd.random() < 0.5:
            vals.append(rnd.randrange(-(1 << 63), 1 << 64))
        else:
            vals.append(struct.unpack("<d", rnd.getrandbits(64).to_bytes(8, "little"))[0])
    for v in vals:
        kind = "d" if isinstance(v, float) else ("u" if v >= 1 << 63 else "i")
        print(kind, repr(v) if kind == "d" else v, encode(v).hex())


if __name__ == "__main__":
    cmd = sys.argv[1:2]
    if cmd == ["selftest"]:
        selftest()
    elif cmd == ["mutants"]:
        mutants()
    elif cmd == ["rules"]:
        rules()
    elif cmd == ["vectors"]:
        vectors()
    elif cmd == ["encode"]:
        for line in sys.stdin:
            kind, text = line.split()
            v = float(text) if kind == "d" else int(text)
            print(kind, text, encode(v).hex())
    else:
        sys.exit(__doc__)
