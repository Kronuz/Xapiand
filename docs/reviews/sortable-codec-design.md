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

## Re-check 1

- Date: 2026-10-27. Scope: L1 to L6 and C1 to C5 only, against revision 2 (Xapiand:8e9790af5) plus the
  lead's rulings on Q5 to Q7 (Xapiand:cc383ab86), `docs/design/sortable-codec.md`,
  `docs/design/sortable-codec/{model.py,sizes.py,overloads.cc}`, `docs/status/sortable-codec.md`.
- Clean room kept: `src/sortable_serialise.{cc,h}`, `src/xapian/` and `src/server/remote_protocol_views.cc`
  not opened. Probes ran under `quiet light codec-recheck`; outputs in `.scratch/sortable-codec-recheck/`
  (`p1.out`, `p2.out`, `rc_bytes.py`/`rc_bytes.out`, `sizes.out`, `rc_lat.py`/`rc_lat.out`).

### Verdicts

| Item | Status |
|---|---|
| L1 one tag per exponent | CLOSED |
| L2 size row and corpora | CLOSED, one residual (R1) |
| L3 `.xapiand/*` at first start | CLOSED |
| L4 DUMP and what the migration carries | PARTIAL: the lead's Q6 ruling is not in D10 or T13 (R2) |
| L5 the marker's write path | CLOSED |
| L6 mixed versions and downgrades | CLOSED (depends on the Q7 `cluster` change) |
| C1 overloads | CLOSED |
| C2 T10's grep | CLOSED |
| C3 missing-value sentinel | CLOSED |
| C4 a mutant per D5 rule | CLOSED (design level) |
| C5 arm64 `long double` | CLOSED |

### L1 CLOSED

`model.py` is the layout of `p4_alt.py`; the architect should not need to look again.

1. Bytes: `rc_bytes.py` (my own comparison, not the designer's `xcheck.py`) encodes P1's 196,961 typed values
   with `model.encode` and `p4_alt.encode`: 0 byte differences; longest 10 bytes; 0 start with `0x00`.
2. Accepted sets: `model.decode` against `p4_alt.decode` over every 1- and 2-byte input, every tag with every
   2-byte field (stride 7) and 6 tails, every escape tag with every field and 4 tails, 2,000,000 random 3-byte
   inputs and 10^6 random 4 to 13-byte inputs seeded on the tags that matter: 9,212,576 accepted, 0
   disagreements (value or rejection).
3. Order: `p1_order.py` rerun against the revised model: order failures 0, cross-type equality failures 0,
   `0x00`-first 0. Its `spec!=model 194227` line is expected and not a defect: `p1_order.py`'s independent
   encoder is written from revision 0's two-tags-per-exponent text, so it differs from any one-tag layout
   (its "max len 11" is that old encoder's). The order check in that probe uses `model.encode`.
4. Strictness: `p2_decoder.py` against the revised `model.decode`: 25,028,525 inputs tried, 19,661,306
   accepted (with repeats), 0 that do not re-encode to themselves.
5. The model's own commands: `selftest` 213,624 values, 0 failures, longest 10 B; three encoder mutants fail
   15,617, 11,004 and 24,897 checks; `rules` shows a witness for each D5 rule.

### L2 CLOSED, with R1

`sizes.py` reruns to the design's table to the byte (legacy probe built from my kept
`.scratch/sortable-codec-arch/legacy_probe`): corpora values 22,790 B old against 21,089 B new, terms 57,601
against 54,951; synthetic values 174,849 against 170,070, terms 581,663 against 553,242; `T5: PASS`, exit 0, no
set larger in values or terms (the worst are within 0.02 B per value: prices 7.77 against 7.79, scores 7.82
against 7.93). The datetime terms are Xapiand's: `sizes.py:dt_terms` builds hour, day, month, year, decade
and century by calendar truncation with `GenerateTerms::year` flooring, matching `def_accuracy_datetime`
(`src/database/schema.cc:169-176`) and the `case UnitTime::*` branches (`src/multivalue/generate_terms.cc:253-293`);
the bucket table gives 4.12 B new against 4.40 to 4.41 B old, equal to my earlier `p3_sizes.out`. The two 64-bit
sets are 8.95 B and 8.96 B against 8.99 B and 9.24 B. D6 now states the corpus with each number.

R1 (condition for step 1's T5, not a blocker): `sizes.py` and T5 have no sub-1/16-seconds set, which my
finding 2 listed and which is where revision 0 lost (+24%). I measured it with the revised model and the
legacy probe (`rc_lat.out`, 2,000 values each): latencies 0.0005 to 0.06 s 7.99 B old, 7.94 B new; 1e-9 to 1e-3
7.99 B, 7.94 B. Add one such set to `sizes.py`, so T5 guards the window. Also measured: doubles below 2^-60
(uniform 1e-30 to 1e-20) are 7.98 B old and 9.93 B new (+24%, the 3-byte escape). D6 says this is "not
measured" and no set holds such values; it is real and the T5 rule "no larger, every set" would fail on it.
Accept it and state the number in D6, or widen the window; the lead decides (rare in practice, a deliberate
trade).

### L3 CLOSED

D10 item 2 runs the in-place routine for `.xapiand/*` at node start, before the manager opens them
(`src/manager.cc:708, 836` are the first uses; I read both), so D9's refusal cannot stop an upgraded node;
the text names why (never empty, old version-slot bytes) and that `.xapiand/indexes` holds the settings.
The unreplayed-WAL case stops with a message instead of decoding old version bytes. T13 starts the new binary
on an old data directory, single node and 2 nodes, kill -9 included. Residual: item 2 has no flag, so a
node with an unreplayed-WAL internal index refuses to start with the message; that is the right outcome and
T13 tests it.

### L4 PARTIAL (R2)

Closed parts: D10's carry list (documents, blobs, schema, foreign schemas, user metadata, settings via
`.xapiand/indexes` first) and the manual route's order (foreign schemas, schemas and settings, RESTORE,
metadata); T13 checks identity of each and runs both routes.

R2, open: the lead's Q6 ruling is in the rulings paragraph only (`sortable-codec.md:555-559`: the in-place
`--upgrade-codec` keeps every document's `_version`; only dump and restore may restart them). The design body
still says the opposite in five places: D10 (`:371-376`, "What it does not: `_version`", "every version restarts
at 1, here and in DUMP and RESTORE alike", "never decodes a slot or a term"), the manual route (`:388`), Q6
(`:486`, "Every document restarts at version 1", recommends (a)), T13 ("`_version` is 1 for every document"), and
the step 2 changelog line (`:523`); the ledger's "Blocked on" (`docs/status/sortable-codec.md:33`) still lists Q6
as open. T13 would therefore fail a correct implementation of the ruling.

The ruling is also not a text edit. It says the upgrade "rewrites values in place", but D10's mechanism
copies data into a new `<shard>.codec2` and never reads an old slot or term; the version lives only in the `V`
term and `DB_SLOT_VERSION` in old bytes (`src/engine/xapian/shard.cc:52-64`), and a fresh add stamps version 1
(`shard.cc:116-118`). Keeping versions needs two things the design has not specified: a reader for the old
version bytes (the cost Q6 (b) names: the GPL algorithm back in the tree, or a table derived by black-box
probing of the old codec, which is feasible for the small unsigned range a version occupies and checkable
exhaustively against `legacy_probe`), and a way for the upgrade's indexer to stamp a given version instead of
1. Fix: rewrite D10, Q6, T13, the manual-route sentence and the changelog line to the ruling, say which reader
D10 uses and how the indexer sets the version, and keep the "legacy mode never decodes a slot or a term"
sentence true or amend it for the version slot alone. This blocks step 2, not step 1.

### L5 CLOSED

D9 now has a creation table: new primary (marker written by `reopen_writable` straight to the engine, not the
WAL, committed before the open returns; covers RESTORE's `DB_DISABLE_WAL`), replica (no write at creation; a
revision-0 replica receives the whole database, checked at `replication_protocol_views.cc:515-538`: `to_revision
== 0` copies every file), readable open (writes nothing, shard stays empty and unmarked, adopted by the next
writable open) and remote shards. The revision effect (0 to 1 on the marker's commit, via `set_metadata`'s
`prior_revision + 1`, `shard.cc:1878-1905`) is stated as by-reading and checked in T8, which also adds the
fresh 2-node replication, the crash between creation and the marker, a `"3"` marker and RESTORE. The read-only
guard is named: `search_views.cc:1131` compares the command minus its first character with `schema`, `wal`,
`nodes`, `metrics`; the design refuses `_value_codec` by its stored name and every key beginning with `_`, and
T8 tries both spellings. Worth checking at the step 2 review only: a blanket `_` prefix refusal also bars user
metadata keys that start with `_`.

### L6 CLOSED

D11 bumps the discovery major to 2 (`src/server/discovery.cc:73`; read) and the remote protocol to 48.0, states
the one-way property that matters (the bus drops frames newer than itself, accepts older ones:
`cluster/bus.h` `on_datagram`, rx at or below its own version, which I confirmed in the pinned `cluster`
checkout), so a new node would hear an old node; the lead's Q7 ruling picks the additive `Kronuz/cluster`
minimum-version change, coordinated with Detent's agent. T14 covers both start orders and the handshake.
Residual for the lead: D11 item 1 and T14 still read "Q7 chooses" / "after Q7's change"; fold the ruling into
the body when D10 is revised. The `cluster` change is a dependency of step 2d, not of step 1. Downgrades: D11
item 4 and Q5's ruling (accept; changelog) state that a release older than this design cannot refuse a new
shard and that every later release refuses an unknown marker.

### Conditions

- C1 CLOSED. D7 is one constrained template (`encodable`: standard integers except `bool` and the character
  types, `float`, `double`). `c++ -std=c++20 -fsyntax-only overloads.cc` (Apple clang) exits 0: 20 `OK` forms,
  7 `NO` forms. It is a stub, so T11 must compile the real header with the same forms on Linux (where
  `int64_t` is `long`); the design says so.
- C2 CLOSED. T10 filters by path prefix (`grep -v '^src/xapian/'`) and says why `--exclude-dir=xapian` is wrong.
- C3 CLOSED. `max_finite` and `min_finite` are named in D7; D7 and D12 say a missing value sorts after every
  finite number and below +inf, and the mechanism note will state it against -inf and +inf.
- C4 CLOSED at design level. `model.py rules` has a witness for every D5 rule (including the 9-byte tail and the
  64-bit tail, which the model's rule 9 would otherwise hide), with six shown needed; step 1's done list and T4
  require a decoder mutant per rule including 6 and 8, and a fuzz minute budget. The C++ mutants do not exist
  yet; that is step 1's work.
- C5 CLOSED. D5 labels the question unverified and says nothing depends on it.

### New findings

- N1 (R1 above, lead's call): doubles below 2^-60 cost 24% more than the old codec (9.93 B against 7.98 B),
  unmeasured in D6 and incompatible with T5's "no larger on every set" if such a set is ever added.
- N2 (R2 above): the Q6 ruling contradicts D10, Q6 and T13 as written, and implementing it needs a reader for the
  old version bytes that D10's "never decodes old bytes" currently forbids.

### Whether step 1 may start

Yes. L1 and L2 closed, which were the items that blocked step 1; the bytes are fixed (identical to the
`p4_alt.py` layout, 0 byte differences). Step 1 should add the sub-1/16-seconds set to `sizes.py` (R1) and
decide the below-2^-60 trade before its `vectors.txt` freezes the escape window (the vectors fixture freezes
the escape base too). Step 2 stays blocked on R2 (and Q7's `cluster` change for 2d).

Not run: the C++ library (does not exist yet), no Xapiand build, unit tests, docs E2E, harness, cluster or
fuzzing; no benchmarks; no arm64 or Linux host; T8, T13 and T14 are rows, not results, so L3 to L6 are
closed as design text checked against the code by reading, not as behavior; the by-reading claims about the
revision effect, the replica copy and `_version` remain unverified live; the Q7 `cluster` change was not
reviewed (not written).
