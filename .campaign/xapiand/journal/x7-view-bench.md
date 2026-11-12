# x7-view-bench: journal

State: waiting on jobs 261111-175933-48460d (build+copy to /Users/kronuz/.scratch/x7-view-bench/ff03d4e), 261111-175937-76ae57 (strict run, A1, after the build)
Quest: .campaign/xapiand/quests/x7-view-bench.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A2 passed (build-test 261111-160901-23e3eb). First bench build failed (Viewed incomplete); fixed by including engine/strata/viewed.h, resubmitted. Benchmark committed (34b610bf5, tests/bench_shard_view.cc, target xapiand_bench_shard_view, EXCLUDE_FROM_ALL). Build job in build-bench running.

## Next

1. Read the strict job 261111-175937-76ae57 output (the table is in its log).
2. Write docs/perf/shard-view.md, paste Evidence, State ready for review.

## Blocked on

Nothing

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
