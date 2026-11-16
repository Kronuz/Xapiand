# x7-view-bench: journal

State: waiting on jobs 261111-190002-a37529 (build-test A2 at the docs commit); then ready for review
Quest: .campaign/xapiand/quests/x7-view-bench.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting for build-test a37529; then set State to ready for review.

## Next

1. When a37529 passes, set State to ready for review and commit.

## Blocked on

Nothing

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261111-175937-76ae57 (strict run at ff03d4e; numbers in docs/perf/shard-view.md) |
| A2 | 261111-190002-a37529 (build-test at the docs commit; 261111-160901-23e3eb passed earlier at the code commit) |

## Result

Added xapiand_bench_shard_view (tests/bench_shard_view.cc, EXCLUDE_FROM_ALL target) and docs/perf/shard-view.md. Shard::view() per call: median 18.6 ns at 1 thread; 20.3 ns (shared) / 21.4 ns (per-handle) at 8, p90 29.8 ns at 8 vs 18.9 at 1; 52.5 vs 298 Mcalls/s (5.7x on 8 threads). Job 261111-175937-76ae57 at ff03d4e, i9-9980HK macOS x86_64. Open: no Linux/arm64 row, no concurrent writer.

## Rulings I made

Measured both a shared Shard object and a handle per thread (one extra row, same cost). Batch-mean samples (1000 calls) because a clock read is about the call's cost. Branch is master, not main.
