# research-indexing: journal

State: landed
Quest: .campaign/xapiand/quests/research-indexing.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:18febff on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/research-indexing.md`: "Done criteria (set by the lead; contributors tick, never edit): none set yet." |
| A2 | `docs/status/research-indexing.md`: "The note (`docs/research/indexing.md`) holds the bound on both machines, today's attribution, the survey" |

## Result

Landed at strata:18febff on 2026-10-30. Inverters A/B/C measure 340 / 139 / 118 ns per token; C scales 115/105/92.5/85 ns per token at 1/2/4/8 threads. No review named in the ledger.

- Merge: Land the documents of the unmerged branches, labelled, and triage the branches.
- strata:56776ab adds the per-stage bound benchmark and note results.
- strata:0406611 records MinHash k=2 savings of 3.4% of postings plus positions in 48 ms.
- Copy-merge floor is 24.6 ns per posting against today's 497 ns per posting, 20x.
- Quiet VM cache-resident path measures 56 ns per token; Mac bound is 49.
- Target: 4.5x faster baseline indexing; 5M-document goal about 1:55 at one thread and about 0:57 at eight.
- Seven of eight CI jobs green; ASan+UBSan cancelled at the shared workflow's 60-minute cap.
- Docs: `docs/research/indexing.md`, research/indexing/.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
