# track-c-c1: journal

State: landed
Quest: .campaign/xapiand/quests/track-c-c1.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:de99ec7e on 2026-10-14 under the earlier campaign skill.

## Next

1. Nothing: landed. Track C's other items are quest `track-c`.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/track-c.md`: "Membership in the log, single-step changes, replacements, quorums, learners; any member votes (M1)" ("Progress in consensus": D7 at 0307308 and 2f8e6e2; D1, D3, D4, D5, D8 at f050b7a, 3b20cef, 9f7d43e; D6 at 90d3df0) |
| A2 | `docs/design/track-c.md`: "Witness mixes, hand-off to a data voter that answered, the truncation bound from the current leader's report" (D8 learners; D9 witnesses at 080e1c1, 91de8ea, 6cc0d88, 9f7d43e) |
| A3 | `docs/gates/track-c-c1.md`: "Its reader refuses each format Track C added: `RFTINIT2`, `RFTJOIN1`, `RFTBTCH2`, checkpoint bundle v3" (T1 at consensus 1822fd7: b7073bb's fixtures recover; its reader refuses RFTINIT2, RFTJOIN1, RFTBTCH2, bundle v3, RFTSNP02 and a DTC2 frame) |
| A4 | `docs/gates/track-c-c1.md`: "has every element of T2's fault mix and every oracle T2 lists, plus these" (T2: the randomized simulation, crashes and partitions on every seed) |
| A5 | `docs/gates/track-c-c1.md`: "`test/liveness_test.cc` at `6cc0d88`, 400 seeds (3 and 5 voters alternating)" (T3: 400 seeds, rerun after the review's fixes with identical results) |
| A6 | `docs/gates/track-c-c1.md`: "`consensus_test` `learner_install_and_promotion` (`7c60f58`)" (T4 rows) |
| A7 | `docs/gates/track-c-c1.md`: "`FUZZ_DECODER` pins every input to one decoder, and `tools/fuzz.sh` gives each its own CPU-hour" (T8 at consensus 56641cb: eight decoders, one CPU-hour each, zero crashes; coverage tests to the targets) |
| A8 | `docs/reviews/track-c-design.md`: "**C1's gate: APPROVE WITH CONDITIONS.** M8 and M9 close" (sixth revision check: "C1's gate: APPROVE WITH CONDITIONS") |
| A9 | open: conditions 22 and 23 of `docs/reviews/track-c-design.md` are due before the coordinated DTC2 commit, carried to quest track-c |

## Result

Landed at strata:de99ec7e on 2026-10-14. Code in Kronuz/consensus main (through faef7ad for the reruns); gate report `docs/gates/track-c-c1.md`; review `docs/reviews/track-c-design.md`, sixth revision check: "C1's gate: APPROVE WITH CONDITIONS. M8 and M9 close, and Conditions 18 to 21 close", with conditions 22 and 23 due before the DTC2 commit, and D11's additions approved with conditions.

- T8 fuzzing, one CPU-hour per decoder: control codec 123,228,844 executions, membership 149,154,413, founding record 76,627,012, five more decoders, zero crashes; the first minutes found a non-canonical RFTINIT2, now refused.
- C3 (repair at or below the commit cursor) was built in consensus afd456d with no gate report of its own; C2 and C4 were partly built when distributed work parked (owner, 2026-10-20): quest `track-c`.
- Docs: `docs/design/track-c.md`, `docs/gates/track-c-c1.md`, `docs/reviews/track-c-design.md`; Detent's `docs/LIBRARY_BOUNDARIES.md` records the landed consensus changes (Detent 83977ad).
- Left open: A9.

## Rulings I made

None beyond the quest file's Decisions (the plan and the reviews are the record).
