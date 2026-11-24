# faultlab-plain-sync: faultlab: faultfs models a plain sync (survives a process crash, not a power cut)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-plain-sync`.

## Outcome

faultfs tells a plain sync from a full one. A full sync makes the data it covers survive both a
process crash and a power cut, as every sync does today. A plain sync (macOS fsync) makes it
survive a process crash, but at a power cut faultfs may drop it like unsynced data: the drive's
cache is lost. The crash-model sweeps of faultfs's users can then run a plain-mode journal and see
exactly what that mode loses.

## Acceptance criteria

- A1 Value tests: after a plain sync, a process crash keeps the data and a power cut may drop it
  (the sweep enumerates both outcomes). After a full sync, a power cut keeps it. A mutant that
  treats a plain sync as full is killed by a named test (`--fails-without`).
- A2 faultfs recognizes the sync calls completion makes in each mode (fsync, F_FULLFSYNC,
  F_BARRIERFSYNC, and fdatasync on Linux), with a test per call. On platforms where plain already
  makes the full call, the behavior is unchanged.
- A3 [artifact] faultlab's docs state the model per sync kind; `CHANGELOG.md` names the change.

## Contracts

Provides `faultlab:plain-sync`. Existing users see no change unless they issue plain syncs on
macOS.

## Decisions

- Lead, 2026-11-24: split from journal-sync-mode, whose criterion A2 (the crash model in both
  modes) needs it.
