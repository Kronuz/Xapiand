# format13: journal

State: landed
Quest: .campaign/xapiand/quests/format13.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:76fa331 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | open: left unticked in the ledger in `docs/status/format13.md` |
| A2 | open: left unticked in the ledger in `docs/status/format13.md` |
| A3 | open: left unticked in the ledger in `docs/status/format13.md` |
| A4 | open: left unticked in the ledger in `docs/status/format13.md` |
| A5 | open: left unticked in the ledger in `docs/status/format13.md` |
| A6 | `docs/status/format13.md`: "[x] K6 media_test's sweep covers bytes 0 to 15 of every segment: a transient flip is retried and healed" |
| A7 | open: left unticked in the ledger in `docs/status/format13.md` |
| A8 | `docs/status/format13.md`: "[x] K8 Step 7c changes the digest's value in exactly one commit (F21), which re-pins digest.txt" |
| A9 | `docs/status/format13.md`: "[x] K9 A fresh open touches the header page and one run of pages ending at the footer, and no byte of lengths (T7)" |
| A10 | open: left unticked in the ledger in `docs/status/format13.md` |
| A11 | `docs/status/format13.md`: "[x] K11 F20's checks: a mask bit the binary cannot read and a version range beyond it refuse open" |
| A12 | open: left unticked in the ledger in `docs/status/format13.md` |
| A13 | `docs/status/format13.md`: "[x] K13 F13-R18 is fixed in the rebase plan before 7c-4, and each F13-R25 item is in the design" |
| A14 | open: left unticked in the ledger in `docs/status/format13.md` |

## Result

Landed at strata:76fa331 on 2026-10-20. Merge review fixes FM1, FM3, and FM4 recorded; ctest 36/36 at each code commit. Review: `docs/reviews/format-13-code.md` -- APPROVE WITH CONDITIONS (step 7c, `phase4/format13` at strata:f2c19c6); `docs/reviews/format-13-design.md` -- APPROVE WITH CONDITIONS; `docs/reviews/format-13-merge.md` -- conditions done, verdict unclear in ledger; `docs/reviews/format-13-mutants.md` -- verdict unclear in ledger.

- Merge: Format 13 checkpoint: pre-freeze versions become 13 and 2.
- Main merge ff5392e preserves format-13 commits and resolutions; fixtures and digest match pre-merge branch.
- FM4 records 1M-key peak through finish 132.2 to 85.8 MiB over key map; charged 56.0 to 93.6.
- T5 key hits 0.71 to 0.80; misses equal; indexing 0.95 to 0.98.
- Merge review and mutant review records include FM1-FM4 and MR1/MR2 checks.
- Format remains not frozen; pre_freeze remains 0x80000001 for segments and files family 0x80000005 on directory-lock work.
- K1-K5, K7, K10, K12, and K14 are not checked in the ledger.
- Docs: `docs/design/format-13.md`, prospect `docs/perf/format13-landing.md`.
- Left open: A1, A2, A3, A4, A5, A7, A10, A12, A14.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
