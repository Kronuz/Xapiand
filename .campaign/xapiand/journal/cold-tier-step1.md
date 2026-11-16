# cold-tier-step1: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step1.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:66ddb16 on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step1.md`: "[x] **K1** (T6's 1,000-schedule model is owed to the gate; the rendezvous tests stand in). Record and body" |
| A2 | `docs/status/cold-tier-step1.md`: "[x] **K2.** D9's table in code, each map point's failure rule with a faultfs-injected test" |
| A3 | `docs/status/cold-tier-step1.md`: "[x] **K4** (the part item 1 touches). A missing or resized manifest-named file fails the open (D11)." |
| A4 | `docs/status/cold-tier-step1.md`: "[x] **K7** (listing only). prospect's construction sites and Xapiand's seam, with the pin bump" |
| A5 | open: marked partial ([~]); CI on every platform left to the lead in `docs/status/cold-tier-step1.md` |

## Result

Landed at strata:66ddb16 on 2026-10-27. Eager and lazy suites pass 54 of 54 on rel, ASan+UBSan and TSan. Reviews: `docs/reviews/cold-tier-step1-code.md` -- APPROVE WITH CONDITIONS (head strata:40fadb9); `docs/reviews/cold-tier-design.md` (preliminary, re-check 1, re-check 2; not on this branch).

- Merge: Merge branch 'phase/cold-tier-step1' (cold tier step 1, items 1 and 2).
- Landed on the branch as strata:d0ab622; review fixes strata:444c26f.
- Review-fix rel suite passes 55 of 55, eager and lazy.
- K1 mutants M1-M4 are killed; T6's 1,000 seeded schedules are not built.
- K2 mutants M5-M14 are killed; output-map probe and transient-damage healing checks pass.
- K4 covers missing/truncated files; twelve damage cases wait for item 4.
- Perf is not measured; lazy segments remain off by default pending item 6.
- Docs: `docs/mechanisms/segment-records.md`.
- Left open: A5.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
