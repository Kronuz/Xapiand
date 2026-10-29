> Status: CURRENT (2026-10-29) T11 commit-barrier experiments, workstream `phase7a/t11-barriers` (performance engineer). Not landed; awaiting the architect's durability check.

# T11: fewer device flushes per strata commit on macOS

Target: gate 7a T11, engine rows at most 0.5x Xapian's CPU (the gate's estimator: smoke CPU per document times the engine rows' profile
share). The rerun on master `8f6332a01` measured 0.515x (pairs 0.48 to 0.57); the varying part is `basic_index::commit_files`, which waits on
`F_FULLFSYNC` and which the profile counts as busy.

Host: macOS 15 x86_64 (16 logical CPUs), APFS, `quiet exclusive`. Procedure as `docs/perf/t11/README.md`: `harness/profile_bulk.sh`, 20000
accounts documents by one RESTORE `?commit`, fresh `--solo` node per run, `sample(1)`; each round runs xapian, then each strata arm, then base
again (the A/A). One binary build per arm (`RelWithDebInfo`, frame pointers, no LTO, `BUILD_TESTS=ON`). Raw: `raw/`.

## Arms

| Arm | strata / completion | Flushes per commit |
|---|---|---|
| A, A2 | master code (strata `6d597cf`, completion `89613a7`, prospect `4384c72`) | 4 full |
| B1 = E1 | strata `0765e28`: directory folded into the data batch | 3 full |
| B2 = E2 | strata `307d08b`: B1 plus the manifest temporary ordered with a barrier | 2 full, 1 barrier |
| B3 = E3 | strata `d01a1c4`: one ordering point for data, directory and temporary | 1 full, 1 barrier |
| F | final heads, pinned through prospect `23207cc` (strata `cccb2f9`, completion `3665ee7`); E3 without the dropped per-file call | 1 full, 1 barrier |

A is master's code with completion at `main` (it differs from the Xapiand pin `7734544` by CL-2's owner-lock work, which is off this path).

## Results (four valid rounds, 2 to 5; ratios are strata over Xapian, pair by pair)

Gate estimator (engine CPU per document, smoke CPU x profile share):

| Arm | mean (ms) | spread | / Xapian | pairs |
|---|---|---|---|---|
| A (base) | 0.2041 | 2.6% | 0.495 | 0.497 0.479 0.495 0.508 |
| A2 (A/A) | 0.2079 | 7.4% | 0.504 | 0.516 0.484 0.517 0.498 |
| E1 | 0.1934 | 6.4% | 0.469 | 0.461 0.441 0.491 0.482 |
| E2 | 0.1738 | 7.1% | 0.421 | 0.401 0.415 0.440 0.429 |
| E3 | 0.1645 | 3.4% | 0.399 | 0.388 0.392 0.410 0.405 |

Second grid, base against the final heads F (Xapian mean 0.4203 ms, spread 3.2%):

| Metric | A | A2 | F | F / Xapian (A / Xapian) |
|---|---|---|---|---|
| Engine CPU per doc, gate estimator (ms) | 0.2057, spread 5.9% | 0.2038, 8.1% | 0.1664, 3.2% | **0.396** (pairs 0.405 0.383 0.398 0.397); A 0.489, A2 0.485 |
| Engine CPU per doc, profile sample count (ms) | 0.2188 | 0.2181 | 0.1734 | 0.379 (A 0.479) |
| Engine on the critical thread (ms/doc) | 0.0178, spread 8.7% | 0.0183, 17.8% | 0.0166, 8.7% | 0.193 (A 0.207): inside the A/A, unchanged |
| Whole process CPU per doc (ms) | 0.8239, 1.5% | 0.8126, 1.9% | 0.8162, 2.1% | 0.659 (A 0.665): unchanged |
| Wall of the 20000-document RESTORE (s) | 3.44, 5.1% | 3.42, 1.5% | 2.90, 1.9% | 0.558 (A 0.662): 16% less |
| Docs/s | 5817 | 5849 | 6893 | 1.79x Xapian's (A 1.51x): 18% more than base |
| Peak RSS (MB, 3 runs) | 835 to 853 | | 857 to 863 | about +2% (not explained; within a few runs' reach) |
| Index directory after `?commit` (KB, files) | 2196, 60 | | 2124 to 2196, 60 | unchanged |

With the first-exec round kept (five rounds), E1's mean pair is 0.438 (one invalid pair 0.312) and F's 0.362 (one invalid pair 0.221); see `raw/INVALID.md`.

All four axes: time improves (16% less wall, 18% more docs/s), throughput the same figure, memory about +2% RSS (3 runs per arm, unexplained, small), disk unchanged
(same 60 files). What the estimator measures: whole-process CPU did not move (0.824 to 0.816 ms per document), because a flush wait
is blocked time, not CPU. The engine row falls because the profile counts the wait as busy (the same property the T11 README noted); the
real effects are 0.54 s less wall per 20000 documents and a commit that blocks its writer for about 25 ms instead of 86 ms (probe).

Savings, honestly: no CPU-hours; the bulk load's CPU per document is flat. What moves is time blocked on the device: about 60 ms less per commit in the probe
(four flushes to one flush plus a barrier), 0.54 s less wall on the 20000-document load. The gate row moves because its estimator counts that wait as busy.

## Probe (the prediction, `raw/commit_probe.cc`, median of 30 interleaved rounds)

| Pattern | 3 files of 200 KB | 9 files |
|---|---:|---:|
| Before (4 full) | 85.7 ms | 89.6 ms |
| E1 (3 full) | 63.9 ms | 68.9 ms |
| E2 (2 full, 1 barrier) | 45.1 ms | 48.9 ms |
| E3 (1 full, 1 barrier) | 24.9 ms | 27.9 ms |

## Experiments

| # | Change | Ratio vs base (gate estimator) | A/A | Kept |
|---|---|---|---|---|
| E1 | `IO::sync_files_and_directory`: first directory barrier into the data batch | 0.469 vs 0.495 (-5%, pairs overlap the A/A) | 1.02 | yes, a stepping stone; weakest claim |
| E2 | E1 plus `File::sync_ordered` (F_BARRIERFSYNC) for the manifest temporary | 0.421 vs 0.495 (-15%) | 1.02 | superseded by E3; in the history as the fallback |
| E3 | `IO::order_files_and_directory`: one ordering point before the rename, one full flush after | 0.399 vs 0.495 (-19%) | 1.02 | yes; E2's `File::sync_ordered` is dropped |

No loser. Each was kept on its own A/B against the previous arm (E1 inside the A/A noise on its own: pairs 0.46 to 0.49 against 0.48 to 0.52).

## Ordering argument

Invariants of the commit (strata `open()`): it opens the highest manifest generation, fails closed when that manifest is invalid or names a
missing or different file, and a commit is acknowledged only when the last directory flush returns. A crash must leave either the previous
manifest or the new one with every file it names intact.

1. **Directory folded into the data batch (E1).** The directory `fsync` joins the files' before one flush. When the flush returns the bytes
   and the entries are durable, which is everything the second flush added; no later step depended on the state between them.
2. **Manifest temporary: barrier instead of flush (E2).** `fcntl(2)`: `F_BARRIERFSYNC` persists I/O fsync'd on the device before it ahead of any I/O issued
   after it. The temporary is complete on the media before its rename can be; the final directory flush makes both durable.
3. **One ordering point (E3).** The data files, their entries and the temporary are all fsync'd before the barrier; the rename is issued after
   it. A visible manifest therefore names files and entries that were written first; the final fsync plus full flush is unchanged and is the
   acknowledgement point. A file on another device is flushed in full inside the call, since a barrier orders one device.

What E3 trusts beyond the old protocol: that the drive honors a barrier (as the old one trusted it to honor a flush), and that APFS issues the
rename's metadata after the barrier call returns (it is issued after, in program order). In-memory state (segment refs, meta, fields) now changes after
the commit is durable; a failed write leaves the next commit to redo it under new names.

## Correctness

- completion: 16 of 16 tests, including a new model with a barrier call (stage-before-flush, one barrier on the directory's device, full flushes elsewhere, faults and `EINTR` at each call); CI dispatched on `perf/commit-barriers`.
- strata: 57 of 57 tests; `faultfs` models the ordering point (after a crash, a change made after it survives only if everything staged before it did; both outcomes reachable, 400 seeds). The crash suites (index, lifecycle, directory, background merge, reclaim, retention, fields) enumerate crash points through commits.
- Mutants die: with the ordering mark removed from `faultfs`, lifecycle, index and directory fail (E2 and E3 each); with the manifest temporary left out of the ordered call, or the data files left out, the same three fail.
- prospect, same suites on the search layout: 9 of 9 (`index`, `lifecycle`, `directory`, `background_merge`, `reclaim`, plain and mapped).
- Xapiand: `ctest` 28 of 28; `harness/engine_crash_check.sh` PASS (T8: Xapian 5581 of 5581 acknowledged writes recovered, strata 6247 of 6247, `index::check` passes on 5 shards after each kill and restart).
- Not covered: power loss on real hardware (nothing here can pull the plug; `kill -9` leaves the page cache intact, so T8 exercises atomicity, not device ordering). The `XAPIAND_CRASH_POINTS` points sit outside `commit_files`, so that build was not repeated.

## Platforms

macOS x86_64 APFS only. Linux and FreeBSD: both calls fall back to `sync_files` then `sync_directory`, unchanged and not helped. macOS arm64: not measured.

## Branch heads

completion `perf/commit-barriers` `3665ee7`; strata `perf/commit-barriers` `cccb2f9`; prospect `perf/commit-barriers` `23207cc`; Xapiand `phase7a/t11-barriers` (local, not pushed).
