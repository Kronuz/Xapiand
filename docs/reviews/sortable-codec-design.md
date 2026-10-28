# Xapiand review: sortable codec design (preliminary)

- Date: 2026-10-27.
- Reviewed: `docs/design/sortable-codec.md` revision 0 with the lead's rulings on Q1 to Q4, its
  model and probes in `docs/design/sortable-codec/` (`model.py`, `sizes.py`, `legacy_probe.cc`), and
  the ledger `docs/status/sortable-codec.md`, all at Xapiand:22418c6b8 (branch
  `design/sortable-codec`, local only). Background: `docs/dev/value-compare.md` section 1.
- Checked against: the design's D1 to D10, rows T1 to T12, steps 1 to 3; `AGENTS.md` (load-bearing
  invariants, WAL replay idempotence); the code paths the design cites in `src/multivalue/keymaker.cc`,
  `src/serialise_list.h`, `src/multivalue/generate_terms.cc`, `src/database/{schema,shard,handler}.cc`,
  `src/server/{replication_protocol_views,search_views,discovery}.cc`, `src/index_resolver_lru.cc`,
  `src/manager.cc`; `docs/src/content/docs/reference-guide/indexes/backups.md`.
- Clean room: `src/sortable_serialise.{cc,h}`, `src/xapian/` and `src/server/remote_protocol_views.cc`
  were not opened. The old codec was compiled from its files and called as a black box; one grep
  printed a line of `remote_protocol_views.{h,cc}` (protocol constants and class names), nothing more.
- Checked by: probes in `.scratch/sortable-codec-arch/` (kept), each run under
  `quiet light codec-arch`, Python 3.15 and Apple clang (C++20):
  - `p1_order.py`: an independent encoder written from D2 and D3's text, checked against
    `model.encode` and an exact comparator on 196,961 typed values (adversarial set plus 60,000
    random); output `p1_order.out`.
  - `p2_decoder.py` (output `p2_decoder.out`): D5 strictness over 25,430,429 inputs: every input of 0
    to 3 bytes, every escape tag with every 2-byte field and 12 tails, every tag with 0 to 10 tail
    bytes, bit flips, truncations and extensions of 40,013 valid encodings, and 10^6 random inputs of
    4 to 16 bytes.
  - `p3_sizes.py` (output `p3_sizes.out`): the size row on 13 distributions the design did not
    sample, with datetime accuracy terms built the way Xapiand builds them.
  - `p4_alt.py` (output `p4_alt.out`): the alternative tag layout of finding 1, checked for order,
    strictness and size.
  - `p5_overloads.cc` (output `p5_overloads.out`): D7's overload set called with ordinary integer
    types, syntax-only compiles.
  - `legacy_probe` (built from `docs/design/sortable-codec/legacy_probe.cc` and
    `src/sortable_serialise.cc` at 22418c6b8, which matches 6c5c1a40d for those files) and
    `legacy_facts.out`; `sizes_rerun.out` is `sizes.py` rerun with it.
  - Not run: no Xapiand build, no unit tests, docs E2E or harness runs, no fuzzing, no live node or
    cluster, no benchmarks; the arm64 `long double` question in D5 was not checked; no host other than
    this Mac was touched.

## Verdict

**REJECT: the format is correct but not yet the one to freeze on disk, and D9's migration does not
cover what an upgraded node opens first.**

D1 to D5 hold as written: exact, total, strict, and never `0x00` first. What should change before step
1 writes its vectors fixture is the tag layout. Spending two tags per exponent on the power-of-two bit
leaves the direct range at 2^-4 to 2^57, so every 64-bit identifier and every value below 1/16 pays a
3-byte escape. A layout with one tag per exponent keeps every rule and is no larger for any value
(finding 1). D6's "6% smaller" holds only on the two sample corpora (finding 2). D9's
refuse-and-rebuild rule is the right mechanism and its necessity is proven, but as written it bricks
the node that applies it (finding 3) and loses what DUMP does not carry (finding 4).

What holds, and reproduces:

- Order and exactness. `p1_order.py`: the encoder written from D2 and D3's text gives the same bytes as
  `model.encode` for all 196,961 typed values; 0 order failures between adjacent values under an exact
  comparator that does not use the codec; 0 cases where equal numbers of different types (int64,
  uint64, double) encode differently; no encoding starts with `0x00`; the longest is 11 bytes. The set
  covers 2^k + d for k in {30, 31, 32, 52, 53, 54, 57, 58, 62, 63, 64}, d in [-40, 40], both signs;
  the double neighbours of each 2^k; every exponent from -8 to -2 and 54 to 61 (both sides of both
  escapes) with fraction lengths 0, 1, 7, 8, 9, 15, 16, 17, 52, 53, 56, 63 in four bit patterns;
  subnormals, the smallest normal, `DBL_MAX`, ±inf, NaN, -0.0; the old codec's failures
  (-(2^a) - d for a 30 to 62; 2147483649 against 2147483650; 1 + 2^-40).
- Decoder strictness. `p2_decoder.py`: 25,430,429 inputs, 11,187,633 accepted (counted with
  repeats), 0 that do not re-encode to themselves. The unique counts match the layout: 126 accepted
  1-byte inputs (zero, NaN, 62 positive and 62 negative powers of two) and 31,620 2-byte inputs
  (124 fraction tags x 255 nonzero tails).
- The multi-value marker. `SERIALISED_LIST_MAGIC` is `'\0'` (`src/serialise_list.h:33`); D2 never
  writes `0x00` first, and -inf (old: the empty string, `legacy_facts.out`) is now `010000`.
- Concatenation. The only place encodings are concatenated is the multi-key sort key, and it frames
  each key the Xapian way: `0x00` escaped and `00 00` terminated forwards, bytes complemented and `ff ff`
  terminated in reverse (`src/multivalue/keymaker.cc:760-783`). Distance sorts walk a multi-value list
  by `memcmp` of its elements against the encoded reference (`keymaker.cc:150-162`), which D1 to D3
  keep valid. Nothing needs prefix-freeness.
- D9's premise. Old bytes decode under the new decoder as other numbers with no error: old 1 (`be`) as
  67108864, old 2147483647 (`c5ffffffff`) as 8589934591/8 (`model.decode`); so a marker, not a
  per-value check, is the only way to tell them apart.
- The old codec's failures reproduce as black-box facts (`legacy_facts.out`): -(2^30) is `3a20`, a
  prefix of -(2^30) - 1 `3a2000000180`; 2147483649 is `c60080`, after 2147483650 `c600000001`; +inf is
  17 bytes of `ff`; -0.0 is `80`.
- `sizes.py` reproduces exactly with an independently built probe (`sizes_rerun.out`): values 36,230 B
  against 38,445 B, accuracy terms 109,069 B against 114,634 B over 48,000 terms.
- The call-site census holds: 24 direct calls in 8 files outside `src/engine/xapian/`, plus 5 in
  `src/engine/xapian/{shard,convert}.cc`, 10 files in all (`grep`); `remote_protocol_views.cc` has none.

What blocks: findings 1 and 2 (step 1: the bytes and the size claim), findings 3 to 5 (step 2: D9).

## Findings

**1. The power-of-two tag bit halves the direct range, and a layout without it is no larger for any
value.** D2 gives each exponent two tags (power of two, then fraction) so that a negative power of two
sorts above the negatives of its exponent (D3). With 124 direct tags that leaves E = -4 to 57, and
everything outside takes the 3-byte escape: every integer of 2^58 or more is 11 bytes, every |v| below
1/16 is 3 + n bytes (0.01 is `81085747ae147ae147b0`, 10 bytes; 0.003 is 10 bytes). Measured in
`p3_sizes.out` against the old codec: snowflake-style ids in [2^60, 2^62) 10.95 B against 8.99 B
(22% larger), random uint64 hashes 10.93 B against 9.24 B (18% larger), and their accuracy terms
10.30 B against 8.61 B. These are exactly the values D1 exists to order correctly (numeric `_id`s,
external 64-bit keys), and sub-1/16 values are common in seconds-valued fields (latencies, timedeltas).

The bit is not needed. Give a negative magnitude in (2^(U-1), 2^U] the tag of U, with tail 1 - f of
the exponent U - 1, and let -(2^U) be that tag with no tail: the empty tail sorts first in its tag,
which is where the most negative value of [-(2^U), -(2^(U-1))) belongs. Positives keep tag E with the
power of two as the empty tail (it already sorts first). Both signs then need one tag per exponent, so
124 tags cover E = -60 to 63: every `int64` and `uint64` with no escape, and doubles down to 2^-60.
Fractions, 1 - f, the canonical last byte and the escape fields are unchanged, so the size of every
value is the same or smaller. `p4_alt.py` checks it (`p4_alt.out`): on P1's 196,961 typed values,
0 order, round-trip or cross-type failures, longest 10 bytes; over every input of 1 to 3 bytes and
every escape tag with every field and 4 tails, 16,268,708 accepted, 0 non-canonical; on the corpora,
21,089 B for both layouts (they hold no value outside 2^-4 to 2^57); per value on 2,000-value sets,
old codec against D2 against this layout:

| Set | Old B | D2 B | One tag per exponent B |
|---|---|---|---|
| snowflake ids in [2^60, 2^62) | 8.99 | 10.96 | 8.96 |
| uint64 hashes | 9.23 | 10.94 | 8.97 |
| latencies 0.0005 to 0.06 s | 7.99 | 9.94 | 7.94 |
| scores in [0, 1) | 7.94 | 7.92 | 7.81 |
| prices 0.01 to 1000 | 7.82 | 7.81 | 7.81 |
| signed ints -1000 to 1000 | 2.75 | 2.22 | 2.22 |
| datetimes, microseconds | 7.98 | 7.94 | 7.94 |

D2 is 24% larger than the old codec on sub-1/16 latencies and 22% on 64-bit ids; the one-tag layout is
smaller than the old codec on every set sampled. The probe is a model, not the spec: its escape bases
and the direct window (-60 to 63) are one choice, which the designer should tune on the T5 sets.

Fix: revise D2 and D3 to the one-tag-per-exponent layout (or record the measured cost of keeping the
bit and why it is worth paying), regenerate `model.py`'s examples, rerun its self-test and mutants, and
state the max size (10 bytes: a double escape plus 7 tail bytes; integers 9). This must land before
step 1 writes `vectors.txt`, since the fixture freezes the bytes.

**2. The size row is measured on data that cannot show the regression, and the datetime terms are not
Xapiand's.** D6 and T5 sample `accounts` and `cities` (no negatives, no integer above 2^31, no value
below 1/16) plus synthetic datetimes. `sizes.py:20-27, 63-65` builds the datetime sets' accuracy terms
with the numeric floors 100 to 10^8, but a datetime field's default accuracies are hour, day, month,
year, decade and century (`src/database/schema.cc:169-176`), applied by calendar truncation
(`src/multivalue/generate_terms.cc:253-293`). With those (`p3_sizes.out`), datetime terms are 4.40 B
old against 4.12 B new, still smaller. Over `p3_sizes.py`'s 13 sets of 2,000 values the D2 layout is
2.2% larger in values (176,038 B against 172,169 B) and 3.3% larger in terms (600,822 B against
581,663 B), all of it from the two 64-bit sets; every other set is smaller or within 0.02 B (scores in
[0, 1): 7.95 B against 7.93 B).

Fix: T5 adds negative integers, two-decimal prices, scores in [0, 1), sub-1/16 seconds, epoch
milliseconds, 64-bit ids and hashes, and datetime terms built from `def_accuracy_datetime`; its target
stays "no larger than the old codec, every set", which the D2 layout fails and finding 1's layout must
be shown to pass. D6 states the corpus with the number ("6% smaller on accounts and cities"), not as a
general claim.

**3. D9 refuses the internal indexes, so an upgraded node cannot start.** Every node holds
`.xapiand/nodes` and `.xapiand/indexes` (`src/manager.cc:708, 836`, `src/index_resolver_lru.cc:248,
386`), written with old version-slot bytes (`src/engine/xapian/shard.cc:64`, read at
`src/index_resolver_lru.cc:413`). They are never empty, so D9 refuses them, and the node fails at
startup, before it could serve the RESTORE that D9 prescribes. `.xapiand/indexes` also holds each
index's settings (shard count, replicas, placement), which DUMP does not carry (finding 4).

Fix: D9 says what the new binary does with an old data directory's `.xapiand/*`: either rebuilds them
in place (their documents' stored data is intact; only slot and term bytes are old) or refuses to
start with a message that names the offline procedure. T8 starts the new binary on a data directory
written by the old one, single node and 2-node.

**4. DUMP carries documents only; the migration loses schemas, metadata and settings.**
`dump_documents` writes each document's data object and blobs (`src/database/handler.cc:721-756,
820-862`); explicit schemas, foreign-schema indexes, user metadata and index settings are not in it
(`backups.md:102-120` already tells users to set a schema before restoring). D9 also does not say
whether `_version` survives a DUMP and RESTORE; by reading it restarts, which clients using optimistic
concurrency would see. Unverified live.

Fix: D9's procedure lists every artefact to carry and in what order (foreign-schema indexes, then
schemas and settings, then documents, then metadata), and states the version behavior; T8's "identical
documents" adds identical schema, metadata and settings, and checks `_version`.

**5. The marker's write path is unspecified where shards are created without a WAL.** D9 sets and
commits `_value_codec` "when the shard is created". Replicas are created with
`DB_CREATE_OR_OPEN | DB_REPLICA | DB_DISABLE_WAL` and then fed WAL lines or whole files
(`src/server/replication_protocol_views.cc:329, 985, 1000`); RESTORE creates with `DB_DISABLE_WAL`
(`src/database/handler.cc:873`); `reopen_readable` can create a shard it cannot write
(`src/database/shard.cc`, local branch of `reopen_readable`). The design does not say whether the
marker goes through the WAL, whether its commit moves every new shard from revision 0 to 1 (the
replication handshake compares revisions, `replication_protocol_views.cc:529`), or what a read-only
open does with an empty unmarked shard. The metadata API's read-only list is
`src/server/search_views.cc:1131` (`schema`, `wal`, `nodes`, `metrics`, stored without a prefix); the
marker's name should follow that convention or the guard must match the key as stored.

Fix: D9 names the write path and the revision it leaves; a read-only open accepts an empty unmarked
shard without writing; T8 adds a fresh 2-node cluster that replicates a new index (both shards carry
the key, revisions agree) and a crash between shard creation and the first commit.

**6. "A cluster upgrades every node together" is not enforced.** The discovery bus is built with
protocol 1.0 (`src/server/discovery.cc:73-74, 106-108`). An old and a new node would form one cluster:
the old one would replay new-byte WAL lines it has no marker check for, and a coordinator would merge
sort keys of both encodings. A downgraded binary likewise opens new shards and reads every number
wrong, silently.

Fix: step 2 bumps the discovery protocol major (and the replication and remote protocol versions) so
mixed nodes refuse each other; T8 adds an old plus new pair that must not form a cluster. The changelog
entry says a downgrade needs DUMP and RESTORE too.

**7. D7's overload set is ambiguous for ordinary integer types, differently per platform.**
`p5_overloads.out` (macOS, where `int64_t` is `long long`): `encode(5, out)` (int), `encode(5L, out)`
and `encode(size_t, out)` do not compile; on Linux, where `int64_t` is `long`, `long long` fails
instead. `bool` fails only by accident.

Fix: the library takes every standard integer type (signed to the `int64` path, unsigned to `uint64`),
`float` and `double`, through constrained templates or a full overload set, and deletes `bool` and
the character types. T11 compiles calls with `int`, `long`, `long long`, their unsigned forms,
`size_t`, `float` and `double` on every platform.

**8. T10's grep must not use `--exclude-dir=xapian`.** That flag also skips `src/engine/xapian/`, which
holds five of the direct calls (`shard.cc:56, 64, 88`, `convert.cc:314`); this review's first census
missed them that way. Fix: T10 filters by path prefix (`grep -rn sortable_ src | grep -v '^src/xapian/'`).

**9. The missing-value sort key is an encoded `DBL_MAX`.** `MAX_CMPVALUE` is
`Serialise::floating(DBL_MAX)` (`src/multivalue/keymaker.h:46`), so a document without a value sorts
below one whose distance is +inf. The old codec behaves the same (its +inf is above `DBL_MAX`), so this
is not a regression. Fix in step 2: name the sentinel as a `sortable` constant and state, in the
mechanism note, where missing values sort relative to ±inf.

**10. D5's reject rules need mutants of their own.** Step 1's mutants cover the encoder (D2, D3). Each
of D5's eleven reject rules should have a mutant that removes it and a test that catches it; most fall
to T4's exhaustive 1- to 3-byte part, but "more than 8 tail bytes" and "neither a double nor an
integer" need inputs of 10 bytes or more, which only the fuzzer reaches today.

## Blocking items / Required changes

- L1 (step 1, finding 1): revise D2 and D3 to one tag per exponent for both signs, or record why the
  bit is kept with the measured cost; update `model.py`, its examples, self-test and mutants, and the
  stated maximum size.
- L2 (step 1, finding 2): broaden T5 and `sizes.py` to the sets listed, with datetime terms from
  `def_accuracy_datetime`; restate D6's claim with its corpus. The chosen layout passes T5 on every
  set.
- L3 (step 2, finding 3): D9 handles an old data directory's `.xapiand/*`; T8 starts the new binary on
  one.
- L4 (step 2, finding 4): D9's migration procedure covers schemas, foreign schemas, settings, metadata
  and versions; T8 checks them.
- L5 (step 2, finding 5): D9 names the marker's write path, its revision effect, and the read-only open
  rule; T8 adds the replication and crash cases.
- L6 (step 2, finding 6): mixed-version nodes refuse each other; T8 tests it.

L1 and L2 block step 1. L3 to L6 block step 2, not step 1: they change no byte of the codec.

## Conditions

- C1 (finding 7): the library's API takes every standard integer type without ambiguity on every
  platform, deletes `bool` and character types; T11 compiles it.
- C2 (finding 8): T10's grep filters by path prefix.
- C3 (finding 9): the missing-value sentinel becomes a named codec constant in step 2.
- C4 (finding 10): a mutant per D5 reject rule, each caught by a unit test, in step 1's done list.
- C5: D5's arm64 `long double` question stays labelled unverified until someone runs `legacy_probe` on
  an arm64 Linux host; nothing in this design depends on its answer, since old bytes are refused.

Not run: no Xapiand build, unit tests, docs E2E, harness, cluster or fuzzing; no benchmarks; no arm64
or Linux host; only syntax-only compiles and Python probes on this Mac.
