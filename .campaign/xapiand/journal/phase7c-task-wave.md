# phase7c-task-wave: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-task-wave.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:b6b8f7534 on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | open: left unticked, deferred to the next half (the http wave) in `docs/status/phase7c-task-wave.md` |
| A2 | `docs/status/phase7c-task-wave.md`: "[x] 2 `Host` forwards `roots`, `runs_here`, `post_call` (C9); `Run` forwards them too. Mutants M6, M7 die" |
| A3 | `docs/status/phase7c-task-wave.md`: "[x] 3 Four-phase teardown on C3: chores cancelled and drained (`IndexChores::cancel_all`)" |
| A4 | `docs/status/phase7c-task-wave.md`: "[x] 4 control_plane's published wakes cleared by a scope guard (`Unpublish`)" |
| A5 | `docs/status/phase7c-task-wave.md`: "[x] 5 `BlockingScheduler::run` (Host::run) owns no roots, so its spawn always runs" |
| A6 | open: left unticked, deferred to the next half (F2, the http wave) in `docs/status/phase7c-task-wave.md` |
| A7 | `docs/status/phase7c-task-wave.md`: "[x] 7 Shutdown test under ASan with `live_frames()`" |
| A8 | open: left unticked, deferred to the next half (H1 removes the caller) in `docs/status/phase7c-task-wave.md` |
| A9 | open: left unticked, deferred to the next half (reactor R) in `docs/status/phase7c-task-wave.md` |
| A10 | `docs/status/phase7c-task-wave.md`: "[x] Floor for this half: separate ASan+UBSan and TSan builds on Linux, macOS ASan" |
| A11 | `docs/status/phase7c-task-wave.md`: "[x] g++-13 ctest: the hang stays at C3 (Evidence)" |
| A12 | `docs/status/phase7c-http-wave.md`: "Done criteria (the lead's owed items; tick only with evidence)" |

## Result

Landed at Xapiand:b6b8f7534 on 2026-11-03. macOS ASan checks: control plane 127, reactor requests 347, blocking guard 14, engine host 99; no reports. No review named in the ledger.

- Merge: Merge branch 'phase7c/task-wave' (task C3; Host forwarding; four-phase teardown; wake guards; live_frames test).
- `Xapiand:6a15db3db`, `a7bae3742`, `286c97539`, `58a734d0b`, `8bdb3badb`, `0be2c2597`, `6d85bcbab`, `80e158db2`, `7b4317502` recorded.
- Docs E2E: 644 of 644 assertions; graceful shutdown scenarios pass.
- Linux g++-13 Release ctest: 32/35; known pool and blocking-guard hangs; node passes alone.
- Linux clang 18 TSan ctest: 34/35; pool exception report remains.
- Lua bounds Release rc 0; async checker 0 owed, 0 unclassified.
- Items 1, 6, 8, 9 and HTTP service phases landed in `phase7c-http-wave`.
- Docs: `docs/mechanisms/control-plane.md`.
- Left open: A1, A6, A8, A9.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
