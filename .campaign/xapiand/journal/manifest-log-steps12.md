# manifest-log-steps12: journal

State: landed
Quest: .campaign/xapiand/quests/manifest-log-steps12.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:591658a on 2026-11-04 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/manifest-log-steps12.md`: "[x] Unit tests with fixtures (step 1: `test/framing_test.cc`, fixtures `test/fixtures/framing/`" |
| A2 | `docs/status/manifest-log-steps12.md`: "[x] tail_model's single-bit-flip property as a test (0 silent): step 1, 219,672 flips, 0 silent, 584 in padding." |
| A3 | `docs/status/manifest-log-steps12.md`: "[x] Every honest crash image as a test: step 1, 1,020 images, 672 torn, 0 misread." |
| A4 | `docs/status/manifest-log-steps12.md`: "[x] A libFuzzer target for the decoder, 5 minutes under ASan+UBSan (Homebrew LLVM)" |
| A5 | `docs/status/manifest-log-steps12.md`: "[x] A revert check per rule, and mutants (step 1): five mutants, each fails `journal_framing`" |
| A6 | `docs/status/manifest-log-steps12.md`: "[x] Linux on the local VM (Ubuntu 24.04, g++ 13.3): journal 12 of 12 at `journal:5336512`" |
| A7 | `docs/status/manifest-log-steps12.md`: "[x] A how-it-works note in `docs/mechanisms/` (step 1: journal `docs/mechanisms/framing.md`" |
| A8 | open: left unticked in the ledger (TSan; nothing concurrent) in `docs/status/manifest-log-steps12.md` |

## Result

Landed at strata:591658a on 2026-11-04. Step 1 fuzz run: 481,901 inputs clean; step 2 fuzz: 339,203 inputs clean. Review: `docs/reviews/manifest-log-design.md` (steps 1 and 2 may start, step 1 with L1 tail rule); `docs/reviews/manifest-log-codec-security.md` (SR1, SR5 fixed); journal `docs/reviews/framing-security.md` (SR1, SR2 fixed).

- Merge: Merge remote-tracking branch 'origin/main' into phase/manifest-log-steps12.
- Step 1 committed as journal:b1b7368; framing constant-time end support journal:9a3734f.
- Step 2 codec landed in the strata branch; journal pin advanced to journal:df7db00.
- Step 2 mutants: 20 of 20 killed; replay and decoder checks leave state unchanged on rejection.
- Linux journal tests 12 of 12; strata codec tests 7 of 7.
- Journal PR #2 and strata CI were pushed but not watched; step 3 conditions and monotone-scalar owner question remain open.
- Docs: `docs/mechanisms/manifest-log-codec.md`; journal `docs/mechanisms/framing.md`.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
