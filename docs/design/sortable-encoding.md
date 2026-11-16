> Status: ACCEPTED (2026-11-16), revised for the owner's D87; the design re-check approved it at Xapiand:3f95a5edd. Workstream sortable-encoding, quest branch `quest/xapiand/sortable-encoding`. Phase 8a's first item (`docs/status/road-to-release.md:32`). Adopts the bytes of `Kronuz/sortable` (pinned at sortable:915b014; the bytes are those of sortable:67b2a90) and supersedes the unlanded `docs/design/sortable-codec.md` revision 2 (branch `design/sortable-codec`).

# A clean-room, memcmp-ordered number encoding for Xapiand

Xapiand turns every number it sorts, ranges, or looks up by term into bytes whose `memcmp` order is the
numeric order. Today that is `sortable_serialise`, a GPL file of Xapian origin, and its Python twin in the
client. This design replaces both with MIT code: the `Kronuz/sortable` codec (exact, cross-type, 1 to 10
bytes) in C++ and a clean-room Python port with a cross-language identity test. There is no format marker
and no compatibility with earlier bytes: formats are open before the release (owner, D87).

**The owner's answers (D87).** Q1: the variable-length codec, held to a speed gate (it must not become a
bottleneck). Q2: Python names that read well, with no aliases for the old ones. Q3: confirmed, NaN and an
out-of-range integer answer 400 and -0.0 indexes as 0. No marker and no compatibility code. D91 puts the
cluster before the format freeze; this encoding stays open until then and is embedded in the future journal
commands.

**Why variable.** No 8-byte code can be exact across `int64`, `uint64` and `double` together, and Xapiand's
slots and terms mix the three. On 430,166 real numbers the variable form averages 4.25 B per value; any
8-byte form is 88% larger per value, 26% larger per distinct value and 32% per distinct term
(`sortable-encoding/sizes.txt`).

**Clean room.** Not opened: `src/sortable_serialise.{cc,h}`, anything of Xapian, and
`contrib/python/xapiand-py/xapiand/utils/sortable.py`. One `git grep` printed the lines of ARCHITECTURE.md
that name the old routine (`:44, 334, 336, 410, 579-580, 617-620`), two of them about its negatives;
nothing more of those sections was read. Sources: Xapiand's MIT callers, prospect
`ordered.hh` and phase-4 D3a, `sortable` and its model, the sortable-codec design and review, IEEE 754 and
Lucene's `NumericUtils`.

## Where numbers are encoded today

Direct calls at Xapiand:cd9c216b5, excluding the two GPL files. Ids already use `sortable` (exact-id-keys,
Xapiand:73fbcb48d: `Serialise::id`, `Unserialise::id`).

| Use | Sites | Type |
|---|---|---|
| Field values: integer, positive, floating, datetime (µs-rounded seconds) | `serialise.h:141, 173, 180, 187`; decoded at `:292, 309, 328, 333` | `int64`, `uint64`, `long double`, `double` |
| time and timedelta | decoded at `serialise.cc:1228, 1235, 1247, 1254` | `double` |
| Versions (`DB_SLOT_VERSION`) | `database/shard.cc:96`, `handler.cc:323, 491, 2048`, `search_views.cc:1904, 2059`, `schemas_lru.cc:124`, `index_resolver_lru.cc:340`, `engine/strata/shard.cc:634` | `double` |
| `get_hashed` | `database/utils.h:103-104`; no caller (only an include, `cast.h:28`) | `uint64` |
| Sort sentinels | `multivalue/keymaker.h:47-48` | `double` |

Everything else reaches the codec through `Serialise::`/`Unserialise::`: accuracy terms
(`generate_terms.cc`), sort keys and distances (`keymaker.cc`), ranges (`range.cc:92-121`), aggregations
(`aggregations/metrics.h`). Nothing persists a decoded number; the document's own MsgPack data stays the
source of truth.

The types meet in one order in four places:

1. The global numeric slot `DB_SLOT_NUMERIC` (`database/utils.h:52`) serves floating, integer and positive
   (`schema.cc:2293-2301`).
2. integer, positive and floating share ctype `N` (`schema.h:421-424`), and the slot is
   `xxh32(prefix, ctype)` (`database/utils.cc:102-104`): one field typed integer in one index and floating
   in another sorts and ranges as one slot across a wide search.
3. A floating field stores integers (`serialise.cc:417-418`); floats' accuracy terms are truncated to
   `int64` (`schema.cc:5359, 5365`) and share the global accuracy prefixes with integer and positive terms
   (`generate_terms.cc:345-375`), matched by exact term equality.
4. The Python client exports `sortable_serialise` and `sortable_unserialise` as public API
   (`contrib/python/xapiand-py/xapiand/utils/__init__.py:26, 37-38`); nothing in the client calls them.

## Decisions

**D1. One exact number space across `int64`, `uint64` and `double`.** Equal numbers encode to equal bytes
whatever their type (5, `5u` and 5.0 are `c040`), and `memcmp` order is numeric order across types. The
"8-byte map" cannot provide this: `int64` and `uint64` together hold 2^63 + 2^64 distinct integers, more
than 2^64 codes, so any 8-byte encoding of both is lossy or orders them in separate spaces. prospect's
per-type maps (`ordered.hh:34, 38`: `i64` sign-flipped, `f64` Lucene-style) encode 5 as `8000000000000005`
and 5.0 as `c014000000000000`, breaking the first three meeting points above; mapping everything through `f64`
keeps the order but merges integers above 2^53 (snowflake ids, nanosecond times, hashes), short of
Lucene's exact `long`.

**D2. Variable width: the `Kronuz/sortable` codec, bytes unchanged.** The library is published, header-only,
MIT, CI-green on Linux x86_64 and arm64, macOS, FreeBSD and sanitizers (`sortable/docs/status/sortable.md`);
its bytes are fixed by `tools/model.py`, and `test/vectors.txt` and `test/reject.txt` have the sha256
`c4d4a425c09e6d3e...` and `a679d215ec843a0f...` from sortable:67b2a90 to sortable:915b014. Real data,
`sizes.py` (mean bytes; terms are the exact-value term plus Xapiand's accuracy terms):

| Set | Values | Value B | Distinct value B | Distinct term B |
|---|---|---|---|---|
| accounts._id, accountNumber, age (int) | 3,000 | 1.95 to 3.74 | 1.96 to 3.74 | 1.79 to 3.37 |
| accounts.balance (2-decimal float) | 1,000 | 7.82 | 7.82 | 7.40 |
| cities._id, population (int) | 2,000 | 2.23, 3.11 | 2.23, 3.11 | 2.22, 3.06 |
| cities coordinates (float) | 2,000 | 7.94 | 7.94 | 7.90 |
| file sizes, 200,000 real files (int) | 200,000 | 2.18 | 2.95 | 3.05 |
| file mtimes (datetime, µs) | 200,000 | 6.23 | 7.58 | 7.56 |
| git commit times, Kronuz repos (datetime, s) | 22,166 | 4.99 | 4.99 | 4.89 |
| **All** | **430,166** | **4.25** | **6.37** | **6.08** |

A fixed 8 bytes is 1.88x the variable bytes per value, 1.26x per distinct value and 1.32x per distinct
term; an exact fixed 10 bytes (sign, 15-bit exponent, 64-bit significand) is 2.35x, 1.57x and 1.65x.
strata stores a `bytes` slot as a per-segment sorted dictionary plus ordinals (phase-4 D3a), so distinct
values and distinct terms are what reach disk.

*Speed (D87's gate).* Stored values decode with the trusted typed conversions (`sortable::to_double`,
`to_int64`, `to_uint64` on bytes), which keep the read bounds and skip the canonical checks; bytes from
outside the program (ids in a request, user-supplied serialized values) use the strict `decode`. sortable's
`docs/perf/` measures both against the old codec's public API on a frozen real-value set
(`data/real-values.tsv.gz`, 434,977 values); T8 repeats it in Xapiand at named commits.

**D3. The encoding.** A finite nonzero |v| = 2^E x (1 + f), f with L <= 63 fraction bits. The first byte is a
tag, then a 2-byte escape field (extreme exponents only), then the tail: n = ceil(L/8) bytes of f x 2^(8n),
big-endian, last byte nonzero, empty for a power of two.

| Input | Bytes | Example |
|---|---|---|
| 0, -0.0, +0.0 | `80` | `80` |
| Positive, E = -60..63 (every integer, most doubles) | tag `0x82 + E + 60`, tail of f | 1 `be`, 5 `c040`, 1.5 `be80`, 0.01 `b747ae147ae147b0`, 2^64-1 `fdfffffffffffffffe` |
| Positive double, E < -60 (incl. all subnormals) | `81`, field `E + 1074`, tail | 5e-324 `810000`, DBL_MIN `810034` |
| Positive double, E >= 64 | `fe`, field `E - 64`, tail | DBL_MAX `fe03bffffffffffffff0` |
| Negative | U = the exponent of the least power of two >= |v|; tag `0xFF -` positive tag of U; field `0xFFFF -` field; tail 2^(8n) - F (F the positive tail's n bytes), empty when |v| = 2^U | -1 `41`, -5 `3ec0`, -1.5 `4080`, -2^63 `02`, -5e-324 `7effff` |
| +inf, -inf | `feffff`, `010000` | |
| NaN (any payload or sign) | `ff` | |

Integers take their exact magnitude, a `double` its exact binary value (a subnormal normalizes below
E = -1022). `0x00` and `0x7f` are never written, so nothing collides with StringList's `0x00` marker.

*Order, proof sketch.* (1) Tags rise with the exponent for positives, `0x82..0xFD` between the escapes
`0x81` and `0xFE`, whose big-endian fields rise with E; +inf takes the largest field. (2) Inside a tag,
values compare by f; a shorter tail equals the longer one padded with zeros, and `memcmp` sorts a prefix
first, so tail order is f's order. (3) Negatives mirror the tag and field, so a larger magnitude has a
smaller tag; inside tag U the magnitudes lie in (2^(U-1), 2^U] and 1 - f falls as the magnitude rises.
2^(8n) - F keeps the length n and a nonzero last byte (its lowest set bit is F's), so step 2 applies
reversed, and -(2^U), the most negative value of its tag, has the empty tail and sorts first. (4) Every
negative tag is below `0x80` and every positive above; NaN is `0xFF`, above +inf. Equal numbers have one
exponent and one fraction, so one encoding, whatever their type. The encoding is not prefix-free; the key
maker already frames concatenated keys (`0x00` escaped).

**D4. Values Xapiand will not store (D87 Q3).** A NaN field value or range bound answers 400; -0.0 indexes
as 0; a `uint64` of 2^63 or more given to an integer field answers 400 instead of wrapping negative
(`serialise.cc:440-441`).

**D5. The decoder is strict.** `sortable::decode` accepts exactly the outputs of `encode`: it rejects the
empty input, the unwritten tags, bytes after zero, NaN or an infinity, a missing or non-canonical escape
field, a tail over 8 bytes or ending in `00`, 64 fraction bits, and any value that is neither a `double` nor
an integer in [-2^63, 2^64) (nine rules, `sortable/docs/mechanisms/sortable.md`). It reads only inside its
input, allocates nothing, never throws. Its result converts exactly (`to_int64`, `to_uint64`, `to_double`)
or saturating (`saturate_int64`, `saturate_uint64`, today's `clamp_int64` semantics, `serialise.h:313-326`).
In Xapiand a rejected value raises `SerializationError` naming it.

**D6. Xapiand's switch.** `Serialize::integer/positive/floating/timestamp` and the `Unserialize` twins call
`sortable`, and every direct call in the first table moves to them; versions encode their `uint64` directly,
not through `double`, through `Serialize::version`/`Unserialize::version`, which the engine adapter
(`engine/strata/shard.cc`) also calls. The floating path is `double` end to end: the `long double` overloads
go, and a string parses with strict_stod, so one decimal string gives one value on every platform and
equals the same number sent as JSON or as a range bound (review F3). An absent range bound is unbounded: it
encodes as -inf or +inf for every numeric type, so `x >= 5` includes +inf and a range over the shared slot
answers the same whatever the query's field type (F4c). `MAX_CMPVALUE`/`MIN_CMPVALUE` become
`sortable::max_finite`/`min_finite`. `get_hashed` is deleted. `src/sortable_serialise.{cc,h}` are removed
with `git rm`, unopened; the ARCHITECTURE.md, AGENTS.md, README.md and docs-site paragraphs about the old
routine are replaced whole from this design, not edited.

**D7. The Python client: a clean-room port.** `contrib/python/xapiand-py/xapiand/utils/sortable.py` is
removed with `git rm`, unopened, with its two exports. The port written from D3 and D5 alone (branch
`codex/py-sortable`, Xapiand:f4b0ee4fa) becomes `xapiand/sortable.py`: `encode(int | float) -> bytes` and
`decode(data: bytes, kind: type[int] | type[float])`. Integer arithmetic only; an `int` must lie in
[-2^63, 2^64) (else `ValueError`), `bool` raises `TypeError`; `decode` raises `ValueError` for anything D5
rejects and for a value that is not exactly an integer when `kind` is `int`, and rounds to nearest even for
`float`, as `to_double` does.

**D8. No format marker, no compatibility (D87).** A build reads only the bytes it writes. Indexes written
before the switch (developer data: no release shipped them) are deleted and re-created, or dumped with the
old build and restored with the new one; dumps hold typed MsgPack (`changelog.md:182`), so they cross the
switch unchanged. Any later pre-release byte change does the same. Xapian-era data (alpha.9, alpha.10) is
already refused at startup and moves by dump and restore (`changelog.md:113-116`). With no marker, the
design review's F1 (the marker's crash window), F8, F9 and F10 have nothing to apply to, and the marker's
gate row is gone (T8 is now speed).

## Gate rows

T1 and T3 run in Xapiand's own CTest, so every CI platform and sanitizer job runs them on every push (review
F2). Both read the fetched library's fixtures, never a copy, and assert their sha256 (D2), so a pin bump or
a client edit that changes bytes fails a check.

| Row | Target | Procedure |
|---|---|---|
| T1 Order | `memcmp` agrees with an exact comparator on every adjacent pair, any mix of types; equal numbers give equal bytes; 0 failures | CTest `sortable_property`, built from `${sortable_SOURCE_DIR}/test/property.cc`: the edge set (2^k + d for k 0..63, d in [-1024, 1024], both signs; every 2^e for e -1074..1023 with `nextafter` neighbours; fractions of every length at the window edges) plus 3x10^6 random values with a fixed, printed seed (F11). A Xapiand unit test runs every line of `vectors.txt` through `Serialize::`/`Unserialize::` and checks the bytes, adjacent order and cross-type equality |
| T2 Round trip | `decode(encode(x)) == x` exactly (-0.0 to 0, NaN to one NaN) on T1's set; all 2^32 binary32 values widened; 0 failures | sortable's own CI and its exhaustive run (done at sortable:937c7c6; the bytes are unchanged since) |
| T3 C++ equals Python | The client's `encode` reproduces every line of `vectors.txt` (113,491); for 10^6 seeded random inputs (half integers with log-uniform magnitudes in [-2^63, 2^64) (F12), half random binary64 bit patterns) the C++ and Python bytes are identical; `decode` accepts and rejects the same inputs over every 0-2 byte string, every 3-byte string starting with an escape tag, `reject.txt`, and 10^5 seeded mutations of valid encodings, with equal results | CTest `sortable_xlang`: builds `sortable_cli` from `${sortable_SOURCE_DIR}/tools/cli.cc` and runs `tests/sortable_xlang.py SORTABLE_SOURCE_DIR SORTABLE_CLI` (stdlib only, the client module loaded by path) |
| T4 Decoder fuzz | One CPU-hour, 0 crashes, 0 accepted inputs that fail to re-encode to themselves, ASan and UBSan clean | sortable's own (done at 2e85e15 and, with the trusted conversions, at 2299f14); rerun if `sortable.hh` changes |
| T5 Same answers | Docs E2E: every assertion passes and response bodies equal the pre-switch run except raw-byte fields; real corpus: every sort and range answer equals the pre-switch answer, or differs only where brute-force truth says the old one was wrong | `harness/e2e_capture.sh` with each binary, then `harness/bodydiff.py`; `tests/sort_range_identity.py OLD NEW` restores accounts, cities and a frozen file-metadata ndjson (digest checked, F6), runs ascending and descending sorts per numeric and datetime field, 1,000 seeded ranges and a seeded geo set over cities (F5), compares hit ids, and checks each difference against brute force |
| T6 Exact at the old failures | Range, sort and min/max answers equal brute force on 2^k + d (k 31..63), -(2^a) - d (a 30..62), 1 + 2^-k, negative fractions (pre-1970 datetimes with sub-second parts), ids and versions above 2^53; float bounds on integer fields; open bounds with ±inf values; one field typed integer in one index and floating in another under one wide search; the global numeric slot; NaN bounds answer 400; 0 wrong (F4) | `tests/numeric_range_check.py` (the harness check extended), a sort check and an aggregation check |
| T7 Size and cost (the lead, on `main`) | On-disk index size and the bulk-index plus query mix before and after, the honest magnitude either way, the disk delta in dollars; the restore rate that prices re-creating an index | `harness/loadtest.py` both builds, interleaved, under `quiet exclusive`; `du` of the data directory after `FLUSH` of the restored accounts, cities and the frozen file corpus, first on the branch (F7) |
| T8 Speed (D87) | Encode and decode ns per value at or below the old codec on the real-value set; indexing, sort and range throughput at or above master's, outside the A/A band only if faster | a Xapiand micro-benchmark over `Serialize::`/`Unserialize::` and `harness/loadtest.py`, under `quiet exclusive`, at named commits |
| T9 No GPL | No `sortable_serialise` in `src/` or `contrib/`; both GPL files gone; ARCHITECTURE and the license note updated | `git grep -n sortable_serialise -- src contrib` prints nothing (the quest's `no-gpl` check) |
| T10 Platforms | T1, T3 and the unit tests pass on every CI platform and sanitizer job | each repository's CI |

## Changes to other designs

1. prospect `docs/design/phase-4.md` D3a ("Xapiand's replacement", `:149-155`), strata `docs/PLAN.md:679-680`
   and `docs/status/road-to-release.md:32` name "the 8-byte map"; they name `Kronuz/sortable`. D3a's typed
   columns keep the fixed per-type maps, which are column keys, not byte strings; D3a should say which
   Xapiand fields, if any, use them, so no later quest adds a third number encoding.
2. `docs/design/sortable-codec.md` revision 2 is SUPERSEDED by this design (the lead relabels it).
3. The license audit: `contrib/python/xapiand-py/setup.py:24` says Apache 2.0 while the package's headers
   say MIT.

## Steps

1. **The library's proof** (done in `sortable`: T2's exhaustive run, T4's CPU-hours, `tools/cli.cc`). In
   Xapiand: T1's and T3's CTests and the Python port, with no byte change.
2. **One seam** (no byte change): every direct call of the first table behind `Serialise::`/`Unserialise::`
   (versions through `version`), `get_hashed` deleted.
3. **The switch** (after the design review confirms this revision): D4-D7 with T1-T6, T8 and T9; `git rm`
   both GPL files and the Python module; the American-spelling rename (D88); changelog `[Unreleased]`
   (exact numeric order; earlier strata indexes are re-created or restored); a how-it-works note
   `docs/mechanisms/value-encoding.md`; the extraction ledger row. Review: one code review; the lead
   measures T7 on `main`.

## Limits

1. Silent: a build from before the switch opening an index written after it, or the reverse, reads wrong
   numbers with no error. Accepted before the release (D87); the changelog says such an index is
   re-created or restored.
2. The tail is not prefix-free; a caller concatenating encodings must frame them (the key maker does).
3. Doubles below 2^-60 take the 3-byte escape (up to 10 B); none of the real sets holds one.
