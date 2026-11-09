# retention: journal

State: landed
Quest: .campaign/xapiand/quests/retention.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:0156b57 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/retention.md`: "`770866d`: default 0; `fallback()` report; D4 text; `test/retention_test.cc`." |
| A2 | `docs/status/retention.md`: "RET-1: `retain()` references every pending merge's sources; the race is a test in" |
| A3 | `docs/status/retention.md`: "RET-2: lifecycle, index, reclaim and media run at the default; 2 (media: 1) is confined to" |
| A4 | `docs/status/retention.md`: "RET-3: D4 says 47-108% more settled disk and cites `docs/perf/retention.md`, now on the branch." |
| A5 | `docs/status/retention.md`: "RET-4/RET-6/RET-7: D4 and `index.hh` say the report is about local manifests" |
| A6 | `docs/status/retention.md`: "RET-8: a test with a gap (pinned checkpoint under later commits) kills `discarded = newest - to`." |

## Result

Landed at strata:0156b57 on 2026-10-21. ASan+UBSan full ctest: 25 tests, all pass. Review: `docs/reviews/retention.md` -- REJECT, RET-1 and RET-2 blocking.

- Merge: Merge remote-tracking branch 'origin/phase4/retention'.
- `strata:770866d`: default 0, fallback report, D4 text, and retention test.
- RET-1 fix retains pending merge sources; tests cover 0, 1, and 2 predecessors.
- Each fix reverted alone fails its corresponding test; pending-source removal aborts on a live mapping.
- Follow-ups remain for crash-persistent fallback reporting, group-host recorded-generation retention, prospect mirror suites, and gate-row baselines.
- Review round 1 rejected on RET-1 and RET-2; fixes folded in, re-check pending.
- Docs: ledger names `docs/perf/retention.md`, but the repository glob found no matching docs files.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
