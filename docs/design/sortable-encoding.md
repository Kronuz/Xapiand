> Status: DRAFT (2026-11-03) designing (ready for preliminary review). Workstream sortable-encoding, branch `design/sortable-encoding` from Xapiand:432299ed0. Phase 8a's first item (`docs/status/road-to-release.md:32`). Adopts the bytes of `Kronuz/sortable` (sortable:67b2a90) and, on Q1 (a), supersedes the unlanded `docs/design/sortable-codec.md` revision 2 (branch `design/sortable-codec`).

# A clean-room, memcmp-ordered number encoding for Xapiand

Xapiand turns every number it sorts, ranges, or looks up by term into bytes whose `memcmp` order is the
numeric order. Today that is `sortable_serialise`, a GPL file of Xapian origin, and its Python twin in the
client. This design replaces both with MIT code: the `Kronuz/sortable` codec (exact, cross-type, 1 to 10
bytes) in C++, a clean-room Python port with a cross-language identity test, and a per-shard format marker
so a new binary refuses old bytes instead of misreading them.

**Recommendation.** Use the variable-length codec, not the "8-byte map" the plan names (Q1). No 8-byte code
can be exact across `int64`, `uint64` and `double` together, and Xapiand's slots and terms mix the three. On
430,166 real numbers the variable form averages 4.25 B per value; any 8-byte form is 88% larger per value,
26% larger per distinct value and 32% per distinct term (`sortable-encoding/sizes.txt`).

**Clean room.** Not opened: `src/sortable_serialise.{cc,h}`, anything of Xapian, and
`contrib/python/xapiand-py/xapiand/utils/sortable.py`. One `git grep` printed the lines of ARCHITECTURE.md
that name the old routine (`:44, 334, 336, 410, 579-580, 617-620`), two of them about its negatives;
nothing more of those sections was read. Sources: Xapiand's MIT callers, prospect
`ordered.hh` and phase-4 D3a, `sortable` and its model, the sortable-codec design and review, IEEE 754 and
Lucene's `NumericUtils`.

## Where numbers are encoded today

26 direct calls in 9 files, grep at Xapiand:432299ed0, excluding the two GPL files:

| Use | Sites | Type |
|---|---|---|
| Field values: integer, positive, floating, datetime (µs-rounded seconds) | `serialise.h:140, 172, 179, 186`; decoded at `:275, 292, 311, 316` | `int64`, `uint64`, `long double`, `double` |
| time and timedelta | `serialise.cc:1145, 1152, 1164, 1171` | `double` |
| Numeric document ids (`N` term, `DB_SLOT_ID`) | `database/shard.cc:113, 909`, `handler.cc:519, 1836`; literal zero `"N\x80"` at `handler.cc:428, 435, 2236`, `schema.cc:2985, 5291` | `double` |
| Versions (`DB_SLOT_VERSION`) | `shard.cc:99`, `handler.cc:340, 372, 1914`, `search_views.cc:1878, 2032`, `schemas_lru.cc:123`, `index_resolver_lru.cc:340`, `engine/strata/shard.cc:632` | `uint64`, or `double` (`strata/shard.cc:632`) |
| `get_hashed` | `database/utils.h:104`; no caller (only an include, `cast.h:28`) | `uint64` |

Everything else reaches the codec through `Serialise::`/`Unserialise::`: accuracy terms
(`generate_terms.cc`), sort keys and distances (`keymaker.cc`, sentinels `keymaker.h:47-48`), ranges
(`range.cc:92-121`), aggregations (`aggregations/metrics.h:159-189`). Nothing persists a decoded number;
the document's own MsgPack data stays the source of truth.

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
Lucene's exact `long`. Q1 asks the owner.

**D2. Variable width: the `Kronuz/sortable` codec, bytes unchanged.** The library is published, header-only,
MIT, CI-green on Linux x86_64 and arm64, macOS, FreeBSD and sanitizers (`sortable/docs/status/sortable.md`);
its bytes are fixed by `tools/model.py` and were validated by the architect's re-check of the sortable-codec
design. Real data, `sizes.py` (mean bytes; terms are the exact-value term plus Xapiand's accuracy terms):

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
values and distinct terms are what reach disk; the on-disk effect after strata's own compression is an
estimate until T7 measures it. Fixed width would buy a simpler decoder, but the variable one already
exists with exhaustive and fuzz proof.

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

Integers take their exact magnitude, a `double` its exact binary value (a subnormal normalises below
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

**D4. Values Xapiand will not store.** Carried from the lead's rulings on the sortable-codec design
(2026-10-27), reversible before release: a NaN field value answers 400; -0.0 indexes as 0; a `uint64` of
2^63 or more given to an integer field answers 400 instead of wrapping negative (`serialise.cc:440-441`). Q3.

**D5. The decoder is strict.** `sortable::decode` accepts exactly the outputs of `encode`: it rejects the
empty input, the unwritten tags, bytes after zero, NaN or an infinity, a missing or non-canonical escape
field, a tail over 8 bytes or ending in `00`, 64 fraction bits, and any value that is neither a `double` nor
an integer in [-2^63, 2^64) (nine rules, `sortable/docs/mechanisms/sortable.md`). It reads only inside its
input, allocates nothing, never throws. Its result converts exactly (`to_int64`, `to_uint64`, `to_double`)
or saturating (`saturate_int64`, `saturate_uint64`, today's `clamp_int64` semantics, `serialise.h:296-309`).
In Xapiand a rejected stored value raises `SerialisationError` naming the slot.

**D6. Xapiand's switch.** `Serialise::integer/positive/floating/timestamp` and the `Unserialise` twins call
`sortable`; the floating path narrows `long double` to `double` (field values are `f64` already,
`schema.cc:5359`). Every direct call in the first table moves to them, ids and versions encode their
integers directly, not through `double`. The `"N\x80"` literals stay correct (zero is `80`) and become a
named constant. `MAX_CMPVALUE`/`MIN_CMPVALUE` become `sortable::max_finite`/`min_finite`.
`get_hashed` is deleted. `src/sortable_serialise.{cc,h}` are removed with `git rm`, unopened; the
ARCHITECTURE.md, AGENTS.md, README.md and docs-site paragraphs about the old routine are replaced whole
from this design, not edited.

**D7. The Python client: a clean-room port.** `contrib/python/xapiand-py/xapiand/utils/sortable.py` is
removed with `git rm`, unopened, and a new module is written from D3 and D5 alone:
`encode(int | float) -> bytes` and `decode(bytes) -> int | float | None`. Integer arithmetic only: a
`float` is split with `struct.pack(">d")`, an `int` must lie in [-2^63, 2^64) (else `ValueError`), `bool`
raises `TypeError`. `decode` returns `int` for an integer in [-2^63, 2^64) and `float` otherwise (exact,
by D5's last rule), `None` for anything D5 rejects. T3 proves its bytes equal the C++ library's. The
exported names change with it (Q2).

**D8. A format marker per shard.** Formats are open until the release, so old bytes are not upgraded; they
are refused. Every shard carries metadata `_format`, Xapiand's index-format generation: absent means 0
(`sortable_serialise` bytes), the switch writes `"1"`, and any later pre-release byte change bumps it. A build
accepts only its own generation. `ShardEndpoint::open_index_blocking` writes it through the engine
(`engine/shard.h:118`) and commits it before the open returns when the open created or adopted the shard
(`Opened::created`, `engine/open.h:44`, after `database/shard.cc:494`), so no WAL record ever precedes a
shard's marker; it checks the marker before the WAL replay (`:515`); another generation on a shard with
documents or a WAL refuses the open with "index X was written by format generation N; dump it with the
build that wrote it and restore it with this one", and an empty shard is adopted. The HTTP metadata API
(`search_views.cc:1171` guards only `schema`, `wal`, `metrics`) also refuses writes to `_format`.
There are no replicas or remote shards before Phase 7b, so the single open path covers every shard.

**Migration cost.** Xapian-era data (alpha.9, alpha.10) is already refused at startup and moves by dump and
restore (`changelog.md:113-116`); dumps hold typed MsgPack and carry `_version` (`:182`), so this design adds
nothing there. Strata indexes written before the switch (developer data: no release shipped them) are
refused by D8 and move the same way: one dump with the old build and one restore, at the restore's
indexing rate (T7 measures it).

## Changes to other designs

1. prospect `docs/design/phase-4.md` D3a ("Xapiand's replacement", `:149-155`), strata `docs/PLAN.md:679-680`
   and `docs/status/road-to-release.md:32` name "the 8-byte map"; on Q1 (a) they name `Kronuz/sortable`.
   D3a's typed columns keep the fixed per-type maps, which are column keys, not byte strings.
2. `docs/design/sortable-codec.md` revision 2: D1-D7 stand (this design adopts them); D9-D11 (in-place
   upgrade of Xapian shards, cluster and remote-protocol bumps) are obsolete since X1 deleted Xapian and the
   cluster; D8 here replaces its D9 marker. The lead relabels it SUPERSEDED on Q1 (a).
3. The license audit: `contrib/python/xapiand-py/setup.py:24` says Apache 2.0 while the package's headers
   say MIT.

## Gate rows

| Row | Target | Procedure |
|---|---|---|
| T1 Order | `memcmp` agrees with an exact comparator on every adjacent pair, any mix of types; equal numbers give equal bytes; 0 failures | `sortable_property` (`sortable/test/property.cc`): edge set (2^k + d for k 0..63, d in [-1024, 1024], both signs; every 2^e for e -1074..1023 with `nextafter` neighbours; fractions of every length at the window edges; runs `1 0^r 1`, `1^r`) plus 10^7 random values per type and 10^7 cross-type pairs, seed printed |
| T2 Round trip | `decode(encode(x)) == x` exactly (-0.0 to 0, NaN to one NaN) on T1's set; all 2^32 binary32 values widened; 0 failures | `sortable_property`; `cmake -DSORTABLE_EXHAUSTIVE=ON` then `sortable_exhaustive_float` once under `quiet light` |
| T3 C++ equals Python | The client's `encode` reproduces every line of `sortable/test/vectors.txt` (113,491); for 10^6 seeded random inputs (half integers in [-2^63, 2^64), half random binary64 bit patterns, passed as hex bits) the C++ and Python bytes are identical; `decode` accepts and rejects the same inputs over every 0-2 byte string, every 3-byte string starting with an escape tag, `test/reject.txt`, and 10^6 seeded mutations of valid encodings, with equal results | `contrib/python/xapiand-py/tests/test_sortable.py` (vectors, rejects); `harness/sortable_xlang.py SORTABLE_CLI` drives a new `sortable/tools/cli.cc` (`encode`/`decode` over stdin lines) against the client and prints the first mismatch. Home: Xapiand's CI, on every push. sortable gains a `SORTABLE_BUILD_TOOLS` option so the CLI builds as a subproject; a CTest asserts the sha256 of the pinned `vectors.txt` and `reject.txt`, runs the C++ encoder and the Python client over both, and runs the CLI comparison |
| T4 Decoder fuzz | One CPU-hour, 0 crashes, 0 accepted inputs that fail to re-encode to themselves, ASan and UBSan clean | `sortable_fuzz -max_total_time=3600 -timeout=5` (`sortable/fuzz/decode_fuzz.cc`) at the gate's commit, under `quiet light` |
| T5 Same answers | Docs E2E: every assertion passes and response bodies equal the pre-switch run except raw-byte fields; real corpus: every sort and range answer equals the pre-switch answer, or differs only where brute-force truth says the old one was wrong | `harness/e2e_capture.sh` with each binary, then `harness/bodydiff.py`; new `harness/sort_range_identity.py OLD NEW` restores accounts, cities and a 200,000-file metadata corpus, runs ascending and descending sorts per numeric and datetime field and 1,000 seeded ranges, compares hit ids, and checks each difference against brute force |
| T6 Exact at the old failures | Range, sort and min/max answers equal brute force on 2^k + d (k 31..63), -(2^a) - d (a 30..62), 1 + 2^-k, ids and versions above 2^53; float bounds on integer fields; the global numeric slot; 0 wrong | `harness/numeric_range_check.py` extended with these values, a sort check and an aggregation check |
| T7 Size and cost (the lead, on `main`) | On-disk index size and the bulk-index plus query mix before and after, the honest magnitude either way; the restore rate that prices the migration | `harness/loadtest.py` both builds, interleaved, under `quiet exclusive`; `du` of the data directory after `FLUSH` |
| T8 Marker | A pre-switch index (generation 0) with documents, and one with only a WAL, is refused by the switch build with the D8 message, and the switch's index by a step-2 build; an empty unmarked shard is adopted; a new shard has a committed `_format` when the open returns; a switch-build shard that crashes after a write and before any explicit commit reopens under the switch build and is refused by a step-2 build (executable model: review job 261110-034552-09edf8); `_format = "2"` is refused; a metadata write of `_format` answers 400; RESTORE into a fresh index marks every shard | `harness/format_marker_check.sh OLD NEW`; unit tests for the adoption rule |
| T9 No GPL | No `sortable_serialise` in `src/` or `contrib/`; both GPL files gone; ARCHITECTURE and the license note updated | `git grep -n sortable_serialise -- src contrib` prints nothing, added to `harness/seam_check.sh` |
| T10 Platforms | T1, T2's CI part, T3 and the unit tests pass on every CI platform and sanitizer job | each repository's CI; `cpp-libraries/tools/preflight.sh` for `sortable` |

## Open questions for the owner

**Q1. Width.** (a) Variable, `Kronuz/sortable` (D2): exact, cross-type, 4.25 B per value on real data.
(b) 8 bytes through `f64` for every number: one shape with strata's `f64` columns, but integers above 2^53
merge and values cost 1.88x. (c) 8 bytes per type, with Xapiand's shared numeric slot, ctype and accuracy
prefixes split by type: exact, but a field typed integer in one index and floating in another no longer
sorts as one, which narrows wide searches. Recommendation: (a).

**Q2. The Python client's names.** (a) `xapiand.utils.sortable.encode` and `decode`, the C++ names, and the
old exports deleted (no caller in the client). (b) Keep `sortable_serialise`/`sortable_unserialise` with
the new bytes. Recommendation: (a): the bytes change anyway; old names would promise a compatibility they lack.

**Q3. D4's carried rulings.** Confirm NaN and an out-of-range integer answer 400 and -0.0 indexes as 0.
Recommendation: confirm; JSON has no NaN, and the alternative to the 400 is a silently negative number.

## First steps

1. **The library's remaining proof** (developer, `sortable` repository; starts now): T2's exhaustive
   binary32 run, T4's CPU-hour, and `tools/cli.cc` for T3. Review: the security reviewer's decoder-safety
   review (the floor for a decoder on users' disks).
2. **One seam and the marker** (developer, branch `phase8/sortable-encoding`; starts now, no byte change):
   move every direct call of the first table behind `Serialise::`/`Unserialise::` without opening the GPL
   files, delete `get_hashed`, name the zero literal, and build D8 at generation 0 with T8's cases that
   need no switch.
3. **The switch** (same branch, after Q1): D5-D7 with T1-T6, T9 and T10; `git rm` both GPL files and the
   Python module; the clean-room Python port and T3's harness; changelog `[Unreleased]` (exact numeric order;
   earlier strata indexes need a dump and restore); a how-it-works note `docs/mechanisms/value-encoding.md`;
   the extraction ledger row. Review: one code review; the lead measures T7 on `main`.

## Limits

1. Silent: a build older than D8 opening a generation-1 index reads wrong numbers with no error (a
   downgrade); the changelog says a downgrade is a dump and a restore. Step 2 lands before step 3 so every
   build from then on checks.
2. The tail is not prefix-free; a caller concatenating encodings must frame them (the key maker does).
3. Doubles below 2^-60 take the 3-byte escape (up to 10 B); none of the real sets holds one.
