> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-10-29) ledger of the `phase7a/t11-barriers` workstream (performance engineer); not landed.

# Ledger: T11 commit barriers

## State

Gate 7a T11 CPU row on the gate's estimator: 0.372x Xapian (pairs 0.355 to 0.408) on the landing heads, against 0.465x (A/A 0.478x) for master's code in the same session (an earlier grid on the pre-fix heads: 0.396x against 0.489x). Three experiments, no loser, nothing landed. Branches are pushed for completion, strata and prospect; Xapiand's branch is local.
The architect's scoped check (`strata:docs/reviews/commit-barriers-durability.md`) rejected on CB-1 and CB-2; both are fixed below (L1, L2) with C1; C3 is a recorded follow-up and the lead handles C2. Waiting for the re-check.

## Done

- E1 fold the first directory barrier into the data batch: completion `cdf1cb3`, strata `0765e28`.
- E2 order the manifest temporary with a barrier: completion `5a338d8`, strata `307d08b` (superseded by E3, kept in history as the weaker-assumption fallback).
- strata faultfs models ordering points: strata `67671ec`.
- E3 one ordering point and one flush: completion `fdfb94f`, strata `d01a1c4`.
- Cleanup, docs and pins: completion `3665ee7`, strata `bbfb986` `cccb2f9`, prospect `23207cc`, Xapiand pin commit `d153d95f3`.
- TSan race fix (the commit copied a whole segment ref while a merge read it): strata `e471404`, prospect `b9b3c02`, Xapiand pin `d7d98d74e`.
- Measurements, raw data, probe: `docs/perf/t11-barriers/`.
- Review fixes: completion `03cef1d` (pushed), strata `16e9054` (pin), prospect `828cd4a` (pin), Xapiand pin in the commit that carries this ledger.
  - L1 (CB-1): `DeviceSyncCalls::barrier` (`posix.h`) falls back to `F_FULLFSYNC` only on `ENOTSUP`, `EOPNOTSUPP`, `ENOTTY`; any other error returns to the caller. EINVAL is excluded: Apple's `fcntl(2)` lists no such use, and an unknown `fcntl` command returns `ENOTTY` here (probe `.scratch/errno/p.c` on APFS, a FAT and an exFAT volume: both accept the barrier, command 0x7fff gives `ENOTTY`). Test `completion/test/sync_calls_test.cc` (`errno_rule`): `EIO`, `ENOSPC`, `EDQUOT`, `EINVAL`, `EBADF` throw with one barrier and no flush; the three unsupported errors fall back; a failing fallback throws; `EINTR` retries the barrier. Revert check (fall back on every error) fails the test; the architect's `TRACE_FAIL_BARRIER=EIO` trace now aborts at the first commit with `index_fenced: order journal files: Input/output error` and 0 acknowledged commits (was 38 acknowledged).
  - L2 (CB-2): `PosixIO::replace_sync_calls(fsync, fcntl)` (macOS) routes every sync call of `sync_files`, `sync_files_and_directory`, `order_files_and_directory` and `sync_directory`. The test records the sequence on a real directory and asserts exact sequences: ordered = fsync each file, fsync the directory, one barrier, no flush; directory-only ordered; `sync_directory` = fsync directory then flush; durable batches one flush; a foreign file syncs itself. Mutants killed: C5 (directory dropped from the ordered call, the architect's `mutants/c5.py` change), CB-1 revert, no `ENOTTY`, `EINVAL` as unsupported, option ignored, barrier issued as full flush, `sync_directory` without the flush. ASan+UBSan and TSan builds of the test pass.
  - Suites run once at the end on the new heads: completion 17 of 17 (Release); strata lifecycle, index, background_merge, directory, retention, reclaim*, flush_memory, format14, cold_segment, fields, files, install*, shared* 32 of 32; `harness/engine_crash_check.sh` PASS on both engines (strata check passes on 5 shards after each kill -9).
  - C1 (CB-3): completion README, ARCHITECTURE and `docs/PLATFORM_FINDINGS.md` say the barrier orders, is not a flush, and is guaranteed only on Apple SSDs; the T11 README sentence is replaced. `PosixIOOptions::full_flush_ordering` (opt-in, default off) makes `order_files_and_directory` issue `F_FULLFSYNC`; test `full_flush_option`. strata has no sentence to replace (its notes never made the claim); no strata host wiring of the option yet.

## Next

1. Architect's re-check of CB-1, CB-2 (reruns `run-trace.sh` with and without `TRACE_FAIL_BARRIER=EIO`, the completion test against `c5.py`).
2. Lead: land completion, then strata, prospect, Xapiand pins; rerun gate 7a T11 on `main`.
3. Follow-up C3 (CB-5): faultfs keeps post-mark writes to a staged file whole; snapshot staged bytes in `stage()` and treat later writes as unsynced in `crash()` before any protocol writes after a barrier. Not done.
4. Follow-up: a host wiring for `full_flush_ordering` in strata's index options (owner's ruling Q1 on the default).
5. Unmeasured: macOS arm64 (probe and bulk), Linux and FreeBSD (unchanged by construction); power loss on hardware.
6. Optional: investigate the +3% peak RSS (3 runs per arm).

## Blocked on

Nothing.
