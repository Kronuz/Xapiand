# flushmem: journal

State: landed
Quest: .campaign/xapiand/quests/flushmem.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:965339c on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/flushmem.md`: "fusefilter `filter8::build(vector&&)`; `sorted_encoder` uses it; `keys::filter_build_bytes` is 32 n + 1 MiB." |
| A2 | `docs/status/flushmem.md`: "termdict `builder` takes a block sink; the keys section streams into the section sink in merge and flush." |
| A3 | `docs/status/flushmem.md`: "A streamed merge frees its versions before the filter build." |
| A4 | `docs/status/flushmem.md`: "Policy: waiting and serializing measured as no help (single merge dominates); smaller merges argued, not built." |

## Result

Landed at strata:965339c on 2026-10-26. `budget_peak` is 30% smaller for fill, 38% for read, and 14% for ycsb_a. No review named in the ledger.

- Merge: Merge branch 'perf/flushmem'.
- strata:39c9cc1 is the base; fusefilter:755b392 and termdict:8367263 are pinned library branches.
- A 1M-key merge falls from 101 to 61 MiB.
- Segment bytes, throughput, RSS, and dirty footprint are unchanged.
- Full ctest: 53 green.
- The cap does not hold: merge scratch is 61 B per output key.
- Linux and FreeBSD remain unmeasured; the merge-scratch bound and Detent pin bump remain open.
- Docs: `docs/perf/flushmem.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
