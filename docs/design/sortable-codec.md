> Status: DRAFT (2026-10-27) revision 2, designing (ready for preliminary review). Workstream sortable-codec, branch `design/sortable-codec` from Xapiand:6c5c1a40d. Owner ruling 2026-10-27: replace the value encoding now with our own exact MIT codec. Revision 2 folds in the architect's REJECT (`docs/reviews/sortable-codec-design.md`, L1-L6, C1-C5); the table at the end maps each finding to its decision.

# An exact, memcmp-ordered number codec

Xapiand stores every number it sorts or ranges on (values, accuracy terms, numeric ids, versions) through
`sortable_serialise`, a GPL file of Xapian origin. It is lossy: 2147483649 encodes to bytes that decode as
2164260864 and sort after 2147483650, and -(2^a) is a byte prefix of -(2^a)-1 for a >= 30, so the two sort
the wrong way round (`docs/dev/value-compare.md` section 1, Xapiand:eb83813a8; wrong since Xapiand:cdd00e367,
2016). This design replaces it with one exact codec in a new MIT library, switches Xapiand to it, makes
a node refuse an index written in the old bytes, and upgrades such an index in place. It stops at the first
buildable steps.

Clean room: no file under `src/xapian/`, neither `src/sortable_serialise.{cc,h}` nor
`src/server/remote_protocol_views.cc` was opened, nor the ARCHITECTURE.md paragraphs that describe the old
algorithm (from `ARCHITECTURE.md:389` and `:651`; a grep showed their first lines). One grep for protocol
versions printed two constant lines of `remote_protocol_views.cc` (`:640-641`), nothing else of it. The old
codec was compiled from its files and called as a black box (`sortable-codec/legacy_probe.cc`) to measure its sizes and document compatibility. The format below
comes from public techniques: sign-magnitude with a biased exponent as in IEEE 754, complemented negatives
as in FoundationDB's tuple layer and CockroachDB's key encoding, and Lucene's fixed-width maps
(`NumericUtils`) for the fixed-width alternative.

Supporting files in `docs/design/sortable-codec/`: `model.py` (the executable reference model, the
format's oracle), `sizes.py` (the size row), `overloads.cc` (the syntax-only check of D7's API), `legacy_probe.cc` (the
black-box probe). The architect's probes (`.scratch/sortable-codec-arch/`, untracked) hold the layout
that revision 2 adopts (`p4_alt.py`) and the broader corpora (`p3_sizes.py`).

## Where the numbers go today

`sortable_serialise` has 28 direct calls in 10 files outside `src/xapian/` (grep at Xapiand:6c5c1a40d):
`src/serialise.{h,cc}`, `src/database/{utils.h,shard.cc,handler.cc,schemas_lru.cc}`,
`src/index_resolver_lru.cc`, `src/engine/xapian/{shard.cc,convert.cc}`, `src/server/search_views.cc`.
Everything else reaches it through `Serialise::`/`Unserialise::` (`generate_terms.cc` 40 calls,
`keymaker.cc` 34, `serialise.cc` 24, `query_dsl.cc` 14, `range.cc` 14, `schema.cc` 14, `keymaker.h` 11,
`bucket.cc` 3, `handler.cc` 2).

| Use | Where | Type today |
|---|---|---|
| Field values: integer, positive, floating | `serialise.h:169-186`, read back `:289-316` with saturation (`clamp_int64`, `:295`) | `int64`, `uint64`, `long double` |
| date and datetime (seconds, rounded to microseconds) | `serialise.h:138-140`, `serialise.cc:385-387` | `double` |
| time and timedelta (seconds) | `serialise.cc:530-538`, `:1142-1173` | `double` |
| Accuracy terms (floats truncated to `int64` first) | `generate_terms.cc:226, 238, 353, 369`, `schema.cc:5356` | `int64`, `uint64` |
| Datetime accuracy terms | `generate_terms.cc:253-293, 388-444` | `double` |
| Geo accuracy terms (trixel prefixes) | `generate_terms.cc:494`, queried at `:203` | `uint64` |
| Sort keys (distances re-encoded) | `keymaker.cc:146-414` | per field type |
| Aggregations | `aggregations/metrics.h:158-188` | per field type |
| Numeric document ids (`QN` term and `DB_SLOT_ID`) | `shard.cc:112-115, 1540`, `handler.cc:1631`; literal `"QN\x80"` at `handler.cc:419, 426, 2131`, `schema.cc:2987, 5293` | `double` |
| Document versions (`DB_SLOT_VERSION`) | `handler.cc:331, 363, 1707`, `shard.cc:98`, `engine/xapian/shard.cc:56, 64, 88`, `convert.cc:314`, `search_views.cc:1871, 2021`, `schemas_lru.cc:123`, `index_resolver_lru.cc:413` | `uint64`, or `double` (`engine/xapian/shard.cc:56, 64`) |
| `get_hashed` | `database/utils.h:116-118`, no caller | `uint64` |

Geo values (`Serialise::ranges`, `trixel_id`, `serialise.cc:774-795`) use their own fixed big-endian
encoding and do not change.

### Types do meet in one order

1. **One slot holds all three numeric types.** The global numeric slot `DB_SLOT_NUMERIC`
   (`database/utils.h:50`) is the slot of the floating, integer and positive global specifications
   (`schema.cc:2294-2302`), so a range over it compares integers with floats.
2. **A field's numeric types share its slot and terms.** Slots are `xxh32(prefix, ctype)`
   (`database/utils.cc:59-67`) and integer, positive and floating share ctype `N` (`schema.h:419-422`), so a
   search over two indexes where `price` is integer in one and floating in the other sorts and ranges one
   slot. date and datetime share `D`, time and timedelta share `T` (`schema.h:427-433`).
3. **A floating field stores integers.** `Serialise::integer(FieldType::floating, v)` encodes `v` as a float
   (`serialise.cc:416-417`), and `positive` likewise (`:433-434`).
4. **Query bounds cross types.** An integer field's range casts a float bound with `Cast::integer`
   (`range.cc:71-72`), which truncates toward zero (`cast.cc:162-163`): `_from: 1.5` becomes 1 and matches 1,
   and a bound beyond the `int64` range is undefined behavior. By reading; unverified live (check: index 1
   and 2 in an integer field, query `_from: 1.5`, expect only 2).
5. **Not across kinds.** Keyword, text, uuid, boolean and geo have their own ctypes and encodings, so no
   number is compared with a string. `_id` numeric and keyword ids use distinct terms (`QN` against `Q`
   with the keyword prefix).

Also found while reading, both pre-existing: a positive value of 2^63 or more given to an integer field
wraps negative (`serialise.cc:439-440` passes a `uint64_t` to `integer(std::int64_t)`), and the old codec
encodes -inf as the empty string (measured with the probe), which a value slot reads as "no value".

## Decisions

**D1. One exact number space.** The codec encodes `int64`, `uint64` and binary64 `double` into one total
order: -inf < negative numbers < 0 < positive numbers < +inf < NaN, numeric order within. Equal numbers
of different types encode to equal bytes (5, `5u` and 5.0 are one encoding), so the cases above compare
exactly with no conversion. date, datetime, time and timedelta keep their representation (`double`
seconds) and use the same codec. Every number Xapiand stores goes through it, including versions, numeric
ids and geo trixel terms; there is no second numeric encoding. Rejected: one fixed-width map per type
(prospect's `ordered.hh`, Lucene's `NumericUtils`), because those orders do not compare across types and
Xapiand's slots mix them; converting every number to `double`, because integers above 2^53 then collide.

**D2. The format: one tag per binary exponent.** A finite nonzero value is |v| = 2^E x (1 + f), with E its
binary exponent and f a fraction of L bits, L <= 63 (every `uint64` and every `double` fits; a subnormal
double normalises the same way). The first byte is a tag; the bytes after it are the escape field (only
under an escape tag) and then the tail:

| First byte | Meaning | Then |
|---|---|---|
| `0x00` | never written: StringList's multi-value magic (`serialise_list.h:33`) | |
| `0x01` | negative with U >= 64, or -inf | field `0xFFFF - (U - 64)`; tail |
| `0x02`-`0x7D` | negative with U = 63 down to -60: tag `0x7D - (U + 60)` | tail |
| `0x7E` | negative with U < -60 | field `0xFFFF - (U + 1074)`; tail |
| `0x7F` | never written | |
| `0x80` | zero | |
| `0x81` | positive with E < -60 | field `E + 1074`, big-endian; tail |
| `0x82`-`0xFD` | positive with E = -60 to 63: tag `0x82 + (E + 60)` | tail |
| `0xFE` | positive with E >= 64: field `E - 64`; +inf is field `0xFFFF` | tail |
| `0xFF` | NaN | |

A positive value takes the tag of its exponent E. Its tail is the fraction: n = ceil(L / 8) bytes holding
f x 2^(8n) big-endian, the last byte nonzero, and empty when f = 0 (an exact power of two). A negative value
takes the tag of U, the exponent of the smallest power of two at or above its magnitude, mirrored (`0xFF`
minus the positive tag): a magnitude 2^(U-1) (1 + f) in (2^(U-1), 2^U) has the tail of 1 - f, the n bytes of 2^(8n) - F
where F is the positive fraction's n bytes, and -(2^U) has an empty tail. Neither sign has a "has fraction"
bit, so 124 direct tags cover E = -60 to 63: every `int64` and `uint64`, and every `double` down to 2^-60,
takes no escape field. The longest encoding is 10 bytes (a `double` outside the window: tag, field, 7 tail
bytes); an integer takes at most 9.

Examples (from `model.py`): 0 `80`; 1 `be`; -1 `41`; 3 `bf80`; 1.5 `be80`; 2147483649 `dd00000002`; -1073741824 `23`; -1073741825 `22fffffffc`; 1761523200 `dca3fad8`; 0.01 `b747ae147ae147b0`; 0.0005 `b30624dd2f1a9fc0`; 2^60 `fa`; 2^64-1 `fdfffffffffffffffe`; -2^63 `02`; 1e-30 `8103ce4484bfeebc2a`; DBL_MAX `fe03bffffffffffffff0`; -DBL_MAX `01fc3f00000000000010`; +inf `feffff`; -inf `010000`; NaN `ff`.

Within a tag, a positive value's order is its tail's order, and a shorter tail is the longer one with zeros
after it, so `memcmp` (a prefix sorts first) is numeric order; the empty tail, 2^E itself, sorts first.
Across tags the exponent decides. A larger escape exponent has a larger field, so it sorts above a smaller
one, and the escapes bound the direct tags on both sides (0x81 below 0x82, 0xFE above 0xFD).

**D3. Negative values reverse the order without a terminator.** The tag mirrors (`0xFF - t`), the escape
field is complemented (`0xFFFF - field`) and the tail is 1 - f. 1 - f keeps the length n and its last byte
nonzero (the lowest set bit of F is the lowest set bit of 2^(8n) - F), so the same prefix rule orders it,
now reversed: a larger magnitude has a smaller 1 - f. A tag U holds the magnitudes in (2^(U-1), 2^U], so
-(2^U), the largest magnitude of the tag and the most negative value in it, is the empty tail, which sorts
first, and the next tag down (U + 1) starts at magnitudes above 2^U. This is what the old codec gets wrong:
it complements a trailing-zero-stripped encoding, so a shorter negative encoding is a prefix of a longer one
and sorts first. The encoding is not prefix-free (a value can be a prefix of another, as with positive
tails); a caller concatenating several encodings frames them, as Xapiand's key maker already does
(`keymaker.cc:760-783`: forward keys escape `0x00` as `00 ff` and end with `00 00`, reverse keys complement
every byte and end with `ff ff`).

**D4. Special values.** -0.0 encodes as zero, so -0.0 == 0.0 == 0, as the old codec does (both give `80`,
measured) and as prospect's map does (`ordered.hh:37-43`); its sign does not round-trip (Q2). NaN encodes as
the single byte `0xFF`, greater than everything, payload and sign dropped, so the codec is total and never
fails on input (the old codec asserts on NaN in this build, measured). Whether Xapiand indexes a NaN is
policy above the codec (Q1). +inf and -inf encode as above; -inf is no longer the empty string.

**D5. The decoder is strict and safe on arbitrary bytes.** `decode(bytes)` reads only within the given
length, allocates nothing, never throws, and accepts exactly the outputs of `encode`. It rejects:

1. The empty input, and the tags never written (`0x00`, `0x7F`).
2. Zero or NaN followed by any byte.
3. An escape tag without its 2-byte field.
4. An escape exponent that fits a direct tag, or beyond 1023 (1024 for a negative, whose exponent is one
   above its magnitude's).
5. +inf or -inf followed by any byte (field `0xFFFF` with a tail).
6. A tail of more than 8 bytes.
7. A tail whose last byte is 0.
8. A tail of 8 bytes with its lowest bit set (64 fraction bits).
9. A value that is neither a `double` nor an integer in [-2^63, 2^64).

`model.py rules` gives a witness input for each (rule 1 has three: the empty input and the two
unassigned tags) and shows that rules 2, 3, 4, 5, 7 and 9 are each needed: with one switched off the
witness is accepted although it does not re-encode to itself. Rules 6 and 8 are implied by rule 9 in the
model (more than 63 fraction bits is neither a `double` nor an integer in range); the C++ decoder tests
them first, to bound its reads, so they need their witnesses (C4). The result is a `number` (sign,
exponent, 64-bit significand, or a special), with typed conversions: `to_int64`/`to_uint64` exact or empty,
`saturate_int64`/`saturate_uint64` with today's clamp semantics (`serialise.h:295-307`, NaN to 0), and
`to_double`, exact for every `double` and round-to-nearest-even for an integer above 2^53. Decoding uses
integer operations and `std::bit_cast` only, so the bytes are the same on every platform; the old codec
works in `long double`, which is 80-bit on x86_64, binary64 on Apple arm64 and binary128 on Linux aarch64
(platform ABIs; that this changes its bytes across platforms is unverified: build `legacy_probe` on an arm64
host and diff its output on `model.py`'s boundary set). Nothing in this design depends on the answer, since
old bytes are refused (C5).

**D6. Variable length, no larger than the old codec on the corpus below.** Value slots are stored per
document, so bytes matter. The corpus (T5 uses it, `sizes.py`):

1. The numeric fields of Xapiand's own load-test datasets, `docs/public/assets/accounts.ndjson` and
   `cities.ndjson`, 1,000 documents each, with the numeric accuracy terms of `def_accuracy_num`
   (floors 100 to 10^8, floats truncated to `int64` first, as `generate_terms.cc:226-239` does).
2. Thirteen synthetic distributions of 2,000 values (seed 27): small, signed and negative integers,
   two-decimal prices of both signs, scores in [0, 1), epoch milliseconds, snowflake-style ids in
   [2^60, 2^62), `uint64` hashes, datetimes at millisecond, microsecond and whole-second precision, and
   timedeltas. They are the architect's sets (`.scratch/sortable-codec-arch/p3_sizes.py`).
3. The datetime sets' accuracy terms are Xapiand's own: `def_accuracy_datetime` is hour, day, month, year,
   decade and century (`schema.cc:169-176`), applied by calendar truncation of the date
   (`generate_terms.cc:253-293`), with `GenerateTerms::year` flooring the year (`generate_terms.h:116`).
4. An estimate of the version slot (versions 1 to 1000).

The size table, `sizes.py` run 2026-10-27 on x86_64 macOS with the old codec built from Xapiand:6c5c1a40d
(mean bytes per value, and per accuracy term; the version row is an estimate, not a measurement of a
live slot):

| Set | Values | Old mean B | New mean B | New max B | Terms | Old term B | New term B |
|---|---|---|---|---|---|---|---|
| accounts.accountNumber | 1000 | 3.94 | 3.74 | 4 | 6000 | 2.59 | 2.37 |
| accounts.balance | 1000 | 7.83 | 7.82 | 8 | 6000 | 1.51 | 1.39 |
| accounts.age | 1000 | 2.00 | 1.95 | 2 | 6000 | 1.00 | 1.00 |
| accounts._id | 1000 | 2.68 | 2.23 | 3 | 6000 | 1.18 | 1.15 |
| cities._id | 1000 | 2.68 | 2.23 | 3 | 6000 | 1.18 | 1.15 |
| cities.population | 1000 | 3.66 | 3.11 | 4 | 6000 | 2.13 | 2.10 |
| small ints 0..100 | 2000 | 1.96 | 1.92 | 2 | 12000 | 1.00 | 1.00 |
| signed ints -1000..1000 | 2000 | 2.73 | 2.22 | 3 | 12000 | 2.14 | 1.99 |
| negative ints -1e6..0 | 2000 | 3.94 | 3.69 | 4 | 12000 | 3.53 | 3.20 |
| prices 2 decimals 0.01..1000 | 2000 | 7.79 | 7.77 | 8 | 12000 | 1.18 | 1.15 |
| negative prices -1000..-0.01 | 2000 | 7.82 | 7.81 | 8 | 12000 | 3.10 | 2.83 |
| scores uniform [0,1) | 2000 | 7.93 | 7.82 | 8 | 12000 | 1.00 | 1.00 |
| epoch ms as integer | 2000 | 6.75 | 6.00 | 6 | 12000 | 5.94 | 5.75 |
| snowflake ids ~2^60..2^62 | 2000 | 8.99 | 8.95 | 9 | 12000 | 8.61 | 8.30 |
| uint64 hashes | 2000 | 9.24 | 8.96 | 9 | 12000 | 8.75 | 8.51 |
| datetime ms precision | 2000 | 7.99 | 7.94 | 8 | 12000 | 4.41 | 4.12 |
| datetime us precision | 2000 | 7.98 | 7.94 | 8 | 12000 | 4.40 | 4.12 |
| datetime whole seconds | 2000 | 5.00 | 4.98 | 5 | 12000 | 4.40 | 4.12 |
| timedelta 0..3600 s, ms | 2000 | 7.96 | 7.91 | 8 | 0 | - | - |
| estimate: versions 1..1000 | 1000 | 2.68 | 2.23 | 3 | 0 | - | - |

Totals:

1. The corpora (accounts and cities: 6,000 values, 36,000 terms): values 21,089 B against 22,790 B (7.5%
   smaller), terms 54,951 B against 57,601 B (4.6% smaller).
2. The synthetic sets and the version estimate (27,000 values, 144,000 terms): values 170,070 B against
   174,849 B (2.7% smaller), terms 553,242 B against 581,663 B (4.9% smaller).
3. Together: values 191,159 B against 197,639 B (3.3% smaller), terms 608,193 B against 639,264 B (4.9%
   smaller).

Xapiand's datetime accuracy buckets, on the microsecond set (the six terms of `def_accuracy_datetime`,
mean bytes per term):

| Bucket | Old B | New B |
|---|---|---|
| hour | 4.94 | 4.76 |
| day | 4.49 | 3.99 |
| month | 4.50 | 4.00 |
| year | 4.50 | 4.00 |
| decade | 4.00 | 4.00 |
| century | 4.00 | 4.00 |

The honest result: smaller, or equal to two decimals, in every set (T5 prints `PASS`); the gain is 2.7%
to 7.5% of the bytes in total. It is largest for small and negative integers and ids (signed integers
2.73 B to 2.22 B) and smallest for full-entropy doubles (scores 7.93 B to 7.82 B). 64-bit identifiers
are 8.99 B to 8.95 B (snowflake-style ids) and 9.24 B to 8.96 B (hashes). Revision 0's layout was 22% and
18% larger than the old codec on those two sets and 24% larger on latencies of 0.0005 to 0.06 s
(architect's measurement, `p3_sizes.out` and `p4_alt.out`), because its two tags per exponent left only
E = -4 to 57 direct; the window E = -60 to 63 removes that cost, and latencies measure 7.94 B against the
old 7.99 B (`p4_alt.out`, the same bytes: the cross-check below). A `double` below 2^-60 takes the escape
(1e-30 is 9 B); no set holds such values, so the table does not show what they cost against the old codec
(not measured). Fixed encodings for comparison on the same 33,000 values: 9 bytes (a type tag and an
8-byte map) would total 297,000 B, 50% larger than the old codec, and an 8-byte map per type 264,000 B (34%
larger) that cannot order across types (D1). The byte-aligned tail keeps the codec small: 7-bit groups
with a continuation bit (prefix-free, so negatives could be plain complements) cost 2 bytes for 1 and 9
for the microsecond timestamp sampled, against 1 and 8 here (revision 0's `model.py`).


**D7. Home: a new library, `Kronuz/sortable`.** Header-only C++20, MIT, no dependencies, in the
`cpp-libraries` shared CI matrix, target `sortable::sortable`, header `sortable.hh`, namespace `sortable`.
The catalog has no order-preserving numeric codec: `varint` is a length codec built for compact framing
(its continuation bit marks the last byte, its order is not numeric), and `endian` is byte order. Both
consumers need it outside Xapiand: strata's `bytes` value columns (P1, `phase4/values-bytes`) store
Xapiand's slot bytes unchanged and order them by `memcmp` (`strata/docs/design/xapiand-on-strata.md`
D3), and typed slots arrive later by decoding these bytes. API:

```cpp
namespace sortable {
inline constexpr std::size_t max_size = 10;
template <class T> concept encodable =   // every standard integer type, float and double
    (std::integral<T> && !std::same_as<T, bool> && !is_character_v<T>) || std::same_as<T, float> || std::same_as<T, double>;
template <encodable T> std::size_t encode(T v, char* out) noexcept;   // returns the bytes written
template <encodable T> std::string encode(T v);
std::optional<number> decode(std::string_view bytes) noexcept;
inline constexpr std::string_view max_finite, min_finite;             // encode(DBL_MAX), encode(-DBL_MAX)
}
```

One constrained template replaces the three-overload set, which did not compile for `int`, `long` or
`size_t` on macOS (where `int64_t` is `long long`) and for `long long` on Linux (`.scratch/sortable-codec-arch/
p5_overloads.out`): signed types take the `int64` path, unsigned types the `uint64` path, `float` widens to
`double` exactly, and `bool`, the character types and `long double` do not compile (the narrowing of
`long double` stays explicit at Xapiand's call sites, D8). `overloads.cc` in the design directory is the
syntax-only check; `c++ -std=c++20 -fsyntax-only` accepts the 20 call forms and rejects the 7 others
(Apple clang 17, `-fsyntax-only`, no errors). Step 1 compiles the same forms on every T11 platform.
`max_finite` and `min_finite` are the encodings Xapiand's key maker uses for a missing sort value
(`MAX_CMPVALUE` and `MIN_CMPVALUE`, `keymaker.h:46-47`, today `Serialise::floating(DBL_MAX)`): a document
without a value sorts after every finite number and below +inf, as it does with the old codec, and the
mechanism note says so (D12).

Prospect's fixed-width maps (`include/prospect/ordered.hh`) are the same mechanism for typed columns and
belong in this library too (Q3); that move is prospect's step, not this design's.

**D8. Xapiand's switch.** `Serialise::integer/positive/floating/timestamp` and their `Unserialise` twins
call `sortable`; `Serialise::floating(long double)` narrows to `double` first (field values are `double`
already). Every direct call in the table above moves to them or to `sortable` (versions and ids encode
their `uint64` directly instead of through `double`). `src/sortable_serialise.{cc,h}` are deleted, and with
them the GPL in Xapiand's own tree (the Phase 8 item at `strata/docs/PLAN.md:673`, done early, and
`docs/design/extraction.md:111, 148`). The `"QN\x80"` literals become a named constant (zero still encodes
as `80`). The unused `get_hashed` goes. A malformed stored value raises `SerialisationError` naming the
slot, instead of decoding to a clamped guess. With the switch:

1. A range bound is encoded as given for the value-slot comparison (D1), so `_from: 1.5` on an integer
   field is exact; the accuracy-term prefilter takes the floor of a lower bound and the ceiling of an upper
   one (over-covering, which the slot comparison corrects, `docs/mechanisms/numeric-range-terms.md` on
   `fix/numeric-range`), and a bound beyond the `int64` range no longer goes through an undefined cast.
2. A positive value of 2^63 or more for an integer field is rejected (400) instead of wrapping (Q4).
3. A NaN field value is rejected (400) (Q1).
4. Accuracy buckets are unchanged (numbers, not bytes); only their term bytes change.

**D9. A marker on every shard, checked on every open.** Every shard carries the metadata key `_value_codec`
with the value `"2"`; the old encoding, "1", is the absence of the key. The check and the write live in
`Shard` (`src/database/shard.cc`), on top of the engine's metadata calls, so they hold for any engine under
the seam. Where the marker is written, by how the shard came to exist:

| The shard is | The marker |
|---|---|
| A new primary, created by a writable open (`open_local` returns `created`, `shard.cc:437-439`), including the shards of a RESTORE (`DB_DISABLE_WAL`, `handler.cc:874`, `main.cc:870`) | Written by `reopen_writable` straight to the engine (`wdb->set_metadata`, the call `Shard::set_metadata` makes at `:1885`, never its WAL branch at `:1892-1902`) and committed before the open returns, so no document and no WAL line can precede it. The WAL is not used on purpose: a crash cannot leave a WAL line without a marker, and the adoption rule below stays "no WAL file". |
| A replica (`DB_REPLICA`: `replication_protocol_views.cc:329, 985`) | Never written at creation. A replica at revision 0 always receives the primary's whole database (`from_revision` is 0 on a new shard or a uuid mismatch, and `to_revision == 0` copies every file, `replication_protocol_views.cc:521-538`), so the marker arrives with the files, or with the WAL line of a later `set_metadata`. |
| Created by a readable open (`DB_CREATE_OR_OPEN` in `reopen_readable`, `:593`) | Not written (it cannot write). The shard stays empty and unmarked; the next writable open adopts it. |
| A remote shard (`!endpoint.is_local()`, `:404`, `:518`) | Nothing: the node that serves the shard checks its own local open. |

The marker's commit moves a new primary from revision 0 to 1: by reading, `set_metadata` marks the shard
modified and schedules revision `prior + 1` (`:1889-1890`). Unverified; T8 reads the revision of a fresh
index and of a replica of it. The consequence for replication is an improvement: a replica of a new, still
empty index used to be at the primary's revision (0 and 0, nothing copied) and now copies the marked, empty
database once, which is how it gets the marker.

The check runs right after the engine shard is assigned (`:463`, `:611`), local shards only, and before the
WAL replay (`:468-483`), so an old shard's WAL, which holds old bytes, is never replayed into a new shard.
It reads `_value_codec` (`engine().metadata(key)`, as `Shard::get_metadata` does at `:1812`):

1. `"2"`: the open goes on.
2. Any other value: refuse, "written by value encoding N, which this version does not read; upgrade this
   node".
3. Absent, and the shard is empty (no document, no metadata key, no WAL file): a writable, non-replica open
   writes the marker and goes on (the adoption, which also covers a crash between creation and the first
   commit, since nothing else has run); a replica or readable open goes on unmarked and writes nothing.
4. Absent, and the shard is not empty: refuse with a new `engine::errc::format` that no retry loop repeats
   (`retry_or_throw` rethrows unknown kinds at once, `:642`; `engine::read_fate` maps it to "fail", which
   step 2 checks). The message names the index and the way out: "index 'X' was written with value encoding 1
   (Xapiand before <release>); stop the node and run 'xapiand --upgrade-codec X', or dump it with that
   release and restore it with this one".

The node never answers from old bytes, which the new decoder could otherwise accept as different numbers
(the old 2, `be40`, decodes as 5/4, the old 100, `bfa4`, as 105/32, and the old 2147483647, `c5ffffffff`, as 8589934591/33554432, about 256 (`model.decode`; the old 1, `be`, happens to agree)). The cost is one metadata read per shard open (not measured; T12). The metadata API refuses writes to
`_value_codec`. Its read-only list (`search_views.cc:1131`) compares the request's command, minus its first
character, with `schema`, `wal`, `nodes`, `metrics`, while the code stores the schema as `_schema`
(`schemas_lru.cc:347`, `reserved/schema.h:57`); whether a client can write `_schema` through it is unverified
live (check: write the metadata key `_schema` over HTTP and read the schema back). The guard therefore
refuses `_value_codec` by its stored name and every key that starts with `_`, and T8 tries both spellings.

**D10. Old data is upgraded in place, offline, by the new binary.** The old encoding's bytes are never read:
the upgrade copies what is codec-independent (each document's stored data and blobs, the shard's metadata)
and indexes it again, as RESTORE does, so no old slot or term byte crosses over. Two entry points, one
routine:

1. `xapiand --upgrade-codec [INDEX...]` with the node stopped (`main.cc:1114-1117` dispatches `--dump` and
   `--restore` the same way): for every local shard of the named indexes (all, if none is named) that has no
   marker, in this order: the `.xapiand/*` internal indexes, foreign-schema indexes, the rest.
2. The same routine, for the internal indexes only, at the start of the node, before the manager opens
   `.xapiand/*` (first uses `manager.cc:708`, `:836`), so an upgraded node starts. `.xapiand/nodes` and `.xapiand/indexes`
   are never empty and are written with old version-slot bytes (`engine/xapian/shard.cc:64`, read at
   `index_resolver_lru.cc:413`), so without this a D9 refusal would stop the node before it could serve
   anything; `.xapiand/indexes` is also the only record of every index's shard count, replicas and
   placement (`index_resolver_lru.cc:248-272, 386-440`).

The routine, per shard directory:

1. Open it in a legacy mode that skips the D9 check, replays no WAL and reads only the stored data
   (`engine().scan`, as `dump_documents` does, `handler.cc:820-862`), the blobs (`_dump_document`,
   `handler.cc:721-756`) and the metadata. It never decodes a slot or a term. If the WAL holds committed
   lines the shard has not applied (the comparison the replay makes), it stops with "stop the old release
   cleanly, or dump with it": replaying them here would decode old version bytes
   (`engine/xapian/shard.cc:78-92`).
2. Create `<shard>.codec2` with the normal create path (the marker first, D9), copy every metadata key
   (`_schema` included, so schemas and foreign-schema references travel as they are), and index each
   document's data into it (the restore indexer, `handler.cc:874`, aimed at that one shard).
3. Commit, fsync, rename the old directory to `<shard>.codec1.bak` and the new one into place. The backup
   stays until the operator removes it; a leftover `.codec2` is deleted and redone, a marked shard is
   skipped, so the routine is safe to rerun after a crash.

What it carries: documents, blobs, the schema, foreign schemas (they are indexes), user metadata, and the
index settings (they are documents of `.xapiand/indexes`, upgraded first). What it does not: `_version`.
A document's version exists only in the version slot and the `V` term (`engine/xapian/shard.cc:52-64`), both
old bytes, and a document written fresh gets version 1 (`:116-118`), so every version restarts at 1, here
and in DUMP and RESTORE alike (by reading; T13 checks it live). A client that holds a `_version` and writes
with `?version=` is refused after the upgrade and reads the document again (Q6). Docids are dense again.
Foreign schemas are read with their version (`schemas_lru.cc:111-126`); a restart at 1 changes a cached
version, not a schema (unverified; T13 resolves an index through a foreign schema after the upgrade).

A cluster upgrades every node, and each node rebuilds its own copies of `.xapiand/*`. The copies are the same
documents with new shard uuids; a replica whose uuid differs from the primary's is replaced by the primary's
whole database (`replication_protocol_views.cc:521-525`, by reading), so they converge (T13, on two nodes).

The manual route stays for anyone who prefers it, and for a shard the routine refuses: dump each index with
the old release (`xapiand --dump`), start the new release on an empty data directory, and restore. DUMP
carries documents only (`handler.cc:721-756, 820-862`), so the order is: (1) foreign-schema indexes, (2)
each index created with its schema and settings (`backups.md:102-120` already says to set the schema before
restoring), (3) RESTORE, (4) user metadata (`GET` of the old index's metadata keys, `search_views.cc:1060-1078`,
written back). `_version` restarts at 1, as above. T13 runs both routes on the same data and compares.

**D11. Mixed-version clusters and downgrades.**

1. The discovery protocol major goes from 1 to 2 (`discovery.cc:73`), and the remote protocol from 47.0 to
   48.0 (`remote_protocol_views.h:57-58`). The replication protocol has no version constant (grep), so
   discovery is what keeps versions apart there. An old node drops a version-2 datagram
   (`cluster/bus.h:104`: a frame newer than the receiver's version is dropped), so it never hears a new
   node. The reverse does not hold: the bus accepts every frame at or below its own version
   (`cluster/bus.h:34, 104`), so a new node would hear an old node's heartbeats and could follow an old
   leader. Q7 chooses the fix.
2. A new node that hears an incompatible frame while it starts refuses to start, naming the sender: "node
   <address> runs discovery protocol 1.0 (value encoding 1); upgrade every node together". Later, it logs
   the same once per sender and ignores the frame. The remote protocol's version check is in a file this
   design did not open (`remote_protocol_views.cc`); step 2 confirms that a v47 peer fails at the
   handshake (T14).
3. A new node that receives an old primary's whole-database copy ends up with an unmarked, non-empty shard
   and refuses it at its next open (D9, case 4): loud, on the new side.
4. A release older than this design cannot be made to refuse a new shard: it predates the marker, opens
   the files and reads every number wrong with no error. Every release from this one on refuses a marker it
   does not know (D9, case 2). The changelog says a downgrade needs a dump with the new release and a
   restore with the old (Q5).

**D12. What depends on the bytes, and what does not.** Value-range sources and sorting compare bytes and
need nothing but D1-D3. The key maker computes distances in the field's type and re-encodes them; its
framing escapes `0x00`, which the tails contain, already (D3). Aggregations decode through `Unserialise`
(`metrics.h:158-188`). Numeric-range decomposition works on numbers (`range_terms.h` on
`fix/numeric-range`) and is untouched; its term bytes change. Geo values do not use the codec; geo accuracy
terms do, through `Serialise::positive`, consistently on both sides. strata never sees old bytes: it is fed
by DUMP and RESTORE (prospect `docs/design/phase-4.md`, "Xapiand's replacement"), and its `bytes` columns hold
the new codec's bytes. A missing sort value is `sortable::max_finite` (D7), after every finite number and
before +inf; the mechanism note `docs/mechanisms/value-codec.md` states where missing values sort against
-inf and +inf.

## Changes to other designs

1. **prospect `docs/design/phase-4.md` D3a, "Xapiand's replacement"**, says Phase 8 replaces
   `sortable_serialise` with the same 8-byte big-endian map. This design replaces it now, with a
   variable-length cross-type codec, because Xapiand's slots mix types (D1). D3a's own typed columns keep
   the 8-byte map; Q3 decides whether that map moves into `sortable`.
2. **strata `docs/PLAN.md:673`** (Phase 8, "replace `sortable_serialise` with the 8-byte map"): done by
   this design's step 2, with this codec.
3. **strata `docs/design/xapiand-on-strata.md` D3** ("replacing `sortable_serialise` is Phase 8 anyway"):
   values stay opaque bytes; P1 is unaffected, since its columns order by `memcmp`.
4. **`fix/numeric-range`** (in review) edits `range.cc` and `generate_terms.cc`; step 2 rebases onto it after
   it lands and keeps its decomposition and tests.
5. **`docs/dev/value-compare.md` finding 1** is fixed by this design; T7 reruns its probes.
6. **`Kronuz/cluster`** (`bus.h`) gains a minimum accepted version and a callback for a frame below it, if
   Q7 is (a). Detent is changing the cluster libraries (completion, journal, consensus), so the change goes
   through its agent first.

## Gate rows

| Row | Target | Procedure |
|---|---|---|
| T1 Round trip | `decode(encode(x))` gives back x exactly for every value of each type: `int64`, `uint64`, `double` bit for bit (except -0.0 to 0 and NaN to the canonical NaN, D4); 0 failures | `sortable/test/property_test.cc`: exhaustive over every `int64` in [-2^20, 2^20]; every 2^k + d and -(2^k) + d for k 0..63, d in [-1024, 1024]; every `double` 2^e for e -1074..1023 with both neighbours (`nextafter`) and sign, and a fraction of every length 1..63 at the window's edges (E = -62..-58 and 61..66); runs of zeros and ones of every length 1..63 (`1 0^r 1`, `1^r`); 10^7 random bit patterns per type, seed printed |
| T2 Order | For every adjacent pair of a sorted set of values (exact order, any mix of types), `memcmp` of the encodings agrees, and equal numbers of different types encode to equal bytes; 0 failures. Includes the old codec's failures: 2147483649 against 2147483650, -1073741824 against -1073741825, 1+2^-40 against 1 | Same test: the T1 set, each value also as every type that holds it exactly, sorted by an exact comparator that does not use the codec (integers through `__int128`, `double` against integer by exponent and significand), then adjacent pairs; plus 10^7 random cross-type pairs |
| T3 Exhaustive binary32 | Round trip and adjacent order over all 2^32 `float` values widened to `double` (NaNs to one) | `sortable/test/exhaustive_float.cc`, not in CI; the developer runs it once under `quiet light`, and the gate reruns it |
| T4 Decoder safety | Every input of 0 to 16 bytes is either rejected or re-encodes to itself; no read past the end; every D5 rule has a witness input in the committed reject fixture and a mutant of the decoder that drops the rule, caught by the unit test; one CPU-hour of fuzzing with 0 findings, ASan and UBSan clean | `sortable/fuzz/decode_fuzz.cc` (libFuzzer: decode; when accepted, `encode(decoded) == input`; split input pairs and check `memcmp` against the exact comparator); `-max_total_time=3600 -timeout=5` at the gate's commit; exhaustive over all 1- and 2-byte inputs and the 3-byte inputs starting with an escape tag in the unit test; `python3 -I docs/design/sortable-codec/model.py rules` lists the witnesses (`sortable/test/reject.txt`) |
| T5 Size | On the D6 corpus, value bytes and accuracy-term bytes no larger than the old codec's in every set; today `T5: PASS` on 20 sets (D6) | `python3 -I docs/design/sortable-codec/sizes.py PROBE docs/public/assets/{accounts,cities}.ndjson` prints the table and `T5: PASS` or the failing sets (exit status 1), with PROBE built by `c++ -std=c++20 -O1 -I src -o PROBE docs/design/sortable-codec/legacy_probe.cc src/sortable_serialise.cc` at Xapiand:6c5c1a40d; after step 2, the same with `sortable` in place of `model.py` |
| T6 Two implementations agree | The C++ library's bytes equal `model.py`'s for the T1 boundary set and 10^5 seeded random values; the vectors file is committed as the format's fixture | `python3 -I docs/design/sortable-codec/model.py vectors > sortable/test/vectors.txt` (113,491 lines today), read by `sortable/test/vectors_test.cc` on every CI platform |
| T7 Xapiand answers exactly | Range, sort and aggregation answers equal brute-force truth over the old codec's failing values (2^k + d for k 31..63, -(2^a) - d for a 30..62, 1+2^-k), integer, positive, floating, datetime, both closed and half-open, single node; float bounds on integer fields and the global numeric slot included; 0 wrong | `harness/numeric_range_check.py` (from `fix/numeric-range`) extended with these values and a sort and a min/max aggregation check; `.scratch/value-compare/live_probe_uid.py` from the exploration, against the step-2 build |
| T8 Old shards refused, new shards marked | Every case of D9: an old shard with documents, and one with a non-empty WAL, fails to open, read or write with the D9 message, on a node and on a 2-node cluster; an empty unmarked shard is adopted by a writable open and left alone by a readable or replica open; a new primary has the key after its first commit, at the revision D9 predicts, and so does a replica of it once replicated (both shards carry the key, revisions agree); a crash between shard creation and the marker commit (fault injected after `open_local`) restarts and adopts the empty shard; a shard marked `"3"` is refused as newer; a metadata write to `_value_codec` answers 400 in the stored and the stripped spelling; RESTORE into a new index marks every shard | `harness/value_codec_migration.sh` with both binaries; unit tests in `tests/` for the adoption rule, the revision and the crash case |
| T9 Nothing else regressed | Docs E2E suite every assertion; unit tests under ASan and UBSan; `harness/id_terms_check.py`; cluster and WAL recovery checks; raw slot or term bytes shown by docs examples (`:info` `values`, `terms`) updated | `harness/run_all.sh`; `harness/e2e_check.sh` |
| T10 No GPL in Xapiand's own tree | `src/sortable_serialise.*` gone; the grep below finds nothing; ARCHITECTURE.md and `docs/dev/extraction-ledger.md` updated | `grep -rn sortable_ src \| grep -v '^src/xapian/'`, added to `harness/seam_check.sh` (filter by path prefix: `--exclude-dir=xapian` would also skip `src/engine/xapian/`, which holds 5 of the direct calls) |
| T11 Platforms | T1, T2, T4's exhaustive part and T6 pass on Linux x86_64 (GCC, Clang), Linux arm64, macOS arm64 and x86_64, FreeBSD, ASan and UBSan; the 20 call forms of D7 compile and the 7 others do not, on every one | `cpp-libraries/tools/preflight.sh` and the shared workflow; `overloads.cc` in the test directory |
| T12 Cost (the lead measures, on `main`) | Bulk indexing of the accounts corpus and a range, sort and aggregation query mix within noise of the pre-switch `main`, or better, including the marker read per shard open; the honest magnitude stated either way | `harness/loadtest.py` both modes, interleaved, under `quiet exclusive` |
| T13 Upgrade carries everything | The new binary started on a data directory written by the old one (single node, then 2 nodes): `.xapiand/nodes` and `.xapiand/indexes` are upgraded, the node reaches READY and serves a PUT of a new index; an old user index is refused with the D9 message until `--upgrade-codec`, then answers T7's questions; the schema, a foreign-schema index and an index that references it, user metadata and the index settings (shard count, replicas, placement) are identical before and after; documents and blobs are identical; `_version` is 1 for every document and the settings document resolves; both nodes' `.xapiand/indexes` agree. A kill -9 of the old node during a write, then the new binary: refused with the WAL message, and the old release's clean stop plus the upgrade succeeds. The manual route (old dump, new restore) on the same data gives the same documents and T7's answers | `harness/value_codec_migration.sh` (the old binary builds the fixtures: indexes with numeric, datetime and `_id` fields at the old codec's failing values, a foreign schema, metadata, blobs) |
| T14 Mixed versions refused | An old and a new node on one multicast group and cluster name do not form a cluster, in both start orders: the new node refuses to start with the D11 message and the old node never lists it; a v47 remote client against a v48 server fails at the handshake; after Q7's change, the incompatible-frame callback fires once per sender | `harness/value_codec_migration.sh`, two nodes, both binaries; a unit test in `cluster` for the minimum version |

## Open questions for the owner

**Q1. NaN in a document.** (a) Reject at indexing with 400; the codec still encodes NaN (D4) for any
internal NaN. (b) Index it, sorting above +inf. Recommendation: (a). JSON has no NaN, a range or sort over
NaN means nothing, prospect's D3a rejects it, and the old codec asserts on it, so no index holds one.

**Q2. -0.0.** (a) Encode as 0 (D4), losing the sign of a negative zero in the slot; the document's own data
keeps it. (b) Encode it just below +0.0, as Java's `Double.compare` does, so `_range: {_from: 0}` excludes
-0.0. Recommendation: (a): numeric equality is what ranges and terms mean, and it matches the old codec
and prospect.

**Q3. The library's home.** (a) New `Kronuz/sortable` with this codec now, and prospect's `ordered.hh`
maps moved into it in a later prospect step, so the order-preserving maps have one home. (b) New
`sortable` with this codec only; prospect keeps `ordered.hh`. (c) Inside Xapiand (`src/`), no library.
Recommendation: (a). Creating the public repository is the lead's push, after T1-T6 and T11 pass locally.

**Q4. A `uint64` of 2^63 or more in an integer field.** (a) Reject with 400. (b) Store it exactly (the codec
can) and let integer readers saturate to `INT64_MAX`. Recommendation: (a); today it silently stores a
negative number, and a positive field exists for these values.

**Q5. A release older than this design on a new data directory.** It cannot refuse a new shard (D11, case
4), and reads wrong numbers silently. (a) Accept it: the changelog says downgrades need a dump and a
restore, and every later release refuses unknown markers. (b) Put new shards under a versioned root (for
example `<data>/v2/`), so an old release finds no shards at its path and the upgrade keeps the old
directories as the downgrade copy; this costs the old data's disk until the operator deletes it and moves
every path the code builds. Recommendation: (a): formats are open until release, the alpha line has no
downgrade promise, and (b) changes every path for a case the changelog covers.

**Q6. `_version` after the upgrade.** Every document restarts at version 1 (D10). (a) Accept, say so in the
changelog. (b) Carry versions: the upgrade would decode the old version slot with a reader of the old
format, which needs the old algorithm in Xapiand's tree again (the GPL this design removes) or a table
derived by black-box probing. Recommendation: (a): the old format cannot return to the tree, and a version
only matters to a client that holds one across the upgrade.

**Q7. How a new node refuses an old one.** (a) `Kronuz/cluster`'s `Bus` takes a minimum accepted
(major, minor) and a callback for frames below it; Xapiand passes 2.0 and logs or refuses on the callback.
(b) Xapiand only: add the codec generation to the bus token (`opts.cluster_name`), so an old and a new node
ignore each other's frames. Recommendation: (a). (b) needs no library change, but old and new nodes then
form two clusters in silence, and the new node can never say why. (a) is a small change to a library Detent
is also changing, so it goes through that agent.

## Steps

**Step 1. The library** (developer, branch and worktree in a new local `~/code/Kronuz/sortable`, not
pushed until the lead lands it). `sortable.hh` (D2-D5, D7), README, AGENTS.md, a how-it-works note
(ARCHITECTURE.md), the property tests (T1, T2), the exhaustive binary32 run (T3), the fuzz target and its
exhaustive short-input test (T4), the vectors fixture and the reject fixture from `model.py` (T6, T4), the
shared CI matrix and the call-form check (T11). Done: T1, T2, T4 (exhaustive part, a mutant per D5 rule
including rules 6 and 8, and 10 minutes of fuzzing; the CPU-hour at the gate), T6 and T11 pass; T3 run once;
a mutant per rule of D2 and D3 (the tag of a negative's U, the complemented field, 1 - f for negatives, the
canonical last byte) each caught by a test; ASan and UBSan clean. Blocked by Q3 only for where it lives; the
bytes are settled by D2-D5. Review: the security reviewer's decoder-safety review before it lands; a code
review.

**Step 2. Xapiand's switch** (developer, branch `fix/sortable-codec`, after `fix/numeric-range` lands and
step 1 is fetchable; one branch, because the switch alone cannot start an upgraded node). In order:

1. **2a, the switch.** D8: `FetchContent` of `sortable` pinned, every call in the table moved,
   `src/sortable_serialise.*` deleted, the D8 query fixes, the missing-value constants.
2. **2b, the marker.** D9: the write on creation, the check on open, `errc::format`, the metadata guard.
3. **2c, the upgrade.** D10: the legacy open, `--upgrade-codec`, the automatic upgrade of `.xapiand/*`.
4. **2d, versions.** D11: the discovery and remote protocol bumps, the incompatible-frame handling (after Q7;
   if (a), the `cluster` change lands first as its own step, through Detent's agent).

Also a changelog entry under `[Unreleased]` (exact numeric order; old indexes need `--upgrade-codec`; a
downgrade needs a dump and a restore; `_version` restarts at 1), ARCHITECTURE.md and the extraction ledger.
Done: T5 (with `sortable`), T7, T8, T9, T10, T13, T14, each D8 fix with a test that fails on master; a
mechanism note `docs/mechanisms/value-codec.md`. Review: the architect's scoped check of D9 and D10 (they
decide what a node opens, replays and rebuilds), a code review; the lead measures T12 on `main` after
landing.

**Step 3. Follow-ups, outside this branch.** prospect's `ordered.hh` into `sortable` (Q3 (a)); the strata
PLAN and xapiand-on-strata notes of "Changes to other designs".

**Lead rulings (2026-10-27, on the designer's recommendations; reversible before release).**
Q1 (a): NaN in a document is rejected at indexing with 400. Q2 (a): -0.0 encodes as 0. Q3 (a): a
new `Kronuz/sortable` library now; prospect's `ordered.hh` moves into it later. Q4 (a): a `uint64`
of 2^63 or more in an integer field is rejected with 400.

## Revision 2: the architect's review folded in

`docs/reviews/sortable-codec-design.md` (2026-10-27, REJECT) at Xapiand:57deb969c.

| Finding | Where it is closed |
|---|---|
| L1 (finding 1) one tag per exponent | D2, D3, D5; `model.py` rewritten; `selftest`, `mutants` and `rules` pass; the bytes equal the layout `p4_alt.py` validated (0 of 196,961 typed values differ in bytes and the accepted sets over 16,268,708 inputs agree with 0 disagreements, `.scratch/sortable-codec-rev2/xcheck.py`) |
| L2 (finding 2) the size row | D6, T5, `sizes.py`: the 13 broader sets, the corpora, Xapiand's datetime buckets |
| L3 (finding 3) `.xapiand/*` | D10 (the automatic upgrade at start), T13 |
| L4 (finding 4) DUMP carries documents only | D10 (what the upgrade carries, the manual route's order, `_version`), Q6, T13 |
| L5 (finding 5) the marker's write path | D9 (the table, the revision, the read-only open, the metadata guard), T8 |
| L6 (finding 6) mixed versions and downgrades | D11, Q5, Q7, T14 |
| C1 (finding 7) overloads | D7, `overloads.cc`, T11 |
| C2 (finding 8) T10's grep | T10 |
| C3 (finding 9) the missing-value sentinel | D7 (`max_finite`, `min_finite`), D12 |
| C4 (finding 10) a mutant per D5 rule | D5, `model.py rules`, T4, step 1 |
| C5 the arm64 `long double` question | D5, stays unverified |

**Lead rulings on revision 2 (2026-10-27).** Q5 (accept): a release older than this design cannot
refuse a new shard; the release notes say so and the upgrade is one-way. Q6 (amended): the in-place
`--upgrade-codec` keeps every document's `_version`, since it rewrites values in place and clients
rely on versions for optimistic concurrency; only the dump-and-restore path may restart them, and
the release notes say which path does what. Q7 (accept): new nodes refuse old ones through an
additive `Kronuz/cluster` change, coordinated with Detent's agent before it lands.

**Lead ruling on N1 (2026-10-27).** Keep the window E = -60..63: doubles below 2^-60 cost 9.93 B
against the old codec's 7.98 B (+24%, the 3-byte escape), measured by the re-check; such values
are rare in indexed data (latencies and ratios sit far above 2^-60), and widening the window would
cost a byte elsewhere. D6 states the number.
