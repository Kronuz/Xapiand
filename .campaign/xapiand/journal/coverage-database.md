# coverage-database: journal

State: waiting on jobs 261119-023502-f03bab (probe 3) and the baseline coverage job (see campaign job list); cabf22 failed on my test file's compile error (extra namespace), fixed; probe 900c61 had silently failed the same way
Quest: .campaign/xapiand/quests/coverage-database.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline coverage job 4358ca (ctest, warms build-cov; its coverage-uncovered.txt lists the database gaps in .scratch/cov-before). Probe job 08fd11 (--unversioned: builds tests/test_coverage_database.cc under harness/coverage_one.sh database src/database/schema.cc, output .scratch/probe1.out). tests/test_coverage_database.cc is an in-process node (RunningNode and http_call from test_loop_yield.cc) driven by requests; main() is still an exploration that prints each reply (P/R lines): turn them into value checks.

## Next

1. Read probe1.out: pin the property-refusal messages (R lines) in a table of checks; then the type matrix, schema writes, updates, scripts, deletes, WAL/shard paths, driven by coverage-uncovered.txt.
2. Final coverage job (WORKLOADS ctest corpora e2e; newman is at /usr/local/bin/newman) and suite job at the head, Evidence rows, push, ready.

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
