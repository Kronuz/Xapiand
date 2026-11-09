# numeric-range: journal

State: landed
Quest: .campaign/xapiand/quests/numeric-range.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:5f356d78f on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/numeric-range.md`: "Terms are a prefilter, not the whole filter" |
| A2 | `docs/status/numeric-range.md`: "`range_terms::decompose` (`src/multivalue/range_terms.h`) replaces `_numeric`'s walk" |
| A3 | `docs/status/numeric-range.md`: "Open ends: type limits instead of `min/max<T>(accuracy)` (2x the coarsest accuracy)" |
| A4 | `docs/status/numeric-range.md`: "Property test `tests/test_range_terms.cc` (321553 checks, 0 wrong)" |
| A5 | `docs/status/numeric-range.md`: "master 399 wrong (n 157/206, p 140/197, f 61/197, d 41/197), now 0." |
| A6 | `docs/status/numeric-range.md`: "CR1 (blocker): a terms-only field (no value slot) throws `TermsOnlyRangeError` (400, field named)" |

## Result

Landed at Xapiand:5f356d78f on 2026-10-27. Property test: 321553 checks, 0 wrong; 54000 random and boundary 64-bit ranges. Review: `docs/reviews/numeric-range-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'fix/numeric-range' (exact numeric range terms).
- Xapiand:76097e087 and 3dc64ef63 -- decomposition and follow-up.
- Live harness: 797 ranges; master 399 wrong, branch 0.
- E2E 576/576; ctest 20/20 Release; ASan+UBSan unit tests and range check PASS.
- Review CR1 tests: open-ended and too-wide terms-only ranges return HTTP 400; engine test 50 checks.
- Open: value-source performance for selective open-ended queries; integer slot precision, floating cast, RESTORE first-document issue, and custom-accuracy findings.
- Docs: `docs/mechanisms/numeric-range-terms.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
