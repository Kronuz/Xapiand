# cold-tier-step2-item2: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item2.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:033e546 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "RB `hard_caps`; CI `caps_by_io` (kernel call counters unchanged, `refused == 0`)" |
| A2 | `docs/status/cold-tier-step2.md`: "CI `fx::agrees` throughout, `stress` at 6 barriers, `posix_counts`" |
| A3 | `docs/status/cold-tier-step2.md`: "CI `directory_descriptor` (182 IOs, the 183rd refused before the factory runs)" |
| A4 | `docs/status/cold-tier-step2.md`: "a soft-cap crossing (and a lowered cap, and a pass that finds the count over) posts to every listener" |

## Result

Landed at strata:033e546 on 2026-11-02. All 21 resource-budget and counted-IO mutants die. Review: `docs/reviews/cold-tier-step2-items1-2.md` (combined review follow-ups).

- Merge: Merge branch 'phase/cold-tier-step2-item2' (counted IO and the process-wide resource budget).
- Landed as strata:e17f179; tests and limits ledger strata:6c4def5; include allowlist strata:bc42a01.
- ASan+UBSan and TSan report zero issues in the two new tests.
- Preflight: mac-hardened, mac-asan-ubsan and Linux ok; FreeBSD not run.
- Stress cases run 77,815 and 72,000 operations; 16,696 refusals, 11,890 kernel-model failures, 6,413 allocation failures.
- `docs/design/limits.md` gains cold-tier rows; item 3 consumes the budget API.
- Docs: `docs/design/limits.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
