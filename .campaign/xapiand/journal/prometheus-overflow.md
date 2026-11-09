# prometheus-overflow: journal

State: landed
Quest: .campaign/xapiand/quests/prometheus-overflow.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:55316114d on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/prometheus-overflow.md`: "the `find_package(Sanitizers)` block moves above `include(FetchContent)`" |

## Result

Landed at Xapiand:55316114d on 2026-10-26. Fixed build: 0 reports in 2 runs × 30 rounds; first per-target variant also had 0 in 14 and 30 rounds. No review named in the ledger.

- Merge: Merge branch 'explore/prometheus-overflow'.
- Control binary: 2 of 4 runs abort at round 8; earlier recheck 2 of 2.
- Fresh ASan and TSan configurations: all 26 fetched-dependency targets carry the flag.
- TSan endpoint, engine shard, and debounce tests exit 0 with 0 reports.
- Open: server workload not run under TSan; blocking-guard test unavailable on master.
- Docs: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
