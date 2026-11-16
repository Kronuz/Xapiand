# agg-shard-parity review: correctness

Commit: cdb757fc2a583e47b11b485234cfb3ba416d0de0
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261112-002820-f46d26 (build-test at cdb757fc2, exit 0): 38/38 ctest including `test_aggregation_shards`.
- 261112-013131-08b954 (A2, fix reverted to 887aea745's `bucket.h`, at 2231eaa7c, which differs from
  cdb757fc2 only in the journal): exit 0, log ends "killed: the test failed on its own checks". The script
  returns 1 on any setup failure and returns 0 only when `grep "FAIL .*test_aggregation_shards.cc"` matches.
  Only the harness's `FAIL <file>:<line>` lines match that pattern. ctest's "FAILED:" and "(Failed)" lines do not.
- 261112-013135-842794 (A2, `AGG_PARITY_DROP_SHARD=1`, same shape): exit 0, killed the same way. The
  hook leaves shard 1 out of both the one-spy search and the merges (test:205-230). n=1 is unaffected.
- 261112-002821-1d7a01 is void. It exits 0 on "setup failed" (its script) and did so on a failed cmake (log tail).

## What I checked myself

Probe 261112-013642-150488 at cdb757fc2. Script: `.scratch/review-agg-shard-parity/probe.sh` and
`order_probe.inc`. It patches the test in the job checkout and restores it on exit. It ran on x86_64,
where `LDBL_MANT_DIG` is 64, so the two-double split runs for real:

1. It sends 27 edge slots through the real `serialise_order`/`unserialise_order`: 0, ±1, -17, -12.25,
   INT64_MAX/MIN (±1), UINT64_MAX (-1), 2^53+1, -(2^60+3), ±1/3 (64-bit mantissa), dates (double and long
   double), ±DBL_MAX, DBL_MIN, the smallest double denormal, ±inf, NaN, and idx 0/1/2/1000/SIZE_MAX. Every
   slot in double range round-trips exactly. Two do not: -0 comes back as +0 (N2), and LDBL_MAX comes back
   as inf (N3). Junk after the order bytes is refused.
2. It ran the parity test with a NaN-strict compare, where a NaN on one side only counts as a DIFF. Result:
   0 DIFF over 774 compared results. 258 floating pairs are NaN on both sides. Those 258 are the whole
   "258 values differ by rounding, worst 0" count, so every non-NaN floating value is bit-equal across layouts.

Read: bucket.h 80-260, 545-720, 985-1017, 1030-1040, 1160-1175; metrics.h avg and variance merge/update;
aggregations.cc's merge entry points; and the whole test.

Q1. The round trip is exact for every slot a value can produce (probe item 1). Merging two partials that
hold the same bucket keeps the first creator's slot and idx (bucket.h:592-599, 638-647); the second merges
only its sub-results. That is right for every kind:

- `_values`/`_terms`: the key is `"{}"` of the slot, so the same key means the same slot.
- `_range`: slot is the `from` bound (or -inf) and idx is the range position, both the same on every shard.
- `_histogram`: slot is the first value seen in the bucket (bucket.h:990-1017), not the bucket key. It
  differs per shard, but it stays inside the bucket's interval, so the order between buckets holds. The
  same is true of one spy.

Three paths construct buckets: `aggregate` (bucket.h:698-706) and both merge overloads. `_prune` moves
nodes with `extract`, which keeps slot and idx. `FilterAggregation` wraps one `Aggregation` and has no
bucket map. No path builds a bucket without slot and idx.

Q2. The test compares values for every kind the doc lists. It compares keys, `_doc_count` and every metric
field, in bucket order, nested to three levels, over three delete scenarios (one of them leaves an empty
shard) and three paths. `test_one_shard_matches_the_rows` anchors the reference. The `_terms`-past-`_shard_size`
check is not vacuous. 355 distinct values against a cap of 2x20 force pruning on every layout. Its checks
are: no count above the true one, the top 3 keys in order, and counts at or above 95% of the true count.
Both mutants kill the test.

Q4. Nothing outside `src/aggregations` calls `serialise_results`/`merge_results`. `src/engine/callbacks.h:101-102`
only declares the interface. The tests that implement it return their own bytes, and only
`test_aggregation_shards` merges `AggregationMatchSpy` partials. Partials are never persisted.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | `compare` accepts a NaN on one side as equal: `rel` is NaN, so `rel > 1e-9` is false. Results do contain NaN (258 pairs: sample variance over one value, metrics.h:736). A merge that produced NaN where the reference has a number would pass silently. Today no such case hides there. | tests/test_aggregation_shards.cc:285-286; probe 150488 (0 DIFF under the strict compare) | Add `\|\| std::isnan(x) != std::isnan(y)` to the DIFF condition, and leave NaN pairs out of `inexact` |
| N2 | no | nit | Slot -0 comes back as +0 (`hi + lo` with `lo = +0`). The comparators use `!=`/`<`, where -0 == +0, and the slot is never output, so the order is unaffected. | probe 150488 | None needed |
| N3 | no | nit | A slot beyond double range comes back as ±inf. The comment's "exact on x86" holds only inside double range. No value can produce such a slot (values are doubles or 64-bit integers). | bucket.h:548-553; probe 150488 | Optional: say "exact for any slot in double range" |
| N4 | no | gap | Void job 1d7a01 is still tagged `--for A2 --fails-without`, and `campaign status` lists it as A2's evidence. | `campaign status agg-shard-parity`; its for.json | The lead makes sure it does not count: 08b954 and 842794 are the A2 evidence |
| N5 | no | gap | Only the default orders are compared. Not compared: `_sort` by key descending, by count ascending, or by a sub-aggregation field (`CmpByField*` with `value_ptr`, set in both merge paths); `_limit` below the bucket count on a numeric field; and date, boolean and positive fields (probe item 1 covers their slot round trip). These are options and types, not kinds, so A1 stands. | tests/test_aggregation_shards.cc:352-410 | Optional follow-up specs |
| N6 | no | nit | The sample variance of a single value is NaN (0/0) in user-visible results. This predates the quest and is outside its scope. | metrics.h:736 | Hunt list if wanted |

## Not run

- No Linux, aarch64 (quad long double) or sanitizer run; those are CI's.
- No timings.
- I did not read the GPL `sortable_serialise` sources.
- Multi-type fields, where the same key string comes from different types, are not checked. Slot is first-wins there with one spy too.
