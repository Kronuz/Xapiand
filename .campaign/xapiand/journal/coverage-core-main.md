# coverage-core-main: journal

State: ready for review (CI to confirm at this head)
Quest: .campaign/xapiand/quests/coverage-core-main.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline (coverage-areas job 8642ea at 4517f42e4): main.cc 377/743 lines, 164/465 branches; manager.cc 610/835, 179/428; target 90/80 each.
Written, not yet run: tests/test_coverage_main.cc (includes src/main.cc under `#define main`/`#define opts main_opts`; job 82ceb7 builds it via coverage_one.sh) and harness/coverage_e2e_main.sh (the `main` workload, added to harness/coverage.sh and its default WORKLOADS).

## Next

0. 72c517 failed on one check (restore of a non-dump exits 0 with 'unsupported type': product bug, pinned; lead: hunt row). Suite f3a2a3 (A3) passed. Lead: manager.cc is slated for removal (D74), no more manager lines. Rerun id in State/log below.
   Earlier: 396553 passed (E2E 89/89, main.cc 79.1% lines/60.6% br, manager.cc 79.6%/52.8%). Added at 737496e23: more open-files cases, fd-exhausted SIGTERM, bad restores; rerun 72c517. Left (fault injection/root/tty/abandoned shutdown): demote 507-543, setuid fail, tty branches, backtrace handler (death by signal skips the profile flush), host_address fallbacks, shutdown abandonment 1083-1103/1138-1152/1224-1240.
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
| A1 | 261119-050554-5cd724 (coverage ctest+main, E2E 92/92, ctest green). main.cc 606/743 lines (81.6%), 293/465 branches (63.0%); manager.cc 665/835 lines (79.6%), 225/428 branches (52.6%). Baseline (8642ea): main.cc 377/743, 164/465; manager.cc 610/835, 179/428. Below the 90/80 targets; the lead amends them (no fault-injection hooks; manager.cc slated for removal, D74). |
| A2 | 261119-121932-49544b (the value-checking tests pass); tests/test_coverage_main.cc checks values (exit codes, fd counts, limits); product facts pinned: open-files refusal EX_OSFILE (minimum 1471), restore of a non-dump exits 0 with "unsupported type". No fixed bug in this quest. |
| A3 | suite 261119-121932-7af433 at head (the earlier f3a2a3 passed at 737496e23); CI runs on the pushed branch |

Uncovered, main.cc lines: 192,217,224 (tty only: is_tty is a static const), 240-241 and 257-258 (sig_handler: /dev/null open fails in a live node; manager throws), 270-285 (backtrace_sig_handler: dies by signal, no profile flush), 366-367,388-389,393-394,426-428 (system-wide file warnings, setrlimit failure text), 507-543 and 588-591 (demote: root only, setuid failure), 618-619, 710-718 (chdir/getcwd failure), 731-753 (resolve_index_endpoints fallback), 918-925 (cleanup_manager with a manager), 945-981 (executable-path fallbacks), 1085-1103 (logger hook lambdas), 1137-1155 (end-of-main teardown branches).
Uncovered, manager.cc lines: 123-125, 243-262 (sig_exit without manager), 325-326, 364-369 (node file write failures), 403-455 (host_address fallbacks), 495-496, 587-588, 637-663 (shutdown_sig on a negative signal and the insisted path), 681-693, 732-756, 783, 814-824, 836-848, 885-889 (setup failures), 932-934, 1013-1014, 1086-1102, 1141-1150, 1164-1199 (shutdown abandonment), 1224-1240 (unfinished_work), 1279-1283, 1391-1396. All need fault injection, a stuck write, root or a tty. Full branch lists: coverage-uncovered.txt in the job's out/cov.

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Lead 2026-11-19: no fault-injection hooks in this quest; lead amends main.cc and manager.cc targets to the measured numbers (rounded down) and files the hooks as a hunt row. When 72c517 reports: put exact measured numbers and the uncovered ranges (listed in Next 0) in the Evidence table, then mark ready.

- Lead 2026-11-19: coverage.sh and docs/dev/coverage.md are in my file set. coverage.sh now discovers harness/coverage_e2e_<name>.* as workload <name> and honors NOBUILD=1; documented (untested until the coverage job runs).
- max_clients -2 is NOT confirmed (hunt.md row stays UNCONFIRMED/refuted): adjustOpenFilesLimit throws SystemExit(EX_OSFILE=72) for --max-files 300 and for a 1024 hard limit with defaults (pool 200, clients 1000); it logs "Your open files limit of 300 is not enough for the server to start. Please increase ... to at least 1471" (plus "database_pool_size has been reduced to 34"). The real finding: the computed minimum_files (1471) includes the capped default pool, so any limit under 1471 refuses with defaults although the pool would shrink to fit. Lead decides whether to fix.
- Linux fix 868260bbf: get_max_files_per_proc = min(sysconf(_SC_OPEN_MAX), soft limit); the automatic raise walks down in 16 steps, so the 1024 case accepts [1008, 1024]; explicit case exact. Reasoned from system.cc, not run on Linux (no VM job reachable); CI arm64 is the proof.
- CI fixes at the head after 5af2697d2: Linux x86 limit-of-3 case needs fds 0-2 open (fill_stdio); FreeBSD get_executable_path has no OS source (src/main.cc only resolves argv0): test passes the sysctl path. Product gap for a hunt row: FreeBSD get_executable_path(nullptr) is empty, so setup_data_path finds no data beside the executable; add KERN_PROC_PATHNAME in src/main.cc.
- CI data: FreeBSD runs privileged (limit raised back to 3470): cases now probe can_raise_to. Linux x86 UBSan sig_handler :226: still unexplained; the child now prints its exit status/signal and atexit.
- Linux x86 ASan+UBSan sig_handler: probe open EMFILE and atexit reached, yet exit 70: the exit machinery, not the handler, picks 70 (cause not identified: CHECK_IO_FDES honeypot is off in CI; not proven). Test now maps main.cc exit() to _exit(status). Hunt row candidate: what exits 70 at process exit under ASan+UBSan with no descriptors.
