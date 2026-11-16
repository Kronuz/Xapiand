# termdict-validate-at-open: journal

State: landed
Quest: .campaign/xapiand/quests/termdict-validate-at-open.md (the lead's; never edit it)

## Doing

Nothing: landed at termdict:8b1e964 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/validate-at-open.md`: "K1 met. `open_rejects` (test/test.cc): hand-built FSTs, one per rejected case" |
| A2 | `docs/status/validate-at-open.md`: "13,615,556 runs in 2,101 s, no report, peak 610 MB." |
| A3 | `docs/status/validate-at-open.md`: "K3 met. Mutants (`.scratch/mut/run.py`: a copy of fst.hh or termdict.hh per mutant" |
| A4 | `docs/status/validate-at-open.md`: "K4 met. English Wikipedia vocabulary, 3,964,752 terms (strata/.scratch/vocab.txt)" |
| A5 | open: the lead's CI watch; not recorded in `docs/status/validate-at-open.md` |

## Result

Landed at termdict:8b1e964 on 2026-10-26.

- Review: `docs/reviews/validate-at-open-security.md` -- APPROVE.
- Left open: A5.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
