# strata-engine-search: journal

State: landed
Quest: .campaign/xapiand/quests/strata-engine-search.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:4caf46c9b on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/strata-engine-search.md`: "A search over a Xapian and a strata index together is refused." |
| A2 | `docs/status/strata-engine-search.md`: "random trees of every supported node (900+ queries)" |
| A3 | `docs/status/strata-engine-search.md`: "`multi_snapshot` + `prospect::search` (pruned for ascending ids, exhaustive for a spy or descending ids" |
| A4 | `docs/status/strata-engine-search.md`: "**Estimates are exact counts**; Xapian's are estimates unless `check_at_least` covers the matches." |
| A5 | `docs/status/strata-engine-search.md`: "FILTER weighs (308), descending ignored (200), deleted documents visible (5), pruning drops ties (141)" |
| A6 | `docs/status/strata-engine-search.md`: "A PHRASE keeps its terms' query positions as offsets" (this half landed later, at Xapiand:80327f82c; the refusals landed here) |
| A7 | `docs/status/strata-engine-search.md`: "The spy now sees each match with its id in its shard, weight and stored data" |
| A8 | open: partial: the server under ASan and TSan (T10) not run in `docs/status/strata-engine-search.md` |

## Result

Landed at Xapiand:4caf46c9b on 2026-10-28. Unit proof: 227,724 checks; worst relative weight difference 4.2e-16, tolerance 1e-9. Review: `docs/reviews/strata-engine-search-code.md` named; ledger says CR1-CR4 and CR6 closed, ready for lead re-check.

- Merge: Merge branch 'phase7a/strata-engine-search' (S2: strata search behind the seam).
- Xapiand:90cfd9775 search, `Shard::pin`, dispatcher, and tests; Xapiand:544b0ecb0 spies and search check.
- Xapiand:e4e29fb16 append fix; Xapiand:1791c7961 flat scorers for append runs.
- Round 2: 230,444 checks; worst relative weight difference 3.9e-16, tolerance 1e-9.
- ASan+UBSan and TSan round 2: 152,733 checks, 0 reports each.
- Docs suite: Xapian 576 pass; strata 346 pass, 230 fail, with failures attributed to later-step refusals or value aggregations.
- Query smoke passes on both engines; open: lead re-check of CR2 and prospect pin update.
- Docs: `docs/mechanisms/strata-search.md` named.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
