# coverage-database: journal

State: waiting on jobs 261119-225300-be51b7 (run3: captures the pin table into ~/.scratch/xapiand/coverage-database/pins3.inc; next: review it, commit as tests/test_coverage_database_pins.inc, resubmit A1-A3 and the suite)
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
| A1 | job 261119-220859-27b00e (coverage ctest+corpora, light); expected below target: test is still an exploration main |
| A2 | job 261119-220854-d2b115 (coverage_one database schema.cc); value checks not yet written |
| A3 | suite job 261119-220855-4b8309 |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
