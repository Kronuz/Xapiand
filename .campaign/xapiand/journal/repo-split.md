# repo-split: journal

State: landed
Quest: .campaign/xapiand/quests/repo-split.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:426cc59 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/repo-split.md`: "strata keeps the core, its tests (`core_kv`, `files`, `faultfs`, `merge_policy`" |
| A2 | `docs/status/repo-split.md`: "455 of strata's 713 commits carry over, all authored by Germán Méndez Bravo." |
| A3 | `docs/status/repo-split.md` |
| A4 | `docs/status/repo-split.md`: "strata's 9 tests and prospect's 36 pass on macOS (45, as before the split)" |
| A5 | open: both repositories' CI runs in progress when recorded in `docs/status/repo-split.md` |

## Result

Landed at strata:426cc59 on 2026-10-20. Identity diffs: 4 files / 42,124 KiB; 7 / 45,272 KiB; 5 / 42,128 KiB; 9 / 86,300 KiB; all identical. No review named in the ledger.

- Merge: Merge remote-tracking branch 'origin/phase4/repo-split'.
- strata:1453626 removes search; strata::core is the only target.
- strata:3d3fd9b updates repository documentation and diagrams.
- prospect:d804951 leaves core headers and shared docs with strata.
- prospect:c5111d9 performs the rename; prospect:5fc4171 migrates namespace, target, and callers.
- cpp-libraries:5d7f459 adds strata and prospect catalog entries.
- macOS tests: strata 9, prospect 36, total 45; Linux prospect adds directory_tmpfs (37).
- Docs: `docs/design/core-split.md`, `docs/perf/p15-region-size.md`.
- Left open: A5.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
