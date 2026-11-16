"""Exact, ordered number bytes, matching Kronuz/sortable's MIT reference.

Integers span [-2**63, 2**64); bool is excluded. Zero and NaN are canonical.
Invalid bytes and inexact integer conversions raise ValueError.
"""

from __future__ import annotations

import math
from typing import overload


def encode(value: float) -> bytes:
    """Encode an integer or binary64 float in numeric lexicographic order."""
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise TypeError("expected int or float, excluding bool")
    if isinstance(value, float):
        if math.isnan(value):
            return b"\xff"
        if math.isinf(value):
            return b"\x01\0\0" if value < 0 else b"\xfe\xff\xff"
        numerator, denominator = abs(value).as_integer_ratio()
    else:
        if not -(1 << 63) <= value < 1 << 64:
            raise ValueError("integer outside [-2**63, 2**64)")
        numerator, denominator = abs(value), 1
    if not numerator:
        return b"\x80"
    exponent = numerator.bit_length() - denominator.bit_length()
    shift = 65 - numerator.bit_length()
    fraction = (numerator << shift if shift >= 0 else numerator >> -shift) & (
        (1 << 64) - 1
    )
    negative = value < 0
    if negative and fraction:
        exponent += 1
        fraction = (1 << 64) - fraction
    if -60 <= exponent <= 63:
        tag, field = 0x82 + exponent + 60, b""
    else:
        tag = 0x81 if exponent < -60 else 0xFE
        raw = exponent + 1074 if exponent < -60 else exponent - 64
        field = (0xFFFF - raw if negative else raw).to_bytes(2, "big")
    tail = fraction.to_bytes(8, "big").rstrip(b"\0")
    return bytes([0xFF - tag if negative else tag]) + field + tail


@overload
def decode(data: bytes, kind: type[int]) -> int: ...


@overload
def decode(data: bytes, kind: type[float]) -> float: ...


def decode(data: bytes, kind: type[int | float]) -> int | float:
    """Decode canonical bytes; float conversion rounds large integers to nearest even."""
    if kind is not int and kind is not float:
        raise TypeError("kind must be int or float")
    if not isinstance(data, bytes):
        raise TypeError("data must be bytes")
    if not data or data[0] in (0, 0x7F):
        raise ValueError("invalid tag")
    tag = data[0]
    if tag in (0x80, 0xFF):
        if len(data) != 1:
            raise ValueError("special value has a tail")
        if tag == 0x80:
            return 0 if kind is int else 0.0
        if kind is int:
            raise ValueError("NaN is not an integer")
        return math.nan
    negative = tag < 0x80
    tag = 0xFF - tag if negative else tag
    offset = 1
    if 0x82 <= tag <= 0xFD:
        exponent = tag - 0x82 - 60
    else:
        if len(data) < 3:
            raise ValueError("truncated exponent")
        raw = int.from_bytes(data[1:3], "big")
        raw = 0xFFFF - raw if negative else raw
        offset = 3
        if tag == 0xFE and raw == 0xFFFF:
            if len(data) != 3 or kind is int:
                raise ValueError("invalid infinity or integer conversion")
            return -math.inf if negative else math.inf
        exponent = raw - 1074 if tag == 0x81 else raw + 64
        if (tag == 0x81 and exponent >= -60) or exponent > (1024 if negative else 1023):
            raise ValueError("noncanonical exponent")
    tail = data[offset:]
    if len(tail) > 8 or (tail and tail[-1] == 0) or (len(tail) == 8 and tail[-1] & 1):
        raise ValueError("noncanonical fraction")
    fraction = int.from_bytes(tail, "big") << (64 - 8 * len(tail))
    if negative and fraction:
        exponent -= 1
        fraction = (1 << 64) - fraction
    fraction_bits = (
        0 if not fraction else 64 - ((fraction & -fraction).bit_length() - 1)
    )
    is_double = -1074 <= exponent <= 1023 and fraction_bits <= (
        52 if exponent >= -1022 else exponent + 1074
    )
    is_integer = (
        exponent >= 0
        and fraction_bits <= exponent
        and (
            exponent <= (62 if negative else 63)
            or (negative and exponent == 63 and fraction == 0)
        )
    )
    if not (is_double or is_integer):
        raise ValueError("value is neither binary64 nor an integer in range")
    significand = (1 << 63) | (fraction >> 1)
    if kind is int:
        if not is_integer:
            raise ValueError("value is not an integer in range")
        magnitude = significand >> (63 - exponent)
        return -magnitude if negative else magnitude
    result = math.ldexp(float(significand), exponent - 63)
    return -result if negative else result
