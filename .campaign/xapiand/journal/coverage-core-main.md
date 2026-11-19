# coverage-core-main: journal

State: waiting on jobs 261119-044115-72c517 (coverage) and the A3 verify jobs (campaign verify --for A3)
Quest: .campaign/xapiand/quests/coverage-core-main.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline (coverage-areas job 8642ea at 4517f42e4): main.cc 377/743 lines, 164/465 branches; manager.cc 610/835, 179/428; target 90/80 each.
Written, not yet run: tests/test_coverage_main.cc (includes src/main.cc under `#define main`/`#define opts main_opts`; job 82ceb7 builds it via coverage_one.sh) and harness/coverage_e2e_main.sh (the `main` workload, added to harness/coverage.sh and its default WORKLOADS).

## Next

0. 396553 passed (E2E 89/89, main.cc 79.1% lines/60.6% br, manager.cc 79.6%/52.8%). Added at 737496e23: more open-files cases, fd-exhausted SIGTERM, bad restores; rerun 72c517. Left (fault injection/root/tty/abandoned shutdown): demote 507-543, setuid fail, tty branches, backtrace handler (death by signal skips the profile flush), host_address fallbacks, shutdown abandonment 1083-1103/1138-1152/1224-1240.
   Earlier: 2fdee2 (at f84681f1a): ctest green, E2E 89 checks 4 failed (log stamps carry a leading glyph, ISO uses a space + ms, third SIGINT ends node before it logs); fixed at 83c4df6f9, rerun 396553. 2fdee2 numbers: main.cc 588/743 lines 79.1%, 282/465 branches 60.6%; manager.cc 665/835 79.6%, 225/428 52.6% (targets 90/80: below, more scenarios needed after 396553; read coverage-uncovered.txt).
   Earlier: e1a2b8 failed (ctest: the 2 open-files cases; E2E: 10 of 86 checks). Fixed at f84681f1a: open-files cases pinned to the real SystemExit(EX_OSFILE) (3 fds open, max 300 / 1024); driver expects EX_SOFTWARE (70) for a name mismatch, -v -v for INFO texts, 20x PUT statuses, a refused 300 limit, daemon waited for "Ready to Rock". Rerun 2fdee2 (coverage ctest+main, no NOBUILD: the test changed). Then read main.log, per-file numbers, `campaign verify coverage-core-main --for A3`, Evidence table.

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
- max_clients -2 is NOT confirmed (hunt.md row stays UNCONFIRMED/refuted): adjustOpenFilesLimit throws SystemExit(EX_OSFILE=72) for --max-files 300 and for a 1024 hard limit with defaults (pool 200, clients 1000); it logs "Your open files limit of 300 is not enough for the server to start. Please increase ... to at least 1471" (plus "database_pool_size has been reduced to 34"). The real finding: the computed minimum_files (1471) includes the capped default pool, so any limit under 1471 refuses with defaults although the pool would shrink to fit. Lead decides whether to fix.
