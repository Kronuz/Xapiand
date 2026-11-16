> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-11-02) the X6 developer's ledger (phase 7c step X6: the deletions, the destructor asserts, the stop's one bound, `async_check --strict` in CI); built and proved on Release, Debug, ASan+UBSan and TSan; not reviewed; local branch `phase7c/x6-deletions`, not pushed.

# phase7c X6 (build): delete what 7c replaced, enforce the never-block rule

Branch `phase7c/x6-deletions` in `Xapiand-wt/x6` (local only, from master `Xapiand:d3f6cc1fd`). Design: `docs/design/phase7c.md` (D4, D13, D14, D15, D16, T1, section 5's X6); what X3 and X5 left: `docs/status/phase7c-x3.md`, `phase7c-x5.md`;
the carried conditions: strata `docs/status/road-to-release.md`. How it works: `docs/mechanisms/control-plane.md` (the stop).

## State

All six criteria are built and proved. Nothing is blocked. Overlap with X4 (`phase7c/x4-lua`): none in `src/lua/`; the branches will conflict textually in `docs/src/content/docs/changelog.md` and possibly `harness/async_allowlist.tsv`.
Two things for the lead (below, "Findings"): `--strict --owed` cannot be the gate until X1 and X4 land, and D16's order differs from the code's in one step.

## Done criteria

- [x] **K1. Deletions (D4, D13, D15).** `Xapiand:432f9a45f`.
  - Gone: `--committers`, `--fsynchers`, `--http-servers` (with `num_committers`, `NUM_`/`MIN_`/`MAX_COMMITTERS` and `*_HTTP_SERVERS`), the `num_committers` statistic, the `xapiand_http_clients_*` gauges (4) and the always-zero `xapiand_committers_capacity` and `_pool_size`. The reactor count is `ceil(--processors)`
    (the 10-loop cap goes with D15); the fsync pool is `ceil(0.5 x processors)`, 1 to 10, with no option. The debug-only resume wrapper `Hop` in `engine/host_asio.h` is `Resume`. Nothing named offload, `thread_pool`, `release_on_pool`, `DocPreparer`, the cleanup thread or `control_offload` is left in `src`, `tests` or the docs
    (grep; `harness/profile_light.*` and `baseline_*` keep their words for the pre-7c baseline, which the lead still measures: `profile_light.py` says a post-7c binary has no hop).
  - Remove, don't refuse: `harness/removed_options_check.sh BIN` runs each of the six removed options (the three above and `--http-clients`, `--bulk-preparers`, `--bulk-indexers`): all are `The following arguments were not expected` (exit 109) and none is in `--help`. It runs in CI.
    Mutant **K1-committers** (the option put back in `opts.cc`): the script fails (`--committers still parses`, `still in --help`). Docs: changelog (`Removed`), `options.md` (`--processors`), the man page.
  - `harness/thread_census.sh` under `stall_load.py --dry-run --census --phase loaded` on the Release binary (16 CPUs, macOS, 20,000 documents, heavy load running): `main` 1 (the control loop), HTTP loops 16 (`unnamed` on macOS), `Xapiand:W` 16 (the `WorkPool`), `Xapiand:LOG` 1, `Xapiand:WL` 8 (WAL writers), `Xapiand:FS` 9 (8 fsynchers and the debouncer wheel); total 51, offenders none.
    That is section 2's list (WL and FS until 7b); completion's bounded fallback worker did not start. No committer, cleanup, preparer or indexer thread exists. (macOS names threads from a `sample`; the Linux gate VM reads `/proc`.)
- [x] **K2. Destructor asserts (D14).** `Xapiand:0cc864607`, `Xapiand:640a6bf84`. `src/at_end.h` (`at_end::assert_none`: aborts in a build without `NDEBUG`, says who ended with what). `~ShardEndpoint` checks `outstanding()`: references, the turn held or awaited (`try_lock`), views readers hold
  (`engine::Shard::readers_claimed()`, from the index's admission gate; it replaces the unused `refs()`); `~IndexChores` checks that its group is idle (`on_idle` probe, no wait); `~DatabasePool` checks `detached_`.
  - Test `test_pool.cc test_a_registry_that_ends_with_work_outstanding_aborts`: per rule a forked child, once with the work outstanding (SIGABRT and the message) and once without (exits 0): the turn held and awaited by a second task, a view held by a search, a chore running, a detached task counted, a reference alive.
    Debug only (`#ifndef NDEBUG`); a child has a 30 s alarm, so a missing assert fails instead of hanging.
  - Mutants on the Debug build, each removes one check, all killed: **K2-refs**, **K2-turn**, **K2-views**, **K2-chores** (the child hangs in `~WorkGroup`: the alarm ends it, 2 checks fail), **K2-detached**, **K2-dtor** (the destructor stops calling `outstanding()`).
  - Debug ctest 29 of 29, so no existing test ends an entry with work outstanding.
- [x] **K3. Shutdown (D16).** `Xapiand:402a03ce5`, `Xapiand:3dd4dd5f1`.
  - Stated: `docs/mechanisms/control-plane.md` ("The stop: its order and its one bound") and `options.md` ("Stopping the node"). Order: control drains, **Stop 1** the HTTP service (`abort_all`, acceptors closed, sessions finish within the grace, loops stop), **Stop 2** the autocommits, **Stop 3** the indexes, **Stop 4** the write-ahead logs, **Stop 5** the fsync threads, then destruction.
    Each step logs its `Stop N` line. Test `test_node.cc test_the_stop_runs_in_its_stated_order`: a node that served a PUT is stopped, exits 0, the five lines appear in order and once each. Mutant **K3-order** (the WAL block moved before the registry's): killed.
  - One bound: `join()` takes `deadline = now + shutdown_write_bound` (35 s); the control drains, the service's wait and the registry's loop end by it; the registry loop also ends at the deadline when the service stopped at once (it had no limit then); a second signal ends it (`insisted()`).
    The abandoned path is `abandoned_` (service or registry). The stop is the graceful wait plus 35 s, plus the flush of the logs (no limit of its own). `options.md`, the changelog and the note say so.
  - Tests (the bound 2 s, measured from the child's `SHUTDOWN_AT` line to its `Exiting` line, margin 800 ms): `test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound` (the two-bound code ends at about 4.5 s: killed by K3-twice), `test_a_registry_that_cannot_close_ends_the_stop_at_the_bound` (a held turn and no request: exit 75 at 2 s, `Shutdown abandoned the indexes that are still in use`),
    `test_a_second_signal_ends_the_shutdowns_wait` (bound 60 s, a second SIGTERM at 1 s: exit 75 within 20 s). Mutants: **K3-twice** (`deadline + bound` in the registry loop): 3 checks fail; **K3-unbounded** (no deadline in the registry loop): the node never exits, exit code and log checks fail; **K3-signal** (`insisted()` removed there): the second-signal test fails.
- [x] **K4. Enforcement in CI.** `Xapiand:3189ab438`. `.github/workflows/ci.yml`, job `build-test`, right after the build: `harness/async_check.sh --strict --extern strata=build/_deps/strata-src`, and `harness/removed_options_check.sh build/bin/xapiand`. The script falls back to `python3` where `/usr/local/bin/python3` is absent.
  - Master failed `--strict` before this step (three stale count rows after X5's housekeeping pass: `pool.cc` mutex 20, `pool.cc` yield, `shard.cc` mutex 12); the rows are corrected in `Xapiand:0cc864607`. At the head: 264 hits, allow 219, owed 31, compiled out 13, **0 unclassified**, exit 0.
  - The plant (run locally, the CI command, since the job cannot run without a push): `engine::sync_wait(task::task<void>{});` inserted at the top of `metrics_view` in `src/server/search_views.cc` gives `search_views.cc:768: [sync_wait] unclassified` and **exit 1**; removed, exit 0. (A `std::fflush` in `at_end.h` was the first real catch: `Xapiand:640a6bf84`.)
  - The gate tools' own tests: `python3 -I harness/test_gate_tools.py` 14 of 14.
- [x] **K5. Docs.** `Xapiand:fe13fc906`. `ARCHITECTURE.md`: the `DatabaseCleanup` box and its edge are out of the D2 source and `assets/runtime.svg` is re-rendered with the source's own command (`d2 --theme 0 --pad 20`), the committer debouncer is gone from the box, and the networking section no longer has an offload `thread_pool`;
  `docs/src/content/docs/architecture/internals.md` replaces the Worker-tree paragraph with the never-block rule and its CI check; `docs/mechanisms/control-plane.md` no longer narrates replaced threads. Grep for `committer|cleanup thread|DatabaseCleanup|thread_pool|offload|bulk preparer|http server` over the docs, `man`, `README` and `AGENTS`: only the changelog and `docs/dev/side-findings.md` (history) remain.
  The SVG was checked with `rsvg-convert` (its fonts clip two container titles; the committed original does the same).
- [x] **K6. Proof.** At `Xapiand:fe13fc906` plus the test commit `Xapiand:3dd4dd5f1` (the later commit only drops a `fflush` from the debug abort path, rebuilt; Debug ctest at the head, `Xapiand:640a6bf84`, is 29 of 29 and the death tests pass). Separate builds: Release; Debug; ASan+UBSan (`RelWithDebInfo`, `detect_leaks=0`); TSan.

| | Release | Debug | ASan+UBSan |
|---|---|---|---|
| ctest | 29 of 29 | 29 of 29 | 29 of 29 |
| docs E2E (newman 6.2.3, `harness/engine_e2e.sh`), 371 requests | 641 of 641 | 641 of 641 | 641 of 641 |
| `graceful_shutdown_check.sh`, `wal_recovery_check.sh`, `cli_roundtrip_check.sh` | all rc 0 | all rc 0 | all rc 0 |

  - No "blocks on database work" and no sanitizer line in any E2E node log. `test_node` (with the final measure) passes on Release, Debug and ASan.
  - TSan, the concurrency targets (`pool`, `engine_host`, `engine_error`, `blocking_guard`, `wal_replay`, `engine_strata`, `engine_search_strata`, `debounce_or_run`, `reactor_requests`, `control_plane`, `node`): 0 reports. The first run had two failures that were not reports: `test_control_plane`'s lag p99 (the host was loaded by other builds; it
    passes alone, 113 of 113) and the node tests' stop margin (process teardown under TSan is slow: fixed by measuring to the log's `Exiting` line, `Xapiand:3dd4dd5f1`); `control_plane` and `node` then 2 of 2 on TSan, 0 reports.
    `harness/engine_mixed_load.py --engine strata --secs 120`: 17,148 documents, the final dump equals the model, 0 failures, 0 sanitizer reports.
  - Mutants, one per rule, all killed (`.scratch/mut.py`: apply, build one target, run one test, restore): K1 1, K2 6, K3 4, K4 the planted `sync_wait` (above). K5 is documentation: no rule to mutate.
  - Every new decoder or parser: none (no fuzz target owed).

## Findings for the lead

1. **`--strict --owed` is not the gate yet.** 31 hits stay owed: D10 4 (`src/lua`, X4), X1 15 (the Xapian paths, `wal.cc`, `utils.cc`), D4 4 and D5 2 (`engine/strata/shard.cc`: the open-core registry's mutex and the directory check in the open chore), D6 4 (strata's `drain`), D7 2 (`storage_commit` in the commit chore).
   CI runs `--strict` (unclassified fails). Once X1 and X4 land, the D4, D5, D6 and D7 rows are the only ones left; they name decisions that X2 and X3 built, so each row needs a read at its line to become `allow` (done by the owner of that code), and then `--strict --owed` can replace `--strict`.
2. **D16's step order differs from the code in two places.** The acceptors close at the start of `join()`, after the legacy wait for in-flight requests (`shutdown_checks`, with the new connections still accepted, up to about 45 s); D16 closes them first. And the loops stop before the indexes close (the registry closes on the main thread through `finish_idle_close`, so it needs no loop).
   I stated the code's order and did not change the graceful wait, as the task said. Making the acceptors close first means `shutdown_checks` calls the service's stop at once: a separate step if the architect wants D16 to the letter.
3. **The abandoned registry exits 75 with `Shutdown abandoned the indexes that are still in use`**, also when no request is in flight (an index whose close does not finish in the bound, for instance on a hung disk). Before, that wait had no limit. The next start replays the logs.
4. **`--processors N` now sets the reactor count with no cap** (was `min(10, processors)`). On a 64-CPU host that is 64 loops.
5. The census on the Linux gate VM (`/proc`) is the T1 evidence; the macOS run above uses `sample` names.

## Done (commits)

- `Xapiand:432f9a45f` K1: the thread options, the dead statistic and gauges, `Hop`, `removed_options_check.sh`.
- `Xapiand:0cc864607` K2: `at_end.h`, the three destructor checks, `readers_claimed`, `test_pool` death tests, the allowlist counts.
- `Xapiand:402a03ce5` K3: one deadline in `join()`, the `Stop N` lines, the tests, the note, `options.md`, the changelog.
- `Xapiand:3189ab438` K4: the CI steps and the interpreter fallback.
- `Xapiand:fe13fc906` K5: the diagram, ARCHITECTURE, internals.
- `Xapiand:3dd4dd5f1`: the node tests measure the stop from the log's clock.
- `Xapiand:640a6bf84`: the `fflush` the strict check flagged.

## Next

- Review (the lead's skim). Sensitive area touched: shutdown (`join()`).
- The lead: findings 1 and 2; rebase onto master when X4 merges (conflicts: changelog, allowlist rows); the Debug, ASan and TSan build directories are deleted (disk was under 20 GiB); `Xapiand-wt/x6/.scratch/build-rel` stays, and `.scratch/final.sh` rebuilds the others and reruns the proof.

## Blocked on

Nothing.

## Log

### 2026-11-02

- Read D4, D13 to D16, T1, X6, the X3 and X5 ledgers and the strata carried conditions. `--http-servers` is the reactor count, so D15's removal means the count follows `--processors` (the tests and the crash check pass `--processors`).
- Master failed `async_check --strict` (three stale rows) before any change; fixed with K2.
- A `std::fflush` in the new header was an unclassified hit: `--strict` caught it on the first run after K2.
- The first K2-chores mutant run hung (the forked child waited for a chore it should have refused to destroy) and left an orphan; the child now has an `alarm(30)`.
- The scripts and logs are in `Xapiand-wt/x6/.scratch/` (`final.sh`, `k2-mut.sh`, `k3-mut.sh`, `k3-order-mut.py`, `mut.py`).
