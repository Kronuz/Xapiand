# faultlab-plain-sync: journal

State: ready for review
Quest: .campaign/xapiand/quests/faultlab-plain-sync.md (the lead's; never edit it)

## Doing

Nothing: F1 and N3 are fixed at faultlab:83fbde7 and every job is done.

## Next

1. The lead: re-review or land. CI at faultlab:83fbde7 is running.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261124-091412-7c4ab3 (build-test at faultlab:83fbde7); fails without: 261124-084008-5de615 (plain treated as full, faultlab:f0426b2: the named macOS fsync case FAILs, plus 6 more), 261124-091409-a11424 (F1: order_pushed over every stray, faultlab:abb149e: only "full mode: a stray staged after another thread's directory sync ..." FAILs, 16 checks) |
| A2 | 261124-091412-7c4ab3 |
| A3 | `docs/mechanisms/crash-model.md` (Plain and full syncs), `docs/api.md`, `README.md`, `include/faultlab/faultfs.h`, `CHANGELOG.md` (2026-11-24) at faultlab:eefba53 |

## Result

faultfs models plain and full syncs: `filesystem::set_flush_mode(flush_mode, sync_platform)` and
`sync_call_at(sync_site)` (types.h `call_for`, `persists`). Docs: faultlab
`docs/mechanisms/crash-model.md#plain-and-full-syncs`. On macOS a plain fsync survives a kill,
and a crash may drop it (the sweep reaches both outcomes). F_FULLFSYNC, and fsync or fdatasync on
Linux, survive every crash. F_BARRIERFSYNC orders only, and a plain directory sync settles
nothing. The default is full on the host, so users that set no mode see no change: faultfs,
model, policy, seeds, sector, recorder, scenario and the demo pass unchanged. Open: completion's
own flush-mode enum. Once completion:flush-mode lands, journal-sync-mode maps it to
faultlab's enum (R1).

## Review findings

- F1 (blocking, fixed at faultlab:83fbde7): order_pushed walked every stray, so a full-mode race stray got
  an at_mark at an aliasing later point (states oX and o0). It now walks `pushed_`, the strays push()
  staged, cleared wherever strays_ is. Test: model_test "full mode: a stray staged after another
  thread's directory sync ...", which uses the reviewer's R4c injector. Mutant: 261124-091409-a11424. Probe: 261124-091414-2d1c19 at 83fbde7. R4c now reaches only `A A0 AX o`, as on main (no o0/oX). The default-mode differential is identical to main, and the plain property shows 0 O and 0 L violations over 192000 crashes.
- N3 (fixed): crash-model.md says the plain model holds only where F_BARRIERFSYNC orders (Apple SSDs).
- N1: no change. N2: left to journal-sync-mode or the lead. N4: left (the lead filed it).
- The lead's F1/N3 message: answered by the fix above.

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
