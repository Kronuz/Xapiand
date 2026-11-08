# prospect-inverter: journal

State: landed
Quest: .campaign/xapiand/quests/prospect-inverter.md (the lead's; never edit it)

## Doing

Nothing: landed at prospect:e14508c on 2026-10-18 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/inverter.md`: "rebased onto layers with the architect's R23 and R24 applied" |
| A2 | `docs/status/inverter.md`: "ctest 35 of 35 (macOS, Apple clang, Release); segments, manifests and meta byte-identical to `ref-base-20k`" |
| A3 | `docs/status/inverter.md`: "Findings folded in: CR1 (`0da5a05`), CR2 (`adc82ba`), CR9 (`c8b65c6`), CR10, CR11 (`5cde290`" |
| A4 | `docs/status/inverter.md`: "experiments in `docs/perf/inverter.md`, each written before it runs" |
| A5 | `docs/status/inverter.md`: "1M confirmation at `c6bf4d9` (gate VM, three rounds): 3.494x Tantivy (3.207-3.498)" |
| A6 | `docs/status/inverter.md`: "CI run 39189683111 green on linux-x86_64-clang, linux-x86_64-gcc, linux-arm64-gcc, macos-arm64" |

## Result

Landed at prospect:e14508c on 2026-10-18. Landed on strata main as 2ec8c36 to 5b7787e before the repository split; prospect:740c76b to prospect:e14508c in prospect's history.

- Reviews: `docs/reviews/inverter-code.md` -- REJECT (on CR1/CR2, then CR9, then CR11, each closed in turn); `docs/reviews/inverter-main.md` -- APPROVE with conditions CR14 to CR18 on main (carried by `prospect-inverter-followups`).
- Raw run tables (`.scratch/runs/*/runs.tsv`) stayed in the strata worktrees; the numbers are in `docs/perf/inverter.md`.
- Docs: `docs/perf/inverter.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
