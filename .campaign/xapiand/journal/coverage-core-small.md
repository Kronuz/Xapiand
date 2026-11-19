# coverage-core-small: journal

State: waiting on jobs 261119-003947-bff9f2 (cast probe)
Quest: .campaign/xapiand/quests/coverage-core-small.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Four tests written (tests/test_coverage_core_{cast,datetime,endpoint,query_dsl}.cc, helper tests/test_coverage_core.h); none compiled yet. Cast probe job running; then one job for the other three with BUILD shared (build-cov-core).

## Next

1. Read the probe output, fix expectations (a mismatch is my misreading or a product bug: check against the source), then run all four with harness/coverage_one.sh, REPEAT=20.
2. Full coverage.sh job, per-file numbers, suite job, push, mark ready.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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
