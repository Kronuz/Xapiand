> Status: CURRENT (2026-10-29) experiment E2 of `docs/perf/t11-barriers/README.md`; result is recorded there.

# E2: order the manifest temporary with `F_BARRIERFSYNC`

Workstream: `phase7a/t11-barriers`. Builds on E1.

## Hypothesis

The temporary's flush only has to put its bytes ahead of its rename. `F_BARRIERFSYNC` ("fsync, then a barrier to the drive": I/O fsync'd on the
device before it persists before any I/O issued after it) gives that without waiting for the media; the directory flush after the rename makes
both durable. Three full flushes become two plus a barrier; the probe predicts 64 ms to 45 ms per commit.

## Change

completion `File::sync_ordered()` (default `sync()`; `PosixFile` on macOS: `F_BARRIERFSYNC`, the full flush when the filesystem has none).
strata writes the manifest temporary and calls it, in place of `sync()`.

## Procedure and decision

As E1, arm E2 against E1. Keep if the engine CPU row falls by more than the A/A spread.

## Ordering argument

Before the rename, the temporary's bytes are fsync'd and barriered, so the rename (issued after the call returns) cannot reach the media
before them. The rename only becomes durable at the final directory flush, which also flushes the temporary. Data files and their entries are
already durable (E1's flush). So a visible manifest is complete, and every file it names is durable.
