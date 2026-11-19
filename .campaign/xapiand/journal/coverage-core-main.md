# coverage-core-main: journal

State: waiting on jobs 261119-004113-82ceb7
Quest: .campaign/xapiand/quests/coverage-core-main.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline (coverage-areas job 8642ea at 4517f42e4): main.cc 377/743 lines, 164/465 branches; manager.cc 610/835, 179/428; target 90/80 each.
Written, not yet run: tests/test_coverage_main.cc (includes src/main.cc under `#define main`/`#define opts main_opts`; job 82ceb7 builds it via coverage_one.sh) and harness/coverage_e2e_main.sh (the `main` workload, added to harness/coverage.sh and its default WORKLOADS).

## Next

1. Read 82ceb7: fix compile errors or failed checks in test_coverage_main.
2. Submit a coverage job: `WORKLOADS="ctest main" harness/coverage.sh $JOB_OUT/cov` (newman PATH as job 8642ea uses); read main.log for failed E2E checks, fix the driver's guesses (log formats, exit statuses) and pin real values.
3. Compare per-file uncovered lines (coverage-uncovered.txt) and add scenarios for what is left; then full-suite job, push, evidence table.

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

- Lead 2026-11-19: coverage.sh and docs/dev/coverage.md are in my file set. coverage.sh now discovers harness/coverage_e2e_<name>.* as workload <name> and honors NOBUILD=1; documented (untested until the coverage job runs).
- Product bug (pinned in test_coverage_main, test_open_files_limit): adjustOpenFilesLimit with --max-files 300, pool 200, clients 1000 leaves database_pool_size 34 and max_clients -2 (negative).
