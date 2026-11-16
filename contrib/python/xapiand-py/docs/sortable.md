# Python sortable codec

`xapiand/sortable.py` is the client's port of `Kronuz/sortable`, the codec Xapiand's server uses
for every number it sorts, ranges or looks up by term (`docs/mechanisms/value-encoding.md` in the
server repository). It provides `encode(value)` and `decode(data, kind)`, where `kind` is `int` or
`float`:

1. `encode` takes an `int` in [-2**63, 2**64) or a `float`; `bool` raises `TypeError` and an integer
   out of range raises `ValueError`. -0.0 encodes as 0 and every NaN as `ff`.
2. `decode` accepts exactly the bytes `encode` writes and raises `ValueError` for anything else, and
   for a value that is not an integer in range when `kind` is `int`. A `float` result rounds large
   integers to nearest even, as the C++ `to_double` does.

The port is written from the format's description and `sortable`'s MIT reference model alone; the
client's former GPL module was never opened.

## Identity with the server

The server's CTest `sortable_xlang` (`tests/sortable_xlang.py`) runs on every CI platform. It checks
the pinned library's `test/vectors.txt` and `test/reject.txt` by sha256, encodes every vector, compares
10^6 seeded values with the C++ `sortable_cli`, and compares decode's answers over every 0-2 byte
string, every 3-byte string starting with an escape tag, the reject witnesses and 10^5 mutations.

## Speed

`tests/bench_sortable.py SORTABLE_ROOT [PASSES]` times both directions over the vectors. On an Intel
Core i9-9980HK (macOS 15.8.1, Python 3.15.2, `quiet exclusive`, seven samples of three passes over
340,473 values), the median is 903,108 values/s to encode and 736,738 values/s to decode; the samples
range from 800,501 to 976,037 and from 719,237 to 776,982.
