# write-buffer: journal

State: landed
Quest: .campaign/xapiand/quests/write-buffer.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:39c9cc1 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/write-buffer.md`: "[x] K1 Items 2 and 3 as D2-D4, D6, D8 and D10 state them for one writer" |
| A2 | `docs/status/write-buffer.md`: "[x] K2 The full ctest passes unchanged apart from the shims" |
| A3 | `docs/status/write-buffer.md`: "[x] K3 New unit tests per decision (`write_buffer_test`); each WB finding's test is the killer" |
| A4 | `docs/status/write-buffer.md`: "[x] K4 Mutants on every new ordering or ownership check, each killed by a named test." |
| A5 | `docs/status/write-buffer.md`: "[x] K5 ASan+UBSan (Debug -O1, Homebrew clang 23, macOS x86_64): ctest 44 of 44" |
| A6 | `docs/status/write-buffer.md`: "[x] K6 Memory against the base's (`origin/main` `fed0c85`)" |
| A7 | open: left unticked; CI on the ledger-only head not awaited in `docs/status/write-buffer.md` |

## Result

Landed at strata:39c9cc1 on 2026-10-26. ASan+UBSan ctest: 44 of 44; TSan ctest: 44 of 44. Review: `docs/reviews/write-buffer-design.md` (WB-1 to WB-19, C1-C7); `docs/reviews/write-buffer-step1-code.md` (code review; CR1 to CR6 closed; scoped re-check pending).

- Merge: Merge branch 'fix/write-buffer-followups'.
- strata:74f122c builds items 2 and 3; strata:c346f10 adds ASan verification and a bench fix.
- strata:a4b83e7 records the mutant checks; CR1-CR6 fixes are recorded through strata:ba91973.
- Mutants: 14 of 14 killed across ASan and TSan; `slot_relaxed` is detected under TSan.
- Whole-buffer flush output matches base digests at 1 and 3 flush threads.
- Memory per buffered record falls from 347.5 / 389.5 / 368.4 B to 223.4 / 220.5 / 228.6 B for 100 B values.
- Open-index memory with 10 buffered 100 B documents is about 3.0 KB higher; item 4 remains the next step.
- Docs: `docs/design/write-buffer.md`, `docs/perf/write-buffer-bakeoff.md`, `docs/perf/lanebuf-scan-tail.md`.
- Left open: A7.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
