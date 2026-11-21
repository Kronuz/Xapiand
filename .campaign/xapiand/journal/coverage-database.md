# coverage-database: journal

State: waiting on jobs 261120-081411-4fd569 (suite), 261120-081414-17aa17 (A1 coverage, newman e2e), 261120-081415-54bf5f (A2)
Quest: .campaign/xapiand/quests/coverage-database.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline coverage job 4358ca (ctest, warms build-cov; its coverage-uncovered.txt lists the database gaps in .scratch/cov-before). Probe job 08fd11 (--unversioned: builds tests/test_coverage_database.cc under harness/coverage_one.sh database src/database/schema.cc, output .scratch/probe1.out). tests/test_coverage_database.cc is an in-process node (RunningNode and http_call from test_loop_yield.cc) driven by requests; main() is still an exploration that prints each reply (P/R lines): turn them into value checks.

## Next

0. Merged origin/master. Probe 3 aborted on an empty (204) reply parsed as JSON; fixed (json_of tolerant, 2xx printed). Baseline numbers (job d792cf, ctest corpora, test aborted so adds nothing): database 55.3% lines / 45.4% branches of 15516 / 6874; target 90 / 80. Needs ~5400 more lines.

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
| A1 | job 261120-120511-be5e6e (coverage ctest corpora e2e: database 69.5% lines / 58.8% branches, from 55.3% / 45.4%) |
| A2 | job 261120-081415-54bf5f (coverage_one database schema.cc); value pins in tests/test_coverage_database_pins.inc |
| A3 | suite job 261120-081411-4fd569 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>


## Rulings I made (run 4)

- Pins in tests/test_coverage_database_pins.inc are captured replies (status + leading body bytes), reviewed; defects pinned with HUNT H1-H5 tags (see the file header). INFO/DUMP keep only stable bytes (uuid varies).
- Object-typed schema field with scalar value (ty15 s*/w* requests) aborted Debug at schema.cc:5339; skipped in the test, tracked by the hunt.md row "schema.cc:5339 assertion".
