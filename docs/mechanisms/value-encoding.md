> Status: CURRENT (2026-11-16) how Xapiand encodes the numbers it sorts, ranges and looks up by term; workstream sortable-encoding, branch `quest/xapiand/sortable-encoding`. Design: `docs/design/sortable-encoding.md`.

# Value encoding

Every number Xapiand sorts, ranges or looks up by term becomes bytes whose `memcmp` order is its exact numeric
order: field values (integer, positive, floating, date, datetime, time, timedelta), numeric document ids, versions,
accuracy-term values, sort keys and distances. The codec is [`Kronuz/sortable`](https://github.com/Kronuz/sortable)
(MIT, header-only, pinned in `CMakeLists.txt`); its own `docs/mechanisms/sortable.md` is the byte-level reference.

## The bytes

One number space covers `int64`, `uint64` and `double`: equal numbers encode to equal bytes whatever their type
(5, `5u` and 5.0 are `c040`), and order is numeric across types, -inf < negatives < 0 < positives < +inf < NaN.

| Value | Bytes |
|---|---|
| 0 and -0.0 | `80` |
| positive, binary exponent E in -60..63 (every integer, most doubles) | tag `0x82 + E + 60`, then the fraction's bytes, last byte nonzero |
| positive double, E < -60 or E >= 64 | tag `81` or `fe`, a 2-byte exponent field, the fraction |
| negative | the positive layout mirrored, so a larger magnitude sorts first |
| +inf, -inf, NaN | `feffff`, `010000`, `ff` |

A value takes 1 to 10 bytes; on Xapiand's real sets the mean is 4.25 B (`docs/design/sortable-encoding/sizes.txt`).
`0x00` and `0x7f` are never a first byte. The encoding is not prefix-free: the key maker frames concatenated keys.

## Where it is called

`src/serialise.h` holds the only calls. `Serialise::integer`, `positive`, `floating`, `timestamp` (seconds rounded to
the microsecond) and `version` call `sortable::encode`. Stored bytes decode with the trusted conversions
(`sortable::to_double`, `to_int64`, `to_uint64`), which keep the read bounds and skip the canonical checks;
`Unserialise::integer` and `positive` fall back to the strict `sortable::decode` and truncate and saturate a fraction
or an out-of-range value, since a slot shared by integer and floating fields holds both. Bytes that are not a number raise
`SerialisationError`. Ids use the same entry points (`Serialise::id`). A missing sort value keys as
`sortable::max_finite` or `min_finite`, after every finite number and before the infinity.

## Rules for user values

1. A NaN value or range bound answers `400`; -0.0 indexes as 0; an integer of 2^63 or more for an integer field answers
   `400`.
2. The floating path is `double` end to end: strings parse with `strict_stod`, JSON with rapidjson's full precision,
   so one decimal text gives its nearest double however it arrives (`9007199254740993.0000001` is 2^53 + 2).
3. A floating field keeps an integer's exact bytes and accuracy terms; a float's accuracy terms are its truncation,
   saturated to `int64` (`GenerateTerms::truncate`).
4. A range bound encodes in its own type (`range.cc`, `numeric_bound`), so a float bound on an integer field and an integer
   bound on a floating field compare exactly against the slot; its accuracy terms are the same truncation. A string bound
   parses as the field's type. An absent bound is -inf or +inf.

## The Python client

`contrib/python/xapiand-py/xapiand/sortable.py` is a port written from the format's description and sortable's MIT model:
`encode(int | float) -> bytes` and `decode(data, int | float)`. The CTest `sortable_xlang` checks it equals the C++ codec
byte for byte on every vector and on 10^6 seeded inputs, and that both decoders accept and reject the same bytes.

## Checks

| Check | What it shows |
|---|---|
| CTest `test_serialise` | every line of sortable's `vectors.txt` through `Serialise::`/`Unserialise::`: bytes, round trip, cross-type equality, memcmp order |
| CTest `sortable_property` | sortable's order proof over edge and random values |
| CTest `sortable_xlang` | the Python client equals the C++ codec |
| `tests/numeric_range_check.py BIN` | ranges, sorts and min/max equal brute force at the old failures |
| `tests/sort_range_identity.py OLD NEW ...` | every sort and range answer on the real corpus equals the earlier build's or brute force |
| `tests/bench_number_codec.cc` | ns per value of both directions on sortable's real-value set |

## Changing the bytes

There is no format marker: a build reads only the bytes it writes, and a build opening an index written with other bytes
reads wrong numbers without an error. Before the release such an index is re-created, or moved: dump every index with the
earlier build, start the new build on an empty data directory, and restore the dumps. The `.xapiand/indexes` registry is
written in the earlier bytes too, so the old data directory is not reused. Dumps hold typed MsgPack and cross the change.
