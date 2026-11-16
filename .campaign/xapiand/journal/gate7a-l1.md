# gate7a-l1: journal

State: landed
Quest: .campaign/xapiand/quests/gate7a-l1.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:10e0b0aaf on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/gate7a-l1.md`: "Collapse parity (`test_collapse` in `tests/test_engine_search_strata.cc`): 144 cases" |
| A2 | `docs/status/gate7a-l1.md`: "`removed_features_check.sh` (extended): 10 of 10 per engine: `?fuzzy`, `?nearest`, both" |
| A3 | `docs/status/gate7a-l1.md`: "`percent_threshold` and `weight_threshold` leave `DatabaseHandler`, `SearchRequest`" |
| A4 | `docs/status/gate7a-l1.md`: "Searches replace the removed `.terms` assertions; `terms_diff.py` and `vocab_diff.py` are ported" |
| A5 | `docs/status/gate7a-l1.md`: "Remaining refusals (round 2, 2026-10-29): closed" |

## Result

Landed at Xapiand:10e0b0aaf on 2026-10-29. ctest 28 of 28; docs E2E Xapian 649 of 649, strata 649 of 649. Review: strata `docs/reviews/phase-7a-gate.md` named in the ledger.

- Merge: Merge branch 'phase7a/gate-l1' (gate 7a L1 and C3: no refused features on strata).
- Xapiand:1b8954691 removed nearest/fuzzy parameters and weight cutoffs.
- Xapiand:985417616 served collapse on strata; Xapiand:22e56602a added generated-term search checks.
- Xapiand:3956598af pinned prospect multi-phrase scorer; Xapiand:5a261e358 rejects invalid query shapes at construction.
- `search_order_diff.py` 134 of 134 identical; analysis probes 42,798; stem probes 18,006.
- Search tests: 477,059 checks; fuzz_text 3,876,800 texts, 2,932,131 parsed, 944,669 rejected, no violation.
- Open: parser change to accept wildcard/edit-distance/partial words inside PHRASE/NEAR; lead re-check.
- Docs: `docs/dev/testing.md` named.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
