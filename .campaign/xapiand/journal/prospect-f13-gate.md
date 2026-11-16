# prospect-f13-gate: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-f13-gate.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:aa532de on 2026-10-19 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/f13-gate.md`: "identical A against B on both layouts, 1,162 queries; `verify` and the probe's `verify` 0 mismatches" |
| A2 | open: missed by 9.96 MB (843,636,006 against 833,674,096) in `docs/status/f13-gate.md` |
| A3 | open: indexing B/F12 1.08 and 1.06, M20's seeds_of 11% of build and merge CPU in `docs/status/f13-gate.md` |
| A4 | `docs/status/f13-gate.md`: "Row 4a: B against A at the run level, four sessions, 31, 22, 9, 10 wins, 0 losses." |
| A5 | `docs/status/f13-gate.md`: "Row 4b: the harness on S′, five trials, B reordered against Lucene (98 wins, 2 losses)" |
| A6 | `docs/status/f13-gate.md`: "Row 5: merged terms against format 12, 16 and 15 wins, 0 losses" |

## Result

Landed at prospect:aa532de on 2026-10-19. Merged on strata main before the repository split; the commit is its twin in prospect's filtered history.

- Record `docs/perf/format13-gate-vm.md`, raw files' hashes in `docs/perf/format13-gate-vm.sha256`; the raw files stayed on the gate VM (32 MB in `~/bench/results/f13g/`).
- Review: none.
- Docs: `docs/perf/format13-gate-vm.md`, `docs/perf/format13-gate-vm.sha256`.
- Left open: A2, A3.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
