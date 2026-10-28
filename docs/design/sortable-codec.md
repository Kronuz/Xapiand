> Status: DRAFT (2026-10-27) revision 0, designing (ready for preliminary review). Workstream sortable-codec, branch `design/sortable-codec` from Xapiand:6c5c1a40d. Owner ruling 2026-10-27: replace the value encoding now with our own exact MIT codec.

# An exact, memcmp-ordered number codec

Xapiand stores every number it sorts or ranges on (values, accuracy terms, numeric ids, versions) through
`sortable_serialise`, a GPL file of Xapian origin. It is lossy: 2147483649 encodes to bytes that decode as
2164260864 and sort after 2147483650, and -(2^a) is a byte prefix of -(2^a)-1 for a >= 30, so the two sort
the wrong way round (`docs/dev/value-compare.md` section 1, Xapiand:eb83813a8; wrong since Xapiand:cdd00e367,
2016). This design replaces it with one exact codec in a new MIT library, switches Xapiand to it, and makes
a node refuse an index written in the old bytes. It stops at the first buildable steps.

Clean room: no file under `src/xapian/`, neither `src/sortable_serialise.{cc,h}` nor
`src/server/remote_protocol_views.cc` was opened, nor the ARCHITECTURE.md paragraphs that describe the old
algorithm (from `ARCHITECTURE.md:389` and `:651`; a grep showed their first lines). The old codec was compiled from its files and called as a
black box (`sortable-codec/legacy_probe.cc`) to measure its sizes and document compatibility. The format below
comes from public techniques: sign-magnitude with a biased exponent as in IEEE 754, complemented negatives
as in FoundationDB's tuple layer and CockroachDB's key encoding, and Lucene's fixed-width maps
(`NumericUtils`) for the fixed-width alternative.

Supporting files in `docs/design/sortable-codec/`: `model.py` (the executable reference model, the
format's oracle), `sizes.py` (the size row), `legacy_probe.cc` (the black-box probe).

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

**D2. The format.** A finite nonzero value is |v| = 2^E x (1 + f), with E its binary exponent and f a
fraction of L bits, L <= 63 (every `uint64` and every `double` fits; a subnormal double normalises the same
way). The first byte is a tag:

| First byte | Meaning | Then |
|---|---|---|
| `0x00` | never written: StringList's multi-value magic (`serialise_list.h:33`) | |
| `0x01` | negative, E >= 58, or -inf | 2-byte field, complemented; fraction |
| `0x02`-`0x7D` | negative, E = 57 down to -4 | fraction |
| `0x7E` | negative, E < -4 | 2-byte field, complemented; fraction |
| `0x7F` | never written | |
| `0x80` | zero | |
| `0x81` | positive, E < -4 | 2-byte field `2(E + 1074) + has_fraction`, big-endian; fraction |
| `0x82`-`0xFD` | positive, E = -4 to 57: tag `0x82 + 2(E + 4) + has_fraction` | fraction |
| `0xFE` | positive, E >= 58: field `2(E - 58) + has_fraction`; +inf is field `0xFFFF` | fraction |
| `0xFF` | NaN | |

A negative value's tag is `0xFF` minus the positive tag of its magnitude. The fraction is present when
`has_fraction` (f > 0): n = ceil(L / 8) bytes holding f x 2^(8n) big-endian, the last byte nonzero. An exact
power of two has no fraction bytes and the lower of its exponent's two tags, so 1, 2, 4 and every
integer up to 2^57 that is a power of two take one byte.

Examples (from `model.py`): 0 `80`; 1 `8a`; -1 `75`; 3 `8d80`; 1.5 `8b80`; 2147483649 `c900000002`;
-1073741824 `39`; -1073741825 `38fffffffc`; 1761523200 `c7a3fad8`; 0.01 `81085747ae147ae147b0`;
2^64-1 `fe000bfffffffffffffffe`; -2^63 `01fff5`; +inf `feffff`; -inf `010000`; NaN `ff`. At most 11 bytes.

Within a tag, positive values compare by fraction, and a shorter fraction is the longer one with zeros
after it, so `memcmp` (a prefix sorts first) is numeric order. Across tags the exponent decides, and the
power-of-two tag sorts below its exponent's fraction tag. The escapes carry `has_fraction` in the field's
low bit, after the exponent, so a larger exponent always wins; in the tag, the fraction tag of a smaller
exponent would sort above the power-of-two tag of the next one.

**D3. Negative values reverse the order without a terminator.** Tags mirror (`0xFF - t`), the escape field
is complemented (`0xFFFF - field`), and the fraction is stored as 1 - f: the n bytes of 2^(8n) - F, where F
is the positive fraction's n bytes. 1 - f keeps the length n and its last byte nonzero (the low byte of F is
nonzero), so the same prefix rule orders it, now reversed: a larger magnitude has a smaller 1 - f. This is
what the old codec gets wrong: it complements a trailing-zero-stripped encoding, so a shorter negative
encoding is a prefix of a longer one and sorts first. A power of two has no fraction and the higher of its
exponent's two negative tags, above every value of larger magnitude with the same exponent. The encoding is
not prefix-free (a value can be a prefix of another, as with positive fractions); a caller concatenating
several encodings frames them, as Xapiand's key maker already does (`keymaker.cc:760-783`).

**D4. Special values.** -0.0 encodes as zero, so -0.0 == 0.0 == 0, as the old codec does (both give `80`,
measured) and as prospect's map does (`ordered.hh:37-43`); its sign does not round-trip (Q2). NaN encodes as
the single byte `0xFF`, greater than everything, payload and sign dropped, so the codec is total and never
fails on input (the old codec asserts on NaN in this build, measured). Whether Xapiand indexes a NaN is
policy above the codec (Q1). +inf and -inf encode as above; -inf is no longer the empty string.

**D5. The decoder is strict and safe on arbitrary bytes.** `decode(bytes)` reads only within the given
length, allocates nothing, never throws, and accepts exactly the outputs of `encode`: a tag never written,
a special byte with a tail, a short field, an escape exponent that fits a direct tag or lies outside
-1074..1023, fraction bytes after a power-of-two tag, zero or more than 8 fraction bytes, a last fraction
byte of zero, more than 63 fraction bits, a field `0xFFFF` with a tail, and a value that is neither a
`double` nor an integer in [-2^63, 2^64-1] are all rejected. The result is a `number` (sign, exponent,
64-bit significand, or a special), with typed conversions: `to_int64`/`to_uint64` exact or empty,
`saturate_int64`/`saturate_uint64` with today's clamp semantics (`serialise.h:295-307`, NaN to 0), and
`to_double`, exact for every `double` and round-to-nearest-even for an integer above 2^53. Decoding uses
integer operations and `std::bit_cast` only, so the bytes are the same on every platform; the old codec
works in `long double`, which is 80-bit on x86_64, binary64 on Apple arm64 and binary128 on Linux aarch64
(platform ABIs; that this changes its bytes across platforms is unverified: build `legacy_probe` on an arm64
host and diff its output on `model.py`'s boundary set).

**D6. Variable length, measured smaller than today.** Value slots are stored per document, so bytes
matter. On Xapiand's own corpora (`docs/public/assets/accounts.ndjson`, the load-test dataset, and
`cities.ndjson`, 1,000 documents each), with two synthetic datetime sets and an estimate of the version slot
(`sizes.py`, 2026-10-27, the old codec built from Xapiand:6c5c1a40d on x86_64 macOS):

| Field | Values | Old mean B | New mean B | New max B | Accuracy term B, old | new |
|---|---|---|---|---|---|---|
| accounts.accountNumber | 1000 | 3.94 | 3.74 | 4 | 2.59 | 2.37 |
| accounts.balance (float) | 1000 | 7.83 | 7.82 | 8 | 1.51 | 1.39 |
| accounts.age | 1000 | 2.00 | 1.95 | 2 | 1.00 | 1.00 |
| accounts._id, cities._id | 2000 | 2.68 | 2.23 | 3 | 1.18 | 1.15 |
| cities.population | 1000 | 3.66 | 3.11 | 4 | 2.13 | 2.10 |
| synthetic datetime, microseconds, 2020-2025 | 1000 | 7.98 | 7.92 | 8 | 4.75 | 4.51 |
| synthetic datetime, whole seconds | 1000 | 5.00 | 4.98 | 5 | 4.75 | 4.51 |
| estimate: versions 1 to 1000 | 1000 | 2.68 | 2.23 | 3 | | |
| All | 9000 | 4.27 | 4.03 | | 2.39 | 2.27 |

Values total 36,230 B against 38,445 B (6% smaller), accuracy terms 109,069 B against 114,634 B over 48,000
terms (5% smaller), and the old bytes are wrong for some values. A fixed 9-byte encoding (a type tag and an
8-byte map) would be 81,000 B, 2.1x the old; an 8-byte per-type map 72,000 B, and it cannot order across
types (D1). The byte-aligned fraction keeps it small: 7-bit groups with a continuation bit (prefix-free, so
negatives could be plain complements) cost 2 bytes for 1 and 9 for the microsecond timestamp sampled, against
1 and 8 here (`model.py` at its first draft).

**D7. Home: a new library, `Kronuz/sortable`.** Header-only C++20, MIT, no dependencies, in the
`cpp-libraries` shared CI matrix, target `sortable::sortable`, header `sortable.hh`, namespace `sortable`.
The catalog has no order-preserving numeric codec: `varint` is a length codec built for compact framing
(its continuation bit marks the last byte, its order is not numeric), and `endian` is byte order. Both
consumers need it outside Xapiand: strata's `bytes` value columns (P1, `phase4/values-bytes`) store
Xapiand's slot bytes unchanged and order them by `memcmp` (`strata/docs/design/xapiand-on-strata.md`
D3), and typed slots arrive later by decoding these bytes. API:

```cpp
namespace sortable {
inline constexpr std::size_t max_size = 11;
std::size_t encode(std::int64_t v, char* out) noexcept;   // returns the bytes written
std::size_t encode(std::uint64_t v, char* out) noexcept;
std::size_t encode(double v, char* out) noexcept;
template <class T> std::string encode(T v);
std::optional<number> decode(std::string_view bytes) noexcept;
}
```

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

**D9. Old indexes are refused, then rebuilt by DUMP and RESTORE.** Every new shard carries the metadata
key `_value_codec` = `"2"` (the old encoding is "1", implicit), set and committed when the shard is created,
before any document. `Shard::reopen_writable` and `reopen_readable` (`database/shard.cc:379, 493`) read it
right after opening (`:437`, `:591`) and before WAL replay (`:468-473`), so an old shard's WAL, which holds
old bytes, is never replayed into a new shard. A shard without the key opens only if it is empty: no
documents, no metadata, no WAL; the node writes the key and goes on. Otherwise the open fails with an
`engine::Error` that names the index and says: "written with value encoding 1 (Xapiand <= v0.x); dump it
with that version and restore it with this one". The node never answers from old bytes, which the new
decoder could otherwise accept as different numbers: the old 1 (`be`) decodes as 67108864 and the old
2147483647 (`c5ffffffff`) as 1073741823.875 (`model.decode`). The metadata
API refuses writes to `_value_codec`, as it refuses `schema` (`search_views.cc:1131`). Migration is the
existing DUMP and RESTORE: dump writes each document's data (`handler.cc:820-862`), restore re-indexes it
through the indexer (`handler.cc:866-907`), so no old slot byte crosses over. Replicas copy shard files, the
key with them. A cluster upgrades every node together, under the major version the owner accepted.

**D10. What depends on the bytes, and what does not.** Value-range sources and sorting compare bytes and
need nothing but D1-D3. The key maker computes distances in the field's type and re-encodes them; its
reverse-order framing escapes `0x00`, which the fractions contain, already (`keymaker.cc:760-783`).
Aggregations decode through `Unserialise` (`metrics.h:158-188`). Numeric-range decomposition works on
numbers (`range_terms.h` on `fix/numeric-range`) and is untouched; its term bytes change. Geo values do not
use the codec; geo accuracy terms do, through `Serialise::positive`, consistently on both sides. strata
never sees old bytes: it is fed by DUMP and RESTORE (prospect `docs/design/phase-4.md`, "Xapiand's
replacement"), and its `bytes` columns hold the new codec's bytes.

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

## Gate rows

| Row | Target | Procedure |
|---|---|---|
| T1 Round trip | `decode(encode(x))` gives back x exactly for every value of each type: `int64`, `uint64`, `double` bit for bit (except -0.0 to 0 and NaN to the canonical NaN, D4); 0 failures | `sortable/test/property_test.cc`: exhaustive over every `int64` in [-2^20, 2^20]; every 2^k + d and -(2^k) + d for k 0..63, d in [-1024, 1024]; every `double` 2^e for e -1074..1023 with both neighbours (`nextafter`) and sign; runs of zeros and ones of every length 1..63 (`1 0^r 1`, `1^r`); 10^7 random bit patterns per type, seed printed |
| T2 Order | For every adjacent pair of a sorted set of values (exact order, any mix of types), `memcmp` of the encodings agrees, and equal numbers of different types encode to equal bytes; 0 failures. Includes the old codec's failures: 2147483649 against 2147483650, -1073741824 against -1073741825, 1+2^-40 against 1 | Same test: the T1 set, each value also as every type that holds it exactly, sorted by an exact comparator that does not use the codec (integers through `__int128`, `double` against integer by exponent and significand), then adjacent pairs; plus 10^7 random cross-type pairs |
| T3 Exhaustive binary32 | Round trip and adjacent order over all 2^32 `float` values widened to `double` (NaNs to one) | `sortable/test/exhaustive_float.cc`, not in CI; the developer runs it once under `quiet light`, and the gate reruns it |
| T4 Decoder safety | Every input of 0 to 16 bytes is either rejected or re-encodes to itself; no read past the end; one CPU-hour of fuzzing with 0 findings, ASan and UBSan clean | `sortable/fuzz/decode_fuzz.cc` (libFuzzer: decode; when accepted, `encode(decoded) == input`; split input pairs and check `memcmp` against the exact comparator); `-max_total_time=3600 -timeout=5` at the gate's commit; exhaustive over all 1- and 2-byte inputs and the 3-byte inputs starting with an escape tag in the unit test |
| T5 Size | Value bytes and accuracy-term bytes on `accounts.ndjson` and `cities.ndjson` no larger than the old codec's, every field; today 36,230 B against 38,445 B and 109,069 B against 114,634 B | `python3 -I docs/design/sortable-codec/sizes.py PROBE docs/public/assets/{accounts,cities}.ndjson`, with PROBE built by `c++ -std=c++20 -O1 -I src -o PROBE docs/design/sortable-codec/legacy_probe.cc src/sortable_serialise.cc` at Xapiand:6c5c1a40d; after step 2, the same with `sortable` in place of `model.py` |
| T6 Two implementations agree | The C++ library's bytes equal `model.py`'s for the T1 boundary set and 10^5 seeded random values; the vectors file is committed as the format's fixture | `python3 -I docs/design/sortable-codec/model.py vectors > sortable/test/vectors.txt` (109,295 lines today), read by `sortable/test/vectors_test.cc` on every CI platform |
| T7 Xapiand answers exactly | Range, sort and aggregation answers equal brute-force truth over the old codec's failing values (2^k + d for k 31..63, -(2^a) - d for a 30..62, 1+2^-k), integer, positive, floating, datetime, both closed and half-open, single node; float bounds on integer fields and the global numeric slot included; 0 wrong | `harness/numeric_range_check.py` (from `fix/numeric-range`) extended with these values and a sort and a min/max aggregation check; `.scratch/value-compare/live_probe_uid.py` from the exploration, against the step-2 build |
| T8 Old indexes refused | An index written by the pre-switch binary (with documents, and with a non-empty WAL) fails to open, read or write with the D9 message, on a node and on a 2-node cluster; an empty unmarked shard is adopted; a new shard has the key from its first commit; a metadata write to `_value_codec` answers 400; DUMP with the old binary and RESTORE with the new gives identical documents and T7's answers | A harness script (`harness/value_codec_migration.sh`) with both binaries; a unit test in `tests/` for the adoption rule |
| T9 Nothing else regressed | Docs E2E suite every assertion; unit tests under ASan and UBSan; `harness/id_terms_check.py`; cluster and WAL recovery checks; raw slot or term bytes shown by docs examples (`:info` `values`, `terms`) updated | `harness/run_all.sh`; `harness/e2e_check.sh` |
| T10 No GPL in Xapiand's own tree | `src/sortable_serialise.*` gone; `grep -rn sortable_serialise src` outside `src/xapian/` finds nothing; ARCHITECTURE.md and `docs/dev/extraction-ledger.md` updated | the grep, added to `harness/seam_check.sh` |
| T11 Platforms | T1, T2, T4's exhaustive part and T6 pass on Linux x86_64 (GCC, Clang), Linux arm64, macOS arm64 and x86_64, FreeBSD, ASan and UBSan | `cpp-libraries/tools/preflight.sh` and the shared workflow |
| T12 Cost (the lead measures, on `main`) | Bulk indexing of the accounts corpus and a range, sort and aggregation query mix within noise of the pre-switch `main`, or better; the honest magnitude stated either way | `harness/loadtest.py` both modes, interleaved, under `quiet exclusive` |

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

## Steps

**Step 1. The library** (developer, branch and worktree in a new local `~/code/Kronuz/sortable`, not
pushed until the lead lands it). `sortable.hh` (D2-D5, D7), README, AGENTS.md, a how-it-works note
(ARCHITECTURE.md), the property tests (T1, T2), the exhaustive binary32 run (T3), the fuzz target and its
exhaustive short-input test (T4), the vectors fixture and `model.py vectors` (T6), the shared CI matrix
(T11). Done: T1, T2, T4 (exhaustive part and 10 minutes of fuzzing; the CPU-hour at the gate), T6 and T11
pass; T3 run once; a mutant per rule of D2 and D3 (the power-of-two tag order, the escape's fraction bit,
1 - f for negatives, the canonical last byte) each caught by a test; ASan and UBSan clean. Blocked by Q3
only for where it lives; the bytes are settled by D2-D5. Review: the security reviewer's decoder-safety
review before it lands; a code review.

**Step 2. Xapiand's switch** (developer, branch `fix/sortable-codec`, after `fix/numeric-range` lands and
step 1 is fetchable). D8 and D9: `FetchContent` of `sortable` pinned, every call in the table moved,
`src/sortable_serialise.*` deleted, the marker and its refusal, the metadata API guard, the D8 query
fixes, a changelog entry under `[Unreleased]` (exact numeric order; old indexes need DUMP and RESTORE),
ARCHITECTURE.md and the extraction ledger. Done: T5 (with `sortable`), T7, T8, T9, T10, each D8 fix with a
test that fails on master; a mechanism note `docs/mechanisms/value-codec.md`. Review: the architect's scoped
check of D9 (it decides what a node opens and replays), a code review; the lead measures T12 on `main`
after landing.

**Step 3. Follow-ups, outside this branch.** prospect's `ordered.hh` into `sortable` (Q3 (a)); the strata
PLAN and xapiand-on-strata notes of "Changes to other designs".

**Lead rulings (2026-10-27, on the designer's recommendations; reversible before release).**
Q1 (a): NaN in a document is rejected at indexing with 400. Q2 (a): -0.0 encodes as 0. Q3 (a): a
new `Kronuz/sortable` library now; prospect's `ordered.hh` moves into it later. Q4 (a): a `uint64`
of 2^63 or more in an integer field is rejected with 400.
