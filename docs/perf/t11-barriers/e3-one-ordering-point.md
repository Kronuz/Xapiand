> Status: CURRENT (2026-10-29) experiment E3 of `docs/perf/t11-barriers/README.md`; result is recorded there.

# E3: one ordering point for the data, the directory and the temporary

Workstream: `phase7a/t11-barriers`. Builds on E2.

## Hypothesis

Nothing needs to be durable before the rename; it needs to be ordered. Stage the data files, the directory and the temporary with `fsync`,
issue one barrier, rename, and flush once (the commit's acknowledgement point). One full flush plus one barrier per commit; the probe predicts
45 ms to 25 ms.

## Change

completion `IO::order_files_and_directory(files)` (default: `sync_files_and_directory`; macOS `PosixIO`: fsync everything, one barrier on the
directory's device, a full flush on any other device the files sit on). strata `commit_files` builds the manifest from locals, writes the
temporary, makes one call with every file and the temporary, renames, and runs `sync_dir()`; the index's own state follows the durable commit.
strata's faultfs models the ordering point (after a crash a change made after it survives only if everything staged before it did).

## Procedure and decision

As E1, arm E3 against the best earlier arm. Keep if the engine CPU row falls by more than the A/A spread and the crash tests pass; the
mutant that drops the ordering in faultfs must fail them.

## Ordering argument

Everything R1 needs (data bytes, their entries, the temporary's bytes) is fsync'd before the barrier; the rename is issued after it. By the
barrier's definition the rename cannot persist without them, so a manifest visible after a crash is complete and names durable files. The
last directory flush (fsync plus full flush) is unchanged and is the acknowledgement point: everything is on the media by then. Files on a
device other than the directory's are flushed in full before the call returns, since a barrier does not order across devices.
