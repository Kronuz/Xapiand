> Status: CURRENT (2026-10-29) experiment E1 of `docs/perf/t11-barriers/README.md`; result is recorded there.

# E1: fold the first directory barrier into the data batch

Workstream: `phase7a/t11-barriers` (performance engineer). Targets gate 7a T11, the engine CPU row on the gate's estimator.

## Query class and footprint

The commit path of the strata engine on macOS: `basic_index::commit_files`, hit once per `?commit` and per autocommit flush. A commit made
four device flushes (`F_FULLFSYNC`): the data batch, the directory, the manifest temporary, the directory after the rename. The T11 profile
counts the wait as busy: 655 to 1154 samples of a 3500 to 4400 sample row (17 to 26%).

## Hypothesis

The directory sync before the manifest temporary exists only so the data files' directory entries are durable before the manifest is written.
Staging the files and the directory with `fsync`, then flushing the device once, gives the same durable state at the first flush's return, so
the second flush is redundant. Four flushes become three; the probe (`completion` `.scratch/probe/commit_probe.cc`, 3 files of 200 KB, 30
rounds) predicts 86 ms to 64 ms per commit.

## Change

completion: `IO::sync_files_and_directory(files)` (additive; the default is `sync_files` then `sync_directory`). `PosixIO` on macOS stages
every file and the directory, then one `F_FULLFSYNC` per distinct device. strata: `commit_files` calls it where it called `sync_files`, and
drops the later `sync_dir()`.

## Procedure and decision

T11's procedure (`harness/profile_bulk.sh`, `quiet exclusive`): interleaved xapian, strata base, strata E1, strata base (A/A), five rounds.
Keep if the engine CPU row (smoke CPU x profile share) falls by more than the A/A spread, and nothing else regresses; remove otherwise.

## Ordering argument

The new first flush follows the `fsync` of every data file and of the directory, so when it returns the files' bytes and their entries are on
stable storage, which is all the old second flush added. No later step depends on the state between the two. The manifest temporary is still
written and flushed after it; the rename and the final directory flush are unchanged. Acknowledgement still follows the last flush.
