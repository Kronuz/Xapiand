# faultlab-strata-gaps review: correctness

Commit: faultlab:b5640d71303b74de041aef039a2c9ff8d9861405
Verdict: APPROVE
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261120-231216-b15c65: the full faultlab suite (Release, 18 ctest entries) at b5640d7, green. It
  covers A1 (the new faultfs_test cases), A2 (model_test), A3 (both new cases) and A4 (seeds_test,
  unchanged in this diff, so the F3 table is checked as recorded).
- 261120-231240-34dc25 and 261120-231241-145756: the A3 `--fails-without` jobs. I diffed both
  mutant commits against b5640d7: faultlab:7d38b8d removes only the two `clear()` lines in the
  after-fault sync branch, faultlab:e25e663 removes only the `open_reclaim_candidate` override and
  the candidate null return. Each fails exactly its own case (16 checks and 1 case respectively).
  The fails-without evidence targets the fixes.
- CI run 44879138800 (A6) was still running when I checked. It is not part of this verdict.

## What I checked myself

Probe job 261120-232249-a2f0a5 at b5640d7, green both as a plain build and under TSan. Script:
`faultlab-wt/xapiand/faultlab-strata-gaps/.scratch/xapiand/review-faultlab-strata-gaps-correctness/probe.sh`
with `probe.cc` beside it. Rerun it at a fix with the same `campaign job submit` command.

1. Q1, the mapping and `power_on()`. strata's `power_on()` resets only `power_left_`
   (strata `include/strata/core/faultfs.hh:231-234`), while faultlab's folds, releases as `kill()`
   does and then resets the power (`include/faultlab/faultfs.h:368-373`). Only two strata places
   call it: `test/standalone_turn_log_test.cc:559` (crash_case) and `:1244` (t6). In both, every
   `host`, `kv_index`, File and owner lock lives in a `try` scope that closes before `cut()`. The
   only object that survives is `faultfs::io`, and faultlab's `io::enter()` does no epoch check
   (`faultfs.h:1460-1463`), so the io stays usable after `release()`. `host_view` hands out token
   owner locks. The `cut(1)` second crash also runs with no handle open. No strata `bench/`, `src/`
   or `include/` code calls `power_on`, and prospect does not call it either. The probe also shows
   that an owner taken before `power_on()` and destroyed afterwards frees nothing it did not hold,
   because its epoch is stale, and that a lease held at the cut is released. I checked the other
   table rows against the code: the `op` order, the `fault` fields, the `check()` call sites per
   kind, the `acquire_owner` create not counting as a mutation, and `sync_data` reaching `sync()`
   through io.h's default in both copies. They hold. The switch needs include and namespace
   changes only, plus the two A3 behaviors and EAGAIN/EWOULDBLOCK, which the table names.
2. Q2, additions only. Apart from the A3 fixes, the diff changes no existing behavior. `check()`
   and `try_lease` only add a counter increment, `sync()` sleeps only when a latency is set,
   `copy_from` and `take` carry the new state, and the `open()` reorder still throws EISDIR and
   ENOENT for plain opens. The F3 seeds pass in b15c65. journal and consensus pin
   faultlab:a3b71d1, so this commit does not touch them.
3. Q3, the A3 fixes on every path. With an after-effect fault, the sync now clears `staged` and
   `at_mark` before `settle_staged()`, as the success path does (`faultfs.h:1114-1118`, `:1130-1133`).
   Before the fix, `settle_staged()` overwrote `durable` with the older staged snapshot. The probe
   passes on:
   - Two ordering points with the file staged at the first only.
   - A file that a point did not stage, written before and after it (the `at_mark` path).
   - A later write after the failed sync, which stays unsynced.
   The before-effect path is unchanged. Reads return the staged bytes, and a crash never returns
   the dropped bytes. Across 32 seeds the outcomes were the staged bytes, empty, or a per-sector
   tear of the two, all pre-existing behavior. For the reclaim fix (`faultfs.h:1271-1273`,
   `:1428-1436`), null comes back for a missing name and for a directory, read-only and read-write,
   in the root and in a subdirectory io. Each call is counted in `calls()`, and plain opens still
   throw. This matches io.h ("Null means absent or unsafe/nonregular") and journal's own MemoryIO,
   which returns null for a missing reclaim candidate.
4. Q4, `sync_latency`. The sleep is byte-identical to strata's
   (strata `faultfs.hh:564`, faultlab `faultfs.h:1105`). Both sleep before taking the lock and
   before `check()`, only in `File::sync`, and `sync_data` and the ordering point's foreign-file
   `f->sync()` reach it the same way. In both, a crash or cut that lands during the sleep applies
   to that sync. Neither has an ordering or durability effect the other lacks. The probe measured
   two concurrent 100 ms syncs at 105-106 ms, so the waits overlap and are not serialized.
5. Q5, thread safety. `ops_` is written only under `mu_` (`check()`, `faultfs.h:471`, called with
   the lock held, and `try_lease`, `:1146`) and read under `mu_` (`:386-389`). `sync_latency_` is a
   single relaxed atomic config value that guards no other data. The probe's stress run (4 writers
   and a watcher that reads counts and toggles the latency) matches the sequential totals, and the
   same run under TSan is clean.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | The A3 sync test covers one ordering point with the file staged at it. The `at_mark` path and a second point are untested in the suite, though the probe shows both correct. | test/faultfs_test.cc:366-388; probe 261120-232249-a2f0a5 | Optional: fold the probe's at_mark and two-point cases into the suite. |
| N2 | no | gap | The journal's "no caller relies on either old behavior" is argued, not run. prospect already pins faultlab:b64102f, the parent commit, and will see the null reclaim opens and the after-fault sync on its next bump. journal's reclamation expects null, as its MemoryIO shows. | prospect CMakeLists.txt:27; journal include/journal/memory_io.h:135-138 | Run prospect's suite at its faultlab bump, alongside faultlab-adopt-strata. |

## Not run

- strata's own tests against faultlab. That is faultlab-adopt-strata's work, and Q1 rests on
  reading its two callers.
- The journal, consensus and prospect suites at b5640d7.
- Platforms other than this macOS host, and CI's sanitizer matrix (run 44879138800 was still in
  progress).
- The A2 port of strata's 16 cases, checked by case beyond the suite's pass.
