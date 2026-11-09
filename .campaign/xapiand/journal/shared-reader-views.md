# shared-reader-views: journal

State: landed
Quest: .campaign/xapiand/quests/shared-reader-views.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:23e78bb on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/shared-reader-views.md`: "[x] K1 Items 3 and 5 as D1, D4, D6 to D9 and D11 state them (`909ff4e`)" |
| A2 | `docs/status/shared-reader-views.md`: "[x] K2 SR-D19 and SR-D20 closed (`909ff4e`, `4392d39`, the NDEBUG fix below)." |
| A3 | `docs/status/shared-reader-views.md`: "[x] K3 15 mutants, 15 killed (`.scratch/srv/mutants.log`, logs in `.scratch/srv/out/`)" |
| A4 | `docs/status/shared-reader-views.md`: "[x] K4 macOS x86_64, Homebrew clang 23, light holds, at `4392d39`: ASan+UBSan (Debug -O1)" |
| A5 | open: left unticked; the last push's CI not run in `docs/status/shared-reader-views.md` |
| A6 | `docs/status/shared-reader-views.md`: "New in `strata/core/index.hh`: `basic_index_options::readers`; `basic_index::view`" |

## Result

Landed at strata:23e78bb on 2026-10-26. ASan+UBSan ctest 43 of 43 and TSan ctest 43 of 43 at `4392d39`; later NDEBUG build passes named tests. Review: `docs/reviews/shared-reader-views-code.md` -- approve with conditions.

- Merge: Merge branch 'phase4/shared-reader-views'.
- `strata:909ff4e`: items 3 and 5 with lease; `strata:25dae46`: test-order fix; `strata:4392d39`: close guard and CI fix; `strata:bc59e2b`: ledger.
- K1-K4 and K6 are marked met; K5 was green at `4392d39` on all 8 jobs, but last push still needed its run.
- Mutants: 15 of 15 killed.
- Follow-ups include code review, architect check, SR-D19/SR-D20 design revision, and item 6; install-source mapping hazard remains noted for follow-up.
- Docs: `docs/design/shared-reader.md` and `docs/design/write-buffer.md`.
- Left open: A5.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
