# install-own-files: journal

State: landed
Quest: .campaign/xapiand/quests/install-own-files.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:ef48ed0 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/install-own-files.md`: "`17e8b3c` the fix and `install_own_test`." |
| A2 | `docs/status/install-own-files.md`: "`9cf444d` C1 (D5 sentence, this ledger) and C5 (scrubber note in `media-faults.md`)." |
| A3 | `docs/status/install-own-files.md`: "`ff812c6` C4: faultfs `damage_writes`." |
| A4 | `docs/status/install-own-files.md`: "`853dba3` C3: `read_checked` counts EIO, `corrupt_index` and `corrupt_segment` only." |
| A5 | `docs/status/install-own-files.md`: "C2: the reopen runs between two `guarded` regions (copies written and synced; swap and commit)" |

## Result

Landed at strata:ef48ed0 on 2026-10-26. Full ctest under ASan+UBSan: 47/47; no TSan run; CI not dispatched. Reviews: `docs/reviews/install-own-files-code.md` -- APPROVE WITH CONDITIONS; `docs/reviews/shared-reader-views-code.md` -- "the pre-existing hazard"; `docs/reviews/install-architect.md` -- C1.

- Merge: Merge branch 'fix/install-errors'.
- `strata:17e8b3c`: fix and `install_own_test`; landed on main at `53c088d`.
- `strata:9cf444d`, `strata:ff812c6`, `strata:853dba3`: docs/ruling and damage/error handling follow-ups.
- Mapped regression aborts on main with "a file truncated below a live mapping"; fixed mapped and unmapped tests pass.
- Reopening adds one open and mmap per mapped segment; unmapped IO reads nothing more.
- A damaged write installs but surfaces as `corrupt_segment` at first read; `heal()` names it.
- docs/design/phase-3.md and docs/design/media-faults.md are named as changed outputs.
- Docs: `docs/design/phase-3.md` and `docs/design/media-faults.md` are named, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
