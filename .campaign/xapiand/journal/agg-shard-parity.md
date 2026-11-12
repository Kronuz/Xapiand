# agg-shard-parity: journal

State: ready for review (A2 mutant 2 re-run pending: job 261112-013031-0c0f1d)
Quest: .campaign/xapiand/quests/agg-shard-parity.md (the lead's; never edit it)

## Doing

Nothing but the pending job 261112-013031-0c0f1d (A2 mutant 2 in the right shape).

## Next

1. Read 0c0f1d: it must pass (the test fails with a DIFF/FAIL line when shard 1 is dropped); then A2 is complete.
2. Review.

## Next (old, done)

1. Read the A1 job; fix compile errors or real mismatches (a mismatch is a finding: kind and values below, src/ is not mine).
2. A2 mutant submitted (done).
3. Write docs/dev/agg-shard-parity.md (A3) with the kinds and what differs by design.

## Blocked on

Nothing. (Lead ruled 11-12: CMakeLists.txt line and docs/dev/agg-shard-parity.md are in the file set; the check now builds tests.)

## Finding F1: merged per-shard partials lose the buckets' order keys (src/aggregations/bucket.h)

A1 (261112-000934-6482ef, log /Users/kronuz/.scratch/campaign/jobs/261112-000934-6482ef/a1/log): test_aggregation_shards
is the only failing ctest (36 of 38 pass otherwise). 168 DIFF lines, all on the "merged bytes" and "merged objects" paths,
0 on "one spy". The one-spy path equals the one-shard answer for all 43 requests, 3 scenarios, 2 and 5 shards (774 compared).
The other two tests pass (rows match; `_terms` past `_shard_size` approximate as ruled).
Failing requests (12 each = 3 scenarios x 2 layouts x 2 merge paths): `_values`/`_terms` over age and score,
`_histogram` age/age shift/score, `_range` age, and every nesting of those (`several at once`, `_terms cat > ...`,
`_filter > _terms age > ...`). Passing under merge: every metric (count..extended_stats), `_values`/`_terms` over cat (keyword),
`_filter` with metric subs. Examples: `_values age` 2 shards `x[7]/_key "-15"` vs `"-17"`; `_histogram age` `x[0]/_doc_count 7` vs `37`;
`_range age` `x[0]/_doc_count 184` vs `114`.
(The "5514 floating values differ by rounding, worst 2" line is noise from comparing misaligned buckets.)

Cause (read from source; diagnosis job 261112-002548-2c8cb9 compares by key to confirm order-only): a bucket's sort keys
`Aggregation::slot` (numeric key) and `idx` (range position) are set only while collecting, in `BucketAggregation::aggregate`
(bucket.h:677-678). `serialise_results` (bucket.h:547-561) writes only the bucket name and its sub-results, and
both `merge_results` overloads (bytes bucket.h:563 ff., objects bucket.h:607 ff.) emplace a new bucket without setting
`slot` or `idx`: they stay 0 (aggregations.cc ctor). Orders by key (`CmpByKeyAsc/Desc`, histogram default), by count
tie-break (`CmpByCountDesc`, `_values`/`_terms` default: ties fall to slot, then the key string) and by index
(`CmpByIndex`, `_range` default) all read them, so a merged bucket sorts by its string key (or arbitrarily for ranges): "-15" < "-17"
lexicographically, 10 before 5. Keyword buckets have slot 0 anyway, so they match. Counts and sums merge correctly.
(Possible knock-on: `_limit` truncation then keeps the wrong buckets, since `_get_result` and `_best` pick by that order.)

User-visible? Not today: nothing outside src/aggregations calls `serialise_results`/`merge_results` (grep of src: only the
`MatchSpy` interface at src/engine/callbacks.h:101-102); `DatabaseHandler::get_mset` (handler.cc:1537) gives `search_batched`
one borrowed spy and the strata engine's one spy sees every shard (docs/mechanisms/strata-callbacks.md item 10). So the
HTTP path is the "one spy" path, which is equal. The bug is latent in the public merge API (the remote-shard and per-shard
paths the Xapian engine used, per docs/mechanisms/aggregation-scope.md) and would bite any cross-node merge.
Fix shape (lead's quest): serialise the order keys with each bucket (slot, idx) and set them in both merge overloads
(or drop the merge API if nothing will use it: one mechanism). A2's mutant is meaningless until then: the test fails without it.

## F1 fix (lead ruled: fix here)

Commit "Carry a bucket's order keys through serialised and merged results" (src/aggregations/bucket.h): serialise_results sends
each bucket's slot (long double as two doubles) and idx; both merge_results overloads set them on a bucket they create.
The bug was latent: no production caller merges partials (the server uses one spy over every shard). Wire format of
partials changed (a per-bucket order string); both ends are ours and nothing stores them.
A1 = verify 261112-002820-f46d26 at the fixed head. A2 mutant 1 = the fix reverted (261112-002821-1d7a01, expects DIFF),
mutant 2 = drop shard 1 (261112-002824-edede9), both after A1. A3 = docs/dev/agg-shard-parity.md (written, states finding and fix).
If 2c8cb9 (diagnosis at the pre-fix commit) shows a difference beyond order, fix or report it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test f46d26 at cdb757fc2 (38/38 ctest incl. test_aggregation_shards, async_check 0 unclassified); pre-fix failure 261112-000934-6482ef (F1) |
| A2 | mutant 1, fix reverted: 261112-002821-1d7a01 passed (test reports DIFF); mutant 2, shard 1 dropped: 261112-002824-edede9 made the test fail but its command shape was wrong, re-run as 261112-013031-0c0f1d (pending) |
| A3 | docs/dev/agg-shard-parity.md |
| F1 | diagnosis 261112-002548-2c8cb9 at the pre-fix commit: 0 DIFF when buckets compare by key, so order only |

## Result

tests/test_aggregation_shards.cc (in CMakeLists.txt's engine-test list) compares 43 aggregation requests over 1, 2 and 5 shards, three
delete scenarios, through one spy and through per-shard merges (bytes, objects); `_terms` past `_shard_size` is approximate by ruling.
It found F1 (merged partials lost slot/idx, so numeric buckets reordered); fixed in src/aggregations/bucket.h. The bug was latent: no
production caller merges partials (the server uses one spy over every shard). Writeup: docs/dev/agg-shard-parity.md. Leaves open: the
merge API has no production caller; whether to keep it is the lead's call (one mechanism).

## Rulings I made

- In-process node: the engine seam (scratch shards, engine::search with one MatchSpy), as tests/test_engine_sort.cc and
  test_wide_search.cc do. Production searches all shards with ONE spy (src/engine/strata/search.cc), so the primary
  path is one spy over n shards; per-shard spies merged through serialise_results/merge_results (bytes and objects)
  are tested too, as the remote path and as the merge the mutant drops a partial from.
- Mutant hook is test-side: env AGG_PARITY_DROP_SHARD=k leaves shard k out of every n-shard run; n=1 is unaffected for k=1.
- Floating values compare to 1e-9 relative (values are multiples of 0.25, so sums are exact; variance may round).
