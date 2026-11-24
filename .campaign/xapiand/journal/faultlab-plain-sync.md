# faultlab-plain-sync: journal

State: working
Quest: .campaign/xapiand/quests/faultlab-plain-sync.md (the lead's; never edit it)

## Doing

Building and running faultlab's C++ tests locally on faultlab:eefba53 (model, sync value tests).

## Next

1. Green locally; then the mutant branch `mutant/faultlab-plain-sync` (plain treated as full) and
   its `--fails-without` job; `campaign verify faultlab-plain-sync`.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | `docs/mechanisms/crash-model.md` (Plain and full syncs), `docs/api.md`, `README.md`, `include/faultlab/faultfs.h`, `CHANGELOG.md` (2026-11-24) at faultlab:eefba53 |

## Result

faultfs models plain and full syncs: `filesystem::set_flush_mode(flush_mode, sync_platform)` and
`sync_call_at(sync_site)` (types.h `call_for`, `persists`). Docs: faultlab
`docs/mechanisms/crash-model.md#plain-and-full-syncs`.

## Rulings I made

- R1 faultfs mirrors completion's per-mode, per-platform call table itself (types.h `call_for`):
  faultfs replaces PosixIO, so it never sees the syscalls. Its `flush_mode` is faultlab's own enum
  since completion:flush-mode has not landed; journal-sync-mode maps one to the other, or faultlab
  adopts completion's enum at its next completion pin.
- R2 The mode is per filesystem (one device, one mode), set by a setter with default `full` on the
  host platform, so existing users are unchanged; the platform is selectable so Linux CI sweeps
  macOS's plain model too.
- R3 A plain fsync on macOS pushes the bytes: staged at the next ordering point's epoch (a
  stray), so that point orders them device-wide (F_BARRIERFSYNC orders everything the drive got
  before it); a write between the push and the point is recorded in at_mark at the point and
  as_of prefers it at one epoch, so the cut keeps reaching it. Ordering points stage whatever
  their call (contract: order only), unchanged in both modes.
- R4 A persisting File::sync now settles strays when no ordering point is open, as
  sync_directory already did (both are a device flush); before, only sync_directory did. Affects
  full mode only in the concurrent stray race. A mode switched mid-run is then correct: a full
  sync flushes every push.
- R5 In plain mode on macOS nothing settles until a crash, so ordering epochs and their staged
  copies accumulate; documented in crash-model.md. Barrier-unsupported fallback to F_FULLFSYNC is
  not modeled (faultfs has no such filesystem).
- Tests went into test/model_test.cc: CMakeLists.txt is outside the file set, so no new test
  executable.
