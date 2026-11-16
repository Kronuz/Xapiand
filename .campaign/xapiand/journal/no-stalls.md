# no-stalls: journal

State: landed
Quest: .campaign/xapiand/quests/no-stalls.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:18febff on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/no-stalls.md`: "Done criteria (set by the lead; contributors tick, never edit): none set." |
| A2 | `docs/status/no-stalls.md`: "The note holds the bound (section 2), the survey from pinned sources (3), the harness and calibration (4.0, 4.1)" |

## Result

Landed at strata:18febff on 2026-10-30. At 5,000 docs/s over 180 s, RocksDB leveled stalled 120 of 180 s; universal stalled 19 to 39 s with a 12.6 s stop. No review named in the ledger.

- Merge: Land the documents of the unmerged branches, labelled, and triage the branches.
- strata:31dd085 adds the strata and RocksDB ingest harnesses and additive index hooks.
- strata:cb14936 adds the bound and survey; strata:85c2050 records harness calibration.
- Strata had no stall; segments 5 → 90, query p50 0.1 → 1.5 ms, p99 0.2 → 3.6 ms.
- Feed-forward service p99 under 8 ms; adaptive refresh yields 42 segments against 90 and query p99 1.2 against 3.6 ms.
- Cheapest-first scheduling yields 35% fewer segments for one index and 23% fewer across 32.
- Research note recommends background flush, host merge scheduler, and refresh-then-admission debt response.
- A second many-index hot experiment remains open.
- Docs: `docs/research/no-stalls.md`, research/no-stalls/.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
